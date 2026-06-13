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
#include <string>
#include <variant>
#include <vector>

#include "musx/musx.h"

namespace denigma::classify {

/// Classification for a standard articulation mark.
struct StandardArticulation
{
    /// Standard articulation types recognized by the classifier.
    enum class Type
    {
        Accent,
        BowDirectionUp,
        BowDirectionDown,
        SoftAccent,
        Spiccato,
        Staccatissimo,
        Staccato,
        Stress,
        StrongAccent,
        Tenuto,
        Unstress
    };

    /// One or more articulation types represented by the source symbol.
    std::vector<Type> types;
};

/// Classification for tremolo articulation marks.
struct Tremolo
{
    /// Distinguishes measured from unmeasured tremolo marks.
    enum class Style
    {
        Measured,
        Unmeasured
    };

    /// Tremolo style.
    Style style{};
    /// Number of tremolo strokes.
    int marks{};
};

/// Classification for fermata articulation marks.
struct Fermata
{
    /// Visual fermata shape.
    enum class Shape
    {
        Normal,
        Angled,
        DoubleAngled,
        Square,
        DoubleSquare,
        HalfCurve,
        DoubleDot,
        Curlew
    };

    /// Finale playback-duration class associated with the fermata.
    enum class Duration
    {
        Auto,
        VeryShort,
        Short,
        Long,
        VeryLong
    };

    /// Visual fermata shape.
    Shape shape{};
    /// Playback-duration class.
    Duration duration{};
};

/// Classification for breath marks and caesuras.
struct BreathMark
{
    /// Breath mark or caesura type.
    enum class Type
    {
        Comma,
        Tick,
        Upbow,
        Salzedo,
        Caesura,
        CaesuraCurved,
        CaesuraShort,
        CaesuraThick,
        ChantCaesura,
        CaesuraSingleStroke
    };

    /// Classified breath mark type.
    Type type{};
};

/// Classification for arpeggio articulation marks.
struct Arpeggio
{
    /// Arpeggio type.
    enum class Type
    {
        VerticalSegment,
        Normal,
        Up,
        Down
    };

    /// Classified arpeggio type.
    Type type{};
};

/// Classification for Finale vertical entry bracket shapes.
struct VerticalEntryBracket
{
};

/// Variant payload for articulation classification.
using ArticulationValue = std::variant<std::monostate, StandardArticulation, Tremolo, Fermata, BreathMark, Arpeggio, VerticalEntryBracket>;

/// Result returned by articulation classification.
struct ArticulationClassification
{
    /// Classified articulation payload, or std::monostate when no articulation was recognized.
    ArticulationValue value{};
    /// SMuFL glyph name associated with the recognized symbol, when available.
    std::optional<std::string> glyphName;

    /// Returns true when the source was recognized as an articulation.
    bool isArticulation() const noexcept
    { return !std::holds_alternative<std::monostate>(value); }

    /// Returns true when the source was recognized as an articulation.
    explicit operator bool() const noexcept
    { return isArticulation(); }

    /// Returns the classified payload as T, or nullptr when it has another type.
    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }

    /// Returns true when the classified payload has type T.
    template <typename T>
    bool is() const noexcept
    { return std::holds_alternative<T>(value); }
};

/// Classifies the selected symbol from an articulation assignment.
ArticulationClassification classifyArticulation(
    const musx::dom::details::ArticulationAssign::SelectedSymbolContext& context);
/// Classifies a Finale articulation definition.
ArticulationClassification classifyArticulation(
    const musx::dom::MusxInstance<musx::dom::others::ArticulationDef>& def);
/// Classifies an articulation symbol from a font and character code.
ArticulationClassification classifyArticulationSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol);

} // namespace denigma::classify
