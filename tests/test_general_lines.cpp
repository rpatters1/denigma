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

struct GeneralLineContext
{
    DocumentPtr document;
    MusxInstance<others::SmartShape> shape;
};

constexpr std::string_view kSmartShapeOptionsXml = R"xml(
  <options>
    <smartShapeOptions>
      <hookLength>32</hookLength>
      <smartLineWidth>256</smartLineWidth>
      <smartDashOn>18</smartDashOn>
      <smartDashOff>20</smartDashOff>
    </smartShapeOptions>
  </options>
)xml";

GeneralLineContext makeBuiltInLine(
    std::string_view shapeType,
    bool withOptions = true,
    std::string_view extraShapeXml = {})
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
)xml";
    if (withOptions) {
        xml += kSmartShapeOptionsXml;
    }
    xml += R"xml(  <others>
    <measSpec cmper="1"/>
    <smartShape cmper="1">
      <shapeType>)xml";
    xml += shapeType;
    xml += R"xml(</shapeType>
)xml";
    xml += extraShapeXml;
    xml += R"xml(      <startTermSeg><endPt><inst>1</inst><meas>1</meas></endPt></startTermSeg>
      <endTermSeg><endPt><inst>1</inst><meas>1</meas><edu>1024</edu></endPt></endTermSeg>
    </smartShape>
  </others>
</finale>
)xml";
    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 1) };
}

GeneralLineContext makeCustomLine(
    std::string_view lineXml,
    std::string_view startText = {},
    std::string_view continuationText = {},
    std::string_view endText = {},
    std::string_view centerFullText = {},
    std::string_view centerAbbrText = {})
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
    if (!centerFullText.empty()) {
        xml += "      <centerFullRawTextID>4</centerFullRawTextID>\n";
    }
    if (!centerAbbrText.empty()) {
        xml += "      <centerAbbrRawTextID>5</centerAbbrRawTextID>\n";
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
    const auto appendText = [&](int number, std::string_view text) {
        if (!text.empty()) {
            xml += "    <smartShapeText number=\"" + std::to_string(number) + "\">" + std::string(text)
                + "</smartShapeText>\n";
        }
    };
    appendText(1, startText);
    appendText(2, continuationText);
    appendText(3, endText);
    appendText(4, centerFullText);
    appendText(5, centerAbbrText);
    xml += "  </texts>\n</finale>\n";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::SmartShape>(SCORE_PARTID, 1) };
}

std::optional<classifiedshape::GeneralLine> classifyAsGeneralLine(const GeneralLineContext& context)
{
    EXPECT_TRUE(context.shape);
    if (!context.shape) {
        return std::nullopt;
    }
    const auto classification = classifySmartShape(context.shape);
    if (const auto* line = classification.as<classifiedshape::GeneralLine>()) {
        return *line;
    }
    return std::nullopt;
}

} // namespace

TEST(GeneralLineClassification, BuiltInSolidLineUpHooksRightEndUp)
{
    const auto context = makeBuiltInLine("solidLineUp");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Solid);
    EXPECT_TRUE(line->lineVisible);
    EXPECT_EQ(line->lineWidth, 256);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::None);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(line->endCap.hookLength, 32 * 64);
    EXPECT_FALSE(line->customLine);
    EXPECT_FALSE(line->startText);
}

TEST(GeneralLineClassification, BuiltInDashLineDownBothHooksBothEndsDown)
{
    const auto context = makeBuiltInLine("dashLineDown2");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Dashed);
    EXPECT_EQ(line->dashOn, 18 * 64);
    EXPECT_EQ(line->dashOff, 20 * 64);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(line->startCap.hookLength, -32 * 64);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(line->endCap.hookLength, -32 * 64);
}

TEST(GeneralLineClassification, BuiltInMixedHookDirections)
{
    const auto context = makeBuiltInLine("solidLineUpDown");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_GT(line->startCap.hookLength, 0);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_LT(line->endCap.hookLength, 0);
}

TEST(GeneralLineClassification, CustomLineHooksPreserveSignAndTexts)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapStartType>hook</lineCapStartType>\n"
        "      <lineCapEndType>hook</lineCapEndType>\n"
        "      <lineCapStartHookLength>1536</lineCapStartHookLength>\n"
        "      <lineCapEndHookLength>-1536</lineCapEndHookLength>\n"
        "      <makeHorz/>\n",
        "^fontid(0)^size(12)^nfx(0)espr.");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Solid);
    EXPECT_TRUE(line->lineVisible);
    EXPECT_EQ(line->lineWidth, 141);
    EXPECT_TRUE(line->horizontal);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(line->startCap.hookLength, 1536);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(line->endCap.hookLength, -1536);
    EXPECT_TRUE(line->customLine);
    EXPECT_TRUE(line->startText);
    EXPECT_FALSE(line->continuationText);
    EXPECT_FALSE(line->endText);
    EXPECT_EQ(line->startText.getText(true), "espr.");
}

