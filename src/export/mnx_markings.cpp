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

std::vector<EventMarkingType> calcMarkingType(
    const details::ArticulationAssign::SelectedSymbolContext& articContext,
    std::optional<int>& numMarks)
{
    auto checkSymbol = [&](char32_t sym, const MusxInstance<FontInfo>& fontInfo) -> std::vector<EventMarkingType> {
        // First, check for standard Unicode chars
        switch (sym) {
            case 0x1D167:
            case 0x1D16A:
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            case 0x1D168:
            case 0x1D16B:
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            case 0x1D169:
            case 0x1D16C:
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            case 0x1D17B: return { EventMarkingType::Accent };
            case 0x1D17C: return { EventMarkingType::Staccato };
            case 0x1D17D: return { EventMarkingType::Tenuto };
            case 0x1D17E: return { EventMarkingType::Staccatissimo };
            case 0x1D17F: return { EventMarkingType::StrongAccent };
            default: break;                
        }

        /// @todo Spiccato marking has no SMuFl glyph!
        if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
            if (glyphName == "articAccentAbove" || glyphName == "articAccentBelow" || glyphName == "articAccentAboveLegacy") {
                return { EventMarkingType::Accent };
            } else if (glyphName == "articAccentStaccatoAbove" || glyphName == "articAccentStaccatoBelow" ||
                    glyphName == "articAccentStaccatoAboveLegacy" || glyphName == "articAccentStaccatoBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Staccato };
            } else if (glyphName == "articTenutoAccentAbove" || glyphName == "articTenutoAccentBelow" ||
                    glyphName == "articTenutoAccentAboveLegacy" || glyphName == "articTenutoAccentBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Tenuto };
            } else if (glyphName == "stringsDownBow" || glyphName == "stringsDownBowTurned") {
                return { EventMarkingType::BowDirectionDown };
            } else if (glyphName == "stringsUpBow" || glyphName == "stringsUpBowTurned") {
                return { EventMarkingType::BowDirectionUp };
            } else if (glyphName == "articStaccatissimoAbove" || glyphName == "articStaccatissimoBelow") {
                return { EventMarkingType::Spiccato };
            } else if (glyphName == "articStaccatissimoStrokeAbove" || glyphName == "articStaccatissimoStrokeBelow" ||
                    glyphName == "articStaccatissimoWedgeAbove" || glyphName == "articStaccatissimoWedgeBelow") {
                return { EventMarkingType::Staccatissimo };
            } else if (glyphName == "articStaccatoAbove" || glyphName == "articStaccatoBelow") {
                return { EventMarkingType::Staccato };
            } else if (glyphName == "articMarcatoStaccatoAbove" || glyphName == "articMarcatoStaccatoBelow" ||
                    glyphName == "articMarcatoStaccatoAboveLegacy" || glyphName == "articMarcatoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::StrongAccent };
            } else if (glyphName == "articTenutoStaccatoAbove" || glyphName == "articTenutoStaccatoBelow" ||
                    glyphName == "articTenutoStaccatoAboveLegacy" || glyphName == "articTenutoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
            } else if (glyphName == "articStressAbove" || glyphName == "articStressBelow") {
                return { EventMarkingType::Stress };
            } else if (glyphName == "articUnstressAbove" || glyphName == "articUnstressBelow") {
                return { EventMarkingType::Unstress };
            } else if (glyphName == "articMarcatoAbove" || glyphName == "articMarcatoBelow") {
                return { EventMarkingType::StrongAccent };
            } else if (glyphName == "articMarcatoTenutoAbove" || glyphName == "articMarcatoTenutoBelow") {
                return { EventMarkingType::StrongAccent, EventMarkingType::Tenuto };
            } else if (glyphName == "articTenutoAbove" || glyphName == "articTenutoBelow") {
                return { EventMarkingType::Tenuto };
            } else if (glyphName == "stemPendereckiTremolo" || glyphName == "buzzRoll" ||
                    glyphName == "pendereckiTremolo" || glyphName == "unmeasuredTremolo" ||
                    glyphName == "unmeasuredTremoloSimple" || glyphName == "stockhausenTremolo") {
                numMarks = 0;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo1" || glyphName == "tremoloFingered1" || glyphName == "tremolo1Alt") {
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo2" || glyphName == "tremoloFingered2" || glyphName == "tremolo2Alt") {
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo3" || glyphName == "tremoloFingered3" || glyphName == "tremolo3Alt") {
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo4" || glyphName == "tremoloFingered4" || glyphName == "tremolo4Legacy") {
                numMarks = 4;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo5" || glyphName == "tremoloFingered5" || glyphName == "tremolo5Legacy") {
                numMarks = 5;
                return { EventMarkingType::Tremolo };
            }
        }
        return {};
    };
    
    auto checkShape = [&](Cmper shapeId) -> std::vector<EventMarkingType> {
        const auto& articDef = *articContext.definition;
        if (auto shape = articDef.getDocument()->getOthers()->get<others::ShapeDef>(articDef.getRequestedPartId(), shapeId)) {
            switch (shape->recognize()) {
            case KnownShapeDefType::TenutoMark:
                return { EventMarkingType::Tenuto };
            default:
                break;
            }
        }
        return {};
    };

    auto result = articContext.symbol.isShape
                ? checkShape(articContext.symbol.shapeId)
                : checkSymbol(articContext.symbol.character, articContext.symbol.font);
    return result;
}

