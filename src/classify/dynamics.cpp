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
#include "denigma/classify/dynamics.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "smufl_mapping.h"
#include "classify/classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

using namespace dynamics;

namespace {

struct DynamicText
{
    struct SourceSpan
    {
        size_t start{};
        size_t end{};
    };

    std::string text;
    std::vector<std::optional<std::string>> glyphNames;
    std::vector<std::optional<size_t>> glyphIds;
    std::vector<SourceSpan> sourceSpans;
};

static DynamicText normalizeDynamicText(const DynamicText& input)
{
    DynamicText result;
    result.text.reserve(input.text.size());
    result.glyphNames.reserve(input.glyphNames.size());
    result.glyphIds.reserve(input.glyphIds.size());
    result.sourceSpans.reserve(input.sourceSpans.size());
    bool previousWasSpace = false;
    DynamicText::SourceSpan pendingSpaceSpan{};
    for (size_t i = 0; i < input.text.size(); ++i) {
        const unsigned char ch = static_cast<unsigned char>(input.text[i]);
        if (utils::isSpace(ch)) {
            if (!previousWasSpace) {
                pendingSpaceSpan = input.sourceSpans[i];
            } else {
                pendingSpaceSpan.end = input.sourceSpans[i].end;
            }
            previousWasSpace = true;
            continue;
        }
        if (previousWasSpace && !result.text.empty()) {
            result.text.push_back(' ');
            result.glyphNames.emplace_back(std::nullopt);
            result.glyphIds.emplace_back(std::nullopt);
            result.sourceSpans.push_back(pendingSpaceSpan);
        }
        previousWasSpace = false;
        result.text.push_back(utils::toLowerCase(input.text[i]));
        result.glyphNames.push_back(input.glyphNames[i]);
        result.glyphIds.push_back(input.glyphIds[i]);
        result.sourceSpans.push_back(input.sourceSpans[i]);
    }
    return result;
}

static const std::unordered_map<std::string_view, std::string_view>& dynamicGlyphLettersMap()
{
    static const std::unordered_map<std::string_view, std::string_view> result = {
        { "dynamicPiano", "p" },
        { "dynamicPianoSmall", "p" },
        { "dynamicPP", "pp" },
        { "dynamicPPP", "ppp" },
        { "dynamicPPPP", "pppp" },
        { "dynamicPPPPP", "ppppp" },
        { "dynamicPPPPPP", "pppppp" },
        { "dynamicMezzo", "m" },
        { "dynamicMezzoSmall", "m" },
        { "dynamicMP", "mp" },
        { "dynamicMF", "mf" },
        { "dynamicForte", "f" },
        { "dynamicForteSmall", "f" },
        { "dynamicFF", "ff" },
        { "dynamicFFF", "fff" },
        { "dynamicFFFF", "ffff" },
        { "dynamicFFFFF", "fffff" },
        { "dynamicFFFFFF", "ffffff" },
        { "dynamicFortePiano", "fp" },
        { "dynamicPF", "pf" },
        { "dynamicForzando", "fz" },
        { "dynamicSforzando", "s" },
        { "dynamicSforzandoLegacy", "s" },
        { "dynamicSforzandoSmall", "s" },
        { "dynamicSforzando1", "sf" },
        { "dynamicSforzandoPiano", "sfp" },
        { "dynamicSforzandoPianissimo", "sfpp" },
        { "dynamicSforzato", "sfz" },
        { "dynamicSforzatoPiano", "sfzp" },
        { "dynamicSforzatoFF", "sffz" },
        { "dynamicRinforzando", "r" },
        { "dynamicRinforzandoSmall", "r" },
        { "dynamicRinforzando1", "rf" },
        { "dynamicRinforzando2", "rfz" },
        { "dynamicZ", "z" },
        { "dynamicZSmall", "z" },
        { "dynamicNiente", "n" },
        { "dynamicNienteForHairpin", "n" },
        { "dynamicNienteSmall", "n" }
    };
    return result;
}

static std::optional<std::string_view> dynamicLetterGlyphName(char ch)
{
    switch (utils::toLowerCase(ch)) {
    case 'p': return "dynamicPiano";
    case 'm': return "dynamicMezzo";
    case 'f': return "dynamicForte";
    case 's': return "dynamicSforzando";
    case 'r': return "dynamicRinforzando";
    case 'z': return "dynamicZ";
    case 'n': return "dynamicNiente";
    default: return std::nullopt;
    }
}

static std::string glyphNameToDynamicText(std::string_view glyphName)
{
    const auto& glyphLetters = dynamicGlyphLettersMap();
    const auto glyphIt = glyphLetters.find(glyphName);
    return glyphIt == glyphLetters.end() ? std::string{} : std::string(glyphIt->second);
}

static void appendDynamicText(
    DynamicText& text,
    const std::string& chunk,
    const musx::dom::MusxInstance<musx::dom::FontInfo>& font,
    size_t chunkStart)
{
    size_t nextGlyphId = text.glyphIds.size();
    for (utils::Utf8Iterator iter(chunk); !iter.atEnd(); iter.next()) {
        const std::string codepointText = chunk.substr(iter.offset(), iter->byteCount);
        const DynamicText::SourceSpan sourceSpan{ chunkStart + iter.offset(), chunkStart + iter.offset() + iter->byteCount };
        if (font) {
            if (const auto* glyphName = smufl_mapping::getGlyphNameForFont(
                    font->getName(),
                    iter->codepoint,
                    font->calcIsSMuFL(),
                    smufl_mapping::SmuflGlyphSource::Finale)) {
                if (const std::string glyphText = glyphNameToDynamicText(*glyphName); !glyphText.empty()) {
                    text.text += glyphText;
                    text.glyphNames.insert(text.glyphNames.end(), glyphText.size(), std::string(*glyphName));
                    text.glyphIds.insert(text.glyphIds.end(), glyphText.size(), nextGlyphId++);
                    text.sourceSpans.insert(text.sourceSpans.end(), glyphText.size(), sourceSpan);
                    continue;
                }
            }
        }
        text.text += codepointText;
        text.glyphNames.insert(text.glyphNames.end(), codepointText.size(), std::nullopt);
        text.glyphIds.insert(text.glyphIds.end(), codepointText.size(), std::nullopt);
        text.sourceSpans.insert(text.sourceSpans.end(), codepointText.size(), sourceSpan);
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
    static const std::unordered_map<std::string_view, Dynamic> dynamicTokens = {
        { "pppppp", Dynamic::pppppp },
        { "ppppp", Dynamic::ppppp },
        { "pppp", Dynamic::pppp },
        { "ppp", Dynamic::ppp },
        { "pp", Dynamic::pp },
        { "p", Dynamic::p },
        { "mp", Dynamic::mp },
        { "mf", Dynamic::mf },
        { "f", Dynamic::f },
        { "ff", Dynamic::ff },
        { "fff", Dynamic::fff },
        { "ffff", Dynamic::ffff },
        { "fffff", Dynamic::fffff },
        { "ffffff", Dynamic::ffffff },
        { "fp", Dynamic::fp },
        { "ffp", Dynamic::ffp },
        { "fz", Dynamic::fz },
        { "forzando", Dynamic::fz },
        { "ffz", Dynamic::ffz },
        { "pf", Dynamic::pf },
        { "sf", Dynamic::sf },
        { "sforzando", Dynamic::sf },
        { "sfp", Dynamic::sfp },
        { "sfpp", Dynamic::sfpp },
        { "sfz", Dynamic::sfz },
        { "sforzato", Dynamic::sfz },
        { "sforzado", Dynamic::sfz },
        { "sffz", Dynamic::sffz },
        { "sfzp", Dynamic::sfzp },
        { "rf", Dynamic::rf },
        { "rinf", Dynamic::rf },
        { "rinf.", Dynamic::rf },
        { "rinforzando", Dynamic::rf },
        { "rfz", Dynamic::rfz },
        { "n", Dynamic::n },
        { "niente", Dynamic::n }
    };
    const auto tokenIt = dynamicTokens.find(text);
    return tokenIt == dynamicTokens.end() ? Dynamic::None : tokenIt->second;
}

static bool isDynamicLikeText(std::string_view text)
{
    const bool dynamicLettersOnly = !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch == 'p' || ch == 'm' || ch == 'f' || ch == 's' || ch == 'r' || ch == 'z';
    });
    return dynamicLettersOnly;
}

