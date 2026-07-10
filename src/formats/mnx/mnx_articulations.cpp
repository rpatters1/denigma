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
#include <stdexcept>

#include "denigma/classify/articulations.h"
#include "mnx.h"
#include "mnx_mapping.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

std::optional<mnxdom::Fermata> makeFermata(
    const classify::articulation::Fermata& fermata,
    const classify::glyph::GlyphStyle& glyphStyle,
    VerticalPlacement placement)
{
    if (placement == VerticalPlacement::NotApplicable) {
        return std::nullopt;
    }

    auto convertSymbol = [](classify::articulation::Fermata::Shape shape) {
        switch (shape) {
        case classify::articulation::Fermata::Shape::Normal: return mnxdom::FermataSymbol::Normal;
        case classify::articulation::Fermata::Shape::Angled: return mnxdom::FermataSymbol::Angled;
        case classify::articulation::Fermata::Shape::DoubleAngled: return mnxdom::FermataSymbol::DoubleAngled;
        case classify::articulation::Fermata::Shape::Square: return mnxdom::FermataSymbol::Square;
        case classify::articulation::Fermata::Shape::DoubleSquare: return mnxdom::FermataSymbol::DoubleSquare;
        case classify::articulation::Fermata::Shape::HalfCurve: return mnxdom::FermataSymbol::HalfCurve;
        case classify::articulation::Fermata::Shape::DoubleDot: return mnxdom::FermataSymbol::DoubleDot;
        case classify::articulation::Fermata::Shape::Curlew: return mnxdom::FermataSymbol::Curlew;
        }
        throw std::logic_error("Unhandled fermata shape.");
    };
    auto convertDuration = [](classify::articulation::Fermata::Duration duration) {
        switch (duration) {
        case classify::articulation::Fermata::Duration::Auto: return mnxdom::FermataDuration::Auto;
        case classify::articulation::Fermata::Duration::VeryShort: return mnxdom::FermataDuration::VeryShort;
        case classify::articulation::Fermata::Duration::Short: return mnxdom::FermataDuration::Short;
        case classify::articulation::Fermata::Duration::Long: return mnxdom::FermataDuration::Long;
        case classify::articulation::Fermata::Duration::VeryLong: return mnxdom::FermataDuration::VeryLong;
        }
        throw std::logic_error("Unhandled fermata duration.");
    };

    mnxdom::Fermata result;
    result.set_or_clear_symbol(convertSymbol(fermata.shape));
    result.set_or_clear_duration(convertDuration(fermata.duration));
    result.set_or_clear_orient(enumConvert<mnxdom::Orientation>(placement));
    result.set_or_clear_pointing(enumConvert<mnxdom::MarkingUpDownAuto>(glyphStyle.placement));
    return result;
}

std::optional<mnxdom::sequence::BreathMark> makeBreathMark(const classify::articulation::BreathMark& breathMark, VerticalPlacement placement)
{
    std::optional<mnxdom::BreathMarkSymbol> symbol;
    switch (breathMark.type) {
    case classify::articulation::BreathMark::Type::Comma:
        symbol = mnxdom::BreathMarkSymbol::Comma;
        break;
    case classify::articulation::BreathMark::Type::Tick:
        symbol = mnxdom::BreathMarkSymbol::Tick;
        break;
    case classify::articulation::BreathMark::Type::Upbow:
        symbol = mnxdom::BreathMarkSymbol::Upbow;
        break;
    case classify::articulation::BreathMark::Type::Salzedo:
        symbol = mnxdom::BreathMarkSymbol::Salzedo;
        break;
    }
    if (!symbol) {
        return std::nullopt;
    }
    mnxdom::sequence::BreathMark result;
    result.set_symbol(symbol.value());
    result.set_or_clear_orient(enumConvert<mnxdom::Orientation>(placement));
    return result;
}

std::optional<musx::util::ArpeggioSpanCandidate> makeArpeggio(
    const EntryInfoPtr& sourceEntry,
    const MusxInstance<details::ArticulationAssign>& assign,
    const classify::articulation::Arpeggio& arpeggio)
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
    case classify::articulation::Arpeggio::Type::VerticalSegment:
        return musx::util::calcArpeggioSpanForAssignment(
            sourceEntry, assign, {}, [](const details::ArticulationAssign::SelectedSymbolContext&) {
                // The selected symbol was already classified as a vertical arpeggio segment;
                // this callback bypasses musx's raw-SMuFL-codepoint fallback for legacy fonts.
                return true;
            });
    case classify::articulation::Arpeggio::Type::Normal:
        makeArpeggioCandidate(musx::util::ArpeggioDirection::Auto, musx::util::ArpeggioArrow::None);
        break;
    case classify::articulation::Arpeggio::Type::Up:
        makeArpeggioCandidate(musx::util::ArpeggioDirection::Up, musx::util::ArpeggioArrow::Up);
        break;
    case classify::articulation::Arpeggio::Type::Down:
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

