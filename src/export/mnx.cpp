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

void MnxMusxMapping::logMessage(LogMsg&& msg, LogSeverity severity)
{
    std::string logEntry;
    if (currStaff > 0 && currMeas > 0) {
        std::string staffName = [&]() -> std::string {
            if (document) {
                if (auto staff = others::StaffComposite::createCurrent(document, SCORE_PARTID, currStaff, currMeas, 0)) {
                    auto instName = staff->getFullInstrumentName();
                    if (!instName.empty()) return instName;
                }
            }
            return "Staff " + std::to_string(currStaff);
        }();

        std::string v;
        if (!voice.empty()) {
            v = " " + voice;
        }
        logEntry += "[" + staffName + " m" + std::to_string(currMeas) + v + "] ";
    }
    denigmaContext->logMessage(LogMsg() << logEntry << msg.str(), severity);
}

static void createMnx(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    if (!mnxDocument->mnx().support()) {
        mnxDocument->mnx().create_support();
    }
    auto support = mnxDocument->mnx().support().value();
    support.set_useBeams(true);
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

static void createMappings(const MnxMusxMappingPtr& context)
{
    auto textRepeatDefs = context->document->getOthers()->getArray<others::TextRepeatDef>(SCORE_PARTID);
    for (const auto& def : textRepeatDefs) {
        if (auto repeatText = context->document->getOthers()->get<others::TextRepeatText>(SCORE_PARTID, def->getCmper())) {
            FontType fontType = convertFontToType(def->font);
            context->textRepeat2Jump.emplace(def->getCmper(), convertTextToJump(repeatText->text, fontType));
        }
    }
}

void exportJson(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
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
    if (auto fontOptions = document->getOptions()->get<options::FontOptions>()) {
        context->defaultMusicFont = fontOptions->getFontInfo(options::FontOptions::FontType::Music);
    } else {
        throw std::invalid_argument("Musx document contains no font options.");
    }

    createMappings(context);   // map repeat text, text exprs, articulations, etc. to semantic values

    createMnx(context);
    createGlobal(context);
    createParts(context);
    createLayouts(context); // must come after createParts
    createScores(context); // must come after createLayouts

    if (!denigmaContext.noValidate) {
        denigmaContext.logMessage(LogMsg() << "Validation starting.", LogSeverity::Verbose);
        if (auto validateResult = mnx::validation::schemaValidate(*context->mnxDocument, denigmaContext.mnxSchema); !validateResult) {
            denigmaContext.logMessage(LogMsg() << "Schema validation errors:", LogSeverity::Warning);
            for (const auto& error : validateResult.errors) {
                denigmaContext.logMessage(LogMsg() << "    " << error.to_string(), LogSeverity::Warning);
            }
        } else {
            denigmaContext.logMessage(LogMsg() << "Schema validation succeeded.");
            if (auto semanticResult = mnx::validation::semanticValidate(*context->mnxDocument); !semanticResult) {
                denigmaContext.logMessage(LogMsg() << "Semantic validation errors:", LogSeverity::Warning);
                for (const auto& error : semanticResult.errors) {
                    denigmaContext.logMessage(LogMsg() << "    " << error.to_string(), LogSeverity::Warning);
                }
            } else {
                size_t layoutSize = context->mnxDocument->layouts() ? context->mnxDocument->layouts().value().size() : 0;
                denigmaContext.logMessage(LogMsg() << "Semantic validation complete (" << context->mnxDocument->global().measures().size() << " measures, "
                    << context->mnxDocument->parts().size() << " parts, " << layoutSize << " layouts).");
            }
        }
        context->mnxDocument->save(outputPath, denigmaContext.indentSpaces.value_or(-1));
    }
}

void exportMnx(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
    // for now mnx and json both export as JSON text files.
    // the final plan is that the Mnx export will export a zip archive, similar to mxl for musicxml
    exportJson(outputPath, xmlBuffer, denigmaContext);
}

} // namespace mnxexp
} // namespace denigma
