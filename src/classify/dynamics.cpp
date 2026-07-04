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
#include <vector>

#include "smufl_mapping.h"
#include "classify/classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

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

static std::string glyphNameToDynamicText(std::string_view glyphName)
{
    if (glyphName == "dynamicPiano" || glyphName == "dynamicPianoSmall") {
        return "p";
    }
    if (glyphName == "dynamicPP") {
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
    if (glyphName == "dynamicMezzo" || glyphName == "dynamicMezzoSmall") {
        return "m";
    }
    if (glyphName == "dynamicMP") {
        return "mp";
    }
    if (glyphName == "dynamicMF") {
        return "mf";
    }
    if (glyphName == "dynamicForte" || glyphName == "dynamicForteSmall") {
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
    if (glyphName == "dynamicPF") {
        return "pf";
    }
    if (glyphName == "dynamicForzando") {
        return "fz";
    }
    if (glyphName == "dynamicSforzando" || glyphName == "dynamicSforzandoLegacy" || glyphName == "dynamicSforzandoSmall") {
        return "s";
    }
    if (glyphName == "dynamicSforzando1") {
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
    if (glyphName == "dynamicSforzatoPiano") {
        return "sfzp";
    }
    if (glyphName == "dynamicSforzatoFF") {
        return "sffz";
    }
    if (glyphName == "dynamicRinforzando" || glyphName == "dynamicRinforzandoSmall") {
        return "r";
    }
    if (glyphName == "dynamicRinforzando1") {
        return "rf";
    }
    if (glyphName == "dynamicRinforzando2") {
        return "rfz";
    }
    if (glyphName == "dynamicZ" || glyphName == "dynamicZSmall") {
        return "z";
    }
    if (glyphName == "dynamicNiente" || glyphName == "dynamicNienteForHairpin" || glyphName == "dynamicNienteSmall") {
        return "n";
    }
    return {};
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

static std::string_view withoutFinalPeriods(std::string_view text)
{
    while (!text.empty() && text.back() == '.') {
        text.remove_suffix(1);
    }
    return text;
}

static DynamicChange qualifierChangeForText(std::string_view text)
{
    bool sawIncrease = false;
    bool sawDecrease = false;
    const auto isBoundary = [](unsigned char ch) {
        return utils::isSpace(ch) || utils::isPunctuation(ch);
    };

    for (size_t start = 0; start < text.size();) {
        while (start < text.size() && isBoundary(static_cast<unsigned char>(text[start]))) {
            ++start;
        }
        size_t end = start;
        while (end < text.size() && !isBoundary(static_cast<unsigned char>(text[end]))) {
            ++end;
        }
        if (start < end) {
            const std::string_view token = withoutFinalPeriods(text.substr(start, end - start));
            if (token == "piu" || token == "più") {
                sawIncrease = true;
            } else if (token == "meno" || token == "menos") {
                sawDecrease = true;
            }
        }
        start = end;
    }

    if (sawIncrease == sawDecrease) {
        return DynamicChange::Absolute;
    }
    return sawIncrease ? DynamicChange::RelativeIncrease : DynamicChange::RelativeDecrease;
}

static DynamicChange classifyDynamicChange(std::string_view prefixText, std::string_view suffixText)
{
    const DynamicChange prefixChange = qualifierChangeForText(prefixText);
    const DynamicChange suffixChange = qualifierChangeForText(suffixText);
    if (prefixChange == DynamicChange::Absolute) {
        return suffixChange;
    }
    if (suffixChange == DynamicChange::Absolute || suffixChange == prefixChange) {
        return prefixChange;
    }
    return DynamicChange::Absolute;
}

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
    if (text == "ffp") return Dynamic::ffp;
    if (text == "fz" || text == "forzando") return Dynamic::fz;
    if (text == "ffz") return Dynamic::ffz;
    if (text == "pf") return Dynamic::pf;
    if (text == "sf" || text == "sforzando") return Dynamic::sf;
    if (text == "sfp") return Dynamic::sfp;
    if (text == "sfpp") return Dynamic::sfpp;
    if (text == "sfz" || text == "sforzato" || text == "sforzado") return Dynamic::sfz;
    if (text == "sffz") return Dynamic::sffz;
    if (text == "sfzp") return Dynamic::sfzp;
    if (text == "rf" || text == "rinf" || text == "rinf." || text == "rinforzando") return Dynamic::rf;
    if (text == "rfz") return Dynamic::rfz;
    if (text == "n" || text == "niente") return Dynamic::n;
    return Dynamic::None;
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

static std::vector<size_t> chunkOffsets(const std::vector<musx::util::EnigmaTextChunk>& chunks)
{
    std::vector<size_t> offsets;
    offsets.reserve(chunks.size());
    size_t offset = 0;
    for (const auto& chunk : chunks) {
        offsets.push_back(offset);
        offset += chunk.text.size();
    }
    return offsets;
}

static size_t totalChunkTextSize(const std::vector<musx::util::EnigmaTextChunk>& chunks)
{
    size_t result = 0;
    for (const auto& chunk : chunks) {
        result += chunk.text.size();
    }
    return result;
}

static std::vector<musx::util::EnigmaTextChunk> sourceChunksForRange(
    const std::vector<musx::util::EnigmaTextChunk>& chunks,
    const std::vector<size_t>& offsets,
    size_t start,
    size_t end)
{
    std::vector<musx::util::EnigmaTextChunk> result;
    if (start >= end) {
        return result;
    }

    for (size_t i = 0; i < chunks.size(); ++i) {
        const size_t chunkStart = offsets[i];
        const size_t chunkEnd = chunkStart + chunks[i].text.size();
        if (end <= chunkStart) {
            break;
        }
        if (start >= chunkEnd) {
            continue;
        }

        auto chunk = chunks[i];
        const size_t localStart = start > chunkStart ? start - chunkStart : 0;
        const size_t localEnd = std::min(end, chunkEnd) - chunkStart;
        chunk.text = chunk.text.substr(localStart, localEnd - localStart);
        if (!chunk.text.empty()) {
            result.push_back(std::move(chunk));
        }
    }

    return result;
}

static DynamicText makeDynamicTextFromChunks(const std::vector<musx::util::EnigmaTextChunk>& chunks)
{
    DynamicText text;
    const auto offsets = chunkOffsets(chunks);
    for (size_t i = 0; i < chunks.size(); ++i) {
        appendDynamicText(text, chunks[i].text, chunks[i].styles.font, offsets[i]);
    }
    return text;
}

static std::vector<musx::util::EnigmaTextChunk> collectVisibleDynamicChunks(
    const musx::util::EnigmaParsingContext& rawTextCtx)
{
    std::vector<musx::util::EnigmaTextChunk> result;
    auto chunks = rawTextCtx.collectEnigmaTextChunks(
        musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (auto& chunk : chunks) {
        if (!chunk.styles.font || chunk.styles.font->hidden) {
            continue;
        }
        result.push_back(std::move(chunk));
    }
    return result;
}

static std::string normalizedTextForRange(const DynamicText& text, size_t start, size_t end)
{
    if (start >= end || start >= text.text.size()) {
        return {};
    }
    return utils::trimAscii(std::string_view(text.text).substr(start, std::min(end, text.text.size()) - start));
}

static void appendPhraseRun(
    DynamicPhraseClassification& phrase,
    std::vector<musx::util::EnigmaTextChunk> chunks,
    std::optional<DynamicMark> dynamic = std::nullopt)
{
    if (chunks.empty() && !dynamic) {
        return;
    }
    phrase.runs.push_back({ std::move(chunks), std::move(dynamic) });
}

static DynamicPhraseClassification classifyDynamicFromChunks(
    const std::vector<musx::util::EnigmaTextChunk>& chunks,
    bool isDynamicsCategory)
{
    DynamicPhraseClassification phrase;
    const auto offsets = chunkOffsets(chunks);
    const size_t sourceEnd = totalChunkTextSize(chunks);
    const DynamicText normalizedText = normalizeDynamicText(makeDynamicTextFromChunks(chunks));
    const auto matches = findDynamicTokens(normalizedText);

    if (!matches.empty()) {
        size_t previousSourceEnd = 0;
        for (size_t i = 0; i < matches.size(); ++i) {
            const auto& match = matches[i];
            const size_t matchEnd = match.start + match.length;
            const size_t dynamicSourceStart = normalizedText.sourceSpans[match.start].start;
            const size_t dynamicSourceEnd = normalizedText.sourceSpans[matchEnd - 1].end;
            appendPhraseRun(phrase, sourceChunksForRange(chunks, offsets, previousSourceEnd, dynamicSourceStart));

            const size_t prefixStart = i == 0 ? 0 : matches[i - 1].start + matches[i - 1].length;
            const size_t suffixEnd = i + 1 < matches.size() ? matches[i + 1].start : normalizedText.text.size();
            const std::string prefixText = normalizedTextForRange(normalizedText, prefixStart, match.start);
            const std::string suffixText = normalizedTextForRange(normalizedText, matchEnd, suffixEnd);
            appendPhraseRun(phrase,
                sourceChunksForRange(chunks, offsets, dynamicSourceStart, dynamicSourceEnd),
                DynamicMark{ match.dynamic, matchedGlyphNames(normalizedText, match), classifyDynamicChange(prefixText, suffixText) });

            previousSourceEnd = dynamicSourceEnd;
        }
        appendPhraseRun(phrase, sourceChunksForRange(chunks, offsets, previousSourceEnd, sourceEnd));
        return phrase;
    }

    if (isDynamicLikeText(normalizedText.text) || isDynamicsCategory) {
        appendPhraseRun(phrase,
            chunks,
            DynamicMark{ Dynamic::Other, knownGlyphNames(normalizedText), classifyDynamicChange({}, normalizedText.text) });
        return phrase;
    }

    appendPhraseRun(phrase, chunks);
    return phrase;
}

} // namespace

bool DynamicPhraseClassification::hasDynamic() const noexcept
{
    return std::any_of(runs.begin(), runs.end(), [](const DynamicPhraseRun& run) {
        return run.dynamic.has_value();
    });
}

std::string dynamicRunPlainText(const std::vector<DynamicPhraseRun>& runs)
{
    return dynamicRunPlainText(runs.begin(), runs.end());
}

DynamicPhraseClassification classifyDynamic(const musx::util::EnigmaParsingContext& rawTextCtx, bool isDynamicsCategory)
{
    if (!rawTextCtx) {
        DynamicPhraseClassification phrase;
        if (isDynamicsCategory) {
            phrase.runs.push_back({ {}, DynamicMark{ Dynamic::Other, {}, DynamicChange::Absolute } });
        }
        return phrase;
    }

    return classifyDynamicFromChunks(collectVisibleDynamicChunks(rawTextCtx), isDynamicsCategory);
}

DynamicPhraseClassification classifyDynamic(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return {};
    }

    const bool isDynamicsCategory = categoryTypeFromId(def->categoryId) == ExpressionCategoryType::Dynamics;

    return classifyDynamic(def->getRawTextCtx(musx::dom::SCORE_PARTID), isDynamicsCategory);
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

} // namespace denigma::classify
