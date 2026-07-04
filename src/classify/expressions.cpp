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
#include "denigma/classify/expressions.h"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/denigma.h"
#include "denigma/classify/articulations.h"
#include "classify/classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

namespace {

using CategoryType = ExpressionCategoryType;

struct ResolvedTextExpression
{
    CategoryType categoryType{ CategoryType::Invalid };
    musx::dom::MusxInstance<musx::dom::others::TextExpressionDef> expressionDef;
    musx::util::EnigmaParsingContext rawTextCtx;
    std::string text;
    std::string normalizedText;
    std::string errorMessage;
};

static std::string normalizeExpressionText(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool previousWasSpace = false;
    for (const char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (utils::isSpace(uch)) {
            previousWasSpace = true;
            continue;
        }
        if (previousWasSpace && !result.empty()) {
            result.push_back(' ');
        }
        previousWasSpace = false;
        result.push_back(utils::toLowerCase(ch));
    }
    return result;
}

static ClassificationBasis basisForRecognition(CategoryType categoryType, CategoryType expectedCategory)
{
    if (categoryType == expectedCategory) {
        return ClassificationBasis::FinaleCategoryConfirmed;
    }
    if (categoryType == CategoryType::Invalid || categoryType == CategoryType::Misc) {
        return ClassificationBasis::Heuristic;
    }
    return ClassificationBasis::FinaleCategoryCorrected;
}

static ClassificationBasis basisForSymbolRecognition(CategoryType categoryType)
{
    if (categoryType == CategoryType::Invalid || categoryType == CategoryType::Misc) {
        return ClassificationBasis::Heuristic;
    }
    return ClassificationBasis::FinaleCategoryCorrected;
}

static bool hasCategory(CategoryType categoryType)
{
    return categoryType != CategoryType::Invalid;
}

static bool isWeakTextCategory(CategoryType categoryType)
{
    return categoryType == CategoryType::ExpressiveText
        || categoryType == CategoryType::TechniqueText
        || categoryType == CategoryType::Misc;
}

static std::string_view withoutFinalPeriods(std::string_view text)
{
    while (!text.empty() && text.back() == '.') {
        text.remove_suffix(1);
    }
    return text;
}

static musx::util::EnigmaTextChunk sliceChunk(const musx::util::EnigmaTextChunk& chunk, size_t start, size_t length)
{
    auto result = chunk;
    result.text = chunk.text.substr(start, length);
    return result;
}

static musx::util::EnigmaTextChunk sliceChunk(const musx::util::EnigmaTextChunk& chunk, std::span<const char> sourceText)
{
    const auto* chunkStart = chunk.text.data();
    const size_t start = static_cast<size_t>(sourceText.data() - chunkStart);
    return sliceChunk(chunk, start, sourceText.size());
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

static std::vector<musx::util::EnigmaTextChunk> collectVisibleExpressionChunks(
    const musx::util::EnigmaParsingContext& rawTextCtx)
{
    std::vector<musx::util::EnigmaTextChunk> result;
    auto chunks = rawTextCtx.collectEnigmaTextChunks(
        musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (auto& chunk : chunks) {
        if (!chunk.styles.font || chunk.styles.font->hidden || chunk.text.empty()) {
            continue;
        }
        result.push_back(std::move(chunk));
    }
    return result;
}

static void appendGenericRun(std::vector<ExpressionRun>& runs, const musx::util::EnigmaTextChunk& chunk)
{
    if (!chunk.text.empty()) {
        const std::string normalizedText = normalizeExpressionText(chunk.text);
        const DynamicChange change = qualifierChangeForText(normalizedText);
        if (change != DynamicChange::Absolute) {
            runs.push_back({ chunk, ClassificationBasis::Heuristic, DynamicQualifier{ change, chunk.text } });
        } else {
            runs.push_back({ chunk, ClassificationBasis::FallbackToGenericText, GenericText{ chunk.text } });
        }
    }
}

static std::vector<ExpressionRun> classifyExpressionRuns(const ResolvedTextExpression& resolved)
{
    if (!resolved.rawTextCtx) {
        return {};
    }

    std::vector<ExpressionRun> result;
    const bool forceDynamicOther = resolved.categoryType == CategoryType::Dynamics;
    for (const auto& chunk : collectVisibleExpressionChunks(resolved.rawTextCtx)) {
        const auto dynamicSpans = detail::findDynamicSpans(chunk);
        if (dynamicSpans.empty()) {
            if (auto dynamic = classifyDynamicRun(chunk, forceDynamicOther)) {
                result.push_back({ chunk, dynamic->dynamic == Dynamic::Other
                    ? ClassificationBasis::FinaleCategory
                    : basisForRecognition(resolved.categoryType, CategoryType::Dynamics), *dynamic });
            } else {
                appendGenericRun(result, chunk);
            }
            continue;
        }

        size_t cursor = 0;
        for (const auto& dynamicSpan : dynamicSpans) {
            const size_t dynamicStart = static_cast<size_t>(dynamicSpan.sourceText.data() - chunk.text.data());
            if (dynamicStart > cursor) {
                appendGenericRun(result, sliceChunk(chunk, cursor, dynamicStart - cursor));
            }
            result.push_back({
                sliceChunk(chunk, dynamicSpan.sourceText),
                basisForRecognition(resolved.categoryType, CategoryType::Dynamics),
                dynamicSpan.mark
            });
            cursor = dynamicStart + dynamicSpan.sourceText.size();
        }
        if (cursor < chunk.text.size()) {
            appendGenericRun(result, sliceChunk(chunk, cursor, chunk.text.size() - cursor));
        }
    }
    return result;
}

static std::optional<ExpressionClassification> makeDynamicExpression(
    std::vector<ExpressionRun> runs,
    CategoryType categoryType)
{
    const auto dynamicIt = std::find_if(runs.begin(), runs.end(), [](const ExpressionRun& run) {
        return std::holds_alternative<DynamicMark>(run.value);
    });
    if (dynamicIt == runs.end()) {
        return std::nullopt;
    }

    const auto& dynamicMark = std::get<DynamicMark>(dynamicIt->value);

    ExpressionClassification result;
    result.type = ExpressionType::Dynamic;
    result.basis = dynamicMark.dynamic == Dynamic::Other
        ? ClassificationBasis::FinaleCategory
        : basisForRecognition(categoryType, CategoryType::Dynamics);
    result.value = dynamicMark;
    result.runs = std::move(runs);
    return result;
}

static bool matchesAny(std::string_view text, const std::initializer_list<std::string_view>& values)
{
    const std::string_view comparableText = withoutFinalPeriods(text);
    return std::any_of(values.begin(), values.end(), [&](std::string_view value) {
        return comparableText == withoutFinalPeriods(value);
    });
}

static Technique classifyTechniqueText(std::string_view normalizedText)
{
    if (matchesAny(normalizedText, { "arco" })) {
        return { TechniqueType::Arco, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "pizz", "pizzicato" })) {
        return { TechniqueType::Pizzicato, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "col legno", "c. legno" })) {
        return { TechniqueType::ColLegno, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "col legno battuto", "col legno batt", "c. legno battuto", "c. legno batt" })) {
        return { TechniqueType::ColLegnoBattuto, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "col legno tratto", "col legno tratt", "c. legno tratto", "c. legno tratt" })) {
        return { TechniqueType::ColLegnoTratto, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "sul pont", "sul ponticello", "s. pont" })) {
        return { TechniqueType::SulPonticello, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "sul tasto", "s. tasto" })) {
        return { TechniqueType::SulTasto, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "flautando", "flaut" })) {
        return { TechniqueType::Flautando, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "ord", "ordinario" })) {
        return { TechniqueType::Ordinario, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, {
        "straight mute", "straight", "con sordino straight", "straight sord",
        "metal mute", "wood mute", "fiber mute", "fibre mute"
    })) {
        return { TechniqueType::StraightMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "cup mute", "cup", "con sordino cup", "cup sord" })) {
        return { TechniqueType::CupMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "harmon mute", "harmon", "wah-wah mute", "wah wah mute", "wah-wah", "wah wah" })) {
        return { TechniqueType::HarmonMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "plunger mute", "plunger" })) {
        return { TechniqueType::PlungerMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "bucket mute", "bucket" })) {
        return { TechniqueType::BucketMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "solotone mute", "solotone" })) {
        return { TechniqueType::SolotoneMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "stop mute", "brass mute" })) {
        return { TechniqueType::StopMute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "stopped", "stop" })) {
        return { TechniqueType::Stopped, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "con sord", "mute", "muted" })) {
        return { TechniqueType::Mute, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "senza sord", "open" })) {
        return { TechniqueType::Open, std::string(normalizedText) };
    }
    if (matchesAny(normalizedText, { "trem", "tremolo" })) {
        return { TechniqueType::Tremolo, std::string(normalizedText) };
    }
    return {};
}

