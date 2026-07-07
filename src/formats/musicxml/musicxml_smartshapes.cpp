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
#include <array>
#include <iterator>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "mx/api/CurveData.h"
#include "mx/api/DirectionData.h"
#include "mx/api/LineData.h"
#include "mx/api/OttavaData.h"
#include "mx/api/WedgeData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

constexpr int MUSICXML_MAX_NUMBER_LEVEL = 16;

using SmartShapeNumberLevels = std::unordered_map<Cmper, int>;

struct MusicXmlOttavaEndpointAdjustment
{
    bool enabled{};
    bool startBeforeGraceNotes{};
    bool stopAfterEntriesAtEnd{};
};

constexpr MusicXmlOttavaEndpointAdjustment MUSICXML_OTTAVA_ENDPOINT_ADJUSTMENT{
    .enabled = true,
    .startBeforeGraceNotes = true,
    .stopAfterEntriesAtEnd = true
};

struct NumberedSmartShape
{
    Cmper shapeCmper{};
    MusicPoint start;
    MusicPoint end;
    int numberLevel{};
};

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
    const auto voiceIt = staff.voices.find(voiceIndex);
    if (voiceIt == staff.voices.end()) {
        return nullptr;
    }
    auto& voice = voiceIt->second;
    if (location.noteIndex >= voice.notes.size()) {
        return nullptr;
    }
    return &voice.notes[location.noteIndex];
}

bool rangesOverlap(const NumberedSmartShape& lhs, const NumberedSmartShape& rhs)
{
    return lhs.start < rhs.end && rhs.start < lhs.end;
}

void normalizeRange(MusicPoint& start, MusicPoint& end)
{
    if (end < start) {
        std::swap(start, end);
    }
}

std::optional<int> findSmartShapeNumberLevel(
    const SmartShapeNumberLevels& numberLevels,
    const MusxInstance<others::SmartShape>& shape)
{
    if (!shape) {
        return std::nullopt;
    }
    const auto numberIt = numberLevels.find(shape->getCmper());
    if (numberIt == numberLevels.end()) {
        return std::nullopt;
    }
    return numberIt->second;
}

