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

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <ranges>
#include <stdexcept>
#include <string>
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
/// resolved by pairing with a hidden built-in ottava, then by vertical placement
/// (engravers place alta lines above the staff and bassa lines below); when neither
/// resolves the direction, the line is not classified as an ottava at all.
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
            marking->directionIsExplicit = contMarking->directionIsExplicit;
        } else if (contMarking->direction != Direction::Unknown
            && contMarking->direction != marking->direction) {
            return std::nullopt;
        }
    }

    const auto placement = shape->calcVerticalPlacementForBeatAttached();
    auto direction = marking->direction;
    // "8va"-style markings state alta, but engravers sometimes use them below the
    // staff to mean bassa. Demote non-explicit alta there and let the pairing or
    // the placement fallback decide.
    if (direction == Direction::Up && !marking->directionIsExplicit
        && placement == musx::dom::VerticalPlacement::Below) {
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
        if (counterpartUp && counterpartDown) {
            // Ambiguous: hidden ottavas in both directions cover the line.
            return std::nullopt;
        }
        if (counterpartUp) {
            direction = Direction::Up;
        } else if (counterpartDown) {
            direction = Direction::Down;
        } else {
            switch (placement) {
            case musx::dom::VerticalPlacement::Above:
                direction = Direction::Up;
                break;
            case musx::dom::VerticalPlacement::Below:
                direction = Direction::Down;
                break;
            default:
                // Floating placement cannot resolve the direction.
                return std::nullopt;
            }
        }
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

// Resolves one end of an entry-attached line to a specific note. A shape names a note explicitly
// when its endpoint is on a chord; otherwise the entry's first note carries the line.
musx::dom::NoteInfoPtr resolveEntryNote(
    const std::shared_ptr<musx::dom::others::SmartShape::TerminationSeg>& termSeg,
    musx::dom::NoteNumber noteId)
{
    if (!termSeg) {
        return {};
    }
    const auto entry = termSeg->endPoint->calcAssociatedEntry();
    if (!entry) {
        return {};
    }
    if (noteId != 0) {
        if (const auto note = entry.findNoteId(noteId)) {
            return note;
        }
    }
    if (entry->getEntry()->notes.empty()) {
        return {};
    }
    return musx::dom::NoteInfoPtr(entry, 0);
}

bool isSamePitch(const musx::dom::NoteInfoPtr& left, const musx::dom::NoteInfoPtr& right)
{
    // Compare written pitch only. staffPosition, which NoteProperties::operator== also covers,
    // moves with cross-staff notation and would report two same-pitch notes as differing.
    const auto leftPitch = left.calcNoteProperties();
    const auto rightPitch = right.calcNoteProperties();
    return leftPitch.noteName == rightPitch.noteName
        && leftPitch.octave == rightPitch.octave
        && leftPitch.alteration == rightPitch.alteration;
}

// Whether any of a line's texts names a pitch-slide marking.
//
// This can only ever be a hint. The label may be in any language, or absent, so a shape that fails
// here is not thereby something else; see the caller for what happens to it.
bool namesPitchMotion(const GeneralLine& line)
{
    // Matched as word prefixes, so "gliss." and "glissando" both hit while "gripping" does not.
    // Short entries like "rip" make prefix matching rather than substring matching necessary.
    static constexpr std::array<std::string_view, 5> markingWords{
        "gliss", "port", "slide", "smear", "rip" };

    const auto namesMarking = [](const musx::util::EnigmaParsingContext& textContext) {
        if (!textContext) {
            return false;
        }
        std::string text;
        for (const auto& chunk : textContext.collectEnigmaTextChunks(
                 musx::util::EnigmaString::EnigmaParsingOptions())) {
            if (chunk.styles.font && chunk.styles.font->hidden) {
                continue;
            }
            text += chunk.text;
        }

        // Fold to lowercase and drop punctuation, keeping spaces so word boundaries survive.
        // Removing spaces too would let "big lissome" match "gliss".
        std::string normalized;
        normalized.reserve(text.size());
        for (const unsigned char character : text) {
            if (std::isspace(character)) {
                normalized.push_back(' ');
            } else if (std::isalnum(character)) {
                normalized.push_back(static_cast<char>(std::tolower(character)));
            }
        }

        for (const auto word : std::views::split(std::string_view{ normalized }, ' ')) {
            const std::string_view candidate{ word.begin(), word.end() };
            if (candidate.empty()) {
                continue;
            }
            if (std::ranges::any_of(markingWords, [candidate](std::string_view marking) {
                    return candidate.starts_with(marking);
                })) {
                return true;
            }
        }
        return false;
    };

    return namesMarking(line.startText) || namesMarking(line.centerFullText)
        || namesMarking(line.centerAbbrText) || namesMarking(line.endText);
}

// Whether the shape uses whichever line style the glissando or tab slide tool is currently
// configured to draw. A match is suggestive only: the tools' current styles say nothing about
// what an existing shape looks like, so a mismatch proves nothing either way.
bool matchesPitchMotionLineStyle(const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape)
{
    const auto options = shape->getDocument()->getOptions()->get<musx::dom::options::SmartShapeOptions>();
    if (!options || shape->lineStyleId == 0) {
        return false;
    }
    return shape->lineStyleId == options->ssLineStyleCmpGlissando
        || shape->lineStyleId == options->ssLineStyleCmpTabSlide;
}

// Whether an ordinary line drawn between two entries is evidence of a pitch slide rather than a
// bracket. Finale gives such a line no marker of its own, so the shape and its line definition
// together have to carry the case: it connects two notes of differing pitch, carries no bracket
// hardware, follows its endpoints instead of being pinned flat, and either names the marking or
// uses a line style one of the dedicated tools currently draws.
bool isPitchMotionEvidence(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    const GeneralLine& line,
    const musx::dom::NoteInfoPtr& startNote,
    const musx::dom::NoteInfoPtr& endNote)
{
    if (line.horizontal || !line.lineVisible) {
        // A horizontal line is not a pitch slide, and neither is a line that draws nothing.
        return false;
    }
    if (line.startCap.type != LineCap::Type::None || line.endCap.type != LineCap::Type::None) {
        // Hooks and arrowheads belong to brackets and arrows.
        return false;
    }
    if (isSamePitch(startNote, endNote)) {
        return false;
    }
    return namesPitchMotion(line) || matchesPitchMotionLineStyle(shape);
}

/// @param requireEvidence True for a line that carries no inherent pitch-slide meaning, so that
/// the shape must corroborate the reading. (See @ref isPitchMotionEvidence.)
std::optional<Glissando> classifyGlissando(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    bool requireEvidence)
{
    auto line = classifyGeneralLineAppearance(shape);
    if (!line) {
        return std::nullopt;
    }
    const auto startNote = resolveEntryNote(shape->startTermSeg, shape->startNoteId);
    const auto endNote = resolveEntryNote(shape->endTermSeg, shape->endNoteId);
    if (!startNote || !endNote) {
        // A glissando is a marking between two notes. A line with an endpoint on no note is not
        // one, whatever it was drawn with.
        return std::nullopt;
    }
    if (requireEvidence && !isPitchMotionEvidence(shape, *line, startNote, endNote)) {
        return std::nullopt;
    }
    return Glissando{ startNote, endNote, std::move(*line) };
}

} // namespace

