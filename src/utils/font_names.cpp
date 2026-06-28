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

#include <array>
#include <utility>

#include "utils/stringutils.h"

namespace utils {
namespace {

static constexpr auto finaleToSmuflFontMap = std::to_array<std::pair<std::string_view, std::string_view>>({
    std::pair<std::string_view, std::string_view>{ "ashmusic", "Finale Ash" },
    std::pair<std::string_view, std::string_view>{ "broadwaycopyist", "Finale Broadway" },
    std::pair<std::string_view, std::string_view>{ "engraver", "Finale Engraver" },
    std::pair<std::string_view, std::string_view>{ "engraverfontset", "Finale Engraver" },
    std::pair<std::string_view, std::string_view>{ "jazz", "Finale Jazz" },
    std::pair<std::string_view, std::string_view>{ "maestro", "Finale Maestro" },
    std::pair<std::string_view, std::string_view>{ "petrucci", "Finale Legacy" },
    std::pair<std::string_view, std::string_view>{ "pmusic", "Finale Maestro" },
    std::pair<std::string_view, std::string_view>{ "sonata", "Finale Maestro" },
});

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
    const std::string normalized = normalizedFontName(fontName);
    for (const auto& [finaleNameKey, smuflName] : finaleToSmuflFontMap) {
        if (finaleNameKey == normalized || normalizedFontName(smuflName) == normalized) {
            return smuflName;
        }
    }
    return std::nullopt;
}

bool isFinaleLegacyMusicFontMappedToSmufl(std::string_view fontName)
{
    return mappedSmuflFontForFinaleLegacyFont(fontName).has_value();
}

} // namespace utils