static bool isSmuflGlyphMatch(const DynamicText& text, const DynamicTokenMatch& match)
{
    return std::all_of(text.glyphNames.begin() + static_cast<std::ptrdiff_t>(match.start),
                       text.glyphNames.begin() + static_cast<std::ptrdiff_t>(match.start + match.length),
                       [](const std::optional<std::string>& glyphName) { return glyphName.has_value(); });
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

static std::vector<std::string> matchedGlyphNames(const DynamicText& text, const DynamicTokenMatch& match)
{
    std::vector<std::string> result;
    std::optional<size_t> previousGlyphId;
    for (size_t i = match.start; i < match.start + match.length; ++i) {
        const auto& glyphName = text.glyphNames[i];
        const auto& glyphId = text.glyphIds[i];
        if (!glyphName || !glyphId) {
            return {};
        }
        if (!previousGlyphId || *previousGlyphId != *glyphId) {
            result.push_back(*glyphName);
            previousGlyphId = glyphId;
        }
    }
    return result;
}

static std::vector<std::string> knownGlyphNames(const DynamicText& text)
{
    std::vector<std::string> result;
    std::optional<size_t> previousGlyphId;
    for (size_t i = 0; i < text.glyphNames.size(); ++i) {
        const auto& glyphName = text.glyphNames[i];
        const auto& glyphId = text.glyphIds[i];
        if (!glyphName || !glyphId) {
            previousGlyphId.reset();
            continue;
        }
        if (!previousGlyphId || *previousGlyphId != *glyphId) {
            result.push_back(*glyphName);
            previousGlyphId = glyphId;
        }
    }
    return result;
}

static std::vector<DynamicTokenMatch> findDynamicTokens(const DynamicText& text)
{
    if (const Dynamic exact = classifyExactDynamicToken(text.text); exact != Dynamic::None) {
        return { DynamicTokenMatch{ exact, 0, text.text.size() } };
    }

    std::vector<DynamicTokenMatch> result;
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
                    result.push_back(match);
                }
            }
        }
        start = end;
    }

    return result;
}

