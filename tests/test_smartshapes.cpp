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

#include <filesystem>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "denigma/classify/smartshapes.h"
#include "formats/enigmaxml/enigmaxml.h"

using namespace denigma::classify;
using namespace musx::dom;
namespace classifiedshape = denigma::classify::smartshape;

namespace {

struct SmartShapeContext
{
    DocumentPtr document;
    MusxInstance<others::SmartShape> shape;
};

SmartShapeContext makeCustomLine(
    std::string_view startText,
    std::string_view continuationText,
    std::string_view endText,
    std::string_view lineXml)
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
    <measSpec cmper="1"/>
    <ssLineStyle cmper="1">
)xml";
    xml += lineXml;
    if (!startText.empty()) {
        xml += "      <leftStartRawTextID>1</leftStartRawTextID>\n";
    }
    if (!continuationText.empty()) {
        xml += "      <leftContRawTextID>2</leftContRawTextID>\n";
    }
    if (!endText.empty()) {
        xml += "      <rightEndRawTextID>3</rightEndRawTextID>\n";
    }
    xml += R"xml(    </ssLineStyle>
    <smartShape cmper="1">
      <shapeType>smartLine</shapeType>
      <lineStyleID>1</lineStyleID>
      <startTermSeg><endPt><inst>1</inst><meas>1</meas></endPt></startTermSeg>
      <endTermSeg><endPt><inst>1</inst><meas>1</meas><edu>1024</edu></endPt></endTermSeg>
    </smartShape>
  </others>
  <texts>
)xml";
    if (!startText.empty()) {
        xml += "    <smartShapeText number=\"1\">" + std::string(startText) + "</smartShapeText>\n";
    }
    if (!continuationText.empty()) {
        xml += "    <smartShapeText number=\"2\">" + std::string(continuationText) + "</smartShapeText>\n";
    }
    if (!endText.empty()) {
        xml += "    <smartShapeText number=\"3\">" + std::string(endText) + "</smartShapeText>\n";
    }
    xml += "  </texts>\n</finale>\n";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 1) };
}

} // namespace

TEST(SmartShapeClassification, ClassifiesPedalTextAtEachCustomLinePosition)
{
    const auto context = makeCustomLine(
        "^fontid(0)^size(24)^nfx(0)&#xE650;",
        "^fontid(0)^size(24)^nfx(0)&#xE656;",
        "^fontid(0)^size(24)^nfx(0)&#xE655;",
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>32</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n");
    ASSERT_TRUE(context.shape);

    const auto classification = classifySmartShape(context.shape);
    const auto* pedal = classification.as<classifiedshape::KeyboardPedal>();
    ASSERT_NE(pedal, nullptr);
    ASSERT_TRUE(pedal->startText);
    EXPECT_EQ(pedal->startText->type, keyboardpedal::Type::PedalOne);
    ASSERT_TRUE(pedal->continuationText);
    EXPECT_EQ(pedal->continuationText->type, keyboardpedal::Type::HalfPedal);
    ASSERT_TRUE(pedal->endText);
    EXPECT_EQ(pedal->endText->type, keyboardpedal::Type::PedalUp);
    EXPECT_FALSE(pedal->lineVisible);
}

TEST(SmartShapeClassification, ClassifiesAsciiPedalTextWithOrdinaryHooks)
{
    const auto context = makeCustomLine(
        "Sost. Ped.", {}, {},
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapStartType>hook</lineCapStartType>\n"
        "      <lineCapEndType>hook</lineCapEndType>\n");

    const auto classification = classifySmartShape(context.shape);
    const auto* pedal = classification.as<classifiedshape::KeyboardPedal>();
    ASSERT_NE(pedal, nullptr);
    ASSERT_TRUE(pedal->startText);
    EXPECT_EQ(pedal->startText->type, keyboardpedal::Type::PedalTwo);
    EXPECT_EQ(pedal->startCap.type, classifiedshape::KeyboardPedal::Cap::Type::Hook);
    EXPECT_EQ(pedal->endCap.type, classifiedshape::KeyboardPedal::Cap::Type::Hook);
    EXPECT_TRUE(pedal->lineVisible);
}

TEST(SmartShapeClassification, DoesNotClassifyOrdinaryHooksWithoutPedalEvidence)
{
    const auto context = makeCustomLine(
        {}, {}, {},
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapStartType>hook</lineCapStartType>\n"
        "      <lineCapEndType>hook</lineCapEndType>\n");

    const auto classification = classifySmartShape(context.shape);
    EXPECT_EQ(classification.as<classifiedshape::KeyboardPedal>(), nullptr);
}

TEST(SmartShapeClassification, ClassifiesPedalReleaseAtRightEnd)
{
    const auto context = makeCustomLine(
        {}, {}, "*",
        "      <lineStyle>dashed</lineStyle>\n"
        "      <dashedParams><lineWidth>141</lineWidth><dashOn>1152</dashOn><dashOff>1152</dashOff></dashedParams>\n");

    const auto classification = classifySmartShape(context.shape);
    const auto* pedal = classification.as<classifiedshape::KeyboardPedal>();
    ASSERT_NE(pedal, nullptr);
    ASSERT_TRUE(pedal->endText);
    EXPECT_EQ(pedal->endText->type, keyboardpedal::Type::PedalUp);
    EXPECT_EQ(pedal->lineStyle, others::SmartShapeCustomLine::LineStyle::Dashed);
}

TEST(SmartShapeClassification, ClassifiesPedalPumpCustomCaps)
{
    const auto inputPath = std::filesystem::path("inputs") / "pedal_custom_lines.musx";
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    auto document = denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
    ASSERT_TRUE(document);

    const auto startPumpLine = document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 17);
    const auto startPump = classifyKeyboardPedalCustomLine(startPumpLine);
    ASSERT_TRUE(startPump);
    EXPECT_EQ(startPump->startCap.type, classifiedshape::KeyboardPedal::Cap::Type::PedalChange);
    EXPECT_EQ(startPump->startCap.customShapeType, KnownShapeDefType::PedalArrowheadLongUpDownShortUp);

    const auto classifyStartArrow = [&](Cmper shapeId) {
        auto mutableLine = std::make_shared<others::SmartShapeCustomLine>(
            document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper{ 100 });
        mutableLine->lineCapStartType = others::SmartShapeCustomLine::LineCapType::ArrowheadCustom;
        mutableLine->lineCapStartArrowId = shapeId;
        return classifyKeyboardPedalCustomLine(mutableLine);
    };
    const auto pedalDown = classifyStartArrow(91);
    ASSERT_TRUE(pedalDown);
    EXPECT_EQ(pedalDown->startCap.type, classifiedshape::KeyboardPedal::Cap::Type::PedalDown);
    EXPECT_EQ(pedalDown->startCap.customShapeType, KnownShapeDefType::PedalArrowheadDown);

    const auto pedalUp = classifyStartArrow(92);
    ASSERT_TRUE(pedalUp);
    EXPECT_EQ(pedalUp->startCap.type, classifiedshape::KeyboardPedal::Cap::Type::PedalUp);
    EXPECT_EQ(pedalUp->startCap.customShapeType, KnownShapeDefType::PedalArrowheadUp);

    const auto endPumpLine = document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 19);
    const auto endPump = classifyKeyboardPedalCustomLine(endPumpLine);
    ASSERT_TRUE(endPump);
    EXPECT_EQ(endPump->endCap.type, classifiedshape::KeyboardPedal::Cap::Type::PedalChange);
    EXPECT_EQ(endPump->endCap.customShapeType, KnownShapeDefType::PedalArrowheadShortUpDownLongUp);
}
