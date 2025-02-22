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

[[maybe_unused]] static mnx::sequence::Tuplet createTuplet(mnx::ContentArray& content, const std::shared_ptr<const details::TupletDef>& musxTuplet)
{
    auto innerBase = enumConvert<mnx::NoteValueBase>(calcNoteTypeFromEdu(musxTuplet->displayDuration));
    auto innerDots = calcAugmentationDotsFromEdu(musxTuplet->displayDuration);
    auto outerBase = enumConvert<mnx::NoteValueBase>(calcNoteTypeFromEdu(musxTuplet->referenceDuration));
    auto outerDots = calcAugmentationDotsFromEdu(musxTuplet->referenceDuration);
    auto mnxTuplet = content.append<mnx::sequence::Tuplet>(
        musxTuplet->displayNumber, innerBase, innerDots, musxTuplet->referenceNumber, outerBase, outerDots);

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
            case details::TupletDef::NumberStyle::RatioPlusDenominatorNote: return mnx::TupletDisplaySetting::NoNumber;
            case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnx::TupletDisplaySetting::Both;
        }
        return mnx::TupletDisplaySetting::NoNumber;
    }());

    return mnxTuplet;
}

[[maybe_unused]] static mnx::sequence::Event createEvent(mnx::ContentArray& content, const std::shared_ptr<const EntryInfo>& musxEntryInfo)
{
    const auto& musxEntry = musxEntryInfo->getEntry();

    auto base = enumConvert<mnx::NoteValueBase>(calcNoteTypeFromEdu(musxEntry->duration));
    auto dots = calcAugmentationDotsFromEdu(musxEntry->duration);
    auto mnxEvent = content.append<mnx::sequence::Event>(base, dots);

    mnxEvent.set_id(calcEventId(musxEntry->getEntryNumber()));
    /// @todo markings
    /// @todo orient
    /// @todo slurs
    if (musxEntry->freezeStem) {
        mnxEvent.set_stemDirection(musxEntry->upStem ? mnx::StemDirection::Up : mnx::StemDirection::Down);
    }

    if (musxEntry->isNote) {
        for (const auto& musxNote : musxEntry->notes) {
            if (!mnxEvent.notes()) {
                mnxEvent.create_notes();
            }
            auto [noteName, octave, alteration] = musxNote->calcNoteProperties(musxEntryInfo->keySignature->getAlteration().value_or(0));
            auto mnxNote = mnxEvent.notes().value().append(enumConvert<mnx::NoteStep>(noteName), octave, alteration);
            /// @todo accidental display
            mnxNote.set_id(calcNoteId(musxNote->getNoteId()));
            /// @todo tie
        }
    } else {
        [[maybe_unused]] auto mnxRest = mnxEvent.create_rest();
        /// @ todo: set the staff line if musx rest is not floating
    }

    return mnxEvent;
}

} // namespace mnxexp
} // namespace denigma