TEST(GeneralLineClassification, CustomLinePresetArrowhead)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapEndType>arrowheadPreset</lineCapEndType>\n"
        "      <lineCapEndArrowID>3</lineCapEndArrowID>\n");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::ArrowheadPreset);
    ASSERT_TRUE(line->endCap.preset);
    EXPECT_EQ(*line->endCap.preset, ArrowheadPreset::SmallCurved);
}

TEST(GeneralLineClassification, CustomLineInvalidPresetArrowheadIsUnresolved)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapEndType>arrowheadPreset</lineCapEndType>\n"
        "      <lineCapEndArrowID>99</lineCapEndArrowID>\n");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::ArrowheadPreset);
    EXPECT_FALSE(line->endCap.preset);
}

TEST(GeneralLineClassification, CustomLineMissingCustomArrowheadStaysDescriptive)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapStartType>arrowheadCustom</lineCapStartType>\n"
        "      <lineCapStartArrowID>77</lineCapStartArrowID>\n");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::ArrowheadCustom);
    EXPECT_FALSE(line->startCap.customArrowhead);
    EXPECT_EQ(line->startCap.customArrowheadType, KnownShapeDefType::Unrecognized);
}

TEST(GeneralLineClassification, CustomLineCharGlyphResolvesSmuflName)
{
    // wiggleWavyWide carries no specific line semantics and stays descriptive.
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>60086</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Char);
    EXPECT_TRUE(line->lineVisible);
    EXPECT_EQ(line->lineChar, char32_t(60086));
    ASSERT_TRUE(line->lineCharGlyphName);
    EXPECT_EQ(*line->lineCharGlyphName, "wiggleWavyWide");
}

TEST(GeneralLineClassification, CustomLineSpaceCharIsInvisible)
{
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>32</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n",
        {}, {}, {},
        "^fontid(0)^size(12)^nfx(2)Glissando",
        "^fontid(0)^size(12)^nfx(2)Gliss.");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_FALSE(line->lineVisible);
    EXPECT_TRUE(line->centerFullText);
    EXPECT_TRUE(line->centerAbbrText);
    EXPECT_EQ(line->centerFullText.getText(true), "Glissando");
    EXPECT_EQ(line->centerAbbrText.getText(true), "Gliss.");
}

TEST(GeneralLineClassification, ZeroWidthLineIsInvisible)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>0</lineWidth></solidParams>\n",
        "^fontid(0)^size(12)^nfx(0)txt");
    const auto line = classifyAsGeneralLine(context);
    ASSERT_TRUE(line);
    EXPECT_FALSE(line->lineVisible);
}

TEST(GeneralLineClassification, PedalEvidenceStillClassifiesAsKeyboardPedal)
{
    const auto context = makeCustomLine(
        "      <lineStyle>solid</lineStyle>\n"
        "      <solidParams><lineWidth>141</lineWidth></solidParams>\n"
        "      <lineCapStartType>hook</lineCapStartType>\n"
        "      <lineCapEndType>hook</lineCapEndType>\n",
        "^fontid(0)^size(12)^nfx(0)Ped.");
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    EXPECT_NE(classification.as<classifiedshape::KeyboardPedal>(), nullptr);
    EXPECT_EQ(classification.as<classifiedshape::GeneralLine>(), nullptr);
}

TEST(GeneralLineClassification, EntryAttachedLineIsNotClassified)
{
    const auto context = makeBuiltInLine("solidLineUp", true, "      <entryBased/>\n");
    ASSERT_TRUE(context.shape);
    EXPECT_TRUE(context.shape->entryBased);
    const auto classification = classifySmartShape(context.shape);
    EXPECT_EQ(classification.as<classifiedshape::GeneralLine>(), nullptr);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(classification.value));
}

TEST(GeneralLineClassification, MissingLineStyleYieldsMonostate)
{
    const auto context = makeBuiltInLine("smartLine", false);
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    EXPECT_EQ(classification.as<classifiedshape::GeneralLine>(), nullptr);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(classification.value));
}

TEST(TrillVibratoLineClassification, WiggleTrillBodyClassifiesAsTrillLine)
{
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>60068</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n");
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    const auto* trill = classification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trill, nullptr);
    EXPECT_FALSE(trill->includesTrSymbol);
    ASSERT_TRUE(trill->line);
    ASSERT_TRUE(trill->line->lineCharGlyphName);
    EXPECT_EQ(*trill->line->lineCharGlyphName, "wiggleTrill");
}

