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
#include "musicxml_formatted_text.h"
#include "mx/api/MarkData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

mx::api::DirectionData createDynamicDirection(
    const MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = mx::api::DirectionData{};
    direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(Fraction::fromEdu(assignment->eduPosition));
    direction.placement = enumConvert<mx::api::Placement>(placement);
    direction.isStaffValueSpecified = isStaffValueSpecified;
    if (assignment->layer > 0 || assignment->voice2) {
        const LayerIndex layer = assignment->layer > 0 ? assignment->layer - 1 : 0;
        direction.voice = musicXmlVoiceNumber(staffIndex, layer, assignment->voice2 ? 2 : 1);
    }
    return direction;
}

std::optional<mx::api::MarkData> createDynamicMark(const classify::dynamics::Mark& dynamic, mx::api::Placement placement)
{
    const auto markType = enumConvert<mx::api::MarkType>(dynamic.dynamic);
    if (markType == mx::api::MarkType::unspecified) {
        return std::nullopt;
    }

    auto mark = mx::api::MarkData(placement, markType);
    if (mark.markType == mx::api::MarkType::otherDynamics) {
        mark.name = classify::dynamicGlyphsToLetters(dynamic.glyphs);
        if (mark.name.empty()) {
            return std::nullopt;
        }
    }
    return mark;
}

} // namespace

std::vector<mx::api::DirectionData> createDynamicExpressionDirections(
    MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    std::vector<mx::api::DirectionData> result;
    mx::api::DirectionData pendingWords = createDynamicDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);

    auto flushPendingWords = [&]() {
        if (!pendingWords.words.empty()) {
            result.emplace_back(std::move(pendingWords));
            pendingWords = createDynamicDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
        }
    };

    auto appendWordsToDirection = [&](mx::api::DirectionData& direction, const musx::util::EnigmaTextChunk& chunk) {
        auto words = musicXmlWordsFromEnigmaTextChunk(context, chunk);
        if (words) {
            direction.words.emplace_back(std::move(*words));
        }
    };

    mx::api::DirectionData* currentDynamicDirection = nullptr;
    for (const auto& run : classification.runs) {
        if (const auto* dynamic = run.as<classify::dynamics::Mark>()) {
            auto mark = createDynamicMark(*dynamic, enumConvert<mx::api::Placement>(placement));
            if (!mark) {
                continue;
            }
            flushPendingWords();
            result.emplace_back(createDynamicDirection(context, staffIndex, assignment, placement, isStaffValueSpecified));
            result.back().marks.emplace_back(std::move(*mark));
            currentDynamicDirection = &result.back();
        } else if (run.as<classify::expression::GenericText>() || run.as<classify::expression::DynamicQualifier>()) {
            if (currentDynamicDirection) {
                appendWordsToDirection(*currentDynamicDirection, run.chunk);
            } else {
                appendWordsToDirection(pendingWords, run.chunk);
            }
        } else {
            currentDynamicDirection = nullptr;
        }
    }

    flushPendingWords();
    return result;
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
