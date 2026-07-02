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

#include "musicxml_formatted_text.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

std::string_view musicXmlFontFamilyFallbackName(MusicXmlFontFamilyFallback fallback)
{
    switch (fallback) {
    case MusicXmlFontFamilyFallback::None:
        return {};
    case MusicXmlFontFamilyFallback::Music:
        return "music";
    case MusicXmlFontFamilyFallback::Engraved:
        return "engraved";
    case MusicXmlFontFamilyFallback::Handwritten:
        return "handwritten";
    case MusicXmlFontFamilyFallback::Text:
        return "text";
    case MusicXmlFontFamilyFallback::Serif:
        return "serif";
    case MusicXmlFontFamilyFallback::SansSerif:
        return "sans-serif";
    case MusicXmlFontFamilyFallback::Cursive:
        return "cursive";
    case MusicXmlFontFamilyFallback::Fantasy:
        return "fantasy";
    case MusicXmlFontFamilyFallback::Monospace:
        return "monospace";
    }
    throw std::invalid_argument("Unknown MusicXML font-family fallback.");
}

} // namespace

mx::api::FontData MusicXmlMusxMapping::musicXmlFontDataFromFontInfo(const musx::dom::FontInfo& fontInfo,
    MusicXmlFontFamilyFallback fallback) const
{
    mx::api::FontData result;
    const auto fontName = fontInfo.getName();
    if (!fontName.empty()) {
        result.fontFamily.emplace_back(fontName);
    }
    if (const auto fallbackName = musicXmlFontFamilyFallbackName(fallback); !fallbackName.empty()) {
        result.fontFamily.emplace_back(fallbackName);
    }

    if (!fontInfo.getSizeIsPercent() && fontInfo.fontSize > 0) {
        constexpr auto kUnscaledMmPerStaff = musx::dom::EVPU_PER_STANDARD_STAFF / musx::dom::EVPU_PER_MM;
        auto staffScaling = 1.0;
        if (!fontInfo.absolute) {
            const bool hasInitializedScaling = musicXmlScore && musicXmlScore->defaults.scalingMillimeters > 0.0;
            ASSERT_IF(!hasInitializedScaling) {
                throw std::logic_error("MusicXML font conversion requires initialized score scaling for non-absolute font sizes.");
            }
            staffScaling = musicXmlScore->defaults.scalingMillimeters / kUnscaledMmPerStaff;
        }
        result.sizeType = mx::api::FontSizeType::point;
        result.sizePoint = static_cast<double>(fontInfo.fontSize) * staffScaling;
    }

    // Finale font styles are explicit: unset bold/italic means normal, not unspecified.
    // Emit normal so MusicXML importers do not inherit style from surrounding context.
    result.style = fontInfo.italic ? mx::api::FontStyle::italic : mx::api::FontStyle::normal;
    result.weight = fontInfo.bold ? mx::api::FontWeight::bold : mx::api::FontWeight::normal;
    result.underline = fontInfo.underline ? 1 : 0;
    result.lineThrough = fontInfo.strikeout ? 1 : 0;
    return result;
}

void parseMusicXmlFormattedText(const MusicXmlMusxMapping& context, const musx::util::EnigmaParsingContext& text,
    const MusicXmlFormattedTextOptions& options)
{
    const musx::util::EnigmaString::EnigmaParsingOptions parsingOptions(options.accidentalStyle);
    text.parseEnigmaText([&](const std::string& chunk, const musx::util::EnigmaStyles& styles) -> bool {
        ASSERT_IF(!styles.font) {
            throw std::logic_error("MusicXML formatted text chunk has no font data.");
        }
        if (options.onChunk) {
            options.onChunk(context.musicXmlFontDataFromFontInfo(*styles.font, options.fallback), chunk);
        }
        return true;
    }, parsingOptions);
}

mx::api::LyricData musicXmlLyricFromSyllable(const MusicXmlMusxMapping& context,
    const musx::dom::texts::LyricsTextBase& lyricText, size_t syllableIndex, const MusicXmlFormattedTextOptions& options)
{
    ASSERT_IF(syllableIndex >= lyricText.syllables.size()) {
        throw std::out_of_range("MusicXML lyric syllable index is out of range.");
    }

    const auto& syllable = lyricText.syllables[syllableIndex];
    mx::api::LyricData result;
    result.text = syllable->syllable;
    result.syllabic = [&] {
        if (syllable->hasHyphenBefore && syllable->hasHyphenAfter) {
            return mx::api::LyricSyllabic::middle;
        } else if (syllable->hasHyphenBefore) {
            return mx::api::LyricSyllabic::end;
        } else if (syllable->hasHyphenAfter) {
            return mx::api::LyricSyllabic::begin;
        }
        return mx::api::LyricSyllabic::single;
    }();

    auto fontData = mx::api::FontData{};
    auto foundFont = false;
    const auto matchesDefaultLyricFont = [&](const mx::api::FontData& candidate) {
        if (!context.musicXmlScore) {
            return false;
        }
        for (const auto& lyricFont : context.musicXmlScore->defaults.lyricFonts) {
            if (lyricFont.number.empty() && lyricFont.name.empty() && candidate == lyricFont.font) {
                return true;
            }
        }
        return false;
    };
    result.text.clear();
    lyricText.iterateStylesForSyllable(syllableIndex, [&](const std::string& chunk, const musx::util::EnigmaStyles& styles) -> bool {
        result.text += chunk;
        if (!foundFont) {
            ASSERT_IF(!styles.font) {
                throw std::logic_error("MusicXML lyric syllable chunk has no font data.");
            }
            fontData = context.musicXmlFontDataFromFontInfo(*styles.font, options.fallback);
            if (!matchesDefaultLyricFont(fontData)) {
                result.printData.fontData = fontData;
            }
            foundFont = true;
        }
        return true;
    });
    if (options.onChunk) {
        options.onChunk(result.printData.fontData, result.text);
    }
    return result;
}

std::vector<mx::api::WordsData> musicXmlWordsFromEnigmaText(const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text, const MusicXmlFormattedTextOptions& options)
{
    std::vector<mx::api::WordsData> result;
    auto parserOptions = options;
    parserOptions.onChunk = [&](const mx::api::FontData& fontData, const std::string& chunk) {
        mx::api::WordsData words;
        words.fontData = fontData;
        words.text = chunk;
        result.emplace_back(std::move(words));
        if (options.onChunk) {
            options.onChunk(fontData, chunk);
        }
    };
    parseMusicXmlFormattedText(context, text, parserOptions);
    return result;
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
