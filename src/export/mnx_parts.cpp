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
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

static void createBeams(
    [[maybe_unused]] const MnxMusxMappingPtr& context,
    mnx::part::Measure mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffCmper)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const auto mnxMeasures = mnxMeasure.parent<mnx::Array<mnx::part::Measure>>();
    const auto partId = musxMeasure->getRequestedPartId();
    if (auto gfhold = details::GFrameHoldContext(musxDocument, partId, staffCmper, musxMeasure->getCmper())) {
        if (gfhold.calcIsCuesOnly()) {
            return; // skip cues until MNX spec includes them
        }
        gfhold.iterateEntries([&](const EntryInfoPtr& entryInfo) -> bool {
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
        });
    }
}

static void createClefs(
    const MnxMusxMappingPtr& context,
    const mnx::Part& mnxPart,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffCmper,
    std::optional<ClefIndex>& prevClefIndex)
{
    const auto& musxDocument = musxMeasure->getDocument();
    auto clefOptions = musxDocument->getOptions()->get<options::ClefOptions>();
    if (!clefOptions) {
        throw std::invalid_argument("Musx document contains no clef options.");
    }

    auto addClef = [&](ClefIndex clefIndex, musx::util::Fraction location) {
        if (clefIndex == prevClefIndex) {
            return;
        }
        auto musxStaff = musx::dom::others::StaffComposite::createCurrent(
            musxDocument, musxMeasure->getRequestedPartId(), staffCmper, musxMeasure->getCmper(), location.calcEduDuration());
        if (!musxStaff) {
            context->logMessage(LogMsg() << "Part Id " << mnxPart.id().value_or(std::to_string(mnxPart.calcArrayIndex()))
                << " has no staff information for staff " << staffCmper, LogSeverity::Warning);
            return;
        }
        const auto& musxClef = clefOptions->getClefDef(clefIndex);
        auto clefFont = musxClef->calcFont();
        auto glyphName = utils::smuflGlyphNameForFont(clefFont, musxClef->clefChar);
        if (auto clefInfo = mnxClefInfoFromClefDef(musxClef, musxStaff, glyphName)) {
            auto [clefSign, octave, hideOctave] = clefInfo.value();
            int staffPosition = mnxStaffPosition(musxStaff, musxClef->staffPosition);
            auto mnxClef = mnxMeasure.ensure_clefs().append(clefSign, staffPosition, octave);
            if (location) {
                mnxClef.ensure_position(mnxFractionFromFraction(location));
            }
            if (hideOctave) {
                mnxClef.clef().set_showOctave(false);
            }
            if (mnxStaffNumber) {
                mnxClef.set_staff(mnxStaffNumber.value());
            }
            if (glyphName) {
                mnxClef.clef().set_glyph(glyphName.value());
            }
            prevClefIndex = clefIndex;
        } else {
            context->logMessage(LogMsg() << "Clef char " << int(musxClef->clefChar) << " has no clef info. " << " (glyph name is " << glyphName.value_or("") << ")"
                << " Clef change was skipped.", LogSeverity::Warning);
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
    } else if (musxMeasure->getCmper() == 1) {
        ClefIndex firstIndex = others::Staff::calcFirstClefIndex(musxDocument, musxMeasure->getRequestedPartId(), staffCmper);
        addClef(firstIndex, 0);
    }
}

static void createDynamics(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffCmper,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasExpression) {
        auto exprAssigns = context->document->getOthers()->getArray<others::MeasureExprAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : exprAssigns) {
            if (!asgn->calcIsAssignedInRequestedPart()) {
                continue;
            }
            if (asgn->staffAssign == staffCmper && asgn->textExprId && !asgn->hidden) {
                if (auto expr = asgn->getTextExpression()) {
                    if (auto cat = context->document->getOthers()->get<others::MarkingCategory>(expr->getRequestedPartId(), expr->categoryId)) {
                        if (cat->categoryType == others::MarkingCategory::CategoryType::Dynamics) { /// @todo: be smarter about identifying dynamics
                            if (auto text = expr->getTextBlock()) {
                                if (auto rawTextCtx = text->getRawTextCtx(SCORE_PARTID)) {
                                    /// @note This block is a placeholder until the mnx::Dynamic object is better defined.
                                    auto fontInfo = rawTextCtx.parseFirstFontInfo();
                                    std::string dynamicText = rawTextCtx.getText(true, musx::util::EnigmaString::AccidentalStyle::Unicode);
                                    auto mnxDynamic = mnxMeasure.ensure_dynamics().append(
                                        dynamicText, mnxFractionFromEdu(asgn->eduPosition));
                                    if (auto smuflGlyph = utils::smuflGlyphNameForFont(fontInfo, dynamicText)) {
                                        mnxDynamic.set_glyph(std::string(smuflGlyph.value()));
                                    }
                                    if (mnxStaffNumber) {
                                        mnxDynamic.set_staff(mnxStaffNumber.value());
                                    }
                                    if (asgn->layer > 0) {
                                        mnxDynamic.set_voice(calcVoice(mnxStaffNumber.value_or(1), asgn->layer - 1, 1));
                                    }
                                } else {
                                    context->logMessage(LogMsg() << "Text block " << text->getCmper() << " has non-existent raw text block " << text->textId, LogSeverity::Warning);
                                }
                            }  else {
                                context->logMessage(LogMsg() << "Text expression " << expr->getCmper() << " has non-existent text block " << expr->textIdKey, LogSeverity::Warning);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void createOttavas(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffCmper,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    context->ottavasApplicableInMeasure.clear();
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
                    context->ottavasApplicableInMeasure.emplace(shape->getCmper(), shape);
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

static void createMeasures(const MnxMusxMappingPtr& context, mnx::Part& part)
{
    auto& musxDocument = context->document;
    context->clearCounts();

    // Retrieve the linked parts in order.
    auto musxMeasures = musxDocument->getOthers()->getArray<others::Measure>(SCORE_PARTID);
    auto mnxMeasures = part.create_measures();
    const auto it = context->part2Inst.find(part.id().value_or(""));
    if (it == context->part2Inst.end() || it->second.empty()) {
        context->logMessage(LogMsg() << "Part Id " << part.id().value_or(std::to_string(part.calcArrayIndex()))
            << " is not mapped", LogSeverity::Warning);
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
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxMeasures.at(context->currMeas++);
        for (size_t x = 0; x < context->currPartStaves.size(); x++) {
            context->currStaff = context->currPartStaves[x];
            std::optional<int> staffNumber = (context->currPartStaves.size() > 1) ? std::optional<int>(int(x) + 1) : std::nullopt;
            createBeams(context, mnxMeasure, musxMeasure, context->currPartStaves[x]);
            createClefs(context, part, mnxMeasure, staffNumber, musxMeasure, context->currPartStaves[x], prevClefs[x]);
            createDynamics(context, musxMeasure, context->currPartStaves[x], mnxMeasure, staffNumber);
            createOttavas(context, musxMeasure, context->currPartStaves[x], mnxMeasure, staffNumber);
            createSequences(context, mnxMeasure, staffNumber, musxMeasure, context->currPartStaves[x]);
        }
    }
    context->clearCounts();
}

void createParts(const MnxMusxMappingPtr& context)
{
    auto musxMiscOptions = context->document->getOptions()->get<options::MiscOptions>();
    auto multiStaffInsts = context->document->getOthers()->getArray<others::MultiStaffInstrumentGroup>(SCORE_PARTID);
    const auto scrollView = context->document->getScrollViewStaves(SCORE_PARTID);
    int partNumber = 0;
    auto parts = context->mnxDocument->parts();
    for (const auto& item : scrollView) {
        const auto staff = item->getStaffInstance(1, 0);
        const auto instIt = context->document->getInstruments().find(staff->getCmper());
        if (instIt == context->document->getInstruments().end()) {
            continue;
        }
        std::string id = "P" + std::to_string(++partNumber);
        auto part = parts.append();
        part.set_id(id);
        auto fullName = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (!fullName.empty()) {
            part.set_name(trimNewLineFromString(fullName));
        }
        auto abrvName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (!abrvName.empty()) {
            part.set_shortName(trimNewLineFromString(abrvName));
        }
        const auto& [topStaffId, instInfo] = *instIt;
        if (instInfo.staves.size() > 1) {
            part.set_staves(int(instInfo.staves.size()));
            for (const auto& [staffId, index] : instInfo.staves) {
                context->inst2Part.emplace(staffId, id);
            }
            context->part2Inst.emplace(id, instInfo.getSequentialStaves());
        } else {
            context->inst2Part.emplace(staff->getCmper(), id);
            context->part2Inst.emplace(id, std::vector<StaffCmper>({ StaffCmper(staff->getCmper()) }));
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
        createMeasures(context, part);
    }
}

} // namespace mnxexp
} // namespace denigma
