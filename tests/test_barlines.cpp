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

#include <optional>

#include "core/musx_reader.h"
#include "denigma/classify/barlines.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct BarlineContext
{
    DocumentPtr document;
    MusxInstance<others::Staff> staff;
    MusxInstance<others::Measure> measure;
    MusxInstance<options::BarlineOptions> options;
};

static BarlineContext makeBarlineContext(
    std::string_view measureFields,
    std::string_view nextMeasureFields = {},
    bool drawBarlines = true,
    bool drawFinalBarlineOnLastMeas = true,
    bool drawDoubleBarlineBeforeKeyChanges = false,
    bool hideStaffBarlines = false,
    int staffLines = 5,
    std::optional<Evpu> topBarlineOffset = std::nullopt,
    std::optional<Evpu> botBarlineOffset = std::nullopt)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <options>
    <barlineOptions>
)xml";
    if (drawBarlines) {
        xml += "      <drawBarlines/>\n";
    }
    if (drawFinalBarlineOnLastMeas) {
        xml += "      <drawFinalBarlineOnLastMeas/>\n";
    }
    if (drawDoubleBarlineBeforeKeyChanges) {
        xml += "      <drawDoubleBarlineBeforeKeyChanges/>\n";
    }
    xml += R"xml(    </barlineOptions>
  </options>
  <others>
    <staffSpec cmper="1">
)xml";
    xml += "      <staffLines>" + std::to_string(staffLines) + "</staffLines>\n";
    if (topBarlineOffset) {
        xml += "      <topBarlineOffset>" + std::to_string(*topBarlineOffset) + "</topBarlineOffset>\n";
    }
    if (botBarlineOffset) {
        xml += "      <botBarlineOffset>" + std::to_string(*botBarlineOffset) + "</botBarlineOffset>\n";
    }
    if (hideStaffBarlines) {
        xml += "      <hideBarlines/>\n";
    }
    xml += R"xml(    </staffSpec>
    <measSpec cmper="1">
)xml";
    xml += measureFields;
    xml += R"xml(
    </measSpec>
)xml";
    if (!nextMeasureFields.empty()) {
        xml += R"xml(    <measSpec cmper="2">
)xml";
        xml += nextMeasureFields;
        xml += R"xml(
    </measSpec>
)xml";
    }
    xml += R"xml(  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return {
        document,
        document->getOthers()->get<others::Staff>(SCORE_PARTID, 1),
        document->getOthers()->get<others::Measure>(SCORE_PARTID, 1),
        document->getOptions()->get<options::BarlineOptions>()
    };
}

} // namespace

TEST(BarlineClassification, ClassifiesSupportedMeasureTypes)
{
    const auto regular = makeBarlineContext("      <barline>normal</barline>");
    EXPECT_EQ(classifyBarline(regular.staff, regular.measure, false, regular.options).type, BarlineType::Regular);

    const auto doubleBar = makeBarlineContext("      <barline>double</barline>");
    EXPECT_EQ(classifyBarline(doubleBar.staff, doubleBar.measure, false, doubleBar.options).type, BarlineType::Double);

    const auto finalBar = makeBarlineContext("      <barline>final</barline>");
    EXPECT_EQ(classifyBarline(finalBar.staff, finalBar.measure, false, finalBar.options).type, BarlineType::Final);

    const auto heavy = makeBarlineContext("      <barline>solid</barline>");
    EXPECT_EQ(classifyBarline(heavy.staff, heavy.measure, false, heavy.options).type, BarlineType::Heavy);

    const auto dashed = makeBarlineContext("      <barline>dash</barline>");
    EXPECT_EQ(classifyBarline(dashed.staff, dashed.measure, false, dashed.options).type, BarlineType::Dashed);

    const auto tick = makeBarlineContext("      <barline>partial</barline>");
    EXPECT_EQ(classifyBarline(tick.staff, tick.measure, false, tick.options).type, BarlineType::Tick);
}

TEST(BarlineClassification, DefaultConstructsToUnsupported)
{
    const BarlineClassification classification;

    EXPECT_EQ(classification.type, BarlineType::Unsupported);
    EXPECT_FALSE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesHiddenBarlinesAsNoBarline)
{
    const auto optionsHidden = makeBarlineContext("      <barline>normal</barline>", {}, false);
    EXPECT_EQ(classifyBarline(optionsHidden.staff, optionsHidden.measure, false, optionsHidden.options).type, BarlineType::NoBarline);

    const auto staffHidden = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, true);
    EXPECT_EQ(classifyBarline(staffHidden.staff, staffHidden.measure, false, staffHidden.options).type, BarlineType::NoBarline);
}

TEST(BarlineClassification, UnsupportedTypesReturnUnsupported)
{
    const auto custom = makeBarlineContext("      <barline>custom</barline>");
    EXPECT_EQ(classifyBarline(custom.staff, custom.measure, false, custom.options).type, BarlineType::Unsupported);
}

TEST(BarlineClassification, ForcesRegularWhenFinalBarlineOptionIsDisabled)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, false);
    EXPECT_EQ(classifyBarline(context.staff, context.measure, true, context.options).type, BarlineType::Regular);
}

TEST(BarlineClassification, UsesDoubleBarlineBeforeKeyChanges)
{
    const auto context = makeBarlineContext(
        R"xml(      <barline>normal</barline>
      <keySig>
        <key>0</key>
      </keySig>)xml",
        R"xml(      <barline>normal</barline>
      <keySig>
        <key>1</key>
      </keySig>)xml",
        true,
        true,
        true);

    EXPECT_EQ(classifyBarline(context.staff, context.measure, false, context.options).type, BarlineType::Double);
}

TEST(BarlineClassification, ReportsShortFlag)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>");
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_FALSE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesSymmetricShortBarlineAtMinimumExtension)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -36, 36);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, BarlineType::Regular);
    EXPECT_TRUE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesSymmetricShortBarlineAtMaximumExtension)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -12, 12);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, BarlineType::Regular);
    EXPECT_TRUE(classification.isShort);
}

TEST(BarlineClassification, RejectsShortBarlineOutsideExtensionRange)
{
    const auto belowMinimum = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -37, 37);
    EXPECT_FALSE(classifyBarline(belowMinimum.staff, belowMinimum.measure, false, belowMinimum.options).isShort);

    const auto aboveMaximum = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -11, 11);
    EXPECT_FALSE(classifyBarline(aboveMaximum.staff, aboveMaximum.measure, false, aboveMaximum.options).isShort);
}

TEST(BarlineClassification, RejectsAsymmetricShortBarline)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -24, 12);

    EXPECT_FALSE(classifyBarline(context.staff, context.measure, false, context.options).isShort);
}

TEST(BarlineClassification, ClassifiesOneLineShortBarlineWithOutwardOffsets)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 1, 36, -36);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, BarlineType::Regular);
    EXPECT_TRUE(classification.isShort);
}
