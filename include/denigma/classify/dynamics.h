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
#include <string_view>
#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace dynamics {

/// @enum Dynamic
/// @brief Dynamic marking classes recognized by the classifier.
enum class Dynamic
{
    None,
    Other,
    pppppp,
    ppppp,
    pppp,
    ppp,
    pp,
    p,
    mp,
    mf,
    f,
    ff,
    fff,
    ffff,
    fffff,
    ffffff,
    fp,
    ffp,
    fz,
    ffz,
    pf,
    sf,
    sfp,
    sfpp,
    sfz,
    sffz,
    sfzp,
    rf,
    rfz,
    n
};

/// @enum Change
/// @brief Whether a classified dynamic is absolute or indicates relative motion.
enum class Change
{
    Absolute,
    RelativeIncrease,
    RelativeDecrease
};

/// @struct Mark
/// @brief Dynamic metadata attached to a source text run.
struct Mark
{
    /// Classified dynamic.
    Dynamic dynamic{};
    /// Glyph names that participated in the matched dynamic, in source order.
    /// Empty when any matched character does not resolve to a glyph name.
    std::vector<std::string> glyphs;
};

} // namespace dynamics

/// Classifies a source text chunk as a single plain dynamic mark.
std::optional<dynamics::Mark> classifyDynamicRun(const musx::util::EnigmaTextChunk& chunk, bool forceOther = false);
/// Returns the canonical text spelling for a dynamic.
std::string dynamicCanonicalText(dynamics::Dynamic dynamic);
/// Returns canonical SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalGlyphs(dynamics::Dynamic dynamic);
/// Returns canonical per-letter SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalLetterGlyphs(dynamics::Dynamic dynamic);
/// Returns per-letter SMuFL glyph names for recognized dynamic letters, omitting other characters.
std::vector<std::string> dynamicLettersToLetterGlyphs(std::string_view letters);
/// Returns the dynamic letters represented by known dynamic glyph names.
std::string dynamicGlyphsToLetters(const std::vector<std::string>& glyphs);

} // namespace classify
} // namespace denigma
