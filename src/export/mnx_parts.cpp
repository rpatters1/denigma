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

#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

static void createBeams(
    [[maybe_unused]] const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure,
    InstCmper staffCmper)
{
    const auto& musxDocument = musxMeasure->getDocument();
    if (auto gfhold = musxDocument->getDetails()->get<details::GFrameHold>(musxMeasure->getPartId(), staffCmper, musxMeasure->getCmper())) {
        gfhold->iterateEntries([&](const EntryInfoPtr& entryInfo) -> bool {
            auto processBeam = [&](mnx::Array<mnx::part::Beam>&& mnxBeams, unsigned beamNumber, const EntryInfoPtr& firstInBeam, auto&& self) -> void {
                assert(firstInBeam.calcLowestBeamStart() <= beamNumber);
                auto beam = mnxBeams.append();
                for (auto next = firstInBeam; next; next = next.getNextInBeamGroup(/*includeHidden*/true)) {
                    if (next->getEntry()->isHidden) {
                        if (context->visifiedEntries.find(next->getEntry()->getEntryNumber()) == context->visifiedEntries.end()) {
                            continue;
                        }
                    }
                    beam.events().push_back(calcEventId(next->getEntry()->getEntryNumber()));
                    if (unsigned lowestBeamStart = next.calcLowestBeamStart()) {
                        unsigned nextBeamNumber = beamNumber + 1;
                        unsigned lowestBeamStub = next.calcLowestBeamStub();
                        if (lowestBeamStub && lowestBeamStub <= nextBeamNumber && next.calcNumberOfBeams() >= nextBeamNumber) {
                            if (!beam.hooks().has_value()) {
                                beam.create_hooks();
                            }
                            mnx::BeamHookDirection hookDir = next.calcBeamStubIsLeft()
                                                            ? mnx::BeamHookDirection::Left
                                                            : mnx::BeamHookDirection::Right;
                            beam.hooks().value().append(mnx::part::BeamHook::Initializer(calcEventId(next->getEntry()->getEntryNumber()), hookDir));
                        } else if (lowestBeamStart <= nextBeamNumber && next.calcNumberOfBeams() >= nextBeamNumber) {
                            if (!beam.inner().has_value()) {
                                beam.create_inner();
                            }
                            self(beam.inner().value(), nextBeamNumber, next, self);
                        }
                    }
                    if (unsigned lowestBeamEnd = next.calcLowestBeamEnd()) {
                        if (lowestBeamEnd <= beamNumber) {
                            break;
                        }
                    }
                }
            };
            if (entryInfo.calcIsBeamStart()) {
                if (!mnxMeasure.beams().has_value()) {
                    mnxMeasure.create_beams();
                }
                processBeam(mnxMeasure.beams().value(), 1, entryInfo, processBeam);
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
    const std::shared_ptr<others::Measure>& musxMeasure,
    InstCmper staffCmper,
    std::optional<ClefIndex>& prevClefIndex)
{
    const auto& musxDocument = musxMeasure->getDocument();
    auto clefDefs = musxDocument->getOptions()->get<options::ClefOptions>();
    if (!clefDefs) {
        throw std::invalid_argument("Musx document contains no clef options.");
    }

    auto addClef = [&](ClefIndex clefIndex, musx::util::Fraction location) {
        if (clefIndex == prevClefIndex) {
            return;
        }
        if (clefIndex >= ClefIndex(clefDefs->clefDefs.size())) {
            throw std::invalid_argument("Invalid clef index " + std::to_string(clefIndex));
        }
        const auto& musxClef = clefDefs->clefDefs[clefIndex];
        auto& clefFont = musxClef->font;
        if (!clefFont || !musxClef->useOwnFont) {
            clefFont = context->defaultMusicFont;
        }
        FontType fontType = convertFontToType(clefFont);
        if (auto clefInfo = convertCharToClef(musxClef->clefChar, fontType)) {
            auto musxStaff = musx::dom::others::StaffComposite::createCurrent(
                musxDocument, musxMeasure->getPartId(), staffCmper, musxMeasure->getCmper(), location.calcEduDuration());
            if (!musxStaff) {
                context->logMessage(LogMsg() << "Part Id " << mnxPart.id().value_or(std::to_string(mnxPart.calcArrayIndex()))
                    << " has no staff information for staff " << staffCmper, LogSeverity::Warning);
                return;
            }
            if (!mnxMeasure.clefs().has_value()) {
                mnxMeasure.create_clefs();
            }
            int staffPosition = mnxStaffPosition(musxStaff, musxClef->staffPositon);
            std::optional<int> octave = clefInfo->second ? std::optional<int>(clefInfo->second) : std::nullopt;
            auto mnxClef = mnxMeasure.clefs().value().append(staffPosition, clefInfo->first, octave);
            if (location) {
                mnxClef.create_position(mnxFractionFromFraction(location));
            }
            if (mnxStaffNumber) {
                mnxClef.set_staff(mnxStaffNumber.value());
            }
            if (auto metaDataPath = clefFont->calcSMuFLMetaDataPath()) {
                if (auto glyphName = utils::smuflGlyphNameForFont(metaDataPath.value(), musxClef->clefChar, *context->denigmaContext)) {
                    mnxClef.clef().set_glyph(glyphName.value());
                }
            }
            prevClefIndex = clefIndex;
        }
    };
    
    if (auto gfhold = musxDocument->getDetails()->get<details::GFrameHold>(musxMeasure->getPartId(), staffCmper, musxMeasure->getCmper())) {
        if (gfhold->clefId.has_value()) {
            addClef(gfhold->clefId.value(), 0);
        } else {
            auto clefList = musxDocument->getOthers()->getArray<others::ClefList>(musxMeasure->getPartId(), gfhold->clefListId);
            for (const auto& clefItem : clefList) {
                addClef(clefItem->clefIndex, musx::util::Fraction::fromEdu(clefItem->xEduPos));
            }
        }
    } else if (musxMeasure->getCmper() == 1) {
        int firstIndex = others::Staff::calcFirstClefIndex(musxDocument, musxMeasure->getPartId(), staffCmper);
        addClef(ClefIndex(firstIndex), 0);
    }
}

static void createDynamics(const MnxMusxMappingPtr& context, const std::shared_ptr<others::Measure>& musxMeasure, InstCmper staffCmper,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasExpression) {
        auto shapeAssigns = context->document->getOthers()->getArray<others::MeasureExprAssign>(musxMeasure->getPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (asgn->staffAssign == staffCmper && asgn->textExprId && !asgn->hidden) {
                if (auto expr = asgn->getTextExpression()) {
                    if (auto cat = context->document->getOthers()->get<others::MarkingCategory>(expr->getPartId(), expr->categoryId)) {
                        if (cat->categoryType == others::MarkingCategory::CategoryType::Dynamics) {
                            if (auto text = expr->getTextBlock()) {
                                if (auto rawText = text->getRawTextBlock()) {
                                    /// @note This block is a placeholder until the mnx::Dynamic object is better defined.
                                    auto fontInfo = rawText->parseFirstFontInfo();
                                    std::string dynamicText = text->getText(true, musx::util::EnigmaString::AccidentalStyle::Unicode);
                                    if (!mnxMeasure.dynamics().has_value()) {
                                        mnxMeasure.create_dynamics();
                                    }
                                    auto mnxDynamic = mnxMeasure.dynamics().value().append(dynamicText, mnxFractionFromEdu(asgn->eduPosition));
                                    if (auto smuflGlyph = utils::smuflGlyphNameForFont(fontInfo, dynamicText, *context->denigmaContext)) {
                                        mnxDynamic.set_glyph(smuflGlyph.value());
                                    }
                                    if (mnxStaffNumber) {
                                        mnxDynamic.set_staff(mnxStaffNumber.value());
                                    }
                                    if (asgn->layer > 0) {
                                        mnxDynamic.set_voice(calcVoice(asgn->layer - 1, 1));
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

static void createOttavas(const MnxMusxMappingPtr& context, const std::shared_ptr<others::Measure>& musxMeasure, InstCmper staffCmper,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    context->ottavasApplicableInMeasure.clear();
    if (musxMeasure->hasSmartShape) {
        auto shapeAssigns = context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(asgn->getPartId(), asgn->shapeNum)) {
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
                        if (!mnxMeasure.ottavas().has_value()) {
                            mnxMeasure.create_ottavas();
                        }
                        auto mnxOttava = mnxMeasure.ottavas().value().append(
                            enumConvert<mnx::OttavaAmount>(shape->shapeType),
                            mnxFractionFromSmartShapeEndPoint(shape->startTermSeg->endPoint),
                            shape->endTermSeg->endPoint->measId,
                            mnxFractionFromSmartShapeEndPoint(shape->endTermSeg->endPoint)
                        );
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
        context->partStaves = it->second;
    }
    std::vector<std::optional<ClefIndex>> prevClefs;
    for (size_t x = 0; x < context->partStaves.size(); x++) {
        prevClefs.push_back(std::nullopt);
    }
    for (const auto& musxMeasure : musxMeasures) {
        context->currMeas++;
        auto mnxMeasure = mnxMeasures.append();
        for (size_t x = 0; x < context->partStaves.size(); x++) {
            context->currStaff = context->partStaves[x];
            context->currMeasDura = musxMeasure->createTimeSignature()->calcTotalDuration(); /// @todo pass in staff if MNX ever supports ind. time sig per staff
            std::optional<int> staffNumber = (context->partStaves.size() > 1) ? std::optional<int>(int(x) + 1) : std::nullopt;
            createClefs(context, part, mnxMeasure, staffNumber, musxMeasure, context->partStaves[x], prevClefs[x]);
            createDynamics(context, musxMeasure, context->partStaves[x], mnxMeasure, staffNumber);
            createOttavas(context, musxMeasure, context->partStaves[x], mnxMeasure, staffNumber);
            context->visifiedEntries.clear();
            createSequences(context, mnxMeasure, staffNumber, musxMeasure, context->partStaves[x]);
            createBeams(context, mnxMeasure, musxMeasure, context->partStaves[x]);
        }
    }
    context->clearCounts();
}

void createParts(const MnxMusxMappingPtr& context)
{
    auto multiStaffInsts = context->document->getOthers()->getArray<others::MultiStaffInstrumentGroup>(SCORE_PARTID);
    auto scrollView = context->document->getOthers()->getArray<others::InstrumentUsed>(SCORE_PARTID, BASE_SYSTEM_ID);
    int partNumber = 0;
    auto parts = context->mnxDocument->parts();
    for (const auto& item : scrollView) {
        auto staff = item->getStaff();
        auto multiStaffInst = staff->getMultiStaffInstGroup();
        if (multiStaffInst && context->inst2Part.find(staff->getCmper()) != context->inst2Part.end()) {
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
        if (multiStaffInst) {
            part.set_staves(int(multiStaffInst->staffNums.size()));
            for (auto inst : multiStaffInst->staffNums) {
                context->inst2Part.emplace(inst, id);
            }
            context->part2Inst.emplace(id, multiStaffInst->staffNums);
        } else {
            context->inst2Part.emplace(staff->getCmper(), id);
            context->part2Inst.emplace(id, std::vector<InstCmper>({ InstCmper(staff->getCmper()) }));
        }
        createMeasures(context, part);
    }
}

} // namespace mnxexp
} // namespace denigma
