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

#include "musicxml.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils/stringutils.h"

#include "mx/api/PartData.h"
#include "mx/api/PartGroupData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

void mapPartToInstrumentStaves(MusicXmlMusxMapping& context, const std::string& id, const InstrumentInfo& instInfo)
{
    const auto staves = instInfo.getSequentialStaves();
    if (staves.empty()) {
        return;
    }
    auto& mappedStaves = context.partIdToStaves[id];
    mappedStaves.reserve(staves.size());
    for (StaffCmper staffId : staves) {
        context.staffToPartId.emplace(staffId, id);
        mappedStaves.emplace_back(staffId);
    }
}

void populatePartMetadata(MusicXmlMusxMapping& context, mx::api::PartData& part, const std::string& id, const MusxInstance<others::StaffComposite>& staff)
{
    part.uniqueId = id;
    part.instrumentData.uniqueId = id + "-I1";
    if (const auto soundId = musicXmlSoundIdFromInstrumentUuid(staff->instUuid)) {
        part.instrumentData.soundID = *soundId;
    }

    const auto fullName = utils::trimAscii(staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode));
    if (!fullName.empty()) {
        part.name = utils::trimNewLineFromString(fullName);
        if (!staff->showNamesForPart(context.finaleOptions.forPartId)) {
            part.namePrintObject = mx::api::Bool::no;
        }
        if (part.name != fullName) {
            part.displayName = fullName;
        }
    }

    const auto abbreviatedName = utils::trimAscii(staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode));
    if (!abbreviatedName.empty()) {
        part.abbreviation = utils::trimNewLineFromString(abbreviatedName);
        if (!staff->showNamesForPart(context.finaleOptions.forPartId)) {
            part.abbreviationPrintObject = mx::api::Bool::no;
        }
        if (part.abbreviation != abbreviatedName) {
            part.displayAbbreviation = abbreviatedName;
        }
    }
}

void sortGroups(std::vector<details::StaffGroupInfo>& groups)
{
    std::sort(groups.begin(), groups.end(), [](const details::StaffGroupInfo& lhs, const details::StaffGroupInfo& rhs) {
        if (lhs.startSlot < rhs.startSlot) return true;
        if (lhs.startSlot > rhs.startSlot) return false;
        if (lhs.endSlot > rhs.endSlot) return true;
        if (lhs.endSlot < rhs.endSlot) return false;
        if (lhs.group->bracket && rhs.group->bracket) {
            return lhs.group->bracket->horzAdjLeft < rhs.group->bracket->horzAdjLeft;
        }
        return false;
    });
}

std::unordered_map<StaffCmper, int> createStaffToPartIndex(const MusicXmlMusxMapping& context)
{
    std::unordered_map<StaffCmper, int> result;
    for (int partIndex = 0; partIndex < static_cast<int>(context.musicXmlScore->parts.size()); ++partIndex) {
        const auto& part = context.musicXmlScore->parts.at(static_cast<size_t>(partIndex));
        const auto stavesIt = context.partIdToStaves.find(part.uniqueId);
        if (stavesIt == context.partIdToStaves.end()) {
            continue;
        }
        for (StaffCmper staffId : stavesIt->second) {
            result.emplace(staffId, partIndex);
        }
    }
    return result;
}

void populateGroupNames(mx::api::PartGroupData& groupData, const MusxInstance<details::StaffGroup>& staffGroup)
{
    if (staffGroup->hideName) {
        return;
    }

    const auto fullName = utils::trimAscii(staffGroup->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode));
    if (!fullName.empty()) {
        groupData.name = utils::trimNewLineFromString(fullName);
        groupData.displayName = fullName;
    }

    const auto abbreviatedName = utils::trimAscii(staffGroup->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode));
    if (!abbreviatedName.empty()) {
        groupData.abbreviation = utils::trimNewLineFromString(abbreviatedName);
        groupData.displayAbbreviation = abbreviatedName;
    }
}

void createPartGroups(MusicXmlMusxMapping& context)
{
    auto& score = *context.musicXmlScore;
    score.partGroups.clear();

    const auto scrollView = context.document->getScrollViewStaves(context.forPartId);
    auto groups = details::StaffGroupInfo::getGroupsAtMeasure(1, context.forPartId, scrollView);
    sortGroups(groups);

    const auto staffToPartIndex = createStaffToPartIndex(context);
    int groupNumber = 0;
    for (const auto& groupInfo : groups) {
        if (!groupInfo.startSlot || !groupInfo.endSlot) {
            continue;
        }
        const auto startSlot = groupInfo.startSlot.value();
        const auto endSlot = groupInfo.endSlot.value();
        if (startSlot >= scrollView.size() || endSlot >= scrollView.size()) {
            continue;
        }

        const auto startPartIt = staffToPartIndex.find(scrollView[startSlot]->staffId);
        const auto endPartIt = staffToPartIndex.find(scrollView[endSlot]->staffId);
        if (startPartIt == staffToPartIndex.end() || endPartIt == staffToPartIndex.end()) {
            continue;
        }
        if (startPartIt->second == endPartIt->second) {
            continue;
        }

        auto& partGroup = score.partGroups.emplace_back(mx::api::PartGroupData{});
        partGroup.firstPartIndex = startPartIt->second;
        partGroup.lastPartIndex = endPartIt->second;
        partGroup.number = ++groupNumber;
        partGroup.bracketType = enumConvert<mx::api::BracketType>(groupInfo.group->bracket->style);
        partGroup.groupBarline = enumConvert<mx::api::GroupBarline>(groupInfo.group->drawBarlines);
        populateGroupNames(partGroup, groupInfo.group);
    }
}

} // namespace

void createParts(MusicXmlMusxMapping& context)
{
    auto& parts = context.musicXmlScore->parts;
    parts.clear();
    context.musicXmlScore->partGroups.clear();
    context.staffToPartId.clear();
    context.partIdToStaves.clear();

    const auto scrollView = context.document->getScrollViewStaves(context.forPartId);
    int partNumber = 0;
    for (const auto& item : scrollView) {
        const auto staff = item->getStaffInstance(1, 0);
        const auto instIt = context.document->getInstruments().find(staff->getCmper());
        if (instIt == context.document->getInstruments().end()) {
            continue;
        }

        const auto& [topStaffId, instInfo] = *instIt;
        static_cast<void>(topStaffId);
        const std::string id = createPartId(++partNumber);
        auto& part = parts.emplace_back(mx::api::PartData{});
        populatePartMetadata(context, part, id, staff);
        mapPartToInstrumentStaves(context, id, instInfo);
    }

    createPartGroups(context);
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
