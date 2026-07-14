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
#include "denigma/classify/chords.h"

#include <algorithm>
#include <cstdint>
#include <cctype>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "smufl_mapping.h"

namespace denigma {
namespace classify {

namespace {

// Chord-text parsing is adapted from the MIT-licensed Dolet for Sibelius source:
// https://github.com/MakeMusicInc/DoletSibelius/blob/main/Dolet8.plg

std::string prefixText(musx::dom::others::ChordSuffixElement::Prefix prefix)
{
    using Prefix = musx::dom::others::ChordSuffixElement::Prefix;
    switch (prefix) {
    case Prefix::None: return {};
    case Prefix::Minus: return musx::util::EnigmaString::fromU8(u8"−");
    case Prefix::Plus: return "+";
    case Prefix::Sharp: return musx::util::EnigmaString::fromU8(u8"♯");
    case Prefix::Flat: return musx::util::EnigmaString::fromU8(u8"♭");
    }
    return {};
}

std::optional<std::string_view> unicodeTextForGlyph(std::string_view glyphName)
{
    static const std::unordered_map<std::string_view, std::string_view> glyphText {
        { "accidentalFlat", "♭" },
        { "accidentalNatural", "♮" },
        { "accidentalSharp", "♯" },
        { "accidentalDoubleFlat", "𝄫" },
        { "accidentalDoubleSharp", "𝄪" },
        { "csymAccidentalFlat", "♭" },
        { "csymAccidentalFlatSmall", "♭" },
        { "csymAccidentalNatural", "♮" },
        { "csymAccidentalNaturalSmall", "♮" },
        { "csymAccidentalSharp", "♯" },
        { "csymAccidentalSharpSmall", "♯" },
        { "csymAccidentalDoubleFlat", "𝄫" },
        { "csymAccidentalDoubleFlatSmall", "𝄫" },
        { "csymAccidentalDoubleSharp", "𝄪" },
        { "csymAccidentalDoubleSharpSmall", "𝄪" },
        { "csymDiminished", "°" },
        { "csymDiminishedSmall", "°" },
        { "csymHalfDiminished", "ø" },
        { "csymHalfDiminishedSmall", "ø" },
        { "csymAugmented", "+" },
        { "csymAugmentedSmall", "+" },
        { "csymMajorSeventh", "Δ" },
        { "csymMajorSeventhSmall", "Δ" },
        { "csymMinor", "m" },
        { "csymMinorSmall", "m" },
        { "csymParensLeftTall", "(" },
        { "csymParensLeftVeryTall", "(" },
        { "csymParensRightTall", ")" },
        { "csymParensRightVeryTall", ")" },
        { "csymBracketLeftTall", "[" },
        { "csymBracketRightTall", "]" },
        { "csymAlteredBassSlash", "/" },
        { "csymDiagonalArrangementSlash", "/" },
    };
    const auto found = glyphText.find(glyphName);
    return found == glyphText.end() ? std::nullopt : std::optional<std::string_view>(found->second);
}

chord::SuffixString::Position suffixStringPosition(musx::dom::Evpu verticalOffset)
{
    if (verticalOffset > 0) {
        return chord::SuffixString::Position::Above;
    }
    if (verticalOffset < 0) {
        return chord::SuffixString::Position::Below;
    }
    return chord::SuffixString::Position::Inline;
}

bool hasOuterParentheses(std::string_view text)
{
    if (text.size() < 2 || text.front() != '(' || text.back() != ')') {
        return false;
    }

    int depth{};
    for (size_t index = 0; index < text.size(); ++index) {
        if (text[index] == '(') {
            ++depth;
        } else if (text[index] == ')') {
            --depth;
            if (depth < 0 || (depth == 0 && index + 1 != text.size())) {
                return false;
            }
        }
    }
    return depth == 0;
}

std::string normalizeForParsing(std::string text)
{
    for (auto& character : text) {
        if (character >= 'A' && character <= 'Z' && character != 'M') {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
    }
    const auto replaceAll = [&text](std::string_view from, std::string_view to) {
        for (size_t position{}; (position = text.find(from, position)) != std::string::npos; position += to.size()) {
            text.replace(position, from.size(), to);
        }
    };
    replaceAll("maj", "M");
    replaceAll("dim", "°");
    replaceAll("−", "-");
    replaceAll("♭", "b");
    replaceAll("♯", "#");
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char character) {
        return std::isspace(character) || character == '(' || character == ')' || character == '[' || character == ']' || character == ',';
    }), text.end());
    return text;
}

void addDegree(
    ChordSuffixClassification& result,
    int value,
    chord::Degree::Type type,
    int alteration = 0,
    bool printObject = true);

bool parseDegrees(std::string_view text, ChordSuffixClassification& result)
{
    using DegreeType = chord::Degree::Type;
    for (size_t position{}; position < text.size();) {
        auto type = DegreeType::Alter;
        if (text.substr(position, 3) == "add") {
            type = DegreeType::Add;
            position += 3;
        } else if (text.substr(position, 4) == "omit") {
            type = DegreeType::Remove;
            position += 4;
        } else if (text.substr(position, 2) == "no") {
            type = DegreeType::Remove;
            position += 2;
        }
        int alteration{};
        if (position < text.size() && (text[position] == 'b' || text[position] == '#')) {
            alteration = text[position++] == 'b' ? -1 : 1;
        }
        const auto numberStart = position;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) {
            ++position;
        }
        if (numberStart == position) {
            return false;
        }
        const auto value = std::stoi(std::string(text.substr(numberStart, position - numberStart)));
        if (type == DegreeType::Alter && alteration == 0) {
            type = DegreeType::Add;
        }
        addDegree(result, value, type, alteration);
    }
    return true;
}