std::optional<mnx::Fermata> calcFermata(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement)
{
    if (placement == VerticalPlacement::NotApplicable) {
        return std::nullopt;
    }

    std::optional<mnx::Fermata> result;

    if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
        auto makeFermata = [&](mnx::FermataSymbol symbol, mnx::FermataDuration duration = mnx::FermataDuration::Auto) {
            auto& fermata = result.emplace();
            fermata.set_or_clear_symbol(symbol);
            fermata.set_or_clear_duration(duration);
            fermata.set_or_clear_orient(enumConvert<mnx::Orientation>(placement));
            fermata.set_or_clear_pointing(calcPointing(glyphName.value(), placement));
        };

        if (glyphName == "fermataAbove" || glyphName == "fermataBelow") {
            makeFermata(mnx::FermataSymbol::Normal);
        } else if (glyphName == "fermataVeryShortAbove" || glyphName == "fermataVeryShortBelow") {
            makeFermata(mnx::FermataSymbol::DoubleAngled, mnx::FermataDuration::VeryShort);
        } else if (glyphName == "fermataShortAbove" || glyphName == "fermataShortBelow") {
            makeFermata(mnx::FermataSymbol::Angled, mnx::FermataDuration::Short);
        } else if (glyphName == "fermataLongAbove" || glyphName == "fermataLongBelow") {
            makeFermata(mnx::FermataSymbol::Square, mnx::FermataDuration::Long);
        } else if (glyphName == "fermataVeryLongAbove" || glyphName == "fermataVeryLongBelow") {
            makeFermata(mnx::FermataSymbol::DoubleSquare, mnx::FermataDuration::VeryLong);
        } else if (glyphName == "fermataLongHenzeAbove" || glyphName == "fermataLongHenzeBelow") {
            makeFermata(mnx::FermataSymbol::DoubleDot, mnx::FermataDuration::Long);
        } else if (glyphName == "fermataShortHenzeAbove" || glyphName == "fermataShortHenzeBelow") {
            makeFermata(mnx::FermataSymbol::HalfCurve, mnx::FermataDuration::Short);
        } else if (glyphName == "curlewSign") {
            makeFermata(mnx::FermataSymbol::Curlew);
        }
    }

    return result;
}

std::optional<mnx::Fermata> calcFermata(const musx::util::EnigmaParsingContext& ctx, VerticalPlacement placement)
{
    const auto fontInfo = ctx.parseFirstFontInfo();
    const auto symStr = ctx.getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode);
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        return calcFermata(fontInfo, sym.value(), placement);
    }
    return std::nullopt;
}

