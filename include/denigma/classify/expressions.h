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

#include <string>
#include <string_view>
#include <vector>

#include "denigma/classify/dynamics.h"
#include "musx/musx.h"

namespace denigma {
namespace classify {

using ExpressionCategoryType = musx::dom::others::MarkingCategory::CategoryType;

/// @enum ExpressionType
/// @brief Exporter-neutral semantic classes for Finale/MUSX text expressions.
enum class ExpressionType
{
    Dynamic,
    TempoMark,
    TempoAlteration,
    TechniqueText,
    RehearsalMark,
    GenericText,
    Suppress
};

/// @enum ClassificationBasis
/// @brief Diagnostic reason for the selected expression classification.
enum class ClassificationBasis
{
    FinaleCategory,
    Heuristic,
    FinaleCategoryConfirmed,
    FinaleCategoryCorrected,
    FallbackToGenericText
};

/// @enum TechniqueType
/// @brief Common performance technique text values recognized by the classifier.
enum class TechniqueType
{
    None,
    Arco,
    Pizzicato,
    ColLegno,
    ColLegnoBattuto,
    ColLegnoTratto,
    SulPonticello,
    SulTasto,
    Flautando,
    Ordinario,
    Mute,
    StraightMute,
    CupMute,
    HarmonMute,
    PlungerMute,
    BucketMute,
    SolotoneMute,
    StopMute,
    Stopped,
    Open,
    Tremolo,
    Other
};

struct Technique
{
    TechniqueType type{};
    std::string text;
};

struct TempoInfo
{
    std::string text;
    int beatsPerMinute{};
    int beatUnitEdu{};
};

struct ExpressionClassification
{
    ExpressionType type{ ExpressionType::GenericText };
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    std::string text;

    Dynamic dynamic{};
    std::string dynamicPrefixText;
    std::string dynamicSuffixText;
    DynamicChange dynamicChange{ DynamicChange::Absolute };
    Technique technique;
    TempoInfo tempo;
};

struct ExpressionAssignmentClassification
{
    musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign> assignment;
    ExpressionClassification classification;
};

ExpressionClassification classifyExpression(
    std::string_view text,
    ExpressionCategoryType categoryType = ExpressionCategoryType::Invalid);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

std::vector<ExpressionAssignmentClassification> classifyExpressionAssignments(
    const musx::dom::MusxInstanceList<musx::dom::others::MeasureExprAssign>& assignments);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

} // namespace classify
} // namespace denigma
