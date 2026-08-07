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
#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/denigma.h"
#include "musx/musx.h"

namespace utils {

struct SmuflGlyphMetricsEvpu
{
    musx::dom::EvpuFloat advance{};
    musx::dom::EvpuFloat top{};
    musx::dom::EvpuFloat bottom{};
};

std::optional<std::string> smuflGlyphNameForFont(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t codepoint);

std::optional<std::string> smuflGlyphNameForFont(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, const std::string& text);

std::optional<musx::dom::EvpuFloat> smuflGlyphWidthForFont(const std::string& fontName, const std::string& glyphName);

std::optional<SmuflGlyphMetricsEvpu> smuflGlyphMetricsForFont(const musx::dom::FontInfo& fontInfo, char32_t codepoint);

/// @brief How an exporter turns music-font characters in a text run into portable SMuFL symbols.
///
/// The policies differ only for a run that mixes mappable and unmappable characters in one font,
/// which is what a legacy metronome font produces when a note glyph and its number are typed
/// together. #PreferSmufl keeps such a run as text, preserving the font's kerning between the two
/// but rendering as mojibake wherever that font is missing. #SplitSmufl converts the glyph and
/// leaves the rest as text, losing the kerning but degrading gracefully, since the remaining
/// characters are ordinarily plain digits and punctuation that any fallback font renders correctly.
enum class SmuflSymbolPolicy
{
    PreserveText, ///< Never convert; every character stays text in its original font.
    PreferSmufl,  ///< Convert only when every character in the run maps.
    SplitSmufl    ///< Convert every mappable character, splitting the run around those that do not.
};

/// @brief One stretch of a text run that is either entirely SMuFL glyphs or entirely plain text.
struct SmuflGlyphRun
{
    bool isSmufl{};                  ///< True when this stretch converted to glyphs.
    std::string text;                ///< The source characters of this stretch.
    std::vector<std::string> glyphs; ///< Canonical SMuFL names, one per character, when #isSmufl.
};

/// @brief Maps every character of @p text to a canonical SMuFL glyph name.
/// @return One name per character, or an empty vector if any character has no mapping.
std::vector<std::string> smuflGlyphNamesForText(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, const std::string& text);

/// @brief Splits @p text into alternating glyph and plain-text stretches, coalescing adjacent
/// characters of the same kind.
std::vector<SmuflGlyphRun> smuflSplitRunsByGlyphMapping(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, const std::string& text);

} // namespace utils