static DynamicText makeDynamicTextFromChunk(const musx::util::EnigmaTextChunk& chunk)
{
    DynamicText text;
    appendDynamicText(text, chunk.text, chunk.styles.font, 0);
    return text;
}

static std::span<const char> sourceSpanForMatch(const musx::util::EnigmaTextChunk& chunk, const DynamicText& text, const DynamicTokenMatch& match)
{
    const size_t matchEnd = match.start + match.length;
    const size_t sourceStart = text.sourceSpans[match.start].start;
    const size_t sourceEnd = text.sourceSpans[matchEnd - 1].end;
    return std::span<const char>(chunk.text.data() + sourceStart, sourceEnd - sourceStart);
}

} // namespace

std::optional<DynamicMark> classifyDynamicRun(const musx::util::EnigmaTextChunk& chunk, bool forceOther)
{
    if (!chunk.styles.font || chunk.styles.font->hidden) {
        return std::nullopt;
    }

    const DynamicText normalizedText = normalizeDynamicText(makeDynamicTextFromChunk(chunk));
    if (normalizedText.text.empty()) {
        return std::nullopt;
    }

    if (const Dynamic exact = classifyExactDynamicToken(normalizedText.text); exact != Dynamic::None) {
        DynamicTokenMatch match{ exact, 0, normalizedText.text.size() };
        return DynamicMark{ exact, matchedGlyphNames(normalizedText, match) };
    }
    if (isDynamicLikeText(normalizedText.text) || forceOther) {
        return DynamicMark{ Dynamic::Other, knownGlyphNames(normalizedText) };
    }
    return std::nullopt;
}

namespace detail {

std::vector<DynamicSpan> findDynamicSpans(const musx::util::EnigmaTextChunk& chunk)
{
    if (!chunk.styles.font || chunk.styles.font->hidden) {
        return {};
    }

    const DynamicText normalizedText = normalizeDynamicText(makeDynamicTextFromChunk(chunk));
    std::vector<DynamicSpan> result;
    for (const auto& match : findDynamicTokens(normalizedText)) {
        result.push_back({
            sourceSpanForMatch(chunk, normalizedText, match),
            DynamicMark{ match.dynamic, matchedGlyphNames(normalizedText, match) }
        });
    }
    return result;
}

} // namespace detail

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
    case Dynamic::ffp: return "ffp";
    case Dynamic::fz: return "fz";
    case Dynamic::ffz: return "ffz";
    case Dynamic::pf: return "pf";
    case Dynamic::sf: return "sf";
    case Dynamic::sfp: return "sfp";
    case Dynamic::sfpp: return "sfpp";
    case Dynamic::sfz: return "sfz";
    case Dynamic::sffz: return "sffz";
    case Dynamic::sfzp: return "sfzp";
    case Dynamic::rf: return "rf";
    case Dynamic::rfz: return "rfz";
    case Dynamic::n: return "n";
    }
    return {};
}

