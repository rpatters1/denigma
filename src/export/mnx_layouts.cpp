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

namespace denigma {
namespace mnxexp {

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
static void buildMnxStaff(mnx::layout::Staff&& mnxStaff,
    const MnxMusxMappingPtr& context,
    const std::shared_ptr<others::Measure>& meas,
    const std::shared_ptr<others::InstrumentUsed>& staffSlot)
{
    auto it = context->inst2Part.find(staffSlot->staffId);
    if (it == context->inst2Part.end()) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " was not assigned to any MNX part.");
    }
    auto staff = others::StaffComposite::createCurrent(context->document, staffSlot->getPartId(), staffSlot->staffId, meas->getCmper(), 0);
    if (!staff) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " does not have a Staff instance.");
    }
    auto mnxSource = mnxStaff.sources().append(it->second);
    if (auto multiStaffInst = staff->getMultiStaffInstGroup()) {
        if (auto index = multiStaffInst->getIndexOf(staffSlot->staffId)) {
            mnxSource.set_staff(int(*index) + 1);
        }
    }
    if (staff->showNamesForPart(meas->getPartId())) {
        if (meas->calcShouldShowFullNames()) {
            if (staff->multiStaffInstId) {
                mnxSource.set_label(staff->getFullName());
            } else {
                if (staff->masks->fullName) {
                    mnxSource.set_label(staff->getFullInstrumentName());
                } else {
                    mnxSource.set_labelref(mnx::LabelRef::Name);
                }
            }
        } else {
            if (staff->multiStaffInstId) {
                mnxSource.set_label(staff->getAbbreviatedName());
            } else {
                if (staff->masks->abrvName) {
                    mnxSource.set_label(staff->getAbbreviatedInstrumentName());
                } else {
                    mnxSource.set_labelref(mnx::LabelRef::ShortName);                    
                }
            }
        }
        if (mnxSource.label() && mnxSource.label().value() == "") {
            mnxSource.clear_label();
        }
    }
    if (!staff->hideStems) {
        switch (staff->stemDirection) {
        default:
            break;
        case others::Staff::StemDirection::AlwaysUp:
            mnxSource.set_stem(mnx::LayoutStemDirection::Up);
            break;
        case others::Staff::StemDirection::AlwaysDown:
            mnxSource.set_stem(mnx::LayoutStemDirection::Down);
            break;
        }
    }

    /** @todo (not sure if this will ever be done: will source from voiced parts settings if so)
    if (staffSlot->hasVoiceNumber()) {
        mnxSource.set_voice(staffSlot->getVoiceName());
    }
    */
}

static void buildOrderedContent(
    mnx::ContentArray&& content,
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
            auto mnxGroup = content.append<mnx::layout::Group>();
            if (!group.group->hideName) {
                auto name = forMeas->calcShouldShowFullNames()
                    ? group.group->getFullInstrumentName()
                    : group.group->getAbbreviatedInstrumentName();
                if (!name.empty()) {
                    mnxGroup.set_label(name);
                }
            }
            if (group.startSlot != group.endSlot || group.group->bracket->showOnSingleStaff) {
                switch (group.group->bracket->style) {
                case details::StaffGroup::BracketStyle::None:
                    break;
                case details::StaffGroup::BracketStyle::PianoBrace:
                    mnxGroup.set_symbol(mnx::LayoutSymbol::Brace);
                    break;
                default:
                    mnxGroup.set_symbol(mnx::LayoutSymbol::Bracket);
                    break;
                }
            }
            // @todo add group properties to content
            buildOrderedContent(mnxGroup.content(), context, groups, systemStaves, forMeas, index, *group.endSlot, groupIndex + 1);
            index = (std::max)(*group.endSlot + 1, index);
        } else {
            auto mnxStaff = content.append<mnx::layout::Staff>();
            buildMnxStaff(std::move(mnxStaff), context, forMeas, systemStaves[index++]);
        }
        if (index > toIndex) {
            break;
        }
    }
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

    // Retrieve the linked parts in order.
    auto musxLinkedParts = others::PartDefinition::getInUserOrder(context->document);

    // Iterate over each linked part and generate layouts.
    for (const auto& linkedPart : musxLinkedParts) {
        Cmper baseSystemIuList = linkedPart->calcSystemIuList(BASE_SYSTEM_ID);
        auto staffSystems = context->document->getOthers()->getArray<others::StaffSystem>(linkedPart->getCmper());
        const SystemCmper minSystem = BASE_SYSTEM_ID;
        const SystemCmper maxSystem = SystemCmper(staffSystems.size());
        for (SystemCmper sysId = minSystem; sysId <= maxSystem; sysId++) { //NOTE: unusual loop limits are *on purpose*
            Cmper systemIuList = sysId ? linkedPart->calcSystemIuList(staffSystems[sysId - 1]->getCmper()) : baseSystemIuList;
            if (sysId != BASE_SYSTEM_ID && systemIuList == baseSystemIuList) {
                continue;
            }
            if (!mnxDocument->layouts()) {
                mnxDocument->create_layouts();
            }
            auto layout = mnxDocument->layouts().value().append();
            layout.set_id(calcSystemLayoutId(linkedPart->getCmper(), sysId));

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
            buildOrderedContent(layout.content(), context, groups, systemStaves, meas);
        }
    }
}

} // namespace mnxexp
} // namespace denigma
