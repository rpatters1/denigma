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

#include <filesystem>
#include <memory>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/articulations.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct FontContext
{
    DocumentPtr document;
    MusxInstance<FontInfo> fontInfo;
};

static FontContext makeFontContext(const std::string& fontName = "Maestro", int charsetVal = 4095)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>)xml";
    xml += std::to_string(charsetVal);
    xml += R"xml(</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>)xml";
    xml += fontName;
    xml += R"xml(</name>
    </fontName>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    auto fontInfo = std::make_shared<FontInfo>(document);
    fontInfo->fontId = 1;
    fontInfo->fontSize = 24;
    return { document, fontInfo };
}

static void expectSingleArticulationMark(
    const musx::dom::MusxInstance<FontInfo>& fontInfo,
    char32_t symbol,
    articulation::ArticulationMark::Type expectedType,
    const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, expectedType);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectSingleTechniqueMark(
    const musx::dom::MusxInstance<FontInfo>& fontInfo,
    char32_t symbol,
    articulation::TechniqueMark::Type expectedType,
    const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* technique = classification.as<articulation::TechniqueMark>();
    ASSERT_NE(technique, nullptr);
    EXPECT_EQ(technique->type, expectedType);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectHarmonMute(
    const musx::dom::MusxInstance<FontInfo>& fontInfo,
    char32_t symbol,
    articulation::HarmonMute::Qualifier expectedQualifier,
    const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* harmonMute = classification.as<articulation::HarmonMute>();
    ASSERT_NE(harmonMute, nullptr);
    EXPECT_EQ(harmonMute->qualifier, expectedQualifier);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

} // namespace

TEST(ArticulationClassification, ClassifiesUnicodeArticulationMarks)
{
    const auto fontContext = makeFontContext("Times New Roman", 0);
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 0x1D17B);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, articulation::ArticulationMark::Type::Accent);
    EXPECT_FALSE(classification.glyphName);
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, glyph::GlyphStyle::Placement::Automatic);
}

TEST(ArticulationClassification, ClassifiesLegacyGlyphArticulationMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 62);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, articulation::ArticulationMark::Type::Accent);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "articAccentAbove");
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, glyph::GlyphStyle::Placement::Above);
}

TEST(ArticulationClassification, AssignsGlyphStyleToEachComboArticulationMark)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 223);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 2u);
    EXPECT_EQ(articulation->marks[0].type, articulation::ArticulationMark::Type::Accent);
    EXPECT_EQ(articulation->marks[0].glyphStyle.placement, glyph::GlyphStyle::Placement::Below);
    EXPECT_EQ(articulation->marks[1].type, articulation::ArticulationMark::Type::Staccato);
    EXPECT_EQ(articulation->marks[1].glyphStyle.placement, glyph::GlyphStyle::Placement::Below);
}

TEST(ArticulationClassification, ClassifiesTechniqueGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE632, articulation::TechniqueMark::Type::BuzzPizzicato, "pluckedBuzzPizzicato");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE631, articulation::TechniqueMark::Type::SnapPizzicato, "pluckedSnapPizzicatoAbove");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xF432, articulation::TechniqueMark::Type::SnapPizzicato, "pluckedSnapPizzicatoBelowGerman");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE636, articulation::TechniqueMark::Type::Fingernails, "pluckedWithFingernails");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E1, articulation::TechniqueMark::Type::BrassFlip, "brassFlip");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E6, articulation::TechniqueMark::Type::BrassHalfMuted, "brassMuteHalfClosed");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E2, articulation::TechniqueMark::Type::BrassSmear, "brassSmear");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E3, articulation::TechniqueMark::Type::BrassBend, "brassBend");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xF767, articulation::TechniqueMark::Type::BrassLift, "brassLiftSlight");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE624, articulation::TechniqueMark::Type::ThumbPosition, "stringsThumbPosition");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE625, articulation::TechniqueMark::Type::ThumbPosition, "stringsThumbPositionTurned");
}

TEST(ArticulationClassification, ClassifiesHarmonMuteGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectHarmonMute(fontContext.fontInfo, 0xE5E8, articulation::HarmonMute::Qualifier::Closed, "brassHarmonMuteClosed");
    expectHarmonMute(fontContext.fontInfo, 0xE5E9, articulation::HarmonMute::Qualifier::HalfLeft, "brassHarmonMuteStemHalfLeft");
    expectHarmonMute(fontContext.fontInfo, 0xE5EA, articulation::HarmonMute::Qualifier::HalfRight, "brassHarmonMuteStemHalfRight");
    expectHarmonMute(fontContext.fontInfo, 0xE5EB, articulation::HarmonMute::Qualifier::Open, "brassHarmonMuteStemOpen");
}