std::optional<mnx::sequence::BreathMark> calcBreathMark(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement)
{
    std::optional<mnx::sequence::BreathMark> result;

    if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
        auto makeBreathMark = [&](mnx::BreathMarkSymbol breathSym) {
            auto& breathMark = result.emplace();
            breathMark.set_symbol(breathSym);
            breathMark.set_or_clear_orient(enumConvert<mnx::Orientation>(placement));
        };
        if (glyphName == "breathMarkComma" || glyphName == "breathMarkCommaLegacy") {
            makeBreathMark(mnx::BreathMarkSymbol::Comma);
        } else if (glyphName == "breathMarkTick") {
            makeBreathMark(mnx::BreathMarkSymbol::Tick);
        } else if (glyphName == "breathMarkUpbow") {
            makeBreathMark(mnx::BreathMarkSymbol::Upbow);
        } else if (glyphName == "breathMarkSalzedo") {
            makeBreathMark(mnx::BreathMarkSymbol::Salzedo);
        }
    }

    return result;
}

std::optional<mnx::sequence::BreathMark> calcBreathMark(const musx::util::EnigmaParsingContext& ctx, VerticalPlacement placement)
{
    const auto fontInfo = ctx.parseFirstFontInfo();
    const auto symStr = ctx.getText(/*trimTags*/ true, musx::util::EnigmaString::AccidentalStyle::Unicode);
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        return calcBreathMark(fontInfo, sym.value(), placement);
    }
    return std::nullopt;
}

std::optional<musx::util::ArpeggioSpanCandidate> calcArpeggio(const EntryInfoPtr& sourceEntry, const MusxInstance<details::ArticulationAssign>& assign)
{
    const static auto symFilter = [&](const details::ArticulationAssign::SelectedSymbolContext& symContext) -> bool {
        if (auto glyphName = utils::smuflGlyphNameForFont(symContext.symbol.font, symContext.symbol.character)) {
            return glyphName == "arpeggioVerticalSegment";
        }
        return false;
    };
    if (auto musxResult = musx::util::calcArpeggioSpanForAssignment(sourceEntry, assign, {}, symFilter)) {
        return musxResult;
    }
    if (const auto symContext = assign->calcSelectedSymbolContext(sourceEntry)) {
        if (auto glyphName = utils::smuflGlyphNameForFont(symContext->symbol.font, symContext->symbol.character)) {
            std::optional<musx::util::ArpeggioSpanCandidate> result;
            auto makeArpeggioCandidate = [&](mnx::MarkingUpDownAuto direction) {
                auto& candidate = result.emplace();
                candidate.sourceEntry = sourceEntry;
                candidate.topEntry = sourceEntry;
                candidate.bottomEntry = sourceEntry;
                switch (direction) {
                case mnx::MarkingUpDownAuto::Auto:
                    candidate.arrow = musx::util::ArpeggioArrow::None;
                    candidate.direction = musx::util::ArpeggioDirection::Auto;
                    break;
                case mnx::MarkingUpDownAuto::Up:
                    candidate.arrow = musx::util::ArpeggioArrow::Up;
                    candidate.direction = musx::util::ArpeggioDirection::Up;
                    break;
                case mnx::MarkingUpDownAuto::Down:
                    candidate.arrow = musx::util::ArpeggioArrow::Down;
                    candidate.direction = musx::util::ArpeggioDirection::Down;
                    break;
                }
            };

            if (glyphName == "arpeggiato") {
                makeArpeggioCandidate(mnx::MarkingUpDownAuto::Auto);
            } else if (glyphName == "arpeggiatoUp") {
                makeArpeggioCandidate(mnx::MarkingUpDownAuto::Up);
            } else if (glyphName == "arpeggiatoDown") {
                makeArpeggioCandidate(mnx::MarkingUpDownAuto::Down);
            }
            return result;
        }
    }
    return std::nullopt;
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
    auto savedMeas = context->currMeas;
    auto savedStaff = context->currStaff;
    context->currPartStaves = partIt->second;
    context->currMeas = candidate.sourceEntry.getMeasure();
    context->currStaff = context->currPartStaves.front();

    const auto splitTopNote = findArpeggioBoundaryNoteInCurrentPart(context, candidate.topEntry, candidate.bottomEntry, true);
    const auto splitBottomNote = findArpeggioBoundaryNoteInCurrentPart(context, candidate.bottomEntry, candidate.topEntry, false);

    context->currPartStaves = savedPartStaves;
    context->currMeas = savedMeas;
    context->currStaff = savedStaff;

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
