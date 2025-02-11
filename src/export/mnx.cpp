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

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

static void createMnx([[maybe_unused]] const MnxMusxMappingPtr& context)
{
}

static void createGlobal([[maybe_unused]] const MnxMusxMappingPtr& context)
{
}

static void createParts(const MnxMusxMappingPtr& context)
{
    auto multiStaffInsts = context->document->getOthers()->getArray<others::MultiStaffInstrumentGroup>(SCORE_PARTID);
    auto scrollView = context->document->getOthers()->getArray<others::InstrumentUsed>(SCORE_PARTID, BASE_SYSTEM_ID);
    int partNumber = 0;
    auto parts = context->mnxDocument->parts();
    for (const auto& item : scrollView) {
        auto staff = item->getStaff();
        auto multiStaffInst = staff->getMultiStaffInstGroup();
        if (multiStaffInst && context->inst2Part.find(staff->getCmper()) != context->inst2Part.end()) {
            continue;
        }
        std::string id = "P" + std::to_string(++partNumber);
        auto part = parts.append();
        part.set_id(id);
        auto fullName = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (!fullName.empty()) {
            part.set_name(fullName);
        }
        auto abrvName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (!abrvName.empty()) {
            part.set_shortName(abrvName);
        }
        if (multiStaffInst) {
            part.set_staves(multiStaffInst->staffNums.size());
            for (auto inst : multiStaffInst->staffNums) {
                context->inst2Part.emplace(inst, id);
            }
            context->part2Inst.emplace(id, multiStaffInst->staffNums);
        } else {
            context->inst2Part.emplace(staff->getCmper(), id);
            context->part2Inst.emplace(id, std::vector<InstCmper>({ InstCmper(staff->getCmper()) }));
        }
    }
}

static void createScores(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto musxLinkedParts = others::PartDefinition::getInUserOrder(context->document);
    for (const auto& linkedPart : musxLinkedParts) {
        if (!mnxDocument->scores()) {
            mnxDocument->create_scores();
        }
        auto mnxScore = mnxDocument->scores().value().append(linkedPart->getName(EnigmaString::AccidentalStyle::Unicode));
        if (mnxScore.name().empty()) {
            mnxScore.set_name(linkedPart->isScore()
                              ? std::string("Score")
                              : std::string("Part ") + std::to_string(linkedPart->getCmper()));
        }
        mnxScore.set_layout(calcSystemLayoutId(linkedPart->getCmper(), BASE_SYSTEM_ID));
        auto mmRests = context->document->getOthers()->getArray<others::MultimeasureRest>(linkedPart->getCmper());
        for (const auto& mmRest : mmRests) {
            if (!mnxScore.multimeasureRests()) {
                mnxScore.create_multimeasureRests();
            }
            auto mnxMmRest = mnxScore.multimeasureRests().value().append(mmRest->getStartMeasure(), mmRest->calcNumberOfMeasures());
            if (!mmRest->calcIsNumberVisible()) {
                mnxMmRest.set_label("");
            }
        }
        auto pages = context->document->getOthers()->getArray<others::Page>(linkedPart->getCmper());
        auto baseIuList = linkedPart->calcSystemIuList(BASE_SYSTEM_ID);
        for (size_t x = 0; x < pages.size(); x++) {
            if (!mnxScore.pages()) {
                mnxScore.create_pages();
            }
            auto mnxPage = mnxScore.pages().value().append();
            auto mnxSystems = mnxPage.systems();
            auto page = pages[x];
            if (!page->isBlank() && page->lastSystem.has_value()) {
                for (SystemCmper sysId = page->firstSystem; sysId <= *page->lastSystem; sysId++) {
                    auto system = context->document->getOthers()->get<others::StaffSystem>(linkedPart->getCmper(), sysId);
                    if (!system) {
                        throw std::logic_error("System " + std::to_string(sysId) + " on page " + std::to_string(page->getCmper())
                            + " in part " + linkedPart->getName() + " does not exist.");
                    }
                    auto mnxSystem = mnxSystems.append(system->startMeas);
                    if (linkedPart->calcSystemIuList(sysId) == baseIuList) {
                        mnxSystem.set_layout(calcSystemLayoutId(linkedPart->getCmper(), BASE_SYSTEM_ID));
                    } else {
                        mnxSystem.set_layout(calcSystemLayoutId(linkedPart->getCmper(), sysId));
                    }
                }
            }
        }
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
    context->mnxDocument = std::make_unique<mnx::Document>();

    createMnx(context);
    createGlobal(context);
    createParts(context);
    createLayouts(context); // must come after createParts
    createScores(context); // must come after createLayouts
    
    if (auto validationErr = context->mnxDocument->validate(denigmaContext.mnxSchema)) {
        denigmaContext.logMessage(LogMsg() << "Validation error: " << validationErr.value(), LogSeverity::Warning);
    }
    context->mnxDocument->save(outputPath, denigmaContext.indentSpaces.value_or(-1));
}

} // namespace mnxexp
} // namespace denigma
