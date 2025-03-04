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

static void createEvent(const MnxMusxMappingPtr& context, mnx::ContentArray content, const EntryInfoPtr& musxEntryInfo, std::optional<int> mnxStaffNumber)
{
    const auto& musxEntry = musxEntryInfo->getEntry();
    /// @todo handle special case when hidden notes are for beam over barline from previous measure.
    if (musxEntry->isHidden) {
        content.append<mnx::sequence::Space>(1, mnxNoteValueFromEdu(musxEntry->duration));
        return;
    }

    auto musxStaff = musxEntryInfo.createCurrentStaff();
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber())
            + " has no staff information for staff " + std::to_string(musxEntryInfo.getStaff()));
    }
    
    auto mnxEvent = content.append<mnx::sequence::Event>(mnxNoteValueFromEdu(musxEntry->duration));
    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));

    auto musxLyrics = musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(musxEntry->getPartId(), musxEntry->getEntryNumber());
    for (const auto& lyr : musxLyrics) {
        if (auto lyrText = musxEntry->getDocument()->getTexts()->get<texts::LyricsVerse>(lyr->lyricNumber)) {
            if (lyr->syllable > lyrText->syllables.size()) {
                /// @todo log it
            } else {
                if (!mnxEvent.lyrics().has_value()) {
                    auto mnxLyrics = mnxEvent.create_lyrics();
                    mnxLyrics.create_lines();
                }
                [[maybe_unused]]auto mnxLyricLine = mnxEvent.lyrics().value().lines().value().append(
                    calcLyricLineId(std::string(texts::LyricsVerse::XmlNodeName), lyr->lyricNumber),
                    lyrText->syllables[lyr->syllable - 1]->syllable // Finale syllable numbers are 1-based.
                );
                /// @todo "type"
            }
        }
    }

    /// @todo markings
    /// @todo orient
    /// @todo slurs
    if (musxEntry->freezeStem) {
        mnxEvent.set_stemDirection(musxEntry->upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
    }
    // since the staff number was set in the sequence, we don't set mnxStaffNumber here.

    if (musxEntry->isNote) {
        for (size_t x = 0; x < musxEntryInfo->getEntry()->notes.size(); x++) {
            auto musxNote = NoteInfoPtr(musxEntryInfo, x);
            if (!mnxEvent.notes()) {
                mnxEvent.create_notes();
            }
            auto [noteName, octave, alteration, _] = musxNote.calcNoteProperties();
            auto mnxAlter = (alteration == 0 && musxNote->harmAlt == 0 && (!musxNote->showAcci || !musxNote->freezeAcci))
                          ? std::nullopt
                          : std::optional<int>(alteration);
            auto mnxNote = mnxEvent.notes().value().append(enumConvert<mnx::NoteStep>(noteName), octave, mnxAlter);
            if (musxNote->freezeAcci) {
                mnxNote.create_accidentalDisplay(musxNote->showAcci);
            }
            mnxNote.set_id(calcNoteId(musxNote));
            if (musxNote->crossStaff) {
                InstCmper noteStaff = musxNote.calcStaff();
                std::optional<int> mnxNoteStaff;
                for (size_t y = 0; y < context->partStaves.size(); y++) {
                    if (context->partStaves[y] == noteStaff) {
                        mnxNoteStaff = y + 1;
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
                auto mnxTie = mnxNote.create_tie();
                auto tiedTo = musxNote.calcTieTo();
                if (tiedTo && tiedTo->tieEnd) {
                    mnxTie.set_target(calcNoteId(tiedTo));
                } else {
                    mnxTie.set_location(mnx::SlurTieEndLocation::Outgoing);
                }
                if (auto tieAlter = musxEntry->getDocument()->getDetails()->getForNote<details::TieAlterStart>(musxNote)) {
                    if (tieAlter->freezeDirection) {
                        mnxTie.set_side(tieAlter->down ? mnx::SlurTieSide::Down : mnx::SlurTieSide::Up);
                    }
                }
            }
        }
    } else {
        auto mnxRest = mnxEvent.create_rest();
        if (!musxEntry->floatRest && !musxEntry->notes.empty()) {
            auto musxRest = NoteInfoPtr(musxEntryInfo, 0);
            auto staffPosition = std::get<3>(musxRest.calcNoteProperties());
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
        const auto& entry = next->getEntry();
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

        const auto currElapsedDuration = next->elapsedDuration - context->duraOffset;
        if (currElapsedDuration >= context->currMeasDura && next->actualDuration > 0) { // zero-length tuplets have 0 actual dura
            if (currElapsedDuration > context->currMeasDura) {
                if (auto prev = next.getPreviousInFrame()) {
                    context->logMessage(LogMsg() << "Entry " << prev->getEntry()->getEntryNumber() << " at index << " << prev.getIndexInFrame()
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
            auto [count, value] = mnxNoteValueQuantityFromFraction(context, currElapsedDuration - elapsedInSequence);
            content.append<mnx::sequence::Space>(count, value);
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
    auto gfhold = musxMeasure->getDocument()->getDetails()->get<details::GFrameHold>(
        musxMeasure->getPartId(), staffCmper, musxMeasure->getCmper());
    if (!gfhold) {
        return; // nothing to do
    }
    for (LayerIndex layer = 0; layer < MAX_LAYERS; layer++) {
        if (auto entryFrame = gfhold->createEntryFrame(layer)) {
            auto entries = entryFrame->getEntries();
            if (!entries.empty()) {
                for (int voice = 1; voice <= 2; voice++) {
                    if (auto firstEntry = entryFrame->getFirstInVoice(voice)) {
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
