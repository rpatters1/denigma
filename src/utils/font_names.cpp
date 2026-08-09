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
#include "font_names.h"

#include "smufl_mapping.h"

#include "utils/stringutils.h"

namespace utils {
namespace {

MusicFontType musicFontTypeFromMapping(smufl_mapping::MusicFontType fontType)
{
    switch (fontType) {
    case smufl_mapping::MusicFontType::Engraving: return MusicFontType::Engraving;
    case smufl_mapping::MusicFontType::Text: return MusicFontType::Text;
    }
    return MusicFontType::Unknown;
}

MusicFontStyle musicFontStyleFromMapping(smufl_mapping::MusicFontStyle fontStyle)
{
    switch (fontStyle) {
    case smufl_mapping::MusicFontStyle::Engraved: return MusicFontStyle::Engraved;
    case smufl_mapping::MusicFontStyle::Handwritten: return MusicFontStyle::Handwritten;
    }
    return MusicFontStyle::Unknown;
}

/// The registry's own key normalization removes whitespace but keeps punctuation, while Finale
/// documents spell the same face with hyphens and underscores as readily as with spaces. Its keys
/// are stored fully normalized, so handing it #normalizedFontName's output matches them directly
/// and makes the lookup as tolerant as the rest of Denigma's font-name handling.
std::optional<smufl_mapping::LegacyFontInfo> legacyFontInfo(std::string_view fontName)
{
    return smufl_mapping::getLegacyFontInfo(normalizedFontName(fontName));
}

} // namespace

std::string normalizedFontName(std::string_view fontName)
{
    std::string result;
    result.reserve(fontName.size());
    for (unsigned char c : fontName) {
        if (isAlphaNumeric(c)) {
            result.push_back(toLowerCase(c));
        }
    }
    return result;
}

std::optional<std::string_view> mappedSmuflFontForFinaleLegacyFont(std::string_view fontName)
{
    // The registry names a successor only where one has been established, and leaves it empty
    // rather than guessing, which is the same answer an unknown font gives a caller here.
    if (const auto legacyFont = legacyFontInfo(fontName)) {
        if (!legacyFont->smuflSuccessorFont.empty()) {
            return legacyFont->smuflSuccessorFont;
        }
    }
    return std::nullopt;
}

bool isFinaleLegacyMusicFontMappedToSmufl(std::string_view fontName)
{
    return mappedSmuflFontForFinaleLegacyFont(fontName).has_value();
}

MusicFontType musicFontTypeForFont(std::string_view fontName)
{
    if (const auto smuflFont = smufl_mapping::getSmuflFontInfo(normalizedFontName(fontName))) {
        return musicFontTypeFromMapping(smuflFont->fontType);
    }
    if (const auto legacyFont = legacyFontInfo(fontName)) {
        return musicFontTypeFromMapping(legacyFont->fontType);
    }
    return MusicFontType::Unknown;
}

MusicFontStyle musicFontStyleForFont(std::string_view fontName)
{
    if (const auto smuflFont = smufl_mapping::getSmuflFontInfo(normalizedFontName(fontName))) {
        return musicFontStyleFromMapping(smuflFont->fontStyle);
    }
    if (const auto legacyFont = legacyFontInfo(fontName)) {
        return musicFontStyleFromMapping(legacyFont->fontStyle);
    }
    return MusicFontStyle::Unknown;
}

std::optional<double> legacySmuflSizeRatioForFont(std::string_view fontName)
{
    if (const auto legacyFont = legacyFontInfo(fontName)) {
        return legacyFont->smuflSizeRatio();
    }
    return std::nullopt;
}

} // namespace utils
