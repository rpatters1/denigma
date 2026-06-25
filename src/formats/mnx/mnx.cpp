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
#include <ostream>
#include <unordered_map>

#include "mnx.h"
#include "core/musx_reader.h"
#include "utils/stringutils.h"

using namespace musx::dom;
using namespace musx::factory;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {

musx::util::Logger::LogCallback makeMusxLogCallback(const MnxMusxMappingPtr& context)
{
    return [context](musx::util::Logger::LogLevel logLevel, const std::string& msg) {
        const MessageSeverity severity = [logLevel]() {
            switch (logLevel) {
            default:
            case musx::util::Logger::LogLevel::Info: return MessageSeverity::Info;
            case musx::util::Logger::LogLevel::Warning: return MessageSeverity::Warning;
            case musx::util::Logger::LogLevel::Error: return MessageSeverity::Error;
            case musx::util::Logger::LogLevel::Verbose: return MessageSeverity::Verbose;
            }
        }();
        context->logMessage(LogMsg() << msg, severity);
    };
}

} // namespace

void MnxMusxMapping::logMessage(LogMsg&& msg, MessageSeverity severity)
{
    std::string logEntry;
    if (current.staff > 0 && current.meas > 0) {
        std::string staffName = [&]() -> std::string {
            if (document) {
                if (auto staff = others::StaffComposite::createCurrent(document, SCORE_PARTID, current.staff, current.meas, 0)) {
                    auto instName = staff->getFullInstrumentName();
                    if (!instName.empty()) return instName;
                }
            }
            return "Staff " + std::to_string(current.staff);
        }();

        std::string v;
        if (!current.voice.empty()) {
            v = " " + current.voice;
        }
        logEntry += "[" + staffName + " m" + std::to_string(current.meas) + v + "] ";
    }
    denigmaContext->logMessage(LogMsg() << logEntry << msg.str(), severity);
}

void MnxMusxMapping::logDiscardedCueLayerFrame(LayerIndex layer)
{
    discardedCueFrames++;
    logMessage(LogMsg() << "discarded cue material detected by --cue-layer in measure "
        << current.meas << ", staff " << current.staff << ", layer " << (layer + 1)
        << "; MNX does not currently support cues.");
}

void MnxMusxMapping::logDiscardedHeuristicCueHold()
{
    discardedCueFrames++;
    logMessage(LogMsg() << "discarded cue material detected heuristically in measure "
        << current.meas << ", staff " << current.staff
        << "; MNX does not currently support cues.");
}

void MnxMusxMapping::setCurrentMeasureStaff(const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffCmper)
{
    current.clear();
    current.meas = musxMeasure->getCmper();
    current.staff = staffCmper;
    current.gfhold.emplace(musxMeasure->getDocument(), musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper());
    if (!*current.gfhold) {
        return;
    }

    current.layerVoices = current.gfhold->calcVoices();
    if (const auto cueSummary = current.gfhold->calcCueSummary()) {
        current.cueDiscardPlan.discardWholeHold = true;
        current.cueDiscardPlan.discardLayers.insert(cueSummary.cueLayers.begin(), cueSummary.cueLayers.end());
        logDiscardedHeuristicCueHold();
        return;
    }

    if (denigmaContext->cueLayer) {
        const LayerIndex cueLayer = static_cast<LayerIndex>(*denigmaContext->cueLayer - 1);
        if (current.gfhold.value()->frames[cueLayer] != 0) {
            current.cueDiscardPlan.discardLayers.emplace(cueLayer);
            logDiscardedCueLayerFrame(cueLayer);
        }
    }
}

std::optional<int> MnxMusxMapping::mnxPartStaffFromStaff(StaffCmper staff) const
{
    const auto it = std::find(currPartStaves.begin(), currPartStaves.end(), staff);
    if (it == currPartStaves.end()) {
        return std::nullopt;
    }
    return static_cast<int>(std::distance(currPartStaves.begin(), it) + 1);
}

std::string mnxPartDisplayName(const MnxMusxMappingPtr&, const mnxdom::Part& part)
{
    const std::string partId = part.id_or(std::to_string(part.calcArrayIndex()));
    if (part.name() && !part.name().value().empty()) {
        return part.name().value() + " (" + partId + ")";
    }
    return "Part " + std::to_string(part.calcArrayIndex() + 1) + " (" + partId + ")";
}

std::string mnxPartDisplayName(const MnxMusxMappingPtr& context, const std::string& partId)
{
    auto parts = context->mnxDocument->parts();
    for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex) {
        auto part = parts[partIndex];
        if (part.id() && part.id().value() == partId) {
            return mnxPartDisplayName(context, part);
        }
    }
    return partId;
}

std::string mnxPartDisplayList(const MnxMusxMappingPtr& context, const std::vector<std::string>& partIds)
{
    std::string result;
    for (size_t i = 0; i < partIds.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += mnxPartDisplayName(context, partIds[i]);
    }
    return result;
}

