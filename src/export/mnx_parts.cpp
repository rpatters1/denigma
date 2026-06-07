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
#include "classify/clefs.h"
#include "classify/dynamics.h"

namespace denigma {
namespace mnxexp {

static void createBeams(
    const MnxMusxMappingPtr& context,
    mnx::part::Measure mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const auto mnxMeasures = mnxMeasure.parent<mnx::Array<mnx::part::Measure>>();
    const auto partId = musxMeasure->getRequestedPartId();
    if (context->current.gfhold) {
        if (context->current.cueDiscardPlan.discardWholeHold) {
            return; // skip cues until MNX spec includes them
        }
        auto processEntry = [&](const EntryInfoPtr& entryInfo) -> bool {
            auto processBeam = [&](mnx::Array<mnx::part::Beam>&& mnxBeams, unsigned beamNumber, const EntryInfoPtr& firstInBeam, auto&& self) -> void {
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
                                    mnx::BeamHookDirection hookDir = manual->isLeft()
                                                                     ? mnx::BeamHookDirection::Left
                                                                     : mnx::BeamHookDirection::Right;
                                    hookBeam.set_direction(hookDir);
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
            if (context->beamedEntries.find(entryInfo->getEntry()->getEntryNumber()) != context->beamedEntries.end()) {
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
            if (context->current.cueDiscardPlan.skipsLayer(layer)) {
                continue;
            }
            if (auto entryFrame = context->current.gfhold->createEntryFrame(layer)) {
                entryFrame->iterateEntries(processEntry);
            }
        }
    }
}

static std::optional<ClefIndex> createClef(
    const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    ClefIndex clefIndex,
    musx::util::Fraction location,
    const MusxInstance<others::Staff>& musxStaff)
{
    MUSX_ASSERT_IF(!musxStaff) {
        context->logMessage(LogMsg() << "invalid or unmapped staff passed to createClef", LogSeverity::Warning);
        return std::nullopt;
    }
    const auto& musxClef = context->clefOptions->getClefDef(clefIndex);
    const auto clef = classify::classifyClef(musxClef, musxStaff);
    std::optional<mnx::ClefSign> clefSign;
    if (clef && !clef.isBlank && std::abs(clef.octave) <= 3) {
        switch (clef.type) {
        case music_theory::ClefType::G: clefSign = mnx::ClefSign::GClef; break;
        case music_theory::ClefType::C: clefSign = mnx::ClefSign::CClef; break;
        case music_theory::ClefType::F: clefSign = mnx::ClefSign::FClef; break;
        /// @todo handle Percussion and Tab cases when defined in mnx spec
        default: break;
        }
    }
    if (clefSign) {
        int staffPosition = mnxStaffPosition(musxStaff, musxClef->staffPosition);
        auto mnxClef = mnxMeasure.ensure_clefs().append(clefSign.value(), staffPosition, mnx::OttavaAmountOrZero(clef.octave));
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
            << " Clef change was skipped.", LogSeverity::Warning);
    }
    return std::nullopt;
};

static void createClefs(
    const MnxMusxMappingPtr& context,
    const mnx::Part& mnxPart,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    std::optional<ClefIndex>& prevClefIndex)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const StaffCmper staffCmper = context->current.staff;

    auto addClef = [&](ClefIndex clefIndex, musx::util::Fraction location) {
        if (clefIndex == prevClefIndex) {
            return;
        }
        auto musxStaff = musx::dom::others::StaffComposite::createCurrent(
            musxDocument, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), location.calcEduDuration());
        if (!musxStaff) {
            context->logMessage(LogMsg() << mnxPartDisplayName(context, mnxPart)
                << " has no staff information for staff " << staffCmper, LogSeverity::Warning);
            return;
        }
        if (auto newClefIndex = createClef(context, mnxMeasure, mnxStaffNumber, clefIndex, location, musxStaff)) {
            prevClefIndex = newClefIndex;
        }
    };

    auto staff = others::StaffComposite::createCurrent(musxDocument, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), 0);
    if (staff && staff->transposition && staff->transposition->setToClef) {
        addClef(staff->transposedClef, 0);
    } else if (auto gfhold = musxDocument->getDetails()->get<details::GFrameHold>(musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper())) {
        if (gfhold->clefId.has_value()) {
            addClef(gfhold->clefId.value(), 0);
        } else {
            auto clefList = musxDocument->getOthers()->getArray<others::ClefList>(musxMeasure->getRequestedPartId(), gfhold->clefListId);
            const auto ctx = details::GFrameHoldContext(gfhold);
            for (const auto& clefItem : clefList) {
                const auto location = ctx.snapLocationToEntryOrKeep(musx::util::Fraction::fromEdu(clefItem->xEduPos), /*findExact*/ true);
                addClef(clefItem->clefIndex, location);
            }
        }
    }
}

