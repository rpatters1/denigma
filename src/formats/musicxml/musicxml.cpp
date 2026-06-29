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

#include <algorithm>
#include <ostream>
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

namespace {

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
    int baseDivisions = 1; // default to 1 division per quarter
    context.document->iterateEntries(context.forPartId, [&](const EntryInfoPtr& entryInfo) -> bool {
        auto quarterDuration = entryInfo.calcGlobalActualDuration() * 4;
        baseDivisions = utils::checkedLcm(baseDivisions, quarterDuration.denominator());
        return true;
    });
    timing.divisions = baseDivisions;
}

void createPlaceholderMeasure(MusicXmlMusxMapping& context, mx::api::PartData& part, bool addHelloWorldNote)
{
    using namespace mx::api;

    part.measures.emplace_back(MeasureData{});
    auto& measure = part.measures.back();
    measure.timeSignature.beats = "4";
    measure.timeSignature.beatType = "4";
    measure.timeSignature.isImplicit = false;

    size_t staffCount = 1;
    if (const auto stavesIt = context.partIdToStaves.find(part.uniqueId); stavesIt != context.partIdToStaves.end()) {
        staffCount = (std::max)(staffCount, stavesIt->second.size());
    }
    if (const auto partSymbolIt = context.partIdToPartSymbol.find(part.uniqueId); partSymbolIt != context.partIdToPartSymbol.end()) {
        measure.partSymbol = partSymbolIt->second;
    }

    measure.staves.resize(staffCount);
    for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
        auto clef = ClefData{};
        if (staffIndex == 1) {
            clef.setBass();
        } else {
            clef.setTreble();
        }
        measure.staves[staffIndex].clefs.emplace_back(clef);
    }

    if (!addHelloWorldNote) {
        return;
    }

    measure.staves.front().voices[0] = VoiceData{};
    auto& voice = measure.staves.front().voices.at(0);

    NoteData note;
    note.pitchData.step = Step::c;
    note.pitchData.alter = 0;
    note.pitchData.octave = 4;
    note.pitchData.accidental = Accidental::none;
    note.durationData.durationName = DurationName::whole;
    note.durationData.durationTimeTicks = context.timing.calcMusicXmlDivisions(1);
    note.tickTimePosition = 0;
    voice.notes.push_back(note);
}

mx::api::ScoreData createMusicXmlDocument(const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    auto document = denigma::createMusxDocument<MusxReader>(inputData, denigmaContext);
    auto context = MusicXmlMusxMapping(denigmaContext, document, SCORE_PARTID);
    context.musicXmlScore = std::make_unique<mx::api::ScoreData>();

    createTiming(context, context.timing);
    createMetaData(context);
    createDefaults(context);
    createParts(context);

    using namespace mx::api;

    // placeholder output
    auto& score = *context.musicXmlScore;
    if (score.parts.empty()) {
        auto& fallbackPart = score.parts.emplace_back(PartData{});
        fallbackPart.uniqueId = "P1";
        fallbackPart.instrumentData.uniqueId = "P1-I1";
    }
    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        createPlaceholderMeasure(context, score.parts[partIndex], partIndex == 0);
    }

    score.sort();
    return score;
}

} // namespace

void exportMusicXml(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    MusxLoggerScope musxLogger(makeMusxLogCallback(denigmaContext));
    auto& documentManager = mx::api::DocumentManager::getInstance();

    const auto idResult = documentManager.createFromScore(createMusicXmlDocument(inputData, denigmaContext));
    if (!idResult.ok()) {
        throw std::runtime_error(mxResultMessage("createFromScore", idResult.error()));
    }

    const int documentId = idResult.value();
    const auto writeResult = documentManager.writeToStream(documentId, output);
    documentManager.destroyDocument(documentId);
    if (!writeResult.ok()) {
        throw std::runtime_error(mxResultMessage("writeToStream", writeResult.error()));
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
