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
#include "denigma/classify/keyboard_pedals.h"

#include <array>
#include <string>

#include "classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

namespace {

std::string normalizePedalText(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool pendingSpace = false;
    for (const char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (ch != '*' && (utils::isSpace(uch) || utils::isPunctuation(uch))) {
            pendingSpace = !result.empty();
            continue;
        }
        if (pendingSpace) {
            result.push_back(' ');
            pendingSpace = false;
        }
        result.push_back(utils::toLowerCase(ch));
    }
    return result;
}

std::optional<keyboardpedal::Type> classifyNormalizedPedalText(std::string_view text)
{
    std::string normalizedStorage = normalizePedalText(text);
    std::string_view normalized = normalizedStorage;
    constexpr std::array trailingQualifiers = {
        std::string_view{ " sempre" }, std::string_view{ " simile" }, std::string_view{ " ad lib" }
    };
    for (bool removed = true; removed;) {
        removed = false;
        for (const auto qualifier : trailingQualifiers) {
            if (normalized.ends_with(qualifier)) {
                normalized.remove_suffix(qualifier.size());
                removed = true;
                break;
            }
        }
    }
    constexpr std::array leadingQualifiers = { std::string_view{ "sempre " }, std::string_view{ "con " } };
    for (const auto qualifier : leadingQualifiers) {
        if (normalized.starts_with(qualifier)) {
            normalized.remove_prefix(qualifier.size());
            break;
        }
    }

    using Type = keyboardpedal::Type;
    if (normalized == "*") {
        return Type::PedalUp;
    }
    if (normalized == "senza ped") {
        return Type::PedalUp;
    }
    if (normalized == "sost" || normalized == "sost ped") {
        return Type::PedalTwo;
    }
    if (normalized == "una corda") {
        return Type::PedalThree;
    }
    if (!normalized.starts_with("ped")) {
        return std::nullopt;
    }

    normalized.remove_prefix(3);
    while (!normalized.empty() && normalized.front() == ' ') {
        normalized.remove_prefix(1);
    }
    while (!normalized.empty() && normalized.back() == ' ') {
        normalized.remove_suffix(1);
    }
    if (normalized.empty() || normalized == "i" || normalized == "1") {
        return Type::PedalOne;
    }
    if (normalized == "ii" || normalized == "2") {
        return Type::PedalTwo;
    }
    if (normalized == "iii" || normalized == "3") {
        return Type::PedalThree;
    }
    return std::nullopt;
}

std::optional<keyboardpedal::Type> standaloneGlyphType(std::string_view glyphName)
{
    using Type = keyboardpedal::Type;
    if (glyphName == "keyboardPedalHalf" || glyphName == "keyboardPedalHalf2" || glyphName == "keyboardPedalHalf3") {
        return Type::HalfPedal;
    }
    if (glyphName == "keyboardPedalUpNotch") {
        return Type::PedalUpNotch;
    }
    if (glyphName == "keyboardPedalUpSpecial") {
        return Type::PedalUpSpecial;
    }
    if (glyphName == "keyboardPedalHookStart") {
        return Type::HookStart;
    }
    if (glyphName == "keyboardPedalHookEnd") {
        return Type::HookEnd;
    }
    if (glyphName == "keyboardPedalHyphen") {
        return Type::Hyphen;
    }
    return std::nullopt;
}

} // namespace

std::optional<KeyboardPedalClassification> classifyKeyboardPedal(std::string_view text)
{
    if (const auto type = classifyNormalizedPedalText(text)) {
        return KeyboardPedalClassification{ *type, false };
    }
    return std::nullopt;
}

std::optional<KeyboardPedalClassification> classifyKeyboardPedal(
    const musx::util::EnigmaParsingContext& textContext)
{
    if (!textContext) {
        return std::nullopt;
    }

    std::string symbolicText;
    std::optional<keyboardpedal::Type> standaloneType;
    bool recognizedGlyph = false;
    const auto chunks = textContext.collectEnigmaTextChunks(
        musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (const auto& chunk : chunks) {
        if (!chunk.styles.font || chunk.styles.font->hidden || chunk.text.empty()) {
            continue;
        }
        for (utils::Utf8Iterator iter(chunk.text); !iter.atEnd(); iter.next()) {
            if (!iter.valid()) {
                return std::nullopt;
            }
            const auto glyphName = detail::glyphNameForFont(chunk.styles.font, iter->codepoint);
            if (glyphName) {
                if (const auto type = standaloneGlyphType(*glyphName)) {
                    if (standaloneType && *standaloneType != *type) {
                        return std::nullopt;
                    }
                    standaloneType = *type;
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalUp") {
                    standaloneType = keyboardpedal::Type::PedalUp;
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalPed" || *glyphName == "keyboardPedalPedNoDot") {
                    symbolicText += "ped";
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalP") {
                    symbolicText += 'p';
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalE") {
                    symbolicText += 'e';
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalD") {
                    symbolicText += 'd';
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalDot") {
                    symbolicText += '.';
                    recognizedGlyph = true;
                    continue;
                }
                if (*glyphName == "keyboardPedalSost" || *glyphName == "keyboardPedalSostNoDot"
                    || *glyphName == "keyboardPedalS") {
                    symbolicText += "sost";
                    recognizedGlyph = true;
                    continue;
                }
            }
            if (iter->codepoint <= 0x7F) {
                symbolicText.push_back(static_cast<char>(iter->codepoint));
            } else {
                return std::nullopt;
            }
        }
    }

    if (standaloneType) {
        const std::string normalizedRemainder = normalizePedalText(symbolicText);
        if (!normalizedRemainder.empty() && normalizedRemainder != "sempre") {
            return std::nullopt;
        }
        return KeyboardPedalClassification{ *standaloneType, true };
    }
    if (const auto type = classifyNormalizedPedalText(symbolicText)) {
        return KeyboardPedalClassification{ *type, recognizedGlyph };
    }
    return std::nullopt;
}

} // namespace denigma::classify
