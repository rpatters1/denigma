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

#include <optional>
#include <string>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace notehead {

/// @enum Shape
/// @brief Notehead shape families recognized by Denigma's classifier.
enum class Shape
{
    Unclassified,
    /// A real alternate-notehead glyph was found (the source definitely specifies a non-default
    /// notehead), but it does not belong to one of the specifically recognized shape families below.
    Other,
    Null,
    Regular,
    X,
    Diamond,
    SmallSlash,
    LargeSlash,
    Circled
};

/// @enum Fill
/// @brief Distinguishes a filled (solid) notehead glyph from an unfilled (open/outline) one.
enum class Fill
{
    /// Fill state is not known or not applicable (e.g. for #Shape::Unclassified or #Shape::Other).
    Unspecified,
    Filled,
    Unfilled
};

} // namespace notehead

/// @struct NoteheadClassification
/// @brief Result returned by notehead classification.
struct NoteheadClassification
{
    /// Classified notehead shape family.
    notehead::Shape shape{ notehead::Shape::Unclassified };
    /// Filled/unfilled state of the glyph. #notehead::Fill::Unspecified when not known or not applicable.
    notehead::Fill fill{ notehead::Fill::Unspecified };
    /// SMuFL glyph name associated with the recognized symbol, when available.
    std::optional<std::string> glyphName;
    /// The resolved font, character, size, and offset that produced this classification.
    musx::dom::NoteInfoPtr::NoteheadInfo noteheadInfo;

    /// Returns true when the source was recognized as a notehead.
    explicit operator bool() const noexcept
    { return shape != notehead::Shape::Unclassified; }
};

/// Classifies a notehead symbol from a font and character code.
NoteheadClassification classifyNoteheadSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol);

/// Classifies the effective notehead for a note, resolving the note's font/character
/// via #musx::dom::NoteInfoPtr::calcNoteheadInfo.
NoteheadClassification classifyNotehead(const musx::dom::NoteInfoPtr& note);

} // namespace classify
} // namespace denigma
