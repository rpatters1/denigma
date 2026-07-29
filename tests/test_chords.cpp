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
#include "gtest/gtest.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "denigma/classify/chords.h"
#include "formats/enigmaxml/enigmaxml.h"

using namespace musx::dom;

namespace {

struct FontContext
{
    DocumentPtr document;
    std::shared_ptr<FontInfo> font;
};

// Chord suffixes are classified relative to their font size: a vertical offset of at least half the
// em is a stacked line, and anything smaller is a glyph nudge within one line.
constexpr int SUFFIX_FONT_SIZE = 12;
constexpr Evpu STACKED_OFFSET = Evpu(SUFFIX_FONT_SIZE * musx::dom::EVPU_PER_POINT);
constexpr Evpu NUDGE_OFFSET = Evpu(SUFFIX_FONT_SIZE * musx::dom::EVPU_PER_POINT / 4.0);

FontContext makeFontContext(const std::string& fontName)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>)xml";
    xml += fontName;
    xml += R"xml(</name>
    </fontName>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    auto font = std::make_shared<FontInfo>(document);
    font->fontId = 1;
    font->fontSize = SUFFIX_FONT_SIZE;
    return { document, font };
}

MusxInstanceList<others::ChordSuffixElement> makeSuffix(const FontContext& context, char32_t symbol)
{
    MusxInstanceList<others::ChordSuffixElement> suffix(context.document, SCORE_PARTID);
    auto element = std::make_shared<others::ChordSuffixElement>(
        context.document, SCORE_PARTID, CommonClassBase::ShareMode::All, Cmper(1), Inci(0));
    element->font = context.font;
    element->symbol = symbol;
    suffix.push_back(element);
    return suffix;
}

void appendSuffixElement(
    MusxInstanceList<others::ChordSuffixElement>& suffix,
    const FontContext& context,
    char32_t symbol,
    Inci index,
    Evpu verticalOffset = 0)
{
    auto element = std::make_shared<others::ChordSuffixElement>(
        context.document, SCORE_PARTID, CommonClassBase::ShareMode::All, Cmper(1), index);
    element->font = context.font;
    element->symbol = symbol;
    element->ydisp = verticalOffset;
    suffix.push_back(element);
}

DocumentPtr loadChordsFixture()
{
    const auto inputPath = std::filesystem::path("inputs") / "chords.musx";
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

std::vector<denigma::classify::ChordSuffixClassification> classifyFixtureSuffixes(const DocumentPtr& document)
{
    std::vector<denigma::classify::ChordSuffixClassification> classifications;
    const auto assignments = document->getDetails()->getArray<details::ChordAssign>(SCORE_PARTID);
    for (const auto& assignment : assignments) {
        if (assignment->showSuffix && assignment->suffixId != 0) {
            classifications.emplace_back(denigma::classify::classifyChordSuffix(assignment->getChordSuffix()));
        }
    }
    return classifications;
}

} // namespace

TEST(ChordSuffixClassifier, ConvertsSmuflSymbolsToUnicodeText)
{
    const auto context = makeFontContext("Finale Maestro");
    const auto classification = denigma::classify::classifyChordSuffix(makeSuffix(context, 0xE871));

    EXPECT_EQ(classification.calcText(), musx::util::EnigmaString::fromU8(u8"ø"));
    EXPECT_FALSE(classification.hasUnrecognizedGlyphs);
}

TEST(ChordSuffixClassifier, ClassifiesAnAbsentSuffixAsMajor)
{
    const auto classification = denigma::classify::classifyChordSuffix();

    EXPECT_TRUE(classification.strings.empty());
    ASSERT_TRUE(classification.quality);
    EXPECT_EQ(*classification.quality, denigma::classify::chord::Quality::Major);
}

TEST(ChordSuffixClassifier, ConvertsLegacyFinaleChordSymbols)
{
    const auto context = makeFontContext("Maestro");
    const auto classification = denigma::classify::classifyChordSuffix(makeSuffix(context, 111));

    EXPECT_EQ(classification.calcText(), musx::util::EnigmaString::fromU8(u8"°"));
    EXPECT_FALSE(classification.hasUnrecognizedGlyphs);
}

