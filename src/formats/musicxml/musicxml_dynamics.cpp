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
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "denigma/classify/dynamics.h"
#include "musicxml_formatted_text.h"
#include "mx/api/DynamicsData.h"
#include "mx/api/MarkData.h"
#include "mx/api/SymbolData.h"
#include "utils/stringutils.h"

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

/// @brief Returns the MusicXML dynamic element that spells these letters, or nullopt when
/// MusicXML has no element for them.
///
/// MusicXML names its dynamic elements after the letters they draw, so a glyph maps to an element
/// exactly when the glyph's letters name one. The bare affix letters "m", "r", "s", and "z" have
/// no element of their own.
std::optional<mx::api::StandardDynamic> musicXmlStandardDynamic(std::string_view letters)
{
    static const std::unordered_map<std::string_view, mx::api::StandardDynamic> elements = {
        { "p", mx::api::StandardDynamic::p },
        { "pp", mx::api::StandardDynamic::pp },
        { "ppp", mx::api::StandardDynamic::ppp },
        { "pppp", mx::api::StandardDynamic::pppp },
        { "ppppp", mx::api::StandardDynamic::ppppp },
        { "pppppp", mx::api::StandardDynamic::pppppp },
        { "f", mx::api::StandardDynamic::f },
        { "ff", mx::api::StandardDynamic::ff },
        { "fff", mx::api::StandardDynamic::fff },
        { "ffff", mx::api::StandardDynamic::ffff },
        { "fffff", mx::api::StandardDynamic::fffff },
        { "ffffff", mx::api::StandardDynamic::ffffff },
        { "mp", mx::api::StandardDynamic::mp },
        { "mf", mx::api::StandardDynamic::mf },
        { "sf", mx::api::StandardDynamic::sf },
        { "sfp", mx::api::StandardDynamic::sfp },
        { "sfpp", mx::api::StandardDynamic::sfpp },
        { "fp", mx::api::StandardDynamic::fp },
        { "rf", mx::api::StandardDynamic::rf },
        { "rfz", mx::api::StandardDynamic::rfz },
        { "sfz", mx::api::StandardDynamic::sfz },
        { "sffz", mx::api::StandardDynamic::sffz },
        { "fz", mx::api::StandardDynamic::fz },
        { "n", mx::api::StandardDynamic::n },
        { "pf", mx::api::StandardDynamic::pf },
        { "sfzp", mx::api::StandardDynamic::sfzp }
    };
    const auto found = elements.find(letters);
    return found != elements.end() ? std::optional{ found->second } : std::nullopt;
}

/// @brief Decomposes a marking with no dedicated MusicXML dynamic element into the ordered
/// components of one `<dynamics>` element, one component per source glyph.
///
/// The glyph sequence is how the source spells the marking, and MusicXML says the same thing the
/// same way: Finale draws "sfmp" either as the composite `dynamicSforzando1` and `dynamicMP`
/// glyphs or as four separate letters, and each spelling exports as itself. A glyph whose letters
/// name a MusicXML dynamic element writes that element; every other glyph writes an
/// `<other-dynamics>` carrying its letters and its own SMuFL name.
std::optional<mx::api::CompoundDynamicsData> createCompoundDynamics(const std::vector<std::string>& glyphs)
{
    if (glyphs.size() < 2) {
        return std::nullopt; // a lone glyph is the plain other-dynamics case
    }

    auto result = mx::api::CompoundDynamicsData{};
    for (const auto& glyph : glyphs) {
        auto letters = classify::dynamicGlyphsToLetters({ glyph });
        if (letters.empty()) {
            return std::nullopt; // an unmapped glyph would silently drop part of the spelling
        }
        if (const auto standard = musicXmlStandardDynamic(letters)) {
            result.components.emplace_back(*standard);
        } else {
            result.components.emplace_back(mx::api::OtherDynamicsData{ std::move(letters), glyph });
        }
    }
    return result;
}

std::optional<mx::api::MarkData> createDynamicMark(
    const classify::dynamics::Mark& dynamic, std::string_view sourceText, mx::api::Placement placement)
{
    const auto markType = enumConvert<mx::api::MarkType>(dynamic.dynamic);
    if (markType == mx::api::MarkType::unspecified) {
        return std::nullopt;
    }

    auto mark = mx::api::MarkData(placement, markType);
    if (mark.markType == mx::api::MarkType::otherDynamics) {
        if (auto compound = createCompoundDynamics(dynamic.glyphs)) {
            mark.markType = mx::api::MarkType::compoundDynamics;
            mark.choice = std::move(*compound);
        } else if (auto letters = classify::dynamicGlyphsToLetters(dynamic.glyphs); !letters.empty()) {
            mark.name = std::move(letters); // one glyph spells the whole marking
            mark.choice = mx::api::OtherMarkData{ dynamic.glyphs.front() };
        } else {
            // No glyphs to follow, so the marking can only be spelled with its letters. The
            // classifier reports a glyphless dynamic only when the source text is dynamic letters,
            // so normalize that text the way the classifier did before matching it.
            auto sourceLetters = std::string{};
            for (const char ch : sourceText) {
                if (!utils::isSpace(static_cast<unsigned char>(ch))) {
                    sourceLetters.push_back(utils::toLowerCase(ch));
                }
            }
            if (sourceLetters.empty()) {
                return std::nullopt;
            }
            mark.name = std::move(sourceLetters);
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
            auto mark = createDynamicMark(*dynamic, run.chunk.text, enumConvert<mx::api::Placement>(placement));
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
