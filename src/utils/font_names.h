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

namespace utils {

/// @brief What a music font looks like, independent of whether it is set in the score or inline
/// with running text.
///
/// The values come from the smufl_mapping font registries, which cover both legacy and SMuFL
/// fonts. Denigma keeps its own enum so that the registry stays a private dependency of this
/// library rather than bubbling up to everything that needs a font name.
enum class MusicFontStyle
{
    Unknown,        ///< Not a font the registries know, so nothing can be said about its look.
    Engraved,       ///< Imitates traditional plate engraving.
    Handwritten     ///< Imitates manuscript.
};

std::string normalizedFontName(std::string_view fontName);

/// @brief The SMuFL font that supersedes a Finale legacy music font, if one is known.
std::optional<std::string_view> mappedSmuflFontForFinaleLegacyFont(std::string_view fontName);

bool isFinaleLegacyMusicFontMappedToSmufl(std::string_view fontName);

/// @brief What @p fontName looks like, for either a legacy or a SMuFL music font.
MusicFontStyle musicFontStyleForFont(std::string_view fontName);

/// @brief The factor converting a point size in a legacy music font to the equivalent point size
/// in a substituted SMuFL font.
///
/// A SMuFL font spans four staff spaces to the em, which is what makes a point size portable
/// between SMuFL fonts. Legacy fonts promise nothing, so the registry records what each one
/// actually does. Returns nullopt when the font is unknown, or known to have no staff-relative
/// size, in which case a substituted glyph's size cannot be derived and must be left unstated.
std::optional<double> legacySmuflSizeRatioForFont(std::string_view fontName);

} // namespace utils
