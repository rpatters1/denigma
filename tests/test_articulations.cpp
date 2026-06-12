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
#include <vector>

#include "denigma/classify/articulations.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct ArticulationDefContext
{
    DocumentPtr document;
    MusxInstance<others::ArticulationDef> def;
};

static ArticulationDefContext makeArticulationDefContext(char32_t character, const std::string& fontName = "Maestro", int charsetVal = 4095)
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
    <articDef cmper="1">
      <charMain>)xml";
    xml += std::to_string(static_cast<uint32_t>(character));
    xml += R"xml(</charMain>
      <fontMain>1</fontMain>
      <sizeMain>24</sizeMain>
      <efxMain>0</efxMain>
    </articDef>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
    return { document, document->getOthers()->get<others::ArticulationDef>(SCORE_PARTID, 1) };
}

} // namespace

TEST(ArticulationClassification, ClassifiesUnicodeStandardArticulations)
{
    const auto context = makeArticulationDefContext(0x1D17B, "Times New Roman", 0);
    const auto classification = classifyArticulation(context.def);

    const auto* articulation = classification.as<StandardArticulation>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->types.size(), 1u);
    EXPECT_EQ(articulation->types.front(), StandardArticulation::Type::Accent);
    EXPECT_FALSE(classification.glyphName);
}

TEST(ArticulationClassification, ClassifiesLegacyGlyphArticulations)
{
    const auto context = makeArticulationDefContext(62);
    const auto classification = classifyArticulation(context.def);

    const auto* articulation = classification.as<StandardArticulation>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->types.size(), 1u);
    EXPECT_EQ(articulation->types.front(), StandardArticulation::Type::Accent);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "articAccentAbove");
}

TEST(ArticulationClassification, ClassifiesFermatasBreathMarksAndArpeggios)
{
    const auto fermataContext = makeArticulationDefContext(85);
    const auto fermataClassification = classifyArticulation(fermataContext.def);
    const auto* fermata = fermataClassification.as<Fermata>();
    ASSERT_NE(fermata, nullptr);
    EXPECT_EQ(fermata->shape, Fermata::Shape::Normal);
    EXPECT_EQ(fermata->duration, Fermata::Duration::Auto);

    const auto breathContext = makeArticulationDefContext(44);
    const auto breathClassification = classifyArticulation(breathContext.def);
    const auto* breath = breathClassification.as<BreathMark>();
    ASSERT_NE(breath, nullptr);
    EXPECT_EQ(breath->type, BreathMark::Type::Comma);

    const auto arpeggioContext = makeArticulationDefContext(103);
    const auto arpeggioClassification = classifyArticulation(arpeggioContext.def);
    const auto* arpeggio = arpeggioClassification.as<Arpeggio>();
    ASSERT_NE(arpeggio, nullptr);
    EXPECT_EQ(arpeggio->type, Arpeggio::Type::VerticalSegment);
}

TEST(ArticulationClassification, ClassifiesTremoloMarks)
{
    const auto context = makeArticulationDefContext(190);
    const auto classification = classifyArticulation(context.def);

    const auto* tremolo = classification.as<Tremolo>();
    ASSERT_NE(tremolo, nullptr);
    EXPECT_EQ(tremolo->style, Tremolo::Style::Measured);
    EXPECT_EQ(tremolo->marks, 3);
}

TEST(ArticulationClassification, ClassifiesVerticalEntryBracketShapes)
{
    std::vector<char> xml;
    readFile(std::filesystem::path(MUSX_TEST_DATA_PATH) / "nonArpeggios.enigmaxml", xml);
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(xml);
    ASSERT_TRUE(document);

    auto sourceEntry = musx::dom::EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 131);
    ASSERT_TRUE(sourceEntry);
    auto assign = document->getDetails()->get<details::ArticulationAssign>(SCORE_PARTID, 131, 0);
    ASSERT_TRUE(assign);
    const auto symbolContext = assign->calcSelectedSymbolContext(sourceEntry);
    ASSERT_TRUE(symbolContext);

    const auto classification = classifyArticulation(symbolContext.value());
    EXPECT_TRUE(classification.is<VerticalEntryBracket>());
}
