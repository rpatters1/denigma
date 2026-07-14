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
#include <cstdlib>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "denigma/classify/smartshapes.h"
#include "musicxml_formatted_text.h"
#include "mx/api/CurveData.h"
#include "mx/api/DirectionData.h"
#include "mx/api/LineData.h"
#include "mx/api/MarkData.h"
#include "mx/api/OttavaData.h"
#include "mx/api/SpannerData.h"
#include "mx/api/SpannerNumber.h"
#include "mx/api/WedgeData.h"
#include "mx/api/WordsData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

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

// Start/continue/stop events sharing an identity are one logical spanner; mx::api
// assigns the MusicXML number attribute from true serialization order at write time.
mx::api::SpannerNumber smartShapeSpannerNumber(const MusxInstance<others::SmartShape>& shape)
{
    return mx::api::SpannerNumber{ std::to_string(shape->getCmper()) };
}

std::optional<MusicXmlNoteLocation> findEntryNoteLocation(
    const MusicXmlMusxMapping& context,
    const EntryInfoPtr& entryInfo,
    NoteNumber noteId)
{
    if (!entryInfo) {
        return std::nullopt;
    }

    const auto entryNumber = entryInfo->getEntry()->getEntryNumber();
    if (noteId != 0) {
        const auto noteIt = context.noteLocations.find(musicXmlNoteKey(entryNumber, noteId));
        if (noteIt != context.noteLocations.end()) {
            return noteIt->second;
        }
    }

    const auto entryIt = context.entryNumberToFirstNote.find(entryNumber);
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

struct MusicXmlEndpointStaffLocation
{
    size_t measureIndex{};
    size_t staffIndex{};
};

std::optional<MusicXmlEndpointStaffLocation> staffLocationForEndpoint(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    if (!context.currentPart || !endpoint) {
        return std::nullopt;
    }

    const auto measureNumber = std::to_string(endpoint->measId);
    const auto& measures = context.currentPart->measures;
    const auto measureIt = std::ranges::find_if(measures, [&measureNumber](const mx::api::MeasureData& measure) {
        return measure.number == measureNumber;
    });
    if (measureIt == measures.end()) {
        return std::nullopt;
    }

    const auto stavesIt = context.partIdToStaves.find(context.currentPart->uniqueId);
    if (stavesIt == context.partIdToStaves.end()) {
        return std::nullopt;
    }
    const auto staffIt = std::ranges::find(stavesIt->second, endpoint->staffId);
    if (staffIt == stavesIt->second.end()) {
        return std::nullopt;
    }

    const auto measureIndex = static_cast<size_t>(std::distance(measures.begin(), measureIt));
    const auto staffIndex = static_cast<size_t>(std::distance(stavesIt->second.begin(), staffIt));
    if (staffIndex >= measureIt->staves.size()) {
        return std::nullopt;
    }
    return MusicXmlEndpointStaffLocation{ measureIndex, staffIndex };
}

mx::api::StaffData* staffDataForEndpoint(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    if (!endpoint || !endpoint->calcIsAssigned()) {
        return nullptr;
    }
    const auto location = staffLocationForEndpoint(context, endpoint);
    if (!location) {
        return nullptr;
    }
    return &context.currentPart->measures[location->measureIndex].staves[location->staffIndex];
}

// Hosts a floating curve endpoint. MusicXML curve notations can only hang off a
// <note>, so an endpoint with no coinciding entry anchors to a hidden rest at the
// endpoint's tick in a reserved voice. The tick, not graphical offsets, carries the
// horizontal position: readers re-lay out curves from their anchors and ignore
// exported offsets. Idempotent: concurrent floating endpoints at one tick share a
// single anchor rest, and re-resolving an endpoint returns its current location.
std::optional<MusicXmlNoteLocation> createFloatingCurveAnchor(
    MusicXmlMusxMapping& context,
    const std::shared_ptr<smartshape::EndPoint>& endpoint)
{
    const auto staffLocation = staffLocationForEndpoint(context, endpoint);
    if (!staffLocation) {
        return std::nullopt;
    }
    const auto musxMeasure = context.document->getOthers()->get<others::Measure>(context.forPartId, endpoint->measId);
    if (!musxMeasure) {
        return std::nullopt;
    }
    const auto measureTicks = context.timing.calcMusicXmlDivisions(musxMeasure->calcDuration());
    auto tick = std::clamp(calcEndpointTick(context, endpoint), 0, (std::max)(0, measureTicks - 1));

    const int userVoiceNumber = musicXmlFloatingAnchorVoiceNumber(staffLocation->staffIndex);
    auto& staff = context.currentPart->measures[staffLocation->measureIndex].staves[staffLocation->staffIndex];

    if (calcEndpointTick(context, endpoint) >= measureTicks) {
        // An endpoint at the measure end anchors at the start of the last beat or at
        // the last entry of the staff, whichever comes later. A rest one tick before
        // the barline renders poorly: readers give the phantom sub-beat segment
        // minimal width, cramming the curve against the barline.
        /// @todo Search the whole measure stack (every staff, not just the endpoint's)
        /// for the last entry tick, so the anchor aligns with the latest sounding event
        /// under the endpoint. Probably belongs in a musxdom helper.
        const auto measureDurationEdu = musxMeasure->calcDuration().calcEduDuration();
        const auto beatValue = musxMeasure->createTimeSignature(endpoint->staffId)
            ->calcBeatValueAt((std::max)(0, measureDurationEdu - 1));
        const auto beatTicks = context.timing.calcNearestMusicXmlDivisions(beatValue);
        int anchorTick = (std::max)(0, measureTicks - beatTicks);
        for (const auto& [voiceIndex, staffVoice] : staff.voices) {
            if (voiceIndex == userVoiceNumber - 1) {
                continue;
            }
            for (const auto& note : staffVoice.notes) {
                if (note.tickTimePosition > anchorTick && note.tickTimePosition < measureTicks) {
                    anchorTick = note.tickTimePosition;
                }
            }
        }
        tick = anchorTick;
    }

    auto& voice = staff.voices[userVoiceNumber - 1];

    auto insertIt = voice.notes.begin();
    while (insertIt != voice.notes.end() && insertIt->tickTimePosition < tick) {
        ++insertIt;
    }
    const auto noteIndex = static_cast<size_t>(std::distance(voice.notes.begin(), insertIt));
    const auto location = MusicXmlNoteLocation{
        .measureIndex = staffLocation->measureIndex,
        .staffIndex = staffLocation->staffIndex,
        .userVoiceNumber = userVoiceNumber,
        .noteIndex = noteIndex
    };
    if (insertIt != voice.notes.end() && insertIt->tickTimePosition == tick) {
        return location;
    }

    // Anchors must carry a coherent type/duration pair: readers reconcile <type>
    // against <duration> and may re-place a rest whose two disagree. Use the largest
    // subdivision of a quarter that the divisions grid expresses exactly and that
    // fits the available gap.
    const auto anchorDuration = [&context](int maxTicks) -> std::pair<int, mx::api::DurationName> {
        constexpr std::array<std::pair<int, mx::api::DurationName>, 6> subdivisions{ {
            { 1, mx::api::DurationName::quarter },
            { 2, mx::api::DurationName::eighth },
            { 4, mx::api::DurationName::dur16th },
            { 8, mx::api::DurationName::dur32nd },
            { 16, mx::api::DurationName::dur64th },
            { 32, mx::api::DurationName::dur128th },
        } };
        for (const auto& [divisor, name] : subdivisions) {
            if (context.timing.divisions % divisor != 0) {
                continue;
            }
            const int ticks = context.timing.divisions / divisor;
            if (ticks <= maxTicks) {
                return { ticks, name };
            }
        }
        return { (std::max)(1, maxTicks), mx::api::DurationName::dur128th };
    };

    const auto nextTick = insertIt != voice.notes.end() ? insertIt->tickTimePosition : measureTicks;
    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.userRequestedVoiceNumber = userVoiceNumber;
    rest.tickTimePosition = tick;
    std::tie(rest.durationData.durationTimeTicks, rest.durationData.durationName) = anchorDuration(nextTick - tick);
    rest.printData.printObject = mx::api::Bool::no;
    if (insertIt != voice.notes.begin()) {
        // Keep the anchor voice non-overlapping: truncate the previous anchor if it
        // reaches past the new one.
        auto& previous = *std::prev(insertIt);
        if (previous.tickTimePosition + previous.durationData.durationTimeTicks > tick) {
            std::tie(previous.durationData.durationTimeTicks, previous.durationData.durationName) =
                anchorDuration(tick - previous.tickTimePosition);
        }
    }
    voice.notes.insert(insertIt, std::move(rest));
    return location;
}

bool processSlur(
    MusicXmlMusxMapping& context,
    const MusxInstance<others::SmartShape>& shape,
    const classify::SmartShapeClassification& classification)
{
    const auto* slur = classification.as<classify::smartshape::Slur>();
    if (!slur) {
        return false;
    }

    auto startLocation = findEntryNoteLocation(context, slur->startEntry, shape->startNoteId);
    auto endLocation = findEntryNoteLocation(context, slur->endEntry, shape->endNoteId);

    // Endpoints with no coinciding entry anchor to synthesized hidden rests. The end
    // anchor is created after the start anchor, so re-resolve the start afterward:
    // when both share the anchor voice, the second insertion can shift the first
    // anchor's note index.
    const bool startIsFloating = !startLocation;
    const bool endIsFloating = !endLocation;
    if (startIsFloating) {
        startLocation = createFloatingCurveAnchor(context, shape->startTermSeg->endPoint);
    }
    if (endIsFloating) {
        endLocation = createFloatingCurveAnchor(context, shape->endTermSeg->endPoint);
        if (startIsFloating && startLocation) {
            startLocation = createFloatingCurveAnchor(context, shape->startTermSeg->endPoint);
        }
    }
    if (!startLocation || !endLocation) {
        context.logMessage(LogMsg() << "Omitting slur smart shape " << shape->getCmper()
            << " because an endpoint could not be attached or anchored.", MessageSeverity::Verbose);
        return false;
    }

    auto* startNote = noteDataAt(context, *startLocation);
    auto* endNote = noteDataAt(context, *endLocation);
    if (!startNote || !endNote) {
        return false;
    }

    // A floating endpoint's vertical position is layout-independent (EVPU relative to
    // the top staff line, as is MusicXML default-y), so it survives the anchor hop.
    const auto applyFloatingOffset = [&context](mx::api::CurvePoints& curvePoints, const std::shared_ptr<others::SmartShape::TerminationSeg>& termSeg) {
        curvePoints.positionData.isDefaultYSpecified = true;
        curvePoints.positionData.defaultY = context.musicXmlTenthsFromEvpu(termSeg->endPointAdj->calcVertOffset());
    };

    auto start = mx::api::CurveStart{ mx::api::CurveType::slur };
    start.number = smartShapeSpannerNumber(shape);
    start.curveOrientation = enumConvert<mx::api::CurveOrientation>(slur->contour);
    if (shape->calcIsDashed()) {
        start.lineData.lineType = mx::api::LineType::dashed;
    }
    if (startIsFloating) {
        applyFloatingOffset(start.curvePoints, shape->startTermSeg);
    }
    startNote->noteAttachmentData.curveStarts.emplace_back(std::move(start));
    auto stop = mx::api::CurveStop{ mx::api::CurveType::slur };
    stop.number = smartShapeSpannerNumber(shape);
    if (endIsFloating) {
        applyFloatingOffset(stop.curvePoints, shape->endTermSeg);
    }
    endNote->noteAttachmentData.curveStops.emplace_back(std::move(stop));
    return true;
}

void processArpeggiatedTie(
    MusicXmlMusxMapping& context,
    const classify::smartshape::ArpeggiatedTie& arpeggiatedTie)
{
    ASSERT_IF(!arpeggiatedTie.tiedFrom || !arpeggiatedTie.tiedTo) {
        return;
    }

    const auto startLocation = findEntryNoteLocation(
        context, arpeggiatedTie.tiedFrom.getEntryInfo(), arpeggiatedTie.tiedFrom->getNoteId());
    const auto endLocation = findEntryNoteLocation(
        context, arpeggiatedTie.tiedTo.getEntryInfo(), arpeggiatedTie.tiedTo->getNoteId());
    if (!startLocation || !endLocation) {
        return;
    }

    auto* startNote = noteDataAt(context, *startLocation);
    auto* endNote = noteDataAt(context, *endLocation);
    if (!startNote || !endNote) {
        return;
    }

    startNote->isTieStart = true;
    endNote->isTieStop = true;
}

void appendHairpin(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    mx::api::WedgeType wedgeType)
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
    wedgeStart.number = smartShapeSpannerNumber(shape);
    wedgeStart.wedgeType = wedgeType;
    wedgeStart.lineData.lineType = mx::api::LineType::solid;
    startDirection.wedgeStarts.emplace_back(std::move(wedgeStart));
    staff.directions.emplace_back(std::move(startDirection));

    auto stopDirection = createSmartShapeDirection(context, endPoint, staffId, staffIndex, placement);
    auto wedgeStop = mx::api::WedgeStop{};
    wedgeStop.number = smartShapeSpannerNumber(shape);
    stopDirection.wedgeStops.emplace_back(std::move(wedgeStop));
    if (auto* stopStaff = staffDataForEndpoint(context, endPoint)) {
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

std::optional<mx::api::OttavaType> ottavaTypeFromOctaveShift(int octaveShift)
{
    switch (octaveShift) {
    case 1: return mx::api::OttavaType::o8va;
    case -1: return mx::api::OttavaType::o8vb;
    case 2: return mx::api::OttavaType::o15ma;
    case -2: return mx::api::OttavaType::o15mb;
    default: return std::nullopt; // mx::api has no 22ma/22mb OttavaType. (See mx-api-gaps.md.)
    }
}

void appendOttava(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    const classify::smartshape::Ottava& ottava)
{
    if (!ottava.calcIsSemanticCarrier()) {
        // A paired visual custom line: its hidden counterpart carries the octave shift.
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
    const auto ottavaType = ottavaTypeFromOctaveShift(ottava.octaveShift);
    if (!ottavaType) {
        context.logMessage(LogMsg() << "Skipping ottava smart shape " << shape->getCmper()
            << " with octave shift " << ottava.octaveShift
            << " because mx::api cannot express octave shifts beyond two octaves.", MessageSeverity::Warning);
        return;
    }

    // A hidden carrier has no meaningful endpoint geometry of its own; place it by
    // its direction, which is how its (possibly text-expression) proxy reads.
    const auto placement = shape->hidden
        ? (ottava.octaveShift > 0 ? VerticalPlacement::Above : VerticalPlacement::Below)
        : shape->calcVerticalPlacementForBeatAttached();
    auto startDirection = createSmartShapeDirection(
        context, startPoint, staffId, staffIndex, placement, calcOttavaStartTick(context, startPoint));
    auto ottavaStart = mx::api::OttavaStart{};
    ottavaStart.ottavaType = *ottavaType;
    ottavaStart.spannerStart.tickTimePosition = startDirection.tickTimePosition;
    ottavaStart.spannerStart.number = smartShapeSpannerNumber(shape);
    startDirection.ottavaStarts.emplace_back(std::move(ottavaStart));
    staff.directions.emplace_back(std::move(startDirection));

    if (auto* stopStaff = staffDataForEndpoint(context, endPoint)) {
        auto stopDirection = createSmartShapeDirection(
            context, endPoint, staffId, staffIndex, placement, calcOttavaStopTick(context, endPoint, *stopStaff));
        auto ottavaStop = mx::api::OttavaStop{};
        ottavaStop.spannerStop.tickTimePosition = stopDirection.tickTimePosition;
        ottavaStop.spannerStop.number = smartShapeSpannerNumber(shape);
        constexpr int kOttavaSize = 8;
        constexpr int k15maSize = 15;
        ottavaStop.size = std::abs(ottava.octaveShift) == 1 ? kOttavaSize : k15maSize;
        stopDirection.ottavaStops.emplace_back(std::move(ottavaStop));
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

bool hasUnsupportedPedalIdentity(const classify::smartshape::KeyboardPedal& pedal)
{
    const auto isUnsupported = [](const std::optional<classify::KeyboardPedalClassification>& marking) {
        if (!marking) {
            return false;
        }
        using Type = classify::keyboardpedal::Type;
        return marking->type == Type::PedalTwo || marking->type == Type::PedalThree;
    };
    return isUnsupported(pedal.startText) || isUnsupported(pedal.continuationText) || isUnsupported(pedal.endText);
}

mx::api::LineType pedalLineType(const classify::smartshape::KeyboardPedal& pedal)
{
    using LineStyle = others::SmartShapeCustomLine::LineStyle;
    switch (pedal.line.lineStyle) {
    case LineStyle::Solid: return mx::api::LineType::solid;
    case LineStyle::Dashed: return mx::api::LineType::dashed;
    case LineStyle::Char: return mx::api::LineType::wavy;
    }
    return mx::api::LineType::unspecified;
}

void appendKeyboardPedal(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    const classify::smartshape::KeyboardPedal& pedal)
{
    const auto startPoint = shape->startTermSeg->endPoint;
    const auto endPoint = shape->endTermSeg->endPoint;
    if (!startPoint->calcIsAssigned() || !endPoint->calcIsAssigned() || startPoint->staffId != staffId) {
        return;
    }
    if (hasUnsupportedPedalIdentity(pedal)) {
        /// @todo Export sostenuto and una-corda pedal spanners when mx::api exposes the
        /// corresponding MusicXML pedal event types. Do not misrepresent them as damper pedal.
        return;
    }

    const auto placement = shape->calcVerticalPlacementForBeatAttached();
    auto startDirection = createSmartShapeDirection(context, startPoint, staffId, staffIndex, placement);
    auto* stopStaff = staffDataForEndpoint(context, endPoint);
    if (!stopStaff) {
        return;
    }
    auto stopDirection = createSmartShapeDirection(context, endPoint, endPoint->staffId, staffIndex, placement);

    if (!pedal.line.lineVisible) {
        // mx::api's pedal marks are the only way to request sign="yes" with line="no".
        startDirection.marks.emplace_back(enumConvert<mx::api::Placement>(placement), mx::api::MarkType::pedal);
        stopDirection.marks.emplace_back(enumConvert<mx::api::Placement>(placement), mx::api::MarkType::damp);
    } else {
        auto start = mx::api::SpannerStart{};
        auto stop = mx::api::SpannerStop{};
        start.tickTimePosition = startDirection.tickTimePosition;
        stop.tickTimePosition = stopDirection.tickTimePosition;
        /// @todo Assign identity spanner numbers to pedals once mx::api serializes them.
        /// MusicXML 3.1+ gives <pedal> a number attribute and core::Pedal supports it, but
        /// mx::api's pedal writer and SpannerNumberResolver ignore it. (See mx-api-gaps.md.)
        start.lineData.lineType = pedalLineType(pedal);
        stop.lineData.lineType = pedalLineType(pedal);
        /// @todo Preserve Finale pedal text/sign choices, ordinary hooks, custom cap geometry,
        /// dashed/character lines, and pump changes when mx::api exposes them. Its current pedal
        /// writer forces line="yes" sign="yes", ignores LineData, and supports only start/stop.
        startDirection.pedalStarts.emplace_back(std::move(start));
        stopDirection.pedalStops.emplace_back(std::move(stop));
    }

    staff.directions.emplace_back(std::move(startDirection));
    stopStaff->directions.emplace_back(std::move(stopDirection));
}

double tenthsFromEfix(Efix value)
{
    return (static_cast<double>(value) / EFIX_PER_EVPU)
        * (MUSICXML_DEFAULT_TENTHS_PER_STAFF / EVPU_PER_STANDARD_STAFF);
}

mx::api::LineHook lineHookFromCap(const classify::smartshape::LineCap& cap)
{
    using CapType = classify::smartshape::LineCap::Type;
    switch (cap.type) {
    case CapType::Hook:
        if (cap.hookLength == 0) {
            return mx::api::LineHook::none;
        }
        return cap.hookLength > 0 ? mx::api::LineHook::up : mx::api::LineHook::down;
    case CapType::ArrowheadPreset:
    case CapType::ArrowheadCustom:
        // Arrowhead geometry is unrepresentable in MusicXML; line-end="arrow" is the
        // closest expressible form.
        return mx::api::LineHook::arrow;
    case CapType::None:
        break;
    }
    return mx::api::LineHook::none;
}

mx::api::LineType lineTypeFromGeneralLine(const classify::smartshape::GeneralLine& line)
{
    using LineStyle = others::SmartShapeCustomLine::LineStyle;
    switch (line.lineStyle) {
    case LineStyle::Solid: return mx::api::LineType::solid;
    case LineStyle::Dashed: return mx::api::LineType::dashed;
    case LineStyle::Char: return mx::api::LineType::wavy;
    }
    return mx::api::LineType::unspecified;
}

void applyGeneralLineDashes(const classify::smartshape::GeneralLine& line, mx::api::LineData& lineData)
{
    if (line.lineStyle != others::SmartShapeCustomLine::LineStyle::Dashed) {
        return;
    }
    if (line.dashOn != 0) {
        lineData.isDashLengthSpecified = true;
        lineData.dashLength = tenthsFromEfix(line.dashOn);
    }
    if (line.dashOff != 0) {
        lineData.isSpaceLengthSpecified = true;
        lineData.spaceLength = tenthsFromEfix(line.dashOff);
    }
}

void appendGeneralLine(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    size_t staffIndex,
    const MusxInstance<others::SmartShape>& shape,
    const classify::smartshape::GeneralLine& line)
{
    const auto startPoint = shape->startTermSeg->endPoint;
    const auto endPoint = shape->endTermSeg->endPoint;
    if (!startPoint->calcIsAssigned() || !endPoint->calcIsAssigned() || startPoint->staffId != staffId) {
        return;
    }
    auto* stopStaff = staffDataForEndpoint(context, endPoint);
    if (!stopStaff) {
        return;
    }

    if (line.continuationText) {
        context.logMessage(LogMsg() << "Omitting continuation text of custom line smart shape "
            << shape->getCmper() << ": MusicXML has no continuation-text concept.", MessageSeverity::Verbose);
    }
    if (line.centerFullText || line.centerAbbrText) {
        context.logMessage(LogMsg() << "Omitting center text of custom line smart shape "
            << shape->getCmper() << ": MusicXML has no equivalent outside of glissando text.",
            MessageSeverity::Verbose);
    }

    const auto placement = shape->calcVerticalPlacementForBeatAttached();
    auto startDirection = createSmartShapeDirection(context, startPoint, staffId, staffIndex, placement);
    auto stopDirection = createSmartShapeDirection(context, endPoint, endPoint->staffId, staffIndex, placement);
    startDirection.words = musicXmlWordsFromEnigmaText(context, line.startText);
    stopDirection.words = musicXmlWordsFromEnigmaText(context, line.endText);

    if (line.lineVisible) {
        const bool hasCaps = line.startCap.type != classify::smartshape::LineCap::Type::None
            || line.endCap.type != classify::smartshape::LineCap::Type::None;
        auto start = mx::api::SpannerStart{};
        auto stop = mx::api::SpannerStop{};
        start.tickTimePosition = startDirection.tickTimePosition;
        stop.tickTimePosition = stopDirection.tickTimePosition;
        start.number = smartShapeSpannerNumber(shape);
        stop.number = smartShapeSpannerNumber(shape);
        applyGeneralLineDashes(line, start.lineData);
        applyGeneralLineDashes(line, stop.lineData);
        if (!hasCaps && !startDirection.words.empty()) {
            // The MusicXML idiom for a hookless text line is words followed by dashes.
            startDirection.dashesStarts.emplace_back(std::move(start));
            stopDirection.dashesStops.emplace_back(std::move(stop));
        } else {
            start.lineData.lineType = lineTypeFromGeneralLine(line);
            stop.lineData.lineType = start.lineData.lineType;
            start.lineData.lineHook = lineHookFromCap(line.startCap);
            stop.lineData.lineHook = lineHookFromCap(line.endCap);
            if (start.lineData.lineHook == mx::api::LineHook::up || start.lineData.lineHook == mx::api::LineHook::down) {
                start.lineData.isStopLengthSpecified = true;
                start.lineData.endLength = tenthsFromEfix(std::abs(line.startCap.hookLength));
            }
            if (stop.lineData.lineHook == mx::api::LineHook::up || stop.lineData.lineHook == mx::api::LineHook::down) {
                stop.lineData.isStopLengthSpecified = true;
                stop.lineData.endLength = tenthsFromEfix(std::abs(line.endCap.hookLength));
            }
            startDirection.bracketStarts.emplace_back(std::move(start));
            stopDirection.bracketStops.emplace_back(std::move(stop));
        }
    }

    const bool startHasContent = !startDirection.words.empty()
        || !startDirection.bracketStarts.empty() || !startDirection.dashesStarts.empty();
    const bool stopHasContent = !stopDirection.words.empty()
        || !stopDirection.bracketStops.empty() || !stopDirection.dashesStops.empty();
    if (!startHasContent && !stopHasContent) {
        context.logMessage(LogMsg() << "Omitting custom line smart shape " << shape->getCmper()
            << " with no expressible MusicXML content.", MessageSeverity::Verbose);
        return;
    }
    if (startHasContent) {
        staff.directions.emplace_back(std::move(startDirection));
    }
    if (stopHasContent) {
        stopStaff->directions.emplace_back(std::move(stopDirection));
    }
}

void appendTrillLine(
    MusicXmlMusxMapping& context,
    const MusxInstance<others::SmartShape>& shape,
    const classify::smartshape::TrillLine& trillLine)
{
    /// @todo Emit the wavy-line extension when mx::api can pair wavy-line start/stop.
    /// (See mx-api-gaps.md.)
    if (!trillLine.includesTrSymbol) {
        context.logMessage(LogMsg() << "Omitting trill extension smart shape " << shape->getCmper()
            << " because mx::api cannot pair wavy-line start/stop.", MessageSeverity::Verbose);
        return;
    }
    const auto startEntry = shape->startTermSeg->endPoint->calcAssociatedEntry();
    const auto location = findEntryNoteLocation(context, startEntry, shape->startNoteId);
    auto* note = location ? noteDataAt(context, *location) : nullptr;
    if (!note) {
        context.logMessage(LogMsg() << "Omitting trill smart shape " << shape->getCmper()
            << " because no note is associated with its start point.", MessageSeverity::Verbose);
        return;
    }
    auto mark = musicXmlMark(mx::api::MarkType::trillMark, shape->calcVerticalPlacementForBeatAttached());
    mark.tickTimePosition = note->tickTimePosition;
    note->noteAttachmentData.marks.emplace_back(std::move(mark));
}

void processSmartShapesForStaff(
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
            continue;
        }
        if (assign->centerShapeNum != 0) {
            continue;
        }
        const auto shape = context.document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
        ASSERT_IF(!shape) {
            continue;
        }
        if (shape->startTermSeg->endPoint->staffId != staffId
            || shape->startTermSeg->endPoint->measId != musxMeasure->getCmper()) {
            continue;
        }

        const auto classification = classify::classifySmartShape(shape);
        if (shape->hidden && !std::holds_alternative<classify::smartshape::Ottava>(classification.value)) {
            // Hidden ottavas can be semantic carriers for visible custom lines and
            // must be processed; all other hidden shapes are skipped.
            continue;
        }
        if (processSlur(context, shape, classification)) {
            continue;
        }

        std::visit([&](const auto& value) {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, classify::smartshape::Crescendo>) {
                appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::crescendo);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::Decrescendo>) {
                appendHairpin(context, staff, staffId, staffIndex, shape, mx::api::WedgeType::diminuendo);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::Ottava>) {
                appendOttava(context, staff, staffId, staffIndex, shape, value);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::KeyboardPedal>) {
                appendKeyboardPedal(context, staff, staffId, staffIndex, shape, value);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::TrillLine>) {
                appendTrillLine(context, shape, value);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::VibratoLine>) {
                /// @todo Emit vibrato lines as paired wavy-line ornaments when mx::api
                /// supports them. (See mx-api-gaps.md.)
                context.logMessage(LogMsg() << "Omitting vibrato line smart shape " << shape->getCmper()
                    << " because mx::api cannot pair wavy-line start/stop.", MessageSeverity::Verbose);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::GeneralLine>) {
                appendGeneralLine(context, staff, staffId, staffIndex, shape, value);
            } else if constexpr (std::is_same_v<Value, classify::smartshape::ArpeggiatedTie>) {
                processArpeggiatedTie(context, value);
            } else if constexpr (std::is_same_v<Value, classify::PseudoTie>) {
                if (value.type == classify::PseudoTie::Type::LaissezVibrer) {
                    applyPseudoLvTies(context, shape->startTermSeg->endPoint->calcAssociatedEntry());
                }
            } else if constexpr (std::is_same_v<Value, classify::smartshape::NonArpeggio>) {
                appendArpeggioCandidate(context, value.candidate);
            }
        }, classification.value);
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

    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        auto& measure = context.currentPart->measures[measureIndex];
        for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
            auto& staff = measure.staves[staffIndex];
            processSmartShapesForStaff(context, staff, musxMeasures[measureIndex], staves[staffIndex], staffIndex);
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
