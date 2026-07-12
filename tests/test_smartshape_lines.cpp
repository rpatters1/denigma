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

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "denigma/classify/smartshapes.h"
#include "formats/enigmaxml/enigmaxml.h"

using namespace denigma::classify;
using namespace musx::dom;
namespace classifiedshape = denigma::classify::smartshape;

namespace {

// Assigned smart shapes drawn in smartshape_lines.musx. The visual ottava custom
// lines use three line styles: 53 renders "8va" (weak alta glyph), 54 renders a bare
// "15" glyph (direction unknown), and 55 renders explicit ASCII "8vb".
constexpr Cmper kSolidLineDownBoth = 1;
constexpr Cmper kDashLineDownBoth = 3;
constexpr Cmper kSolidLineDown = 4;
constexpr Cmper kDashLineDown = 5;
constexpr Cmper kSolidLineUpBoth = 6;
constexpr Cmper kDashLineUpBoth = 7;
constexpr Cmper kSolidLineUp = 8;
constexpr Cmper kDashLineUp = 9;
constexpr Cmper kSolidLine = 10;
constexpr Cmper kDashLine = 11;
constexpr Cmper kEntryAttachedGlissando = 13;
constexpr Cmper kHiddenQuindicesima = 14;       // twoOctaveUp, m14-m23 staff 1
constexpr Cmper kQuindicesimaSegment1 = 15;     // style 54, m14-m15 staff 1
constexpr Cmper kQuindicesimaSegment2 = 16;     // style 54, m16-m18 staff 1
constexpr Cmper kBassaInsideQuindicesima = 19;  // style 55, m19-m20 staff 1
constexpr Cmper kBuiltInTrillExtension = 20;
constexpr Cmper kBuiltInTrill = 21;
constexpr Cmper kBassaOtherStaff = 24;          // style 55, m21-m23 staff 3
constexpr Cmper kPairedAltaLine = 25;           // style 53, m26-m28 staff 1
constexpr Cmper kHiddenOttava = 26;             // octaveUp, m26-m28 staff 1
constexpr Cmper kCustomLineWithEndHook = 27;    // style 21: solid, horizontal, end hook +1536
constexpr Cmper kNeutralWiggleLine = 28;        // style 52: wiggleCircularConstant char line

DocumentPtr loadFixture()
{
    const auto inputPath = std::filesystem::path("inputs") / "smartshape_lines.musx";
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

SmartShapeClassification classifyByCmper(const DocumentPtr& document, Cmper shapeCmper)
{
    const auto shape = document->getOthers()->get<others::SmartShape>(SCORE_PARTID, shapeCmper);
    EXPECT_TRUE(shape) << "smart shape " << shapeCmper;
    if (!shape) {
        return {};
    }
    return classifySmartShape(shape);
}

} // namespace

TEST(SmartShapeLinesFixture, BuiltInLinesClassify)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    using LineStyle = others::SmartShapeCustomLine::LineStyle;
    using CapType = classifiedshape::LineCap::Type;
    struct Expected
    {
        Cmper shapeCmper;
        LineStyle lineStyle;
        int startHookDirection; // +1 up, -1 down, 0 none
        int endHookDirection;
    };
    constexpr Expected expectations[] = {
        { kSolidLineDownBoth, LineStyle::Solid, -1, -1 },
        { kDashLineDownBoth, LineStyle::Dashed, -1, -1 },
        { kSolidLineDown, LineStyle::Solid, 0, -1 },
        { kDashLineDown, LineStyle::Dashed, 0, -1 },
        { kSolidLineUpBoth, LineStyle::Solid, 1, 1 },
        { kDashLineUpBoth, LineStyle::Dashed, 1, 1 },
        { kSolidLineUp, LineStyle::Solid, 0, 1 },
        { kDashLineUp, LineStyle::Dashed, 0, 1 },
        { kSolidLine, LineStyle::Solid, 0, 0 },
        { kDashLine, LineStyle::Dashed, 0, 0 },
    };

