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

namespace denigma {
namespace mnxexp {

static mnx::sequence::MultiNoteTremolo createMultiNoteTremolo(mnx::ContentArray content, const musx::dom::EntryFrame::TupletInfo& tupletInfo, int numberOfBeams)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    const auto entryCount = static_cast<unsigned>(tupletInfo.numEntries());
    auto mnxTremolo = content.append<mnx::sequence::MultiNoteTremolo>(numberOfBeams, entryCount, mnxNoteValueFromEdu((musxTuplet->calcReferenceDuration() / entryCount).calcEduDuration()));
    /// @todo: additional fields (like noteheads) when defined by MNX committee.
    return mnxTremolo;
}

static mnx::sequence::Tuplet createTuplet(mnx::ContentArray content, const musx::dom::EntryFrame::TupletInfo& tupletInfo)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    auto mnxTuplet = content.append<mnx::sequence::Tuplet>(
        musxTuplet->displayNumber, mnxNoteValueFromEdu(musxTuplet->displayDuration),
        musxTuplet->referenceNumber, mnxNoteValueFromEdu(musxTuplet->referenceDuration));

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

static void createSlurs(const MnxMusxMappingPtr&, mnx::sequence::Event& mnxEvent, const MusxInstance<Entry>& musxEntry)
{
    auto createOneSlur = [&](const EntryNumber targetEntry) -> mnx::sequence::Slur {
        auto mnxSlurs = mnxEvent.create_slurs();
        return mnxSlurs.append(calcEventId(targetEntry));
    };
    if (musxEntry->smartShapeDetail) {
        auto shapeAssigns = musxEntry->getDocument()->getDetails()->getArray<details::SmartShapeEntryAssign>(SCORE_PARTID, musxEntry->getEntryNumber());
        for (const auto& assign : shapeAssigns) {
            if (auto shape = musxEntry->getDocument()->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum)) {
                if (shape->startTermSeg->endPoint->entryNumber != musxEntry->getEntryNumber()) {
                    continue;
                }
                std::optional<mnx::SlurTieSide> side;
                mnx::LineType lineType = mnx::LineType::Solid;
                switch (shape->shapeType) {
                    default:
                        continue;
                    case others::SmartShape::ShapeType::SlurAuto:
                        break;
                    case others::SmartShape::ShapeType::DashSlurAuto:
                    case others::SmartShape::ShapeType::DashContourSlurAuto:
                        lineType = mnx::LineType::Dashed;
                        break;
                    case others::SmartShape::ShapeType::SlurUp:
                        side = mnx::SlurTieSide::Up;
                        break;
                    case others::SmartShape::ShapeType::DashSlurUp:
                    case others::SmartShape::ShapeType::DashContourSlurUp:
                        lineType = mnx::LineType::Dashed;
                        side = mnx::SlurTieSide::Up;
                        break;
                    case others::SmartShape::ShapeType::SlurDown:
                        side = mnx::SlurTieSide::Down;
                        break;
                    case others::SmartShape::ShapeType::DashSlurDown:
                    case others::SmartShape::ShapeType::DashContourSlurDown:
                        lineType = mnx::LineType::Dashed;
                        side = mnx::SlurTieSide::Down;
                        break;
                }
                auto mnxSlur = createOneSlur(shape->endTermSeg->endPoint->entryNumber);
                mnxSlur.set_lineType(lineType);
                if (side) {
                    mnxSlur.set_side(side.value());
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
                    auto mnxMarkings = mnxEvent.create_markings();
                    switch (mark) {
                        case EventMarkingType::Accent:
                            if (!mnxMarkings.accent().has_value()) {
                                mnxMarkings.create_accent();
                            }
                            break;
                        case EventMarkingType::Breath:
                            if (!mnxMarkings.breath().has_value()) {
                                mnxMarkings.create_breath();
                            }
                            if (breathMark.has_value()) {
                                mnxMarkings.breath().value().set_symbol(breathMark.value());
                            }
                            break;
                        case EventMarkingType::SoftAccent:
                            if (!mnxMarkings.softAccent().has_value()) {
                                mnxMarkings.create_softAccent();
                            }
                            break;
                        case EventMarkingType::Spiccato:
                            if (!mnxMarkings.spiccato().has_value()) {
                                mnxMarkings.create_spiccato();
                            }
                            break;
                        case EventMarkingType::Staccatissimo:
                            if (!mnxMarkings.staccatissimo().has_value()) {
                                mnxMarkings.create_staccatissimo();
                            }
                            break;
                        case EventMarkingType::Staccato:
                            if (!mnxMarkings.staccato().has_value()) {
                                mnxMarkings.create_staccato();
                            }
                            break;
                        case EventMarkingType::Stress:
                            if (!mnxMarkings.stress().has_value()) {
                                mnxMarkings.create_stress();
                            }
                            break;
                        case EventMarkingType::StrongAccent:
                            if (!mnxMarkings.strongAccent().has_value()) {
                                mnxMarkings.create_strongAccent();
                            }
                            break;
                        case EventMarkingType::Tenuto:
                            if (!mnxMarkings.tenuto().has_value()) {
                                mnxMarkings.create_tenuto();
                            }
                            break;
                        case EventMarkingType::Tremolo:
                            if (!mnxMarkings.tremolo().has_value()) {
                                mnxMarkings.create_tremolo(numMarks.value_or(0));
                            }
                            break;
                        case EventMarkingType::Unstress:
                            if (!mnxMarkings.unstress().has_value()) {
                                mnxMarkings.create_unstress();
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
    auto mnxAlter = (alteration == 0 && musxNote->harmAlt == 0 && (!musxNote->showAcci || !musxNote->freezeAcci))
        ? std::nullopt
        : std::optional<int>(alteration);
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
    auto mnxNote = mnxEvent.create_notes().append(enumConvert<mnx::NoteStep>(noteName), octave, mnxAlter);
    if (musxNote->freezeAcci) {
        auto acciDisp = mnxNote.create_accidentalDisplay(musxNote->showAcci);
        acciDisp.set_force(true);
    }
    const auto musxEntry = musxNote.getEntryInfo()->getEntry();
    if (musxEntry->noteDetail) {
        for (const auto& linkedPart : context->musxParts) {
            if (auto noteAlts = musxEntry->getDocument()->getDetails()->getForNote<details::NoteAlterations>(musxNote, linkedPart->getCmper())) {
                if (noteAlts->enharmonic) {
                    auto [enharmonicLev, enharmonicAlt] = musxNote.calcDefaultEnharmonic();
                    auto mnxWritten = mnxNote.create_written();
                    mnxWritten.set_diatonicDelta(enharmonicLev - musxNote->harmLev);
                    break;
                }
            }
        }
    }
    return mnxNote;
}

mnx::sequence::KitNote createKitNote(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const MusxInstance<others::PercussionNoteInfo>& percNoteInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    auto mnxNote = mnxEvent.create_kitNotes().append(calcPercussionKitId(percNoteInfo));
    auto part = mnxNote.getEnclosingElement<mnx::Part>();
    MNX_ASSERT_IF(!part.has_value()) {
        throw std::logic_error("Note created without a part.");
    }
    part->create_kit();
    if (!part->kit()->contains(mnxNote.kitComponent())) {
        auto kitElement = part->kit()->append(mnxNote.kitComponent(), mnxStaffPosition(musxStaff, percNoteInfo->calcStaffReferencePosition()));
        const auto& percNoteType = percNoteInfo->getNoteType();
        if (percNoteType.instrumentId != 0) {
            kitElement.set_name(percNoteType.createName(percNoteInfo->getNoteTypeOrderId()));
            kitElement.set_sound(calcPercussionSoundId(percNoteInfo));
            auto sounds = context->mnxDocument->global().create_sounds();
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
    const MusxInstance<others::Staff>& musxStaff, const MusxInstance<others::PercussionNoteInfo>& percNoteInfo, std::optional<int> mnxStaffNumber)
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
    mnxNote.set_id(calcNoteId(musxNote));
    if (musxNote->crossStaff) {
        StaffCmper noteStaff = musxNote.calcStaff();
        std::optional<int> mnxNoteStaff;
        for (size_t y = 0; y < context->currPartStaves.size(); y++) {
            if (context->currPartStaves[y] == noteStaff) {
                mnxNoteStaff = int(y + 1);
                break;
            }
        }
        if (mnxNoteStaff) {
            if (*mnxNoteStaff != mnxStaffNumber.value_or(1)) {
                mnxNote.set_staff(*mnxNoteStaff);
            }
        } else {
            context->logMessage(LogMsg() << " note has cross-staffing to a staff (" << noteStaff
                << ") that is not included in the MNX part.", LogSeverity::Warning);
        }
    }
    if (musxNote->tieStart) {
        auto mnxTies = mnxNote.create_ties();
        auto tiedTo = musxNote.calcTieTo();
        auto mnxTie = (tiedTo && tiedTo->tieEnd && !tiedTo.getEntryInfo()->getEntry()->isHidden)
            ? mnxTies.append(calcNoteId(tiedTo))
            : mnxTies.append();
        if (auto tieAlter = musxEntry->getDocument()->getDetails()->getForNote<details::TieAlterStart>(musxNote)) {
            if (tieAlter->freezeDirection) {
                mnxTie.set_side(tieAlter->down ? mnx::SlurTieSide::Down : mnx::SlurTieSide::Up);
            }
        }
    }
}

static void createNotes(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff, std::optional<int> mnxStaffNumber)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    for (size_t x = 0; x < musxEntry->notes.size(); x++) {
        const auto mnxNote = NoteInfoPtr(musxEntryInfo, x);
        if (const auto percNoteInfo = mnxNote.calcPercussionNoteInfo()) {
            createNote<mnx::sequence::KitNote>(context, mnxEvent, mnxNote, musxStaff, percNoteInfo, mnxStaffNumber);
        } else {
            createNote<mnx::sequence::Note>(context, mnxEvent, mnxNote, musxStaff, nullptr, mnxStaffNumber);
        }
    }
}

static void createRest([[maybe_unused]] const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    auto mnxRest = mnxEvent.create_rest();
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
                    auto mnxLyrics = mnxEvent.create_lyrics();
                    auto mnxLyricsLines = mnxLyrics.create_lines();
                    const size_t sylIndex = size_t(lyr->syllable - 1); // Finale syllable numbers are 1-based.
                    auto mnxLyricLine = mnxLyricsLines.append(
                        calcLyricLineId(std::string(T::TextType::XmlNodeName), lyr->lyricNumber),
                        lyrText->syllables[sylIndex]->syllable 
                    );
                    mnxLyricLine.set_type(mnxLineTypeFromLyric(lyrText->syllables[sylIndex]));
                }
            }
        }
    };
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignChorus>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignSection>(SCORE_PARTID, musxEntry->getEntryNumber()));
}

static void createEvent(const MnxMusxMappingPtr& context, mnx::ContentArray content, EntryInfoPtr musxEntryInfo, std::optional<int> mnxStaffNumber, bool effectiveHidden)
{
    if (auto display = musxEntryInfo.findDisplayEntryForBeamOverBarline()) {
        musxEntryInfo = display;
        effectiveHidden = display->getEntry()->isHidden;
    }

    const auto musxEntry = musxEntryInfo->getEntry();

    if (effectiveHidden) {
        /// @todo include hidden entries perhaps, if MNX starts allowing them.
        content.append<mnx::sequence::Space>(mnxFractionFromEdu(musxEntry->duration));
        return;
    }

    auto musxStaff = musxEntryInfo.createCurrentStaff();
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber())
            + " has no staff information for staff " + std::to_string(musxEntryInfo.getStaff()));
    }
    
    auto mnxEvent = content.append<mnx::sequence::Event>(mnxNoteValueFromEdu(musxEntry->duration));
    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));
    createLyrics(context, mnxEvent, musxEntryInfo);
    createMarkings(context, mnxEvent, musxEntry);
    /// @todo orient
    createSlurs(context, mnxEvent, musxEntry);
    const auto [freezeStem, upStem] = musxEntryInfo.calcEntryStemSettings();
    if (freezeStem && musxEntry->isNote && musxEntry->hasStem()) {
        mnxEvent.set_stemDirection(upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
    }

    if (musxEntry->isNote) {
        createNotes(context, mnxEvent, musxEntryInfo, musxStaff, mnxStaffNumber);
    } else {
        createRest(context, mnxEvent, musxEntryInfo, musxStaff);
    }
}