static std::optional<ExpressionClassification> classifyTechnique(std::string_view normalizedText, CategoryType categoryType)
{
    Technique technique = classifyTechniqueText(normalizedText);
    if (technique.type == TechniqueType::None) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::TechniqueText;
    result.basis = basisForRecognition(categoryType, CategoryType::TechniqueText);
    result.value = std::move(technique);
    return result;
}

static bool isTempoAlterationText(std::string_view normalizedText)
{
    return matchesAny(normalizedText, {
        "accel", "accelerando",
        "rit", "ritardando",
        "rall", "rallentando",
        "a tempo", "tempo i", "tempo iº", "tempo primo",
        "meno mosso", "piu mosso"
    });
}

static bool isTempoMarkText(std::string_view normalizedText)
{
    if (normalizedText.find('=') != std::string_view::npos) {
        return true;
    }
    return matchesAny(normalizedText, {
        "largo", "adagio", "andante", "moderato", "allegro", "presto", "vivace"
    });
}

static bool isAsciiUpperAlphaNumeric(std::string_view text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
    });
}

static bool isRehearsalMarkText(std::string_view text)
{
    return !text.empty() && text.size() <= 3 && isAsciiUpperAlphaNumeric(text);
}

static std::optional<ExpressionClassification> classifyTempoAlteration(
    std::string_view normalizedText,
    CategoryType categoryType,
    bool allowCategoryFallback = true)
{
    const bool recognizedText = isTempoAlterationText(normalizedText);
    if (!recognizedText && (!allowCategoryFallback || categoryType != CategoryType::TempoAlterations)) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::TempoAlteration;
    result.basis = recognizedText
        ? basisForRecognition(categoryType, CategoryType::TempoAlterations)
        : ClassificationBasis::FinaleCategory;
    result.value = TempoAlteration{ TempoInfo{ std::string(normalizedText), 0, 0 } };
    return result;
}

