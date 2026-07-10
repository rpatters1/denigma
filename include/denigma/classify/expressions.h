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

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "denigma/classify/articulations.h"
#include "denigma/classify/dynamics.h"
#include "musx/musx.h"

namespace denigma {
namespace classify {

using ExpressionCategoryType = musx::dom::others::MarkingCategory::CategoryType;

/// @enum ExpressionType
/// @brief Exporter-neutral semantic classes for Finale/MUSX text expressions.
enum class ExpressionType
{
    GenericText,
    Dynamic,
    Fermata,
    BreathMark,
    HarpDiagram,
    PseudoTie,
    NonArpeggio,
    TempoMark,
    TempoAlteration,
    TechniqueText,
    RehearsalMark,
    Error,
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

struct TechniqueText
{
    /// @enum Type
    /// @brief Common performance technique text values recognized by the classifier.
    enum class Type
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
        Other
    };

    Type type{};
    std::string text;
};

struct TempoInfo
{
    std::string text;
    int beatsPerMinute{};
    int beatUnitEdu{};
};

struct TempoText
{
    TempoInfo tempo;
};

struct TempoAlteration
{
    TempoInfo tempo;
};

struct ExpressionFermata
{
    Fermata fermata{};
    std::optional<std::string> glyphName;
    GlyphStyle glyphStyle{};
    bool isRightBarline{};
};

struct ExpressionBreathMark
{
    BreathMark breathMark{};
    std::optional<std::string> glyphName;
};

struct HarpDiagram
{
    enum class PedalPosition
    {
        Flat,
        Natural,
        Sharp
    };

    PedalPosition d{};
    PedalPosition c{};
    PedalPosition b{};
    PedalPosition e{};
    PedalPosition f{};
    PedalPosition g{};
    PedalPosition a{};
};

enum class PseudoTieType
{
    LaissezVibrer,
    TieEnd
};

struct ExpressionPseudoTie
{
    PseudoTieType type{ PseudoTieType::LaissezVibrer };
    musx::dom::CurveContourDirection direction{ musx::dom::CurveContourDirection::Unspecified };
};

struct ExpressionNonArpeggio
{
    musx::util::ArpeggioSpanCandidate candidate;
};

struct RehearsalMark
{
    std::string text;
};

struct GenericText
{
    std::string text;
};

struct DynamicQualifier
{
    DynamicChange change{ DynamicChange::Absolute };
    std::string text;
};

struct ExpressionError
{
    std::string message;
};

struct Suppress
{
};

using ExpressionRunValue = std::variant<std::monostate, DynamicMark, DynamicQualifier, ExpressionFermata, ExpressionBreathMark, TempoText, TempoAlteration, TechniqueText, RehearsalMark, GenericText, ExpressionError, Suppress>;
using ExpressionValue = std::variant<std::monostate, DynamicMark, ExpressionFermata, ExpressionBreathMark, HarpDiagram, ExpressionPseudoTie, ExpressionNonArpeggio, TempoText, TempoAlteration, TechniqueText, RehearsalMark, GenericText, ExpressionError, Suppress>;

struct ExpressionRun
{
    musx::util::EnigmaTextChunk chunk;
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    ExpressionRunValue value{};

    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }
};

struct ExpressionClassification
{
    ExpressionType type{ ExpressionType::GenericText };
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    std::optional<musx::util::EnigmaParsingContext> enigmaCtx;
    ExpressionValue value{};
    std::vector<ExpressionRun> runs;

    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }

private:
    template <typename T, ExpressionType EnumVal>
    const T& checkedPayload(const char* enumDesc) const
    {
        assert(type == EnumVal);
        if (type != EnumVal) {
            throw std::logic_error(std::string("ExpressionClassification type is not ") + enumDesc + ".");
        }
        return std::get<T>(value);
    }

public:

    const DynamicMark& dynamic() const
    { return checkedPayload<DynamicMark, ExpressionType::Dynamic>("Dynamic"); }

    const ExpressionFermata& fermata() const
    { return checkedPayload<ExpressionFermata, ExpressionType::Fermata>("Fermata"); }

    const ExpressionBreathMark& breathMark() const
    { return checkedPayload<ExpressionBreathMark, ExpressionType::BreathMark>("BreathMark"); }

    const HarpDiagram& harpDiagram() const
    { return checkedPayload<HarpDiagram, ExpressionType::HarpDiagram>("HarpDiagram"); }

    const ExpressionPseudoTie& pseudoTie() const
    { return checkedPayload<ExpressionPseudoTie, ExpressionType::PseudoTie>("PseudoTie"); }

    const ExpressionNonArpeggio& nonArpeggio() const
    { return checkedPayload<ExpressionNonArpeggio, ExpressionType::NonArpeggio>("NonArpeggio"); }

    const TempoText& tempoText() const
    { return checkedPayload<TempoText, ExpressionType::TempoMark>("TempoText"); }

    const TempoAlteration& tempoAlteration() const
    { return checkedPayload<TempoAlteration, ExpressionType::TempoAlteration>("TempoAlteration"); }

    const TechniqueText& techniqueText() const
    { return checkedPayload<TechniqueText, ExpressionType::TechniqueText>("TechniqueText"); }

    const RehearsalMark& rehearsalMark() const
    { return checkedPayload<RehearsalMark, ExpressionType::RehearsalMark>("RehearsalMark"); }

    const GenericText& genericText() const
    { return checkedPayload<GenericText, ExpressionType::GenericText>("GenericText"); }

    const ExpressionError& error() const
    { return checkedPayload<ExpressionError, ExpressionType::Error>("Error"); }
};

struct ExpressionAssignmentClassification
{
    musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign> assignment;
    ExpressionClassification classification;
};

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

std::vector<ExpressionAssignmentClassification> classifyExpressionAssignments(
    const musx::dom::MusxInstanceList<musx::dom::others::MeasureExprAssign>& assignments);

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment = {});

ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment = {});

} // namespace classify
} // namespace denigma
