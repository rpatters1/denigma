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
#include "denigma/classify/keyboard_pedals.h"
#include "musx/musx.h"

namespace denigma {
namespace classify {

/// @brief Finale marking-category type used while classifying expressions.
using ExpressionCategoryType = musx::dom::others::MarkingCategory::CategoryType;

/// @enum ExpressionType
/// @brief Exporter-neutral semantic classes for Finale/MUSX text expressions.
enum class ExpressionType
{
    GenericText, ///< Text with no more specific recognized semantic.
    Dynamic, ///< A dynamic mark.
    Fermata, ///< A fermata symbol.
    BreathMark, ///< A breath-mark symbol.
    StringMute, ///< A string-mute on/off symbol.
    HarpDiagram, ///< A harp-pedal diagram.
    KeyboardPedal, ///< A keyboard-pedal marking.
    PseudoTie, ///< A shape expression used as a stand-in for a tie.
    NonArpeggio, ///< A shape expression used as a non-arpeggio sign.
    TempoMark, ///< A tempo indication.
    TempoAlteration, ///< A relative tempo alteration.
    TechniqueText, ///< Text identifying a performance technique.
    RehearsalMark, ///< A rehearsal mark.
    MultimeasureRestNumber, ///< A numeric label for a multimeasure rest.
    MeasureRepeatCount, ///< A count of the iterations of a measure repeat.
    Error, ///< An expression that could not be classified because of invalid source data.
    Suppress ///< An expression that should not be exported.
};

/// @enum ClassificationBasis
/// @brief Diagnostic reason for the selected expression classification.
enum class ClassificationBasis
{
    FinaleCategory, ///< Classification follows the assigned Finale category.
    Heuristic, ///< Classification was inferred without a usable Finale category.
    FinaleCategoryConfirmed, ///< Content analysis confirmed the assigned Finale category.
    FinaleCategoryCorrected, ///< Content analysis overrode the assigned Finale category.
    FallbackToGenericText ///< No specialized classification matched the expression.
};

namespace expression {

/// @struct TechniqueText
/// @brief Classified text identifying a performance technique.
struct TechniqueText
{
    /// @enum Type
    /// @brief Common performance technique text values recognized by the classifier.
    enum class Type
    {
        None, ///< No technique value.
        Arco, ///< Bowed string technique.
        Pizzicato, ///< Plucked string technique.
        ColLegno, ///< Unqualified col legno technique.
        ColLegnoBattuto, ///< Strike the string with the wood of the bow.
        ColLegnoTratto, ///< Draw the wood of the bow across the string.
        SulPonticello, ///< Play near the bridge.
        SulTasto, ///< Play over the fingerboard.
        Flautando, ///< Use a flute-like tone.
        Ordinario, ///< Return to ordinary technique.
        Mute, ///< Apply an unspecified mute.
        StraightMute, ///< Apply a straight mute.
        CupMute, ///< Apply a cup mute.
        HarmonMute, ///< Apply a Harmon mute.
        PlungerMute, ///< Apply a plunger mute.
        BucketMute, ///< Apply a bucket mute.
        SolotoneMute, ///< Apply a Solotone mute.
        StopMute, ///< Apply a stop mute.
        Stopped, ///< Use the stopped-horn technique.
        Open, ///< Return to an open or unmuted technique.
        Other ///< A recognized technique not represented by another value.
    };

