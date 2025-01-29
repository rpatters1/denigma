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

#include "nlohmann/json.hpp"

namespace denigma {
namespace mnx {

struct StaffGroupInfo
{
    std::optional<size_t> startSlot;
    std::optional<size_t> endSlot;
    std::shared_ptr<details::StaffGroup> group;

    StaffGroupInfo(const std::shared_ptr<details::StaffGroup>& inp,
        const std::vector<std::shared_ptr<others::InstrumentUsed>>& systemStaves) : group(inp)
    {
        if (systemStaves.empty()) {
            throw std::logic_error("Attempt to create StaffGroupInfo with no system staves (StaffGroup " + std::to_string(inp->getCmper2()) + ")");
        }
        for (size_t x = 0; x < systemStaves.size(); x++) {
            if (inp->staves.find(systemStaves[x]->staffId) != inp->staves.end()) {
                startSlot = x;
                break;
            }
        }
        if (startSlot) {
            for (size_t x = systemStaves.size() - 1; x >= *startSlot; x--) {
                if (inp->staves.find(systemStaves[x]->staffId) != inp->staves.end()) {
                    endSlot = x;
                    break;
                }
            }
        }
    }

    static std::vector<StaffGroupInfo> getGroupsAtMeasure(MeasCmper measureId,
        const std::shared_ptr<others::PartDefinition>& linkedPart,
        const std::vector<std::shared_ptr<others::InstrumentUsed>>& systemStaves)
    {
        auto rawGroups = linkedPart->getDocument()->getDetails()->getArray<details::StaffGroup>(linkedPart->getCmper(), BASE_SYSTEM_ID);
        std::vector<StaffGroupInfo> retval;
        for (const auto& rawGroup : rawGroups) {
            if (rawGroup->startMeas <= measureId && rawGroup->endMeas >= measureId) {
                StaffGroupInfo group(rawGroup, systemStaves);
                if (group.startSlot && group.endSlot) {
                    retval.emplace_back(std::move(group));
                }
            }
        }
        return retval;
    }
};

// Helper function to create a JSON representation of a single staff
static json buildStaffJson(const MnxMusxMappingPtr& context,
    const std::shared_ptr<others::Measure>& meas,
    const std::shared_ptr<others::InstrumentUsed>& staffSlot)
{
    json staffJson = json::object();
    staffJson["type"] = "staff";
    staffJson["sources"] = json::array();

    json sourceJson = json::object();
    auto it = context->inst2Part.find(staffSlot->staffId);
    if (it == context->inst2Part.end()) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " was not assigned to any MNX part.");
    }
    /// @todo Instead of the raw staff, use a composite of staff and staff styles for the measure here
    auto staff = context->document->getOthers()->get<others::Staff>(staffSlot->getPartId(), staffSlot->staffId);
    if (!staff) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " does not have a Staff instance.");
    }
    sourceJson["part"] = it->second;
    if (auto multiStaffInst = staff->getMultiStaffInstGroup()) {
        if (auto index = multiStaffInst->getIndexOf(staffSlot->staffId)) {
            sourceJson["staff"] = *index + 1;
        }
    }
    if (staff->showNamesForPart(meas->getPartId())) {
        /// @todo if the name has been overridden by a staff style, use "label" instead of "labelref"
        if (meas->calcShouldShowFullNames()) {
            sourceJson["labelref"] = "name";
        } else {
            sourceJson["labelref"] = "shortName";
        }
    }
    switch (staff->stemDirection) {
    default:
        break;
    case others::Staff::StemDirection::AlwaysUp:
        sourceJson["stem"] = "up";
        break;
    case others::Staff::StemDirection::AlwaysDown:
        sourceJson["stem"] = "down";
        break;
    }

    /** @todo (not sure if this will ever be done)
    if (staffSlot->hasVoiceNumber()) {
        sourceJson["voice"] = staffSlot->getVoiceName();
    }
    */

    staffJson["sources"].emplace_back(sourceJson);
   
    return staffJson;
}

