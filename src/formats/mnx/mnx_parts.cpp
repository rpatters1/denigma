/*
 * Copyright (C) 2024, Robert Patterson
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
#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "mnx.h"
#include "mnx_expressions.h"
#include "mnx_smartshapes.h"

#include "denigma/classify/clefs.h"
#include "denigma/classify/dynamics.h"
#include "utils/stringutils.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

static void createBeams(
    const MnxMusxMappingPtr& context,
    mnxdom::part::Measure mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const auto mnxMeasures = mnxMeasure.parent<mnxdom::Array<mnxdom::part::Measure>>();
    const auto partId = musxMeasure->getRequestedPartId();
    if (context->current.staffMeasureContext) {
        if (context->current.cuePlan.isDetectedCueOnly) {
            return; // skip cues until MNX spec includes them
        }
        auto processEntry = [&](const EntryInfoPtr& entryInfo) -> bool {
            auto processBeam = [&](mnxdom::Array<mnxdom::part::Beam>&& mnxBeams, unsigned beamNumber, const EntryInfoPtr& firstInBeam, auto&& self) -> void {
                assert(firstInBeam.calcLowestBeamStart(/*considerBeamOverBarlines*/true) <= beamNumber);
                auto beam = mnxBeams.append();
                for (auto next = firstInBeam; next; next = next.getNextInBeamGroupAcrossBars(EntryInfoPtr::BeamIterationMode::Interpreted)) {
                    const auto entry = next->getEntry();
                    const EntryNumber entryNumber = entry->getEntryNumber();
                    context->beamedEntries.emplace(entryNumber);
                    beam.events().push_back(calcEventId(entryNumber));
                    if (unsigned lowestBeamStart = next.calcLowestBeamStart(/*considerBeamOverBarlines*/true)) {
                        unsigned nextBeamNumber = beamNumber + 1;
                        unsigned lowestBeamStub = next.calcLowestBeamStub();
                        if (lowestBeamStub && lowestBeamStub <= nextBeamNumber && next.calcNumberOfBeams() >= nextBeamNumber) {
                            auto hookBeam = beam.ensure_beams().append();
                            hookBeam.events().push_back(calcEventId(entryNumber));
                            if (entry->stemDetail) {
                                if (auto manual = musxDocument->getDetails()->get<details::BeamStubDirection>(partId, entryNumber)) {
                                    mnxdom::BeamHookDirection hookDir = manual->isLeft()
                                                                     ? mnxdom::BeamHookDirection::Left
                                                                     : mnxdom::BeamHookDirection::Right;
                                    hookBeam.set_or_clear_direction(hookDir);
                                }
                            }
                        } else if (lowestBeamStart <= nextBeamNumber && next.calcNumberOfBeams() >= nextBeamNumber) {
                            self(beam.ensure_beams(), nextBeamNumber, next, self);
                        }
                    }
                    if (unsigned lowestBeamEnd = next.calcLowestBeamEndAcrossBarlines()) {
                        if (lowestBeamEnd <= beamNumber) {
                            break;
                        }
                    }
                }
            };
            if (context->beamedEntries.contains(entryInfo->getEntry()->getEntryNumber())) {
                return true; // skip any entry that is already in a primary beam.
            }
            if (entryInfo.calcIsBeamStart(EntryInfoPtr::BeamIterationMode::Interpreted)) {
                const auto tupletIndices = entryInfo.findTupletInfo();
                for (size_t x : tupletIndices) {
                    const auto& tuplet = entryInfo.getFrame()->tupletInfo[x];
                    if (tuplet.includesEntry(entryInfo)) {
                        if (tuplet.calcIsTremolo()) {
                            /// @todo this may need to be more sophisticated in the future, but Finale can't really put a tremolo inside a beam.
                            return true; // skip any beam that is also a tremolo.
                        }
                    }
                }
                if (auto sourceEntry = entryInfo.findHiddenSourceForBeamOverBarline()) {
                    const auto sourceMeasureId = static_cast<size_t>(sourceEntry.getMeasure());
                    ASSERT_IF(sourceMeasureId >= mnxMeasures.size() || sourceMeasureId == 0) {
                        throw std::logic_error("Source entry's measure " + std::to_string(sourceMeasureId) + " is not a valid measure.");
                    }
                    mnxMeasure = mnxMeasures.at(sourceMeasureId - 1);
                } else {
                    mnxMeasure = mnxMeasures.at(entryInfo.getMeasure() - 1);
                }
                processBeam(mnxMeasure.ensure_beams(), 1, entryInfo, processBeam);
            }
            return true;
        };
        for (const auto& [layer, _] : context->current.layerVoices) {
            if (context->current.cuePlan.isCueLayer(layer)) {
                continue;
            }
            // The entries these beams belong to are not exported, so neither are the beams.
            if (context->current.layersHiddenByAltNotation.contains(layer)) {
                continue;
            }
            if (auto entryFrame = context->current.staffMeasureContext->createEntryFrame(layer)) {
                entryFrame->iterateEntries(processEntry);
            }
        }
    }
}

