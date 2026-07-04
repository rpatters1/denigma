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

#include "musicxml.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

#include "mx/api/CurveData.h"
#include "mx/api/DirectionData.h"
#include "mx/api/LineData.h"
#include "mx/api/WedgeData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

mx::api::LineType musicXmlLineType(const MusxInstance<others::SmartShape>& shape)
{
    return shape && shape->calcIsDashed() ? mx::api::LineType::dashed : mx::api::LineType::solid;
}

mx::api::NoteData* noteDataAt(MusicXmlMusxMapping& context, const MusicXmlNoteLocation& location)
{
    if (!context.currentPart || location.measureIndex >= context.currentPart->measures.size()) {
        return nullptr;
    }
    auto& measure = context.currentPart->measures[location.measureIndex];
    if (location.staffIndex >= measure.staves.size()) {
        return nullptr;
    }
    auto& staff = measure.staves[location.staffIndex];
    if (location.userVoiceNumber <= 0) {
        return nullptr;
    }
    const auto voiceIndex = static_cast<size_t>(location.userVoiceNumber - 1);
    if (voiceIndex >= staff.voices.size()) {
        return nullptr;
    }
    auto& voice = staff.voices[voiceIndex];
    if (location.noteIndex >= voice.notes.size()) {
        return nullptr;
    }
    return &voice.notes[location.noteIndex];
}

std::optional<MusicXmlNoteLocation> findEndpointLocation(
    const MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint,
    NoteNumber noteId)
{
    if (!endpoint || !endpoint->calcIsAssigned()) {
        return std::nullopt;
    }

    if (endpoint->entryNumber == 0) {
        return std::nullopt;
    }

    if (noteId != 0) {
        const auto noteIt = context.noteLocations.find(musicXmlNoteKey(endpoint->entryNumber, noteId));
        if (noteIt != context.noteLocations.end()) {
            return noteIt->second;
        }
    }

    const auto entryIt = context.entryNumberToFirstNote.find(endpoint->entryNumber);
    if (entryIt != context.entryNumberToFirstNote.end()) {
        return entryIt->second;
    }
    return std::nullopt;
}

mx::api::DirectionData createSmartShapeDirection(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint,
    StaffCmper staffId,
    size_t staffIndex,
    VerticalPlacement placement)
{
    auto direction = mx::api::DirectionData{};
    direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(endpoint->calcGlobalPosition());
    direction.placement = enumConvert<mx::api::Placement>(placement);
    direction.isStaffValueSpecified = true;
    (void)staffId;
    (void)staffIndex;
    return direction;
}

mx::api::StaffData* staffDataForEndpoint(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    if (!context.currentPart || !endpoint || !endpoint->calcIsAssigned()) {
        return nullptr;
    }

    const auto measureNumber = std::to_string(endpoint->measId);
    auto measureIt = std::ranges::find_if(context.currentPart->measures, [&measureNumber](const mx::api::MeasureData& measure) {
        return measure.number == measureNumber;
    });
    if (measureIt == context.currentPart->measures.end()) {
        return nullptr;
    }

    const auto stavesIt = context.partIdToStaves.find(context.currentPart->uniqueId);
    if (stavesIt == context.partIdToStaves.end()) {
        return nullptr;
    }
    const auto staffIt = std::ranges::find(stavesIt->second, endpoint->staffId);
    if (staffIt == stavesIt->second.end()) {
        return nullptr;
    }

    const auto staffIndex = static_cast<size_t>(std::distance(stavesIt->second.begin(), staffIt));
    if (staffIndex >= measureIt->staves.size()) {
        return nullptr;
    }
    return &measureIt->staves[staffIndex];
}

