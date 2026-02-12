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
#include <unordered_set>

#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

static void appendMeasureRemainderSpaces(mnx::ContentArray content,
    const musx::util::Fraction& elapsedInVoice,
    const musx::util::Fraction& measureDuration)
{
    const auto remaining = measureDuration - elapsedInVoice;
    const int denom = remaining.denominator();
    ASSERT_IF(denom <= 0) {
        throw std::logic_error("Remaining duration has non-positive denominator.");
    }
    if (remaining <= 0 || (EDU_PER_WHOLE_NOTE % denom) != 0) {
        return;
    }

    const auto eduRemaining = remaining.calcEduDuration();

    std::vector<Edu> groups;
    int mask = 1;
    while (mask <= eduRemaining) {
        mask <<= 1;
    }
    mask >>= 1;

    bool inGroup = false;
    long long groupValue = 0;
    while (mask > 0) {
        if (eduRemaining & mask) {
            groupValue += mask;
            inGroup = true;
        } else if (inGroup) {
            groups.push_back(static_cast<Edu>(groupValue));
            groupValue = 0;
            inGroup = false;
        }
        mask >>= 1;
    }
    if (inGroup) {
        groups.push_back(static_cast<Edu>(groupValue));
    }

    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        content.append<mnx::sequence::Space>(mnxFractionFromEdu(*it));
    }
}

static mnx::sequence::MultiNoteTremolo createMultiNoteTremolo(mnx::ContentArray content, const musx::dom::EntryFrame::TupletInfo& tupletInfo, int marks)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    const auto entryCount = static_cast<unsigned>(tupletInfo.numEntries());
    const Edu eduRefDuration = (musxTuplet->calcReferenceDuration() / entryCount).calcEduDuration();
    auto mnxTremolo = content.append<mnx::sequence::MultiNoteTremolo>(
        marks,
        mnx::NoteValueQuantity::make(entryCount, mnxNoteValueFromEdu(eduRefDuration)));
    /// @todo: additional fields (like noteheads) when defined by MNX committee.
    return mnxTremolo;
}

static mnx::sequence::Tuplet createTuplet(mnx::ContentArray content, const musx::dom::EntryFrame::TupletInfo& tupletInfo)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    auto mnxTuplet = content.append<mnx::sequence::Tuplet>(
        mnx::NoteValueQuantity::make(
            static_cast<unsigned>(musxTuplet->displayNumber),
            mnxNoteValueFromEdu(musxTuplet->displayDuration)),
        mnx::NoteValueQuantity::make(
            static_cast<unsigned>(musxTuplet->referenceNumber),
            mnxNoteValueFromEdu(musxTuplet->referenceDuration)));

    mnxTuplet.set_or_clear_bracket([&]() {
        if (musxTuplet->brackStyle == details::TupletDef::BracketStyle::Nothing) {
            return mnx::AutoYesNo::No;
        }
        return enumConvert<mnx::AutoYesNo>(musxTuplet->autoBracketStyle);
    }());

    mnxTuplet.set_or_clear_showNumber([&]() {
        switch (musxTuplet->numStyle) {
            case details::TupletDef::NumberStyle::Number: return mnx::TupletDisplaySetting::Inner;
            case details::TupletDef::NumberStyle::Nothing: return mnx::TupletDisplaySetting::NoNumber;
            case details::TupletDef::NumberStyle::UseRatio: return mnx::TupletDisplaySetting::Both;
            case details::TupletDef::NumberStyle::RatioPlusDenominatorNote: return mnx::TupletDisplaySetting::Both;
            case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnx::TupletDisplaySetting::Both;
        }
        return mnx::TupletDisplaySetting::Inner;
    }());

    mnxTuplet.set_or_clear_showValue([&]() {
        switch (musxTuplet->numStyle) {
            case details::TupletDef::NumberStyle::Number: return mnx::TupletDisplaySetting::NoNumber;
            case details::TupletDef::NumberStyle::Nothing: return mnx::TupletDisplaySetting::NoNumber;
            case details::TupletDef::NumberStyle::UseRatio: return mnx::TupletDisplaySetting::NoNumber;
            case details::TupletDef::NumberStyle::RatioPlusDenominatorNote: return mnx::TupletDisplaySetting::Inner; // should be Outer, but this is not currently an option
            case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnx::TupletDisplaySetting::Both;
        }
        return mnx::TupletDisplaySetting::NoNumber;
    }());

    return mnxTuplet;
}

