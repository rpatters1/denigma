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
#include "mx/api/SymbolData.h"

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
    if (isTopStaffAssignment(assignment)) {
        direction.systemRelation = mx::api::SystemRelation::onlyTop;
    }
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
    auto direction = createDynamicDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    auto pendingWords = std::vector<mx::api::WordsData>{};

    auto flushPendingWords = [&]() {
        appendMusicXmlWordsRun(direction, std::move(pendingWords));
        pendingWords.clear();
    };

    auto appendWords = [&](const musx::util::EnigmaTextChunk& chunk) {
        auto words = musicXmlWordsFromEnigmaTextChunk(context, chunk);
        if (words) {
            pendingWords.emplace_back(std::move(*words));
        }
    };

    for (const auto& run : classification.runs) {
        if (const auto* dynamic = run.as<classify::dynamics::Mark>()) {
            auto mark = createDynamicMark(*dynamic, enumConvert<mx::api::Placement>(placement));
            if (!mark) {
                if (dynamic->glyphs.empty()) {
                    appendWords(run.chunk);
                    continue;
                }
                // Preserve a fully resolved but semantically unrepresentable dynamic as
                // portable SMuFL symbols; retain the font-based words fallback otherwise.
                const auto sourceWords = musicXmlWordsFromEnigmaTextChunk(context, run.chunk);
                if (!sourceWords) {
                    continue;
                }
                flushPendingWords();
                auto symbols = std::vector<mx::api::WordsChoice>{};
                symbols.reserve(dynamic->glyphs.size());
                for (const auto& glyph : dynamic->glyphs) {
                    auto symbol = mx::api::SymbolData{};
                    symbol.smufl = glyph;
                    symbol.positionData = sourceWords->positionData;
                    symbol.fontData = sourceWords->fontData;
                    if (sourceWords->isColorSpecified) {
                        symbol.color = sourceWords->colorData;
                    }
                    symbol.enclosure = sourceWords->enclosure;
                    symbol.justify = sourceWords->justify;
                    symbols.emplace_back(std::move(symbol));
                }
                direction.directionTypes.emplace_back(std::move(symbols));
                continue;
            }
            flushPendingWords();
            direction.directionTypes.emplace_back(std::move(*mark));
        } else if (run.as<classify::expression::GenericText>() || run.as<classify::expression::DynamicQualifier>()) {
            appendWords(run.chunk);
        }
    }

    flushPendingWords();
    if (mx::api::isDirectionDataEmpty(direction)) {
        return {};
    }
    return { std::move(direction) };
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