static void setArpeggioGraceIndex(mnxdom::RhythmicPosition position, const musx::util::ArpeggioSpanCandidate& candidate)
{
    if (candidate.sourceEntry->graceIndex) {
        position.set_graceIndex(candidate.sourceEntry.calcReverseGraceIndex());
    } else if (auto prevEntry = candidate.sourceEntry.getPreviousSameV(); prevEntry && prevEntry->graceIndex) {
        position.set_graceIndex(0);
    }
}

static void appendArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnxdom::part::Measure& mnxPartMeasure,
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
        mnxdom::IdPair::make(startId, endId));
    setArpeggioGraceIndex(mnxArpeggio.position(), candidate);

    switch (candidate.direction) {
    case musx::util::ArpeggioDirection::Auto:
        mnxArpeggio.set_or_clear_direction(mnxdom::MarkingUpDownAuto::Auto);
        break;
    case musx::util::ArpeggioDirection::Up:
        mnxArpeggio.set_or_clear_direction(mnxdom::MarkingUpDownAuto::Up);
        break;
    case musx::util::ArpeggioDirection::Down:
        mnxArpeggio.set_or_clear_direction(mnxdom::MarkingUpDownAuto::Down);
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

static void appendNonArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnxdom::part::Measure& mnxPartMeasure,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    auto mnxNonArpeggio = mnxPartMeasure.ensure_nonArpeggios().append(
        mnxFractionFromFraction(candidate.sourceEntry.calcGlobalElapsedDuration()),
        mnxdom::IdPair::make(calcNoteId(bottomNote), calcNoteId(topNote)));
    setArpeggioGraceIndex(mnxNonArpeggio.position(), candidate);
}

static void appendArpeggioOrNonArpeggio(const NoteInfoPtr& topNote, const NoteInfoPtr& bottomNote, mnxdom::part::Measure& mnxPartMeasure,
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

void appendArpeggioCandidate(const MnxMusxMappingPtr& context, mnxdom::part::Measure&,
    const musx::util::ArpeggioSpanCandidate& candidate)
{
    const auto topNote = findArepggioBoundaryNote(candidate.topEntry, true);
    const auto bottomNote = findArepggioBoundaryNote(candidate.bottomEntry, false);
    if (!topNote || !bottomNote) {
        context->logMessage(LogMsg() << "skipping arpeggio because its note span could not be resolved.",
            MessageSeverity::Warning);
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
                MessageSeverity::Warning);
            continue;
        }

        const auto topPart = findMnxPartForNote(context, topNote.value());
        const auto bottomPart = findMnxPartForNote(context, bottomNote.value());
        if (!topPart || !bottomPart) {
            context->logMessage(LogMsg() << "skipping arpeggio because its note span includes a staff that is not assigned to an MNX part.",
                MessageSeverity::Warning);
            continue;
        }

        std::vector<std::string> targetParts;
        if (topPart.value() == bottomPart.value()) {
            targetParts.push_back(topPart.value());
        } else {
            targetParts = findAffectedMnxPartsForArpeggio(context, candidate);
            context->logMessage(LogMsg() << "splitting arpeggio across MNX part boundary into "
                << mnxPartDisplayList(context, targetParts) << ".", MessageSeverity::Info);
        }

        bool emittedAny = false;
        for (const auto& partId : targetParts) {
            emittedAny = appendDeferredArpeggioToPart(context, partId, candidate) || emittedAny;
        }
        if (!emittedAny) {
            context->logMessage(LogMsg() << "skipping arpeggio because no valid MNX part span could be resolved.",
                MessageSeverity::Warning);
        }
    }
}

static std::optional<mnxdom::sequence::EventMarkingBase> createEventMarking(
    mnxdom::sequence::EventMarkings mnxMarkings,
    const classify::articulation::ArticulationMark& mark)
{
    const auto setPointing = [&](auto marking) -> mnxdom::sequence::EventMarkingBase {
        marking.set_or_clear_pointing(enumConvert<mnxdom::MarkingUpDownAuto>(mark.glyphStyle.placement));
        return marking;
    };

    switch (mark.type) {
    case classify::articulation::ArticulationMark::Type::Accent:
        return mnxMarkings.ensure_accent();
    case classify::articulation::ArticulationMark::Type::SoftAccent:
        return mnxMarkings.ensure_softAccent();
    case classify::articulation::ArticulationMark::Type::Spiccato:
        return mnxMarkings.ensure_spiccato();
    case classify::articulation::ArticulationMark::Type::Staccatissimo:
        return mnxMarkings.ensure_staccatissimo();
    case classify::articulation::ArticulationMark::Type::Staccato:
        return mnxMarkings.ensure_staccato();
    case classify::articulation::ArticulationMark::Type::Stress:
        return mnxMarkings.ensure_stress();
    case classify::articulation::ArticulationMark::Type::StrongAccent:
        return setPointing(mnxMarkings.ensure_strongAccent());
    case classify::articulation::ArticulationMark::Type::Tenuto:
        return mnxMarkings.ensure_tenuto();
    case classify::articulation::ArticulationMark::Type::Unstress:
        return mnxMarkings.ensure_unstress();
    default:
        break;
    }
    return std::nullopt;
}

