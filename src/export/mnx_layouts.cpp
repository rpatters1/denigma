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
        const std::vector<std::shared_ptr<others::InstrumentUsed>>& scrollViewStaves) : group(inp)
    {
        startSlot = others::InstrumentUsed::getIndexForStaff(scrollViewStaves, inp->startInst);
        endSlot = others::InstrumentUsed::getIndexForStaff(scrollViewStaves, inp->endInst);
    }

    static std::vector<StaffGroupInfo> getGroupsAtMeasure(MeasCmper meas,
        const std::shared_ptr<others::PartDefinition>& linkedPart,
        const std::vector<std::shared_ptr<others::InstrumentUsed>>& scrollViewStaves)
    {
        auto rawGroups = linkedPart->getDocument()->getDetails()->getArray<details::StaffGroup>(
            linkedPart->getCmper(), linkedPart->calcScrollViewIuList());
        std::vector<StaffGroupInfo> retval;
        for (const auto& rawGroup : rawGroups) {
            if (rawGroup->startMeas <= meas && rawGroup->endMeas >= meas) {
                StaffGroupInfo group(rawGroup, scrollViewStaves);
                if (group.startSlot && group.endSlot) {
                    retval.emplace_back(std::move(group));
                }
            }
        }
        return retval;
    }
};

// Helper function to create a JSON representation of a single staff
static json buildStaffJson(const MnxMusxMappingPtr& context, const std::shared_ptr<others::InstrumentUsed>& staff)
{
    json staffJson = json::object();
    staffJson["type"] = "staff";
    staffJson["sources"] = json::array();

    json sourceJson = json::object();
    auto it = context->inst2Part.find(staff->staffId);
    if (it == context->inst2Part.end()) {
        throw std::logic_error("Staff id " + std::to_string(staff->staffId) + " is not accounted for in the MNX part map.");
    }
    sourceJson["part"] = it->second;
    /** @todo All the rest of this */
    /*
    if (staff->hasStemDirection()) {
        sourceJson["stem"] = staff->getStemDirection();
    }
    if (staff->hasLabelRef()) {
        sourceJson["labelref"] = staff->getLabelRef();
    }
    if (staff->hasStaffNumber()) {
        sourceJson["staff"] = staff->getStaffNumber();
    }
    if (staff->hasVoiceNumber()) {
        sourceJson["voice"] = staff->getVoiceName();
    }
    */

    staffJson["sources"].emplace_back(sourceJson);

    /** @todo:
    if (staff->hasLabel()) {
        staffJson["label"] = staff->getLabel();
    }
    */
    
    return staffJson;
}

static json buildOrderedContent(
    const MnxMusxMappingPtr& context,
    const std::vector<StaffGroupInfo>& groups,
    const std::vector<std::shared_ptr<others::InstrumentUsed>>& scrollViewStaves,
    size_t fromIndex = 0,
    size_t toIndex = std::numeric_limits<size_t>::max(),
    size_t groupIndex = 0)
{
    json retval = json::array();
    size_t index = fromIndex;
    while (index < scrollViewStaves.size()) {
        // Skip groups that have already ended
        while (groupIndex < groups.size() && *groups[groupIndex].endSlot < index) {
            groupIndex++;
        }
        if (groupIndex < groups.size() && index >= *groups[groupIndex].startSlot && index <= *groups[groupIndex].endSlot) {
            json groupContent = json::object();
            groupContent["type"] = "group";
            // @todo add group properties to content
            groupContent["content"] = buildOrderedContent(context, groups, scrollViewStaves, index, *groups[groupIndex].endSlot, groupIndex + 1);
            retval.emplace_back(groupContent);
            index = (std::max)(*groups[groupIndex].endSlot + 1, index);
        } else {
            retval.emplace_back(buildStaffJson(context, scrollViewStaves[index++]));
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
        auto layout = json::object();
        layout["id"] = scrollViewLayoutId(linkedPart->getCmper());

        // Retrieve staff groups and staves in scroll view order.
        auto scrollViewStaves = context->document->getOthers()->getArray<others::InstrumentUsed>(
            linkedPart->getCmper(), linkedPart->calcScrollViewIuList());
        std::vector<StaffGroupInfo> groups = StaffGroupInfo::getGroupsAtMeasure(1, linkedPart, scrollViewStaves);
        sortGroups(groups);
        // Create a sequential content array.
        layout["content"] = buildOrderedContent(context, groups, scrollViewStaves);

        // Add the layout to the MNX document.
        if (mnxDocument["layouts"].empty()) {
            mnxDocument["layouts"] = json::array();
        }
        mnxDocument["layouts"].emplace_back(layout);
    }
}

} // namespace mnx
} // namespace denigma