TEST(ChordSuffixClassifier, SeparatesStackedSuffixStrings)
{
    const auto context = makeFontContext("Times New Roman");
    auto suffix = makeSuffix(context, U'm');
    auto stacked = std::make_shared<others::ChordSuffixElement>(
        context.document, SCORE_PARTID, CommonClassBase::ShareMode::All, Cmper(1), Inci(1));
    stacked->font = context.font;
    stacked->symbol = 9;
    stacked->isNumber = true;
    stacked->ydisp = STACKED_OFFSET;
    suffix.push_back(stacked);
    auto trailing = std::make_shared<others::ChordSuffixElement>(
        context.document, SCORE_PARTID, CommonClassBase::ShareMode::All, Cmper(1), Inci(2));
    trailing->font = context.font;
    trailing->symbol = U'7';
    suffix.push_back(trailing);

    const auto classification = denigma::classify::classifyChordSuffix(suffix);
    ASSERT_EQ(classification.strings.size(), 3);
    EXPECT_EQ(classification.strings[0].text, "m");
    EXPECT_EQ(classification.strings[0].position, denigma::classify::chord::SuffixString::Position::Inline);
    EXPECT_EQ(classification.strings[1].text, "9");
    EXPECT_EQ(classification.strings[1].position, denigma::classify::chord::SuffixString::Position::Above);
    EXPECT_EQ(classification.strings[2].text, "7");
    EXPECT_EQ(classification.strings[2].position, denigma::classify::chord::SuffixString::Position::Inline);
    EXPECT_EQ(classification.calcText(), "m97");
}

TEST(ChordSuffixClassifier, IdentifiesOuterParentheses)
{
    const auto context = makeFontContext("Times New Roman");
    auto suffix = makeSuffix(context, U'(');
    appendSuffixElement(suffix, context, U'm', 1);
    appendSuffixElement(suffix, context, U'7', 2);
    appendSuffixElement(suffix, context, U')', 3);

    const auto classification = denigma::classify::classifyChordSuffix(suffix);
    EXPECT_EQ(classification.calcText(), "(m7)");
    EXPECT_TRUE(classification.hasOuterParentheses);
}

TEST(ChordSuffixClassifier, DoesNotTreatInternalParenthesesAsOuter)
{
    const auto context = makeFontContext("Times New Roman");
    auto suffix = makeSuffix(context, U'm');
    appendSuffixElement(suffix, context, U'(', 1);
    appendSuffixElement(suffix, context, U'7', 2);
    appendSuffixElement(suffix, context, U')', 3);

    const auto classification = denigma::classify::classifyChordSuffix(suffix);
    EXPECT_EQ(classification.calcText(), "m(7)");
    EXPECT_FALSE(classification.hasOuterParentheses);
}

TEST(ChordSuffixClassifier, ParenthesizesDegreesOnlyWhenTheSuffixHasDegrees)
{
    const auto context = makeFontContext("Times New Roman");
    auto majorMinor = makeSuffix(context, U'm');
    appendSuffixElement(majorMinor, context, U'(', 1);
    appendSuffixElement(majorMinor, context, U'M', 2);
    appendSuffixElement(majorMinor, context, U'7', 3);
    appendSuffixElement(majorMinor, context, U')', 4);

    const auto withoutDegrees = denigma::classify::classifyChordSuffix(majorMinor);
    ASSERT_TRUE(withoutDegrees.quality);
    EXPECT_EQ(*withoutDegrees.quality, denigma::classify::chord::Quality::MajorMinor);
    EXPECT_TRUE(withoutDegrees.degrees.empty());
    EXPECT_FALSE(withoutDegrees.parenthesizeDegrees);

    auto halfDiminished = makeSuffix(context, U'm');
    appendSuffixElement(halfDiminished, context, U'7', 1);
    appendSuffixElement(halfDiminished, context, U'(', 2);
    appendSuffixElement(halfDiminished, context, U'b', 3);
    appendSuffixElement(halfDiminished, context, U'5', 4);
    appendSuffixElement(halfDiminished, context, U')', 5);

    const auto withDegrees = denigma::classify::classifyChordSuffix(halfDiminished);
    ASSERT_EQ(withDegrees.degrees.size(), 1);
    EXPECT_TRUE(withDegrees.parenthesizeDegrees);
}

