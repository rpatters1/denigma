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
#include <type_traits>
#include <utility>
#include <vector>

#include "core/cue_layers.h"
#include "musicxml_formatted_text.h"
#include "mx/api/DurationData.h"
#include "mx/api/MarkData.h"
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
                return calcDurationInfoFromEdu(tupletInfo.tuplet->calcReferenceDuration().calcEduDuration());
            }
        }
        return entryInfo->getEntry()->calcDurationInfo();
    }();
    duration.durationName = enumConvert<mx::api::DurationName>(durationName);
    duration.durationDots = int(dots);
    duration.durationTimeTicks = context.timing.calcMusicXmlDivisions(actualDuration);
    for (size_t tupletIndex : entryInfo.findTupletInfo()) {
        const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
        if (tupletInfo.calcIsTremolo()) {
            constexpr int tremoloActualNotes = 2;
            constexpr int tremoloNormalNotes = 1;
            duration.timeModificationActualNotes = tremoloActualNotes;
            duration.timeModificationNormalNotes = tremoloNormalNotes;
            break;
        }
    }
    return duration;
}

bool shouldExportAsTuplet(const EntryFrame::TupletInfo& tupletInfo)
{
    if (tupletInfo.tuplet->calcRatio() == 0) {
        /// @todo Decide whether any non-singleton zero-length tuplets need MusicXML representation.
        return false;
    }
    if (tupletInfo.calcIsTremolo()) {
        return false;
    }
    return true;
}

int calcTremoloMarks(const EntryInfoPtr& entryInfo, const EntryFrame::TupletInfo& tupletInfo)
{
    constexpr unsigned maxMusicXmlTremoloMarks = 8;
    const unsigned numBeams = entryInfo.calcNumberOfBeams();
    const unsigned numReferenceBeams = calcNumberOfBeamsInEdu(tupletInfo.tuplet->calcReferenceDuration().calcEduDuration());
    const unsigned marks = numBeams > numReferenceBeams ? numBeams - numReferenceBeams : 0;
    return static_cast<int>((std::min)(marks, maxMusicXmlTremoloMarks));
}