static void createTies(const MnxMusxMappingPtr& context, mnx::sequence::NoteBase& mnxNote, const NoteInfoPtr& musxNote)
{
    bool tieCreated = false;
    if (musxNote->tieStart) {
        auto mnxTies = mnxNote.ensure_ties();
        auto tiedTo = musxNote.calcTieTo();
        auto mnxTie = mnxTies.append();
        if (tiedTo && tiedTo->tieEnd && !tiedTo.getEntryInfo()->getEntry()->isHidden) {
            mnxTie.set_target(calcNoteId(tiedTo));
        } else {
            mnxTie.set_lv(true);
        }
        if (!mnxTie.lv()) {
            if (tiedTo.getEntryInfo().getVoice() == musxNote.getEntryInfo().getVoice()) {
                mnxTie.set_targetType(mnx::TieTargetType::NextNote);
            } else {
                mnxTie.set_targetType(mnx::TieTargetType::CrossVoice);
            }
        }
        if (auto tieAlter = context->document->getDetails()->getForNote<details::TieAlterStart>(musxNote)) {
            if (tieAlter->freezeDirection) {
                mnxTie.set_side(tieAlter->down ? mnx::SlurTieSide::Down : mnx::SlurTieSide::Up);
            }
        }
        tieCreated = true;
    }
    using Curve = CurveContourDirection;
    if (const auto tiedToInfo = musxNote.calcArpeggiatedTieInfo()) {
        const NoteInfoPtr musxTargetNote(tiedToInfo->targetEntry, tiedToInfo->targetNoteIndex);
        auto mnxTies = mnxNote.ensure_ties();
        auto mnxTie = mnxTies.append();
        mnxTie.set_target(calcNoteId(musxTargetNote));
        mnxTie.set_targetType(mnx::TieTargetType::Arpeggio);
        if (tiedToInfo->direction != Curve::Unspecified) {
            mnxTie.set_side(tiedToInfo->direction == Curve::Up ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down);
        }
        tieCreated = true;
    }
    if (!tieCreated) {
        if (const auto pseudoTieInfo = musxNote.calcPseudoLvTieInfo()) {
            auto mnxTies = mnxNote.ensure_ties();
            auto mnxTie = mnxTies.append();
            mnxTie.set_lv(true);
            if (pseudoTieInfo.direction != Curve::Unspecified) {
                mnxTie.set_side(pseudoTieInfo.direction == Curve::Up ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down);
            }
        }
    }
}

static void deferJumpTies(const MnxMusxMappingPtr& context, const NoteInfoPtr& musxNote)
{
    if (musxNote.getEntryInfo().getMeasure() == 4) {
        int x = 0;
        static_cast<void>(x);
    }
    if (musxNote.getEntryInfo()->getEntry()->isHidden) {
        return;
    }

    const auto jumpTies = musxNote.calcJumpTieContinuationsFrom();
    if (jumpTies.empty()) {
        return;
    }

    const auto endNoteId = calcNoteId(musxNote);
    for (const auto& [startNote, direction] : jumpTies) {
        if (!startNote || startNote.getEntryInfo()->getEntry()->isHidden) {
            continue;
        }
        const auto startNoteId = calcNoteId(startNote);
        const std::string key = startNoteId + "->" + endNoteId;
        if (!context->deferredJumpTieKeys.emplace(key).second) {
            continue;
        }

        MnxMusxMapping::DeferredJumpTie deferred{
            startNoteId,
            endNoteId,
            std::nullopt
        };
        if (direction != CurveContourDirection::Unspecified) {
            deferred.side = (direction == CurveContourDirection::Up) ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down;
        }
        context->deferredJumpTies.push_back(std::move(deferred));
    }
}