/// @brief processes as many entries as it can and returns the next entry to process up to the caller
static EntryInfoPtr::InterpretedIterator addEntryToContent(const MnxMusxMappingPtr& context,
    mnx::ContentArray content, const EntryInfoPtr::InterpretedIterator& firstEntryInfo,
    std::optional<int> mnxStaffNumber, musx::util::Fraction& elapsedInSequence,
    bool inGrace, const std::optional<size_t>& tupletIndex = std::nullopt)
{
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

        auto entry = next.getEntryInfo()->getEntry();
        if (inGrace && !entry->graceNote) {
            return next;
        } else if (!inGrace && entry->graceNote) {
            auto grace = content.append<mnx::sequence::Grace>();
            next = addEntryToContent(context, grace.content(), next, mnxStaffNumber, elapsedInSequence, true);
            if (grace.content().size() == 1) {
                auto musxGraceOptions = entry->getDocument()->getOptions()->get<options::GraceNoteOptions>();
                if (!musxGraceOptions) {
                    throw std::invalid_argument("Document contains no grace note options!");
                }
                grace.set_slash(musxGraceOptions->slashFlaggedGraceNotes);
            }
            continue;
        }

        const auto currElapsedDuration = next.getEntryInfo()->elapsedDuration;
        const auto isEndOfFrame = [&]() -> bool {
            if (currElapsedDuration > next.getEntryInfo().getFrame()->measureStaffDuration) {
                return true;
            }
            if (currElapsedDuration < next.getEntryInfo().getFrame()->measureStaffDuration) {
                return false;
            }
            if (next.getEntryInfo().findHiddenSourceForBeamOverBarline()) {
                if (entry->graceNote) {
                    return static_cast<bool>(next.getEntryInfo().findMainEntryForGraceNote());
                }
                return true;
            }
            return false;
        }();
        if (isEndOfFrame) {
            if (currElapsedDuration > next.getEntryInfo().getFrame()->measureStaffDuration) {
                if (auto prev = next.getEntryInfo().getPreviousInFrame(); prev->elapsedDuration < next.getEntryInfo().getFrame()->measureStaffDuration) {
                    context->logMessage(LogMsg() << "Entry " << prev->getEntry()->getEntryNumber() << " at index " << prev.getIndexInFrame()
                        << " exceeds the measure length.", LogSeverity::Warning);
                }
            }
            if (tupletIndex) { // keep tuplets together, even if they exceed the measure
                context->logMessage(LogMsg()
                    << "Tuplet exceeds the measure length. This is not supported in MNX. Results may be unpredictable.", LogSeverity::Warning);
            } else if (next.getEntryInfo().findHiddenSourceForBeamOverBarline()) {
                return {};
            } // allow the extra length in this sequence. Semantic validation catches it.
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
                    createEvent(context, content, next.getEntryInfo(), mnxStaffNumber, next.getEffectiveHidden());
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
                    auto tremolo = createMultiNoteTremolo(content, tuplInfo, next.getEntryInfo().calcNumberOfBeams());
                    next = addEntryToContent(context, tremolo.content(), next, mnxStaffNumber, elapsedInSequence, inGrace, thisTupletIndex);
                    continue;
                } else {
                    auto tuplet = createTuplet(content, tuplInfo);
                    next = addEntryToContent(context, tuplet.content(), next, mnxStaffNumber, elapsedInSequence, inGrace, thisTupletIndex);
                    continue;
                }
            }
        }

        createEvent(context, content, next.getEntryInfo(), mnxStaffNumber, next.getEffectiveHidden());

        elapsedInSequence = currElapsedDuration + next.getEffectiveActualDuration();
        next = next.getNext();
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
    for (LayerIndex layer = 0; layer < MAX_LAYERS; layer++) {
        if (auto entryFrame = gfhold.createEntryFrame(layer)) {
            auto entries = entryFrame->getEntries();
            if (!entries.empty()) {
                for (int voice = 1; voice <= 2; voice++) {
                    if (auto firstEntry = entryFrame->getFirstInterpretedIterator(voice)) {
                        auto sequence = mnxMeasure.sequences().append();
                        if (mnxStaffNumber) {
                            sequence.set_staff(mnxStaffNumber.value());
                        }
                        context->voice = calcVoice(layer, voice);
                        sequence.set_voice(context->voice);
                        musx::util::Fraction elapsedInVoice = 0;
                        addEntryToContent(context, sequence.content(), firstEntry, mnxStaffNumber, elapsedInVoice, false);
                        context->voice.clear();
                    }
                }
            }
        }
    }
}

} // namespace mnxexp
} // namespace denigma
