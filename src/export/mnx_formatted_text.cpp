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
#include "mnx_formatted_text.h"

#include <string>
#include <string_view>
#include <vector>

#include "utils/smufl_support.h"
#include "utils/utf8_iterator.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

namespace {

struct GlyphRun
{
    bool isSmufl{};
    std::string text;
    std::vector<std::string> glyphs;
};

static void appendUtf8(std::string& dst, std::string_view src, size_t offset, size_t length)
{
    dst.append(src.substr(offset, length));
}

static std::vector<std::string> glyphNamesForText(const MusxInstance<FontInfo>& font, const std::string& text)
{
    std::vector<std::string> result;
    bool allMapped = true;
    for (utils::Utf8Iterator iter(text); !iter.atEnd(); iter.next()) {
        if (auto glyphName = utils::smuflGlyphNameForFont(font, iter->codepoint)) {
            result.push_back(*glyphName);
        } else {
            allMapped = false;
        }
    }
    if (!allMapped) {
    result.clear();
    }
    return result;
}

static std::vector<GlyphRun> splitRunsByGlyphMapping(const MusxInstance<FontInfo>& font, const std::string& text)
{
    std::vector<GlyphRun> result;

    auto appendText = [&](std::string_view src, size_t offset, size_t length) {
        if (result.empty() || result.back().isSmufl) {
            result.push_back(GlyphRun{ false, {}, {} });
        }
        appendUtf8(result.back().text, src, offset, length);
    };

    auto appendGlyph = [&](const std::string& glyphName) {
        if (result.empty() || !result.back().isSmufl) {
            result.push_back(GlyphRun{ true, {}, {} });
        }
        result.back().glyphs.push_back(glyphName);
    };

    for (utils::Utf8Iterator iter(text); !iter.atEnd(); iter.next()) {
        if (auto glyphName = utils::smuflGlyphNameForFont(font, iter->codepoint)) {
            appendGlyph(*glyphName);
        } else {
            appendText(text, iter.offset(), iter->byteCount);
        }
    }

    return result;
}

static bool sameFont(const MusxInstance<FontInfo>& lhs, const MusxInstance<FontInfo>& rhs)
{
    if (!lhs || !rhs) {
        return false;
    }
    return lhs->isSame(*rhs);
}

static bool shouldAddStyle(const EnigmaStyles& styles, const MnxFormattedTextOptions& options)
{
    if (options.plainTextOnly || !styles.font) {
        return false;
    }
    if (options.initialFont && sameFont(styles.font, options.initialFont.value())) {
        return false;
    }
    return true;
}

template <typename T>
static void applyStyle(T item, const EnigmaStyles& styles, const MnxFormattedTextOptions& options)
{
    if (!shouldAddStyle(styles, options)) {
        return;
    }

    auto style = item.ensure_style();
    const std::string fontName = styles.font->getName();
    if (!fontName.empty()) {
        style.set_font(fontName);
    }
    if (styles.font->fontSize > 0) {
        style.set_size(static_cast<double>(styles.font->fontSize));
    }
    style.set_or_clear_fontStyle(styles.font->italic ? mnx::FontStyle::Italic : mnx::FontStyle::Plain);
    style.set_or_clear_weight(styles.font->bold ? mnx::FontWeight::Bold : mnx::FontWeight::Plain);
}

static void appendTextChunk(mnx::FormattedText dst, const std::string& text, const EnigmaStyles& styles, const MnxFormattedTextOptions& options, bool addStyle = true)
{
    if (text.empty()) {
        return;
    }
    auto item = dst.append<mnx::text::Text>(text);
    if (addStyle) {
        applyStyle(item, styles, options);
    }
}

static void appendSmuflChunk(mnx::FormattedText dst, const std::vector<std::string>& glyphs, const EnigmaStyles& styles, const MnxFormattedTextOptions& options, bool addStyle = true)
{
    if (glyphs.empty()) {
        return;
    }
    auto item = dst.append<mnx::text::Smufl>(glyphs);
    if (addStyle && styles.font && styles.font->calcIsSMuFL()) {
        applyStyle(item, styles, options);
    }
}

static void appendConvertedChunk(mnx::FormattedText dst, const std::string& text, const EnigmaStyles& styles, const MnxFormattedTextOptions& options)
{
    if (text.empty() || !styles.font || (options.skipHiddenText && styles.font->hidden)) {
        return;
    }

    if (options.plainTextOnly || options.symbolPolicy == MnxFormattedTextSymbolPolicy::PreserveText) {
        appendTextChunk(dst, text, styles, options);
        return;
    }

    if (options.symbolPolicy == MnxFormattedTextSymbolPolicy::PreferSmufl) {
        if (auto glyphs = glyphNamesForText(styles.font, text); !glyphs.empty()) {
            appendSmuflChunk(dst, glyphs, styles, options);
        } else {
            appendTextChunk(dst, text, styles, options);
        }
        return;
    }

    bool addTextStyle = true;
    for (const auto& run : splitRunsByGlyphMapping(styles.font, text)) {
        if (run.isSmufl) {
            appendSmuflChunk(dst, run.glyphs, styles, options, false);
        } else {
            appendTextChunk(dst, run.text, styles, options, addTextStyle);
            addTextStyle = false;
        }
    }
}

} // namespace

void setFormattedText(
    mnx::FormattedText dst,
    const EnigmaParsingContext& src,
    const MnxFormattedTextOptions& options)
{
    dst.clear();

    EnigmaString::EnigmaParsingOptions parsingOptions;
    if (options.plainTextOnly || options.symbolPolicy == MnxFormattedTextSymbolPolicy::PreserveText) {
        parsingOptions = EnigmaString::EnigmaParsingOptions(EnigmaString::AccidentalStyle::Unicode);
    }
    if (options.plainTextOnly) {
        parsingOptions.ignoreStyleTags = true;
    }

    src.parseEnigmaText([&](const std::string& chunk, const EnigmaStyles& styles) -> bool {
        appendConvertedChunk(dst, chunk, styles, options);
        return true;
    }, parsingOptions);
}

mnx::FormattedText makeFormattedText(
    const EnigmaParsingContext& src,
    const MnxFormattedTextOptions& options)
{
    mnx::FormattedText result;
    setFormattedText(result, src, options);
    return result;
}

} // namespace mnxexp
} // namespace denigma