static void createSlurs(const MnxMusxMappingPtr&, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();
    auto createOneSlur = [&](const EntryNumber targetEntry) -> mnx::sequence::Slur {
        auto mnxSlurs = mnxEvent.ensure_slurs();
        return mnxSlurs.append(calcEventId(targetEntry));
    };
    if (musxEntry->smartShapeDetail) {
        auto shapeAssigns = musxEntry->getDocument()->getDetails()->getArray<details::SmartShapeEntryAssign>(SCORE_PARTID, musxEntry->getEntryNumber());
        for (const auto& assign : shapeAssigns) {
            if (auto shape = musxEntry->getDocument()->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum)) {
                if (shape->startTermSeg->endPoint->entryNumber != musxEntry->getEntryNumber()) {
                    continue;
                }
                if (shape->calcIsSlur()) {
                    auto mnxSlur = createOneSlur(shape->endTermSeg->endPoint->entryNumber);
                    mnxSlur.set_lineType(shape->calcIsDashed() ? mnx::LineType::Dashed : mnx::LineType::Solid);
                    if (auto contourDir = shape->calcContourDirection(); contourDir != CurveContourDirection::Unspecified) {
                        mnxSlur.set_side(contourDir == CurveContourDirection::Up ? mnx::SlurTieSide::Up : mnx::SlurTieSide::Down);
                    }
                }
            }
        }
    }
}

static void createMarkings(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const MusxInstance<Entry>& musxEntry)
{
    auto articAssigns = context->document->getDetails()->getArray<details::ArticulationAssign>(SCORE_PARTID, musxEntry->getEntryNumber());
    for (const auto& asgn : articAssigns) {
        if (!asgn->hide) {
            if (auto artic = context->document->getOthers()->get<others::ArticulationDef>(asgn->getRequestedPartId(), asgn->articDef); artic && !artic->noPrint) {
                std::optional<int> numMarks;
                std::optional<mnx::BreathMarkSymbol> breathMark;
                auto marks = calcMarkingType(artic, numMarks, breathMark);
                for (auto mark : marks) {
                    auto mnxMarkings = mnxEvent.ensure_markings();
                    switch (mark) {
                        case EventMarkingType::Accent:
                            if (!mnxMarkings.accent().has_value()) {
                                mnxMarkings.ensure_accent();
                            }
                            break;
                        case EventMarkingType::Breath:
                            if (!mnxMarkings.breath().has_value()) {
                                mnxMarkings.ensure_breath();
                            }
                            if (breathMark.has_value()) {
                                mnxMarkings.breath().value().set_symbol(breathMark.value());
                            }
                            break;
                        case EventMarkingType::SoftAccent:
                            if (!mnxMarkings.softAccent().has_value()) {
                                mnxMarkings.ensure_softAccent();
                            }
                            break;
                        case EventMarkingType::Spiccato:
                            if (!mnxMarkings.spiccato().has_value()) {
                                mnxMarkings.ensure_spiccato();
                            }
                            break;
                        case EventMarkingType::Staccatissimo:
                            if (!mnxMarkings.staccatissimo().has_value()) {
                                mnxMarkings.ensure_staccatissimo();
                            }
                            break;
                        case EventMarkingType::Staccato:
                            if (!mnxMarkings.staccato().has_value()) {
                                mnxMarkings.ensure_staccato();
                            }
                            break;
                        case EventMarkingType::Stress:
                            if (!mnxMarkings.stress().has_value()) {
                                mnxMarkings.ensure_stress();
                            }
                            break;
                        case EventMarkingType::StrongAccent:
                            if (!mnxMarkings.strongAccent().has_value()) {
                                mnxMarkings.ensure_strongAccent();
                            }
                            break;
                        case EventMarkingType::Tenuto:
                            if (!mnxMarkings.tenuto().has_value()) {
                                mnxMarkings.ensure_tenuto();
                            }
                            break;
                        case EventMarkingType::Tremolo:
                            if (!mnxMarkings.tremolo().has_value()) {
                                mnxMarkings.ensure_tremolo(numMarks.value_or(0));
                            }
                            break;
                        case EventMarkingType::Unstress:
                            if (!mnxMarkings.unstress().has_value()) {
                                mnxMarkings.ensure_unstress();
                            }
                            break;
                        default:
                            ASSERT_IF(true) {
                                throw std::logic_error("Encountered unknown event marking type " + std::to_string(int(mark)));
                            }
                    }
                }
            }
        }
    }
}

