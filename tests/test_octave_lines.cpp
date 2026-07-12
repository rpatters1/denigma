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

#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "denigma/classify/octaves.h"
#include "denigma/classify/smartshapes.h"

using namespace denigma::classify;
using namespace musx::dom;
namespace classifiedshape = denigma::classify::smartshape;

namespace {

struct OttavaScenario
{
    DocumentPtr document;
    MusxInstance<others::SmartShape> visualShape;
    MusxInstance<others::SmartShape> hiddenShape;
};

/// Builds a document with a dashed custom line in measure 1 and (optionally) a hidden
/// built-in ottava. The document has no scroll-view staves, so pairing exercises the
/// range-intersection fallback. When endpoint adjustments are given, they apply to the
/// visual line's endpoints (vertical Evpu relative to the top staff line).
OttavaScenario makeOttavaScenario(
    std::string_view startText,
    std::string_view continuationText = {},
    std::string_view hiddenShapeType = {},
    int hiddenStartMeas = 1,
    int hiddenEndMeas = 1,
    std::optional<int> startAdjY = std::nullopt,
    std::optional<int> endAdjY = std::nullopt)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Bravura</name>
    </fontName>
    <measSpec cmper="1"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <measSpec cmper="2"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <measSpec cmper="3"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <staffSpec cmper="1"><staffLines>5</staffLines><lineSpace>24</lineSpace></staffSpec>
    <ssLineStyle cmper="1">
      <lineStyle>dashed</lineStyle>
      <dashedParams><lineWidth>118</lineWidth><dashOn>1152</dashOn><dashOff>1152</dashOff></dashedParams>
      <leftStartRawTextID>1</leftStartRawTextID>
)xml";
    if (!continuationText.empty()) {
        xml += "      <leftContRawTextID>2</leftContRawTextID>\n";
    }
    const auto adjXml = [](std::optional<int> adjY) {
        return adjY ? "<endPtAdj><y>" + std::to_string(*adjY) + "</y><on/></endPtAdj>" : std::string();
    };
    xml += R"xml(    </ssLineStyle>
    <smartShape cmper="1">
      <shapeType>smartLine</shapeType>
      <lineStyleID>1</lineStyleID>
      <startTermSeg><endPt><inst>1</inst><meas>1</meas></endPt>)xml";
    xml += adjXml(startAdjY);
    xml += R"xml(</startTermSeg>
      <endTermSeg><endPt><inst>1</inst><meas>1</meas><edu>1024</edu></endPt>)xml";
    xml += adjXml(endAdjY);
    xml += R"xml(</endTermSeg>
    </smartShape>
)xml";
    if (!hiddenShapeType.empty()) {
        xml += "    <smartShape cmper=\"2\">\n      <shapeType>" + std::string(hiddenShapeType) + "</shapeType>\n"
            + "      <hidden/>\n"
            + "      <startTermSeg><endPt><inst>1</inst><meas>" + std::to_string(hiddenStartMeas)
            + "</meas></endPt></startTermSeg>\n"
            + "      <endTermSeg><endPt><inst>1</inst><meas>" + std::to_string(hiddenEndMeas)
            + "</meas><edu>2048</edu></endPt></endTermSeg>\n    </smartShape>\n";
    }
    xml += "  </others>\n  <texts>\n";
    xml += "    <smartShapeText number=\"1\">" + std::string(startText) + "</smartShapeText>\n";
    if (!continuationText.empty()) {
        xml += "    <smartShapeText number=\"2\">" + std::string(continuationText) + "</smartShapeText>\n";
    }
    xml += "  </texts>\n</finale>\n";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return {
        document,
        document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 1),
        document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 2),
    };
}

} // namespace

TEST(OctaveMarkingClassification, RecognizesAsciiMarkings)
{
    const auto alta = classifyOctaveMarking("8va");
    ASSERT_TRUE(alta);
    EXPECT_EQ(alta->magnitude, 1);
    EXPECT_EQ(alta->direction, octave::Direction::Up);
    EXPECT_FALSE(alta->directionIsExplicit);

    const auto bassa = classifyOctaveMarking("8vb");
    ASSERT_TRUE(bassa);
    EXPECT_EQ(bassa->magnitude, 1);
    EXPECT_EQ(bassa->direction, octave::Direction::Down);
    EXPECT_TRUE(bassa->directionIsExplicit);

    const auto altaBassa = classifyOctaveMarking("8va bassa");
    ASSERT_TRUE(altaBassa);
    EXPECT_EQ(altaBassa->magnitude, 1);
    EXPECT_EQ(altaBassa->direction, octave::Direction::Down);
    EXPECT_TRUE(altaBassa->directionIsExplicit);

    const auto quindicesima = classifyOctaveMarking("15ma");
    ASSERT_TRUE(quindicesima);
    EXPECT_EQ(quindicesima->magnitude, 2);
    EXPECT_EQ(quindicesima->direction, octave::Direction::Up);

    const auto parenthesized = classifyOctaveMarking("(8va)");
    ASSERT_TRUE(parenthesized);
    EXPECT_EQ(parenthesized->magnitude, 1);

    const auto ventiduesima = classifyOctaveMarking("22mb");
    ASSERT_TRUE(ventiduesima);
    EXPECT_EQ(ventiduesima->magnitude, 3);
    EXPECT_EQ(ventiduesima->direction, octave::Direction::Down);
}

TEST(OctaveMarkingClassification, RejectsWeakOrForeignText)
{
    EXPECT_FALSE(classifyOctaveMarking("8"));
    EXPECT_FALSE(classifyOctaveMarking("loco"));
    EXPECT_FALSE(classifyOctaveMarking("8va sempre"));
    EXPECT_FALSE(classifyOctaveMarking("espr."));
    EXPECT_FALSE(classifyOctaveMarking(""));
}

