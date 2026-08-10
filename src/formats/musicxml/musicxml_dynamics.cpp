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

#include <functional>
#include <map>
#include <string>
#include <string_view>
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

/// @brief Returns the StandardDynamic mx declares after @p value, or nullopt at the end of the enum.
///
/// This exists so that the lookup below can visit every StandardDynamic, which no list can promise
/// to do: C++ cannot enumerate an enum, and a list that falls behind one says nothing about what it
/// omits. A switch can promise it. This one has no default arm, so -Wswitch, which this library
/// builds with -Werror, fails the build the moment mx adds a dynamic.
constexpr std::optional<mx::api::StandardDynamic> nextStandardDynamic(mx::api::StandardDynamic value)
{
    using Dynamic = mx::api::StandardDynamic;
    switch (value) {
    case Dynamic::p: return Dynamic::pp;
    case Dynamic::pp: return Dynamic::ppp;
    case Dynamic::ppp: return Dynamic::pppp;
    case Dynamic::pppp: return Dynamic::ppppp;
    case Dynamic::ppppp: return Dynamic::pppppp;
    case Dynamic::pppppp: return Dynamic::f;
    case Dynamic::f: return Dynamic::ff;
    case Dynamic::ff: return Dynamic::fff;
    case Dynamic::fff: return Dynamic::ffff;
    case Dynamic::ffff: return Dynamic::fffff;
    case Dynamic::fffff: return Dynamic::ffffff;
    case Dynamic::ffffff: return Dynamic::mp;
    case Dynamic::mp: return Dynamic::mf;
    case Dynamic::mf: return Dynamic::sf;
    case Dynamic::sf: return Dynamic::sfp;
    case Dynamic::sfp: return Dynamic::sfpp;
    case Dynamic::sfpp: return Dynamic::fp;
    case Dynamic::fp: return Dynamic::rf;
    case Dynamic::rf: return Dynamic::rfz;
    case Dynamic::rfz: return Dynamic::sfz;
    case Dynamic::sfz: return Dynamic::sffz;
    case Dynamic::sffz: return Dynamic::fz;
    case Dynamic::fz: return Dynamic::n;
    case Dynamic::n: return Dynamic::pf;
    case Dynamic::pf: return Dynamic::sfzp;
    case Dynamic::sfzp: return std::nullopt; // the last one mx declares
    }
    return std::nullopt;
}

/// @brief The MusicXML dynamic elements, keyed by the letters they draw.
///
/// mx spells every element it can write, so the letters come from it rather than from a second copy
/// here, and denigma cannot disagree with it about what an element is called.
const std::map<std::string, mx::api::StandardDynamic, std::less<>>& standardDynamicElements()
{
    // std::less<> so a string_view looks up without building a string.
    static const auto elements = [] {
        auto result = std::map<std::string, mx::api::StandardDynamic, std::less<>>{};
        for (auto dynamic = std::optional{ mx::api::StandardDynamic::p }; dynamic;
                dynamic = nextStandardDynamic(*dynamic)) {
            // Hoisted out of ASSERT_IF, which evaluates its test twice.
            const bool inserted = result.emplace(mx::api::toString(*dynamic), *dynamic).second;
            ASSERT_IF(!inserted) {
                break; // nextStandardDynamic cycled, so the walk would never end
            }
        }
        return result;
    }();
    return elements;
}

/// @brief Returns the MusicXML dynamic element that spells these letters, or nullopt when MusicXML
/// has no element for them.
///
/// MusicXML names its dynamic elements after the letters they draw, so a marking maps to an element
/// exactly when its letters name one. The bare affix letters "m", "r", "s", and "z" have no element
/// of their own.
std::optional<mx::api::StandardDynamic> musicXmlStandardDynamic(std::string_view letters)
{
    const auto& elements = standardDynamicElements();
    const auto found = elements.find(letters);
    return found != elements.end() ? std::optional{ found->second } : std::nullopt;
}

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

