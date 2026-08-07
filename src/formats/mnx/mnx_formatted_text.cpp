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
namespace formats {
namespace mnx {
namespace detail {

namespace {

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
    style.set_or_clear_fontStyle(styles.font->italic ? mnxdom::FontStyle::Italic : mnxdom::FontStyle::Plain);
    style.set_or_clear_weight(styles.font->bold ? mnxdom::FontWeight::Bold : mnxdom::FontWeight::Plain);
}

static void appendTextChunk(mnxdom::FormattedText dst, const std::string& text, const EnigmaStyles& styles, const MnxFormattedTextOptions& options, bool addStyle = true)
{
    if (text.empty()) {
        return;
    }
    auto item = dst.appendText(text);
    if (addStyle) {
        applyStyle(item, styles, options);
    }
    if (options.onChunk) {
        options.onChunk(text, {});
    }
}

static void appendSmuflChunk(mnxdom::FormattedText dst, const std::string& text, const std::vector<std::string>& glyphs, const EnigmaStyles& styles, const MnxFormattedTextOptions& options, bool addStyle = true)
{
    if (glyphs.empty()) {
        return;
    }
    auto item = dst.appendSmufl(glyphs);
    if (addStyle && styles.font && styles.font->calcIsSMuFL()) {
        applyStyle(item, styles, options);
    }
    if (options.onChunk) {
        options.onChunk(text, glyphs);
    }
}

static void appendConvertedChunk(mnxdom::FormattedText dst, const std::string& text, const EnigmaStyles& styles, const MnxFormattedTextOptions& options)
{
    if (text.empty() || !styles.font || (options.skipHiddenText && styles.font->hidden)) {
        return;
    }

    if (options.plainTextOnly || options.symbolPolicy == utils::SmuflSymbolPolicy::PreserveText) {
        appendTextChunk(dst, text, styles, options);
        return;
    }

    if (options.symbolPolicy == utils::SmuflSymbolPolicy::PreferSmufl) {
        if (auto glyphs = utils::smuflGlyphNamesForText(styles.font, text); !glyphs.empty()) {
            appendSmuflChunk(dst, text, glyphs, styles, options);
        } else {
            appendTextChunk(dst, text, styles, options);
        }
        return;
    }

    if (options.symbolPolicy == utils::SmuflSymbolPolicy::SplitSmufl) {
        bool addTextStyle = true;
        for (const auto& run : utils::smuflSplitRunsByGlyphMapping(styles.font, text)) {
            if (run.isSmufl) {
                appendSmuflChunk(dst, run.text, run.glyphs, styles, options, false);
            } else {
                appendTextChunk(dst, run.text, styles, options, addTextStyle);
                addTextStyle = false;
            }
        }
    }
}

} // namespace

void setFormattedText(
    mnxdom::FormattedText dst,
    const EnigmaParsingContext& src,
    const MnxFormattedTextOptions& options)
{
    dst.clear();

    EnigmaString::EnigmaParsingOptions parsingOptions;
    if (options.plainTextOnly || options.symbolPolicy == utils::SmuflSymbolPolicy::PreserveText) {
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

mnxdom::FormattedText makeFormattedText(
    const EnigmaParsingContext& src,
    const MnxFormattedTextOptions& options)
{
    mnxdom::FormattedText result;
    setFormattedText(result, src, options);
    return result;
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