    // The fixture's smart shape options: hookLength 20 Evpu, smartLineWidth 118 Efix,
    // smartDashOn/smartDashOff 18 Evpu.
    constexpr Efix expectedHookLength = 20 * 64;

    for (const auto& expected : expectations) {
        const auto classification = classifyByCmper(document, expected.shapeCmper);
        const auto* line = classification.as<classifiedshape::GeneralLine>();
        ASSERT_NE(line, nullptr) << "smart shape " << expected.shapeCmper;
        EXPECT_EQ(line->lineStyle, expected.lineStyle) << "smart shape " << expected.shapeCmper;
        EXPECT_TRUE(line->lineVisible) << "smart shape " << expected.shapeCmper;
        EXPECT_EQ(line->lineWidth, 118) << "smart shape " << expected.shapeCmper;
        if (expected.lineStyle == LineStyle::Dashed) {
            EXPECT_EQ(line->dashOn, 18 * 64) << "smart shape " << expected.shapeCmper;
            EXPECT_EQ(line->dashOff, 18 * 64) << "smart shape " << expected.shapeCmper;
        }
        const auto checkCap = [&](const classifiedshape::LineCap& cap, int hookDirection, const char* which) {
            if (hookDirection == 0) {
                EXPECT_EQ(cap.type, CapType::None) << which << " cap of smart shape " << expected.shapeCmper;
            } else {
                EXPECT_EQ(cap.type, CapType::Hook) << which << " cap of smart shape " << expected.shapeCmper;
                EXPECT_EQ(cap.hookLength, hookDirection * expectedHookLength)
                    << which << " cap of smart shape " << expected.shapeCmper;
            }
        };
        checkCap(line->startCap, expected.startHookDirection, "start");
        checkCap(line->endCap, expected.endHookDirection, "end");
        EXPECT_FALSE(line->customLine) << "smart shape " << expected.shapeCmper;
    }
}

TEST(SmartShapeLinesFixture, CustomLineWithEndHookClassifies)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    const auto classification = classifyByCmper(document, kCustomLineWithEndHook);
    const auto* line = classification.as<classifiedshape::GeneralLine>();
    ASSERT_NE(line, nullptr);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Solid);
    EXPECT_TRUE(line->lineVisible);
    EXPECT_EQ(line->lineWidth, 141);
    EXPECT_TRUE(line->horizontal);
    EXPECT_EQ(line->startCap.type, classifiedshape::LineCap::Type::None);
    EXPECT_EQ(line->endCap.type, classifiedshape::LineCap::Type::Hook);
    // Positive hook length extends up (musx sign convention).
    EXPECT_EQ(line->endCap.hookLength, 1536);
    EXPECT_TRUE(line->customLine);
}

TEST(SmartShapeLinesFixture, NeutralWiggleCharLineStaysDescriptive)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    const auto classification = classifyByCmper(document, kNeutralWiggleLine);
    EXPECT_EQ(classification.as<classifiedshape::TrillLine>(), nullptr);
    EXPECT_EQ(classification.as<classifiedshape::VibratoLine>(), nullptr);
    const auto* line = classification.as<classifiedshape::GeneralLine>();
    ASSERT_NE(line, nullptr);
    EXPECT_EQ(line->lineStyle, others::SmartShapeCustomLine::LineStyle::Char);
    EXPECT_TRUE(line->lineVisible);
    ASSERT_TRUE(line->lineCharGlyphName);
    EXPECT_EQ(*line->lineCharGlyphName, "wiggleCircularConstant");
}

TEST(SmartShapeLinesFixture, EntryAttachedGlissandoIsNotClassified)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);
    const auto classification = classifyByCmper(document, kEntryAttachedGlissando);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(classification.value));
}