mnx::sequence::Note createNormalNote(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const NoteInfoPtr& musxNote)
{
    auto [noteName, octave, alteration, _] = musxNote.calcNotePropertiesConcert();
    for (const auto& it : context->ottavasApplicableInMeasure) {
        auto shape = it.second;
        if (shape->calcAppliesTo(musxNote.getEntryInfo())) {
            // if the note is a tie continuation, the ottava has to apply to the original note
            auto tiedFromNoteInfo = musxNote;
            while (tiedFromNoteInfo && tiedFromNoteInfo->tieEnd) {
                tiedFromNoteInfo = tiedFromNoteInfo.calcTieFrom();
            }
            if (!tiedFromNoteInfo || shape->calcAppliesTo(tiedFromNoteInfo.getEntryInfo())) {
                octave += int(enumConvert<mnx::OttavaAmount>(shape->shapeType));
            } else if (!musxNote.isSameNote(tiedFromNoteInfo)) {
                context->logMessage(LogMsg() << "skipping ottava octave setting for tied-to note since the tied-from note is not under the ottava", LogSeverity::Verbose);
            }
        }
    }
    auto mnxNote = mnxEvent.ensure_notes().append(
        mnx::sequence::Pitch::make(enumConvert<mnx::NoteStep>(noteName), octave, alteration));
    if (musxNote->freezeAcci || musxNote->parenAcci) {
        auto acciDisp = mnxNote.ensure_accidentalDisplay(musxNote->showAcci);
        acciDisp.set_or_clear_force(musxNote->freezeAcci);
        if (musxNote->parenAcci) {
            acciDisp.ensure_enclosure(mnx::AccidentalEnclosureSymbol::Parentheses);
        }
    }
    const auto musxEntry = musxNote.getEntryInfo()->getEntry();
    if (musxNote.calcIsEnharmonicRespellInAnyPart()) {
        auto [enharmonicLev, enharmonicAlt] = musxNote.calcDefaultEnharmonic();
        auto mnxWritten = mnxNote.ensure_written();
        mnxWritten.set_diatonicDelta(enharmonicLev - musxNote->harmLev);
    }
    return mnxNote;
}

mnx::sequence::KitNote createKitNote(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const MusxInstance<others::PercussionNoteInfo>& percNoteInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    auto mnxNote = mnxEvent.ensure_kitNotes().append(calcPercussionKitId(percNoteInfo));
    auto part = mnxNote.getEnclosingElement<mnx::Part>();
    MNX_ASSERT_IF(!part.has_value()) {
        throw std::logic_error("Note created without a part.");
    }
    part->ensure_kit();
    if (!part->kit()->contains(mnxNote.kitComponent())) {
        auto kitElement = part->kit()->append(
            mnxNote.kitComponent(),
            mnxStaffPosition(musxStaff, percNoteInfo->calcStaffReferencePosition()));
        const auto& percNoteType = percNoteInfo->getNoteType();
        if (percNoteType.instrumentId != 0) {
            kitElement.set_name(percNoteType.createName(percNoteInfo->getNoteTypeOrderId()));
            kitElement.set_sound(calcPercussionSoundId(percNoteInfo));
            auto sounds = context->mnxDocument->global().ensure_sounds();
            if (!sounds.contains(kitElement.sound().value())) {
                auto sound = sounds.append(kitElement.sound().value());
                sound.set_name(kitElement.name().value());
                if (percNoteType.generalMidi >= 0) {
                    /// @todo revisit this if there is better support for multiple synth patches
                    sound.set_midiNumber(percNoteType.generalMidi);
                }
            }
        }
    }
    return mnxNote;
}

template <typename MnxNoteType>
static void createNote(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const NoteInfoPtr& musxNote,
    const MusxInstance<others::Staff>& musxStaff, const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    static_assert(std::is_base_of_v<mnx::sequence::NoteBase, MnxNoteType>, "MnxNoteType must have base type NoteBase.");

    const auto musxEntry = musxNote.getEntryInfo()->getEntry();

    MnxNoteType mnxNote = [&]() {
        if constexpr (std::is_same_v<MnxNoteType, mnx::sequence::Note>) {
            return createNormalNote(context, mnxEvent, musxNote);
        } else {
            MUSX_ASSERT_IF(!percNoteInfo) {
                throw std::logic_error("Kit note requested without PercussionNoteInfo instance.");
            }
            return createKitNote(context, mnxEvent, percNoteInfo, musxStaff);
        }
    }();
    const auto noteId = calcNoteId(musxNote);
    mnxNote.set_id(noteId);
    context->noteJsonById.emplace(noteId, mnxNote.pointer());
    if (musxNote->crossStaff && !mnxEvent.staff()) { // createEvent already handled cross-staffing if the entire entry is crossed
        StaffCmper noteStaff = musxNote.calcStaff();
        if (const auto& mnxNoteStaff = context->mnxPartStaffFromStaff(noteStaff)) {
            mnxNote.set_staff(mnxNoteStaff.value());
        } else {
            context->logMessage(LogMsg() << " note has cross-staffing to a staff (" << noteStaff
                << ") that is not included in the MNX part.", LogSeverity::Warning);
        }
    }
    createTies(context, mnxNote, musxNote);
    deferJumpTies(context, musxNote);
}