void addDegree(ChordSuffixClassification& result, int value, chord::Degree::Type type, int alteration, bool printObject)
{
    result.degrees.emplace_back(chord::Degree{ value, alteration, type, printObject });
}

void classifyText(ChordSuffixClassification& result)
{
    const auto text = result.calcText();
    result.parenthesizeDegrees = text.find('(') != std::string::npos && !result.hasOuterParentheses;
    const auto parseText = normalizeForParsing(text);
    using Quality = chord::Quality;
    using DegreeType = chord::Degree::Type;
    if (parseText.empty()) { result.quality = Quality::Major; return; }
    if (parseText == "m") { result.quality = Quality::Minor; return; }
    if (parseText == "5") { result.quality = Quality::Power; return; }
    if (parseText == "6") { result.quality = Quality::MajorSixth; return; }
    if (parseText == "m6") { result.quality = Quality::MinorSixth; return; }
    if (parseText == "7") { result.quality = Quality::Dominant; return; }
    if (parseText == "9") { result.quality = Quality::DominantNinth; return; }
    if (parseText == "11") { result.quality = Quality::DominantEleventh; return; }
    if (parseText == "13") { result.quality = Quality::DominantThirteenth; return; }
    if (parseText == "M7") { result.quality = Quality::MajorSeventh; return; }
    if (parseText == "M9") { result.quality = Quality::MajorNinth; return; }
    if (parseText == "M11") { result.quality = Quality::MajorEleventh; return; }
    if (parseText == "M13") { result.quality = Quality::MajorThirteenth; return; }
    if (parseText == "m7") { result.quality = Quality::MinorSeventh; return; }
    if (parseText == "m9") { result.quality = Quality::MinorNinth; return; }
    if (parseText == "m11") { result.quality = Quality::MinorEleventh; return; }
    if (parseText == "m13") { result.quality = Quality::MinorThirteenth; return; }
    if (parseText == "mM7") { result.quality = Quality::MajorMinor; return; }
    if (parseText == "°") { result.quality = Quality::Diminished; return; }
    if (parseText == "°7" || parseText == "7°") { result.quality = Quality::DiminishedSeventh; return; }
    if (parseText == "ø7") { result.quality = Quality::HalfDiminished; return; }
    if (parseText == "sus" || parseText == "sus4") { result.quality = Quality::SuspendedFourth; return; }
    if (parseText == "sus2") { result.quality = Quality::SuspendedSecond; return; }
    if (parseText == "ped") { result.quality = Quality::Pedal; return; }
    if (parseText == "nc" || parseText == "n.c.") { result.quality = Quality::None; return; }
    if (parseText == "m7b5") { result.quality = Quality::MinorSeventh; addDegree(result, 5, DegreeType::Alter, -1); return; }
    const std::pair<std::string_view, Quality> suspendedPrefixes[] {
        { "13sus4", Quality::SuspendedFourth }, { "11sus4", Quality::SuspendedFourth },
        { "9sus4", Quality::SuspendedFourth }, { "7sus4", Quality::SuspendedFourth },
        { "7sus2", Quality::SuspendedSecond }, { "6sus4", Quality::SuspendedFourth },
        { "6sus2", Quality::SuspendedSecond }, { "sus4", Quality::SuspendedFourth },
        { "sus2", Quality::SuspendedSecond },
    };
    for (const auto& [prefix, quality] : suspendedPrefixes) {
        if (parseText == prefix || parseText == std::string(prefix) + "-3") {
            result.quality = quality;
            if (prefix.starts_with("7") || prefix.starts_with("9") || prefix.starts_with("11") || prefix.starts_with("13")) {
                addDegree(result, 7, DegreeType::Add, 0, false);
            }
            if (prefix.starts_with("9") || prefix.starts_with("11") || prefix.starts_with("13")) {
                addDegree(result, 9, DegreeType::Add, 0, false);
            }
            if (prefix.starts_with("11") || prefix.starts_with("13")) {
                addDegree(result, 11, DegreeType::Add, 0, false);
            }
            if (prefix.starts_with("13")) {
                addDegree(result, 13, DegreeType::Add, 0, false);
            }
            if (prefix.starts_with("6")) {
                addDegree(result, 6, DegreeType::Add, 0, false);
            }
            return;
        }
    }
    if (parseText == "add9no3" || parseText == "add9omit3") {
        result.quality = Quality::Major;
        result.stackDegrees = true;
        addDegree(result, 9, DegreeType::Add);
        addDegree(result, 3, DegreeType::Remove);
        return;
    }
    if (parseText == "7b13") { result.quality = Quality::Dominant; addDegree(result, 13, DegreeType::Add, -1); return; }
    if (parseText == "+#9b9" || parseText == "+add#9addb9") {
        result.quality = Quality::Augmented;
        result.stackDegrees = true;
        addDegree(result, 9, DegreeType::Add, 1);
        addDegree(result, 9, DegreeType::Add, -1);
        return;
    }

    const std::pair<std::string_view, Quality> qualityPrefixes[] {
        { "m7", Quality::MinorSeventh }, { "M7", Quality::MajorSeventh }, { "7", Quality::Dominant },
        { "m", Quality::Minor }, { "+", Quality::Augmented }, { "°", Quality::Diminished },
        { "ø", Quality::HalfDiminished }, { "", Quality::Major },
    };
    for (const auto& [prefix, quality] : qualityPrefixes) {
        if (parseText.starts_with(prefix)) {
            const auto degrees = std::string_view(parseText).substr(prefix.size());
            if (!degrees.empty() && parseDegrees(degrees, result)) {
                result.quality = quality;
                return;
            }
            result.degrees.clear();
        }
    }
}

} // namespace

