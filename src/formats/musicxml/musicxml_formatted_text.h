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
#include "mx/api/LyricData.h"
#include "mx/api/PageTextData.h"
#include "mx/api/WordsData.h"

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

using MusicXmlFormattedTextChunkCallback = std::function<void(const mx::api::FontData&, const std::string&)>;

struct MusicXmlFormattedTextOptions
{
    MusicXmlFontFamilyFallback fallback = MusicXmlFontFamilyFallback::Text;
    musx::util::EnigmaString::AccidentalStyle accidentalStyle = musx::util::EnigmaString::AccidentalStyle::Unicode;
    MusicXmlFormattedTextChunkCallback onChunk;
};

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
std::vector<mx::api::WordsData> musicXmlWordsFromEnigmaText(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options = {});
std::optional<MusicXmlPageTextContent> musicXmlPageTextContentFromEnigmaText(
    const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options = {});

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