static std::optional<ExpressionClassification> classifyTempo(
    std::string_view normalizedText,
    CategoryType categoryType,
    bool allowCategoryFallback = true)
{
    const bool recognizedText = isTempoMarkText(normalizedText);
    if (!recognizedText && (!allowCategoryFallback || categoryType != CategoryType::TempoMarks)) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::TempoMark;
    result.basis = recognizedText
        ? basisForRecognition(categoryType, CategoryType::TempoMarks)
        : ClassificationBasis::FinaleCategory;
    result.value = TempoMark{ TempoInfo{ std::string(normalizedText), 0, 0 } };
    return result;
}

static std::optional<ExpressionClassification> classifyTempo(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    std::string_view normalizedText,
    CategoryType categoryType)
{
    if (def && def->playbackType == musx::dom::others::PlaybackType::Tempo && def->auxData1 > 0) {
        ExpressionClassification result;
        result.type = ExpressionType::TempoMark;
        result.basis = basisForRecognition(categoryType, CategoryType::TempoMarks);
        result.value = TempoMark{ TempoInfo{ std::string(normalizedText), def->value, def->auxData1 } };
        return result;
    }
    return classifyTempo(normalizedText, categoryType);
}

static bool hasTempoPlayback(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    return def && def->playbackType == musx::dom::others::PlaybackType::Tempo && def->auxData1 > 0;
}

