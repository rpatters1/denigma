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
#include <string>
#include <string_view>

#include "utils/stringutils.h"

namespace denigma::classify {

namespace {

using CategoryType = ExpressionCategoryType;

struct TextTokenMatch
{
    size_t start{};
    size_t length{};
    Dynamic dynamic{};
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

static bool isBoundary(char ch)
{
    const unsigned char uch = static_cast<unsigned char>(ch);
    return utils::isSpace(uch) || utils::isPunctuation(uch);
}

static bool isSpaceDelimitedMatch(std::string_view text, const TextTokenMatch& match)
{
    const size_t end = match.start + match.length;
    const bool leftDelimited = match.start == 0 || utils::isSpace(static_cast<unsigned char>(text[match.start - 1]));
    const bool rightDelimited = end == text.size() || utils::isSpace(static_cast<unsigned char>(text[end]));
    return leftDelimited && rightDelimited;
}

static std::optional<TextTokenMatch> findDynamicToken(std::string_view text)
{
    if (const Dynamic exact = classifyExactDynamicToken(text); exact != Dynamic::None) {
        return TextTokenMatch{ 0, text.size(), exact };
    }

    for (size_t start = 0; start < text.size();) {
        while (start < text.size() && isBoundary(text[start])) {
            ++start;
        }
        size_t end = start;
        while (end < text.size() && !isBoundary(text[end])) {
            ++end;
        }
        if (start < end) {
            const std::string_view token = text.substr(start, end - start);
            if (const Dynamic dynamic = classifyExactDynamicToken(token); dynamic != Dynamic::None) {
                TextTokenMatch match{ start, end - start, dynamic };
                if (isSpaceDelimitedMatch(text, match)) {
                    return match;
                }
            }
        }
        start = end;
    }

    return std::nullopt;
}

static std::optional<ExpressionClassification> classifyDynamicExpression(std::string_view normalizedText, CategoryType categoryType)
{
    if (const auto match = findDynamicToken(normalizedText)) {
        ExpressionClassification result;
        result.type = ExpressionType::Dynamic;
        result.basis = basisForRecognition(categoryType, CategoryType::Dynamics);
        result.text = std::string(normalizedText);
        result.dynamic = match->dynamic;
        result.dynamicPrefixText = utils::trimAscii(normalizedText.substr(0, match->start));
        result.dynamicSuffixText = utils::trimAscii(normalizedText.substr(match->start + match->length));
        return result;
    }
    if (categoryType == CategoryType::Dynamics) {
        ExpressionClassification result;
        result.type = ExpressionType::Dynamic;
        result.basis = ClassificationBasis::FinaleCategory;
        result.text = std::string(normalizedText);
        result.dynamic = Dynamic::Other;
        return result;
    }
    return std::nullopt;
}

static std::optional<ExpressionClassification> classifyDynamicExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    std::string_view normalizedText,
    CategoryType categoryType)
{
    const DynamicClassification dynamicClass = denigma::classify::classifyDynamic(def);
    if (!dynamicClass) {
        return classifyDynamicExpression(normalizedText, categoryType);
    }

    ExpressionClassification result;
    result.type = ExpressionType::Dynamic;
    result.basis = dynamicClass.dynamic == Dynamic::Other
        ? ClassificationBasis::FinaleCategory
        : basisForRecognition(categoryType, CategoryType::Dynamics);
    result.text = std::string(normalizedText);
    result.dynamic = dynamicClass.dynamic;
    result.dynamicPrefixText = dynamicClass.prefixText;
    result.dynamicSuffixText = dynamicClass.suffixText;
    return result;
}

static std::string_view withoutFinalPeriods(std::string_view text)
{
    while (!text.empty() && text.back() == '.') {
        text.remove_suffix(1);
    }
    return text;
}

static bool matchesAny(std::string_view text, const std::initializer_list<std::string_view>& values)
{
    const std::string_view comparableText = withoutFinalPeriods(text);
    return std::any_of(values.begin(), values.end(), [&](std::string_view value) {
        return comparableText == withoutFinalPeriods(value);
    });
}

static Technique classifyTechnique(std::string_view normalizedText)
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
    Technique technique = classifyTechnique(normalizedText);
    const bool recognizedText = technique.type != TechniqueType::None;
    if (!recognizedText) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::TechniqueText;
    result.basis = basisForRecognition(categoryType, CategoryType::TechniqueText);
    result.text = std::string(normalizedText);
    result.technique = technique;
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

static bool isAsciiAlphaNumeric(std::string_view text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9');
    });
}

