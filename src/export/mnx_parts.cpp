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

static void createClefs(
    const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const std::shared_ptr<others::Measure>& musxMeasure,
    const std::shared_ptr<others::StaffComposite>& musxStaff,
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
            if (!mnxMeasure.clefs().has_value()) {
                mnxMeasure.create_clefs();
            }
            /// @todo get staff position smarter
            int staffPosition = [&]() {
                switch (clefInfo->first) {
                    case mnx::ClefSign::CClef: return 0;
                    case mnx::ClefSign::FClef: return 2;
                    case mnx::ClefSign::GClef: return -2;
                }
                return 0;
            }();
            std::optional<int> octave = clefInfo->second ? std::optional<int>(clefInfo->second) : std::nullopt;
            auto mnxClef = mnxMeasure.clefs().value().append(staffPosition, clefInfo->first, octave);
            if (location) {
                mnxClef.create_position(location.numerator(), location.denominator());
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
    
    if (auto gfhold = musxDocument->getDetails()->get<details::GFrameHold>(musxMeasure->getPartId(), musxStaff->getCmper(), musxMeasure->getCmper())) {
        if (gfhold->clefId.has_value()) {
            addClef(gfhold->clefId.value(), 0);
        } else {
            auto clefList = musxDocument->getOthers()->getArray<others::ClefList>(musxMeasure->getPartId(), gfhold->clefListId);
            for (const auto& clefItem : clefList) {
                addClef(clefItem->clefIndex, musx::util::Fraction::fromEdu(clefItem->xEduPos));
            }
        }
    } else if (musxMeasure->getCmper() == 1) {
        addClef(musxStaff->calcFirstClefIndex(), 0);
    }
}

static void createMeasures(const MnxMusxMappingPtr& context, mnx::Part& part)
{
    auto& musxDocument = context->document;

    // Retrieve the linked parts in order.
    auto musxMeasures = musxDocument->getOthers()->getArray<others::Measure>(SCORE_PARTID);
    auto mnxMeasures = part.create_measures();
    const auto it = context->part2Inst.find(part.id().value_or(""));
    std::vector<InstCmper> staves;
    if (it == context->part2Inst.end() || it->second.empty()) {
        context->denigmaContext->logMessage(LogMsg() << "Part Id " << part.id().value_or(std::to_string(part.calcArrayIndex()))
            << " is not mapped", LogSeverity::Warning);
    } else {
        staves = it->second;
    }
    std::vector<std::optional<ClefIndex>> prevClefs;
    for (size_t x = 0; x < staves.size(); x++) {
        prevClefs.push_back(std::nullopt);
    }
    for ([[maybe_unused]] const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxMeasures.append();
        for (size_t x = 0; x < staves.size(); x++) {
            auto musxStaff = musx::dom::others::StaffComposite::createCurrent(musxDocument, SCORE_PARTID, staves[x], musxMeasure->getCmper(), 0);
            if (!musxStaff) {
                context->denigmaContext->logMessage(LogMsg() << "Part Id " << part.id().value_or(std::to_string(part.calcArrayIndex()))
                    << " has no staff information for staff " << staves[x], LogSeverity::Warning);
                continue;
            }
            std::optional<int> staffNumber = (staves.size() > 1) ? std::optional<int>(x + 1) : std::nullopt;
            createClefs(context, mnxMeasure, staffNumber, musxMeasure, musxStaff, prevClefs[x]);
        }
    }
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
            part.set_name(fullName);
        }
        auto abrvName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
        if (!abrvName.empty()) {
            part.set_shortName(abrvName);
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