std::vector<std::string> dynamicCanonicalGlyphs(Dynamic dynamic)
{
    switch (dynamic) {
    case Dynamic::None:
    case Dynamic::Other:
        break;
    case Dynamic::pppppp: return { "dynamicPPPPPP" };
    case Dynamic::ppppp: return { "dynamicPPPPP" };
    case Dynamic::pppp: return { "dynamicPPPP" };
    case Dynamic::ppp: return { "dynamicPPP" };
    case Dynamic::pp: return { "dynamicPP" };
    case Dynamic::p: return { "dynamicPiano" };
    case Dynamic::mp: return { "dynamicMP" };
    case Dynamic::mf: return { "dynamicMF" };
    case Dynamic::f: return { "dynamicForte" };
    case Dynamic::ff: return { "dynamicFF" };
    case Dynamic::fff: return { "dynamicFFF" };
    case Dynamic::ffff: return { "dynamicFFFF" };
    case Dynamic::fffff: return { "dynamicFFFFF" };
    case Dynamic::ffffff: return { "dynamicFFFFFF" };
    case Dynamic::fp: return { "dynamicFortePiano" };
    case Dynamic::ffp: return { "dynamicFF", "dynamicPiano" };
    case Dynamic::fz: return { "dynamicForzando" };
    case Dynamic::ffz: return { "dynamicFF", "dynamicZ" };
    case Dynamic::pf: return { "dynamicPF" };
    case Dynamic::sf: return { "dynamicSforzando1" };
    case Dynamic::sfp: return { "dynamicSforzandoPiano" };
    case Dynamic::sfpp: return { "dynamicSforzandoPianissimo" };
    case Dynamic::sfz: return { "dynamicSforzato" };
    case Dynamic::sffz: return { "dynamicSforzatoFF" };
    case Dynamic::sfzp: return { "dynamicSforzatoPiano" };
    case Dynamic::rf: return { "dynamicRinforzando1" };
    case Dynamic::rfz: return { "dynamicRinforzando2" };
    case Dynamic::n: return { "dynamicNiente" };
    }
    return {};
}

std::vector<std::string> dynamicCanonicalLetterGlyphs(Dynamic dynamic)
{
    switch (dynamic) {
    case Dynamic::None:
    case Dynamic::Other:
        break;
    case Dynamic::pppppp: return { "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano" };
    case Dynamic::ppppp: return { "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano" };
    case Dynamic::pppp: return { "dynamicPiano", "dynamicPiano", "dynamicPiano", "dynamicPiano" };
    case Dynamic::ppp: return { "dynamicPiano", "dynamicPiano", "dynamicPiano" };
    case Dynamic::pp: return { "dynamicPiano", "dynamicPiano" };
    case Dynamic::p: return { "dynamicPiano" };
    case Dynamic::mp: return { "dynamicMezzo", "dynamicPiano" };
    case Dynamic::mf: return { "dynamicMezzo", "dynamicForte" };
    case Dynamic::f: return { "dynamicForte" };
    case Dynamic::ff: return { "dynamicForte", "dynamicForte" };
    case Dynamic::fff: return { "dynamicForte", "dynamicForte", "dynamicForte" };
    case Dynamic::ffff: return { "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte" };
    case Dynamic::fffff: return { "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte" };
    case Dynamic::ffffff: return { "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte" };
    case Dynamic::fp: return { "dynamicForte", "dynamicPiano" };
    case Dynamic::ffp: return { "dynamicForte", "dynamicForte", "dynamicPiano" };
    case Dynamic::fz: return { "dynamicForte", "dynamicZ" };
    case Dynamic::ffz: return { "dynamicForte", "dynamicForte", "dynamicZ" };
    case Dynamic::pf: return { "dynamicPiano", "dynamicForte" };
    case Dynamic::sf: return { "dynamicSforzando", "dynamicForte" };
    case Dynamic::sfp: return { "dynamicSforzando", "dynamicForte", "dynamicPiano" };
    case Dynamic::sfpp: return { "dynamicSforzando", "dynamicForte", "dynamicPiano", "dynamicPiano" };
    case Dynamic::sfz: return { "dynamicSforzando", "dynamicForte", "dynamicZ" };
    case Dynamic::sffz: return { "dynamicSforzando", "dynamicForte", "dynamicForte", "dynamicZ" };
    case Dynamic::sfzp: return { "dynamicSforzando", "dynamicForte", "dynamicZ", "dynamicPiano" };
    case Dynamic::rf: return { "dynamicRinforzando", "dynamicForte" };
    case Dynamic::rfz: return { "dynamicRinforzando", "dynamicForte", "dynamicZ" };
    case Dynamic::n: return { "dynamicNiente" };
    }
    return {};
}

std::vector<std::string> dynamicLettersToLetterGlyphs(std::string_view letters)
{
    std::vector<std::string> result;
    result.reserve(letters.size());
    for (const char ch : letters) {
        if (const auto glyphName = dynamicLetterGlyphName(ch)) {
            result.emplace_back(*glyphName);
        }
    }
    return result;
}

std::string dynamicGlyphsToLetters(const std::vector<std::string>& glyphs)
{
    std::string result;
    const auto& letterMap = dynamicGlyphLettersMap();
    for (const auto& glyph : glyphs) {
        const auto glyphIt = letterMap.find(glyph);
        if (glyphIt == letterMap.end()) {
            return {};
        }
        result += glyphIt->second;
    }
    return result;
}

} // namespace denigma::classify
