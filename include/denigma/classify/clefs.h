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

namespace denigma::classify {

/// Result returned by clef classification.
struct ClefClassification
{
    /// Classified music-theory clef type.
    music_theory::ClefType type{};
    /// Octave transposition associated with the clef.
    int octave{};
    /// True when the source clef is a blank Finale clef.
    bool isBlank{};
    /// True when octave information should be shown with the clef.
    bool showOctave{ true };
    /// SMuFL glyph name associated with the clef, when available.
    std::optional<std::string> glyphName;

    /// Returns true when the source was recognized as a clef.
    bool isClef() const noexcept
    { return type != music_theory::ClefType::Unknown; }

    /// Returns true when the source was recognized as a clef.
    explicit operator bool() const noexcept
    { return isClef(); }
};

/// Classifies a Finale clef definition, optionally in the context of a staff.
ClefClassification classifyClef(
    const musx::dom::MusxInstance<musx::dom::options::ClefOptions::ClefDef>& clefDef,
    const musx::dom::MusxInstance<musx::dom::others::Staff>& staff = nullptr);

} // namespace denigma::classify
