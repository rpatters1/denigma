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
#include <string>
#include <string_view>
#include <stdexcept>

#include "classify/articulations.h"
#include "mnx.h"
#include "mnx_mapping.h"
#include "utils/smufl_support.h"
#include "utils/utf8_iterator.h"

namespace denigma {
namespace mnxexp {

mnx::MarkingUpDownAuto calcPointing(const std::string_view glyphName, VerticalPlacement placement)
{
    const auto endsWith = [&](const std::string_view suffix) {
        return glyphName.size() >= suffix.size()
            && glyphName.compare(glyphName.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    const bool glyphIsAbove = endsWith("Above") || endsWith("AboveLegacy");
    const bool glyphIsBelow = endsWith("Below") || endsWith("BelowLegacy");
    switch (placement) {
    case VerticalPlacement::Above:
        if (glyphIsBelow) {
            return mnx::MarkingUpDownAuto::Down;
        }
        break;
    case VerticalPlacement::Below:
        if (glyphIsAbove) {
            return mnx::MarkingUpDownAuto::Up;
        }
        break;
    case VerticalPlacement::Float:
    case VerticalPlacement::NotApplicable:
        break;
    }
    return mnx::MarkingUpDownAuto::Auto;
}

mnx::MarkingUpDownAuto calcPointing(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement)
{
    if (const auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
        return calcPointing(glyphName.value(), placement);
    }
    return mnx::MarkingUpDownAuto::Auto;
}

std::optional<mnx::Fermata> makeFermata(const classify::Fermata& fermata, const std::optional<std::string>& glyphName, VerticalPlacement placement)
{
    if (placement == VerticalPlacement::NotApplicable) {
        return std::nullopt;
    }

    auto convertSymbol = [](classify::Fermata::Shape shape) {
        switch (shape) {
        case classify::Fermata::Shape::Normal: return mnx::FermataSymbol::Normal;
        case classify::Fermata::Shape::Angled: return mnx::FermataSymbol::Angled;
        case classify::Fermata::Shape::DoubleAngled: return mnx::FermataSymbol::DoubleAngled;
        case classify::Fermata::Shape::Square: return mnx::FermataSymbol::Square;
        case classify::Fermata::Shape::DoubleSquare: return mnx::FermataSymbol::DoubleSquare;
        case classify::Fermata::Shape::HalfCurve: return mnx::FermataSymbol::HalfCurve;
        case classify::Fermata::Shape::DoubleDot: return mnx::FermataSymbol::DoubleDot;
        case classify::Fermata::Shape::Curlew: return mnx::FermataSymbol::Curlew;
        }
        throw std::logic_error("Unhandled fermata shape.");
    };
    auto convertDuration = [](classify::Fermata::Duration duration) {
        switch (duration) {
        case classify::Fermata::Duration::Auto: return mnx::FermataDuration::Auto;
        case classify::Fermata::Duration::VeryShort: return mnx::FermataDuration::VeryShort;
        case classify::Fermata::Duration::Short: return mnx::FermataDuration::Short;
        case classify::Fermata::Duration::Long: return mnx::FermataDuration::Long;
        case classify::Fermata::Duration::VeryLong: return mnx::FermataDuration::VeryLong;
        }
        throw std::logic_error("Unhandled fermata duration.");
    };

    mnx::Fermata result;
    result.set_or_clear_symbol(convertSymbol(fermata.shape));
    result.set_or_clear_duration(convertDuration(fermata.duration));
    result.set_or_clear_orient(enumConvert<mnx::Orientation>(placement));
    if (glyphName) {
        result.set_or_clear_pointing(calcPointing(glyphName.value(), placement));
    }
    return result;
}

std::optional<mnx::Fermata> calcFermata(const musx::util::EnigmaParsingContext& ctx, VerticalPlacement placement)
{
    const auto fontInfo = ctx.parseFirstFontInfo();
    const auto symStr = ctx.getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode);
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        const auto classification = classify::classifyArticulationSymbol(fontInfo, sym.value());
        if (const auto* fermata = classification.as<classify::Fermata>()) {
            return makeFermata(*fermata, classification.glyphName, placement);
        }
    }
    return std::nullopt;
}

std::optional<mnx::sequence::BreathMark> makeBreathMark(const classify::BreathMark& breathMark, VerticalPlacement placement)
{
    std::optional<mnx::BreathMarkSymbol> symbol;
    switch (breathMark.type) {
    case classify::BreathMark::Type::Comma:
        symbol = mnx::BreathMarkSymbol::Comma;
        break;
    case classify::BreathMark::Type::Tick:
        symbol = mnx::BreathMarkSymbol::Tick;
        break;
    case classify::BreathMark::Type::Upbow:
        symbol = mnx::BreathMarkSymbol::Upbow;
        break;
    case classify::BreathMark::Type::Salzedo:
        symbol = mnx::BreathMarkSymbol::Salzedo;
        break;
    case classify::BreathMark::Type::Caesura:
    case classify::BreathMark::Type::CaesuraCurved:
    case classify::BreathMark::Type::CaesuraShort:
    case classify::BreathMark::Type::CaesuraThick:
    case classify::BreathMark::Type::ChantCaesura:
    case classify::BreathMark::Type::CaesuraSingleStroke:
        break;
    }
    if (!symbol) {
        return std::nullopt;
    }
    mnx::sequence::BreathMark result;
    result.set_symbol(symbol.value());
    result.set_or_clear_orient(enumConvert<mnx::Orientation>(placement));
    return result;
}

std::optional<mnx::sequence::BreathMark> calcBreathMark(const musx::util::EnigmaParsingContext& ctx, VerticalPlacement placement)
{
    const auto fontInfo = ctx.parseFirstFontInfo();
    const auto symStr = ctx.getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode);
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        const auto classification = classify::classifyArticulationSymbol(fontInfo, sym.value());
        if (const auto* breathMark = classification.as<classify::BreathMark>()) {
            return makeBreathMark(*breathMark, placement);
        }
    }
    return std::nullopt;
}

