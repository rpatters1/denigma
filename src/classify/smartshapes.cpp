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
#include "denigma/classify/smartshapes.h"

namespace denigma {
namespace classify {

using namespace smartshape;

SmartShapeClassification classifySmartShape(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape)
{
    SmartShapeClassification result;
    if (!shape || !shape->calcIsValid()) {
        return result;
    }

    result.shapeType = shape->shapeType;
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    switch (shape->shapeType) {
    case ShapeType::OctaveDown:
        result.value = Ottava{ -1 };
        return result;
    case ShapeType::OctaveUp:
        result.value = Ottava{ 1 };
        return result;
    case ShapeType::TwoOctaveDown:
        result.value = Ottava{ -2 };
        return result;
    case ShapeType::TwoOctaveUp:
        result.value = Ottava{ 2 };
        return result;
    case ShapeType::Crescendo:
        result.value = Crescendo{};
        return result;
    case ShapeType::Decrescendo:
        result.value = Decrescendo{};
        return result;
    case ShapeType::CustomLine:
        if (const auto candidate = musx::util::calcNonArpeggioSpanForSmartShape(shape)) {
            result.value = NonArpeggio{ *candidate };
        }
        return result;
    default:
        break;
    }

    if (!shape->calcIsSlur()) {
        return result;
    }

    const auto startEntry = shape->startTermSeg->endPoint->calcAssociatedEntry(true);
    const auto endEntry = shape->endTermSeg->endPoint->calcAssociatedEntry(true);
    if (!startEntry || !endEntry) {
        return result;
    }

    const auto contour = shape->calcContourDirection();
    result.value = Slur{ startEntry, endEntry, contour };
    if (shape->entryBased) {
        return result;
    }

    if (const auto tiedTo = shape->calcArpeggiatedTieToNote(startEntry)) {
        result.value = ArpeggiatedTie{ tiedTo, contour };
        return result;
    }

    if (shape->calcIsPseudoTie(musx::utils::PseudoTieMode::LaissezVibrer, startEntry)) {
        result.value = PseudoTie{ PseudoTie::Type::LaissezVibrer, contour };
        return result;
    }
    if (shape->calcIsPseudoTie(musx::utils::PseudoTieMode::TieEnd, startEntry)) {
        result.value = PseudoTie{ PseudoTie::Type::TieEnd, contour };
        return result;
    }

    result.value = Slur{ startEntry, endEntry, contour };
    return result;
}

} // namespace classify
} // namespace denigma
