/*
 * Copyright (C) 2024, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "mnx.h"

#include "nlohmann/json-schema.hpp"
#include "mnx_schema.xxd"

static const std::string_view MNX_SCHEMA(reinterpret_cast<const char *>(mnx_schema_json), mnx_schema_json_len);

using namespace musx::dom;
using namespace musx::util;
using json = nlohmann::ordered_json;
//using json = nlohmann::json;

namespace denigma {
namespace mnx {

static void createMnx([[maybe_unused]] const MnxMusxMappingPtr& context)
{
    auto mnxObject = json::object();
    mnxObject["version"] = MNX_VERSION_NUMBER;
    context->mnxDocument["mnx"] = mnxObject;
}

static void createGlobal([[maybe_unused]] const MnxMusxMappingPtr& context)
{
    auto globalObject = json::object();
    globalObject["measures"] = json::array();
    context->mnxDocument["global"] = globalObject;
}

static void createParts(const MnxMusxMappingPtr& context)
{
    /// @todo figure out mulitstaff instruments
    auto partsObject = json::array();
    auto multiStaffInsts = context->document->getOthers()->getArray<others::MultiStaffInstrumentGroup>(SCORE_PARTID);
    auto scrollView = context->document->getOthers()->getArray<others::InstrumentUsed>(SCORE_PARTID, SCROLLVIEW_IULIST);
    int partNumber = 0;
    for (const auto& item : scrollView) {
        auto staff = item->getStaff();
        auto multiStaffInst = staff->getMultiStaffInstGroup();
        if (multiStaffInst && context->inst2Part.find(staff->getCmper()) != context->inst2Part.end()) {
            continue;
        }
        std::string id = "P" + std::to_string(++partNumber);
        auto part = json::object();
        part["id"] = id;
        part["name"] = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (part["name"] == "") {
            part.erase("name");
        }
        part["shortName"] = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (part["shortName"] == "") {
            part.erase("shortName");
        }
        if (multiStaffInst) {
            part["staves"] = multiStaffInst->staffNums.size();
            for (auto inst : multiStaffInst->staffNums) {
                context->inst2Part.emplace(inst, id);
            }
            context->part2Inst.emplace(id, multiStaffInst->staffNums);
        } else {
            context->inst2Part.emplace(staff->getCmper(), id);
            context->part2Inst.emplace(id, std::vector<InstCmper>({ InstCmper(staff->getCmper()) }));
        }
        partsObject.emplace_back(part);
    }
    context->mnxDocument["parts"] =  partsObject;
}

static void createScores(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto musxLinkedParts = context->document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
    std::sort(musxLinkedParts.begin(), musxLinkedParts.end(), [](const auto& lhs, const auto& rhs) {
        return lhs->partOrder < rhs->partOrder;
    });
    for (const auto& linkedPart : musxLinkedParts) {
        auto mnxScore = json::object();
        mnxScore["layout"] = scrollViewLayoutId(linkedPart->getCmper());
        auto mmRests = context->document->getOthers()->getArray<others::MultimeasureRest>(linkedPart->getCmper());
        for (const auto& mmRest : mmRests) {
            auto mnxMmRest = json::object();
            mnxMmRest["duration"] = mmRest->calcNumberOfMeasures();
            if (!mmRest->calcIsNumberVisible()) {
                mnxMmRest["label"] = "";
            }
            mnxMmRest["start"] = mmRest->getStartMeasure();
            if (mnxScore["multimeasureRests"].empty()) {
                mnxScore["multimeasureRests"] = json::array();
            }
            mnxScore["multimeasureRests"].emplace_back(mnxMmRest);
        }
        mnxScore["name"] = linkedPart->getName(EnigmaString::AccidentalStyle::Unicode);
        if (mnxScore["name"] == "") {
            mnxScore["name"] = linkedPart->isScore()
                             ? std::string("Score")
                             : std::string("Part ") + std::to_string(linkedPart->getCmper());
        }
        /// @todo pages
        if (mnxDocument["scores"].empty()) {
            mnxDocument["scores"] = json::array();
        }
        mnxDocument["scores"].emplace_back(mnxScore);
    }
}

void convert(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << outputPath.u8string());
        return;
    }
#endif
    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    auto document = musx::factory::DocumentFactory::create<MusxReader>(xmlBuffer);
    auto context = std::make_shared<MnxMusxMapping>(denigmaContext, document);

    createMnx(context);
    createGlobal(context);
    createParts(context);
    createLayouts(context); // must come after createParts
    createScores(context); // must come after createLayouts
    
    // validate the result
    try {
        // Load JSON schema
        json schemaJson = json::parse(denigmaContext.mnxSchema.value_or(std::string(MNX_SCHEMA)));
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schemaJson);
        validator.validate(context->mnxDocument);
    } catch (const std::invalid_argument& e) {
        denigmaContext.logMessage(LogMsg() << "Invalid argument: " << e.what(), LogSeverity::Warning);
    }

    std::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.open(outputPath, std::ios::out | std::ios::binary);
    file << context->mnxDocument.dump(denigmaContext.indentSpaces.value_or(-1));
    file.close();
}

} // namespace mnx
} // namespace denigma