static void createNotes(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    for (size_t x = 0; x < musxEntry->notes.size(); x++) {
        const auto mnxNote = NoteInfoPtr(musxEntryInfo, x);
        if (const auto percNoteInfo = mnxNote.calcPercussionNoteInfo()) {
            createNote<mnx::sequence::KitNote>(context, mnxEvent, mnxNote, musxStaff, percNoteInfo);
        } else {
            createNote<mnx::sequence::Note>(context, mnxEvent, mnxNote, musxStaff, nullptr);
        }
    }
}

static void createRest([[maybe_unused]] const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    auto mnxRest = mnxEvent.ensure_rest();
    if (musxEntryInfo.calcIsFullMeasureRest()) {
        mnxEvent.clear_duration();
        mnxEvent.set_measure(true);
    }
    // If a rest is hidden, it has been detected as a beam workaround, so its staff position is meaningless
    if (!musxEntry->isHidden && !musxEntry->floatRest && !musxEntry->notes.empty()) {
        auto musxRest = NoteInfoPtr(musxEntryInfo, 0);
        auto staffPosition = std::get<3>(musxRest.calcNotePropertiesInView());
        if (mnxEvent.measure() || (mnxEvent.duration() && mnxEvent.duration().value().base() == mnx::NoteValueBase::Whole)) {
            staffPosition += 2; // compensate for discrepancy in Finale vs. MNX whole rest staff positions.
        }
        mnxRest.set_staffPosition(mnxStaffPosition(musxStaff, staffPosition));
    }
}

static void createLyrics(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    auto createLyricsType = [&](const auto& musxLyrics) {
        using PtrType = typename std::decay_t<decltype(musxLyrics)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<details::LyricAssign, T>, "musxLyrics must be a subtype of LyricAssign");
        for (const auto& lyr : musxLyrics) {
            if (auto lyrText = lyr->getLyricText()) {
                if (lyr->syllable > lyrText->syllables.size()) { // Finale syllable numbers are 1-based.
                    context->logMessage(LogMsg() << " Layer " << musxEntryInfo.getLayerIndex() + 1
                        << " Entry index " << musxEntryInfo.getIndexInFrame() << " has an invalid syllable number ("
                        << lyr->syllable << ").", LogSeverity::Warning);
                } else {
                    auto mnxLyrics = mnxEvent.ensure_lyrics();
                    auto mnxLyricsLines = mnxLyrics.ensure_lines();
                    const size_t sylIndex = size_t(lyr->syllable - 1); // Finale syllable numbers are 1-based.
                    auto mnxLyricLine = mnxLyricsLines.append(
                        calcLyricLineId(std::string(T::TextType::XmlNodeName), lyr->lyricNumber),
                        lyrText->syllables[sylIndex]->syllable);
                    mnxLyricLine.set_type(mnxLineTypeFromLyric(lyrText->syllables[sylIndex]));
                }
            }
        }
    };
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignChorus>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignSection>(SCORE_PARTID, musxEntry->getEntryNumber()));
}