static std::optional<ClefIndex> createClef(
    const MnxMusxMappingPtr& context,
    mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    ClefIndex clefIndex,
    musx::util::Fraction location,
    const MusxInstance<others::Staff>& musxStaff)
{
    MUSX_ASSERT_IF(!musxStaff) {
        context->logMessage(LogMsg() << "invalid or unmapped staff passed to createClef", MessageSeverity::Warning);
        return std::nullopt;
    }
    const auto& musxClef = context->finaleOptions.clefOptions->getClefDef(clefIndex);
    const auto clef = classify::classifyClef(musxClef, musxStaff);
    std::optional<mnxdom::ClefSign> clefSign;
    if (clef && !clef.isBlank && std::abs(clef.octave) <= 3) {
        switch (clef.type) {
        case music_theory::ClefType::G: clefSign = mnxdom::ClefSign::GClef; break;
        case music_theory::ClefType::C: clefSign = mnxdom::ClefSign::CClef; break;
        case music_theory::ClefType::F: clefSign = mnxdom::ClefSign::FClef; break;
        /// @todo handle Percussion and Tab cases when defined in mnx spec
        default: break;
        }
    }
    if (clefSign) {
        int staffPosition = mnxStaffPosition(musxStaff, musxClef->staffPosition);
        auto mnxClef = mnxMeasure.ensure_clefs().append(clefSign.value(), staffPosition, mnxdom::OttavaAmountOrZero(clef.octave));
        if (location) {
            mnxClef.ensure_position(mnxFractionFromFraction(location));
        }
        if (!clef.showOctave) {
            mnxClef.clef().set_showOctave(false);
        }
        if (mnxStaffNumber) {
            mnxClef.set_staff(mnxStaffNumber.value());
        }
        if (clef.glyphName) {
            mnxClef.clef().set_glyph(clef.glyphName.value());
        }
        return clefIndex;
    } else {
        context->logMessage(LogMsg() << "Clef char " << int(musxClef->clefChar) << " has no clef info. " << " (glyph name is " << clef.glyphName.value_or("") << ")"
            << " Clef change was skipped.", MessageSeverity::Verbose);
    }
    return std::nullopt;
};

