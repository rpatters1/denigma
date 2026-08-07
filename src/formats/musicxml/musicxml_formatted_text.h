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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "musicxml_mapping.h"
#include "mx/api/DirectionData.h"
#include "mx/api/LyricData.h"
#include "mx/api/PageTextData.h"
#include "mx/api/SymbolData.h"
#include "mx/api/WordsChoice.h"
#include "mx/api/WordsData.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

using MusicXmlFormattedTextChunkCallback = std::function<void(const mx::api::FontData&, const std::string&)>;

struct MusicXmlFormattedTextOptions
{
    MusicXmlFontFamilyFallback fallback = MusicXmlFontFamilyFallback::Text;
    musx::util::EnigmaString::AccidentalStyle accidentalStyle = musx::util::EnigmaString::AccidentalStyle::Unicode;
    /// @brief How music-font characters become `<symbol>` elements. See utils::SmuflSymbolPolicy.
    ///
    /// Denigma splits by default, because it cannot know whether the reader has the source font and
    /// a missing one turns a glyph into whatever character shares its codepoint. That default costs
    /// a reader who does have the font the kerning a legacy metronome font applies, which an unsplit
    /// run would have carried through intact. The roadmap's font-availability assertion is the
    /// intended way for a user to vouch for their fonts, selecting PreserveText so that nothing is
    /// substituted.
    utils::SmuflSymbolPolicy symbolPolicy = utils::SmuflSymbolPolicy::SplitSmufl;
    MusicXmlFormattedTextChunkCallback onChunk;
};

/// @brief Applies attributes shared by every item of a words run.
///
/// `mx::api::WordsChoice` holds its alternatives by value and returns copies, so each item is
/// rebuilt rather than modified in place. @p fn receives the item's position, enclosure, and justify.
template <typename Fn>
void forEachMusicXmlWordsRunItem(std::vector<mx::api::WordsChoice>& run, Fn&& fn)
{
    for (auto& item : run) {
        if (item.isSymbol()) {
            auto symbol = item.symbol();
            fn(symbol.positionData, symbol.enclosure, symbol.justify);
            item = mx::api::WordsChoice(std::move(symbol));
        } else {
            auto words = item.words();
            fn(words.positionData, words.enclosure, words.justify);
            item = mx::api::WordsChoice(std::move(words));
        }
    }
}

struct MusicXmlPageTextContent
{
    std::string text;
    mx::api::FontData fontData;
    std::vector<std::string> creditTypes;
};

void parseMusicXmlFormattedText(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options = {});
std::optional<mx::api::WordsData> musicXmlWordsFromEnigmaTextChunk(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaTextChunk& chunk,
    const MusicXmlFormattedTextOptions& options = {});
mx::api::LyricData musicXmlLyricFromSyllable(
    const MusicXmlMusxMapping& context,
    const musx::dom::texts::LyricsTextBase& lyricText,
    size_t syllableIndex,
    const MusicXmlFormattedTextOptions& options = {});
/// @brief Converts formatted text into an ordered run of words and SMuFL symbols.
///
/// Music-font characters become `<symbol>` items according to MusicXmlFormattedTextOptions::symbolPolicy,
/// so that a glyph survives on a system lacking the source font. Chunk order and fonts are retained.
std::vector<mx::api::WordsChoice> musicXmlWordsFromEnigmaText(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options = {});
/// @brief Appends one direction type holding an ordered run of words and symbols.
void appendMusicXmlWordsRun(
    mx::api::DirectionData& direction,
    std::vector<mx::api::WordsChoice> run);
/// @brief Appends one words-only direction type, for callers that build WordsData directly.
void appendMusicXmlWordsRun(
    mx::api::DirectionData& direction,
    std::vector<mx::api::WordsData> words);
std::optional<MusicXmlPageTextContent> musicXmlPageTextContentFromEnigmaText(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options = {});

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