    /// @brief Semantic technique identified by the classifier.
    Type type{};
    /// @brief Source display text.
    std::string text;
};

/// @struct TempoInfo
/// @brief Text and playback data associated with a tempo expression.
struct TempoInfo
{
    /// @brief Source display text.
    std::string text;
    /// @brief Finale playback tempo in beats per minute, or zero when unspecified.
    int beatsPerMinute{};
    /// @brief Finale EDU duration of one playback beat, or zero when unspecified.
    int beatUnitEdu{};
};

/// @struct TempoText
/// @brief Classified absolute tempo indication.
struct TempoText
{
    /// @brief Tempo text and playback values.
    TempoInfo tempo;
};

/// @struct TempoAlteration
/// @brief Classified relative tempo alteration.
struct TempoAlteration
{
    /// @brief Tempo text and any associated playback values.
    TempoInfo tempo;
};

/// @struct Fermata
/// @brief Fermata expression and its source-specific display properties.
struct Fermata
{
    /// @brief Semantic fermata classification.
    articulation::Fermata fermata{};
    /// @brief SMuFL glyph name when the source glyph can be resolved.
    std::optional<std::string> glyphName;
    /// @brief Visual style encoded by the source glyph.
    GlyphStyle glyphStyle{};
    /// @brief Whether the fermata is assigned to the right barline.
    bool isRightBarline{};
};

/// @struct BreathMark
/// @brief Breath-mark expression and its resolved source glyph.
struct BreathMark
{
    /// @brief Semantic breath-mark classification.
    articulation::BreathMark breathMark{};
    /// @brief SMuFL glyph name when the source glyph can be resolved.
    std::optional<std::string> glyphName;
};

/// @struct HarpDiagram
/// @brief Seven-pedal state encoded by a Finale harp-pedal diagram.
struct HarpDiagram
{
    /// @enum PedalPosition
    /// @brief Notated position of one harp pedal.
    enum class PedalPosition
    {
        Flat, ///< Upper pedal position.
        Natural, ///< Middle pedal position.
        Sharp ///< Lower pedal position.
    };

    /// @brief D-pedal position.
    PedalPosition d{};
    /// @brief C-pedal position.
    PedalPosition c{};
    /// @brief B-pedal position.
    PedalPosition b{};
    /// @brief E-pedal position.
    PedalPosition e{};
    /// @brief F-pedal position.
    PedalPosition f{};
    /// @brief G-pedal position.
    PedalPosition g{};
    /// @brief A-pedal position.
    PedalPosition a{};
};

/// @struct NonArpeggio
/// @brief Shape expression recognized as a non-arpeggio sign.
struct NonArpeggio
{
    /// @brief Source candidate describing the vertical arpeggio span.
    musx::util::ArpeggioSpanCandidate candidate;
};

/// @struct RehearsalMark
/// @brief Classified rehearsal-mark text.
struct RehearsalMark
{
    /// @brief Display text of the rehearsal mark.
    std::string text;
};

/// @struct MultimeasureRestNumber
/// @brief Classified numeric label for a multimeasure rest.
///
/// Finale draws the multimeasure rest number as part of the rest itself. Some documents instead
/// carry the number in a text expression so that it can be repositioned per staff or centered
/// between staves. Such an expression is recognized by its display properties rather than by its
/// marking category, whose name carries no reliable meaning.
struct MultimeasureRestNumber
{
    /// @brief Measure count displayed by the marking.
    int number{};
};

/// @struct MeasureRepeatCount
/// @brief Classified count of the iterations of a measure repeat.
///
/// Finale draws one- and two-bar repeats as staff alternate notation and has no counter of its own,
/// so documents number the iterations with a text expression centered over each repeated measure.
/// Such an expression is recognized by the alternate notation on its staff, because neither its text
/// nor its marking category distinguishes it from any other number.
struct MeasureRepeatCount
{
    /// @brief Iteration number displayed by the marking.
    int count{};
};

/// @struct GenericText
/// @brief Expression text without a more specific semantic classification.
struct GenericText
{
    /// @brief Source display text.
    std::string text;
};

/// @struct DynamicQualifier
/// @brief Text run that qualifies a neighboring dynamic mark.
struct DynamicQualifier
{
    /// @brief Dynamic change indicated by the qualifier.
    dynamics::Change change{ dynamics::Change::Absolute };
    /// @brief Source display text.
    std::string text;
};

/// @struct Error
/// @brief Classification failure caused by invalid or unsupported source data.
struct Error
{
    /// @brief Human-readable diagnostic.
    std::string message;
};

/// @struct Suppress
/// @brief Marker payload for an expression that should not be exported.
struct Suppress
{
};

/// @brief Semantic payload associated with one classified expression text run.
using RunValue = std::variant<
    std::monostate, dynamics::Mark, DynamicQualifier, Fermata, BreathMark, TempoText,
    TempoAlteration, TechniqueText, RehearsalMark, GenericText, Error, Suppress>;

/// @struct RunClassification
/// @brief Classification of one logical run within an expression.
struct RunClassification
{
    /// @brief Parsed Finale text chunk represented by this run.
    musx::util::EnigmaTextChunk chunk;
    /// @brief Evidence used to select #value.
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    /// @brief Semantic payload for this run.
    RunValue value{};

