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

#include <string>
#include <string_view>
#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

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

/// @enum DynamicChange
/// @brief Whether a classified dynamic is absolute or indicates relative motion.
enum class DynamicChange
{
    Absolute,
    RelativeIncrease,
    RelativeDecrease
};

/// @struct DynamicClassification
/// @brief Result returned by dynamic classification.
struct DynamicClassification
{
    /// Classified dynamic.
    Dynamic dynamic{};
    /// True when the source expression includes extra non-dynamic text.
    bool hasAdditionalText{};
    /// Plain text before the classified dynamic, normalized and edge-trimmed.
    std::string prefixText;
    /// Plain text after the classified dynamic, normalized and edge-trimmed.
    std::string suffixText;
    /// Glyph names that participated in the matched dynamic, in source order.
    /// Empty when any matched character does not resolve to a glyph name.
    std::vector<std::string> glyphs;
    /// Relative direction inferred from unambiguous prefix or suffix qualifiers.
    DynamicChange change{ DynamicChange::Absolute };

    /// Returns true when the source was recognized as a dynamic.
    bool isDynamic() const noexcept
    { return dynamic != Dynamic::None; }

    /// Returns true when the source was recognized as a dynamic.
    explicit operator bool() const noexcept
    { return isDynamic(); }
};

/// Classifies a Finale text expression definition as a dynamic marking.
DynamicClassification classifyDynamic(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def);
/// Returns the canonical text spelling for a dynamic.
std::string dynamicCanonicalText(Dynamic dynamic);
/// Returns canonical SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalGlyphs(Dynamic dynamic);
/// Returns canonical per-letter SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalLetterGlyphs(Dynamic dynamic);

} // namespace classify
} // namespace denigma