static std::optional<mnxdom::sequence::EventMarkingBase> createEventMarking(
    mnxdom::sequence::EventMarkings mnxMarkings,
    const classify::articulation::TechniqueMark& mark)
{
    switch (mark.type) {
    case classify::articulation::TechniqueMark::Type::DownBow:
        return mnxMarkings.ensure_bowDirection(mnxdom::MarkingUpDown::Down);
    case classify::articulation::TechniqueMark::Type::UpBow:
        return mnxMarkings.ensure_bowDirection(mnxdom::MarkingUpDown::Up);
    default:
        break;
    }
    return std::nullopt;
}

void processArticulations(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();
    auto mnxPartMeasure = mnxEvent.getEnclosingElement<mnxdom::part::Measure>();
    ASSERT_IF(!mnxPartMeasure) {
        context->logMessage(LogMsg() << "no part measure exists for " << mnxEvent.dump(4), MessageSeverity::Warning);
        return;
    }
    auto articAssigns = context->document->getDetails()->getArray<details::ArticulationAssign>(SCORE_PARTID, musxEntry->getEntryNumber());
    for (const auto& asgn : articAssigns) {
        if (!asgn->hide) { /// @todo eliminate this filter if MNX provides visibility options
            if (const auto classification = classify::classifyArticulation(asgn, musxEntryInfo)) {
                if (const auto* fermata = classification.as<classify::articulation::Fermata>()) {
                    if (auto mnxFermata = makeFermata(*fermata, fermata->glyphStyle, classification.placement)) {
                        mnxEvent.set_fermata(mnxFermata.value());
                    }
                } else if (const auto* breathMark = classification.as<classify::articulation::BreathMark>()) {
                    if (auto mnxBreathMark = makeBreathMark(*breathMark, classification.placement)) {
                        mnxEvent.ensure_markings().set_breath(mnxBreathMark.value());
                    }
                } else if (const auto* arpeggio = classification.as<classify::articulation::Arpeggio>()) {
                    if (auto candidate = makeArpeggio(musxEntryInfo, asgn, *arpeggio)) {
                        appendArpeggioCandidate(context, mnxPartMeasure.value(), candidate.value());
                    }
                } else if (classification.is<classify::articulation::VerticalEntryBracket>()) {
                    if (auto candidate = musx::util::calcNonArpeggioSpanForAssignment(musxEntryInfo, asgn)) {
                        appendArpeggioCandidate(context, mnxPartMeasure.value(), candidate.value());
                    }
                } else if (const auto* tremolo = classification.as<classify::articulation::Tremolo>()) {
                    auto mnxMarkings = mnxEvent.ensure_markings();
                    auto mnxMarking = mnxMarkings.ensure_tremolo(tremolo->marks);
                    mnxMarking.set_or_clear_orient(enumConvert<mnxdom::Orientation>(classification.placement));
                } else if (const auto* articulation = classification.as<classify::articulation::ArticulationMarks>()) {
                    for (const auto& mark : articulation->marks) {
                        auto mnxMarkings = mnxEvent.ensure_markings();
                        if (auto mnxMarking = createEventMarking(mnxMarkings, mark)) {
                            mnxMarking->set_or_clear_orient(enumConvert<mnxdom::Orientation>(classification.placement));
                        }
                    }
                } else if (const auto* technique = classification.as<classify::articulation::TechniqueMark>()) {
                    auto mnxMarkings = mnxEvent.ensure_markings();
                    if (auto mnxMarking = createEventMarking(mnxMarkings, *technique)) {
                        mnxMarking->set_or_clear_orient(enumConvert<mnxdom::Orientation>(classification.placement));
                    }
                }
            }
        }
    }
}

void processArticulations(const MnxMusxMappingPtr& context, mnxdom::sequence::FullMeasureRest& mnxFullMeasureRest, const EntryInfoPtr& musxEntryInfo)
{
    auto articAssigns = context->document->getDetails()->getArray<details::ArticulationAssign>(SCORE_PARTID, musxEntryInfo->getEntry()->getEntryNumber());
    for (const auto& asgn : articAssigns) {
        if (!asgn->hide) {
            if (const auto classification = classify::classifyArticulation(asgn, musxEntryInfo)) {
                if (const auto* fermata = classification.as<classify::articulation::Fermata>()) {
                    if (const auto mnxFermata = makeFermata(*fermata, fermata->glyphStyle, classification.placement)) {
                        mnxFullMeasureRest.set_fermata(mnxFermata.value());
                        continue;
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