SmartShapeNumberLevels assignSmartShapeNumberLevels(
    MusicXmlMusxMapping& context,
    const MusxInstanceList<others::Measure>& musxMeasures,
    const std::vector<StaffCmper>& staves)
{
    /// @todo This musical-range heuristic cannot fully account for MusicXML serialized note order
    /// with backup-based voices; see MusicXmlSmartShapes.DISABLED_SlursOverbarsMatchReference.
    /// If mx::api gains writer-side spanner number assignment, remove this.
    std::vector<NumberedSmartShape> shapes;
    std::unordered_set<Cmper> seenShapes;

    for (const auto& musxMeasure : musxMeasures) {
        if (!musxMeasure->hasSmartShape) {
            continue;
        }

        const auto assigns = context.document->getOthers()->getArray<others::SmartShapeMeasureAssign>(
            musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& assign : assigns) {
            MUSX_ASSERT_IF(!assign) {
                context.logMessage(LogMsg() << "Skipping empty smart shape assignment for measure "
                    << musxMeasure->getCmper() << ".", MessageSeverity::Warning);
                continue;
            }
            if (assign->centerShapeNum != 0 || !seenShapes.insert(assign->shapeNum).second) {
                continue;
            }

            const auto shape = context.document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
            if (!shape || !shape->calcIsValid()) {
                continue;
            }
            const auto startPoint = shape->startTermSeg->endPoint;
            const auto endPoint = shape->endTermSeg->endPoint;
            if (startPoint->measId != musxMeasure->getCmper()
                || std::ranges::find(staves, startPoint->staffId) == staves.end()
                || !startPoint->calcIsAssigned() || !endPoint->calcIsAssigned()) {
                continue;
            }

            auto range = shape->createGlobalMusicRange();
            normalizeRange(range.start, range.end);
            shapes.emplace_back(NumberedSmartShape{ shape->getCmper(), range.start, range.end, 0 });
        }
    }

    std::ranges::sort(shapes, [](const NumberedSmartShape& lhs, const NumberedSmartShape& rhs) {
        if (lhs.start != rhs.start) {
            return lhs.start < rhs.start;
        }
        if (lhs.end != rhs.end) {
            return lhs.end < rhs.end;
        }
        return lhs.shapeCmper < rhs.shapeCmper;
    });

    SmartShapeNumberLevels result;
    for (auto& shape : shapes) {
        std::array<bool, MUSICXML_MAX_NUMBER_LEVEL + 1> usedLevels{};
        for (const auto& previous : shapes) {
            if (previous.shapeCmper == shape.shapeCmper) {
                break;
            }
            if (previous.numberLevel > 0 && rangesOverlap(previous, shape)) {
                usedLevels[size_t(previous.numberLevel)] = true;
            }
        }

        for (int numberLevel = 1; numberLevel <= MUSICXML_MAX_NUMBER_LEVEL; ++numberLevel) {
            if (!usedLevels[size_t(numberLevel)]) {
                shape.numberLevel = numberLevel;
                result.emplace(shape.shapeCmper, numberLevel);
                break;
            }
        }

        if (shape.numberLevel == 0) {
            context.logMessage(LogMsg() << "Skipping MusicXML number for smart shape " << shape.shapeCmper
                << " because more than " << MUSICXML_MAX_NUMBER_LEVEL
                << " smart shapes overlap in the current part.", MessageSeverity::Warning);
        }
    }

    return result;
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

int calcEndpointTick(const MusicXmlMusxMapping& context, const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    return context.timing.calcNearestMusicXmlDivisions(endpoint->calcGlobalPosition());
}

int calcTickAfterEntriesStartingAt(const mx::api::StaffData& staff, int tick)
{
    auto adjustedTick = tick;
    for (const auto& [voiceIndex, voice] : staff.voices) {
        static_cast<void>(voiceIndex);
        for (const auto& note : voice.notes) {
            if (note.tickTimePosition == tick) {
                const auto noteEndTick = note.tickTimePosition + (std::max)(0, note.durationData.durationTimeTicks);
                adjustedTick = (std::max)(adjustedTick, noteEndTick);
            }
        }
    }
    return adjustedTick;
}

int calcOttavaStartTick(const MusicXmlMusxMapping& context, const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    const auto tick = calcEndpointTick(context, endpoint);
    if (!MUSICXML_OTTAVA_ENDPOINT_ADJUSTMENT.enabled || !MUSICXML_OTTAVA_ENDPOINT_ADJUSTMENT.startBeforeGraceNotes) {
        return tick;
    }
    return tick;
}

int calcOttavaStopTick(
    const MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint,
    const mx::api::StaffData& staff)
{
    const auto tick = calcEndpointTick(context, endpoint);
    if (!MUSICXML_OTTAVA_ENDPOINT_ADJUSTMENT.enabled || !MUSICXML_OTTAVA_ENDPOINT_ADJUSTMENT.stopAfterEntriesAtEnd) {
        return tick;
    }
    return calcTickAfterEntriesStartingAt(staff, tick);
}

mx::api::DirectionData createSmartShapeDirection(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint,
    StaffCmper staffId,
    size_t staffIndex,
    VerticalPlacement placement,
    std::optional<int> tickTimePosition = std::nullopt)
{
    auto direction = mx::api::DirectionData{};
    direction.tickTimePosition = tickTimePosition.value_or(calcEndpointTick(context, endpoint));
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

bool processSlur(
    MusicXmlMusxMapping& context,
    const MusxInstance<others::SmartShape>& shape,
    const SmartShapeNumberLevels& numberLevels)
{
    if (shape->hidden || !shape->calcIsSlur() || !shape->entryBased) {
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
    if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
        start.numberLevel = *numberLevel;
    }
    start.curveOrientation = enumConvert<mx::api::CurveOrientation>(shape->calcContourDirection());
    if (shape->calcIsDashed()) {
        start.lineData.lineType = mx::api::LineType::dashed;
    }
    startNote->noteAttachmentData.curveStarts.emplace_back(std::move(start));
    auto stop = mx::api::CurveStop{ mx::api::CurveType::slur };
    if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
        stop.numberLevel = *numberLevel;
    }
    endNote->noteAttachmentData.curveStops.emplace_back(std::move(stop));
    return true;
}

void appendHairpin(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    mx::api::WedgeType wedgeType,
    const SmartShapeNumberLevels& numberLevels)
{
    if (shape->hidden) {
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
    if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
        wedgeStart.numberLevel = *numberLevel;
    }
    wedgeStart.wedgeType = wedgeType;
    wedgeStart.lineData.lineType = mx::api::LineType::solid;
    startDirection.wedgeStarts.emplace_back(std::move(wedgeStart));
    staff.directions.emplace_back(std::move(startDirection));

    auto stopDirection = createSmartShapeDirection(context, endPoint, staffId, staffIndex, placement);
    auto wedgeStop = mx::api::WedgeStop{};
    if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
        wedgeStop.numberLevel = *numberLevel;
    }
    stopDirection.wedgeStops.emplace_back(std::move(wedgeStop));
    if (auto* stopStaff = staffDataForEndpoint(context, endPoint)) {
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

void appendOttava(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    const SmartShapeNumberLevels& numberLevels)
{
    if (shape->hidden) {
        return;
    }
    const auto startPoint = shape->startTermSeg->endPoint;
    const auto endPoint = shape->endTermSeg->endPoint;
    if (!startPoint->calcIsAssigned() || !endPoint->calcIsAssigned()) {
        return;
    }
    if (startPoint->staffId != staffId) {
        return;
    }

    const auto placement = shape->calcVerticalPlacementForBeatAttached();
    auto startDirection = createSmartShapeDirection(
        context, startPoint, staffId, staffIndex, placement, calcOttavaStartTick(context, startPoint));
    auto ottavaStart = mx::api::OttavaStart{};
    ottavaStart.ottavaType = enumConvert<mx::api::OttavaType>(shape->shapeType);
    ottavaStart.spannerStart.tickTimePosition = startDirection.tickTimePosition;
    if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
        ottavaStart.spannerStart.numberLevel = *numberLevel;
    }
    startDirection.ottavaStarts.emplace_back(std::move(ottavaStart));
    staff.directions.emplace_back(std::move(startDirection));

    if (auto* stopStaff = staffDataForEndpoint(context, endPoint)) {
        auto stopDirection = createSmartShapeDirection(
            context, endPoint, staffId, staffIndex, placement, calcOttavaStopTick(context, endPoint, *stopStaff));
        auto ottavaStop = mx::api::SpannerStop{};
        ottavaStop.tickTimePosition = stopDirection.tickTimePosition;
        if (const auto numberLevel = findSmartShapeNumberLevel(numberLevels, shape)) {
            ottavaStop.numberLevel = *numberLevel;
        }
        stopDirection.ottavaStops.emplace_back(std::move(ottavaStop));
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

void processSmartShapesForStaff(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex,
    const SmartShapeNumberLevels& numberLevels)
{
    if (!musxMeasure->hasSmartShape) {
        return;
    }

    const auto assigns = context.document->getOthers()->getArray<others::SmartShapeMeasureAssign>(
        musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
    for (const auto& assign : assigns) {
        MUSX_ASSERT_IF(!assign) {
            continue;
        }
        if (assign->centerShapeNum != 0) {
            continue;
        }
        const auto shape = context.document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
        if (!shape || !shape->calcIsValid()) {
            continue;
        }
        if (shape->startTermSeg->endPoint->staffId != staffId
            || shape->startTermSeg->endPoint->measId != musxMeasure->getCmper()) {
            continue;
        }

        if (processSlur(context, shape, numberLevels)) {
            continue;
        }

        using ST = others::SmartShape::ShapeType;
        switch (shape->shapeType) {
        case ST::Crescendo:
            appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::crescendo, numberLevels);
            break;
        case ST::Decrescendo:
            appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::diminuendo, numberLevels);
            break;
        case ST::OctaveDown:
        case ST::OctaveUp:
        case ST::TwoOctaveDown:
        case ST::TwoOctaveUp:
            appendOttava(context, staff, staffId, staffIndex, shape, numberLevels);
            break;
        default:
            break;
        }
    }
}

} // namespace

void processSmartShapes(
    MusicXmlMusxMapping& context,
    const MusxInstanceList<others::Measure>& musxMeasures,
    const std::vector<StaffCmper>& staves)
{
    if (!context.currentPart) {
        return;
    }

    const auto numberLevels = assignSmartShapeNumberLevels(context, musxMeasures, staves);
    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        auto& measure = context.currentPart->measures[measureIndex];
        for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
            auto& staff = measure.staves[staffIndex];
            processSmartShapesForStaff(context, staff, musxMeasures[measureIndex], staves[staffIndex], staffIndex, numberLevels);
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