std::string ChordSuffixClassification::calcText() const
{
    std::string result;
    for (const auto& string : strings) {
        result += string.text;
    }
    return result;
}

ChordSuffixClassification classifyChordSuffix()
{
    ChordSuffixClassification result;
    result.quality = chord::Quality::Major;
    return result;
}

ChordSuffixClassification classifyChordSuffix(
    const musx::dom::MusxInstanceList<musx::dom::others::ChordSuffixElement>& suffix)
{
    ChordSuffixClassification result;
    std::optional<musx::dom::Evpu> previousVerticalOffset;
    for (const auto& element : suffix) {
        const auto position = suffixStringPosition(element->ydisp);
        if (!previousVerticalOffset || *previousVerticalOffset != element->ydisp) {
            result.strings.emplace_back(chord::SuffixString{ {}, position });
        }
        previousVerticalOffset = element->ydisp;
        auto& string = result.strings.back().text;
        string += prefixText(element->prefix);
        if (element->isNumber) {
            string += std::to_string(static_cast<uint32_t>(element->symbol));
            continue;
        }

        if (element->font && !element->font->calcIsSMuFL()) {
            bool resolvedLegacyGlyph{};
            const auto glyphs = smufl_mapping::getAllLegacyGlyphInfo(element->font->getName(), element->symbol);
            for (const auto* glyph : glyphs) {
                if (const auto text = unicodeTextForGlyph(glyph->name)) {
                    string += *text;
                    resolvedLegacyGlyph = true;
                    break;
                }
            }
            if (resolvedLegacyGlyph) {
                continue;
            }
        }
        const auto glyphName = element->font
            ? smufl_mapping::getGlyphNameForFont(
                  element->font->getName(), element->symbol, element->font->calcIsSMuFL(),
                  smufl_mapping::SmuflGlyphSource::Finale)
            : nullptr;
        if (glyphName) {
            if (const auto text = unicodeTextForGlyph(*glyphName)) {
                string += *text;
                continue;
            }
        }
        if (element->symbol >= 0xE000 && element->symbol <= 0xF8FF) {
            result.hasUnrecognizedGlyphs = true;
        }
        string += musx::util::EnigmaString::toU8(element->symbol);
    }
    result.hasOuterParentheses = hasOuterParentheses(result.calcText());
    classifyText(result);
    return result;
}

} // namespace classify
} // namespace denigma
