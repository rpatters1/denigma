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

#include "mnx.h"
#include "mnx_smartshapes.h"
#include "mnx_markings.h"

namespace denigma {
namespace mnxexp {

namespace {
void appendHairpin(const MnxMusxMappingPtr&, mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::SmartShape>& shape, mnx::DynamicWedgeType wedgeType)
{
    const auto startPos = mnxFractionFromFraction(shape->startTermSeg->endPoint->calcGlobalPosition());
    const auto endPos = mnx::MeasureRhythmicPosition::make(
        calcGlobalMeasureId(shape->endTermSeg->endPoint->measId),
        mnxFractionFromFraction(shape->endTermSeg->endPoint->calcGlobalPosition()));
    auto mnxDynamic = mnxMeasure.ensure_dynamics().appendGradual(wedgeType, startPos, endPos);
    /// @todo Perhaps get smarter about setting start/end grace index using situational heuristics
    mnxDynamic.position().set_graceIndex(0);        // always after grace notes
    mnxDynamic.end().position().set_graceIndex(0);  // always after grace notes
    if (mnxStaffNumber) {
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}
} // namespace

void processSmartShapes(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasSmartShape) {
        const auto assigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& assign : assigns) {
            MUSX_ASSERT_IF(!assign) {
                context->logMessage(LogMsg() << "skipping empty smart shape assignment for measure " << musxMeasure->getCmper(), MessageSeverity::Warning);
                continue;
            }
            if (assign->centerShapeNum != 0) {
                // ignore assignments in the middle of the shape
                continue;
            }
            const auto shape = context->document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
            if (!shape || shape->startTermSeg->endPoint->staffId != context->current.staff) {
                continue;
            }
            using ST = musx::dom::others::SmartShape::ShapeType;
            switch (shape->shapeType) {
            case ST::Crescendo:
                appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnx::DynamicWedgeType::Increasing);
                break;
            case ST::Decrescendo:
                appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnx::DynamicWedgeType::Decreasing);
                break;
            default:
                if (const auto nonArpeggio = musx::util::calcNonArpeggioSpanForSmartShape(shape)) {
                    appendArpeggioCandidate(context, mnxMeasure, nonArpeggio.value());
                }
                break;
            }
        }
    }
}

void createOttavas(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    const StaffCmper staffCmper = context->current.staff;
    context->current.ottavasApplicableInMeasure.clear();
    if (musxMeasure->hasSmartShape) {
        auto shapeAssigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(asgn->getRequestedPartId(), asgn->shapeNum)) {
                if (!shape->hidden && (shape->startTermSeg->endPoint->staffId == staffCmper || shape->endTermSeg->endPoint->staffId == staffCmper)) {
                    switch (shape->shapeType) {
                        default: continue;
                        case others::SmartShape::ShapeType::OctaveDown:
                        case others::SmartShape::ShapeType::OctaveUp:
                        case others::SmartShape::ShapeType::TwoOctaveDown:
                        case others::SmartShape::ShapeType::TwoOctaveUp:
                            break;
                    }
                    context->current.ottavasApplicableInMeasure.emplace(shape->getCmper(), shape);
                    if (!asgn->centerShapeNum && shape->startTermSeg->endPoint->measId == musxMeasure->getCmper()) {
                        auto mnxOttava = mnxMeasure.ensure_ottavas().append(
                            enumConvert<mnx::OttavaAmount>(shape->shapeType),
                            mnxFractionFromSmartShapeEndPoint(shape->startTermSeg->endPoint),
                            calcGlobalMeasureId(shape->endTermSeg->endPoint->measId),
                            mnxFractionFromSmartShapeEndPoint(shape->endTermSeg->endPoint));
                        mnxOttava.end().position().set_graceIndex(0);   // guarantees inclusion of any grace notes at the end of the ottava
                        if (mnxStaffNumber) {
                            mnxOttava.set_staff(mnxStaffNumber.value());
                        }
                        /// @todo: orient (if applicable)
                    }
                }
            }
        }
    }
}

} // namespace mnxexp
} // namespace denigma