static std::optional<mnx::sequence::Event> createEvent(const MnxMusxMappingPtr& context, mnx::ContentArray content,
    EntryInfoPtr musxEntryInfo, bool effectiveHidden, bool hasVoice1Voice2,
    const MusxInstance<details::TupletDef>& tupletDef, bool forTremolo)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    if (effectiveHidden) {
        /// @todo include hidden entries perhaps, if MNX starts allowing them.
        content.append<mnx::sequence::Space>(mnxFractionFromEdu(musxEntry->duration));
        return std::nullopt;
    }

    auto musxStaff = musxEntryInfo.createCurrentStaff();
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber())
            + " has no staff information for staff " + std::to_string(musxEntryInfo.getStaff()));
    }

    Edu effectiveDura = musxEntry->duration;
    if (forTremolo && tupletDef) {
        effectiveDura = tupletDef->calcReferenceDuration().calcEduDuration();
    }
    auto mnxEvent = content.append<mnx::sequence::Event>();
    const auto noteValue = mnxNoteValueFromEdu(effectiveDura);
    mnxEvent.ensure_duration(noteValue.base, noteValue.dots);
    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));
    createLyrics(context, mnxEvent, musxEntryInfo);
    createMarkings(context, mnxEvent, musxEntry);
    /// @todo orient
    createSlurs(context, mnxEvent, musxEntryInfo);
    if (const auto& crossedStaffId = musxEntryInfo.calcCrossedStaffForAll()) {
        if (const auto& mnxPartStaff = context->mnxPartStaffFromStaff(crossedStaffId.value())) {
            mnxEvent.set_staff(mnxPartStaff.value());
        } else {
            context->logMessage(LogMsg() << " entry has cross-staffing to a staff (" << crossedStaffId.value()
                << ") that is not included in the MNX part.", LogSeverity::Warning);
        }
    }
    const auto [freezeStem, upStem] = musxEntryInfo.calcEntryStemSettings();
    if (musxEntry->isNote && musxEntry->hasStem()) {
        if (freezeStem) {
            mnxEvent.set_stemDirection(upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
        } else if (hasVoice1Voice2) {
            // force all stems in v1v2 contexts due to voices controlling stem direction otherwise.
            mnxEvent.set_stemDirection(musxEntryInfo.calcUpStem() ? mnx::StemDirection::Up : mnx::StemDirection::Down);
        }
    }

    if (musxEntry->isNote) {
        createNotes(context, mnxEvent, musxEntryInfo, musxStaff);
    } else {
        createRest(context, mnxEvent, musxEntryInfo, musxStaff);
    }
    return mnxEvent;
}

