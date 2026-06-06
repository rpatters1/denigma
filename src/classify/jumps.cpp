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
#include "classify/jumps.h"

#include <optional>
#include <string>
#include <string_view>

#include "utils/smufl_support.h"
#include "utils/stringutils.h"

namespace denigma::classify {

namespace {

static Jump classifyJumpTextAndGlyph(std::string_view text, std::optional<std::string_view> glyphName)
{
    if (glyphName) {
        if (*glyphName == "segno" || *glyphName == "segnoSerpent1" || *glyphName == "segnoSerpent2" || *glyphName == "segnoJapanese") {
            return Jump::Segno;
        }
        if (*glyphName == "dalSegno") {
            return Jump::DalSegno;
        }
        if (*glyphName == "daCapo") {
            return Jump::DaCapo;
        }
        if (*glyphName == "coda" || *glyphName == "codaSquare" || *glyphName == "codaJapanese") {
            return Jump::Coda;
        }
    }

    const std::string lowerText = utils::toLowerCase(std::string(text));

    if (lowerText == "d.c. al fine") {
        return Jump::DCAlFine;
    }
    if (lowerText == "d.c. al coda") {
        return Jump::DCAlCoda;
    }
    if (lowerText == "d.s. al fine") {
        return Jump::DsAlFine;
    }
    if (lowerText == "d.s. al coda") {
        return Jump::DsAlCoda;
    }
    if (lowerText == "to coda #" || lowerText == "to coda") {
        return Jump::ToCoda;
    }
    if (lowerText == "coda") {
        return Jump::Coda;
    }
    if (lowerText == "fine") {
        return Jump::Fine;
    }
    if (lowerText == "§" || lowerText == "𝄋") {
        return Jump::Segno;
    }
    if (lowerText == "𝄌") {
        return Jump::Coda;
    }

    return Jump::None;
}

} // namespace

/**
 * @brief Provides a default classification from a Finale text repeat definition.
 *
 * This function only maps strings from the Finale 27 Maestro Default file plus
 * a few other obvious symbols (standard Unicode and SMuFL symbols). Anything
 * else returns Jump::None. Text comparison is case-insensitive for ASCII text.
 */
Jump classifyJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatDef>& def)
{
    if (!def) {
        return Jump::None;
    }

    const auto repeatText = def->getDocument()->getOthers()->get<musx::dom::others::TextRepeatText>(def->getRequestedPartId(), def->getCmper());
    if (!repeatText) {
        return Jump::None;
    }

    const auto glyphName = utils::smuflGlyphNameForFont(def->font, repeatText->text);
    const auto glyphNameView = glyphName ? std::optional<std::string_view>(*glyphName) : std::nullopt;
    return classifyJumpTextAndGlyph(repeatText->text, glyphNameView);
}

} // namespace denigma::classify