static void createClefs(
    const MnxMusxMappingPtr& context,
    const mnxdom::Part& mnxPart,
    mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    std::optional<ClefIndex>& prevClefIndex)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const StaffCmper staffCmper = context->current.staff;

    // MNX models a clef as a positioned, drawn clef rather than as a clef change, and its schema
    // places no uniqueness requirement on a measure's clefs. Restating the prevailing clef is
    // therefore valid MNX, and it is the only way to carry Finale's forced (ShowClefMode::Always)
    // clef display: MNX has no counterpart to MusicXML's clef@additional, so a consumer that
    // dedupes against the prevailing clef will still drop the restatement.
    //
    // ShowClefMode::Never has no MNX representation at all, because a clef carries no visibility
    // flag. The clef cannot simply be omitted either, since it determines the staff positions of
    // the following notes, so Finale's hidden clefs are exported as ordinary visible clefs.
    auto addClef = [&](const others::Staff::ClefChange& clefChange) {
        const auto clefIndex = clefChange.clefIndex;
        const auto location = clefChange.position;
        if (clefIndex == prevClefIndex && clefChange.showClefMode != ShowClefMode::Always) {
            return;
        }
        auto musxStaff = musx::dom::others::StaffComposite::createCurrent(
            musxDocument, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), location.calcEduDuration());
        if (!musxStaff) {
            context->logMessage(LogMsg() << mnxPartDisplayName(context, mnxPart)
                << " has no staff information for staff " << staffCmper, MessageSeverity::Warning);
            return;
        }
        if (auto newClefIndex = createClef(context, mnxMeasure, mnxStaffNumber, clefIndex, location, musxStaff)) {
            prevClefIndex = newClefIndex;
        }
    };

    auto staff = others::StaffComposite::createCurrent(musxDocument, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), 0);
    if (!staff) {
        context->logMessage(LogMsg() << mnxPartDisplayName(context, mnxPart)
            << " has no staff information for staff " << staffCmper, MessageSeverity::Warning);
        return;
    }
    staff->iterateClefChangesAtMeasure(
        musxMeasure->getCmper(),
        /*forWrittenPitch*/ true,
        [&](const others::Staff::ClefChange& clefChange) {
            addClef(clefChange);
            return true;
        });
}

static std::optional<std::pair<MusicPoint, InstrumentInfo::InstrumentChange>> findChangeForIdentity(
    const InstrumentInfo& instInfo,
    const InstrumentInfo::InstrumentIdentity& identity)
{
    for (const auto& [point, change] : instInfo.getChanges()) {
        if (change.identity == identity) {
            return std::make_pair(point, change);
        }
    }
    return std::nullopt;
}

static bool createFirstClefForInactiveInstrument(
    const MnxMusxMappingPtr& context,
    mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffCmper,
    std::optional<ClefIndex>& prevClefIndex)
{
    if (context->currSplitInstrumentUuid) {
        const auto& instInfo = context->document->getInstrumentForStaff(staffCmper);
        const auto identity = instInfo.getInstrumentIdentityAt(MusicPoint(musxMeasure->getCmper(), musx::util::Fraction{}));
        if (identity.instUuid != context->currSplitInstrumentUuid.value()) {
            if (musxMeasure->getCmper() == 1) {
                const auto splitIdentity = InstrumentInfo::InstrumentIdentity{ context->currSplitInstrumentUuid.value() };
                if (const auto splitChange = findChangeForIdentity(instInfo, splitIdentity)) {
                    const auto& splitPoint = splitChange->first;
                    const auto musxStaff = others::StaffComposite::createCurrent(
                        musxMeasure->getDocument(),
                        musxMeasure->getRequestedPartId(),
                        staffCmper,
                        splitPoint.measureId,
                        splitPoint.position.calcEduDuration());
                    if (!musxStaff) {
                        return true;
                    }
                    prevClefIndex = createClef(
                        context,
                        mnxMeasure,
                        mnxStaffNumber,
                        musxStaff->calcClefIndex(/*forWrittenPitch*/ true),
                        0,
                        musxStaff);
                }
            }
            return true;
        }
    }
    return false;
}

/// @brief Number of measures repeated by the alternate notation on a staff, or 0 for no repeat.
static int calcRepeatSpan(others::Staff::AlternateNotation altNotation)
{
    switch (altNotation) {
        case others::Staff::AlternateNotation::OneBarRepeat: return 1;
        case others::Staff::AlternateNotation::TwoBarRepeat: return 2;
        default: return 0;
    }
}