bool processSlur(MusicXmlMusxMapping& context, const MusxInstance<others::SmartShape>& shape)
{
    if (!shape || shape->hidden || !shape->startTermSeg || !shape->endTermSeg
        || !shape->startTermSeg->endPoint || !shape->endTermSeg->endPoint
        || !shape->calcIsSlur() || !shape->entryBased) {
        return false;
    }

    const auto startAssign = shape->startTermSeg->endPoint->getEntryAssignment();
    const auto endAssign = shape->endTermSeg->endPoint->getEntryAssignment();
    if (!startAssign || !endAssign) {
        return false;
    }

    const auto startLocation = findEndpointLocation(context, shape->startTermSeg->endPoint, shape->startNoteId);
    const auto endLocation = findEndpointLocation(context, shape->endTermSeg->endPoint, shape->endNoteId);
    if (!startLocation || !endLocation) {
        return false;
    }

    auto* startNote = noteDataAt(context, *startLocation);
    auto* endNote = noteDataAt(context, *endLocation);
    if (!startNote || !endNote) {
        return false;
    }

    auto start = mx::api::CurveStart{ mx::api::CurveType::slur };
    start.curveOrientation = enumConvert<mx::api::CurveOrientation>(shape->calcContourDirection());
    start.lineData.lineType = musicXmlLineType(shape);
    startNote->noteAttachmentData.curveStarts.emplace_back(std::move(start));
    endNote->noteAttachmentData.curveStops.emplace_back(mx::api::CurveStop{ mx::api::CurveType::slur });
    return true;
}

void appendHairpin(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    mx::api::WedgeType wedgeType)
{
    if (!shape || shape->hidden || !shape->startTermSeg || !shape->endTermSeg
        || !shape->startTermSeg->endPoint || !shape->endTermSeg->endPoint) {
        return;
    }
    const auto startPoint = shape->startTermSeg->endPoint;
    const auto endPoint = shape->endTermSeg->endPoint;
    if (!startPoint->calcIsAssigned() || !endPoint->calcIsAssigned()) {
        return;
    }
    if (endPoint->staffId != staffId) {
        return;
    }

    const auto placement = shape->calcVerticalPlacementForBeatAttached();
    auto startDirection = createSmartShapeDirection(context, startPoint, staffId, staffIndex, placement);
    auto wedgeStart = mx::api::WedgeStart{};
    wedgeStart.wedgeType = wedgeType;
    wedgeStart.lineData.lineType = mx::api::LineType::solid;
    wedgeStart.colorData.red = 0;
    wedgeStart.colorData.green = 0;
    wedgeStart.colorData.blue = 0;
    startDirection.wedgeStarts.emplace_back(std::move(wedgeStart));
    staff.directions.emplace_back(std::move(startDirection));

    auto stopDirection = createSmartShapeDirection(context, endPoint, staffId, staffIndex, placement);
    stopDirection.wedgeStops.emplace_back(mx::api::WedgeStop{});
    if (auto* stopStaff = staffDataForEndpoint(context, endPoint)) {
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

} // namespace

void processSmartShapes(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex)
{
    if (!musxMeasure->hasSmartShape) {
        return;
    }

    const auto assigns = context.document->getOthers()->getArray<others::SmartShapeMeasureAssign>(
        musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
    for (const auto& assign : assigns) {
        MUSX_ASSERT_IF(!assign) {
            context.logMessage(LogMsg() << "Skipping empty smart shape assignment for measure "
                << musxMeasure->getCmper() << ".", MessageSeverity::Warning);
            continue;
        }
        if (assign->centerShapeNum != 0) {
            continue;
        }
        const auto shape = context.document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
        if (!shape || !shape->startTermSeg || !shape->startTermSeg->endPoint
            || shape->startTermSeg->endPoint->staffId != staffId
            || shape->startTermSeg->endPoint->measId != musxMeasure->getCmper()) {
            continue;
        }

        if (processSlur(context, shape)) {
            continue;
        }

        using ST = others::SmartShape::ShapeType;
        switch (shape->shapeType) {
        case ST::Crescendo:
            appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::crescendo);
            break;
        case ST::Decrescendo:
            appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::diminuendo);
            break;
        default:
            break;
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