std::optional<musx::util::ArpeggioSpanCandidate> makeArpeggio(
    const EntryInfoPtr& sourceEntry,
    const MusxInstance<details::ArticulationAssign>& assign,
    const classify::Arpeggio& arpeggio)
{
    std::optional<musx::util::ArpeggioSpanCandidate> result;
    auto makeArpeggioCandidate = [&](musx::util::ArpeggioDirection direction, musx::util::ArpeggioArrow arrow) {
        auto& candidate = result.emplace();
        candidate.sourceEntry = sourceEntry;
        candidate.topEntry = sourceEntry;
        candidate.bottomEntry = sourceEntry;
        candidate.arrow = arrow;
        candidate.direction = direction;
    };

    switch (arpeggio.type) {
    case classify::Arpeggio::Type::VerticalSegment:
        return musx::util::calcArpeggioSpanForAssignment(
            sourceEntry, assign, {}, [](const details::ArticulationAssign::SelectedSymbolContext&) {
                // The selected symbol was already classified as a vertical arpeggio segment;
                // this callback bypasses musx's raw-SMuFL-codepoint fallback for legacy fonts.
                return true;
            });
    case classify::Arpeggio::Type::Normal:
        makeArpeggioCandidate(musx::util::ArpeggioDirection::Auto, musx::util::ArpeggioArrow::None);
        break;
    case classify::Arpeggio::Type::Up:
        makeArpeggioCandidate(musx::util::ArpeggioDirection::Up, musx::util::ArpeggioArrow::Up);
        break;
    case classify::Arpeggio::Type::Down:
        makeArpeggioCandidate(musx::util::ArpeggioDirection::Down, musx::util::ArpeggioArrow::Down);
        break;
    }
    return result;
}

static std::optional<NoteInfoPtr> findArepggioBoundaryNote(const EntryInfoPtr& entryInfo, bool topNote)
{
    if (!entryInfo) {
        return std::nullopt;
    }
    const auto entry = entryInfo->getEntry();
    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
        NoteInfoPtr note(entryInfo, noteIndex);
        if (topNote ? note.calcIsTop() : note.calcIsBottom()) {
            return note;
        }
    }
    return std::nullopt;
}

static std::optional<std::string> findMnxPartForNote(const MnxMusxMappingPtr& context, const NoteInfoPtr& note)
{
    const auto it = context->inst2Part.find(note.calcStaff());
    if (it == context->inst2Part.end()) {
        return std::nullopt;
    }
    return it->second;
}

static bool noteIsInCurrentPart(const MnxMusxMappingPtr& context, const NoteInfoPtr& note)
{
    return context->mnxPartStaffFromStaff(note.calcStaff()).has_value();
}