TEST(ArticulationClassification, ClassifiesBrassArticulationGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectSingleArticulationMark(fontContext.fontInfo, 0xE5D0, articulation::ArticulationMark::Type::BrassScoop, "brassScoop");
    expectSingleArticulationMark(fontContext.fontInfo, 0xE5D4, articulation::ArticulationMark::Type::BrassDoit, "brassDoitShort");
    expectSingleArticulationMark(fontContext.fontInfo, 0xE5D7, articulation::ArticulationMark::Type::BrassFalloff, "brassFallLipShort");
    expectSingleArticulationMark(fontContext.fontInfo, 0xE5E0, articulation::ArticulationMark::Type::BrassPlop, "brassPlop");
    expectSingleArticulationMark(fontContext.fontInfo, 0xF768, articulation::ArticulationMark::Type::BrassFalloff, "brassFallSlight");
}

TEST(ArticulationClassification, ClassifiesFermatasBreathMarksAndArpeggios)
{
    const auto fontContext = makeFontContext();
    const auto fermataClassification = classifyArticulationSymbol(fontContext.fontInfo, 85);
    const auto* fermata = fermataClassification.as<articulation::Fermata>();
    ASSERT_NE(fermata, nullptr);
    EXPECT_EQ(fermata->shape, articulation::Fermata::Shape::Normal);
    EXPECT_EQ(fermata->duration, articulation::Fermata::Duration::Auto);

    const auto breathClassification = classifyArticulationSymbol(fontContext.fontInfo, 44);
    const auto* breath = breathClassification.as<articulation::BreathMark>();
    ASSERT_NE(breath, nullptr);
    EXPECT_EQ(breath->type, articulation::BreathMark::Type::Comma);

    const auto arpeggioClassification = classifyArticulationSymbol(fontContext.fontInfo, 103);
    const auto* arpeggio = arpeggioClassification.as<articulation::Arpeggio>();
    ASSERT_NE(arpeggio, nullptr);
    EXPECT_EQ(arpeggio->type, articulation::Arpeggio::Type::VerticalSegment);
}

TEST(ArticulationClassification, ClassifiesCaesuras)
{
    const auto fontContext = makeFontContext();
    const auto normalClassification = classifyArticulationSymbol(fontContext.fontInfo, 34);
    const auto* normal = normalClassification.as<articulation::Caesura>();
    ASSERT_NE(normal, nullptr);
    EXPECT_EQ(normal->type, articulation::Caesura::Type::Normal);

    const auto chantContext = makeFontContext("Finale Maestro");
    const auto chantClassification = classifyArticulationSymbol(chantContext.fontInfo, 0xE8F8);
    const auto* chant = chantClassification.as<articulation::Caesura>();
    ASSERT_NE(chant, nullptr);
    EXPECT_EQ(chant->type, articulation::Caesura::Type::Chant);
}

TEST(ArticulationClassification, ClassifiesOrnaments)
{
    const auto fontContext = makeFontContext();
    const auto trillClassification = classifyArticulationSymbol(fontContext.fontInfo, 217);
    const auto* trill = trillClassification.as<articulation::Ornament>();
    ASSERT_NE(trill, nullptr);
    EXPECT_EQ(trill->type, articulation::Ornament::Type::Trill);
    EXPECT_TRUE(trill->accidentals.empty());

    const auto mordentClassification = classifyArticulationSymbol(fontContext.fontInfo, 77);
    const auto* mordent = mordentClassification.as<articulation::Ornament>();
    ASSERT_NE(mordent, nullptr);
    EXPECT_EQ(mordent->type, articulation::Ornament::Type::Mordent);

    const auto invertedTurnContext = makeFontContext("Kousaku");
    const auto invertedTurnClassification = classifyArticulationSymbol(invertedTurnContext.fontInfo, 255);
    const auto* invertedTurn = invertedTurnClassification.as<articulation::Ornament>();
    ASSERT_NE(invertedTurn, nullptr);
    EXPECT_EQ(invertedTurn->type, articulation::Ornament::Type::InvertedTurn);

    const auto trillFlatContext = makeFontContext("Finale Maestro");
    const auto trillFlatClassification = classifyArticulationSymbol(trillFlatContext.fontInfo, 0xF5B2);
    const auto* trillFlat = trillFlatClassification.as<articulation::Ornament>();
    ASSERT_NE(trillFlat, nullptr);
    EXPECT_EQ(trillFlat->type, articulation::Ornament::Type::Trill);
    ASSERT_EQ(trillFlat->accidentals.size(), 1u);
    EXPECT_EQ(trillFlat->accidentals.front().accidental, articulation::Ornament::Accidental::Flat);
    EXPECT_EQ(trillFlat->accidentals.front().placement, musx::dom::VerticalPlacement::Above);

    const auto turnFlatSharpContext = makeFontContext("Finale Maestro");
    const auto turnFlatSharpClassification = classifyArticulationSymbol(turnFlatSharpContext.fontInfo, 0xF5B6);
    const auto* turnFlatSharp = turnFlatSharpClassification.as<articulation::Ornament>();
    ASSERT_NE(turnFlatSharp, nullptr);
    EXPECT_EQ(turnFlatSharp->type, articulation::Ornament::Type::Turn);
    ASSERT_EQ(turnFlatSharp->accidentals.size(), 2u);
    EXPECT_EQ(turnFlatSharp->accidentals[0].accidental, articulation::Ornament::Accidental::Flat);
    EXPECT_EQ(turnFlatSharp->accidentals[0].placement, musx::dom::VerticalPlacement::Above);
    EXPECT_EQ(turnFlatSharp->accidentals[1].accidental, articulation::Ornament::Accidental::Sharp);
    EXPECT_EQ(turnFlatSharp->accidentals[1].placement, musx::dom::VerticalPlacement::Below);
}