static bool isRehearsalMarkText(std::string_view normalizedText)
{
    if (normalizedText.empty() || normalizedText.size() > 3 || !isAsciiAlphaNumeric(normalizedText)) {
        return false;
    }
    return true;
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
    result.text = std::string(normalizedText);
    result.tempo.text = result.text;
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
    result.text = std::string(normalizedText);
    result.tempo.text = result.text;
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
        result.text = std::string(normalizedText);
        result.tempo = { result.text, def->value, def->auxData1 };
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
        result.tempo = { {}, def->value, def->auxData1 };
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
    result.text = std::string(normalizedText);
    return result;
}

static std::optional<ExpressionClassification> classifyRehearsalMarkText(std::string_view normalizedText, CategoryType categoryType)
{
    if (!isRehearsalMarkText(normalizedText)) {
        return std::nullopt;
    }

    ExpressionClassification result;
    result.type = ExpressionType::RehearsalMark;
    result.basis = basisForRecognition(categoryType, CategoryType::RehearsalMarks);
    result.text = std::string(normalizedText);
    return result;
}

static bool assignmentUsesStaffList(const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    return assignment && assignment->calcIsPartOfStaffListAssignment();
}

static ExpressionClassification classifyStaffListTextExpression(std::string_view normalizedText, CategoryType categoryType)
{
    if (const auto tempoAlteration = classifyTempoAlteration(normalizedText, categoryType, false)) {
        return *tempoAlteration;
    }
    if (const auto tempo = classifyTempo(normalizedText, categoryType, false)) {
        return *tempo;
    }
    if (const auto rehearsalMark = classifyRehearsalMarkText(normalizedText, categoryType)) {
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
    result.text = std::string(normalizedText);
    result.tempo.text = result.text;
    return result;
}

static ExpressionClassification classifyGenericText(std::string_view normalizedText, CategoryType categoryType)
{
    ExpressionClassification result;
    result.type = ExpressionType::GenericText;
    result.text = std::string(normalizedText);
    if (isWeakTextCategory(categoryType)) {
        result.basis = ClassificationBasis::FinaleCategory;
    } else if (hasCategory(categoryType)) {
        result.basis = ClassificationBasis::FinaleCategoryCorrected;
    } else {
        result.basis = ClassificationBasis::FallbackToGenericText;
    }
    return result;
}

static ExpressionClassification suppressShapeExpression()
{
    ExpressionClassification result;
    result.type = ExpressionType::Suppress;
    result.basis = ClassificationBasis::FallbackToGenericText;
    return result;
}

static CategoryType categoryTypeForExpression(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return CategoryType::Invalid;
    }
    const auto category = def->getDocument()->getOthers()->get<musx::dom::others::MarkingCategory>(def->getRequestedPartId(), def->categoryId);
    return category ? category->categoryType : CategoryType::Invalid;
}

static CategoryType categoryTypeForExpression(const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def)
{
    if (!def) {
        return CategoryType::Invalid;
    }
    const auto category = def->getDocument()->getOthers()->get<musx::dom::others::MarkingCategory>(def->getRequestedPartId(), def->categoryId);
    return category ? category->categoryType : CategoryType::Invalid;
}