/// @brief Decomposes a marking into the ordered components of one `<dynamics>` element, one
/// component per source glyph.
///
/// The glyph sequence is how the source spells the marking, and MusicXML says the same thing the
/// same way: Finale draws "sfmp" either as the composite `dynamicSforzando1` and `dynamicMP`
/// glyphs or as four separate letters, and each spelling exports as itself. A glyph whose letters
/// name a MusicXML dynamic element writes that element; every other glyph writes an
/// `<other-dynamics>` carrying its letters and its own SMuFL name.
std::optional<mx::api::CompoundDynamicsData> createDynamicsFromGlyphs(const std::vector<std::string>& glyphs)
{
    if (glyphs.empty()) {
        return std::nullopt;
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

/// @brief Builds the `<dynamics>` mark for a classified marking, or returns nullopt when the
/// marking cannot be spelled as a dynamic at all.
///
/// A marking whose letters name a MusicXML dynamic element writes that element. Every other
/// marking has to be spelled out symbol by symbol as the children of one `<dynamics>`.
std::optional<mx::api::MarkData> createDynamicMark(
    const classify::dynamics::Mark& dynamic,
    std::string_view sourceText,
    mx::api::Placement placement,
    mx::api::HorizontalAlignment horizontalAlignment)
{
    const auto placed = [placement, horizontalAlignment](mx::api::MarkData mark) {
        mark.positionData.placement = placement;
        mark.positionData.horizontalAlignment = horizontalAlignment;
        return mark;
    };

    if (const auto standard = musicXmlStandardDynamic(classify::dynamicCanonicalText(dynamic.dynamic))) {
        return placed(mx::api::MarkData(*standard));
    }
    if (auto fromGlyphs = createDynamicsFromGlyphs(dynamic.glyphs)) {
        return placed(mx::api::MarkData(std::move(*fromGlyphs)));
    }

    // No glyph sequence to follow, so the marking can only be spelled with its letters. The
    // classifier reports a glyphless dynamic only when the source text is dynamic letters, so
    // normalize that text the way the classifier did before writing it.
    auto sourceLetters = std::string{};
    for (const char ch : sourceText) {
        if (!utils::isSpace(static_cast<unsigned char>(ch))) {
            sourceLetters.push_back(utils::toLowerCase(ch));
        }
    }
    if (sourceLetters.empty()) {
        return std::nullopt;
    }
    auto spelling = mx::api::CompoundDynamicsData{ { mx::api::OtherDynamicsData{ std::move(sourceLetters), std::nullopt } } };
    return placed(mx::api::MarkData(std::move(spelling)));
}

} // namespace

/// @brief Reports how many dynamic elements standardDynamicElements() reached.
///
/// @note Nothing in the library needs this, so it is not declared in musicxml.h. It has external
/// linkage for the tests, which declare it themselves and hold it to MusicXML's own count. That is
/// the one thing the walk cannot check about itself: a release build compiles the cycle assertion
/// out, and a chain that skips a dynamic never trips it in the first place.
size_t musicXmlStandardDynamicCount()
{
    return standardDynamicElements().size();
}

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
    const auto horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    const auto justify = musicXmlJustifyForTextExpression(assignment);

    auto flushPendingWords = [&]() {
        appendMusicXmlWordsRun(direction, std::move(pendingWords));
        pendingWords.clear();
    };

    auto appendWords = [&](const musx::util::EnigmaTextChunk& chunk) {
        auto words = musicXmlWordsFromEnigmaTextChunk(context, chunk);
        if (words) {
            words->positionData.horizontalAlignment = horizontalAlignment;
            words->justify = justify;
            pendingWords.emplace_back(std::move(*words));
        }
    };

    for (const auto& run : classification.runs) {
        if (const auto* dynamic = run.as<classify::dynamics::Mark>()) {
            auto mark = createDynamicMark(
                *dynamic, run.chunk.text, enumConvert<mx::api::Placement>(placement), horizontalAlignment);
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
                    symbols.emplace_back(musicXmlSymbolFromWords(*sourceWords, run.chunk.styles.font, glyph));
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
