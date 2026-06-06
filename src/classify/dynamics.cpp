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
#include "classify/dynamics.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "utils/smufl_support.h"
#include "utils/stringutils.h"

namespace denigma::classify {

namespace {

static std::string trimAscii(std::string_view text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

static std::string normalizeDynamicText(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool previousWasSpace = false;
    for (unsigned char ch : text) {
        if (std::isspace(ch)) {
            previousWasSpace = true;
            continue;
        }
        if (previousWasSpace && !result.empty()) {
            result.push_back(' ');
        }
        previousWasSpace = false;
        result.push_back(static_cast<char>(std::tolower(ch)));
    }
    return trimAscii(result);
}

static std::string glyphNameToDynamicText(std::string_view glyphName)
{
    if (glyphName == "dynamicPiano") {
        return "p";
    }
    if (glyphName == "dynamicPianissimo") {
        return "pp";
    }
    if (glyphName == "dynamicPPP") {
        return "ppp";
    }
    if (glyphName == "dynamicPPPP") {
        return "pppp";
    }
    if (glyphName == "dynamicPPPPP") {
        return "ppppp";
    }
    if (glyphName == "dynamicPPPPPP") {
        return "pppppp";
    }
    if (glyphName == "dynamicMezzo") {
        return "m";
    }
    if (glyphName == "dynamicMP") {
        return "mp";
    }
    if (glyphName == "dynamicMF") {
        return "mf";
    }
    if (glyphName == "dynamicForte") {
        return "f";
    }
    if (glyphName == "dynamicFF") {
        return "ff";
    }
    if (glyphName == "dynamicFFF") {
        return "fff";
    }
    if (glyphName == "dynamicFFFF") {
        return "ffff";
    }
    if (glyphName == "dynamicFFFFF") {
        return "fffff";
    }
    if (glyphName == "dynamicFFFFFF") {
        return "ffffff";
    }
    if (glyphName == "dynamicFortePiano") {
        return "fp";
    }
    if (glyphName == "dynamicForzando") {
        return "fz";
    }
    if (glyphName == "dynamicSforzando") {
        return "sf";
    }
    if (glyphName == "dynamicSforzandoPiano") {
        return "sfp";
    }
    if (glyphName == "dynamicSforzandoPianissimo") {
        return "sfpp";
    }
    if (glyphName == "dynamicSforzato") {
        return "sfz";
    }
    if (glyphName == "dynamicSforzatoFF") {
        return "sffz";
    }
    if (glyphName == "dynamicRinforzando" || glyphName == "dynamicRinforzando1" || glyphName == "dynamicRinforzando2") {
        return "rf";
    }
    if (glyphName == "dynamicZ") {
        return "z";
    }
    return {};
}

static size_t utf8ByteCount(unsigned char ch)
{
    if (ch < 0x80) {
        return 1;
    }
    if ((ch & 0xE0) == 0xC0) {
        return 2;
    }
    if ((ch & 0xF0) == 0xE0) {
        return 3;
    }
    if ((ch & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

static void appendDynamicText(std::string& text, const std::string& chunk, const musx::dom::MusxInstance<musx::dom::FontInfo>& font)
{
    for (size_t offset = 0; offset < chunk.size();) {
        const size_t byteCount = std::min(utf8ByteCount(static_cast<unsigned char>(chunk[offset])), chunk.size() - offset);
        const std::string codepointText = chunk.substr(offset, byteCount);
        if (font) {
            if (auto glyphName = utils::smuflGlyphNameForFont(font, codepointText)) {
                if (const std::string glyphText = glyphNameToDynamicText(glyphName.value()); !glyphText.empty()) {
                    text += glyphText;
                    offset += byteCount;
                    continue;
                }
            }
        }
        text += codepointText;
        offset += byteCount;
    }
}

static Dynamic classifyNormalizedDynamicText(std::string text)
{
    if (text == "pppppp") return Dynamic::pppppp;
    if (text == "ppppp") return Dynamic::ppppp;
    if (text == "pppp") return Dynamic::pppp;
    if (text == "ppp") return Dynamic::ppp;
    if (text == "pp") return Dynamic::pp;
    if (text == "p") return Dynamic::p;
    if (text == "mp") return Dynamic::mp;
    if (text == "mf") return Dynamic::mf;
    if (text == "f") return Dynamic::f;
    if (text == "ff") return Dynamic::ff;
    if (text == "fff") return Dynamic::fff;
    if (text == "ffff") return Dynamic::ffff;
    if (text == "fffff") return Dynamic::fffff;
    if (text == "ffffff") return Dynamic::ffffff;
    if (text == "fp") return Dynamic::fp;
    if (text == "fz" || text == "forzando") return Dynamic::fz;
    if (text == "sf") return Dynamic::sf;
    if (text == "sfp") return Dynamic::sfp;
    if (text == "sfpp") return Dynamic::sfpp;
    if (text == "sfz" || text == "sforzando" || text == "sforzato") return Dynamic::sfz;
    if (text == "sffz") return Dynamic::sffz;
    if (text == "rf" || text == "rinf" || text == "rinf." || text == "rinforzando") return Dynamic::rf;
    if (text == "rfz") return Dynamic::rfz;

    const bool dynamicLettersOnly = !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch == 'p' || ch == 'm' || ch == 'f' || ch == 's' || ch == 'r' || ch == 'z';
    });
    if (dynamicLettersOnly) {
        return Dynamic::Other;
    }

    return Dynamic::None;
}

} // namespace

Dynamic classifyDynamic(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return Dynamic::None;
    }

    const auto cat = def->getDocument()->getOthers()->get<musx::dom::others::MarkingCategory>(def->getRequestedPartId(), def->categoryId);
    const bool isDynamicsCategory = cat && cat->categoryType == musx::dom::others::MarkingCategory::CategoryType::Dynamics;

    auto rawTextCtx = def->getRawTextCtx(musx::dom::SCORE_PARTID);
    if (!rawTextCtx) {
        return isDynamicsCategory ? Dynamic::Other : Dynamic::None;
    }

    std::string text;
    rawTextCtx.parseEnigmaText([&](const std::string& chunk, const musx::util::EnigmaStyles& styles) -> bool {
        if (!styles.font || styles.font->hidden) {
            return true;
        }
        appendDynamicText(text, chunk, styles.font);
        return true;
    }, musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));