TEST(SmartShapeLinesFixture, BuiltInTrillShapesClassify)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    const auto trillClassification = classifyByCmper(document, kBuiltInTrill);
    const auto* trill = trillClassification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trill, nullptr);
    EXPECT_TRUE(trill->includesTrSymbol);
    EXPECT_FALSE(trill->line);

    const auto extensionClassification = classifyByCmper(document, kBuiltInTrillExtension);
    const auto* trillExtension = extensionClassification.as<classifiedshape::TrillLine>();
    ASSERT_NE(trillExtension, nullptr);
    EXPECT_FALSE(trillExtension->includesTrSymbol);
}

TEST(SmartShapeLinesFixture, CanonicalOttavaPair)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    const auto visualClassification = classifyByCmper(document, kPairedAltaLine);
    const auto* visual = visualClassification.as<classifiedshape::Ottava>();
    ASSERT_NE(visual, nullptr);
    EXPECT_EQ(visual->octaveShift, 1);
    EXPECT_FALSE(visual->calcIsSemanticCarrier());
    ASSERT_TRUE(visual->hiddenCounterpart);
    EXPECT_EQ(visual->hiddenCounterpart->getCmper(), kHiddenOttava);
    ASSERT_TRUE(visual->line);
    EXPECT_EQ(visual->line->lineStyle, others::SmartShapeCustomLine::LineStyle::Dashed);

    const auto hiddenClassification = classifyByCmper(document, kHiddenOttava);
    const auto* hidden = hiddenClassification.as<classifiedshape::Ottava>();
    ASSERT_NE(hidden, nullptr);
    EXPECT_EQ(hidden->octaveShift, 1);
    EXPECT_TRUE(hidden->calcIsSemanticCarrier());
    EXPECT_TRUE(hidden->hasVisualProxy);
    EXPECT_FALSE(hidden->line);
}

TEST(SmartShapeLinesFixture, MultiSegmentLinesPairWithSingleHiddenOttava)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    // The visual segments render a bare "15" glyph, so their direction is resolved
    // entirely by pairing with the hidden quindicesima.
    for (const Cmper segmentCmper : { kQuindicesimaSegment1, kQuindicesimaSegment2 }) {
        const auto classification = classifyByCmper(document, segmentCmper);
        const auto* segment = classification.as<classifiedshape::Ottava>();
        ASSERT_NE(segment, nullptr) << "smart shape " << segmentCmper;
        EXPECT_EQ(segment->octaveShift, 2) << "smart shape " << segmentCmper;
        EXPECT_FALSE(segment->calcIsSemanticCarrier()) << "smart shape " << segmentCmper;
        ASSERT_TRUE(segment->hiddenCounterpart) << "smart shape " << segmentCmper;
        EXPECT_EQ(segment->hiddenCounterpart->getCmper(), kHiddenQuindicesima) << "smart shape " << segmentCmper;
    }

    const auto hiddenClassification = classifyByCmper(document, kHiddenQuindicesima);
    const auto* hidden = hiddenClassification.as<classifiedshape::Ottava>();
    ASSERT_NE(hidden, nullptr);
    EXPECT_EQ(hidden->octaveShift, 2);
    EXPECT_TRUE(hidden->calcIsSemanticCarrier());
    EXPECT_TRUE(hidden->hasVisualProxy);
}

TEST(SmartShapeLinesFixture, MagnitudeMismatchStaysUnpairedCarrier)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    // An explicit "8vb" line inside the hidden quindicesima's range: the magnitudes
    // differ, so it must not pair and remains a semantic carrier.
    const auto classification = classifyByCmper(document, kBassaInsideQuindicesima);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
    EXPECT_FALSE(ottava->hiddenCounterpart);
}

TEST(SmartShapeLinesFixture, DifferentStaffStaysUnpairedCarrier)
{
    const auto document = loadFixture();
    ASSERT_TRUE(document);

    const auto classification = classifyByCmper(document, kBassaOtherStaff);
    const auto* ottava = classification.as<classifiedshape::Ottava>();
    ASSERT_NE(ottava, nullptr);
    EXPECT_EQ(ottava->octaveShift, -1);
    EXPECT_TRUE(ottava->calcIsSemanticCarrier());
    EXPECT_FALSE(ottava->hiddenCounterpart);
}