bool smartshape::KeyboardPedal::isUnaCorda() const noexcept
{
    const auto matches = [](const std::optional<KeyboardPedalClassification>& marking) {
        return marking && marking->type == keyboardpedal::Type::PedalThree;
    };
    return matches(startText) || matches(continuationText) || matches(endText);
}

bool smartshape::KeyboardPedal::isSostPedal() const noexcept
{
    const auto matches = [](const std::optional<KeyboardPedalClassification>& marking) {
        return marking && marking->type == keyboardpedal::Type::PedalTwo;
    };
    return matches(startText) || matches(continuationText) || matches(endText);
}

bool smartshape::KeyboardPedal::isSustainPedal() const noexcept
{
    const auto matches = [](const std::optional<KeyboardPedalClassification>& marking) {
        return marking && marking->type == keyboardpedal::Type::PedalOne;
    };
    return matches(startText) || matches(continuationText) || matches(endText);
}

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
    case ShapeType::Glissando:
    case ShapeType::TabSlide:
        // Both dedicated tools take one path. A tab slide is a solid line meant for tablature,
        // but it is routinely drawn as a note-attached glissando on an ordinary staff, so it is
        // not a tablature-only feature. Neither shape type is required to be entry-attached for
        // the marking to make sense, and neither describes its own appearance.
        if (auto glissando = classifyGlissando(shape, /* requireEvidence */ false)) {
            result.value = std::move(*glissando);
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
        } else if (shape->lineStyleId != 0 && shape->entryBased) {
            // Users draw pitch slides with the line tools instead of reaching for the dedicated
            // ones. Such a line has no inherent meaning, so it must corroborate the reading; a
            // line that does not is still a line, and stays one rather than being discarded.
            if (auto glissando = classifyGlissando(shape, /* requireEvidence */ true)) {
                result.value = std::move(*glissando);
            } else if (auto generalLine = classifyGeneralLineAppearance(shape)) {
                result.value = std::move(*generalLine);
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
        if (shape->entryBased) {
            // As with an entry-attached custom line: a built-in line drawn between two entries
            // may be a pitch slide, but only the shape's own evidence can say so. In practice a
            // built-in line carries none, since it has no texts and no custom line style, so this
            // all but always falls through to the line itself.
            if (auto glissando = classifyGlissando(shape, /* requireEvidence */ true)) {
                result.value = std::move(*glissando);
            } else if (auto generalLine = classifyGeneralLineAppearance(shape)) {
                result.value = std::move(*generalLine);
            }
        } else if (auto generalLine = classifyGeneralLine(shape)) {
            result.value = std::move(*generalLine);
        }
        return result;
    default:
        break;
    }

    if (!shape->calcIsSlur()) {
        return result;
    }

    // Coinciding entries (within calcAssociatedEntry's quarter-beat tolerance) become
    // the slur's attachment points. Endpoints that resolve to no entry stay null: the
    // shape still classifies as a Slur, and exporters may host such floating endpoints
    // however their target format allows.
    const auto startEntry = shape->startTermSeg->endPoint->calcAssociatedEntry();
    const auto endEntry = shape->endTermSeg->endPoint->calcAssociatedEntry();

    const auto contour = shape->calcContourDirection();
    result.value = Slur{ startEntry, endEntry, contour };
    if (startEntry) {
        if (shape->calcIsPseudoTie(musx::utils::PseudoTieMode::LaissezVibrer, startEntry)) {
            result.value = PseudoTie{ PseudoTie::Type::LaissezVibrer, contour };
            return result;
        }
        if (shape->calcIsPseudoTie(musx::utils::PseudoTieMode::TieEnd, startEntry)) {
            result.value = PseudoTie{ PseudoTie::Type::TieEnd, contour };
            return result;
        }
    }

    if (shape->entryBased) {
        return result;
    }

    if (startEntry) {
        if (const auto tiedTo = shape->calcArpeggiatedTieToNote(startEntry)) {
            MUSX_ASSERT_IF(startEntry->getEntry()->notes.size() != 1) {
                throw std::logic_error("musxdom classified an arpeggiated tie on an entry with note count other than 1.");
            }
            result.value = ArpeggiatedTie{ musx::dom::NoteInfoPtr(startEntry, 0), tiedTo, contour };
            return result;
        }
    }

    result.value = Slur{ startEntry, endEntry, contour };
    return result;
}

} // namespace classify
} // namespace denigma