void applyTremoloData(mx::api::NoteData& note, const EntryInfoPtr& entryInfo)
{
    for (size_t tupletIndex : entryInfo.findTupletInfo()) {
        const auto& tupletInfo = entryInfo.getFrame()->tupletInfo[tupletIndex];
        if (!tupletInfo.calcIsTremolo()) {
            continue;
        }

        const auto entryIndex = entryInfo.getIndexInFrame();
        const auto markType = entryIndex == tupletInfo.startIndex
            ? mx::api::MarkType::tremoloStart
            : entryIndex == tupletInfo.endIndex ? mx::api::MarkType::tremoloStop : mx::api::MarkType::unspecified;
        if (markType == mx::api::MarkType::unspecified) {
            continue;
        }

        auto mark = mx::api::MarkData(markType);
        mark.choice = mx::api::TremoloMarkData{ .tremoloMarks = calcTremoloMarks(entryInfo, tupletInfo) };
        note.noteAttachmentData.marks.emplace_back(std::move(mark));
    }
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

mx::api::PitchData createPitchData(MusicXmlMusxMapping& context, const NoteInfoPtr& noteInfo, MusicXmlPitchContext pitchContext)
{
    const auto [noteName, octave, alteration, staffPosition] = [&]() {
        if (pitchContext == MusicXmlPitchContext::Concert) {
            return noteInfo.calcNotePropertiesConcert();
        }
        return noteInfo.calcNoteProperties();
    }();
    (void)staffPosition;
    const auto octaveAdjustment = calcOttavaOctaveAdjustment(
        context.current.ottavasApplicableInMeasure,
        noteInfo,
        [&](const NoteInfoPtr&) {
            context.logMessage(
                LogMsg() << "skipping ottava octave setting for tied-to note since the tied-from note is not under the ottava",
                MessageSeverity::Verbose);
        });
    return mx::api::PitchData(enumConvert<mx::api::Step>(noteName), alteration, octave + octaveAdjustment);
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

void applyTieAlterStart(const MusicXmlMusxMapping& context, mx::api::TieLetRing& tieLetRing, const NoteInfoPtr& noteInfo)
{
    if (auto tieAlter = context.document->getDetails()->getForNote<details::TieAlterStart>(noteInfo)) {
        if (tieAlter->freezeDirection) {
            tieLetRing.curveOrientation = enumConvert<mx::api::CurveOrientation>(
                tieAlter->down ? CurveContourDirection::Down : CurveContourDirection::Up);
        }
    }
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
    if (!tiedTo || !tiedTo->tieEnd || tiedTo.getEntryInfo()->getEntry()->isHidden) {
        note.tieLetRing = mx::api::TieLetRing{};
        applyTieAlterStart(context, *note.tieLetRing, noteInfo);
        return;
    }
    note.isTieStart = true;
    context.pendingTieStopKeys.emplace(noteKey(tiedTo));
    /// @todo Apply TieAlterStart orientation here too once mx::api can express it on a regular
    /// (paired) tie without duplicating <tied>. See "Curve orientation on a regular (paired) tie"
    /// in mx-api-gaps.md.
}

void applyLyrics(MusicXmlMusxMapping& context, mx::api::NoteData& note, const EntryInfoPtr& entryInfo)
{
    const auto entry = entryInfo->getEntry();

    auto applyLyricType = [&](const auto& lyricAssignments) {
        using PtrType = typename std::decay_t<decltype(lyricAssignments)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<details::LyricAssign, T>, "lyricAssignments must contain LyricAssign subtypes");

        for (const auto& assignment : lyricAssignments) {
            const auto lyricText = assignment->getLyricText();
            if (!lyricText) {
                continue;
            }
            if (assignment->syllable == 0 || assignment->syllable > lyricText->syllables.size()) {
                context.logMessage(LogMsg() << "Layer " << entryInfo.getLayerIndex() + 1
                    << " entry index " << entryInfo.getIndexInFrame() << " has an invalid syllable number ("
                    << assignment->syllable << ").", MessageSeverity::Warning);
                continue;
            }

            const size_t syllableIndex = size_t(assignment->syllable - 1);
            auto lyric = musicXmlLyricFromSyllable(context, *lyricText, syllableIndex);
            /// @todo Check whether Finale exports displayVerseNum by prepending the verse number to lyric text.
            lyric.verseNumber = std::string(T::TextType::XmlNodeName.substr(0, 1)) + std::to_string(assignment->lyricNumber);
            lyric.hasExtend = assignment->wext != 0 || lyricText->syllables[syllableIndex]->strippedUnderscores > 0;
            if (assignment->horzOffset != 0) {
                lyric.positionData.relativeX = context.musicXmlTenthsFromEvpu(assignment->horzOffset);
                lyric.positionData.isRelativeXSpecified = true;
            }
            if (assignment->vertOffset != 0) {
                lyric.positionData.relativeY = context.musicXmlTenthsFromEvpu(assignment->vertOffset);
                lyric.positionData.isRelativeYSpecified = true;
            }
            if (const auto lyricEntryInfo = entry->getDocument()->getDetails()->get<details::LyricEntryInfo>(SCORE_PARTID, entry->getEntryNumber());
                lyricEntryInfo && lyricEntryInfo->justify) {
                lyric.positionData.horizontalAlignmnet = enumConvert<mx::api::HorizontalAlignment>(*lyricEntryInfo->justify);
            }
            if constexpr (requires { assignment->hide; }) {
                lyric.printData.printObject = assignment->hide ? mx::api::Bool::no : mx::api::Bool::yes;
            }
            note.lyrics.emplace_back(std::move(lyric));
        }
    };

    applyLyricType(entry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(SCORE_PARTID, entry->getEntryNumber()));
    applyLyricType(entry->getDocument()->getDetails()->getArray<details::LyricAssignChorus>(SCORE_PARTID, entry->getEntryNumber()));
    applyLyricType(entry->getDocument()->getDetails()->getArray<details::LyricAssignSection>(SCORE_PARTID, entry->getEntryNumber()));
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
    MusicXmlMusxMapping& context,
    const EntryInfoPtr::InterpretedIterator& entryIt,
    const MusxInstance<others::Measure>& musxMeasure,
    const MusxInstance<others::StaffComposite>& measureStartStaff,
    int userVoiceNumber)
{
    const auto& entryInfo = entryIt.getEntryInfo();

    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.userRequestedVoiceNumber = userVoiceNumber;

    if (entryInfo) {
        const auto entry = entryInfo->getEntry();
        rest.isGrace = entry->graceNote;
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
        processArticulations(context, rest, entryInfo);
        if (entryIt.getEffectiveHidden() || (effectiveStaff && effectiveStaff->hideRests)) {
            rest.printData.printObject = mx::api::Bool::no;
        }
    } else if (effectiveStaff && effectiveStaff->blankMeasure) {
        rest.printData.printObject = mx::api::Bool::no;
    }

    return rest;
}

void addSyntheticFullMeasureRest(
    MusicXmlMusxMapping& context,
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
    size_t measureIndex,
    size_t staffIndex,
    bool hasMultipleLayers,
    bool hasVoice1Voice2,
    MusicXmlPitchContext pitchContext)
{
    const auto& entryInfo = entryIt.getEntryInfo();
    const auto entry = entryInfo->getEntry();
    auto rememberFirstNote = [&](size_t noteIndex) {
        context.entryNumberToFirstNote.try_emplace(entry->getEntryNumber(), MusicXmlNoteLocation{
            .measureIndex = measureIndex,
            .staffIndex = staffIndex,
            .userVoiceNumber = userVoiceNumber,
            .noteIndex = noteIndex
        });
    };
    auto rememberExactNote = [&](const NoteInfoPtr& noteInfo, size_t noteIndex) {
        context.noteLocations.try_emplace(
            musicXmlNoteKey(entry->getEntryNumber(), noteInfo->getNoteId()),
            MusicXmlNoteLocation{
                .measureIndex = measureIndex,
                .staffIndex = staffIndex,
                .userVoiceNumber = userVoiceNumber,
                .noteIndex = noteIndex
            });
    };
    if (entryInfo.calcIsFullMeasureRest() || entryInfo.calcDisplaysAsRest()) {
        auto rest = createRestData(context, entryIt, musxMeasure, nullptr, userVoiceNumber);
        rememberFirstNote(voice.notes.size());
        voice.notes.emplace_back(rest);
        return;
    }

    const auto entryNoteheads = classify::classifyEntryNoteheads(entryInfo);
    // The document is created with PartVoicingPolicy::Apply, so a voiced linked part may
    // include only a subset of a chord's notes. (calcDisplaysAsRest above covers the case
    // where voicing excludes every note.)
    std::vector<size_t> includedNoteIndices;
    includedNoteIndices.reserve(entry->notes.size());
    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
        if (NoteInfoPtr(entryInfo, noteIndex).calcIsIncludedInVoicing()) {
            includedNoteIndices.push_back(noteIndex);
        }
    }
    for (size_t includedPosition = 0; includedPosition < includedNoteIndices.size(); ++includedPosition) {
        const size_t noteIndex = includedNoteIndices[includedPosition];
        NoteInfoPtr noteInfo(entryInfo, noteIndex);
        auto note = mx::api::NoteData{};
        // MX uses this as "is in a chord group"; it suppresses <chord/> on the first note internally.
        note.isChord = includedNoteIndices.size() > 1;
        note.isGrace = entry->graceNote;
        if (note.isGrace) {
            note.graceSlash = entryInfo.calcGraceNoteSlash(context.finaleOptions.graceOptions)
                ? mx::api::Bool::yes
                : mx::api::Bool::no;
        }
        note.userRequestedVoiceNumber = userVoiceNumber;
        note.tickTimePosition = context.timing.calcMusicXmlDivisions(entryIt.getEffectiveElapsedDuration(/*global*/ true));
        note.durationData = createDurationData(context, entryInfo, entryIt.getEffectiveActualDuration(/*global*/ true));
        note.pitchData = createPitchData(context, noteInfo, pitchContext);
        if (const auto stavesIt = context.partIdToStaves.find(context.currentPart->uniqueId); stavesIt != context.partIdToStaves.end()) {
            ASSERT_IF(staffIndex >= stavesIt->second.size()) {
                throw std::logic_error("Containing staff index is outside the current MusicXML part staff list.");
            }
            const auto containingStaffId = stavesIt->second[staffIndex];
            if (const auto noteStaffId = noteInfo.calcStaff(); noteStaffId != containingStaffId) {
                const auto noteStaffIt = std::find(stavesIt->second.begin(), stavesIt->second.end(), noteStaffId);
                if (noteStaffIt != stavesIt->second.end()) {
                    note.crossStaffIndex = static_cast<int>(std::distance(stavesIt->second.begin(), noteStaffIt));
                } else {
                    context.logMessage(LogMsg() << "Cross-staff note in entry " << entry->getEntryNumber()
                        << " points to staff " << noteStaffId << ", which is not included in MusicXML part "
                        << context.currentPart->uniqueId << ".", MessageSeverity::Warning);
                }
            }
        }
        applyAccidentalData(note, noteInfo);
        applyNoteheadData(note, noteInfo, entryNoteheads);
        if (includedPosition == 0) {
            note.beams = createBeamData(context, entryInfo);
            applyTupletData(note, entryInfo);
            applyLyrics(context, note, entryInfo);
            processArticulations(context, note, entryInfo);
            applyTremoloData(note, entryInfo);
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
        if (includedPosition == 0) {
            rememberFirstNote(voice.notes.size());
        }
        rememberExactNote(noteInfo, voice.notes.size());
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
    size_t measureIndex,
    size_t staffIndex)
{
    (void)measure;

    context.current.ottavasApplicableInMeasure = collectOttavasForMeasureStaff(
        context.document, context.forPartId, musxMeasure, staffId);
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
    context.cueDiscardPlansByMeasureStaff.emplace(musicXmlMeasureStaffKey(musxMeasure->getCmper(), staffId), cueLayerPlan);
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
                appendEntryNotes(context, voice, entryIt, musxMeasure, userVoiceNumber, measureIndex, staffIndex, hasMultipleLayers, usesV1V2, pitchContext);
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
