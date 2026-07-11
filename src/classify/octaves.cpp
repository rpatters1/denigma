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
#include "denigma/classify/octaves.h"

#include <string>

#include "classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma::classify {

namespace {

using octave::Direction;

/// Accumulates the evidence found in a marking. Conflicting evidence poisons the result.
///
/// Direction evidence has two strengths. "va"/"ma" spellings (and the SMuFL Alta glyph
/// variants) imply alta only weakly, because markings like "8va bassa" and below-staff
/// "8va" lines mean bassa. Explicit evidence ("vb", "mb", "ba", "bassa", "alta", and the
/// Bassa glyph variants) overrides weak evidence; two conflicting explicit directions
/// (or two conflicting weak ones) poison the result.
struct MarkingParts
{
    enum class Strength
    {
        None,
        Weak,
        Explicit
    };

    std::optional<int> magnitude;
    Direction direction{ Direction::Unknown };
    Strength strength{ Strength::None };
    bool conflict{};

    void addMagnitude(int value)
    {
        if (magnitude && *magnitude != value) {
            conflict = true;
        } else {
            magnitude = value;
        }
    }

    void addDirection(Direction value, Strength valueStrength)
    {
        if (value == Direction::Unknown || valueStrength == Strength::None) {
            return;
        }
        if (direction == Direction::Unknown) {
            direction = value;
            strength = valueStrength;
            return;
        }
        if (direction == value) {
            strength = std::max(strength, valueStrength);
            return;
        }
        if (valueStrength == strength) {
            conflict = true;
        } else if (valueStrength == Strength::Explicit) {
            direction = value;
            strength = valueStrength;
        }
    }
};

std::string normalizeOctaveText(std::string_view text)
{
    std::string result;
    result.reserve(text.size());
    bool pendingSpace = false;
    for (const char ch : text) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (utils::isSpace(uch) || utils::isPunctuation(uch)) {
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

struct SuffixDirection
{
    Direction direction;
    MarkingParts::Strength strength;
};

std::optional<SuffixDirection> directionForSuffix(std::string_view suffix)
{
    if (suffix == "va" || suffix == "ma") {
        return SuffixDirection{ Direction::Up, MarkingParts::Strength::Weak };
    }
    if (suffix == "vb" || suffix == "mb" || suffix == "ba") {
        return SuffixDirection{ Direction::Down, MarkingParts::Strength::Explicit };
    }
    return std::nullopt;
}

/// Parses normalized text into marking parts. Returns false when the text contains
/// anything other than a recognizable octave marking. Empty text parses successfully
/// and contributes nothing.
bool parseNormalizedOctaveText(std::string_view normalized, MarkingParts& parts)
{
    size_t pos = 0;
    while (pos < normalized.size()) {
        size_t end = normalized.find(' ', pos);
        if (end == std::string_view::npos) {
            end = normalized.size();
        }
        const std::string_view token = normalized.substr(pos, end - pos);
        pos = end + 1;
        if (token.empty()) {
            continue;
        }
        if (token == "bassa") {
            parts.addDirection(Direction::Down, MarkingParts::Strength::Explicit);
            continue;
        }
        if (token == "alta") {
            parts.addDirection(Direction::Up, MarkingParts::Strength::Explicit);
            continue;
        }

        size_t digits = 0;
        while (digits < token.size() && token[digits] >= '0' && token[digits] <= '9') {
            ++digits;
        }
        const std::string_view number = token.substr(0, digits);
        const std::string_view suffix = token.substr(digits);

        if (number.empty()) {
            // A pure suffix token composes with glyph-supplied magnitude (e.g., the
            // SMuFL "8" glyph followed by letter glyphs spelling "va").
            if (const auto direction = directionForSuffix(suffix)) {
                parts.addDirection(direction->direction, direction->strength);
                continue;
            }
            return false;
        }

        int magnitude = 0;
        if (number == "8") {
            magnitude = 1;
        } else if (number == "15" || number == "16") {
            magnitude = 2;
        } else if (number == "22") {
            magnitude = 3;
        } else {
            return false;
        }
        if (!suffix.empty()) {
            const auto direction = directionForSuffix(suffix);
            if (!direction) {
                return false;
            }
            parts.addDirection(direction->direction, direction->strength);
        }
        parts.addMagnitude(magnitude);
    }
    return true;
}

/// Glyph-supplied marking parts for the SMuFL octave glyphs. Returns false when the
/// glyph is not part of an octave marking.
bool applyOctaveGlyph(std::string_view glyphName, MarkingParts& parts, std::string& asciiText, bool& sawNumberGlyph)
{
    struct GlyphSpec
    {
        std::string_view name;
        int magnitude;
        Direction direction;
        MarkingParts::Strength strength;
    };
    // The Alta glyph variants render as "8va"-style markings, so like the "va" text
    // suffix they imply alta only weakly. The Bassa variants are explicit.
    static constexpr GlyphSpec numberGlyphs[] = {
        { "ottava", 1, Direction::Unknown, MarkingParts::Strength::None },
        { "ottavaAlta", 1, Direction::Up, MarkingParts::Strength::Weak },
        { "ottavaBassa", 1, Direction::Down, MarkingParts::Strength::Explicit },
        { "ottavaBassaBa", 1, Direction::Down, MarkingParts::Strength::Explicit },
        { "ottavaBassaVb", 1, Direction::Down, MarkingParts::Strength::Explicit },
        { "quindicesima", 2, Direction::Unknown, MarkingParts::Strength::None },
        { "quindicesimaAlta", 2, Direction::Up, MarkingParts::Strength::Weak },
        { "quindicesimaBassa", 2, Direction::Down, MarkingParts::Strength::Explicit },
        { "quindicesimaBassaMb", 2, Direction::Down, MarkingParts::Strength::Explicit },
        { "ventiduesima", 3, Direction::Unknown, MarkingParts::Strength::None },
        { "ventiduesimaAlta", 3, Direction::Up, MarkingParts::Strength::Weak },
        { "ventiduesimaBassa", 3, Direction::Down, MarkingParts::Strength::Explicit },
        { "ventiduesimaBassaMb", 3, Direction::Down, MarkingParts::Strength::Explicit },
    };
    for (const auto& spec : numberGlyphs) {
        if (glyphName == spec.name) {
            parts.addMagnitude(spec.magnitude);
            parts.addDirection(spec.direction, spec.strength);
            sawNumberGlyph = true;
            return true;
        }
    }
    if (glyphName == "octaveBassa") {
        parts.addDirection(Direction::Down, MarkingParts::Strength::Explicit);
        return true;
    }
    if (glyphName == "octaveParensLeft" || glyphName == "octaveParensRight") {
        return true;
    }
    // Letter glyphs compose the marking's suffix (e.g., "v" + "a" after an "8" glyph).
    static constexpr std::pair<std::string_view, char> letterGlyphs[] = {
        { "octaveBaselineA", 'a' }, { "octaveSuperscriptA", 'a' },
        { "octaveBaselineB", 'b' }, { "octaveSuperscriptB", 'b' },
        { "octaveBaselineM", 'm' }, { "octaveSuperscriptM", 'm' },
        { "octaveBaselineV", 'v' }, { "octaveSuperscriptV", 'v' },
    };
    for (const auto& [name, letter] : letterGlyphs) {
        if (glyphName == name) {
            asciiText.push_back(letter);
            return true;
        }
    }
    return false;
}

} // namespace

std::optional<OctaveMarkingClassification> classifyOctaveMarking(std::string_view text)
{
    MarkingParts parts;
    if (!parseNormalizedOctaveText(normalizeOctaveText(text), parts) || parts.conflict || !parts.magnitude) {
        return std::nullopt;
    }
    if (parts.direction == Direction::Unknown) {
        // A bare ASCII number is too weak to classify on its own.
        return std::nullopt;
    }
    return OctaveMarkingClassification{
        *parts.magnitude, parts.direction, parts.strength == MarkingParts::Strength::Explicit, false };
}

std::optional<OctaveMarkingClassification> classifyOctaveMarking(
    const musx::util::EnigmaParsingContext& textContext)
{
    if (!textContext) {
        return std::nullopt;
    }

    MarkingParts parts;
    std::string asciiText;
    bool sawGlyph = false;
    bool sawNumberGlyph = false;
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
                if (applyOctaveGlyph(*glyphName, parts, asciiText, sawNumberGlyph)) {
                    sawGlyph = true;
                    continue;
                }
                // A recognized glyph that is not part of an octave marking rules the text out.
                return std::nullopt;
            }
            if (iter->codepoint <= 0x7F) {
                asciiText.push_back(static_cast<char>(iter->codepoint));
            } else {
                return std::nullopt;
            }
        }
    }

    if (!parseNormalizedOctaveText(normalizeOctaveText(asciiText), parts) || parts.conflict || !parts.magnitude) {
        return std::nullopt;
    }
    if (parts.direction == Direction::Unknown && !sawNumberGlyph) {
        // A bare ASCII number is too weak to classify; a bare octave glyph is unmistakable.
        return std::nullopt;
    }
    return OctaveMarkingClassification{
        *parts.magnitude, parts.direction, parts.strength == MarkingParts::Strength::Explicit, sawGlyph };
}

} // namespace denigma::classify
