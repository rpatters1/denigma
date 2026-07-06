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
#include "denigma/classify/jumps.h"

#include <optional>
#include <string>
#include <string_view>

#include "smufl_mapping.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

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

Jump classifyVisualJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatDef>& def)
{
    if (!def) {
        return Jump::None;
    }

    const auto repeatText = def->getDocument()->getOthers()->get<musx::dom::others::TextRepeatText>(def->getRequestedPartId(), def->getCmper());
    if (!repeatText) {
        return Jump::None;
    }

    std::optional<std::string_view> glyphNameView;
    if (def->font) {
        if (const auto codepoint = utils::utf8ToCodepoint(repeatText->text)) {
            if (const auto* glyphName = smufl_mapping::getGlyphNameForFont(
                    def->font->getName(),
                    codepoint.value(),
                    def->font->calcIsSMuFL(),
                    smufl_mapping::SmuflGlyphSource::Finale)) {
                glyphNameView = *glyphName;
            }
        }
    }
    return classifyJumpTextAndGlyph(repeatText->text, glyphNameView);
}

bool isJumpCommand(Jump jump)
{
    switch (jump) {
    case Jump::ToCoda:
    case Jump::DaCapo:
    case Jump::DCAlFine:
    case Jump::DCAlCoda:
    case Jump::DalSegno:
    case Jump::DsAlFine:
    case Jump::DsAlCoda:
        return true;
    case Jump::None:
    case Jump::Segno:
    case Jump::Coda:
    case Jump::Fine:
        return false;
    }
    return false;
}

Jump playbackJumpToMarker(Jump marker)
{
    switch (marker) {
    case Jump::Segno:
        return Jump::DalSegno;
    case Jump::Coda:
        return Jump::ToCoda;
    case Jump::None:
    case Jump::ToCoda:
    case Jump::Fine:
    case Jump::DaCapo:
    case Jump::DCAlFine:
    case Jump::DCAlCoda:
    case Jump::DalSegno:
    case Jump::DsAlFine:
    case Jump::DsAlCoda:
        return marker;
    }
    return marker;
}

Jump classifyVisualJumpForTextRepeatId(const musx::dom::MusxInstance<musx::dom::others::TextRepeatAssign>& assignment, musx::dom::Cmper textRepeatId)
{
    const auto def = assignment->getDocument()->getOthers()->get<musx::dom::others::TextRepeatDef>(assignment->getRequestedPartId(), textRepeatId);
    return classifyVisualJump(def);
}

Jump classifyTargetMarker(const musx::dom::MusxInstance<musx::dom::others::TextRepeatAssign>& assignment)
{
    if (const auto targetMeasure = assignment->calcTargetMeasure()) {
        const auto targetAssigns = assignment->getDocument()->getOthers()->getArray<musx::dom::others::TextRepeatAssign>(
            assignment->getRequestedPartId(), *targetMeasure);
        for (const auto& targetAssign : targetAssigns) {
            if (targetAssign && targetAssign->jumpAction == musx::dom::others::RepeatActionType::NoJump) {
                const auto targetVisual = classifyVisualJumpForTextRepeatId(assignment, targetAssign->textRepeatId);
                if (targetVisual == Jump::Segno || targetVisual == Jump::Coda) {
                    return targetVisual;
                }
            }
        }
    }
    return Jump::None;
}

Jump classifyPlaybackJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatAssign>& assignment, Jump visual)
{
    if (!assignment) {
        return Jump::None;
    }

    using RepeatActionType = musx::dom::others::RepeatActionType;
    switch (assignment->jumpAction) {
    case RepeatActionType::NoJump:
        return (visual == Jump::Segno || visual == Jump::Coda) ? visual : Jump::None;
    case RepeatActionType::Stop:
        return Jump::Fine;
    case RepeatActionType::JumpToMark:
        if (isJumpCommand(visual)) {
            return visual;
        }
        return playbackJumpToMarker(classifyVisualJumpForTextRepeatId(assignment, assignment->targetValue));
    case RepeatActionType::JumpAbsolute:
    case RepeatActionType::JumpRelative:
        if (isJumpCommand(visual)) {
            return visual;
        }
        if (const auto targetMarker = classifyTargetMarker(assignment); targetMarker != Jump::None) {
            return playbackJumpToMarker(targetMarker);
        }
        return playbackJumpToMarker(visual);
    case RepeatActionType::JumpAuto:
        return visual;
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
JumpClassification classifyJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatDef>& def)
{
    const auto visual = classifyVisualJump(def);
    return JumpClassification{ visual, visual };
}

JumpClassification classifyJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatAssign>& assignment)
{
    if (!assignment) {
        return {};
    }
    const auto visual = classifyVisualJumpForTextRepeatId(assignment, assignment->textRepeatId);
    return JumpClassification{ visual, classifyPlaybackJump(assignment, visual) };
}

} // namespace denigma::classify
