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
#include "utils/smufl_support.h"

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

std::optional<int> MnxMusxMapping::mnxPartStaffFromStaff(StaffCmper staff) const
{
    const auto it = std::find(currPartStaves.begin(), currPartStaves.end(), staff);
    if (it == currPartStaves.end()) {
        return std::nullopt;
    }
    return static_cast<int>(std::distance(currPartStaves.begin(), it) + 1);
}

static void createMnx(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto support = mnxDocument->mnx().ensure_support();
    support.set_useBeams(true);
}

static void createScores(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    for (const auto& linkedPart : context->musxParts) {
        auto partGlobals = context->document->getOthers()->get<others::PartGlobals>(linkedPart->getCmper(), MUSX_GLOBALS_CMPER);
        auto mnxScore = mnxDocument->ensure_scores().append(
            linkedPart->getName(EnigmaString::AccidentalStyle::Unicode));
        if (mnxScore.name().empty()) {
            mnxScore.set_name(linkedPart->isScore()
                              ? std::string("Score")
                              : std::string("Part ") + std::to_string(linkedPart->getCmper()));
        }
        mnxScore.set_layout(calcSystemLayoutId(linkedPart->getCmper(), BASE_SYSTEM_ID));
        auto mmRests = context->document->getOthers()->getArray<others::MultimeasureRest>(linkedPart->getCmper());
        for (const auto& mmRest : mmRests) {
            auto mnxMmRest = mnxScore.ensure_multimeasureRests().append(
                mmRest->getStartMeasure(), mmRest->calcNumberOfMeasures());
            if (!mmRest->calcIsNumberVisible()) {
                mnxMmRest.set_label("");
            }
        }
        auto pages = context->document->getOthers()->getArray<others::Page>(linkedPart->getCmper());
        for (size_t x = 0; x < pages.size(); x++) {
            auto mnxPage = mnxScore.ensure_pages().append();
            auto mnxSystems = mnxPage.systems();
            auto page = pages[x];
            if (!page->isBlank() && page->lastSystemId.has_value()) {
                for (SystemCmper sysId = page->firstSystemId; sysId <= page->lastSystemId.value(); sysId++) {
                    auto system = context->document->getOthers()->get<others::StaffSystem>(linkedPart->getCmper(), sysId);
                    if (!system) {
                        throw std::logic_error("System " + std::to_string(sysId) + " on page " + std::to_string(page->getCmper())
                            + " in part " + linkedPart->getName() + " does not exist.");
                    }
                    auto mnxSystem = mnxSystems.append(system->startMeas);
                    mnxSystem.set_layout(calcSystemLayoutId(linkedPart->getCmper(), sysId));
                }
            }
        }
        if (partGlobals && !partGlobals->showTransposed) {
            mnxScore.set_useWritten(true);
        }
    }
}

static void createMappings(const MnxMusxMappingPtr& context)
{
    auto textRepeatDefs = context->document->getOthers()->getArray<others::TextRepeatDef>(SCORE_PARTID);
    for (const auto& def : textRepeatDefs) {
        if (auto repeatText = context->document->getOthers()->get<others::TextRepeatText>(SCORE_PARTID, def->getCmper())) {
            auto glyphName = utils::smuflGlyphNameForFont(def->font, repeatText->text);
            context->textRepeat2Jump.emplace(def->getCmper(), convertTextToJump(repeatText->text, glyphName));
        }
    }
}

void exportJson(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << outputPath.u8string());
        return;
    }
#endif
    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    musx::factory::DocumentFactory::CreateOptions createOptions;
    if (inputData.notationMetadata.has_value()) {
        createOptions.setNotationMetadata(*inputData.notationMetadata);
    }
    if (!inputData.embeddedGraphics.empty()) {
        musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFiles embeddedGraphicFiles;
        embeddedGraphicFiles.reserve(inputData.embeddedGraphics.size());
        for (const auto& graphic : inputData.embeddedGraphics) {
            musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFile file;
            file.filename = graphic.filename;
            file.bytes.assign(graphic.blob.begin(), graphic.blob.end());
            embeddedGraphicFiles.emplace_back(std::move(file));
        }
        createOptions.setEmbeddedGraphics(std::move(embeddedGraphicFiles));
    }
    auto document = musx::factory::DocumentFactory::create<MusxReader>(inputData.primaryBuffer, std::move(createOptions));
    auto context = std::make_shared<MnxMusxMapping>(denigmaContext, document);
    context->mnxDocument = std::make_unique<mnx::Document>();
    context->musxParts = others::PartDefinition::getInUserOrder(document);

    createMappings(context);   // map repeat text, text exprs, articulations, etc. to semantic values

    createMnx(context);
    createGlobal(context);
    createParts(context);
    finalizeJumpTies(context);
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
                    denigmaContext.logMessage(LogMsg() << "    " << error.to_string(4), LogSeverity::Warning);
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

void exportMnx(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    // for now mnx and json both export as JSON text files.
    // the final plan is that the Mnx export will export a zip archive, similar to mxl for musicxml
    exportJson(outputPath, inputData, denigmaContext);
}

} // namespace mnxexp
} // namespace denigma