/// @brief processes as many entries as it can and returns the next entry to process up to the caller
static EntryInfoPtr::InterpretedIterator addEntryToContent(const MnxMusxMappingPtr& context,
    mnx::ContentArray content, const EntryInfoPtr::InterpretedIterator& firstEntryInfo,
    musx::util::Fraction& elapsedInSequence, bool hasVoice1Voice2,
    bool inGrace, const std::optional<size_t>& tupletIndex = std::nullopt, bool inTremolo = false)
{
    auto musxGraceOptions = context->document->getOptions()->get<options::GraceNoteOptions>();
    auto next = firstEntryInfo;
    while (next) {
        if (tupletIndex) {
            const auto& currentTuplet = next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()];
            if (next.getEntryInfo().getIndexInFrame() > currentTuplet.endIndex) {
                // We've already processed the last event of this tuplet
                // (possibly inside a nested tuplet call) – hand back control.
                return next;
            }
        }

        auto entryInfo = next.getEntryInfo();
        auto entry = entryInfo->getEntry();
        if (inGrace && !entry->graceNote) {
            return next;
        } else if (!inGrace && entry->graceNote) {
            auto grace = content.append<mnx::sequence::Grace>();
            next = addEntryToContent(context, grace.content(), next, elapsedInSequence, hasVoice1Voice2, true);
            if (!musxGraceOptions) {
                throw std::invalid_argument("Document contains no grace note options!");
            }
            bool slash = (entry->slashGrace || musxGraceOptions->slashFlaggedGraceNotes)
                        && entryInfo.calcCanBeBeamed() && entryInfo.calcUnbeamed();
            grace.set_or_clear_slash(slash);
            continue;
        }

        if (next.calcIsPastLogicalEndOfFrame()) {
            return {};
        }
        const auto currElapsedDuration = next.getEffectiveElapsedDuration();
        const auto measureDuration = next.getEffectiveMeasureStaffDuration();
        if (currElapsedDuration >= measureDuration) {
            if (currElapsedDuration > measureDuration) {
                if (auto prev = next.getPrevious(); prev && prev.getEffectiveElapsedDuration() < next.getEffectiveMeasureStaffDuration()) {
                    context->logMessage(LogMsg() << "Entry " << prev.getEntryInfo()->getEntry()->getEntryNumber() << " at index " << prev.getEntryInfo().getIndexInFrame()
                        << " exceeds the measure length.", LogSeverity::Warning);
                }
            }
            if (tupletIndex) { // keep tuplets together, even if they exceed the measure
                context->logMessage(LogMsg()
                    << "Tuplet exceeds the measure length. This is not supported in MNX. Results may be unpredictable.", LogSeverity::Warning);
            }
        }

        ASSERT_IF(currElapsedDuration < elapsedInSequence) {
            throw std::logic_error("Next entry's elapsed duration value is smaller than tracked duration for sequence.");
        }
        if (currElapsedDuration > elapsedInSequence) {
            content.append<mnx::sequence::Space>(mnxFractionFromFraction(currElapsedDuration - elapsedInSequence));
            elapsedInSequence = currElapsedDuration;
        }

        if (tupletIndex) {
            auto tuplInfo = next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()];
            if (tuplInfo.endIndex == next.getEntryInfo().getIndexInFrame()) {
                auto thisTupletIndex = next.getEntryInfo().calcNextTupletIndex(tupletIndex);
                if (!thisTupletIndex || next.getEntryInfo().getFrame()->tupletInfo[thisTupletIndex.value()].startIndex != next.getEntryInfo().getIndexInFrame()) {
                    createEvent(context, content, next.getEntryInfo(), next.getEffectiveHidden(), hasVoice1Voice2, tuplInfo.tuplet, inTremolo);
                    elapsedInSequence = currElapsedDuration + next.getEntryInfo()->actualDuration;
                    return next.getNext();
                }
            }
        }

        if (!inGrace) {
            auto thisTupletIndex = next.getEntryInfo().calcNextTupletIndex(tupletIndex);
            if (thisTupletIndex != tupletIndex && thisTupletIndex) {
                auto tuplInfo = next.getEntryInfo().getFrame()->tupletInfo[thisTupletIndex.value()];
                if (tuplInfo.calcIsTremolo()) {
                    const auto numBeams = next.getEntryInfo().calcNumberOfBeams();
                    const auto numFlagsInRef = calcNumberOfBeamsInEdu(tuplInfo.tuplet->calcReferenceDuration().calcEduDuration());
                    if (numFlagsInRef >= numBeams) {
                        context->logMessage(LogMsg() << "not enough flags or beams to create a tremolo. Setting tremolo marks to 1.", LogSeverity::Warning);
                    }
                    const int marks = static_cast<int>(numFlagsInRef < numBeams ? numBeams - numFlagsInRef : 0);
                    auto tremolo = createMultiNoteTremolo(content, tuplInfo, marks);
                    next = addEntryToContent(context, tremolo.content(), next, elapsedInSequence, hasVoice1Voice2, inGrace, thisTupletIndex, /*inTremolo*/true);
                    continue;
                } else {
                    auto tuplet = createTuplet(content, tuplInfo);
                    next = addEntryToContent(context, tuplet.content(), next, elapsedInSequence, hasVoice1Voice2, inGrace, thisTupletIndex);
                    continue;
                }
            }
        }

        auto tupletDef = [&]() -> MusxInstance<details::TupletDef> {
            if (tupletIndex) {
                return next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()].tuplet;
            }
            return nullptr;
        }();

        const auto addedEvent = createEvent(context, content, next.getEntryInfo(), next.getEffectiveHidden(), hasVoice1Voice2, tupletDef, inTremolo);

        if (addedEvent && addedEvent->measure()) {
            elapsedInSequence = currElapsedDuration + next.getEffectiveMeasureStaffDuration();
        } else {
            elapsedInSequence = currElapsedDuration + next.getEffectiveActualDuration();
        }
        next = next.getNext();
        if (inGrace && next) {
            const auto nextInfo = next.getEntryInfo();
            if (nextInfo.calcUnbeamed() || nextInfo.calcIsBeamStart()) {
                // each beam group should be coded as a separate grace note sequence, so that we can control
                // the slashes.
                break;
            }
        }
    }
    return next;
}