TEST(ChordSuffixClassifier, ParenthesizesDegreesWhenTheParenthesesEncloseTheWholeSuffix)
{
    const auto context = makeFontContext("Times New Roman");
    const auto classifyDegreeGroup = [&context](Evpu addedDegreeOffset) {
        auto suffix = makeSuffix(context, U'(');
        Inci index = 1;
        for (const auto character : std::u32string_view(U"add9")) {
            appendSuffixElement(suffix, context, character, index++, addedDegreeOffset);
        }
        for (const auto character : std::u32string_view(U"omit3")) {
            appendSuffixElement(suffix, context, character, index++);
        }
        appendSuffixElement(suffix, context, U')', index);
        return denigma::classify::classifyChordSuffix(suffix);
    };

    const auto stacked = classifyDegreeGroup(STACKED_OFFSET);
    EXPECT_TRUE(stacked.hasOuterParentheses);
    ASSERT_EQ(stacked.degrees.size(), 2);
    EXPECT_TRUE(stacked.parenthesizeDegrees);
    EXPECT_TRUE(stacked.stackDegrees);

    // Finale nudges glyphs off the baseline within one line of a suffix, which is not stacking.
    const auto nudged = classifyDegreeGroup(NUDGE_OFFSET);
    ASSERT_EQ(nudged.degrees.size(), 2);
    EXPECT_TRUE(nudged.parenthesizeDegrees);
    EXPECT_FALSE(nudged.stackDegrees);
}

TEST(ChordSuffixClassifier, RetainsTheSeventhInSeventhSuspensions)
{
    const auto context = makeFontContext("Times New Roman");
    auto suffix = makeSuffix(context, U'7');
    appendSuffixElement(suffix, context, U's', 1);
    appendSuffixElement(suffix, context, U'u', 2);
    appendSuffixElement(suffix, context, U's', 3);
    appendSuffixElement(suffix, context, U'4', 4);

    const auto classification = denigma::classify::classifyChordSuffix(suffix);
    ASSERT_TRUE(classification.quality);
    EXPECT_EQ(*classification.quality, denigma::classify::chord::Quality::SuspendedFourth);
    ASSERT_EQ(classification.degrees.size(), 1);
    EXPECT_EQ(classification.degrees.front().value, 7);
    EXPECT_EQ(classification.degrees.front().type, denigma::classify::chord::Degree::Type::Add);
    EXPECT_TRUE(classification.degrees.front().impliedByText);
}

TEST(ChordSuffixClassifier, TreatsPlainSuspensionResolutionsAsDisplayOnly)
{
    const auto context = makeFontContext("Times New Roman");
    auto suffix = makeSuffix(context, U's');
    appendSuffixElement(suffix, context, U'u', 1);
    appendSuffixElement(suffix, context, U's', 2);
    appendSuffixElement(suffix, context, U'2', 3);
    appendSuffixElement(suffix, context, U'-', 4);
    appendSuffixElement(suffix, context, U'3', 5);

    const auto classification = denigma::classify::classifyChordSuffix(suffix);
    ASSERT_TRUE(classification.quality);
    EXPECT_EQ(*classification.quality, denigma::classify::chord::Quality::SuspendedSecond);
    EXPECT_TRUE(classification.degrees.empty());
}

TEST(ChordSuffixClassifierFixture, ReconstructsEveryDisplayedSuffix)
{
    const auto document = loadChordsFixture();
    ASSERT_TRUE(document);

    const auto classifications = classifyFixtureSuffixes(document);
    ASSERT_FALSE(classifications.empty());
    for (const auto& classification : classifications) {
        EXPECT_FALSE(classification.calcText().empty());
        EXPECT_FALSE(classification.hasUnrecognizedGlyphs) << classification.calcText();
        EXPECT_TRUE(classification.quality) << classification.calcText();
    }
}

TEST(ChordSuffixClassifierFixture, RetainsInternalParenthesesAndStackedStrings)
{
    const auto document = loadChordsFixture();
    ASSERT_TRUE(document);

    const auto classifications = classifyFixtureSuffixes(document);
    const auto majorMinor = std::find_if(classifications.begin(), classifications.end(), [](const auto& classification) {
        const auto text = classification.calcText();
        return text.starts_with("m(") && text.ends_with(')');
    });
    ASSERT_NE(majorMinor, classifications.end());
    EXPECT_FALSE(majorMinor->hasOuterParentheses);

    const auto stacked = std::find_if(classifications.begin(), classifications.end(), [](const auto& classification) {
        return std::any_of(classification.strings.begin(), classification.strings.end(), [](const auto& string) {
            return string.position != denigma::classify::chord::SuffixString::Position::Inline;
        });
    });
    ASSERT_NE(stacked, classifications.end());
    EXPECT_GT(stacked->strings.size(), 1);
    EXPECT_TRUE(stacked->stackDegrees) << stacked->calcText();
}