/// @brief Returns the measure repeat span that applies to an entire part at a measure, or 0 for none.
///
/// MNX gives a part measure a single measure repeat shared by every staff, so a repeat that applies
/// to only some of a part's staves cannot be expressed and is skipped.
static int calcPartRepeatSpan(const MnxMusxMappingPtr& context, mnxdom::Part& part,
    const MusxInstance<others::Measure>& musxMeasure)
{
    std::optional<int> partSpan;
    for (const StaffCmper staffCmper : context->currPartStaves) {
        const auto musxStaff = others::StaffComposite::createCurrent(
            context->document, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), 0);
        const int staffSpan = musxStaff ? calcRepeatSpan(musxStaff->altNotation) : 0;
        if (partSpan && partSpan.value() != staffSpan) {
            context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " has a measure repeat in measure "
                << musxMeasure->getCmper() << " that does not apply to every staff of the part;"
                " MNX has no per-staff measure repeat, so it is not exported.", MessageSeverity::Verbose);
            return 0;
        }
        partSpan = staffSpan;
    }
    return partSpan.value_or(0);
}

/// @brief Exports Finale's one- and two-bar repeat alternate notation as MNX measure repeats.
///
/// A measure repeat occupies the `number` measures beginning with the one that declares it, and
/// repeats the `number` measures before that, so only the first measure of a group carries the
/// marking. Finale instead flags every measure the notation covers, and when a two-bar region spans
/// an odd number of measures its last group still occupies two measures, the second of which falls
/// outside the region and carries no flag. Pairing therefore advances from the first flagged measure
/// of a region rather than following the flags measure by measure.
///
/// Sequence content is left in place. The measure repeat object permits a marking and notation in
/// the same bar, and Finale's alternate notation likewise hides the content without removing it.
static void createMeasureRepeats(const MnxMusxMappingPtr& context, mnxdom::Part& part,
    mnxdom::Array<mnxdom::part::Measure>& mnxMeasures, const MusxInstanceList<others::Measure>& musxMeasures)
{
    // Measure repeats are a property of the part rather than of any one measure and staff, so the
    // current measure and staff no longer apply to messages logged from here.
    context->current.clear();
    const size_t measureCount = musxMeasures.size();
    for (size_t index = 0; index < measureCount;) {
        const int span = calcPartRepeatSpan(context, part, musxMeasures[index]);
        if (span <= 0) {
            index++;
            continue;
        }
        // Skip repeats that would reach back before the first measure or past the last one. Both are
        // semantic violations in MNX, and neither can be represented by a shorter repeat.
        const bool reachesBackTooFar = static_cast<size_t>(span) > index;
        const bool extendsPastEnd = index + static_cast<size_t>(span) > measureCount;
        if (reachesBackTooFar || extendsPastEnd) {
            context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " has a " << span
                << "-bar repeat in measure " << musxMeasures[index]->getCmper()
                << " that would " << (reachesBackTooFar ? "reach back before the first measure" : "extend past the last measure")
                << "; it is not exported.", MessageSeverity::Warning);
        } else {
            auto mnxRepeat = mnxMeasures.at(index).ensure_measureRepeat(span);
            // A counter belongs to the repeat, so only one found on the measure that declares the
            // repeat can be attached. Counters on the measures a group covers are reported below.
            if (const auto counter = context->measureRepeatCounts.extract(musxMeasures[index]->getCmper())) {
                mnxRepeat.ensure_counter(counter.mapped());
            }
        }
        // Finale flags every measure a repeat region covers, so a covered measure repeating the same
        // number of bars is the continuation of this group and needs no comment. A covered measure
        // asking for a different number of bars is a conflict, and MNX can only keep one of them,
        // because it allows only the first measure of a group to declare a repeat.
        for (size_t covered = index + 1; covered < index + static_cast<size_t>(span) && covered < measureCount; covered++) {
            const int coveredSpan = calcPartRepeatSpan(context, part, musxMeasures[covered]);
            if (coveredSpan > 0 && coveredSpan != span) {
                context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " has a " << coveredSpan
                    << "-bar repeat in measure " << musxMeasures[covered]->getCmper() << " that falls inside the "
                    << span << "-bar repeat beginning in measure " << musxMeasures[index]->getCmper()
                    << "; only the repeat that begins the group is exported.", MessageSeverity::Warning);
            }
        }
        index += static_cast<size_t>(span);
    }
    // Whatever is left counts a repeat that was not exported, or a measure that a repeat covers
    // rather than declares. MNX has nowhere to put either, and the count is no longer available to
    // the expression export that would otherwise have carried its text.
    for (const auto& [measureId, count] : context->measureRepeatCounts) {
        context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " has measure repeat counter "
            << count << " in measure " << measureId << ", which does not begin an exported measure repeat;"
            " the counter is not exported.", MessageSeverity::Warning);
    }
    context->measureRepeatCounts.clear();
}

