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

#include <stdexcept>

namespace denigma {
namespace classify {

using namespace smartshape;

namespace {

KeyboardPedal::Cap classifyPedalCap(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine,
    bool atStart)
{
    using CustomLine = musx::dom::others::SmartShapeCustomLine;
    const auto capType = atStart ? customLine->lineCapStartType : customLine->lineCapEndType;
    if (capType == CustomLine::LineCapType::Hook) {
        return { KeyboardPedal::Cap::Type::Hook };
    }
    if (capType != CustomLine::LineCapType::ArrowheadCustom) {
        return {};
    }

    const auto arrowId = atStart ? customLine->lineCapStartArrowId : customLine->lineCapEndArrowId;
    const auto shapeDef = customLine->getDocument()->getOthers()->get<musx::dom::others::ShapeDef>(
        customLine->getRequestedPartId(), arrowId);
    if (!shapeDef) {
        return {};
    }

    const auto recognized = shapeDef->recognize();
    using KnownType = musx::dom::KnownShapeDefType;
    switch (recognized) {
    case KnownType::PedalArrowheadDown:
        return { KeyboardPedal::Cap::Type::PedalDown, recognized };
    case KnownType::PedalArrowheadUp:
        return { KeyboardPedal::Cap::Type::PedalUp, recognized };
    case KnownType::PedalArrowheadShortUpDownLongUp:
    case KnownType::PedalArrowheadLongUpDownShortUp:
        return { KeyboardPedal::Cap::Type::PedalChange, recognized };
    default:
        return { KeyboardPedal::Cap::Type::None, recognized };
    }
}

} // namespace

std::optional<KeyboardPedal> classifyKeyboardPedalCustomLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine)
{
    using CustomLine = musx::dom::others::SmartShapeCustomLine;
    if (!customLine) {
        return std::nullopt;
    }

    KeyboardPedal result;
    result.customLine = customLine;
    result.startText = classifyKeyboardPedal(customLine->getLeftStartRawTextCtx(customLine->getRequestedPartId()));
    result.continuationText = classifyKeyboardPedal(customLine->getLeftContRawTextCtx(customLine->getRequestedPartId()));
    result.endText = classifyKeyboardPedal(customLine->getRightEndRawTextCtx(customLine->getRequestedPartId()));
    result.startCap = classifyPedalCap(customLine, true);
    result.endCap = classifyPedalCap(customLine, false);
    result.lineStyle = customLine->lineStyle;
    result.lineVisible = customLine->lineStyle != CustomLine::LineStyle::Char
        || (customLine->charParams && customLine->charParams->lineChar != U' ');

    const bool hasPedalCap = result.startCap.type == KeyboardPedal::Cap::Type::PedalDown
        || result.startCap.type == KeyboardPedal::Cap::Type::PedalUp
        || result.startCap.type == KeyboardPedal::Cap::Type::PedalChange
        || result.endCap.type == KeyboardPedal::Cap::Type::PedalDown
        || result.endCap.type == KeyboardPedal::Cap::Type::PedalUp
        || result.endCap.type == KeyboardPedal::Cap::Type::PedalChange;
    if (!result.startText && !result.continuationText && !result.endText && !hasPedalCap) {
        return std::nullopt;
    }
    return result;
}

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
        } else if (shape->lineStyleId != 0) {
            const auto customLine = shape->getDocument()->getOthers()->get<musx::dom::others::SmartShapeCustomLine>(
                shape->getRequestedPartId(), shape->lineStyleId);
            if (auto keyboardPedal = classifyKeyboardPedalCustomLine(customLine)) {
                result.value = std::move(*keyboardPedal);
            }
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
        MUSX_ASSERT_IF(startEntry->getEntry()->notes.size() != 1) {
            throw std::logic_error("musxdom classified an arpeggiated tie on an entry with note count other than 1.");
        }
        result.value = ArpeggiatedTie{ musx::dom::NoteInfoPtr(startEntry, 0), tiedTo, contour };
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
