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

struct StandardArticulation
{
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

    std::vector<Type> types;
};

struct Tremolo
{
    enum class Style
    {
        Measured,
        Unmeasured
    };

    Style style{};
    int marks{};
};

struct Fermata
{
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

    enum class Duration
    {
        Auto,
        VeryShort,
        Short,
        Long,
        VeryLong
    };

    Shape shape{};
    Duration duration{};
};

struct BreathMark
{
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

    Type type{};
};

struct Arpeggio
{
    enum class Type
    {
        VerticalSegment,
        Normal,
        Up,
        Down
    };

    Type type{};
};

struct VerticalEntryBracket
{
};

using ArticulationValue = std::variant<std::monostate, StandardArticulation, Tremolo, Fermata, BreathMark, Arpeggio, VerticalEntryBracket>;

struct ArticulationClassification
{
    ArticulationValue value{};
    std::optional<std::string> glyphName;

    bool isArticulation() const noexcept
    { return !std::holds_alternative<std::monostate>(value); }

    explicit operator bool() const noexcept
    { return isArticulation(); }

    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }

    template <typename T>
    bool is() const noexcept
    { return std::holds_alternative<T>(value); }
};

ArticulationClassification classifyArticulation(
    const musx::dom::details::ArticulationAssign::SelectedSymbolContext& context);
ArticulationClassification classifyArticulation(
    const musx::dom::MusxInstance<musx::dom::others::ArticulationDef>& def);
ArticulationClassification classifyArticulationSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol);

} // namespace denigma::classify