    const Dynamic result = classifyNormalizedDynamicText(normalizeDynamicText(text));
    if (result == Dynamic::None && isDynamicsCategory) {
        return Dynamic::Other;
    }
    return result;
}

std::string_view dynamicCanonicalText(Dynamic dynamic)
{
    switch (dynamic) {
    case Dynamic::None:
    case Dynamic::Other:
        return {};
    case Dynamic::pppppp: return "pppppp";
    case Dynamic::ppppp: return "ppppp";
    case Dynamic::pppp: return "pppp";
    case Dynamic::ppp: return "ppp";
    case Dynamic::pp: return "pp";
    case Dynamic::p: return "p";
    case Dynamic::mp: return "mp";
    case Dynamic::mf: return "mf";
    case Dynamic::f: return "f";
    case Dynamic::ff: return "ff";
    case Dynamic::fff: return "fff";
    case Dynamic::ffff: return "ffff";
    case Dynamic::fffff: return "fffff";
    case Dynamic::ffffff: return "ffffff";
    case Dynamic::fp: return "fp";
    case Dynamic::fz: return "fz";
    case Dynamic::sf: return "sf";
    case Dynamic::sfp: return "sfp";
    case Dynamic::sfpp: return "sfpp";
    case Dynamic::sfz: return "sfz";
    case Dynamic::sffz: return "sffz";
    case Dynamic::rf: return "rf";
    case Dynamic::rfz: return "rfz";
    }
    return {};
}

} // namespace denigma::classify