static std::optional<ExpressionClassification> classifyTempo(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    CategoryType categoryType)
{
    if (def && def->playbackType == musx::dom::others::PlaybackType::Tempo && def->auxData1 > 0) {
        ExpressionClassification result;
        result.type = ExpressionType::TempoMark;
        result.basis = basisForRecognition(categoryType, CategoryType::TempoMarks);
        result.value = TempoMark{ TempoInfo{ {}, def->value, def->auxData1 } };
        return result;
    }
    return std::nullopt;
}

static std::optional<ExpressionClassification> classifyRehearsalMark(std::string_view normalizedText, CategoryType categoryType)
{
    if (categoryType != CategoryType::RehearsalMarks) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::RehearsalMark;
    result.basis = ClassificationBasis::FinaleCategory;
    result.value = RehearsalMark{ std::string(normalizedText) };
    return result;
}

static std::optional<ExpressionClassification> classifyRehearsalMarkText(
    std::string_view text,
    std::string_view normalizedText,
    CategoryType categoryType)
{
    if (!isRehearsalMarkText(text)) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::RehearsalMark;
    result.basis = basisForRecognition(categoryType, CategoryType::RehearsalMarks);
    result.value = RehearsalMark{ std::string(normalizedText) };
    return result;
}

static bool assignmentUsesTopStaff(const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    return assignment
        && assignment->staffAssign == static_cast<musx::dom::StaffCmper>(musx::dom::others::StaffList::FloatingValues::TopStaff);
}

static ExpressionClassification classifySystemTextExpression(
    std::string_view text,
    std::string_view normalizedText,
    CategoryType categoryType)
{
    if (const auto tempoAlteration = classifyTempoAlteration(normalizedText, categoryType, false)) {
        return *tempoAlteration;
    }
    if (const auto tempo = classifyTempo(normalizedText, categoryType, false)) {
        return *tempo;
    }
    if (const auto rehearsalMark = classifyRehearsalMarkText(text, normalizedText, categoryType)) {
        return *rehearsalMark;
    }
    if (categoryType == CategoryType::TempoMarks) {
        return *classifyTempo(normalizedText, categoryType);
    }
    if (categoryType == CategoryType::TempoAlterations) {
        return *classifyTempoAlteration(normalizedText, categoryType);
    }

    ExpressionClassification result;
    result.type = ExpressionType::TempoMark;
    result.basis = ClassificationBasis::Heuristic;
    result.value = TempoMark{ TempoInfo{ std::string(normalizedText), 0, 0 } };
    return result;
}

static ExpressionClassification classifyGenericText(std::string_view normalizedText, CategoryType categoryType)
{
    ExpressionClassification result;
    result.type = ExpressionType::GenericText;
    if (isWeakTextCategory(categoryType)) {
        result.basis = ClassificationBasis::FinaleCategory;
    } else if (hasCategory(categoryType)) {
        result.basis = ClassificationBasis::FinaleCategoryCorrected;
    } else {
        result.basis = ClassificationBasis::FallbackToGenericText;
    }
    result.value = GenericText{ std::string(normalizedText) };
    return result;
}

static ExpressionClassification suppressExpression()
{
    ExpressionClassification result;
    result.type = ExpressionType::Suppress;
    result.basis = ClassificationBasis::FallbackToGenericText;
    result.value = Suppress{};
    return result;
}

static CategoryType categoryTypeForExpression(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return CategoryType::Invalid;
    }
    return categoryTypeFromId(def->categoryId);
}

static CategoryType categoryTypeForExpression(const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def)
{
    if (!def) {
        return CategoryType::Invalid;
    }
    return categoryTypeFromId(def->categoryId);
}

