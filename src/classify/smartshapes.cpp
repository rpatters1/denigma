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

#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "classify.h"
#include "denigma/classify/octaves.h"
#include "utils/utf8_iterator.h"

namespace denigma {
namespace classify {

using namespace smartshape;

namespace {

KeyboardPedal::CapType pedalCapSemantic(const LineCap& cap)
{
    using KnownType = musx::dom::KnownShapeDefType;
    switch (cap.type) {
    case LineCap::Type::Hook:
        return KeyboardPedal::CapType::Hook;
    case LineCap::Type::ArrowheadCustom:
        switch (cap.customArrowheadType) {
        case KnownType::PedalArrowheadDown:
            return KeyboardPedal::CapType::PedalDown;
        case KnownType::PedalArrowheadUp:
            return KeyboardPedal::CapType::PedalUp;
        case KnownType::PedalArrowheadShortUpDownLongUp:
        case KnownType::PedalArrowheadLongUpDownShortUp:
            return KeyboardPedal::CapType::PedalChange;
        default:
            return KeyboardPedal::CapType::None;
        }
    default:
        return KeyboardPedal::CapType::None;
    }
}

bool isPedalCap(KeyboardPedal::CapType capType)
{
    return capType == KeyboardPedal::CapType::PedalDown
        || capType == KeyboardPedal::CapType::PedalUp
        || capType == KeyboardPedal::CapType::PedalChange;
}

std::optional<int> builtInOttavaShift(musx::dom::others::SmartShape::ShapeType shapeType)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    switch (shapeType) {
    case ShapeType::OctaveDown:     return -1;
    case ShapeType::OctaveUp:       return 1;
    case ShapeType::TwoOctaveDown:  return -2;
    case ShapeType::TwoOctaveUp:    return 2;
    default:                        return std::nullopt;
    }
}

bool musicRangesIntersect(const musx::dom::MusicRange& lhs, const musx::dom::MusicRange& rhs)
{
    return lhs.start <= rhs.end && rhs.start <= lhs.end;
}

/// Returns true when every entry governed by @p shape is covered by at least one of
/// @p candidates. @p sawEntry reports whether any entry was iterable at all.
bool entriesCoveredByCandidates(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    const std::vector<musx::dom::MusxInstance<musx::dom::others::SmartShape>>& candidates,
    bool& sawEntry)
{
    sawEntry = false;
    const auto staffList = shape->getDocument()->getScrollViewStaves(shape->getRequestedPartId());
    if (!staffList.getIndexForStaff(shape->startTermSeg->endPoint->staffId)
        || !staffList.getIndexForStaff(shape->endTermSeg->endPoint->staffId)) {
        // Entries cannot be iterated (e.g., the staff is not in the scroll view);
        // callers fall back to range intersection.
        return true;
    }
    bool allCovered = true;
    shape->iterateEntries([&](const musx::dom::EntryInfoPtr& entry) {
        sawEntry = true;
        for (const auto& candidate : candidates) {
            if (candidate->calcAppliesTo(entry)) {
                return true;
            }
        }
        allCovered = false;
        return false;
    });
    return allCovered;
}

/// Finds the hidden built-in ottava (if any) that carries the semantics of the visual
/// line @p shape. Pairing succeeds only when suppressing the visual line cannot change
/// any note's octave displacement: every entry it governs must already be governed by
/// a candidate. When the line governs no entries at all, plain range intersection
/// suffices, since the pairing cannot affect pitch either way.
musx::dom::MusxInstance<musx::dom::others::SmartShape> findHiddenOttavaCounterpart(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    const std::vector<musx::dom::MusxInstance<musx::dom::others::SmartShape>>& candidates)
{
    if (candidates.empty()) {
        return nullptr;
    }
    bool sawEntry = false;
    if (!entriesCoveredByCandidates(shape, candidates, sawEntry)) {
        return nullptr;
    }
    const auto range = shape->createGlobalMusicRange();
    musx::dom::MusxInstance<musx::dom::others::SmartShape> intersecting;
    for (const auto& candidate : candidates) {
        const auto candidateRange = candidate->createGlobalMusicRange();
        if (candidateRange.contains(range.start)) {
            return candidate;
        }
        if (!intersecting && musicRangesIntersect(range, candidateRange)) {
            intersecting = candidate;
        }
    }
    if (intersecting) {
        return intersecting;
    }
    return sawEntry ? candidates.front() : nullptr;
}

/// Collects the hidden built-in ottavas that could carry the semantics of a visual
/// line on @p staffId with the given octave shift.
std::vector<musx::dom::MusxInstance<musx::dom::others::SmartShape>> collectHiddenOttavas(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    musx::dom::StaffCmper staffId,
    int octaveShift)
{
    std::vector<musx::dom::MusxInstance<musx::dom::others::SmartShape>> result;
    const auto allShapes = shape->getDocument()->getOthers()->getArray<musx::dom::others::SmartShape>(
        shape->getRequestedPartId());
    for (const auto& candidate : allShapes) {
        if (!candidate->hidden || candidate->getCmper() == shape->getCmper()) {
            continue;
        }
        const auto shift = builtInOttavaShift(candidate->shapeType);
        if (!shift || *shift != octaveShift) {
            continue;
        }
        if (candidate->startTermSeg->endPoint->staffId != staffId
            || candidate->endTermSeg->endPoint->staffId != staffId) {
            continue;
        }
        if (!candidate->calcIsValid()) {
            continue;
        }
        result.push_back(candidate);
    }
    return result;
}

/// Classifies a custom line as a visual ottava. Direction-ambiguous markings are
/// resolved by pairing with a hidden built-in ottava; when they cannot be resolved,
/// the line is not classified as an ottava at all.
std::optional<Ottava> classifyOttavaLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    const GeneralLine& line)
{
    using Direction = octave::Direction;

    auto marking = classifyOctaveMarking(line.startText);
    if (!marking) {
        return std::nullopt;
    }
    if (line.continuationText) {
        const auto contMarking = classifyOctaveMarking(line.continuationText);
        if (!contMarking || contMarking->magnitude != marking->magnitude) {
            return std::nullopt;
        }
        if (marking->direction == Direction::Unknown) {
            marking->direction = contMarking->direction;
        } else if (contMarking->direction != Direction::Unknown
            && contMarking->direction != marking->direction) {
            return std::nullopt;
        }
    }

    auto direction = marking->direction;
    // "8va"-style markings state alta, but engravers sometimes use them below the
    // staff to mean bassa. Demote to Unknown there and let the pairing decide.
    if (direction == Direction::Up
        && shape->calcVerticalPlacementForBeatAttached() == musx::dom::VerticalPlacement::Below) {
        direction = Direction::Unknown;
    }

    const auto startStaffId = shape->startTermSeg->endPoint->staffId;
    const bool singleStaff = startStaffId == shape->endTermSeg->endPoint->staffId;

    musx::dom::MusxInstance<musx::dom::others::SmartShape> counterpartUp;
    musx::dom::MusxInstance<musx::dom::others::SmartShape> counterpartDown;
    if (singleStaff) {
        if (direction != Direction::Down) {
            counterpartUp = findHiddenOttavaCounterpart(
                shape, collectHiddenOttavas(shape, startStaffId, marking->magnitude));
        }
        if (direction != Direction::Up) {
            counterpartDown = findHiddenOttavaCounterpart(
                shape, collectHiddenOttavas(shape, startStaffId, -marking->magnitude));
        }
    }

    if (direction == Direction::Unknown) {
        if (static_cast<bool>(counterpartUp) == static_cast<bool>(counterpartDown)) {
            // Unresolvable (no counterpart) or ambiguous (both directions cover).
            return std::nullopt;
        }
        direction = counterpartUp ? Direction::Up : Direction::Down;
    }

    Ottava result;
    result.octaveShift = direction == Direction::Up ? marking->magnitude : -marking->magnitude;
    result.hiddenCounterpart = direction == Direction::Up ? counterpartUp : counterpartDown;
    result.line = line;
    return result;
}

/// Returns true when a hidden built-in ottava is rendered by a visible ottava custom
/// line overlapping its range on the same staff.
bool calcHasVisualOttavaProxy(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    int octaveShift)
{
    using Direction = octave::Direction;
    const auto staffId = shape->startTermSeg->endPoint->staffId;
    const auto range = shape->createGlobalMusicRange();
    const auto allShapes = shape->getDocument()->getOthers()->getArray<musx::dom::others::SmartShape>(
        shape->getRequestedPartId());
    for (const auto& candidate : allShapes) {
        if (candidate->hidden || candidate->entryBased) {
            continue;
        }
        if (candidate->shapeType != musx::dom::others::SmartShape::ShapeType::CustomLine
            || candidate->lineStyleId == 0) {
            continue;
        }
        if (candidate->startTermSeg->endPoint->staffId != staffId || !candidate->calcIsValid()) {
            continue;
        }
        const auto customLine = candidate->getDocument()->getOthers()->get<musx::dom::others::SmartShapeCustomLine>(
            candidate->getRequestedPartId(), candidate->lineStyleId);
        if (!customLine) {
            continue;
        }
        const auto marking = classifyOctaveMarking(
            customLine->getLeftStartRawTextCtx(candidate->getRequestedPartId()));
        if (!marking || marking->magnitude != std::abs(octaveShift)) {
            continue;
        }
        if ((marking->direction == Direction::Up && octaveShift < 0)
            || (marking->direction == Direction::Down && octaveShift > 0)) {
            continue;
        }
        if (musicRangesIntersect(range, candidate->createGlobalMusicRange())) {
            return true;
        }
    }
    return false;
}

Ottava makeBuiltInOttava(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    int octaveShift)
{
    Ottava result;
    result.octaveShift = octaveShift;
    if (shape->hidden) {
        result.hasVisualProxy = calcHasVisualOttavaProxy(shape, octaveShift);
    }
    return result;
}

bool isTrillWiggleGlyph(std::string_view glyphName)
{
    return glyphName.rfind("wiggleTrill", 0) == 0
        || glyphName == "ornamentZigZagLineNoRightEnd"
        || glyphName == "ornamentZigZagLineWithRightEnd";
}

bool isVibratoWiggleGlyph(std::string_view glyphName)
{
    return glyphName.rfind("wiggleVibrato", 0) == 0
        || glyphName.rfind("wiggleSawtooth", 0) == 0
        || glyphName == "guitarVibratoStroke"
        || glyphName == "guitarWideVibratoStroke";
}

/// Returns true when the text consists of exactly one trill symbol.
bool isBareTrillSymbolText(const musx::util::EnigmaParsingContext& textContext)
{
    if (!textContext) {
        return false;
    }
    bool sawTrill = false;
    const auto chunks = textContext.collectEnigmaTextChunks(
        musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (const auto& chunk : chunks) {
        if (!chunk.styles.font || chunk.styles.font->hidden || chunk.text.empty()) {
            continue;
        }
        for (utils::Utf8Iterator iter(chunk.text); !iter.atEnd(); iter.next()) {
            if (!iter.valid()) {
                return false;
            }
            const auto glyphName = detail::glyphNameForFont(chunk.styles.font, iter->codepoint);
            if (glyphName && *glyphName == "ornamentTrill") {
                if (sawTrill) {
                    return false;
                }
                sawTrill = true;
                continue;
            }
            if (iter->codepoint == U' ') {
                continue;
            }
            return false;
        }
    }
    return sawTrill;
}

std::optional<TrillLine> classifyTrillLine(const GeneralLine& line)
{
    const bool wiggleBody = line.lineStyle == musx::dom::others::SmartShapeCustomLine::LineStyle::Char
        && line.lineCharGlyphName && isTrillWiggleGlyph(*line.lineCharGlyphName);
    const bool trSymbolStart = isBareTrillSymbolText(line.startText);
    if (!wiggleBody && !trSymbolStart) {
        return std::nullopt;
    }
    // Any text this classifier cannot attribute to the trill rules the line out:
    // wiggle bodies also carry markings like flutter tongue or hold instructions.
    if (!trSymbolStart && line.startText) {
        return std::nullopt;
    }
    if (line.continuationText && !isBareTrillSymbolText(line.continuationText)) {
        return std::nullopt;
    }
    if (line.endText || line.centerFullText || line.centerAbbrText) {
        return std::nullopt;
    }
    return TrillLine{ trSymbolStart, line };
}

std::optional<VibratoLine> classifyVibratoLine(const GeneralLine& line)
{
    if (line.lineStyle != musx::dom::others::SmartShapeCustomLine::LineStyle::Char
        || !line.lineCharGlyphName || !isVibratoWiggleGlyph(*line.lineCharGlyphName)) {
        return std::nullopt;
    }
    if (line.startText || line.continuationText || line.endText
        || line.centerFullText || line.centerAbbrText) {
        return std::nullopt;
    }
    return VibratoLine{ line };
}

} // namespace

std::optional<KeyboardPedal> classifyKeyboardPedalCustomLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine)
{
    auto line = classifyGeneralLine(customLine);
    if (!line) {
        return std::nullopt;
    }

    KeyboardPedal result;
    result.line = std::move(*line);
    result.startText = classifyKeyboardPedal(result.line.startText);
    result.continuationText = classifyKeyboardPedal(result.line.continuationText);
    result.endText = classifyKeyboardPedal(result.line.endText);
    result.startCap = pedalCapSemantic(result.line.startCap);
    result.endCap = pedalCapSemantic(result.line.endCap);

    const bool hasPedalCap = isPedalCap(result.startCap) || isPedalCap(result.endCap);
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
    case ShapeType::OctaveUp:
    case ShapeType::TwoOctaveDown:
    case ShapeType::TwoOctaveUp:
        result.value = makeBuiltInOttava(shape, *builtInOttavaShift(shape->shapeType));
        return result;
    case ShapeType::Trill:
    case ShapeType::TrillExtension:
        if (!shape->entryBased) {
            result.value = TrillLine{ shape->shapeType == ShapeType::Trill, std::nullopt };
        }
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
        } else if (shape->lineStyleId != 0 && !shape->entryBased) {
            const auto customLine = shape->getDocument()->getOthers()->get<musx::dom::others::SmartShapeCustomLine>(
                shape->getRequestedPartId(), shape->lineStyleId);
            if (auto keyboardPedal = classifyKeyboardPedalCustomLine(customLine)) {
                result.value = std::move(*keyboardPedal);
            } else if (auto generalLine = classifyGeneralLine(customLine)) {
                if (auto ottava = classifyOttavaLine(shape, *generalLine)) {
                    result.value = std::move(*ottava);
                } else if (auto trillLine = classifyTrillLine(*generalLine)) {
                    result.value = std::move(*trillLine);
                } else if (auto vibratoLine = classifyVibratoLine(*generalLine)) {
                    result.value = std::move(*vibratoLine);
                } else {
                    result.value = std::move(*generalLine);
                }
            }
        }
        return result;
    case ShapeType::SolidLine:
    case ShapeType::SolidLineDown:
    case ShapeType::SolidLineUp:
    case ShapeType::SolidLineDownBoth:
    case ShapeType::SolidLineUpBoth:
    case ShapeType::SolidLineUpLeft:
    case ShapeType::SolidLineDownLeft:
    case ShapeType::SolidLineUpDown:
    case ShapeType::SolidLineDownUp:
    case ShapeType::DashLine:
    case ShapeType::DashLineDown:
    case ShapeType::DashLineUp:
    case ShapeType::DashLineDownBoth:
    case ShapeType::DashLineUpBoth:
    case ShapeType::DashLineUpLeft:
    case ShapeType::DashLineDownLeft:
    case ShapeType::DashLineUpDown:
    case ShapeType::DashLineDownUp:
        if (auto generalLine = classifyGeneralLine(shape)) {
            result.value = std::move(*generalLine);
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
