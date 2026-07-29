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

#include <optional>
#include <variant>

#include "denigma/classify/classifier_common.h"
#include "denigma/classify/general_lines.h"
#include "denigma/classify/keyboard_pedals.h"
#include "musx/musx.h"
#include "musx/util/Arpeggio.h"

namespace denigma {
namespace classify {

namespace smartshape {

/// @struct Ottava
/// @brief An octave-displacement span, from either a built-in ottava shape or a custom line.
///
/// Finale scores sometimes pair a hidden built-in ottava, which carries the octave semantics, with
/// a visible custom line that supplies the appearance. Both shapes classify as an Ottava, and
/// #calcIsSemanticCarrier distinguishes them.
struct Ottava
{
    /// Octaves the written pitches are displaced: negative sounds below written pitch.
    int octaveShift{};

    /// When this shape is a custom line whose octave semantics are carried by a hidden
    /// built-in ottava covering the same music, the counterpart. Consumers should treat
    /// this shape as appearance-only when set.
    musx::dom::MusxInstance<musx::dom::others::SmartShape> hiddenCounterpart;

    /// True when this shape is a hidden built-in ottava that is rendered by a visible
    /// custom line. Consumers should process it despite the shape's hidden flag.
    bool hasVisualProxy{};

    /// Appearance, when this shape is a custom line. (Empty for built-in ottavas.)
    std::optional<GeneralLine> line;

    /// True when this shape carries the octave-displacement semantics. Exactly the
    /// carriers participate in pitch mapping; paired visual custom lines return false.
    [[nodiscard]]
    bool calcIsSemanticCarrier() const noexcept
    { return !hiddenCounterpart; }
};

/// @struct Crescendo
/// @brief A crescendo hairpin. Its extent comes from the source shape's endpoints.
struct Crescendo
{
};

/// @struct Decrescendo
/// @brief A decrescendo hairpin. Its extent comes from the source shape's endpoints.
struct Decrescendo
{
};

/// @struct Slur
/// @brief A slur between two entries.
struct Slur
{
    /// The entry at the slur's start, or null when the endpoint coincides with no entry.
    musx::dom::EntryInfoPtr startEntry;
    /// The entry at the slur's end, or null when the endpoint coincides with no entry.
    /// Exporters may host such floating endpoints however their target format allows.
    musx::dom::EntryInfoPtr endEntry;
    /// The slur's resolved curvature direction.
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

/// @struct ArpeggiatedTie
/// @brief A slur used to tie one note of a rolled chord to a note in a following chord.
struct ArpeggiatedTie
{
    musx::dom::NoteInfoPtr tiedFrom;    ///< The note the tie starts from.
    musx::dom::NoteInfoPtr tiedTo;      ///< The note the tie ends on.
    /// The tie's resolved curvature direction.
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

/// @struct NonArpeggio
/// @brief A bracket spanning notes that are explicitly not to be rolled.
struct NonArpeggio
{
    /// The notes the bracket spans, as resolved by musxdom.
    musx::util::ArpeggioSpanCandidate candidate;
};

/// @struct KeyboardPedal
/// @brief A custom line that notates keyboard pedaling.
///
/// The pedal semantics come from the line's texts, its caps, or both: a line qualifies as a
/// keyboard pedal when at least one text position or one cap identifies a pedal marking.
struct KeyboardPedal
{
    /// @enum CapType
    /// @brief The pedal semantic of a line cap. (Appearance details are in #line.)
    enum class CapType
    {
        None,           ///< No cap, or a cap with no pedal meaning.
        Hook,           ///< A plain hook, which brackets the pedaled music without naming a pedal action.
        PedalDown,      ///< The cap engages the pedal.
        PedalUp,        ///< The cap releases the pedal.
        PedalChange     ///< The cap releases and immediately re-engages the pedal.
    };

    GeneralLine line;                                       ///< Appearance of the pedal line.
    /// The marking in the line's left-start text, when it names one.
    std::optional<KeyboardPedalClassification> startText;
    /// The marking in the line's left-continuation text, when it names one.
    std::optional<KeyboardPedalClassification> continuationText;
    /// The marking in the line's right-end text, when it names one.
    std::optional<KeyboardPedalClassification> endText;
    CapType startCap{};                                     ///< Pedal meaning of the cap at the start of the line.
    CapType endCap{};                                       ///< Pedal meaning of the cap at the end of the line.

    /// @brief Returns whether any text position identifies the una-corda (soft) pedal.
    [[nodiscard]] bool isUnaCorda() const noexcept;

    /// @brief Returns whether any text position identifies the sostenuto pedal.
    [[nodiscard]] bool isSostPedal() const noexcept;

    /// @brief Returns whether any text position identifies the sustain pedal.
    [[nodiscard]] bool isSustainPedal() const noexcept;
};

/// @struct TrillLine
/// @brief A trill, with or without an extension line.
struct TrillLine
{
    bool includesTrSymbol{};        ///< True when the marking includes the trill symbol at the start.
    std::optional<GeneralLine> line; ///< Appearance, when the source is a custom line. (Empty for built-in trills.)
};

/// @struct VibratoLine
/// @brief A vibrato (wavy) line.
struct VibratoLine
{
    GeneralLine line;               ///< Appearance of the vibrato line. (Always a custom line.)
};

} // namespace smartshape

/// @brief The semantic payload of a classified smart shape.
///
/// std::monostate means the shape was not classified: it is invalid, or its type carries no
/// semantics Denigma models. A @ref smartshape::GeneralLine is the fallback for a line-type shape
/// that no more specific classification claimed.
using SmartShapeValue = std::variant<
    std::monostate,
    smartshape::Ottava,
    smartshape::Crescendo,
    smartshape::Decrescendo,
    smartshape::Slur,
    smartshape::ArpeggiatedTie,
    PseudoTie,
    smartshape::NonArpeggio,
    smartshape::KeyboardPedal,
    smartshape::TrillLine,
    smartshape::VibratoLine,
    smartshape::GeneralLine>;

/// @struct SmartShapeClassification
/// @brief Result returned by smart-shape classification.
struct SmartShapeClassification
{
    /// The source shape's type. Meaningful for any valid shape, including one whose #value is
    /// std::monostate, but left at its default when the shape itself was null or invalid.
    musx::dom::others::SmartShape::ShapeType shapeType{};
    /// What the shape means. (See @ref SmartShapeValue.)
    SmartShapeValue value{};

    /// @brief Returns the payload when the shape classified as the requested type, otherwise null.
    /// @tparam T One of the alternatives of @ref SmartShapeValue.
    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }
};

/// @brief Classifies a smart shape's meaning.
///
/// An invalid shape, or one whose type Denigma does not model, yields a classification whose
/// value is std::monostate.
[[nodiscard]]
SmartShapeClassification classifySmartShape(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape);

/// @brief Classifies a custom line style as keyboard pedaling.
///
/// This is the line-style half of @ref classifySmartShape's keyboard-pedal path, exposed for
/// callers that have a line style but no shape.
/// @return std::nullopt when @p customLine is null or names no pedal marking in either its
/// texts or its caps.
[[nodiscard]]
std::optional<smartshape::KeyboardPedal> classifyKeyboardPedalCustomLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine);

} // namespace classify
} // namespace denigma