static std::optional<NoteInfoPtr> findArpeggioBoundaryNoteInCurrentPart(
    const MnxMusxMappingPtr& context,
    const EntryInfoPtr& preferredEntry,
    const EntryInfoPtr& fallbackEntry,
    bool topNote)
{
    auto findInEntry = [&](const EntryInfoPtr& entryInfo) -> std::optional<NoteInfoPtr> {
        if (!entryInfo) {
            return std::nullopt;
        }
        const auto entry = entryInfo->getEntry();
        if (topNote) {
            for (size_t noteIndex = entry->notes.size(); noteIndex-- > 0; ) {
                NoteInfoPtr note(entryInfo, noteIndex);
                if (noteIsInCurrentPart(context, note)) {
                    return note;
                }
            }
        } else {
            for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
                NoteInfoPtr note(entryInfo, noteIndex);
                if (noteIsInCurrentPart(context, note)) {
                    return note;
                }
            }
        }
        return std::nullopt;
    };

    if (auto note = findInEntry(preferredEntry)) {
        return note;
    }
    return findInEntry(fallbackEntry);
}

static void setArpeggioGraceIndex(mnx::RhythmicPosition position, const musx::util::ArpeggioSpanCandidate& candidate)
{
    if (candidate.sourceEntry->graceIndex) {
        position.set_graceIndex(candidate.sourceEntry.calcReverseGraceIndex());
    } else if (auto prevEntry = candidate.sourceEntry.getPreviousSameV(); prevEntry && prevEntry->graceIndex) {
        position.set_graceIndex(0);
    }
}

static void appendArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnx::part::Measure& mnxPartMeasure,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    const auto startId = [&]() -> std::string {
        switch (candidate.direction) {
        case musx::util::ArpeggioDirection::Down:
            return calcNoteId(topNote);
        case musx::util::ArpeggioDirection::Auto:
        case musx::util::ArpeggioDirection::Up:
            return calcNoteId(bottomNote);
        }
        ASSERT_IF(true) {
            throw std::logic_error("Unhandled arpeggio direction.");
        }
        return {};
    }();
    const auto endId = [&]() -> std::string {
        switch (candidate.direction) {
        case musx::util::ArpeggioDirection::Down:
            return calcNoteId(bottomNote);
        case musx::util::ArpeggioDirection::Auto:
        case musx::util::ArpeggioDirection::Up:
            return calcNoteId(topNote);
        }
        ASSERT_IF(true) {
            throw std::logic_error("Unhandled arpeggio direction.");
        }
        return {};
    }();

    auto mnxArpeggio = mnxPartMeasure.ensure_arpeggios().append(
        mnxFractionFromFraction(candidate.sourceEntry.calcGlobalElapsedDuration()),
        mnx::IdPair::make(startId, endId));
    setArpeggioGraceIndex(mnxArpeggio.position(), candidate);

    switch (candidate.direction) {
    case musx::util::ArpeggioDirection::Auto:
        mnxArpeggio.set_or_clear_direction(mnx::MarkingUpDownAuto::Auto);
        break;
    case musx::util::ArpeggioDirection::Up:
        mnxArpeggio.set_or_clear_direction(mnx::MarkingUpDownAuto::Up);
        break;
    case musx::util::ArpeggioDirection::Down:
        mnxArpeggio.set_or_clear_direction(mnx::MarkingUpDownAuto::Down);
        break;
    }

    switch (candidate.arrow) {
    case musx::util::ArpeggioArrow::Auto:
    case musx::util::ArpeggioArrow::None:
        mnxArpeggio.set_or_clear_arrow(false);
        break;
    case musx::util::ArpeggioArrow::Up:
    case musx::util::ArpeggioArrow::Down:
        mnxArpeggio.set_or_clear_arrow(true);
        break;
    }
}

static void appendNonArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnx::part::Measure& mnxPartMeasure,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    auto mnxNonArpeggio = mnxPartMeasure.ensure_nonArpeggios().append(
        mnxFractionFromFraction(candidate.sourceEntry.calcGlobalElapsedDuration()),
        mnx::IdPair::make(calcNoteId(bottomNote), calcNoteId(topNote)));
    setArpeggioGraceIndex(mnxNonArpeggio.position(), candidate);
}

static void appendArpeggioOrNonArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnx::part::Measure& mnxPartMeasure,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    using SpanType = musx::util::ArpeggioSpanType;
    switch (candidate.type) {
    case SpanType::Normal:
        appendArpeggio(topNote, bottomNote, mnxPartMeasure, candidate);
        break;
    case SpanType::Bracket:
        appendNonArpeggio(topNote, bottomNote, mnxPartMeasure, candidate);
        break;
    }
}

void appendArpeggioCandidate(const MnxMusxMappingPtr& context, mnx::part::Measure&,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    const auto topNote = findArepggioBoundaryNote(candidate.topEntry, true);
    const auto bottomNote = findArepggioBoundaryNote(candidate.bottomEntry, false);
    if (!topNote || !bottomNote) {
        context->logMessage(LogMsg() << "skipping arpeggio because its note span could not be resolved.",
            LogSeverity::Warning);
        return;
    }

    if (context->deferredArpeggioKeys.emplace(candidate.key()).second) {
        context->deferredArpeggios.push_back(candidate);
    }
}