static ResolvedTextExpression resolveTextExpressionDefinition(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    ResolvedTextExpression result;
    result.expressionDef = def;
    result.categoryType = categoryTypeForExpression(def);
    if (!def) {
        return result;
    }
    if (const auto textBlock = def->getTextBlock(); !textBlock) {
        result.errorMessage = (LogMsg() << "Text expression " << def->getCmper()
            << " has non-existent text block " << def->textIdKey).str();
        return result;
    }
    result.rawTextCtx = def->getRawTextCtx(musx::dom::SCORE_PARTID);
    if (!result.rawTextCtx) {
        result.errorMessage = (LogMsg() << "Text expression " << def->getCmper()
            << " could not load EnigmaParsingContext for text block " << def->textIdKey).str();
        return result;
    }
    result.text = result.rawTextCtx.getText(true, musx::util::EnigmaString::AccidentalStyle::Unicode);
    result.normalizedText = normalizeExpressionText(result.text);
    return result;
}

static std::optional<ExpressionClassification> classifyTextExpressionError(const ResolvedTextExpression& resolved)
{
    if (resolved.errorMessage.empty()) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::Error;
    result.basis = ClassificationBasis::FallbackToGenericText;
    result.value = ExpressionError{ resolved.errorMessage };
    return result;
}

static std::optional<ExpressionClassification> classifySymbolExpression(const ResolvedTextExpression& resolved)
{
    if (!resolved.rawTextCtx) {
        return std::nullopt;
    }

    const auto symbol = utils::utf8ToCodepoint(resolved.text);
    if (!symbol) {
        return std::nullopt;
    }

    const auto classification = classifyArticulationSymbol(resolved.rawTextCtx.parseFirstFontInfo(), *symbol);
    if (const auto* fermata = classification.as<classify::Fermata>()) {
        ExpressionClassification result;
        result.type = ExpressionType::Fermata;
        result.basis = basisForSymbolRecognition(resolved.categoryType);
        result.value = ExpressionFermata{ *fermata, classification.glyphName, fermata->glyphStyle,
            resolved.expressionDef->horzMeasExprAlign == musx::dom::others::HorizontalMeasExprAlign::RightBarline };
        return result;
    }
    if (const auto* breathMark = classification.as<classify::BreathMark>()) {
        ExpressionClassification result;
        result.type = ExpressionType::BreathMark;
        result.basis = basisForSymbolRecognition(resolved.categoryType);
        result.value = ExpressionBreathMark{ *breathMark, classification.glyphName };
        return result;
    }
    return std::nullopt;
}

static ExpressionClassification withEnigmaCtx(ExpressionClassification result, const ResolvedTextExpression& resolved)
{
    if (resolved.rawTextCtx) {
        result.enigmaCtx = resolved.rawTextCtx;
    }
    return result;
}

static std::optional<ExpressionClassification> classifyResolvedTextExpressionBeforeAssignment(const ResolvedTextExpression& resolved)
{
    const CategoryType categoryType = resolved.categoryType;
    const std::string_view normalizedText = resolved.normalizedText;

    if (const auto error = classifyTextExpressionError(resolved)) {
        return error;
    }
    if (const auto symbol = classifySymbolExpression(resolved)) {
        return withEnigmaCtx(*symbol, resolved);
    }
    if (hasTempoPlayback(resolved.expressionDef)) {
        return withEnigmaCtx(*classifyTempo(resolved.expressionDef, normalizedText, categoryType), resolved);
    }
    if (categoryType == CategoryType::RehearsalMarks) {
        return withEnigmaCtx(*classifyRehearsalMark(normalizedText, categoryType), resolved);
    }
    if (const auto dynamic = makeDynamicExpression(classifyExpressionRuns(resolved), categoryType)) {
        return withEnigmaCtx(*dynamic, resolved);
    }
    if (categoryType == CategoryType::TempoMarks || categoryType == CategoryType::TempoAlterations) {
        if (const auto tempoAlteration = classifyTempoAlteration(normalizedText, categoryType, false)) {
            return withEnigmaCtx(*tempoAlteration, resolved);
        }
        if (const auto tempo = classifyTempo(normalizedText, categoryType, false)) {
            return withEnigmaCtx(*tempo, resolved);
        }
        if (categoryType == CategoryType::TempoMarks) {
            return withEnigmaCtx(*classifyTempo(normalizedText, categoryType), resolved);
        }
        return withEnigmaCtx(*classifyTempoAlteration(normalizedText, categoryType), resolved);
    }
    return std::nullopt;
}