TEST(OctaveLineClassification, GlyphOttavaLineIsUnpairedCarrier)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(24)^nfx(0)&#xE511;",
        "^fontid(0)^size(24)^nfx(0)&#xE51A;&#xE511;&#xE51B;");
    ASSERT_TRUE(scenario.visualShape);
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, 1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
    EXPECT_FALSE(ottava->hiddenCounterpart);
    ASSERT_TRUE(ottava->line);
    EXPECT_EQ(ottava->line->lineStyle, others::SmartShapeCustomLine::LineStyle::Dashed);
}

TEST(OctaveLineClassification, AsciiBassaLineClassifies)
{
    const auto scenario = makeOttavaScenario("^fontid(0)^size(12)^nfx(0)8vb");
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
}

TEST(OctaveLineClassification, BareGlyphUnpairedResolvesFromAbovePlacement)
{
    // No endpoint adjustments means both endpoints sit at the top staff line, which
    // musxdom reports as Above placement.
    const auto scenario = makeOttavaScenario("^fontid(0)^size(24)^nfx(0)&#xE510;");
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, 1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
}

TEST(OctaveLineClassification, BareGlyphUnpairedResolvesFromBelowPlacement)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(24)^nfx(0)&#xE510;", {}, {}, 1, 1, -200, -200);
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
}

TEST(OctaveLineClassification, BareGlyphFloatingPlacementIsNotOttava)
{
    // One endpoint below the staff and one at the top line is Float placement,
    // which cannot resolve the direction.
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(24)^nfx(0)&#xE510;", {}, {}, 1, 1, -200, std::nullopt);
    const auto classification = classifySmartShape(scenario.visualShape);
    EXPECT_EQ(classification.as<classifiedshape::Ottava>(), nullptr);
    EXPECT_NE(classification.as<classifiedshape::GeneralLine>(), nullptr);
}

TEST(OctaveLineClassification, WeakAltaBelowStaffUnpairedClassifiesDown)
{
    // An "8va" line drawn below the staff means bassa when nothing contradicts it.
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(12)^nfx(0)8va", {}, {}, 1, 1, -200, -200);
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
}

TEST(OctaveLineClassification, ExplicitBassaBelowStaffStaysDown)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(12)^nfx(0)8vb", {}, {}, 1, 1, -200, -200);
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
}

TEST(OctaveLineClassification, BareGlyphResolvesDirectionFromHiddenCounterpart)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(24)^nfx(0)&#xE510;", {}, "octaveDown");
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_FALSE(ottava->calcIsSemanticCarrier());
    ASSERT_TRUE(ottava->hiddenCounterpart);
    EXPECT_EQ(ottava->hiddenCounterpart->getCmper(), scenario.hiddenShape->getCmper());
}

TEST(OctaveLineClassification, PairedAltaLineIsAppearanceOnly)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(24)^nfx(0)&#xE511;", {}, "octaveUp");
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, 1);
    EXPECT_FALSE(ottava->calcIsSemanticCarrier());

    const auto hiddenClassification = classifySmartShape(scenario.hiddenShape);
    const auto* hiddenOttava = hiddenClassification.as<classifiedshape::Ottava>();
    ASSERT_NE(hiddenOttava, nullptr);
    EXPECT_EQ(hiddenOttava->octaveShift, 1);
    EXPECT_TRUE(hiddenOttava->calcIsSemanticCarrier());
    EXPECT_TRUE(hiddenOttava->hasVisualProxy);
}

TEST(OctaveLineClassification, NonOverlappingHiddenOttavaDoesNotPair)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(12)^nfx(0)8va", {}, "octaveUp", 3, 3);
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, 1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());

    const auto hiddenClassification = classifySmartShape(scenario.hiddenShape);
    const auto* hiddenOttava = hiddenClassification.as<classifiedshape::Ottava>();
    ASSERT_NE(hiddenOttava, nullptr);
    EXPECT_FALSE(hiddenOttava->hasVisualProxy);
}

TEST(OctaveLineClassification, MagnitudeMismatchDoesNotPair)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(12)^nfx(0)15ma", {}, "octaveUp");
    const auto classification = classifySmartShape(scenario.visualShape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, 2);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
}

TEST(OctaveLineClassification, ConflictingContinuationTextIsNotOttava)
{
    const auto scenario = makeOttavaScenario(
        "^fontid(0)^size(12)^nfx(0)8va",
        "^fontid(0)^size(12)^nfx(0)15ma");
    const auto classification = classifySmartShape(scenario.visualShape);
    EXPECT_EQ(classification.as<classifiedshape::Ottava>(), nullptr);
    EXPECT_NE(classification.as<classifiedshape::GeneralLine>(), nullptr);
}

TEST(OctaveLineClassification, VisibleBuiltInOttavaIsUnchanged)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <measSpec cmper="1"/>
    <smartShape cmper="1">
      <shapeType>octaveDown</shapeType>
      <startTermSeg><endPt><inst>1</inst><meas>1</meas></endPt></startTermSeg>
      <endTermSeg><endPt><inst>1</inst><meas>1</meas><edu>1024</edu></endPt></endTermSeg>
    </smartShape>
  </others>
</finale>
)xml";
    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    const auto shape = document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 1);
    ASSERT_TRUE(shape);
    const auto classification = classifySmartShape(shape);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
    EXPECT_FALSE(ottava->hasVisualProxy);
    EXPECT_FALSE(ottava->line);
}