static void createMeasures(const MnxMusxMappingPtr& context, mnxdom::Part& part)
{
    auto& musxDocument = context->document;
    context->clearCounts();
    if (const auto partId = part.id()) {
        if (const auto splitIt = context->part2SplitInstrumentUuid.find(partId.value()); splitIt != context->part2SplitInstrumentUuid.end()) {
            context->currSplitInstrumentUuid = splitIt->second;
        }
    }

    // Retrieve the linked parts in order.
    auto musxMeasures = musxDocument->getOthers()->getArray<others::Measure>(SCORE_PARTID);
    auto mnxMeasures = part.create_measures();
    const auto it = context->part2Inst.find(part.id_or(""));
    if (it == context->part2Inst.end() || it->second.empty()) {
        context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " is not mapped", MessageSeverity::Warning);
    } else {
        context->currPartStaves = it->second;
    }
    std::vector<std::optional<ClefIndex>> prevClefs;
    for (size_t x = 0; x < context->currPartStaves.size(); x++) {
        prevClefs.push_back(std::nullopt);
    }
    for (std::size_t i = 0; i < musxMeasures.size(); ++i) {
        // create measures in one go, so that createBeams can add a beam to any measure
        mnxMeasures.append();
    }
    size_t measureIndex = 0;
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxMeasures.at(measureIndex++);
        if (const auto partId = part.id()) {
            mnxMeasure.set_id(partId.value() + "." + calcGlobalMeasureId(musxMeasure->getCmper()));
        }
        for (size_t x = 0; x < context->currPartStaves.size(); x++) {
            const StaffCmper staffCmper = context->currPartStaves[x];
            std::optional<int> staffNumber = (context->currPartStaves.size() > 1) ? std::optional<int>(int(x) + 1) : std::nullopt;
            if (createFirstClefForInactiveInstrument(context, mnxMeasure, staffNumber, musxMeasure, staffCmper, prevClefs[x])) {
                continue;
            }
            if (!context->currSplitInstrumentUuid && musxMeasure->getCmper() == 1) {
                const auto musxStaff = others::StaffComposite::createCurrent(musxDocument, musxMeasure->getRequestedPartId(), staffCmper, 1, 0);
                prevClefs[x] = createClef(context, mnxMeasure, staffNumber, musxStaff->calcClefIndex(/*forWrittenPitch*/ true), 0, musxStaff);
            }
            context->setCurrentMeasureStaff(musxMeasure, staffCmper);
            createBeams(context, mnxMeasure, musxMeasure);
            createClefs(context, part, mnxMeasure, staffNumber, musxMeasure, prevClefs[x]);
            // ottaves must be created before sequences, so that the ottava octaves can be correctly calculated for events
            createOttavas(context, musxMeasure, mnxMeasure, staffNumber);
            createSequences(context, mnxMeasure, staffNumber, musxMeasure);
            // the following must come after sequences, in case we need to attach to events (e.g., fermatas);
            processExpressions(context, musxMeasure, mnxMeasure, staffNumber);
            processSmartShapes(context, musxMeasure, mnxMeasure, staffNumber);
        }
    }
    createMeasureRepeats(context, part, mnxMeasures, musxMeasures);
    context->clearCounts();
}

static void logNonBarlineInstrumentChanges(const MnxMusxMappingPtr& context, const InstrumentInfo& instInfo, StaffCmper topStaffId)
{
    for (const auto& [point, change] : instInfo.getChanges()) {
        static_cast<void>(change);
        if (point.position != musx::util::Fraction{}) {
            context->logMessage(LogMsg() << "Instrument change for staff " << topStaffId
                << " occurs inside measure " << point.measureId << " at edu "
                << point.position.calcEduDuration() << ". Split-instruments export routes by measure start.",
                MessageSeverity::Warning);
        }
    }
}

