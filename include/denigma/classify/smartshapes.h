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

#include "denigma/classify/keyboard_pedals.h"
#include "musx/musx.h"
#include "musx/util/Arpeggio.h"

namespace denigma {
namespace classify {

namespace smartshape {

struct Ottava
{
    int octaveShift{};
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
    struct Cap
    {
        enum class Type
        {
            None,
            Hook,
            PedalDown,
            PedalUp,
            PedalChange
        };

        Type type{};
        musx::dom::KnownShapeDefType customShapeType{ musx::dom::KnownShapeDefType::Unrecognized };
    };

    musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine> customLine;
    std::optional<KeyboardPedalClassification> startText;
    std::optional<KeyboardPedalClassification> continuationText;
    std::optional<KeyboardPedalClassification> endText;
    Cap startCap;
    Cap endCap;
    musx::dom::others::SmartShapeCustomLine::LineStyle lineStyle{};
    bool lineVisible{};
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
    smartshape::KeyboardPedal>;

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
