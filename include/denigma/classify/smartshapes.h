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

#include "denigma/classify/general_lines.h"
#include "denigma/classify/keyboard_pedals.h"
#include "musx/musx.h"
#include "musx/util/Arpeggio.h"

namespace denigma {
namespace classify {

namespace smartshape {

struct Ottava
{
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

struct Crescendo
{
};

struct Decrescendo
{
};

struct Slur
{
    musx::dom::EntryInfoPtr startEntry;
    musx::dom::EntryInfoPtr endEntry;
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

struct ArpeggiatedTie
{
    musx::dom::NoteInfoPtr tiedFrom;
    musx::dom::NoteInfoPtr tiedTo;
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

struct PseudoTie
{
    enum class Type
    {
        LaissezVibrer,
        TieEnd
    };

    // This payload exists to distinguish pseudo-tie smart shapes from true slurs.
    // It is not intended to replace NoteInfoPtr pseudo-tie orchestration, which
    // must still resolve all pseudo-tie sources at the note level.
    Type type{};
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

struct NonArpeggio
{
    musx::util::ArpeggioSpanCandidate candidate;
};

struct KeyboardPedal
{
    /// @enum CapType
    /// @brief The pedal semantic of a line cap. (Appearance details are in #line.)
    enum class CapType
    {
        None,
        Hook,
        PedalDown,
        PedalUp,
        PedalChange
    };

    GeneralLine line;                                       ///< Appearance of the pedal line.
    std::optional<KeyboardPedalClassification> startText;
    std::optional<KeyboardPedalClassification> continuationText;
    std::optional<KeyboardPedalClassification> endText;
    CapType startCap{};
    CapType endCap{};
};

struct TrillLine
{
    bool includesTrSymbol{};        ///< True when the marking includes the trill symbol at the start.
    std::optional<GeneralLine> line; ///< Appearance, when the source is a custom line. (Empty for built-in trills.)
};

struct VibratoLine
{
    GeneralLine line;               ///< Appearance of the vibrato line. (Always a custom line.)
};

} // namespace smartshape

using SmartShapeValue = std::variant<
    std::monostate,
    smartshape::Ottava,
    smartshape::Crescendo,
    smartshape::Decrescendo,
    smartshape::Slur,
    smartshape::ArpeggiatedTie,
    smartshape::PseudoTie,
    smartshape::NonArpeggio,
    smartshape::KeyboardPedal,
    smartshape::TrillLine,
    smartshape::VibratoLine,
    smartshape::GeneralLine>;

struct SmartShapeClassification
{
    musx::dom::others::SmartShape::ShapeType shapeType{};
    SmartShapeValue value{};

    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }
};

[[nodiscard]]
SmartShapeClassification classifySmartShape(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape);

[[nodiscard]]
std::optional<smartshape::KeyboardPedal> classifyKeyboardPedalCustomLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine);

} // namespace classify
} // namespace denigma
