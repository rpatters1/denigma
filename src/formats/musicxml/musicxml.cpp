/*
 * Copyright (C) 2026, Robert Patterson
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

#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "musicxml.h"
#include "core/musx_reader.h"
#include "utils/mathutils.h"

#include "mx/api/DocumentManager.h"
#include "mx/api/ScoreData.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

std::string mxResultMessage(std::string_view operation, const mx::api::ApiError& error)
{
    std::string result = "mx " + std::string(operation) + " failed with result code "
        + std::to_string(static_cast<int>(error.code));
    if (!error.message.empty()) {
        result += ": " + error.message;
    }
    if (!error.path.empty()) {
        result += " at " + error.path;
    }
    return result;
}

void createTiming(const MusicXmlMusxMapping& context, MusicXmlTimingPlan& timing)
{
    constexpr int MIN_DIVISIONS_PER_QUARTER = 8;
    int baseDivisions = MIN_DIVISIONS_PER_QUARTER;
    for (const auto& measure : context.document->getOthers()->getArray<others::Measure>(context.forPartId)) {
        const auto quarterDuration = measure->calcDuration() * 4;
        baseDivisions = utils::checkedLcm(baseDivisions, quarterDuration.denominator());
    }
    context.document->iterateEntries(context.forPartId, [&](const EntryInfoPtr& entryInfo) -> bool {
        auto quarterDuration = entryInfo.calcGlobalActualDuration() * 4;
        baseDivisions = utils::checkedLcm(baseDivisions, quarterDuration.denominator());
        return true;
    });
    timing.divisions = baseDivisions;
}

namespace {

mx::api::ScoreData createMusicXmlDocumentFromDocument(
    const musx::dom::DocumentPtr& document,
    const DenigmaContext& denigmaContext,
    const MusxInstance<others::PartDefinition>& part)
{
    auto context = MusicXmlMusxMapping(denigmaContext, document, part ? part->getCmper() : SCORE_PARTID);
    context.musicXmlScore = std::make_unique<mx::api::ScoreData>();

    createTiming(context, context.timing);
    createMetaData(context);
    createDefaults(context);
    createPageTexts(context);
    createParts(context);
    createMeasures(context);

    context.musicXmlScore->sort();
    return *context.musicXmlScore;
}

std::string partOutputName(const DenigmaContext& denigmaContext, const MusxInstance<others::PartDefinition>& part)
{
    if (!part) {
        return {};
    }
    auto partName = part->getName(); // Unicode-encoded partname can contain non-ASCII characters
    if (partName.empty()) {
        partName = "Part" + std::to_string(part->getCmper());
        denigmaContext.logMessage(LogMsg() << "No part name found. Using " << partName << " for part name extension");
    }
    return partName;
}

void writeMusicXmlToCallback(
    const mx::api::ScoreData& score,
    const std::string& suggestedName,
    const MultiOutputCallback& outputCallback)
{
    auto& documentManager = mx::api::DocumentManager::getInstance();

    const auto idResult = documentManager.createFromScore(score);
    if (!idResult.ok()) {
        throw std::runtime_error(mxResultMessage("createFromScore", idResult.error()));
    }

    const int documentId = idResult.value();
    std::ostringstream output;
    const auto writeResult = documentManager.writeToStream(documentId, output);
    documentManager.destroyDocument(documentId);
    if (!writeResult.ok()) {
        throw std::runtime_error(mxResultMessage("writeToStream", writeResult.error()));
    }

    const auto data = output.str();
    outputCallback(suggestedName, std::as_bytes(std::span<const char>(data.data(), data.size())));
}

} // namespace

mx::api::ScoreData createMusicXmlDocument(
    const CommandInputData& inputData,
    const DenigmaContext& denigmaContext,
    const MusxInstance<others::PartDefinition>& part)
{
    auto document = denigma::createMusxDocument<MusxReader>(inputData, denigmaContext, musx::dom::PartVoicingPolicy::Apply);
    return createMusicXmlDocumentFromDocument(document, denigmaContext, part);
}

void convert(
    const CommandInputData& inputData,
    const DenigmaContext& denigmaContext,
    const MultiOutputCallback& outputCallback)
{
    MusxLoggerScope musxLogger(makeMusxLogCallback(denigmaContext));
    auto document = denigma::createMusxDocument<MusxReader>(inputData, denigmaContext, musx::dom::PartVoicingPolicy::Apply);

    if (denigmaContext.allPartsAndScore || !denigmaContext.partName.has_value()) {
        const auto score = createMusicXmlDocumentFromDocument(document, denigmaContext, nullptr);
        writeMusicXmlToCallback(score, partOutputName(denigmaContext, nullptr), outputCallback);
    }
    bool foundPart = false;
    if (denigmaContext.allPartsAndScore || denigmaContext.partName.has_value()) {
        auto parts = document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
        for (const auto& part : parts) {
            if (part->getCmper() != SCORE_PARTID) {
                if (denigmaContext.allPartsAndScore) {
                    const auto partScore = createMusicXmlDocumentFromDocument(document, denigmaContext, part);
                    writeMusicXmlToCallback(partScore, partOutputName(denigmaContext, part), outputCallback);
                } else if (denigmaContext.partName->empty() || part->getName().rfind(denigmaContext.partName.value(), 0) == 0) {
                    const auto partScore = createMusicXmlDocumentFromDocument(document, denigmaContext, part);
                    writeMusicXmlToCallback(partScore, partOutputName(denigmaContext, part), outputCallback);
                    foundPart = true;
                    break;
                }
            }
        }
    }
    if (!foundPart && denigmaContext.partName.has_value() && !denigmaContext.allPartsAndScore) {
        if (denigmaContext.partName->empty()) {
            denigmaContext.logMessage(LogMsg() << "No parts were found in document", MessageSeverity::Warning);
        } else {
            denigmaContext.logMessage(LogMsg() << "No part name starting with \"" << denigmaContext.partName.value() << "\" was found", MessageSeverity::Warning);
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
