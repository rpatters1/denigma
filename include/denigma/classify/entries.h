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
#pragma once

#include <variant>
#include <vector>

#include "denigma/classify/noteheads.h"
#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace entry {

/// @struct ArtificialHarmonic
/// @brief Classification for one artificial-harmonic note pair (a stopped note plus a touched node)
/// found within a chord.
struct ArtificialHarmonic
{
    /// @enum TouchInterval
    /// @brief The notated interval from the stopped note up to the touched node.
    enum class TouchInterval
    {
        Fourth,
        MajorThird,
        Fifth
    };

    /// The stopped (fingered) note. Normally has a #notehead::Shape::Regular notehead.
    musx::dom::NoteInfoPtr stoppedNote;
    /// The touched note (harmonic node). Normally has a #notehead::Shape::Diamond notehead.
    musx::dom::NoteInfoPtr touchedNote;
    /// Notehead classification already computed for #stoppedNote.
    NoteheadClassification stoppedNotehead;
    /// Notehead classification already computed for #touchedNote.
    NoteheadClassification touchedNotehead;
    /// Notated interval from #stoppedNote to #touchedNote.
    TouchInterval interval{ TouchInterval::Fourth };
    /// A third note in the chord at the theoretical sounding pitch, if the source explicitly includes one.
    /// Falsy if the chord has no such note.
    musx::dom::NoteInfoPtr soundingNote;
};

/// @struct ArtificialHarmonics
/// @brief One or more artificial-harmonic note pairs found within a chord (e.g. a double-stop harmonic).
struct ArtificialHarmonics
{
    /// The artificial-harmonic note pairs found in the chord.
    std::vector<ArtificialHarmonic> harmonics;
};

/// Variant payload for chord-level notehead-pattern classification.
using EntryNoteheadValue = std::variant<std::monostate, ArtificialHarmonics>;

} // namespace entry

/// @struct EntryNoteheadClassification
/// @brief Result returned by chord-level notehead-pattern classification.
struct EntryNoteheadClassification
{
    /// Classified payload, or std::monostate when no chord-level notehead pattern was recognized.
    entry::EntryNoteheadValue value{};

    /// Returns true when a chord-level notehead pattern was recognized.
    explicit operator bool() const noexcept
    { return !std::holds_alternative<std::monostate>(value); }

    /// Returns the classified payload as T, or nullptr when it has another type.
    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }

    /// Returns true when the classified payload has type T.
    template <typename T>
    bool is() const noexcept
    { return std::holds_alternative<T>(value); }
};

/// Classifies chord-level notehead patterns (e.g. artificial harmonics) for an entry.
/// @details This does not replace per-note notehead classification (#classifyNotehead); it identifies
/// additional structure across the notes of a chord. std::monostate means no such pattern was found,
/// in which case each note's notehead should still be classified individually via #classifyNotehead.
EntryNoteheadClassification classifyEntryNoteheads(const musx::dom::EntryInfoPtr& entry);

} // namespace classify
} // namespace denigma