static void createMnx(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto mnx = mnxDocument->mnx();
    auto support = mnx.ensure_support();
    support.set_useBeams(true);

    auto client = mnx.ensure_mnxdom().ensure_client();
    client.set_name(DENIGMA_NAME);
    client.set_version(DENIGMA_VERSION);
    client.set_commit(DENIGMA_GIT_COMMIT);

    const auto& inputFilePath = context->denigmaContext->inputFilePath;
    auto source = mnx.ensure_mnxdom().ensure_source();
    const std::u8string sourceFormat = utils::normalizedPathExtension(inputFilePath);
    source.set_format(sourceFormat.empty() ? std::string("unknown") : utils::utf8ToString(sourceFormat));
    if (!inputFilePath.empty()) {
        source.set_filename(utils::pathToString(inputFilePath.filename()));
    }
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
                calcGlobalMeasureId(mmRest->getStartMeasure()), mmRest->calcNumberOfMeasures());
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
                    auto mnxSystem = mnxSystems.append(calcGlobalMeasureId(system->startMeas));
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
        context->textRepeat2Jump.emplace(def->getCmper(), classify::classifyJump(def));
    }
}

static std::unique_ptr<mnxdom::Document> createMnxDocument(const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    DocumentFactory::CreateOptions::EmbeddedGraphicFiles embeddedGraphicFiles;
    if (!inputData.embeddedGraphics.empty()) {
        embeddedGraphicFiles.reserve(inputData.embeddedGraphics.size());
        for (const auto& graphic : inputData.embeddedGraphics) {
            DocumentFactory::CreateOptions::EmbeddedGraphicFile file;
            file.filename = graphic.filename;
            file.bytes.assign(graphic.blob.begin(), graphic.blob.end());
            embeddedGraphicFiles.emplace_back(std::move(file));
        }
    }
    DocumentFactory::CreateOptions createOptions(
        denigmaContext.inputFilePath,
        inputData.notationMetadata.value_or(Buffer{}),
        std::move(embeddedGraphicFiles));
    auto document = DocumentFactory::create<MusxReader>(inputData.primaryBuffer, std::move(createOptions));
    auto context = std::make_shared<MnxMusxMapping>(denigmaContext, document);
    context->mnxDocument = std::make_unique<mnxdom::Document>();
    context->musxParts = others::PartDefinition::getInUserOrder(document);
    MusxLoggerScope mnxMusxLogger(makeMusxLogCallback(context));

    createMappings(context);   // map repeat text, text exprs, articulations, etc. to semantic values

    createMnx(context);
    createGlobal(context);
    createParts(context);
    finalizeArpeggios(context);
    finalizeJumpTies(context);
    // Split-instrument parts need time-varying layout sources; skip scores/layouts until MNX has a stable model for that.
    if (!denigmaContext.mnxSplitInstruments) {
        createLayouts(context); // must come after createParts
        createScores(context); // must come after createLayouts
    }
    if (context->discardedCueFrames > 0) {
        denigmaContext.logMessage(LogMsg() << "discarded " << context->discardedCueFrames
            << " cue frames because MNX does not currently support cues.");
    }

    return std::move(context->mnxDocument);
}

static void validateMnxDocument(const mnxdom::Document& mnxDocument, const DenigmaContext& denigmaContext)
{
    if (!denigmaContext.noValidate) {
        denigmaContext.logMessage(LogMsg() << "Validation starting.", MessageSeverity::Verbose);
        if (auto validateResult = mnxdom::validation::schemaValidate(mnxDocument, denigmaContext.mnxSchema); !validateResult) {
            denigmaContext.logMessage(LogMsg() << "Schema validation errors:", MessageSeverity::Warning);
            for (const auto& error : validateResult.errors) {
                denigmaContext.logMessage(LogMsg() << "    " << error.to_string(), MessageSeverity::Warning);
            }
        } else {
            denigmaContext.logMessage(LogMsg() << "Schema validation succeeded.");
            if (auto semanticResult = mnxdom::validation::semanticValidate(mnxDocument); !semanticResult) {
                denigmaContext.logMessage(LogMsg() << "Semantic validation errors:", MessageSeverity::Warning);
                for (const auto& error : semanticResult.errors) {
                    denigmaContext.logMessage(LogMsg() << "    " << error.to_string(4), MessageSeverity::Warning);
                }
            } else {
                size_t layoutSize = mnxDocument.layouts() ? mnxDocument.layouts().value().size() : 0;
                denigmaContext.logMessage(LogMsg() << "Semantic validation complete (" << mnxDocument.global().measures().size() << " measures, "
                    << mnxDocument.parts().size() << " parts, " << layoutSize << " layouts).");
            }
        }
    }
}

static void writeMnxDocumentJson(std::ostream& output, const mnxdom::Document& mnxDocument, const DenigmaContext& denigmaContext)
{
    output << mnxDocument.root()->dump(denigmaContext.indentSpaces.value_or(-1));
}

void exportJson(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    MusxLoggerScope musxLogger(makeMusxLogCallback(denigmaContext));
    auto mnxDocument = createMnxDocument(inputData, denigmaContext);
    validateMnxDocument(*mnxDocument, denigmaContext);
    writeMnxDocumentJson(output, *mnxDocument, denigmaContext);
}

void exportJson(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    std::ofstream output;
    output.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    output.open(outputPath, std::ios::out | std::ios::binary);
    exportJson(output, inputData, denigmaContext);
    output.close();
}

void exportMnx(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    // for now mnx and json both export as JSON text files.
    // the final plan is that the Mnx export will export a zip archive, similar to mxl for musicxml
    exportJson(outputPath, inputData, denigmaContext);
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