static std::vector<std::string> findAffectedMnxPartsForArpeggio(const MnxMusxMappingPtr& context,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    auto collectEntryParts = [&](const EntryInfoPtr& entryInfo) {
        if (!entryInfo) {
            return;
        }
        const auto entry = entryInfo->getEntry();
        for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
            NoteInfoPtr note(entryInfo, noteIndex);
            if (auto partId = findMnxPartForNote(context, note); partId && seen.emplace(partId.value()).second) {
                result.push_back(partId.value());
            }
        }
    };
    collectEntryParts(candidate.topEntry);
    collectEntryParts(candidate.bottomEntry);
    return result;
}

static bool appendDeferredArpeggioToPart(const MnxMusxMappingPtr& context,
    const std::string& partId,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    const auto partIt = context->part2Inst.find(partId);
    if (partIt == context->part2Inst.end() || partIt->second.empty()) {
        return false;
    }

    auto savedPartStaves = context->currPartStaves;
    auto savedMeas = context->current.meas;
    auto savedStaff = context->current.staff;
    context->currPartStaves = partIt->second;
    context->current.meas = candidate.sourceEntry.getMeasure();
    context->current.staff = context->currPartStaves.front();

    const auto splitTopNote = findArpeggioBoundaryNoteInCurrentPart(context, candidate.topEntry, candidate.bottomEntry, true);
    const auto splitBottomNote = findArpeggioBoundaryNoteInCurrentPart(context, candidate.bottomEntry, candidate.topEntry, false);

    context->currPartStaves = savedPartStaves;
    context->current.meas = savedMeas;
    context->current.staff = savedStaff;

    if (!splitTopNote || !splitBottomNote) {
        return false;
    }
    const auto splitTopPart = findMnxPartForNote(context, splitTopNote.value());
    const auto splitBottomPart = findMnxPartForNote(context, splitBottomNote.value());
    if (!splitTopPart || !splitBottomPart || splitTopPart.value() != partId || splitBottomPart.value() != partId) {
        return false;
    }

    auto parts = context->mnxDocument->parts();
    for (size_t partIndex = 0; partIndex < parts.size(); ++partIndex) {
        auto part = parts[partIndex];
        if (!part.id() || part.id().value() != partId) {
            continue;
        }
        auto measures = part.measures();
        const auto measureIndex = static_cast<size_t>(candidate.sourceEntry.getMeasure() - 1);
        if (measureIndex >= measures.size()) {
            return false;
        }
        auto mnxMeasure = measures.at(measureIndex);
        appendArpeggioOrNonArpeggio(splitTopNote.value(), splitBottomNote.value(), mnxMeasure, candidate);
        return true;
    }
    return false;
}

void finalizeArpeggios(const MnxMusxMappingPtr& context)
{
    for (const auto& candidate : context->deferredArpeggios) {
        const auto topNote = findArepggioBoundaryNote(candidate.topEntry, true);
        const auto bottomNote = findArepggioBoundaryNote(candidate.bottomEntry, false);
        if (!topNote || !bottomNote) {
            context->logMessage(LogMsg() << "skipping arpeggio because its note span could not be resolved.",
                LogSeverity::Warning);
            continue;
        }

        const auto topPart = findMnxPartForNote(context, topNote.value());
        const auto bottomPart = findMnxPartForNote(context, bottomNote.value());
        if (!topPart || !bottomPart) {
            context->logMessage(LogMsg() << "skipping arpeggio because its note span includes a staff that is not assigned to an MNX part.",
                LogSeverity::Warning);
            continue;
        }

        std::vector<std::string> targetParts;
        if (topPart.value() == bottomPart.value()) {
            targetParts.push_back(topPart.value());
        } else {
            targetParts = findAffectedMnxPartsForArpeggio(context, candidate);
            context->logMessage(LogMsg() << "splitting arpeggio across MNX part boundary into "
                << mnxPartDisplayList(context, targetParts) << ".", LogSeverity::Info);
        }

        bool emittedAny = false;
        for (const auto& partId : targetParts) {
            emittedAny = appendDeferredArpeggioToPart(context, partId, candidate) || emittedAny;
        }
        if (!emittedAny) {
            context->logMessage(LogMsg() << "skipping arpeggio because no valid MNX part span could be resolved.",
                LogSeverity::Warning);
        }
    }
}

} // namespace mnxexp
} // namespace denigma