    /// @brief Returns the payload as @c T when it contains that type.
    /// @tparam T Requested payload type.
    /// @return A pointer to the payload, or `nullptr` when the active alternative differs.
    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }
};

} // namespace expression

/// @brief Semantic payload associated with an @ref ExpressionClassification.
using ExpressionValue = std::variant<
    std::monostate, dynamics::Mark, expression::Fermata, expression::BreathMark,
    articulation::StringMute, expression::HarpDiagram, keyboardpedal::Type, PseudoTie,
    expression::NonArpeggio, expression::TempoText, expression::TempoAlteration,
    expression::TechniqueText, expression::RehearsalMark, expression::MultimeasureRestNumber,
    expression::MeasureRepeatCount, expression::GenericText, expression::Error, expression::Suppress>;

/// @struct ExpressionClassification
/// @brief Exporter-neutral semantic classification of one Finale expression.
struct ExpressionClassification
{
    /// @brief Top-level semantic class.
    ExpressionType type{ ExpressionType::GenericText };
    /// @brief Evidence used to select #type and #value.
    ClassificationBasis basis{ ClassificationBasis::FallbackToGenericText };
    /// @brief Parsing context retained when the source contains Enigma text commands.
    std::optional<musx::util::EnigmaParsingContext> enigmaCtx;
    /// @brief Semantic payload associated with #type.
    ExpressionValue value{};
    /// @brief Per-run classifications for expressions containing mixed content.
    std::vector<expression::RunClassification> runs;

    /// @brief Returns the payload as @c T when it contains that type.
    /// @tparam T Requested payload type.
    /// @return A pointer to the payload, or `nullptr` when the active alternative differs.
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

    /// @brief Returns the classified dynamic mark.
    /// @throws std::logic_error if #type is not ExpressionType::Dynamic.
    const dynamics::Mark& dynamic() const
    { return checkedPayload<dynamics::Mark, ExpressionType::Dynamic>("Dynamic"); }

    /// @brief Returns the classified fermata.
    /// @throws std::logic_error if #type is not ExpressionType::Fermata.
    const expression::Fermata& fermata() const
    { return checkedPayload<expression::Fermata, ExpressionType::Fermata>("Fermata"); }

    /// @brief Returns the classified breath mark.
    /// @throws std::logic_error if #type is not ExpressionType::BreathMark.
    const expression::BreathMark& breathMark() const
    { return checkedPayload<expression::BreathMark, ExpressionType::BreathMark>("BreathMark"); }

    /// @brief Returns the classified string-mute symbol.
    /// @throws std::logic_error if #type is not ExpressionType::StringMute.
    const articulation::StringMute& stringMute() const
    { return checkedPayload<articulation::StringMute, ExpressionType::StringMute>("StringMute"); }

    /// @brief Returns the classified harp-pedal diagram.
    /// @throws std::logic_error if #type is not ExpressionType::HarpDiagram.
    const expression::HarpDiagram& harpDiagram() const
    { return checkedPayload<expression::HarpDiagram, ExpressionType::HarpDiagram>("HarpDiagram"); }

    /// @brief Returns the classified keyboard-pedal marking.
    /// @throws std::logic_error if #type is not ExpressionType::KeyboardPedal.
    keyboardpedal::Type keyboardPedal() const
    { return checkedPayload<keyboardpedal::Type, ExpressionType::KeyboardPedal>("KeyboardPedal"); }

    /// @brief Returns the classified pseudo-tie.
    /// @throws std::logic_error if #type is not ExpressionType::PseudoTie.
    const PseudoTie& pseudoTie() const
    { return checkedPayload<PseudoTie, ExpressionType::PseudoTie>("PseudoTie"); }

