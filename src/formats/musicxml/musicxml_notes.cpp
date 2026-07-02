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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

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
    const EntryInfoPtr& entryInfo,
    const Fraction& actualDuration)
{
    auto duration = mx::api::DurationData{};
    const auto [durationName, dots] = [&]() {
        for (size_t tupletIndex : entryInfo.findTupletInfo()) {
            const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
            if (tupletInfo.calcIsTremolo()) {
                const auto entryCount = tupletInfo.numEntries();
                ASSERT_IF(entryCount == 0) {
                    throw std::logic_error("Tremolo tuplet contains no entries.");
                }
                return calcDurationInfoFromEdu((tupletInfo.tuplet->calcReferenceDuration() / entryCount).calcEduDuration());
            }
        }
        return entryInfo->getEntry()->calcDurationInfo();
    }();
    duration.durationName = enumConvert<mx::api::DurationName>(durationName);
    duration.durationDots = int(dots);
    duration.durationTimeTicks = context.timing.calcMusicXmlDivisions(actualDuration);
    return duration;
}

bool shouldExportAsTuplet(const EntryFrame::TupletInfo& tupletInfo)
{
    if (tupletInfo.tuplet->calcRatio() == 0) {
        /// @todo Decide whether any non-singleton zero-length tuplets need MusicXML representation.
        return false;
    }
    if (tupletInfo.calcIsTremolo()) {
        /// @todo Export MusicXML double-note tremolos when mx::api can model <tremolo type="start|stop">.
        return false;
    }
    return true;
}

mx::api::TupletStart createTupletStart(const EntryFrame::TupletInfo& tupletInfo, int numberLevel)
{
    const auto& tupletDef = tupletInfo.tuplet;
    const auto [actualDurationName, actualDots] = calcDurationInfoFromEdu(tupletDef->displayDuration);
    const auto [normalDurationName, normalDots] = calcDurationInfoFromEdu(tupletDef->referenceDuration);

    auto result = mx::api::TupletStart{};
    result.numberLevel = numberLevel;
    result.actualNumber = tupletDef->displayNumber;
    result.actualDurationName = enumConvert<mx::api::DurationName>(actualDurationName);
    result.actualDots = int(actualDots);
    result.normalNumber = tupletDef->referenceNumber;
    result.normalDurationName = enumConvert<mx::api::DurationName>(normalDurationName);
    result.normalDots = int(normalDots);
    if (tupletDef->hidden) {
        result.bracket = mx::api::Bool::no;
        result.showActualNumber = mx::api::Bool::no;
        result.showNormalNumber = mx::api::Bool::no;
        return result;
    }
    result.bracket = [&]() {
        if (tupletDef->brackStyle == details::TupletDef::BracketStyle::Nothing) {
            return mx::api::Bool::no;
        }
        if (tupletDef->autoBracketStyle == details::TupletDef::AutoBracketStyle::Always) {
            return mx::api::Bool::yes;
        }
        return mx::api::Bool::unspecified;
    }();
    std::tie(result.showActualNumber, result.showNormalNumber) = [&]() {
        switch (tupletDef->numStyle) {
        case details::TupletDef::NumberStyle::Nothing: return std::pair{mx::api::Bool::no, mx::api::Bool::no};
        case details::TupletDef::NumberStyle::Number: return std::pair{mx::api::Bool::yes, mx::api::Bool::no};
        case details::TupletDef::NumberStyle::UseRatio:
        case details::TupletDef::NumberStyle::RatioPlusDenominatorNote:
        case details::TupletDef::NumberStyle::RatioPlusBothNotes:
            return std::pair{mx::api::Bool::yes, mx::api::Bool::yes};
        }
        return std::pair{mx::api::Bool::yes, mx::api::Bool::no};
    }();
    /// @todo Export tuplet placement, offsets, hook lengths, slope, and other positioning if mx::api exposes them.
    return result;
}

