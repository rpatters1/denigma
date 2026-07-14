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
#include "classify.h"

#include "smufl_mapping.h"

namespace denigma {
namespace classify {
namespace detail {

std::optional<std::string> glyphNameForFont(
    const std::shared_ptr<musx::dom::FontInfo>& font,
    char32_t codepoint)
{
    if (!font) {
        return std::nullopt;
    }
    if (font->calcIsSMuFL()) {
        if (const auto* name = smufl_mapping::getGlyphName(codepoint)) {
            return std::string(*name);
        }
        return std::nullopt;
    }
    if (const auto legacyInfo = smufl_mapping::getLegacyGlyphInfo(font->getName(), codepoint)) {
        return std::string(legacyInfo->name);
    }
    return std::nullopt;
}

} // namespace detail
} // namespace classify
} // namespace denigma