static ExpressionClassification classifyResolvedTextExpressionDefinition(const ResolvedTextExpression& resolved)
{
    const CategoryType categoryType = resolved.categoryType;
    const std::string_view normalizedText = resolved.normalizedText;

    if (const auto classification = classifyResolvedTextExpressionBeforeAssignment(resolved)) {
        return *classification;
    }
    if (const auto technique = classifyTechnique(normalizedText, categoryType)) {
        return withEnigmaCtx(*technique, resolved);
    }
    return withEnigmaCtx(classifyGenericText(normalizedText, categoryType), resolved);
}

static ExpressionClassification classifyAssignedTextExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    const ResolvedTextExpression resolved = resolveTextExpressionDefinition(def);
    const CategoryType categoryType = resolved.categoryType;
    const std::string_view normalizedText = resolved.normalizedText;

    if (const auto classification = classifyResolvedTextExpressionBeforeAssignment(resolved)) {
        return *classification;
    }
    if (assignmentUsesTopStaff(assignment)) {
        return withEnigmaCtx(classifySystemTextExpression(resolved.text, normalizedText, categoryType), resolved);
    }
    if (const auto technique = classifyTechnique(normalizedText, categoryType)) {
        return withEnigmaCtx(*technique, resolved);
    }
    return withEnigmaCtx(classifyGenericText(normalizedText, categoryType), resolved);
}

static ExpressionClassification classifyTextExpressionDefinition(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    return classifyResolvedTextExpressionDefinition(resolveTextExpressionDefinition(def));
}

static ExpressionClassification classifyShapeExpressionDefinition(const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def)
{
    if (const auto tempo = classifyTempo(def, categoryTypeForExpression(def))) {
        return *tempo;
    }
    return suppressExpression();
}

static ExpressionClassification classifyAssignedShapeExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    const CategoryType categoryType = categoryTypeForExpression(def);

    if (const auto tempo = classifyTempo(def, categoryType)) {
        return *tempo;
    }
    if (const auto nonArpeggio = musx::util::calcNonArpeggioSpanForAssignment(assignment)) {
        ExpressionClassification result;
        result.type = ExpressionType::NonArpeggio;
        result.basis = ClassificationBasis::FinaleCategory;
        result.value = ExpressionNonArpeggio{ nonArpeggio.value() };
        return result;
    }
    if (assignmentUsesTopStaff(assignment)) {
        return classifySystemTextExpression({}, {}, categoryType);
    }
    return suppressExpression();
}

} // namespace

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    if (assignment) {
        return classifyAssignedTextExpression(def, assignment);
    }
    return classifyTextExpressionDefinition(def);
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    if (assignment) {
        return classifyAssignedShapeExpression(def, assignment);
    }
    return classifyShapeExpressionDefinition(def);
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    if (!assignment) {
        return {};
    }
    if (const auto textExpression = assignment->getTextExpression()) {
        return classifyExpression(textExpression, assignment);
    }
    if (const auto shapeExpression = assignment->getShapeExpression()) {
        return classifyExpression(shapeExpression, assignment);
    }
    return suppressExpression();
}

std::vector<ExpressionAssignmentClassification> classifyExpressionAssignments(
    const musx::dom::MusxInstanceList<musx::dom::others::MeasureExprAssign>& assignments)
{
    std::vector<ExpressionAssignmentClassification> results;
    results.reserve(assignments.size());
    for (const auto& assignment : assignments) {
        results.push_back({ assignment, classifyExpression(assignment) });
    }

    for (const auto& topStaffResult : results) {
        if (!assignmentUsesTopStaff(topStaffResult.assignment)) {
            continue;
        }
        for (auto& result : results) {
            if (result.assignment && topStaffResult.assignment->calcIsSameDefinition(*result.assignment)) {
                result.classification = topStaffResult.classification;
            }
        }
    }

    return results;
}

} // namespace denigma::classify
