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
            case details::TupletDef::NumberStyle::RatioPlusDenominatorNote: return mnx::TupletDisplaySetting::NoNumber; // should be Outer, but this is not currently an option
            case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnx::TupletDisplaySetting::Both;
        }
        return mnx::TupletDisplaySetting::NoNumber;
    }());

    return mnxTuplet;
}

static void createEvent(mnx::ContentArray content, const std::shared_ptr<const EntryInfo>& musxEntryInfo, std::optional<int> mnxStaffNumber)
{
    const auto& musxEntry = musxEntryInfo->getEntry();
    if (musxEntry->isHidden) {
        content.append<mnx::sequence::Space>(1, mnxNoteValueFromEdu(musxEntry->duration));
        return;
    }

    auto musxStaff = musx::dom::others::StaffComposite::createCurrent(
        musxEntry->getDocument(), musxEntry->getPartId(), musxEntryInfo->getStaff(),
        musxEntryInfo->getMeasure(), Edu(std::lround(musxEntryInfo->elapsedDuration.calcEduDuration())));
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber())
            + " has no staff information for staff " + std::to_string(musxEntryInfo->getStaff()));
    }
    
    auto mnxEvent = content.append<mnx::sequence::Event>(mnxNoteValueFromEdu(musxEntry->duration));
    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));
    /// @todo markings
    /// @todo orient
    /// @todo slurs
    if (musxEntry->freezeStem) {
        mnxEvent.set_stemDirection(musxEntry->upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
    }
    if (mnxStaffNumber) {
        mnxEvent.set_staff(mnxStaffNumber.value());
    }

    if (musxEntry->isNote) {
        for (const auto& musxNote : musxEntry->notes) {
            if (!mnxEvent.notes()) {
                mnxEvent.create_notes();
            }
            auto [noteName, octave, alteration, _] = musxNote->calcNoteProperties(musxEntryInfo->getKeySignature(), musxEntryInfo->clefIndex);
            auto mnxAlter = (alteration == 0 && musxNote->harmAlt == 0 && (!musxNote->showAcci || !musxNote->freezeAcci))
                          ? std::nullopt
                          : std::optional<int>(alteration);
            auto mnxNote = mnxEvent.notes().value().append(enumConvert<mnx::NoteStep>(noteName), octave, mnxAlter);
            /// @todo accidental display
            mnxNote.set_id(calcNoteId(musxNote->getNoteId()));
            /// @todo tie
        }
    } else {
        auto mnxRest = mnxEvent.create_rest();
        if (!musxEntry->floatRest && !musxEntry->notes.empty()) {
            auto staffPosition = std::get<3>(musxEntry->notes[0]->calcNoteProperties(musxEntryInfo->getKeySignature(), musxEntryInfo->clefIndex));
            mnxRest.set_staffPosition(mnxStaffPosition(musxStaff, staffPosition));
        }
    }
}

/// @brief processes as many entries as it can and returns the next entry to process up to the caller
static std::shared_ptr<const EntryInfo> addEntryToContent(const MnxMusxMappingPtr& context,
    mnx::ContentArray content, const std::shared_ptr<const EntryInfo>& firstEntryInfo,
    std::optional<int> mnxStaffNumber, musx::util::Fraction& elapsedInSequence,
    bool inGrace, const std::optional<size_t>& tupletIndex = std::nullopt)
{
    auto next = firstEntryInfo;
    int voice = next->getEntry()->voice2 ? 2 : 1;
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

        ASSERT_IF(next->elapsedDuration < elapsedInSequence) {
            throw std::logic_error("Next entry's elapsed duration value is smaller than tracked duration for sequence.");
        }
        if (next->elapsedDuration > elapsedInSequence) {
            auto [count, value] = mnxNoteValueQuantityFromFraction(context, next->elapsedDuration - elapsedInSequence);
            content.append<mnx::sequence::Space>(count, value);
        }

        if (tupletIndex) {
            auto tuplInfo = next->getFrame()->tupletInfo[tupletIndex.value()];
            if (tuplInfo.endIndex == next->indexInFrame) {
                createEvent(content, next, mnxStaffNumber);
                elapsedInSequence = next->elapsedDuration + next->actualDuration;
                return next->getNextInVoice(voice);
            }
        }

        if (!inGrace) {
            auto thisTupletIndex = next->calcNextTupletIndex(tupletIndex);
            if (thisTupletIndex != tupletIndex && thisTupletIndex) {
                auto tuplInfo = next->getFrame()->tupletInfo[thisTupletIndex.value()];
                auto tuplet = createTuplet(content, tuplInfo.tuplet);
                next = addEntryToContent(context, tuplet.content(), next, mnxStaffNumber, elapsedInSequence, inGrace, thisTupletIndex);
                continue;
            }
        }

        createEvent(content, next, mnxStaffNumber);
        elapsedInSequence = next->elapsedDuration + next->actualDuration;
        next = next->getNextInVoice(voice);
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
