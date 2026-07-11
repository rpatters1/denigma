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
#include "denigma/classify/classifier_common.h"
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
    KeyboardPedal,
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

namespace expression {

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

struct Fermata
{
    articulation::Fermata fermata{};
    std::optional<std::string> glyphName;
    GlyphStyle glyphStyle{};
    bool isRightBarline{};
};

struct BreathMark
{
    articulation::BreathMark breathMark{};
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

/// @enum KeyboardPedal
/// @brief Keyboard pedal changes recognized in Finale expressions.
enum class KeyboardPedal
{
    /// Sustain pedal (pedal 1), normally the rightmost pedal.
    PedalOne,
    /// Sostenuto pedal (pedal 2), normally the middle pedal.
    PedalTwo,
    /// Una corda or soft pedal (pedal 3), normally the leftmost pedal.
    PedalThree,
    /// Release the currently engaged keyboard pedal.
    PedalUp
};

struct PseudoTie
{
    enum class Type
    {
        LaissezVibrer,
        TieEnd
    };

    Type type{ Type::LaissezVibrer };
    musx::dom::CurveContourDirection direction{ musx::dom::CurveContourDirection::Unspecified };
};

struct NonArpeggio
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
    dynamics::Change change{ dynamics::Change::Absolute };
    std::string text;
};

struct Error
{
    std::string message;
};

struct Suppress
{
};

using RunValue = std::variant<
    std::monostate, dynamics::Mark, DynamicQualifier, Fermata, BreathMark, TempoText,
    TempoAlteration, TechniqueText, RehearsalMark, GenericText, Error, Suppress>;

struct RunClassification
{
    musx::util::EnigmaTextChunk chunk;
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    RunValue value{};

    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }
};

} // namespace expression

using ExpressionValue = std::variant<
    std::monostate, dynamics::Mark, expression::Fermata, expression::BreathMark,
    expression::HarpDiagram, expression::KeyboardPedal, expression::PseudoTie, expression::NonArpeggio,
    expression::TempoText, expression::TempoAlteration, expression::TechniqueText, expression::RehearsalMark,
    expression::GenericText, expression::Error, expression::Suppress>;

struct ExpressionClassification
{
    ExpressionType type{ ExpressionType::GenericText };
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    std::optional<musx::util::EnigmaParsingContext> enigmaCtx;
    ExpressionValue value{};
    std::vector<expression::RunClassification> runs;

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

    const dynamics::Mark& dynamic() const
    { return checkedPayload<dynamics::Mark, ExpressionType::Dynamic>("Dynamic"); }

    const expression::Fermata& fermata() const
    { return checkedPayload<expression::Fermata, ExpressionType::Fermata>("Fermata"); }

    const expression::BreathMark& breathMark() const
    { return checkedPayload<expression::BreathMark, ExpressionType::BreathMark>("BreathMark"); }

    const expression::HarpDiagram& harpDiagram() const
    { return checkedPayload<expression::HarpDiagram, ExpressionType::HarpDiagram>("HarpDiagram"); }

    expression::KeyboardPedal keyboardPedal() const
    { return checkedPayload<expression::KeyboardPedal, ExpressionType::KeyboardPedal>("KeyboardPedal"); }

    const expression::PseudoTie& pseudoTie() const
    { return checkedPayload<expression::PseudoTie, ExpressionType::PseudoTie>("PseudoTie"); }

    const expression::NonArpeggio& nonArpeggio() const
    { return checkedPayload<expression::NonArpeggio, ExpressionType::NonArpeggio>("NonArpeggio"); }

    const expression::TempoText& tempoText() const
    { return checkedPayload<expression::TempoText, ExpressionType::TempoMark>("TempoText"); }

    const expression::TempoAlteration& tempoAlteration() const
    { return checkedPayload<expression::TempoAlteration, ExpressionType::TempoAlteration>("TempoAlteration"); }

    const expression::TechniqueText& techniqueText() const
    { return checkedPayload<expression::TechniqueText, ExpressionType::TechniqueText>("TechniqueText"); }

    const expression::RehearsalMark& rehearsalMark() const
    { return checkedPayload<expression::RehearsalMark, ExpressionType::RehearsalMark>("RehearsalMark"); }

    const expression::GenericText& genericText() const
    { return checkedPayload<expression::GenericText, ExpressionType::GenericText>("GenericText"); }

    const expression::Error& error() const
    { return checkedPayload<expression::Error, ExpressionType::Error>("Error"); }
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