    /// @brief Returns the classified non-arpeggio sign.
    /// @throws std::logic_error if #type is not ExpressionType::NonArpeggio.
    const expression::NonArpeggio& nonArpeggio() const
    { return checkedPayload<expression::NonArpeggio, ExpressionType::NonArpeggio>("NonArpeggio"); }

    /// @brief Returns the classified absolute tempo indication.
    /// @throws std::logic_error if #type is not ExpressionType::TempoMark.
    const expression::TempoText& tempoText() const
    { return checkedPayload<expression::TempoText, ExpressionType::TempoMark>("TempoText"); }

    /// @brief Returns the classified relative tempo alteration.
    /// @throws std::logic_error if #type is not ExpressionType::TempoAlteration.
    const expression::TempoAlteration& tempoAlteration() const
    { return checkedPayload<expression::TempoAlteration, ExpressionType::TempoAlteration>("TempoAlteration"); }

    /// @brief Returns the classified performance-technique text.
    /// @throws std::logic_error if #type is not ExpressionType::TechniqueText.
    const expression::TechniqueText& techniqueText() const
    { return checkedPayload<expression::TechniqueText, ExpressionType::TechniqueText>("TechniqueText"); }

    /// @brief Returns the classified rehearsal mark.
    /// @throws std::logic_error if #type is not ExpressionType::RehearsalMark.
    const expression::RehearsalMark& rehearsalMark() const
    { return checkedPayload<expression::RehearsalMark, ExpressionType::RehearsalMark>("RehearsalMark"); }

    /// @brief Returns the classified multimeasure rest number.
    /// @throws std::logic_error if #type is not ExpressionType::MultimeasureRestNumber.
    const expression::MultimeasureRestNumber& multimeasureRestNumber() const
    { return checkedPayload<expression::MultimeasureRestNumber, ExpressionType::MultimeasureRestNumber>("MultimeasureRestNumber"); }

    /// @brief Returns the classified measure repeat count.
    /// @throws std::logic_error if #type is not ExpressionType::MeasureRepeatCount.
    const expression::MeasureRepeatCount& measureRepeatCount() const
    { return checkedPayload<expression::MeasureRepeatCount, ExpressionType::MeasureRepeatCount>("MeasureRepeatCount"); }

    /// @brief Returns the unclassified expression text.
    /// @throws std::logic_error if #type is not ExpressionType::GenericText.
    const expression::GenericText& genericText() const
    { return checkedPayload<expression::GenericText, ExpressionType::GenericText>("GenericText"); }

    /// @brief Returns the classification diagnostic.
    /// @throws std::logic_error if #type is not ExpressionType::Error.
    const expression::Error& error() const
    { return checkedPayload<expression::Error, ExpressionType::Error>("Error"); }
};

/// @struct ExpressionAssignmentClassification
/// @brief Classification paired with the Finale measure-expression assignment that produced it.
struct ExpressionAssignmentClassification
{
    /// @brief Source measure-expression assignment.
    musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign> assignment;
    /// @brief Semantic classification of the assigned expression.
    ExpressionClassification classification;
};

/// @brief Classifies the expression referenced by a Finale measure-expression assignment.
/// @param assignment Source assignment whose expression definition is resolved and classified.
/// @return Exporter-neutral expression classification.
ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment);

/// @brief Classifies a list of Finale measure-expression assignments.
/// @param assignments Source assignments to classify.
/// @return One assignment/classification pair for each input assignment.
std::vector<ExpressionAssignmentClassification> classifyExpressionAssignments(
    const musx::dom::MusxInstanceList<musx::dom::others::MeasureExprAssign>& assignments);

/// @brief Classifies a Finale text-expression definition.
/// @param def Source text-expression definition.
/// @param assignment Optional assignment providing placement and category context.
/// @return Exporter-neutral expression classification.
ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::TextExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment = {});

/// @brief Classifies a Finale shape-expression definition.
/// @param def Source shape-expression definition.
/// @param assignment Optional assignment providing placement and category context.
/// @return Exporter-neutral expression classification.
ExpressionClassification classifyExpression(
    const musx::dom::MusxInstance<musx::dom::others::ShapeExpressionDef>& def,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment = {});

} // namespace classify
} // namespace denigma
