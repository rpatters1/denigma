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

#include <algorithm>
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
    MusicXmlFontFamilyFallback fallback, MusicXmlFontScaling fontScaling) const
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
        auto scaling = 1.0;
        if (!fontInfo.absolute) {
            if (fontScaling == MusicXmlFontScaling::Page) {
                // Page-attached text sits on the page, so only the page scaling reduces it. Using the
                // combined factor here would shrink it again by the system scaling, which never
                // applies to it. effectivePageFormat is the single-target source: it takes its page
                // percent from the first real page rather than from the page format options alone.
                scaling = finaleOptions.effectivePageFormat->calcPageScaling().toDouble();
            } else {
                const bool hasInitializedScaling = musicXmlScore && musicXmlScore->defaults.scalingMillimeters > 0.0;
                ASSERT_IF(!hasInitializedScaling) {
                    throw std::logic_error("MusicXML font conversion requires initialized score scaling for non-absolute font sizes.");
                }
                scaling = musicXmlScore->defaults.scalingMillimeters / kUnscaledMmPerStaff;
            }
        }
        result.sizeType = mx::api::FontSizeType::point;
        result.sizePoint = static_cast<double>(fontInfo.fontSize) * scaling;
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

std::optional<mx::api::WordsData> musicXmlWordsFromEnigmaTextChunk(const MusicXmlMusxMapping& context, const musx::util::EnigmaTextChunk& chunk,
    const MusicXmlFormattedTextOptions& options)
{
    ASSERT_IF(!chunk.styles.font) {
        throw std::logic_error("MusicXML formatted text chunk has no font data.");
    }
    if (chunk.styles.font->hidden) {
        return std::nullopt;
    }
    mx::api::WordsData result;
    result.fontData = context.musicXmlFontDataFromFontInfo(*chunk.styles.font, options.fallback);
    result.text = chunk.text;
    return result;
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

/// The source font family is deliberately not carried over. A symbol names a glyph rather than a
/// character, and it exists only because Denigma declined to rely on the source font being present;
/// naming that font would ask the reader to resolve a SMuFL glyph name out of it, which a legacy
/// font cannot do at all. Style and weight are dropped for the same reason, since a glyph is neither
/// bold nor italic. A reader who does want the original font is served by not converting at all; see
/// the font-availability assertion in roadmap.md.
///
/// Size is kept only for a SMuFL source. Every SMuFL font sets one em to four staff spaces, so a
/// point size measured in one carries its meaning into another, and a tempo glyph drawn smaller than
/// staff size stays smaller. A legacy font's point size describes only its own design and would
/// mis-scale the substituted glyph, so it is left for the reader to decide.
mx::api::SymbolData musicXmlSymbolFromWords(const mx::api::WordsData& sourceWords,
    const musx::dom::MusxInstance<musx::dom::FontInfo>& font, std::string glyphName)
{
    mx::api::SymbolData result;
    result.smufl = std::move(glyphName);
    result.positionData = sourceWords.positionData;
    if (font && font->calcIsSMuFL()) {
        result.fontData.sizeType = sourceWords.fontData.sizeType;
        result.fontData.sizePoint = sourceWords.fontData.sizePoint;
        result.fontData.sizeCss = sourceWords.fontData.sizeCss;
    }
    if (sourceWords.isColorSpecified) {
        result.color = sourceWords.colorData;
    }
    result.enclosure = sourceWords.enclosure;
    result.justify = sourceWords.justify;
    return result;
}

namespace {

/// Expands one source chunk into run items, converting music-font characters per @p policy.
void appendChunkToWordsRun(std::vector<mx::api::WordsChoice>& run, const mx::api::WordsData& sourceWords,
    const musx::dom::MusxInstance<musx::dom::FontInfo>& font, utils::SmuflSymbolPolicy policy)
{
    const auto appendWords = [&](std::string text) {
        auto words = sourceWords;
        words.text = std::move(text);
        run.emplace_back(std::move(words));
    };

    if (policy == utils::SmuflSymbolPolicy::PreserveText) {
        run.emplace_back(sourceWords);
        return;
    }

    if (policy == utils::SmuflSymbolPolicy::PreferSmufl) {
        auto glyphs = utils::smuflGlyphNamesForText(font, sourceWords.text);
        if (glyphs.empty()) {
            run.emplace_back(sourceWords);
            return;
        }
        for (auto& glyphName : glyphs) {
            run.emplace_back(musicXmlSymbolFromWords(sourceWords, font, std::move(glyphName)));
        }
        return;
    }

    for (const auto& glyphRun : utils::smuflSplitRunsByGlyphMapping(font, sourceWords.text)) {
        if (!glyphRun.isSmufl) {
            appendWords(glyphRun.text);
            continue;
        }
        for (const auto& glyphName : glyphRun.glyphs) {
            run.emplace_back(musicXmlSymbolFromWords(sourceWords, font, glyphName));
        }
    }
}

} // namespace

std::vector<mx::api::WordsChoice> musicXmlWordsFromEnigmaText(const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text, const MusicXmlFormattedTextOptions& options)
{
    std::vector<mx::api::WordsChoice> result;
    text.parseEnigmaText([&](const std::string& chunkText, const musx::util::EnigmaStyles& styles) -> bool {
        musx::util::EnigmaTextChunk chunk{ chunkText, styles };
        auto words = musicXmlWordsFromEnigmaTextChunk(context, chunk, options);
        if (!words) {
            return true;
        }
        if (options.onChunk) {
            options.onChunk(words->fontData, words->text);
        }
        appendChunkToWordsRun(result, *words, styles.font, options.symbolPolicy);
        return true;
    }, musx::util::EnigmaString::EnigmaParsingOptions(options.accidentalStyle));
    return result;
}

void appendMusicXmlWordsRun(mx::api::DirectionData& direction, std::vector<mx::api::WordsChoice> run)
{
    if (run.empty()) {
        return;
    }
    direction.directionTypes.emplace_back(std::move(run));
}

void appendMusicXmlWordsRun(mx::api::DirectionData& direction, std::vector<mx::api::WordsData> words)
{
    if (words.empty()) {
        return;
    }
    std::vector<mx::api::WordsChoice> run;
    run.reserve(words.size());
    for (auto& item : words) {
        run.emplace_back(std::move(item));
    }
    direction.directionTypes.emplace_back(std::move(run));
}

std::optional<MusicXmlPageTextContent> musicXmlPageTextContentFromEnigmaText(const MusicXmlMusxMapping& context,
    const musx::util::EnigmaParsingContext& text, const MusicXmlFormattedTextOptions& options)
{
    MusicXmlPageTextContent result;
    bool foundVisibleFont = false;
    const auto addCreditType = [&](std::string_view type) {
        if (std::find(result.creditTypes.begin(), result.creditTypes.end(), type) == result.creditTypes.end()) {
            result.creditTypes.emplace_back(type);
        }
    };
    const auto creditTypeForInsert = [](std::string_view command) -> std::string_view {
        if (command == "title") { return "title"; }
        if (command == "subtitle") { return "subtitle"; }
        if (command == "composer") { return "composer"; }
        if (command == "lyricist") { return "lyricist"; }
        if (command == "arranger") { return "arranger"; }
        if (command == "copyright") { return "rights"; }
        if (command == "partname") { return "part name"; }
        if (command == "page") { return "page number"; }
        return {};
    };

    text.parseEnigmaText([&](const std::string& chunk, const musx::util::EnigmaStyles& styles) -> bool {
        ASSERT_IF(!styles.font) {
            throw std::logic_error("MusicXML page text chunk has no font data.");
        }
        if (styles.font->hidden) {
            return true;
        }
        const auto fontData = context.musicXmlFontDataFromFontInfo(
            *styles.font, options.fallback, MusicXmlFontScaling::Page);
        if (!foundVisibleFont) {
            result.fontData = fontData;
            foundVisibleFont = true;
        }
        // TODO: mx::api::PageTextData has one FontData and emits one credit-words element, so later font changes are flattened.
        result.text += chunk;
        if (options.onChunk) {
            options.onChunk(fontData, chunk);
        }
        return true;
    }, [&](const std::vector<std::string>& components) -> std::optional<std::string> {
        if (!components.empty()) {
            if (const auto type = creditTypeForInsert(components.front()); !type.empty()) {
                addCreditType(type);
            }
        }
        return std::nullopt;
    }, musx::util::EnigmaString::EnigmaParsingOptions(options.accidentalStyle));

    if (!foundVisibleFont || result.text.empty()) {
        return std::nullopt;
    }
    return result;
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
