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
#include "classify/clefs.h"

#include <string_view>

#include "utils/smufl_support.h"

namespace denigma::classify {

namespace {

static bool isPlainGClef(char32_t symbol, std::optional<std::string_view> glyphName)
{
    return symbol == 0x1D11E || glyphName == "gClef" || glyphName == "gClefSmall";
}

static bool isPlainCClef(char32_t symbol, std::optional<std::string_view> glyphName)
{
    return symbol == 0x1D121 || glyphName == "cClef" || glyphName == "cClefSquare"
        || glyphName == "cClefFrench" || glyphName == "cClefFrench20C";
}

static bool isPlainFClef(char32_t symbol, std::optional<std::string_view> glyphName)
{
    return symbol == 0x1D122 || glyphName == "fClef" || glyphName == "fClefFrench"
        || glyphName == "fClef19thCentury";
}

static bool shouldShowOctave(
    music_theory::ClefType type, int octave, char32_t symbol, std::optional<std::string_view> glyphName)
{
    if (octave == 0) {
        return true;
    }
    switch (type) {
    case music_theory::ClefType::G:
        return !isPlainGClef(symbol, glyphName);
    case music_theory::ClefType::C:
        return !isPlainCClef(symbol, glyphName);
    case music_theory::ClefType::F:
        return !isPlainFClef(symbol, glyphName);
    default:
        return true;
    }
}

} // namespace

ClefClassification classifyClef(
    const musx::dom::MusxInstance<musx::dom::options::ClefOptions::ClefDef>& clefDef,
    const musx::dom::MusxInstance<musx::dom::others::Staff>& staff)
{
    if (!clefDef) {
        return {};
    }

    const bool isBlank = clefDef->isBlank();
    std::optional<std::string> glyphName;
    if (!isBlank) {
        auto clefFont = clefDef->calcFont();
        glyphName = utils::smuflGlyphNameForFont(clefFont, clefDef->clefChar);
    }
    std::optional<std::string_view> glyphNameView;
    if (glyphName) {
        glyphNameView = glyphName.value();
    }

    auto [type, octave] = clefDef->calcInfo(staff);
    return { type, octave, isBlank, shouldShowOctave(type, octave, clefDef->clefChar, glyphNameView), std::move(glyphName) };
}

} // namespace denigma::classify
