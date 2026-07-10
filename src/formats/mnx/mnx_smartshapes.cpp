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
#include "mnx_articulations.h"
#include "denigma/classify/smartshapes.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {
void appendHairpin(const MnxMusxMappingPtr&, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::SmartShape>& shape, mnxdom::DynamicWedgeType wedgeType)
{
    const auto startPos = mnxFractionFromFraction(shape->startTermSeg->endPoint->calcGlobalPosition());
    const auto endPos = mnxdom::MeasureRhythmicPosition::make(
        calcGlobalMeasureId(shape->endTermSeg->endPoint->measId),
        mnxFractionFromFraction(shape->endTermSeg->endPoint->calcGlobalPosition()));
    auto mnxDynamic = mnxMeasure.ensure_dynamics().appendGradual(wedgeType, startPos, endPos);
    /// @todo Perhaps get smarter about setting start/end grace index using situational heuristics
    mnxDynamic.position().set_graceIndex(0);        // always after grace notes
    mnxDynamic.end().position().set_graceIndex(0);  // always after grace notes
    mnxDynamic.set_or_clear_orient(mnxMultiStaffOrientFromVerticalPlacement(mnxStaffNumber, shape->calcVerticalPlacementForBeatAttached()));
    if (mnxStaffNumber > 1) { // we get better import results not specifying the 1st staff number: this could become an option
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}
} // namespace

void processSmartShapes(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
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
            if (!shape || !shape->calcIsValid()) {
                continue;
            }
            if (shape->startTermSeg->endPoint->staffId != context->current.staff || shape->startTermSeg->endPoint->measId != musxMeasure->getCmper()) {
                continue;
            }
            const auto classification = denigma::classify::classifySmartShape(shape);
            if (classification.as<denigma::classify::smartshape::Crescendo>()) {
                appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnxdom::DynamicWedgeType::Increasing);
            } else if (classification.as<denigma::classify::smartshape::Decrescendo>()) {
                appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnxdom::DynamicWedgeType::Decreasing);
            } else if (const auto* nonArpeggio = classification.as<denigma::classify::smartshape::NonArpeggio>()) {
                appendArpeggioCandidate(context, mnxMeasure, nonArpeggio->candidate);
            }
            // slurs, ottavas, pseudotie, and arpeggio tie classifications are handled elsewhere
        }
    }
}

void processSlurs(const MnxMusxMappingPtr&, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();
    const auto currentEntryNumber = musxEntry->getEntryNumber();
    auto createOneSlur = [&](const EntryNumber targetEntry) -> mnxdom::sequence::Slur {
        auto mnxSlurs = mnxEvent.ensure_slurs();
        return mnxSlurs.append(calcEventId(targetEntry));
    };
    if (musxEntry->smartShapeDetail) {
        auto shapeAssigns = musxEntry->getDocument()->getDetails()->getArray<details::SmartShapeEntryAssign>(
            SCORE_PARTID, musxEntry->getEntryNumber());
        for (const auto& assign : shapeAssigns) {
            if (auto shape = musxEntry->getDocument()->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum)) {
                if (!shape->calcIsSlur()) {
                    continue;
                }
                const auto classification = denigma::classify::classifySmartShape(shape);
                const auto* slur = classification.as<denigma::classify::smartshape::Slur>();
                if (!slur) {
                    // pseudotie and arpeggio tie classifications are handled elsewhere
                    continue;
                }
                MUSX_ASSERT_IF(!slur->startEntry || !slur->endEntry) {
                    continue;
                }
                const auto startEntryNumber = slur->startEntry->getEntry()->getEntryNumber();
                if (currentEntryNumber != startEntryNumber) {
                    continue;
                }
                const auto targetEntryNumber = slur->endEntry->getEntry()->getEntryNumber();
                if (targetEntryNumber == currentEntryNumber) {
                    continue;
                }

                auto mnxSlur = createOneSlur(targetEntryNumber);
                mnxSlur.set_lineType(shape->calcIsDashed() ? mnxdom::LineType::Dashed : mnxdom::LineType::Solid);
                if (slur->contour != CurveContourDirection::Unspecified) {
                    mnxSlur.set_side(slur->contour == CurveContourDirection::Up ? mnxdom::SlurTieSide::Up : mnxdom::SlurTieSide::Down);
                }
            }
        }
    }
}

void createOttavas(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    const StaffCmper staffCmper = context->current.staff;
    context->current.ottavasApplicableInMeasure = collectOttavasForMeasureStaff(
        context->document, musxMeasure->getRequestedPartId(), musxMeasure, staffCmper);
    if (musxMeasure->hasSmartShape) {
        auto shapeAssigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(asgn->getRequestedPartId(), asgn->shapeNum)) {
                if (context->current.ottavasApplicableInMeasure.contains(shape->getCmper())) {
                    if (!asgn->centerShapeNum && shape->startTermSeg->endPoint->measId == musxMeasure->getCmper()) {
                        auto mnxOttava = mnxMeasure.ensure_ottavas().append(
                            enumConvert<mnxdom::OttavaAmount>(shape->shapeType),
                            mnxFractionFromSmartShapeEndPoint(shape->startTermSeg->endPoint),
                            mnxdom::MeasureRhythmicPosition::make(calcGlobalMeasureId(shape->endTermSeg->endPoint->measId),
                                                               mnxFractionFromSmartShapeEndPoint(shape->endTermSeg->endPoint)));
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

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