TEST(ArticulationClassification, ClassifiesTremoloMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 190);

    const auto* tremolo = classification.as<articulation::Tremolo>();
    ASSERT_NE(tremolo, nullptr);
    EXPECT_EQ(tremolo->style, articulation::Tremolo::Style::Measured);
    EXPECT_EQ(tremolo->marks, 3);
}

TEST(ArticulationClassification, ClassifiesAsciiParenthesesInNonSymbolFont)
{
    // A non-symbol charsetVal (SYMBOL_CHARSET_MAC is 4095) means literal ASCII characters, not glyph indices.
    const auto fontContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);

    const auto left = classifyArticulationSymbol(fontContext.fontInfo, U'(');
    const auto* leftParen = left.as<articulation::Parenthesis>();
    ASSERT_NE(leftParen, nullptr);
    EXPECT_EQ(leftParen->side, articulation::Parenthesis::Side::Left);
    EXPECT_FALSE(left.glyphName);

    const auto right = classifyArticulationSymbol(fontContext.fontInfo, U')');
    const auto* rightParen = right.as<articulation::Parenthesis>();
    ASSERT_NE(rightParen, nullptr);
    EXPECT_EQ(rightParen->side, articulation::Parenthesis::Side::Right);
    EXPECT_FALSE(right.glyphName);
}

TEST(ArticulationClassification, DoesNotClassifyAsciiParenthesesInSymbolFont)
{
    // In a symbol font, codepoints 40/41 are glyph indices (which happen to map to the same glyphs via
    // the legacy font maps), not literal ASCII text, so they must not be misidentified as plain text parens.
    const auto fontContext = makeFontContext();

    const auto left = classifyArticulationSymbol(fontContext.fontInfo, 40);
    const auto* leftParen = left.as<articulation::Parenthesis>();
    ASSERT_NE(leftParen, nullptr);
    EXPECT_EQ(leftParen->side, articulation::Parenthesis::Side::Left);
    ASSERT_TRUE(left.glyphName);
    EXPECT_EQ(left.glyphName.value(), "noteheadParenthesisLeft");
}

TEST(ArticulationClassification, ClassifiesParenthesisGlyphFromAnyFont)
{
    // Neither a symbol font nor a recognized SMuFL font, yet carrying the raw SMuFL codepoint directly.
    const auto fontContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);

    const auto left = classifyArticulationSymbol(fontContext.fontInfo, 0xE0F5);
    const auto* leftParen = left.as<articulation::Parenthesis>();
    ASSERT_NE(leftParen, nullptr);
    EXPECT_EQ(leftParen->side, articulation::Parenthesis::Side::Left);
    ASSERT_TRUE(left.glyphName);
    EXPECT_EQ(left.glyphName.value(), "noteheadParenthesisLeft");

    const auto right = classifyArticulationSymbol(fontContext.fontInfo, 0xE0F6);
    const auto* rightParen = right.as<articulation::Parenthesis>();
    ASSERT_NE(rightParen, nullptr);
    EXPECT_EQ(rightParen->side, articulation::Parenthesis::Side::Right);
    ASSERT_TRUE(right.glyphName);
    EXPECT_EQ(right.glyphName.value(), "noteheadParenthesisRight");
}

TEST(ArticulationClassification, ClassifiesVerticalEntryBracketShapes)
{
    std::vector<char> xml;
    readFile(std::filesystem::path(MUSX_TEST_DATA_PATH) / "nonArpeggios.enigmaxml", xml);
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);

    auto sourceEntry = musx::dom::EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 131);
    ASSERT_TRUE(sourceEntry);
    auto assign = document->getDetails()->get<details::ArticulationAssign>(SCORE_PARTID, 131, Inci{0});
    ASSERT_TRUE(assign);
    const auto classification = classifyArticulation(assign, sourceEntry);
    EXPECT_TRUE(classification.is<articulation::VerticalEntryBracket>());
    EXPECT_NE(classification.placement, musx::dom::VerticalPlacement::NotApplicable);
}
