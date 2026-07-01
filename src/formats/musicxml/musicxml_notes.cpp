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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "musicxml.h"

#include <cstddef>
#include <cstdint>

#include "core/cue_layers.h"
#include "mx/api/DurationData.h"
#include "mx/api/NoteData.h"
#include "mx/api/PitchData.h"
#include "mx/api/PrintData.h"
#include "mx/api/VoiceData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

mx::api::DurationData createDurationData(
    const MusicXmlMusxMapping& context,
    const MusxInstance<Entry>& entry,
    const Fraction& actualDuration)
{
    auto duration = mx::api::DurationData{};
    const auto [durationName, dots] = entry->calcDurationInfo();
    duration.durationName = enumConvert<mx::api::DurationName>(durationName);
    duration.durationDots = int(dots);
    duration.durationTimeTicks = context.timing.calcMusicXmlDivisions(actualDuration);
    return duration;
}

mx::api::NoteData createFullMeasureRestData(
    const MusicXmlMusxMapping& context,
    const MusxInstance<others::Measure>& musxMeasure)
{
    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.isMeasureRest = true;
    rest.durationData.durationName = mx::api::DurationName::whole;
    rest.durationData.durationTimeTicks = context.timing.calcMusicXmlDivisions(musxMeasure->calcDuration());
    return rest;
}

void addFullMeasureRest(
    const MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure)
{
    auto rest = createFullMeasureRestData(context, musxMeasure);
    rest.userRequestedVoiceNumber = 1;

    auto& voice = staff.voices[0];
    voice.notes.emplace_back(rest);
}

MusicXmlPitchContext pitchContextForStaff(const MusicXmlMusxMapping& context, StaffCmper staffId)
{
    const auto partIt = context.staffToPartId.find(staffId);
    if (partIt == context.staffToPartId.end()) {
        return MusicXmlPitchContext::Written;
    }
    return pitchContextForPart(context, partIt->second);
}

mx::api::PitchData createPitchData(const NoteInfoPtr& noteInfo, MusicXmlPitchContext pitchContext)
{
    const auto [noteName, octave, alteration, staffPosition] = [&]() {
        if (pitchContext == MusicXmlPitchContext::Concert) {
            return noteInfo.calcNotePropertiesConcert();
        }
        return noteInfo.calcNoteProperties();
    }();
    (void)staffPosition;
    return mx::api::PitchData(enumConvert<mx::api::Step>(noteName), alteration, octave);
}

std::uint64_t noteKey(const NoteInfoPtr& noteInfo)
{
    return (std::uint64_t(noteInfo.getEntryInfo()->getEntry()->getEntryNumber()) << 32) | std::uint64_t(noteInfo.getNoteIndex());
}

void applyMusicXmlTies(MusicXmlMusxMapping& context, mx::api::NoteData& note, const NoteInfoPtr& noteInfo)
{
    if (context.pendingTieStopKeys.erase(noteKey(noteInfo)) > 0) {
        note.isTieStop = true;
    }

    if (!noteInfo->tieStart) {
        return;
    }
    const auto tiedTo = noteInfo.calcTieTo();
    if (!tiedTo || tiedTo.getEntryInfo()->getEntry()->isHidden) {
        return;
    }
    note.isTieStart = true;
    context.pendingTieStopKeys.emplace(noteKey(tiedTo));
}

mx::api::NoteData createRestData(
    const MusicXmlMusxMapping& context,
    const EntryInfoPtr::InterpretedIterator& entryIt)
{
    const auto entryInfo = entryIt.getEntryInfo();
    const auto entry = entryInfo->getEntry();

    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.tickTimePosition = context.timing.calcMusicXmlDivisions(entryIt.getEffectiveElapsedDuration(/*global*/ true));
    rest.durationData = createDurationData(context, entry, entryIt.getEffectiveActualDuration(/*global*/ true));
    if (entryIt.getEffectiveHidden()) {
        rest.printData.printObject = mx::api::Bool::no;
    }
    return rest;
}

void appendEntryNotes(
    MusicXmlMusxMapping& context,
    mx::api::VoiceData& voice,
    const EntryInfoPtr::InterpretedIterator& entryIt,
    const MusxInstance<others::Measure>& musxMeasure,
    int userVoiceNumber,
    MusicXmlPitchContext pitchContext)
{
    const auto entryInfo = entryIt.getEntryInfo();
    const auto entry = entryInfo->getEntry();
    if (entryInfo.calcIsFullMeasureRest()) {
        auto rest = createFullMeasureRestData(context, musxMeasure);
        rest.userRequestedVoiceNumber = userVoiceNumber;
        voice.notes.emplace_back(rest);
        return;
    }
    if (entryInfo.calcDisplaysAsRest()) {
        auto rest = createRestData(context, entryIt);
        rest.userRequestedVoiceNumber = userVoiceNumber;
        voice.notes.emplace_back(rest);
        return;
    }

    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
        NoteInfoPtr noteInfo(entryInfo, noteIndex);
        auto note = mx::api::NoteData{};
        note.isChord = noteIndex > 0;
        note.noteType = entry->graceNote ? mx::api::NoteType::grace : mx::api::NoteType::normal;
        note.userRequestedVoiceNumber = userVoiceNumber;
        note.tickTimePosition = context.timing.calcMusicXmlDivisions(entryIt.getEffectiveElapsedDuration(/*global*/ true));
        note.durationData = createDurationData(context, entry, entryIt.getEffectiveActualDuration(/*global*/ true));
        note.pitchData = createPitchData(noteInfo, pitchContext);
        applyMusicXmlTies(context, note, noteInfo);
        if (entryIt.getEffectiveHidden()) {
            note.printData.printObject = mx::api::Bool::no;
        }
        voice.notes.emplace_back(note);
    }
}

} // namespace

void createNotesForMeasureStaff(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex)
{
    (void)measure;
    (void)staffIndex;

    const Fraction legacyPickupSpacer = musxMeasure->calcMinLegacyPickupSpacer(staffId);
    musx::dom::details::GFrameHoldContext gfHold(context.document, context.forPartId, staffId, musxMeasure->getCmper(), legacyPickupSpacer);
    if (!gfHold) {
        addFullMeasureRest(context, staff, musxMeasure);
        return;
    }

    bool emittedNotes = false;
    const auto pitchContext = pitchContextForStaff(context, staffId);
    /// @todo Export cue notes/rests instead of omitting cue layers.
    const auto cueLayerPlan = createCueLayerPlan(gfHold, context.denigmaContext->cueLayer);
    for (const auto& [layer, numVoice2Entries] : gfHold.calcVoices()) {
        if (cueLayerPlan.skipsLayer(layer)) {
            continue;
        }
        const int maxVoice = numVoice2Entries ? 2 : 1;
        const auto entryFrame = gfHold.createEntryFrame(layer);
        ASSERT_IF(!entryFrame) {
            continue;
        }
        for (int voiceNumber = 1; voiceNumber <= maxVoice; ++voiceNumber) {
            const int userVoiceNumber = (int(layer) * 2) + voiceNumber;
            auto& voice = staff.voices[userVoiceNumber - 1];
            for (auto entryIt = entryFrame->getFirstInterpretedIterator(voiceNumber); entryIt; entryIt = entryIt.getNext()) {
                if (entryIt.calcIsPastLogicalEndOfFrame()) {
                    break;
                }
                appendEntryNotes(context, voice, entryIt, musxMeasure, userVoiceNumber, pitchContext);
                emittedNotes = true;
            }
        }
    }

    if (!emittedNotes) {
        addFullMeasureRest(context, staff, musxMeasure);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