static void processExpressions(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    const StaffCmper staffCmper = context->current.staff;
    struct ExpressionAttachmentContext {
        EntryInfoPtr entryInfo;
        const EntryTarget* entryTarget{ nullptr };
    };

    auto calcAttachmentContext = [&](const MusxInstance<others::MeasureExprAssign>& asgn) {
        ExpressionAttachmentContext result;
        result.entryInfo = asgn->calcAssociatedEntry();
        if (!result.entryInfo) {
            return result;
        }
        const auto entryNumber = result.entryInfo->getEntry()->getEntryNumber();
        const auto entryIt = context->entryTargetByNumber.find(entryNumber);
        if (entryIt != context->entryTargetByNumber.end()) {
            result.entryTarget = &entryIt->second;
        }
        return result;
    };

    auto appendDynamic = [&](const MusxInstance<others::MeasureExprAssign>& asgn, const musx::util::EnigmaParsingContext& exprCtx, const classify::DynamicClassification& dynamicClass) {
        /// @note This block is a placeholder until the mnx::Dynamic object is better defined.
        const auto typeStr = classify::dynamicCanonicalText(dynamicClass.dynamic);
        if (typeStr.empty()) {
            return;
        }
        auto mnxDynamic = mnxMeasure.ensure_dynamics().append(
            typeStr, mnxFractionFromEdu(asgn->eduPosition));
        auto fontInfo = exprCtx.parseFirstFontInfo();
        std::string exprText = exprCtx.getText(true, musx::util::EnigmaString::AccidentalStyle::Unicode);
        if (auto smuflGlyph = utils::smuflGlyphNameForFont(fontInfo, exprText)) {
            mnxDynamic.set_glyph(std::string(smuflGlyph.value()));
        }
        if (mnxStaffNumber) {
            mnxDynamic.set_staff(mnxStaffNumber.value());
        }
        if (asgn->layer > 0) {
            mnxDynamic.set_voice(calcVoice(mnxStaffNumber.value_or(1), asgn->layer - 1, 1));
        }
    };

    auto attachFermata = [&](const MusxInstance<others::MeasureExprAssign>& asgn, const MusxInstance<others::TextExpressionDef>& expr,
                             const ExpressionAttachmentContext& attachment, const mnx::Fermata& fermata) {
        if (asgn->calcIsPartOfStaffListAssignment() || expr->horzMeasExprAlign == others::HorizontalMeasExprAlign::RightBarline) {
            return;
        }
        if (attachment.entryTarget) {
            switch (attachment.entryTarget->kind) {
            case EntryTargetKind::Event:
                mnx::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
                break;
            case EntryTargetKind::FullMeasureRest:
                mnx::sequence::FullMeasureRest(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
                break;
            }
        } else if (attachment.entryInfo) {
            context->logMessage(LogMsg() << "Entry " << attachment.entryInfo->getEntry()->getEntryNumber()
                << " was not mapped to an event or full measure rest", LogSeverity::Warning);
        } else {
            if (mnxMeasure.sequences().empty()) {
                mnxMeasure.sequences().append();
            }
            for (auto seq : mnxMeasure.sequences()) {
                if (seq.content().empty()) {
                    auto fullMeasureRest = seq.ensure_fullMeasure();
                    fullMeasureRest.set_fermata(fermata);
                }
            }
        }
    };

    auto attachBreathMark = [&](const ExpressionAttachmentContext& attachment, const mnx::sequence::BreathMark& breathMark) {
        if (attachment.entryTarget && attachment.entryTarget->kind == EntryTargetKind::Event) {
            mnx::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).ensure_markings().set_breath(breathMark);
        }
    };

    if (musxMeasure->hasExpression) {
        auto exprAssigns = context->document->getOthers()->getArray<others::MeasureExprAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : exprAssigns) {
            if (!asgn->calcIsAssignedInRequestedPart()) {
                continue;
            }
            if (asgn->staffAssign == staffCmper && !asgn->hidden) {
                if (asgn->textExprId) {
                    if (auto expr = asgn->getTextExpression()) {
                        if (auto text = expr->getTextBlock()) {
                            if (auto rawTextCtx = text->getRawTextCtx(SCORE_PARTID)) {
                                /// @todo Add calculation for above or below, rather than just Float, for marking placement
                                if (auto dynamicClass = classify::classifyDynamic(expr)) {
                                    appendDynamic(asgn, rawTextCtx, dynamicClass);
                                } else if (auto fermata = calcFermata(rawTextCtx)) {
                                    attachFermata(asgn, expr, calcAttachmentContext(asgn), fermata.value());
                                } else if (auto breathMark = calcBreathMark(rawTextCtx)) {
                                    attachBreathMark(calcAttachmentContext(asgn), breathMark.value());
                                }
                            } else {
                                context->logMessage(LogMsg() << "Text block " << text->getCmper() << " has non-existent raw text block " << text->textId, LogSeverity::Warning);
                            }
                        } else {
                            context->logMessage(LogMsg() << "Text expression " << expr->getCmper() << " has non-existent text block " << expr->textIdKey, LogSeverity::Warning);
                        }
                    }
                }
                if (asgn->shapeExprId) {                    
                    if (const auto candidate = musx::util::calcNonArpeggioSpanForAssignment(asgn)) {
                        appendArpeggioCandidate(context, mnxMeasure, candidate.value());
                    }
                }
            }
        }
    }
}
static void processSmartShapes(
    const MnxMusxMappingPtr& context,
    mnx::part::Measure mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    const StaffCmper staffCmper = context->current.staff;
    if (musxMeasure->hasSmartShape) {
        const auto assigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& assign : assigns) {
            MUSX_ASSERT_IF(!assign) {
                context->logMessage(LogMsg() << "skipping empty smart shape assignment for measure " << musxMeasure->getCmper(), LogSeverity::Warning);
                continue;
            }
            if (assign->centerShapeNum != 0) {
                // ignore assignments in the middle of the shape
                continue;
            }
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum)) {
                if (shape->startTermSeg->endPoint->staffId == staffCmper) {
                    if (const auto nonArpeggio = musx::util::calcNonArpeggioSpanForSmartShape(shape)) {
                        appendArpeggioCandidate(context, mnxMeasure, nonArpeggio.value());
                    }
                }
            }
        }
    }
}