void createSequences(const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffCmper)
{
    auto gfhold = details::GFrameHoldContext(musxMeasure->getDocument(), musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper());
    if (!gfhold) {
        return; // nothing to do
    }
    if (gfhold.calcIsCuesOnly()) {
        context->logMessage(LogMsg() << " skipping cues until MNX committee decides how to handle them.", LogSeverity::Verbose);
        return;
    }
    const auto measureDuration = musxMeasure->calcDuration(staffCmper);
    const std::map<LayerIndex, int> layerVoices = gfhold.calcVoices();
    for (const auto& [layer, numV2] : layerVoices) {
        const int maxVoices = numV2 ? 2 : 1;
        if (auto entryFrame = gfhold.createEntryFrame(layer)) {
            const bool usesV1V2 = numV2 && entryFrame->getFirstInterpretedIterator(2); // ignore entries the iterator will skip
            auto entries = entryFrame->getEntries();
            if (!entries.empty()) {
                for (int voice = 1; voice <= maxVoices; voice++) {
                    if (auto firstEntry = entryFrame->getFirstInterpretedIterator(voice)) {
                        auto sequence = mnxMeasure.sequences().append();
                        if (mnxStaffNumber) {
                            sequence.set_staff(mnxStaffNumber.value());
                        }
                        context->voice = calcVoice(mnxStaffNumber.value_or(1), layer, voice);
                        sequence.set_voice(context->voice);
                        auto elapsedInVoice = musx::util::Fraction(0);
                        addEntryToContent(context, sequence.content(), firstEntry, elapsedInVoice, usesV1V2, false);
                        appendMeasureRemainderSpaces(sequence.content(), elapsedInVoice, measureDuration);
                        context->voice.clear();
                    }
                }
            }
        }
    }
}

void finalizeJumpTies(const MnxMusxMappingPtr& context)
{
    if (context->deferredJumpTies.empty()) {
        return;
    }

    std::unordered_set<std::string> clearedLvTies;
    std::unordered_map<std::string, std::optional<mnx::SlurTieSide>> consensusSides;
    for (const auto& deferred : context->deferredJumpTies) {
        const auto noteIt = context->noteJsonById.find(deferred.startNoteId);
        if (noteIt == context->noteJsonById.end()) {
            continue;
        }

        mnx::sequence::NoteBase startNote(context->mnxDocument->root(), noteIt->second);
        const auto consensusSide = [&]() -> std::optional<mnx::SlurTieSide> {
            if (const auto cached = consensusSides.find(deferred.startNoteId); cached != consensusSides.end()) {
                return cached->second;
            }
            std::optional<mnx::SlurTieSide> side;
            bool hasNonLv = false;
            if (auto tiesOpt = startNote.ties()) {
                auto ties = tiesOpt.value();
                for (size_t i = 0; i < ties.size(); i++) {
                    auto tie = ties.at(i);
                    if (tie.lv()) {
                        continue;
                    }
                    hasNonLv = true;
                    if (!tie.side()) {
                        side.reset();
                        hasNonLv = false;
                        break;
                    }
                    if (!side) {
                        side = tie.side().value();
                    } else if (side.value() != tie.side().value()) {
                        side.reset();
                        hasNonLv = false;
                        break;
                    }
                }
            }
            if (!hasNonLv) {
                side.reset();
            }
            consensusSides.emplace(deferred.startNoteId, side);
            return side;
        }();
        if (clearedLvTies.insert(deferred.startNoteId).second) {
            if (auto tiesOpt = startNote.ties()) {
                auto ties = tiesOpt.value();
                for (size_t i = ties.size(); i-- > 0;) {
                    if (ties.at(i).lv()) {
                        ties.erase(i);
                    }
                }
                if (ties.size() == 0) {
                    startNote.clear_ties();
                }
            }
        }

        auto mnxTies = startNote.ensure_ties();
        bool alreadyLinked = false;
        for (size_t i = 0; i < mnxTies.size(); i++) {
            auto tie = mnxTies.at(i);
            if (tie.target() && tie.target().value() == deferred.endNoteId) {
                alreadyLinked = true;
                break;
            }
        }
        if (alreadyLinked) {
            continue;
        }

        auto mnxTie = mnxTies.append();
        mnxTie.set_target(deferred.endNoteId);
        mnxTie.set_targetType(mnx::TieTargetType::CrossJump);
        if (deferred.side) {
            mnxTie.set_side(deferred.side.value());
        } else if (consensusSide) {
            mnxTie.set_side(consensusSide.value());
        }
    }
}

} // namespace mnxexp
} // namespace denigma