static json buildOrderedContent(
    const MnxMusxMappingPtr& context,
    const std::vector<StaffGroupInfo>& groups,
    const std::vector<std::shared_ptr<others::InstrumentUsed>>& systemStaves,
    const std::shared_ptr<others::Measure> forMeas,
    size_t fromIndex = 0,
    size_t toIndex = std::numeric_limits<size_t>::max(),
    size_t groupIndex = 0)
{
    json retval = json::array();
    size_t index = fromIndex;
    while (index < systemStaves.size()) {
        // Skip groups that have already ended
        while (groupIndex < groups.size() && *groups[groupIndex].endSlot < index) {
            groupIndex++;
        }
        if (groupIndex < groups.size() && index >= *groups[groupIndex].startSlot && index <= *groups[groupIndex].endSlot) {
            auto group = groups[groupIndex];
            json groupContent = json::object();
            groupContent["type"] = "group";
            if (!group.group->hideName) {
                auto name = forMeas->calcShouldShowFullNames()
                    ? group.group->getFullInstrumentName()
                    : group.group->getAbbreviatedInstrumentName();
                if (!name.empty()) {
                    groupContent["label"] = name;
                }
            }
            if (group.startSlot != group.endSlot || group.group->bracket->showOnSingleStaff) {
                switch (group.group->bracket->style) {
                case details::StaffGroup::BracketStyle::None:
                    break;
                case details::StaffGroup::BracketStyle::PianoBrace:
                    groupContent["symbol"] = "brace";
                    break;
                default:
                    groupContent["symbol"] = "bracket";
                    break;
                }
            }
            // @todo add group properties to content
            groupContent["content"] = buildOrderedContent(context, groups, systemStaves, forMeas, index, *group.endSlot, groupIndex + 1);
            retval.emplace_back(groupContent);
            index = (std::max)(*group.endSlot + 1, index);
        } else {
            retval.emplace_back(buildStaffJson(context, forMeas, systemStaves[index++]));
        }
        if (index > toIndex) {
            break;
        }
    }
    return retval;
}

static void sortGroups(std::vector<StaffGroupInfo>& groups)
{
    std::sort(groups.begin(), groups.end(), [](const StaffGroupInfo& lhs, const StaffGroupInfo& rhs) {
        if (lhs.startSlot < rhs.startSlot) return true; // Earlier start slot comes first
        if (lhs.startSlot > rhs.startSlot) return false;
        if (lhs.endSlot > rhs.endSlot) return true;    // Wider span comes first
        if (lhs.endSlot < rhs.endSlot) return false;
        // Same span: prioritize by leftmost bracket
        if (lhs.group->bracket && rhs.group->bracket) {
            return lhs.group->bracket->horzAdjLeft < rhs.group->bracket->horzAdjLeft;
        }
        return false;
    });
}

void createLayouts(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;

    // Retrieve and sort the linked parts by order.
    auto musxLinkedParts = context->document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
    std::sort(musxLinkedParts.begin(), musxLinkedParts.end(), [](const auto& lhs, const auto& rhs) {
        return lhs->partOrder < rhs->partOrder;
    });

    // Iterate over each linked part and generate layouts.
    for (const auto& linkedPart : musxLinkedParts) {
        Cmper baseSystemIuList = linkedPart->calcSystemIuList(BASE_SYSTEM_ID);
        auto staffSystems = context->document->getOthers()->getArray<others::StaffSystem>(linkedPart->getCmper());
        const SystemCmper minSystem = BASE_SYSTEM_ID;
        const SystemCmper maxSystem = staffSystems.size();
        for (SystemCmper sysId = minSystem; sysId <= maxSystem; sysId++) { //NOTE: unusual loop limits are *on purpose*
            Cmper systemIuList = sysId ? linkedPart->calcSystemIuList(staffSystems[sysId - 1]->getCmper()) : baseSystemIuList;
            if (sysId != BASE_SYSTEM_ID && systemIuList == baseSystemIuList) {
                continue;
            }
            auto layout = json::object();
            layout["id"] = calcSystemLayoutId(linkedPart->getCmper(), sysId);

            // Retrieve staff groups and staves in scroll view order.
            auto systemStaves = context->document->getOthers()->getArray<others::InstrumentUsed>(
                linkedPart->getCmper(), systemIuList);
            const MeasCmper forMeas = sysId ? staffSystems[sysId - 1]->startMeas : 1;
            std::vector<StaffGroupInfo> groups = StaffGroupInfo::getGroupsAtMeasure(forMeas, linkedPart, systemStaves);
            sortGroups(groups);
            // Create a sequential content array.
            auto meas = context->document->getOthers()->get<others::Measure>(linkedPart->getCmper(), forMeas);
            if (!meas) {
                throw std::logic_error("No Measure instance found for measure " + std::to_string(forMeas)
                    + " in linked part " + std::to_string(linkedPart->getCmper()));
            }
            layout["content"] = buildOrderedContent(context, groups, systemStaves, meas);

            // Add the layout to the MNX document.
            if (mnxDocument["layouts"].empty()) {
                mnxDocument["layouts"] = json::array();
            }
            mnxDocument["layouts"].emplace_back(layout);
        }
    }
}

} // namespace mnx
} // namespace denigma
