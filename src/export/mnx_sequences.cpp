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

static mnx::sequence::Tuplet createTuplet(mnx::ContentArray content, const std::shared_ptr<const details::TupletDef>& musxTuplet)
{
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

static void createSlurs(const MnxMusxMappingPtr&, mnx::sequence::Event& mnxEvent, const std::shared_ptr<const Entry>& musxEntry)
{
    auto createOneSlur = [&](const EntryNumber targetEntry) -> mnx::sequence::Slur {
        if (!mnxEvent.slurs().has_value()) {
            mnxEvent.create_slurs();
        }
        auto result = mnxEvent.slurs().value().append(calcEventId(targetEntry));
        return result;
    };
    if (musxEntry->smartShapeDetail) {
        auto shapeAssigns = musxEntry->getDocument()->getDetails()->getArray<details::SmartShapeEntryAssign>(musxEntry->getPartId(), musxEntry->getEntryNumber());
        for (const auto& assign : shapeAssigns) {
            if (auto shape = musxEntry->getDocument()->getOthers()->get<others::SmartShape>(musxEntry->getPartId(), assign->shapeNum)) {
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
                    case others::SmartShape::ShapeType::DashContouSlurAuto:
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

static void createMarkings(const MnxMusxMappingPtr& context, mnx::sequence::Event& mnxEvent, const std::shared_ptr<const Entry>& musxEntry)
{
    auto articAssigns = context->document->getDetails()->getArray<details::ArticulationAssign>(musxEntry->getPartId(), musxEntry->getEntryNumber());
    for (const auto& asgn : articAssigns) {
        if (!asgn->hide) {
            if (auto artic = context->document->getOthers()->get<others::ArticulationDef>(asgn->getPartId(), asgn->articDef)) {
                std::optional<int> numMarks;
                std::optional<mnx::BreathMarkSymbol> breathMark;
                auto marks = calcMarkingType(artic, numMarks, breathMark);
                for (auto mark : marks) {
                    if (!mnxEvent.markings().has_value()) {
                        mnxEvent.create_markings();
                    }
                    auto mnxMarkings = mnxEvent.markings().value();
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

static void createEvent(const MnxMusxMappingPtr& context, mnx::ContentArray content, const EntryInfoPtr& musxEntryInfo, std::optional<int> mnxStaffNumber)
{
    const auto& musxEntry = musxEntryInfo->getEntry();

    const bool isRestWorkaround = musxEntryInfo.calcIsBeamedRestWorkaroud();
    if (musxEntry->isHidden && !isRestWorkaround) {
        content.append<mnx::sequence::Space>(mnxFractionFromEdu(musxEntry->duration));
        return;
    }
    MUSX_ASSERT_IF(musxEntry->voice2 && isRestWorkaround) {
        throw std::logic_error("cannot create an MNX event for a voice2 entry that is part of a beaming workaround");
    }
    if (isRestWorkaround) {
        context->visifiedEntries.emplace(musxEntry->getEntryNumber());
        context->logMessage(LogMsg() << "recognized hidden rest " << musxEntry->getEntryNumber()
            << " as beaming workaround and converted it to visible.", LogSeverity::Verbose);
    }

    auto musxStaff = musxEntryInfo.createCurrentStaff();
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber())
            + " has no staff information for staff " + std::to_string(musxEntryInfo.getStaff()));
    }
    
    auto mnxEvent = content.append<mnx::sequence::Event>(mnxNoteValueFromEdu(musxEntry->duration));
    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));

    auto createLyrics = [&](const auto& musxLyrics) {
        using PtrType = typename std::decay_t<decltype(musxLyrics)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<details::LyricAssign, T>, "musxLyrics must be a subtype of LyricAssign");
        for (const auto& lyr : musxLyrics) {
            if (auto lyrText = musxEntry->getDocument()->getTexts()->get<typename T::TextType>(lyr->lyricNumber)) {
                if (lyr->syllable > lyrText->syllables.size()) {
                    context->logMessage(LogMsg() << " Layer " << musxEntryInfo.getLayerIndex() + 1
                        << " Entry index " << musxEntryInfo.getIndexInFrame() << " has an invalid syllable number ("
                        << lyr->syllable << ").", LogSeverity::Warning);
                } else {
                    if (!mnxEvent.lyrics().has_value()) {
                        auto mnxLyrics = mnxEvent.create_lyrics();
                        mnxLyrics.create_lines();
                    }
                    const size_t sylIndex = size_t(lyr->syllable - 1); // Finale syllable numbers are 1-based.
                    auto mnxLyricLine = mnxEvent.lyrics().value().lines().value().append(
                        calcLyricLineId(std::string(T::TextType::XmlNodeName), lyr->lyricNumber),
                        lyrText->syllables[sylIndex]->syllable 
                    );
                    mnxLyricLine.set_type(mnxLineTypeFromLyric(lyrText->syllables[sylIndex]));
                }
            }
        }
    };
    createLyrics(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(musxEntry->getPartId(), musxEntry->getEntryNumber()));
    createLyrics(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignChorus>(musxEntry->getPartId(), musxEntry->getEntryNumber()));
    createLyrics(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignSection>(musxEntry->getPartId(), musxEntry->getEntryNumber()));

    createMarkings(context, mnxEvent, musxEntry);
    /// @todo orient
    createSlurs(context, mnxEvent, musxEntry);
    if (musxEntry->freezeStem && musxEntry->isNote && musxEntry->hasStem()) {
        mnxEvent.set_stemDirection(musxEntry->upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
    }
    // since the staff number was set in the sequence, we don't set mnxStaffNumber here.

    if (musxEntry->isNote) {
        for (size_t x = 0; x < musxEntryInfo->getEntry()->notes.size(); x++) {
            auto musxNote = NoteInfoPtr(musxEntryInfo, x);
            if (!mnxEvent.notes()) {
                mnxEvent.create_notes();
            }
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
            auto mnxNote = mnxEvent.notes().value().append(enumConvert<mnx::NoteStep>(noteName), octave, mnxAlter);
            if (musxNote->freezeAcci) {
                auto acciDisp = mnxNote.create_accidentalDisplay(musxNote->showAcci);
                acciDisp.set_force(true);
            }
            mnxNote.set_id(calcNoteId(musxNote));
            if (musxNote->crossStaff) {
                InstCmper noteStaff = musxNote.calcStaff();
                std::optional<int> mnxNoteStaff;
                for (size_t y = 0; y < context->partStaves.size(); y++) {
                    if (context->partStaves[y] == noteStaff) {
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
    } else {
        auto mnxRest = mnxEvent.create_rest();
        if (musxEntryInfo.calcIsFullMeasureRest()) {
            mnxEvent.clear_duration();
            mnxEvent.set_measure(true);
        }
        // If a rest is hidden, it has been detected as a beam workaround, so its staff position is meaningless
        if (!musxEntry->isHidden && !musxEntry->floatRest && !musxEntry->notes.empty()) {
            auto musxRest = NoteInfoPtr(musxEntryInfo, 0);
            auto staffPosition = std::get<3>(musxRest.calcNoteProperties());
            if (mnxEvent.measure() || (mnxEvent.duration() && mnxEvent.duration().value().base() == mnx::NoteValueBase::Whole)) {
                staffPosition += 2; // compensate for discrepancy in Finale vs. MNX whole rest staff positions.
            }
            mnxRest.set_staffPosition(mnxStaffPosition(musxStaff, staffPosition));
        }
    }
}

/// @brief processes as many entries as it can and returns the next entry to process up to the caller
static EntryInfoPtr addEntryToContent(const MnxMusxMappingPtr& context,
    mnx::ContentArray content, const EntryInfoPtr& firstEntryInfo,
    std::optional<int> mnxStaffNumber, musx::util::Fraction& elapsedInSequence,
    bool inGrace, const std::optional<size_t>& tupletIndex = std::nullopt)
{
    auto next = firstEntryInfo;
    int voice = next && next->getEntry()->voice2 ? 2 : 1; // firstEntryInfo can be null
    while (next) {
        auto entry = next->getEntry();
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

        if (entry->voice2 && next.calcIsBeamedRestWorkaroud()) {
            continue; // skip any v2 that is part of a beaming workaround
        }
        const auto currElapsedDuration = next->elapsedDuration - context->duraOffset;
        if (currElapsedDuration >= context->currMeasDura && next->actualDuration > 0) { // zero-length tuplets have 0 actual dura
            if (currElapsedDuration > context->currMeasDura) {
                if (auto prev = next.getPreviousInFrame()) {
                    context->logMessage(LogMsg() << "Entry " << prev->getEntry()->getEntryNumber() << " at index " << prev.getIndexInFrame()
                        << " exceeds the measure length.", LogSeverity::Warning);
                } else {
                    context->logMessage(LogMsg() << "Encountered entry that exceeds the measure length.", LogSeverity::Warning);
                }
            }
            if (tupletIndex) { // keep tuplets together, even if they exceed the measure
                context->logMessage(LogMsg()
                    << "Tuplet exceeds the measure length. This is not supported in MNX. Results may be unpredictable.", LogSeverity::Warning);
            } else {
                return next;
            }
        }

        ASSERT_IF(currElapsedDuration < elapsedInSequence) {
            throw std::logic_error("Next entry's elapsed duration value is smaller than tracked duration for sequence.");
        }
        if (currElapsedDuration > elapsedInSequence) {
            content.append<mnx::sequence::Space>(mnxFractionFromFraction(currElapsedDuration - elapsedInSequence));
        }

        if (tupletIndex) {
            auto tuplInfo = next.getFrame()->tupletInfo[tupletIndex.value()];
            if (tuplInfo.endIndex == next.getIndexInFrame()) {
                createEvent(context, content, next, mnxStaffNumber);
                elapsedInSequence = currElapsedDuration + next->actualDuration;
                return next.getNextInVoice(voice);
            }
        }

        if (!inGrace) {
            auto thisTupletIndex = next.calcNextTupletIndex(tupletIndex);
            if (thisTupletIndex != tupletIndex && thisTupletIndex) {
                auto tuplInfo = next.getFrame()->tupletInfo[thisTupletIndex.value()];
                auto tuplet = createTuplet(content, tuplInfo.tuplet);
                next = addEntryToContent(context, tuplet.content(), next, mnxStaffNumber, elapsedInSequence, inGrace, thisTupletIndex);
                continue;
            }
        }

        createEvent(context, content, next, mnxStaffNumber);
        elapsedInSequence = currElapsedDuration + next->actualDuration;
        next = next.getNextInVoice(voice);
    }
    return next;
}

void createSequences(const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const std::shared_ptr<others::Measure>& musxMeasure,
    InstCmper staffCmper)
{
    auto gfhold = details::GFrameHoldContext(musxMeasure->getDocument(), musxMeasure->getPartId(), staffCmper, musxMeasure->getCmper());
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
                    if (auto firstEntry = entryFrame->getFirstInVoice(voice)) {
                        while (voice == 2 && firstEntry && firstEntry.calcIsBeamedRestWorkaroud()) {
                            firstEntry = firstEntry.getNextInVoice(voice);
                        }
                        if (!firstEntry) {
                            continue;
                        }
                        auto sequence = mnxMeasure.sequences().append();
                        if (mnxStaffNumber) {
                            sequence.set_staff(mnxStaffNumber.value());
                        }
                        context->voice = calcVoice(layer, voice);
                        sequence.set_voice(context->voice);
                        auto sequenceKey = std::make_tuple(context->currStaff, layer, voice);
                        auto it_leftOver = context->leftOverEntries.find(sequenceKey);
                        if (it_leftOver == context->leftOverEntries.end()) {
                            it_leftOver = context->leftOverEntries.emplace(sequenceKey, std::vector<EntryInfoPtr>{}).first;
                        }
                        auto it_duraOffset = context->duraOffsets.find(sequenceKey);
                        if (it_duraOffset == context->duraOffsets.end()) {
                            it_duraOffset = context->duraOffsets.emplace(sequenceKey, 0).first;
                        }
                        context->duraOffset = 0;
                        musx::util::Fraction elapsedInVoice = 0;
                        if (!it_leftOver->second.empty()) {
                            context->duraOffset = it_duraOffset->second;
                            if (auto leftOver = addEntryToContent(context, sequence.content(), it_leftOver->second[0], mnxStaffNumber, elapsedInVoice, false)) {
                                auto& leftOvers = it_leftOver->second;
                                while (!leftOvers.empty() && !leftOvers[0].isSameEntry(leftOver)) {
                                    leftOvers.erase(leftOvers.begin());
                                }
                            } else {
                                it_leftOver->second.clear();
                                it_duraOffset->second = 0;
                            }
                            while (firstEntry && firstEntry->elapsedDuration < elapsedInVoice) {
                                if (!firstEntry->getEntry()->isHidden) {
                                    context->logMessage(LogMsg() << "Entry at index " << firstEntry.getIndexInFrame()
                                        << " overwritten by leftovers from earlier measure.");
                                }
                                firstEntry = firstEntry.getNextInVoice(voice);
                            }
                        }
                        context->duraOffset = 0;
                        if (auto leftOver = addEntryToContent(context, sequence.content(), firstEntry, mnxStaffNumber, elapsedInVoice, false)) {
                            it_duraOffset->second += context->currMeasDura;
                            if (!it_leftOver->second.empty()) {
                                while (leftOver) {
                                    if (!leftOver->getEntry()->isHidden) {
                                        context->logMessage(LogMsg() << " skipping left over note entries while left over note entries from earlier measure exist.");
                                        break;
                                    }
                                    leftOver = leftOver.getNextInVoice(voice);
                                }
                            } else {
                                while (leftOver) {
                                    it_leftOver->second.push_back(leftOver);
                                    leftOver = leftOver.getNextInVoice(voice);
                                }
                            }
                        }
                        context->voice.clear();
                    }
                }
            }
        }
    }
}

} // namespace mnxexp
} // namespace denigma
