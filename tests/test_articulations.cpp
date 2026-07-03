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

} // namespace

TEST(ArticulationClassification, ClassifiesUnicodeArticulationMarks)
{
    const auto fontContext = makeFontContext("Times New Roman", 0);
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 0x1D17B);

    const auto* articulation = classification.as<ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, ArticulationMark::Type::Accent);
    EXPECT_FALSE(classification.glyphName);
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, musx::dom::VerticalPlacement::NotApplicable);
}

TEST(ArticulationClassification, ClassifiesLegacyGlyphArticulationMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 62);

    const auto* articulation = classification.as<ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, ArticulationMark::Type::Accent);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "articAccentAbove");
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, musx::dom::VerticalPlacement::Above);
}

TEST(ArticulationClassification, AssignsGlyphStyleToEachComboArticulationMark)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 223);

    const auto* articulation = classification.as<ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 2u);
    EXPECT_EQ(articulation->marks[0].type, ArticulationMark::Type::Accent);
    EXPECT_EQ(articulation->marks[0].glyphStyle.placement, musx::dom::VerticalPlacement::Below);
    EXPECT_EQ(articulation->marks[1].type, ArticulationMark::Type::Staccato);
    EXPECT_EQ(articulation->marks[1].glyphStyle.placement, musx::dom::VerticalPlacement::Below);
}

TEST(ArticulationClassification, ClassifiesFermatasBreathMarksAndArpeggios)
{
    const auto fontContext = makeFontContext();
    const auto fermataClassification = classifyArticulationSymbol(fontContext.fontInfo, 85);
    const auto* fermata = fermataClassification.as<Fermata>();
    ASSERT_NE(fermata, nullptr);
    EXPECT_EQ(fermata->shape, Fermata::Shape::Normal);
    EXPECT_EQ(fermata->duration, Fermata::Duration::Auto);

    const auto breathClassification = classifyArticulationSymbol(fontContext.fontInfo, 44);
    const auto* breath = breathClassification.as<BreathMark>();
    ASSERT_NE(breath, nullptr);
    EXPECT_EQ(breath->type, BreathMark::Type::Comma);

    const auto arpeggioClassification = classifyArticulationSymbol(fontContext.fontInfo, 103);
    const auto* arpeggio = arpeggioClassification.as<Arpeggio>();
    ASSERT_NE(arpeggio, nullptr);
    EXPECT_EQ(arpeggio->type, Arpeggio::Type::VerticalSegment);
}

TEST(ArticulationClassification, ClassifiesCaesuras)
{
    const auto fontContext = makeFontContext();
    const auto normalClassification = classifyArticulationSymbol(fontContext.fontInfo, 34);
    const auto* normal = normalClassification.as<Caesura>();
    ASSERT_NE(normal, nullptr);
    EXPECT_EQ(normal->type, Caesura::Type::Normal);

    const auto chantContext = makeFontContext("Finale Maestro");
    const auto chantClassification = classifyArticulationSymbol(chantContext.fontInfo, 0xE8F8);
    const auto* chant = chantClassification.as<Caesura>();
    ASSERT_NE(chant, nullptr);
    EXPECT_EQ(chant->type, Caesura::Type::Chant);
}

TEST(ArticulationClassification, ClassifiesOrnaments)
{
    const auto fontContext = makeFontContext();
    const auto trillClassification = classifyArticulationSymbol(fontContext.fontInfo, 217);
    const auto* trill = trillClassification.as<Ornament>();
    ASSERT_NE(trill, nullptr);
    EXPECT_EQ(trill->type, Ornament::Type::Trill);
    EXPECT_TRUE(trill->accidentals.empty());

    const auto mordentClassification = classifyArticulationSymbol(fontContext.fontInfo, 77);
    const auto* mordent = mordentClassification.as<Ornament>();
    ASSERT_NE(mordent, nullptr);
    EXPECT_EQ(mordent->type, Ornament::Type::Mordent);

    const auto invertedTurnContext = makeFontContext("Kousaku");
    const auto invertedTurnClassification = classifyArticulationSymbol(invertedTurnContext.fontInfo, 255);
    const auto* invertedTurn = invertedTurnClassification.as<Ornament>();
    ASSERT_NE(invertedTurn, nullptr);
    EXPECT_EQ(invertedTurn->type, Ornament::Type::InvertedTurn);

    const auto trillFlatContext = makeFontContext("Finale Maestro");
    const auto trillFlatClassification = classifyArticulationSymbol(trillFlatContext.fontInfo, 0xF5B2);
    const auto* trillFlat = trillFlatClassification.as<Ornament>();
    ASSERT_NE(trillFlat, nullptr);
    EXPECT_EQ(trillFlat->type, Ornament::Type::Trill);
    ASSERT_EQ(trillFlat->accidentals.size(), 1u);
    EXPECT_EQ(trillFlat->accidentals.front().accidental, Ornament::Accidental::Flat);
    EXPECT_EQ(trillFlat->accidentals.front().placement, musx::dom::VerticalPlacement::Above);

    const auto turnFlatSharpContext = makeFontContext("Finale Maestro");
    const auto turnFlatSharpClassification = classifyArticulationSymbol(turnFlatSharpContext.fontInfo, 0xF5B6);
    const auto* turnFlatSharp = turnFlatSharpClassification.as<Ornament>();
    ASSERT_NE(turnFlatSharp, nullptr);
    EXPECT_EQ(turnFlatSharp->type, Ornament::Type::Turn);
    ASSERT_EQ(turnFlatSharp->accidentals.size(), 2u);
    EXPECT_EQ(turnFlatSharp->accidentals[0].accidental, Ornament::Accidental::Flat);
    EXPECT_EQ(turnFlatSharp->accidentals[0].placement, musx::dom::VerticalPlacement::Above);
    EXPECT_EQ(turnFlatSharp->accidentals[1].accidental, Ornament::Accidental::Sharp);
    EXPECT_EQ(turnFlatSharp->accidentals[1].placement, musx::dom::VerticalPlacement::Below);
}

TEST(ArticulationClassification, ClassifiesTremoloMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 190);

    const auto* tremolo = classification.as<Tremolo>();
    ASSERT_NE(tremolo, nullptr);
    EXPECT_EQ(tremolo->style, Tremolo::Style::Measured);
    EXPECT_EQ(tremolo->marks, 3);
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
    const auto symbolContext = assign->calcSelectedSymbolContext(sourceEntry);
    ASSERT_TRUE(symbolContext);

    const auto classification = classifyArticulation(symbolContext.value());
    EXPECT_TRUE(classification.is<VerticalEntryBracket>());
}
