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
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "utils/smufl_support.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

namespace {

static std::string trimAscii(std::string_view text)
{
    while (!text.empty() && utils::isSpace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && utils::isSpace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

struct DynamicText
{
    std::string text;
    std::vector<bool> smuflGlyph;
};

static DynamicText normalizeDynamicText(const DynamicText& input)
{
    DynamicText result;
    result.text.reserve(input.text.size());
    result.smuflGlyph.reserve(input.smuflGlyph.size());
    bool previousWasSpace = false;
    bool previousWasSmuflGlyph = false;
    for (size_t i = 0; i < input.text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(input.text[i]);
        if (utils::isSpace(ch)) {
            previousWasSpace = true;
            previousWasSmuflGlyph = false;
            continue;
        }
        if (previousWasSpace && !result.text.empty()) {
            result.text.push_back(' ');
            result.smuflGlyph.push_back(previousWasSmuflGlyph);
        }
        previousWasSpace = false;
        previousWasSmuflGlyph = false;
        result.text.push_back(utils::toLowerCase(input.text[i]));
        result.smuflGlyph.push_back(input.smuflGlyph[i]);
    }
    return result;
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

static void appendDynamicText(DynamicText& text, const std::string& chunk, const musx::dom::MusxInstance<musx::dom::FontInfo>& font)
{
    for (utils::Utf8Iterator iter(chunk); !iter.atEnd(); iter.next()) {
        const std::string codepointText = chunk.substr(iter.offset(), iter->byteCount);
        if (font) {
            if (auto glyphName = utils::smuflGlyphNameForFont(font, codepointText)) {
                if (const std::string glyphText = glyphNameToDynamicText(glyphName.value()); !glyphText.empty()) {
                    text.text += glyphText;
                    text.smuflGlyph.insert(text.smuflGlyph.end(), glyphText.size(), true);
                    continue;
                }
            }
        }
        text.text += codepointText;
        text.smuflGlyph.insert(text.smuflGlyph.end(), codepointText.size(), false);
    }
}

struct DynamicTokenMatch
{
    Dynamic dynamic{};
    size_t start{};
    size_t length{};
};

static Dynamic classifyExactDynamicToken(std::string_view text)
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
    return Dynamic::None;
}

static bool isDynamicLikeText(std::string_view text)
{
    const bool dynamicLettersOnly = !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch == 'p' || ch == 'm' || ch == 'f' || ch == 's' || ch == 'r' || ch == 'z';
    });
    return dynamicLettersOnly;
}

static bool hasAdditionalText(std::string_view text, const DynamicTokenMatch& match)
{
    return !trimAscii(text.substr(0, match.start)).empty()
        || !trimAscii(text.substr(match.start + match.length)).empty();
}

static bool isSmuflGlyphMatch(const DynamicText& text, const DynamicTokenMatch& match)
{
    return std::all_of(text.smuflGlyph.begin() + static_cast<std::ptrdiff_t>(match.start),
                       text.smuflGlyph.begin() + static_cast<std::ptrdiff_t>(match.start + match.length),
                       [](bool isSmuflGlyph) { return isSmuflGlyph; });
}

static bool isSpaceDelimitedMatch(const DynamicText& text, const DynamicTokenMatch& match)
{
    const size_t end = match.start + match.length;
    const bool leftDelimited = match.start == 0 || utils::isSpace(static_cast<unsigned char>(text.text[match.start - 1]));
    const bool rightDelimited = end == text.text.size() || utils::isSpace(static_cast<unsigned char>(text.text[end]));
    return leftDelimited && rightDelimited;
}

static bool isAcceptedMatch(const DynamicText& text, const DynamicTokenMatch& match)
{
    return isSmuflGlyphMatch(text, match) || isSpaceDelimitedMatch(text, match);
}

static std::optional<DynamicTokenMatch> findDynamicToken(const DynamicText& text)
{
    if (const Dynamic exact = classifyExactDynamicToken(text.text); exact != Dynamic::None) {
        return DynamicTokenMatch{ exact, 0, text.text.size() };
    }

    const auto isBoundary = [](unsigned char ch) {
        return utils::isSpace(ch) || utils::isPunctuation(ch);
    };

    for (size_t start = 0; start < text.text.size();) {
        while (start < text.text.size() && isBoundary(static_cast<unsigned char>(text.text[start]))) {
            ++start;
        }
        size_t end = start;
        while (end < text.text.size() && !isBoundary(static_cast<unsigned char>(text.text[end]))) {
            ++end;
        }
        if (start < end) {
            const std::string_view token = std::string_view(text.text).substr(start, end - start);
            if (const Dynamic dynamic = classifyExactDynamicToken(token); dynamic != Dynamic::None) {
                DynamicTokenMatch match{ dynamic, start, end - start };
                if (isAcceptedMatch(text, match)) {
                    return match;
                }
            }
        }
        start = end;
    }

    return std::nullopt;
}

static DynamicClassification classifyNormalizedDynamicText(const DynamicText& text)
{
    if (const auto match = findDynamicToken(text)) {
        return { match->dynamic, hasAdditionalText(text.text, match.value()) };
    }
    if (isDynamicLikeText(text.text)) {
        return { Dynamic::Other, false };
    }
    return {};
}

} // namespace

DynamicClassification classifyDynamic(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return {};
    }

    const auto cat = def->getDocument()->getOthers()->get<musx::dom::others::MarkingCategory>(def->getRequestedPartId(), def->categoryId);
    const bool isDynamicsCategory = cat && cat->categoryType == musx::dom::others::MarkingCategory::CategoryType::Dynamics;

    auto rawTextCtx = def->getRawTextCtx(musx::dom::SCORE_PARTID);
    if (!rawTextCtx) {
        return isDynamicsCategory ? DynamicClassification{ Dynamic::Other, true } : DynamicClassification{};
    }

    DynamicText text;
    rawTextCtx.parseEnigmaText([&](const std::string& chunk, const musx::util::EnigmaStyles& styles) -> bool {
        if (!styles.font || styles.font->hidden) {
            return true;
        }
        appendDynamicText(text, chunk, styles.font);
        return true;
    }, musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));

    DynamicClassification result = classifyNormalizedDynamicText(normalizeDynamicText(text));
    if (!result && isDynamicsCategory) {
        return { Dynamic::Other, true };
    }
    return result;
}

std::string dynamicCanonicalText(Dynamic dynamic)
{
    switch (dynamic) {
    case Dynamic::None:
    case Dynamic::Other:
        break;
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