void applyTupletData(mx::api::NoteData& note, const EntryInfoPtr& entryInfo)
{
    if (entryInfo->getEntry()->graceNote) {
        return;
    }

    auto activeTuplets = std::vector<size_t>{};
    for (size_t tupletIndex : entryInfo.findTupletInfo()) {
        const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
        if (shouldExportAsTuplet(tupletInfo)) {
            activeTuplets.emplace_back(tupletIndex);
        }
    }

    if (!activeTuplets.empty()) {
        const auto cumulativeRatio = entryInfo->cumulativeRatio;
        if (cumulativeRatio != 1) {
            const auto& tupletDef = entryInfo.getFrame()->tupletInfo[activeTuplets.back()].tuplet;
            const auto [normalDurationName, normalDots] = calcDurationInfoFromEdu(tupletDef->referenceDuration);
            const auto [entryDurationName, entryDots] = entryInfo->getEntry()->calcDurationInfo();
            note.durationData.timeModificationActualNotes = cumulativeRatio.denominator();
            note.durationData.timeModificationNormalNotes = cumulativeRatio.numerator();
            if (normalDurationName != entryDurationName || normalDots != entryDots) {
                note.durationData.timeModificationNormalType = enumConvert<mx::api::DurationName>(normalDurationName);
                note.durationData.timeModificationNormalTypeDots = int(normalDots);
            }
            /// @todo mx::api::NoteWriter currently ignores DurationData::timeModificationNormalType and infers normal-type
            /// by searching sibling notes for exactly one tuplet start and one tuplet stop. That is fragile for nested tuplets.
        }
    }

    for (size_t tupletIndex : activeTuplets) {
        const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
        const int numberLevel = int(tupletIndex) + 1;
        if (tupletInfo.startIndex == entryInfo.getIndexInFrame()) {
            note.noteAttachmentData.tupletStarts.emplace_back(createTupletStart(tupletInfo, numberLevel));
        }
        if (tupletInfo.endIndex == entryInfo.getIndexInFrame()) {
            auto& stop = note.noteAttachmentData.tupletStops.emplace_back();
            stop.numberLevel = numberLevel;
        }
    }
}