TEST(TrillVibratoLineClassification, TrSymbolStartTextClassifiesAsTrillLine)
{
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>32</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n",
        "^fontid(0)^size(24)^nfx(0)&#xE566;");
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    const auto* trill = classification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trill, nullptr);
    EXPECT_TRUE(trill->includesTrSymbol);
}

TEST(TrillVibratoLineClassification, WiggleBodyWithForeignTextStaysGeneralLine)
{
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>60068</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n",
        "^fontid(0)^size(12)^nfx(0)flutter");
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    EXPECT_EQ(classification.as<classifiedshape::TrillLine>(), nullptr);
    EXPECT_NE(classification.as<classifiedshape::GeneralLine>(), nullptr);
}

TEST(TrillVibratoLineClassification, GuitarVibratoBodyClassifiesAsVibratoLine)
{
    const auto context = makeCustomLine(
        "      <lineStyle>char</lineStyle>\n"
        "      <charParams><lineChar>60082</lineChar><fontID>0</fontID><fontSize>24</fontSize></charParams>\n");
    ASSERT_TRUE(context.shape);
    const auto classification = classifySmartShape(context.shape);
    const auto* vibrato = classification.as<classifiedshape::VibratoLine>();
    ASSERT_NE(vibrato, nullptr);
    ASSERT_TRUE(vibrato->line.lineCharGlyphName);
    EXPECT_EQ(*vibrato->line.lineCharGlyphName, "guitarVibratoStroke");
}

TEST(TrillVibratoLineClassification, BuiltInTrillShapesClassify)
{
    const auto trillContext = makeBuiltInLine("trill");
    ASSERT_TRUE(trillContext.shape);
    const auto trillClassification = classifySmartShape(trillContext.shape);
    const auto* trill = trillClassification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trill, nullptr);
    EXPECT_TRUE(trill->includesTrSymbol);
    EXPECT_FALSE(trill->line);

    const auto extContext = makeBuiltInLine("trillExt");
    ASSERT_TRUE(extContext.shape);
    const auto extClassification = classifySmartShape(extContext.shape);
    const auto* trillExt = extClassification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trillExt, nullptr);
    EXPECT_FALSE(trillExt->includesTrSymbol);
}

TEST(GeneralLineClassification, ClassifiesAllFixtureLineStyles)
{
    const auto inputPath = std::filesystem::path("inputs") / "pedal_custom_lines.musx";
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    auto document = denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
    ASSERT_TRUE(document);

    const auto lineStyles = document->getOthers()->getArray<others::SmartShapeCustomLine>(SCORE_PARTID);
    ASSERT_FALSE(lineStyles.empty());
    for (const auto& lineStyle : lineStyles) {
        const auto line = classifyGeneralLine(lineStyle);
        ASSERT_TRUE(line) << "line style " << lineStyle->getCmper();
    }

    // Style 3: solid line with a preset arrowhead at the end.
    const auto presetArrow = classifyGeneralLine(
        document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 3));
    ASSERT_TRUE(presetArrow);
    EXPECT_EQ(presetArrow->endCap.type, classifiedshape::LineCap::Type::ArrowheadPreset);
    ASSERT_TRUE(presetArrow->endCap.preset);
    EXPECT_EQ(*presetArrow->endCap.preset, ArrowheadPreset::SmallFilled);

    // Style 17: custom arrowhead recognized as a pedal pump shape.
    const auto customArrow = classifyGeneralLine(
        document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 17));
    ASSERT_TRUE(customArrow);
    EXPECT_EQ(customArrow->startCap.type, classifiedshape::LineCap::Type::ArrowheadCustom);
    EXPECT_TRUE(customArrow->startCap.customArrowhead);
    EXPECT_EQ(customArrow->startCap.customArrowheadType, KnownShapeDefType::PedalArrowheadLongUpDownShortUp);

    // Style 22: solid line with hooks at both ends.
    const auto bothHooks = classifyGeneralLine(
        document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 22));
    ASSERT_TRUE(bothHooks);
    EXPECT_EQ(bothHooks->startCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(bothHooks->startCap.hookLength, 1536);
    EXPECT_EQ(bothHooks->endCap.type, classifiedshape::LineCap::Type::Hook);
    EXPECT_EQ(bothHooks->endCap.hookLength, 1536);

    // Style 53: dashed custom line rendering an ottava with glyph texts.
    const auto ottavaLine = classifyGeneralLine(
        document->getOthers()->get<others::SmartShapeCustomLine>(SCORE_PARTID, 53));
    ASSERT_TRUE(ottavaLine);
    EXPECT_EQ(ottavaLine->lineStyle, others::SmartShapeCustomLine::LineStyle::Dashed);
    EXPECT_TRUE(ottavaLine->startText);
    EXPECT_TRUE(ottavaLine->continuationText);
    EXPECT_TRUE(ottavaLine->endText);
}
