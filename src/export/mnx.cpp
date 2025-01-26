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

#include "nlohmann/json.hpp"
#include "nlohmann/json-schema.hpp"
#include "mnx_schema.xxd"

#include "musx/musx.h"
#include "denigma.h"

static constexpr int MNX_VERSION_NUMBER = 1;
static const std::string_view MNX_SCHEMA(reinterpret_cast<const char *>(mnx_schema_json), mnx_schema_json_len);

using namespace musx::dom;
using namespace musx::util;
using json = nlohmann::ordered_json;
//using json = nlohmann::json;

namespace denigma {
namespace mnx {

struct MnxMusxMapping
{
    MnxMusxMapping(const DenigmaContext& context, const DocumentPtr& doc)
        : denigmaContext(&context), document(doc) {}

    const DenigmaContext* denigmaContext;
    DocumentPtr document;
    
    std::unordered_map<std::string, std::vector<InstCmper>> part2Inst;
    std::unordered_map<InstCmper, std::string> inst2Part;
};
using MnxMusxMappingPtr = std::shared_ptr<MnxMusxMapping>;

json createMnx([[maybe_unused]] const MnxMusxMappingPtr& context)
{
    auto musxObject = json::object();
    musxObject["version"] = MNX_VERSION_NUMBER;
    return musxObject;
}

json createGlobal([[maybe_unused]] const MnxMusxMappingPtr& context)
{
    auto globalObject = json::object();
    globalObject["measures"] = json::array();
    return globalObject;
}

json createParts(const MnxMusxMappingPtr& context)
{
    /// @todo figure out mulitstaff instruments
    auto partsObject = json::array();
    auto scrollView = context->document->getOthers()->getArray<others::InstrumentUsed>(SCORE_PARTID, SCROLLVIEW_IULIST);
    for (const auto& item : scrollView) {
        auto staff = item->getStaff();
        std::string id = "P" + std::to_string(staff->getCmper());
        auto part = json::object();
        part["id"] = id;
        part["name"] = staff->getFullName(EnigmaString::AccidentalStyle::Unicode);
        if (part["name"] == "") {
            part.erase("name");
        }
        part["shortName"] = staff->getAbbreviatedName(EnigmaString::AccidentalStyle::Unicode);
        if (part["shortName"] == "") {
            part.erase("shortName");
        }
        /// @todo figure out the correct number of staves using groups and mulitstaff instruments
        //part["staves"] = 1; (default)
        partsObject.push_back(part);
        context->inst2Part.emplace(staff->getCmper(), id);
        context->part2Inst.emplace(id, std::vector<InstCmper>({ InstCmper(staff->getCmper()) }));
    }
    return partsObject;
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

    json mnxDocument;
    mnxDocument["mnx"] = createMnx(context);
    mnxDocument["global"] = createGlobal(context);
    mnxDocument["parts"] = createParts(context);

    // validate the result
    try {
        // Load JSON schema
        json schemaJson = json::parse(denigmaContext.mnxSchema.value_or(std::string(MNX_SCHEMA)));
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schemaJson);
        validator.validate(mnxDocument);
    } catch (const std::invalid_argument& e) {
        denigmaContext.logMessage(LogMsg() << "Invalid argument: " << e.what(), LogSeverity::Warning);
    }

    std::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.open(outputPath, std::ios::out | std::ios::binary);
    file << mnxDocument.dump(denigmaContext.indentSpaces.value_or(-1));
    file.close();
}

} // namespace mnx
} // namespace denigma