void applyRestPositionIfNeeded(mx::api::NoteData& rest, const EntryInfoPtr& entryInfo)
{
    if (!entryInfo) {
        return;
    }
    const auto entry = entryInfo->getEntry();
    if (entry->floatRest || entry->notes.empty()) {
        return;
    }
    const NoteInfoPtr restNoteInfo(entryInfo, 0);
    if (restNoteInfo->getNoteId() != Note::RESTID) {
        return;
    }
    const auto [noteName, octave, alteration, staffPosition] = restNoteInfo.calcNotePropertiesInView();
    (void)alteration;
    (void)staffPosition;
    rest.isDisplayStepOctaveSpecified = true;
    rest.pitchData = mx::api::PitchData(enumConvert<mx::api::Step>(noteName), 0, octave);
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

void applyAccidentalData(mx::api::NoteData& note, const NoteInfoPtr& noteInfo)
{
    if (!noteInfo->showAcci || (!noteInfo->freezeAcci && !noteInfo->parenAcci)) {
        return;
    }

    note.pitchData.showAccidental();
    note.pitchData.isAccidentalParenthetical = noteInfo->parenAcci;
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

std::vector<mx::api::Beam> createBeamData(const MusicXmlMusxMapping& context, const EntryInfoPtr& entryInfo)
{
    const auto isTremolo = [&entryInfo]() {
        const auto tupletIndices = entryInfo.findTupletInfo();
        for (size_t x : tupletIndices) {
            const auto& tuplet = entryInfo.getFrame()->tupletInfo[x];
            if (tuplet.includesEntry(entryInfo) && tuplet.calcIsTremolo()) {
                return true;
            }
        }
        return false;
    };

    if (!entryInfo.calcCanBeBeamed() || entryInfo.calcUnbeamed() || isTremolo()) {
        return {};
    }

    const auto calcVisibleBeamCount = [&]() {
        if (entryInfo.calcDisplaysAsRest()) {
            if (!context.finaleOptions.beamOptions->extendSecBeamsOverRests) {
                return 1U;
            }
        }
        return entryInfo.calcNumberOfBeams();
    };

    constexpr unsigned maxMusicXmlBeamLevels = 8;
    const unsigned numBeams = (std::min)(calcVisibleBeamCount(), maxMusicXmlBeamLevels);
    const unsigned lowestBeamStart = entryInfo.calcLowestBeamStart(/*considerBeamOverBarlines*/ true);
    const unsigned lowestBeamEnd = entryInfo.calcLowestBeamEndAcrossBarlines();
    const unsigned lowestBeamStub = lowestBeamStart && lowestBeamEnd ? (std::max)(lowestBeamStart, lowestBeamEnd) : 0;
    const bool stubIsLeft = lowestBeamStub && entryInfo.calcBeamStubIsLeft();

    auto beams = std::vector<mx::api::Beam>{};
    beams.reserve(numBeams);
    for (unsigned beamNumber = 1; beamNumber <= numBeams; ++beamNumber) {
        if (lowestBeamStub && beamNumber >= lowestBeamStub) {
            beams.emplace_back(stubIsLeft ? mx::api::Beam::backwardBroken : mx::api::Beam::forwardBroken);
        } else if (lowestBeamStart && beamNumber >= lowestBeamStart) {
            /// @todo When mx::api exposes beam fans, set fan on the first beam at the start of a beam group using
            /// EntryInfoPtr::calcIsFeatheredBeamStart.
            beams.emplace_back(mx::api::Beam::begin);
        } else if (lowestBeamEnd && beamNumber >= lowestBeamEnd) {
            beams.emplace_back(mx::api::Beam::end);
        } else {
            beams.emplace_back(mx::api::Beam::extend);
        }
    }
    return beams;
}

mx::api::NoteData createRestData(
    const MusicXmlMusxMapping& context,
    const EntryInfoPtr::InterpretedIterator& entryIt,
    const MusxInstance<others::Measure>& musxMeasure,
    const MusxInstance<others::StaffComposite>& measureStartStaff,
    int userVoiceNumber)
{
    const auto& entryInfo = entryIt.getEntryInfo();

    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.noteType = mx::api::NoteType::normal;
    rest.userRequestedVoiceNumber = userVoiceNumber;

    if (entryInfo) {
        const auto entry = entryInfo->getEntry();
        rest.noteType = entry->graceNote ? mx::api::NoteType::grace : mx::api::NoteType::normal;
        rest.durationData = createDurationData(context, entryInfo, entryIt.getEffectiveActualDuration(/*global*/ true));
    } else {
        rest.durationData.durationName = mx::api::DurationName::whole;
    }

    const bool isFullMeasureRest = !entryInfo || entryInfo.calcIsFullMeasureRest();
    if (isFullMeasureRest) {
        rest.isMeasureRest = true;
        rest.durationData.durationTimeTicks = context.timing.calcMusicXmlDivisions(musxMeasure->calcDuration());
    } else {
        rest.tickTimePosition = context.timing.calcMusicXmlDivisions(entryIt.getEffectiveElapsedDuration(/*global*/ true));
    }

    const auto effectiveStaff = entryInfo ? entryInfo.createCurrentStaff() : measureStartStaff;
    if (entryInfo) {
        rest.beams = createBeamData(context, entryInfo);
        applyTupletData(rest, entryInfo);
        applyRestPositionIfNeeded(rest, entryInfo);
        if (entryIt.getEffectiveHidden() || (effectiveStaff && effectiveStaff->hideRests)) {
            rest.printData.printObject = mx::api::Bool::no;
        }
    } else if (effectiveStaff && effectiveStaff->blankMeasure) {
        rest.printData.printObject = mx::api::Bool::no;
    }

    return rest;
}

void addSyntheticFullMeasureRest(
    const MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex)
{
    const int userVoiceNumber = musicXmlVoiceNumber(staffIndex, 0, 1);
    const auto measureStartStaff = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId, musxMeasure->getCmper(), 0);
    auto rest = createRestData(context, EntryInfoPtr::InterpretedIterator{}, musxMeasure, measureStartStaff, userVoiceNumber);

    auto& voice = staff.voices[userVoiceNumber - 1];
    voice.notes.emplace_back(rest);
}

void appendEntryNotes(
    MusicXmlMusxMapping& context,
    mx::api::VoiceData& voice,
    const EntryInfoPtr::InterpretedIterator& entryIt,
    const MusxInstance<others::Measure>& musxMeasure,
    int userVoiceNumber,
    bool hasMultipleLayers,
    bool hasVoice1Voice2,
    MusicXmlPitchContext pitchContext)
{
    const auto& entryInfo = entryIt.getEntryInfo();
    const auto entry = entryInfo->getEntry();
    for (size_t tupletIndex : entryInfo.findTupletInfo()) {
        const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
        if (tupletInfo.startIndex == entryInfo.getIndexInFrame() && tupletInfo.calcIsTremolo()) {
            context.logMessage(LogMsg() << "Double-note tremolo starting at entry " << entry->getEntryNumber()
                << " cannot be exported through mx::api yet. Emitting represented note duration without tremolo ornament.");
        }
    }
    if (entryInfo.calcIsFullMeasureRest() || entryInfo.calcDisplaysAsRest()) {
        auto rest = createRestData(context, entryIt, musxMeasure, nullptr, userVoiceNumber);
        voice.notes.emplace_back(rest);
        return;
    }

    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
        NoteInfoPtr noteInfo(entryInfo, noteIndex);
        auto note = mx::api::NoteData{};
        // MX uses this as "is in a chord group"; it suppresses <chord/> on the first note internally.
        note.isChord = entry->notes.size() > 1;
        note.noteType = entry->graceNote ? mx::api::NoteType::grace : mx::api::NoteType::normal;
        note.userRequestedVoiceNumber = userVoiceNumber;
        note.tickTimePosition = context.timing.calcMusicXmlDivisions(entryIt.getEffectiveElapsedDuration(/*global*/ true));
        note.durationData = createDurationData(context, entryInfo, entryIt.getEffectiveActualDuration(/*global*/ true));
        note.pitchData = createPitchData(noteInfo, pitchContext);
        /// @todo When mx::api::NoteData exposes a per-note staffIndex, set it here from NoteInfoPtr::calcStaff()
        /// for notes crossed to another staff within the same MusicXML part.
        applyAccidentalData(note, noteInfo);
        if (noteIndex == 0) {
            note.beams = createBeamData(context, entryInfo);
            applyTupletData(note, entryInfo);
        }
        if (entry->hasStem()) {
            const auto [freezeStem, upStem] = entryInfo.calcEntryStemSettings();
            if (freezeStem) {
                note.stem = upStem ? mx::api::Stem::up : mx::api::Stem::down;
            } else if (hasVoice1Voice2 || hasMultipleLayers) {
                /// @todo Ideally importers would infer these from independent voices, but explicit stems currently import better.
                note.stem = entryInfo.calcUpStem() ? mx::api::Stem::up : mx::api::Stem::down;
            }
        }
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

    const Fraction legacyPickupSpacer = musxMeasure->calcMinLegacyPickupSpacer(staffId);
    musx::dom::details::GFrameHoldContext gfHold(context.document, context.forPartId, staffId, musxMeasure->getCmper(), legacyPickupSpacer);
    if (!gfHold) {
        addSyntheticFullMeasureRest(context, staff, musxMeasure, staffId, staffIndex);
        return;
    }

    bool emittedNotes = false;
    const auto pitchContext = pitchContextForStaff(context, staffId);
    /// @todo Export cue notes/rests instead of omitting cue layers.
    const auto cueLayerPlan = createCueLayerPlan(gfHold, context.denigmaContext->cueLayer);
    const auto layerVoices = gfHold.calcVoices();
    const bool hasMultipleLayers = layerVoices.size() > 1;
    for (const auto& [layer, numVoice2Entries] : layerVoices) {
        if (cueLayerPlan.skipsLayer(layer)) {
            continue;
        }
        const int maxVoice = numVoice2Entries ? 2 : 1;
        const auto entryFrame = gfHold.createEntryFrame(layer);
        ASSERT_IF(!entryFrame) {
            continue;
        }
        const bool usesV1V2 = numVoice2Entries && entryFrame->getFirstInterpretedIterator(2);
        for (int voiceNumber = 1; voiceNumber <= maxVoice; ++voiceNumber) {
            const int userVoiceNumber = musicXmlVoiceNumber(staffIndex, layer, voiceNumber);
            auto& voice = staff.voices[userVoiceNumber - 1];
            for (auto entryIt = entryFrame->getFirstInterpretedIterator(voiceNumber); entryIt; entryIt = entryIt.getNext()) {
                if (entryIt.calcIsPastLogicalEndOfFrame()) {
                    break;
                }
                appendEntryNotes(context, voice, entryIt, musxMeasure, userVoiceNumber, hasMultipleLayers, usesV1V2, pitchContext);
                emittedNotes = true;
            }
        }
    }

    if (!emittedNotes) {
        addSyntheticFullMeasureRest(context, staff, musxMeasure, staffId, staffIndex);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