static void createOttavas(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    const StaffCmper staffCmper = context->current.staff;
    context->current.ottavasApplicableInMeasure.clear();
    if (musxMeasure->hasSmartShape) {
        auto shapeAssigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(asgn->getRequestedPartId(), asgn->shapeNum)) {
                if (!shape->hidden && (shape->startTermSeg->endPoint->staffId == staffCmper || shape->endTermSeg->endPoint->staffId == staffCmper)) {
                    switch (shape->shapeType) {
                        default: continue;
                        case others::SmartShape::ShapeType::OctaveDown:
                        case others::SmartShape::ShapeType::OctaveUp:
                        case others::SmartShape::ShapeType::TwoOctaveDown:
                        case others::SmartShape::ShapeType::TwoOctaveUp:
                            break;
                    }
                    context->current.ottavasApplicableInMeasure.emplace(shape->getCmper(), shape);
                    if (!asgn->centerShapeNum && shape->startTermSeg->endPoint->measId == musxMeasure->getCmper()) {
                        auto mnxOttava = mnxMeasure.ensure_ottavas().append(
                            enumConvert<mnx::OttavaAmount>(shape->shapeType),
                            mnxFractionFromSmartShapeEndPoint(shape->startTermSeg->endPoint),
                            calcGlobalMeasureId(shape->endTermSeg->endPoint->measId),
                            mnxFractionFromSmartShapeEndPoint(shape->endTermSeg->endPoint));
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

static MusxInstance<others::StaffComposite> findCompositeForIdentity(
    const InstrumentInfo& instInfo,
    const InstrumentInfo::InstrumentIdentity& identity)
{
    for (const auto& [point, change] : instInfo.getChanges()) {
        static_cast<void>(point);
        if (change.identity == identity) {
            return change.topStaffComposite;
        }
    }
    return nullptr;
}

static void createMeasures(const MnxMusxMappingPtr& context, mnx::Part& part)
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
        context->logMessage(LogMsg() << mnxPartDisplayName(context, part) << " is not mapped", LogSeverity::Warning);
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
            if (context->currSplitInstrumentUuid) {
                const auto& instInfo = context->document->getInstrumentForStaff(staffCmper);
                const auto identity = instInfo.getInstrumentIdentityAt(MusicPoint(musxMeasure->getCmper(), musx::util::Fraction{}));
                if (musxMeasure->getCmper() == 1) {
                    const auto musxStaff = findCompositeForIdentity(instInfo, identity);
                    prevClefs[x] = createClef(context, mnxMeasure, staffNumber, musxStaff->calcClefIndexAt(1, 0, /*forWrittenPitch*/ true), 0, musxStaff);
                }
                if (identity.instUuid != context->currSplitInstrumentUuid.value()) {
                    continue;
                }
            } else if (musxMeasure->getCmper() == 1) {
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
            processSmartShapes(context, mnxMeasure, musxMeasure);
        }
    }
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
                LogSeverity::Warning);
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
    mnx::Part& part,
    const std::string& id,
    const InstrumentInfo& instInfo,
    const MusxInstance<others::StaffComposite>& staff)
{
    auto musxMiscOptions = context->document->getOptions()->get<options::MiscOptions>();

    part.set_id(id);
    auto fullName = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!fullName.empty()) {
        part.set_name(trimNewLineFromString(fullName));
    }
    auto abrvName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!abrvName.empty()) {
        part.set_shortName(trimNewLineFromString(abrvName));
    }
    if (instInfo.staves.size() > 1) {
        part.set_staves(int(instInfo.staves.size()));
    }
    auto [transpositionDisp, transpositionAlt] = staff->calcTranspositionInterval();
    if (transpositionDisp || transpositionAlt) {
        auto transposition = part.ensure_transposition(
            mnx::Interval::make(transpositionDisp,
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
                if (auto splitStaff = findCompositeForIdentity(instInfo, identity)) {
                    partStaff = splitStaff;
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

} // namespace mnxexp
} // namespace denigma