static std::string expressionText(const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    if (!def) {
        return {};
    }
    auto rawTextCtx = def->getRawTextCtx(musx::dom::SCORE_PARTID);
    if (!rawTextCtx) {
        return {};
    }
    return rawTextCtx.getText(true, musx::util::EnigmaString::AccidentalStyle::Unicode);
}

} // namespace

ExpressionClassification classifyExpression(std::string_view text, ExpressionCategoryType categoryType)
{
    const std::string normalizedText = normalizeExpressionText(text);

    if (categoryType == CategoryType::RehearsalMarks) {
        return *classifyRehearsalMark(normalizedText, categoryType);
    }
    if (categoryType == CategoryType::Dynamics) {
        return *classifyDynamicExpression(normalizedText, categoryType);
    }
    if (categoryType == CategoryType::TempoMarks || categoryType == CategoryType::TempoAlterations) {
        if (const auto tempoAlteration = classifyTempoAlteration(normalizedText, categoryType, false)) {
            return *tempoAlteration;
        }
        if (const auto tempo = classifyTempo(normalizedText, categoryType, false)) {
            return *tempo;
        }
        if (categoryType == CategoryType::TempoMarks) {
            return *classifyTempo(normalizedText, categoryType);
        }
        return *classifyTempoAlteration(normalizedText, categoryType);
    }
    if (const auto dynamic = classifyDynamicExpression(normalizedText, categoryType)) {
        return *dynamic;
    }
    if (const auto technique = classifyTechnique(normalizedText, categoryType)) {
        return *technique;
    }
    return classifyGenericText(normalizedText, categoryType);
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def)
{
    const CategoryType categoryType = categoryTypeForExpression(def);
    const std::string normalizedText = normalizeExpressionText(expressionText(def));

    if (hasTempoPlayback(def)) {
        return *classifyTempo(def, normalizedText, categoryType);
    }
    if (categoryType == CategoryType::RehearsalMarks) {
        return *classifyRehearsalMark(normalizedText, categoryType);
    }
    if (categoryType == CategoryType::Dynamics) {
        return *classifyDynamicExpression(def, normalizedText, categoryType);
    }
    if (categoryType == CategoryType::TempoMarks || categoryType == CategoryType::TempoAlterations) {
        if (const auto tempoAlteration = classifyTempoAlteration(normalizedText, categoryType, false)) {
            return *tempoAlteration;
        }
        if (const auto tempo = classifyTempo(normalizedText, categoryType, false)) {
            return *tempo;
        }
        if (categoryType == CategoryType::TempoMarks) {
            return *classifyTempo(normalizedText, categoryType);
        }
        return *classifyTempoAlteration(normalizedText, categoryType);
    }
    if (const auto dynamic = classifyDynamicExpression(def, normalizedText, categoryType)) {
        return *dynamic;
    }
    if (const auto technique = classifyTechnique(normalizedText, categoryType)) {
        return *technique;
    }
    return classifyGenericText(normalizedText, categoryType);
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def)
{
    const CategoryType categoryType = categoryTypeForExpression(def);

    if (const auto tempo = classifyTempo(def, categoryType)) {
        return *tempo;
    }
    return suppressShapeExpression();
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    const CategoryType categoryType = categoryTypeForExpression(def);
    const std::string normalizedText = normalizeExpressionText(expressionText(def));

    if (hasTempoPlayback(def)) {
        return *classifyTempo(def, normalizedText, categoryType);
    }
    if (assignmentUsesStaffList(assignment)) {
        return classifyStaffListTextExpression(normalizedText, categoryType);
    }
    return classifyExpression(def);
}

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    const CategoryType categoryType = categoryTypeForExpression(def);

    if (const auto tempo = classifyTempo(def, categoryType)) {
        return *tempo;
    }
    if (assignmentUsesStaffList(assignment)) {
        return classifyStaffListTextExpression({}, categoryType);
    }
    return classifyExpression(def);
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
    return suppressShapeExpression();
}

} // namespace denigma::classify
