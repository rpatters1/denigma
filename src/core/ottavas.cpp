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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "core/ottavas.h"

namespace denigma {

bool isOttavaShapeType(musx::dom::others::SmartShape::ShapeType shapeType)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    switch (shapeType) {
    case ShapeType::OctaveDown:
    case ShapeType::OctaveUp:
    case ShapeType::TwoOctaveDown:
    case ShapeType::TwoOctaveUp:
        return true;
    default:
        return false;
    }
}

OttavaShapeMap collectOttavasForMeasureStaff(
    const musx::dom::DocumentPtr& document,
    musx::dom::Cmper partId,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& measure,
    musx::dom::StaffCmper staffId)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;

    OttavaShapeMap result;
    if (!document || !measure || !measure->hasSmartShape) {
        return result;
    }

    const auto shapeAssigns = document->getOthers()->getArray<musx::dom::others::SmartShapeMeasureAssign>(partId, measure->getCmper());
    for (const auto& assignment : shapeAssigns) {
        if (!assignment) {
            continue;
        }
        const auto shape = document->getOthers()->get<musx::dom::others::SmartShape>(assignment->getRequestedPartId(), assignment->shapeNum);
        if (!shape || result.contains(shape->getCmper())) {
            continue;
        }
        // Cheap pre-filter: only built-in ottavas and custom lines can classify as ottavas.
        if (!isOttavaShapeType(shape->shapeType) && shape->shapeType != ShapeType::CustomLine) {
            continue;
        }
        if (shape->startTermSeg->endPoint->staffId != staffId && shape->endTermSeg->endPoint->staffId != staffId) {
            continue;
        }
        const auto classification = classify::classifySmartShape(shape);
        const auto* ottava = classification.as<classify::smartshape::Ottava>();
        if (!ottava || !ottava->calcIsSemanticCarrier()) {
            continue;
        }
        result.emplace(shape->getCmper(), OttavaInstance{ shape, *ottava });
    }
    return result;
}

int calcOttavaOctaveAdjustment(
    const OttavaShapeMap& ottavas,
    const musx::dom::NoteInfoPtr& noteInfo,
    const std::function<void(const musx::dom::NoteInfoPtr&)>& onTiedFromOutsideOttava)
{
    int adjustment = 0;
    for (const auto& [shapeId, instance] : ottavas) {
        static_cast<void>(shapeId);
        if (!instance.shape || !instance.shape->calcAppliesTo(noteInfo.getEntryInfo())) {
            continue;
        }

        auto tiedFromNoteInfo = noteInfo;
        while (tiedFromNoteInfo && tiedFromNoteInfo->tieEnd) {
            tiedFromNoteInfo = tiedFromNoteInfo.calcTieFrom();
        }
        if (!tiedFromNoteInfo || instance.shape->calcAppliesTo(tiedFromNoteInfo.getEntryInfo())) {
            adjustment += instance.classification.octaveShift;
        } else if (!noteInfo.isSameNote(tiedFromNoteInfo) && onTiedFromOutsideOttava) {
            onTiedFromOutsideOttava(tiedFromNoteInfo);
        }
    }
    return adjustment;
}

} // namespace denigma