static void mapPartToInstrumentStaves(const MnxMusxMappingPtr& context, const std::string& id, const InstrumentInfo& instInfo)
{
    if (instInfo.staves.size() > 1) {
        for (const auto& [staffId, index] : instInfo.staves) {
            static_cast<void>(index);
            context->inst2Part.emplace(staffId, id);
        }
        context->part2Inst.emplace(id, instInfo.getSequentialStaves());
    } else {
        const auto staves = instInfo.getSequentialStaves();
        if (!staves.empty()) {
            context->inst2Part.emplace(staves.front(), id);
            context->part2Inst.emplace(id, std::vector<StaffCmper>({ staves.front() }));
        }
    }
}

static void populatePartMetadata(
    const MnxMusxMappingPtr& context,
    mnxdom::Part& part,
    const std::string& id,
    const InstrumentInfo& instInfo,
    const MusxInstance<others::StaffComposite>& staff)
{
    const auto musxMiscOptions = context->finaleOptions.miscOptions;

    part.set_id(id);
    const auto fullName = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!fullName.empty()) {
        part.set_name(utils::trimNewLineFromString(fullName));
    }
    const auto abrvName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!abrvName.empty()) {
        part.set_shortName(utils::trimNewLineFromString(abrvName));
    }
    if (instInfo.staves.size() > 1) {
        part.set_staves(int(instInfo.staves.size()));
    }
    const auto [transpositionDisp, transpositionAlt] = staff->calcTranspositionInterval();
    if (transpositionDisp || transpositionAlt) {
        auto transposition = part.ensure_transposition(
            mnxdom::Interval::make(transpositionDisp,
                                music_theory::calc12EdoHalfstepsInInterval(transpositionDisp, transpositionAlt)));
        if (staff->transposition && !staff->transposition->noSimplifyKey && staff->transposition->keysig) {
            transposition.set_keyFifthsFlipAt(7 * music_theory::sign(staff->transposition->keysig->adjust));
        }
        if (music_theory::calcTranspositionIsOctave(transpositionDisp, transpositionAlt)) {
            if (musxMiscOptions->keepWrittenOctaveInConcertPitch) {
                transposition.set_prefersWrittenPitches(true);
            }
        }
    }
}

void createParts(const MnxMusxMappingPtr& context)
{
    const auto scrollView = context->document->getScrollViewStaves(SCORE_PARTID);
    int partNumber = 0;
    auto parts = context->mnxDocument->parts();
    for (const auto& item : scrollView) {
        const auto staff = item->getStaffInstance(1, 0);
        const auto instIt = context->document->getInstruments().find(staff->getCmper());
        if (instIt == context->document->getInstruments().end()) {
            continue;
        }
        const auto& [topStaffId, instInfo] = *instIt;

        std::vector<InstrumentInfo::InstrumentIdentity> identities;
        if (context->denigmaContext->mnxSplitInstruments) {
            logNonBarlineInstrumentChanges(context, instInfo, topStaffId);
            identities = instInfo.getInstrumentIdentities();
        }
        if (identities.empty()) {
            identities.push_back(instInfo.getInstrumentIdentityAt(MusicPoint{}));
        }

        for (const auto& identity : identities) {
            auto partStaff = staff;
            if (context->denigmaContext->mnxSplitInstruments) {
                if (const auto splitChange = findChangeForIdentity(instInfo, identity)) {
                    partStaff = splitChange->second.topStaffComposite;
                }
            }
            std::string id = "P" + std::to_string(++partNumber);
            auto part = parts.append();
            if (context->denigmaContext->mnxSplitInstruments) {
                context->part2SplitInstrumentUuid.emplace(id, identity.instUuid);
            }
            populatePartMetadata(context, part, id, instInfo, partStaff);
            mapPartToInstrumentStaves(context, id, instInfo);
            createMeasures(context, part);
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
