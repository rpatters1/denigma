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

#include <string>
#include <vector>

#include "utils/stringutils.h"

#include "mx/api/PartData.h"

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

void populatePartMetadata(mx::api::PartData& part, const std::string& id, const MusxInstance<others::StaffComposite>& staff)
{
    part.uniqueId = id;
    part.instrumentData.uniqueId = id + "-I1";
    if (const auto soundId = musicXmlSoundIdFromInstrumentUuid(staff->instUuid)) {
        part.instrumentData.soundID = *soundId;
    }

    const auto fullName = staff->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!fullName.empty()) {
        part.name = utils::trimNewLineFromString(fullName);
        part.displayName = fullName;
        part.instrumentData.name = fullName;
    }

    const auto abbreviatedName = staff->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode);
    if (!abbreviatedName.empty()) {
        part.abbreviation = utils::trimNewLineFromString(abbreviatedName);
        part.displayAbbreviation = abbreviatedName;
        part.instrumentData.abbreviation = abbreviatedName;
    }
}

} // namespace

void createParts(MusicXmlMusxMapping& context)
{
    auto& parts = context.musicXmlScore->parts;
    parts.clear();
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
        populatePartMetadata(part, id, staff);
        mapPartToInstrumentStaves(context, id, instInfo);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
