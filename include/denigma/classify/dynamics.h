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

#include <iterator>
#include <optional>
#include <string>
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

/// @struct DynamicMark
/// @brief Dynamic metadata attached to a source text run.
struct DynamicMark
{
    /// Classified dynamic.
    Dynamic dynamic{};
    /// Glyph names that participated in the matched dynamic, in source order.
    /// Empty when any matched character does not resolve to a glyph name.
    std::vector<std::string> glyphs;
    /// Relative direction inferred from unambiguous nearby qualifiers.
    DynamicChange change{ DynamicChange::Absolute };
};

/// @struct DynamicPhraseRun
/// @brief Ordered source chunks, optionally classified as a dynamic mark.
struct DynamicPhraseRun
{
    /// Source chunks covered by this run.
    std::vector<musx::util::EnigmaTextChunk> chunks;
    /// Dynamic metadata when this run is recognized as a dynamic mark.
    std::optional<DynamicMark> dynamic;
};

/// @struct DynamicPhraseClassification
/// @brief Ordered dynamic phrase runs suitable for exporter-specific projection.
struct DynamicPhraseClassification
{
    /// Ordered source runs.
    std::vector<DynamicPhraseRun> runs;

    /// Returns true when any run is recognized as a dynamic.
    [[nodiscard]] bool hasDynamic() const noexcept;

    /// Returns true when any run is recognized as a dynamic.
    explicit operator bool() const noexcept
    { return hasDynamic(); }
};

/// Returns the plain text spanned by a range of dynamic phrase runs.
template <std::input_iterator Iterator>
std::string dynamicRunPlainText(Iterator begin, Iterator end)
{
    std::string text;
    for (auto it = begin; it != end; ++it) {
        text += musx::util::EnigmaString::plainTextFromChunks(it->chunks);
    }
    return text;
}

/// Returns the plain text spanned by dynamic phrase runs.
std::string dynamicRunPlainText(const std::vector<DynamicPhraseRun>& runs);

/// Classifies a Finale text expression definition as an ordered dynamic phrase.
DynamicPhraseClassification classifyDynamic(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def);
/// Classifies an already-resolved Enigma text context as an ordered dynamic phrase.
DynamicPhraseClassification classifyDynamic(const musx::util::EnigmaParsingContext& rawTextCtx, bool isDynamicsCategory = false);
/// Returns the canonical text spelling for a dynamic.
std::string dynamicCanonicalText(Dynamic dynamic);
/// Returns canonical SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalGlyphs(Dynamic dynamic);
/// Returns canonical per-letter SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalLetterGlyphs(Dynamic dynamic);

} // namespace classify
} // namespace denigma
