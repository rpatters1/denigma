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
#include <utility>

#include "denigma/classify/dynamics.h"
#include "mx/api/MarkData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

void appendDynamicExpression(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::DynamicMark& dynamic,
    VerticalPlacement placement)
{
    auto direction = mx::api::DirectionData{};
    direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(Fraction::fromEdu(assignment->eduPosition));
    direction.placement = enumConvert<mx::api::Placement>(placement);
    direction.isStaffValueSpecified = true;
    if (assignment->layer > 0 || assignment->voice2) {
        const LayerIndex layer = assignment->layer > 0 ? assignment->layer - 1 : 0;
        direction.voice = musicXmlVoiceNumber(staffIndex, layer, assignment->voice2 ? 2 : 1);
    }

    const auto markType = enumConvert<mx::api::MarkType>(dynamic.dynamic);
    if (markType != mx::api::MarkType::unspecified) {
        auto mark = mx::api::MarkData(direction.placement, markType);
        if (mark.markType == mx::api::MarkType::otherDynamics) {
            mark.name = classify::dynamicGlyphsToLetters(dynamic.glyphs);
            if (mark.name.empty()) {
                return;
            }
        }
        direction.marks.emplace_back(std::move(mark));
    }

    if (!mx::api::isDirectionDataEmpty(direction)) {
        staff.directions.emplace_back(std::move(direction));
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
