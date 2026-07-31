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
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "core/denigma.h"
#include "mnxdom.h"
#include "test_utils.h"

using namespace denigma;

namespace {

/// @brief The dynamics of one measure, as MNX should spell them.
struct ExpectedDynamic
{
    std::string_view type;
    std::optional<mnx::DynamicValue> value;
    std::string_view prefix;
    std::string_view suffix;
    std::vector<std::string> glyphs;
    std::optional<mnx::DynamicRelativeValue> relativeValue;
    std::optional<mnx::DynamicPrefix> accentPrefix;
    std::optional<mnx::DynamicSuffix> accentSuffix;
    std::optional<mnx::DynamicWedgeType> wedgeType;
};

void checkDynamic(const mnx::part::DynamicGroupBase& dynamic, const ExpectedDynamic& expected, const std::string& label)
{
    ASSERT_EQ(dynamic.type(), expected.type) << label;
    EXPECT_EQ(dynamic.value(), expected.value) << label;
    EXPECT_EQ(dynamic.prefix_or({}), expected.prefix) << label;
    EXPECT_EQ(dynamic.suffix_or({}), expected.suffix) << label;
    if (expected.glyphs.empty()) {
        EXPECT_FALSE(dynamic.glyphs().has_value()) << label;
    } else {
        ASSERT_TRUE(dynamic.glyphs().has_value()) << label;
        EXPECT_EQ(std::vector<std::string>(dynamic.glyphs().value().begin(), dynamic.glyphs().value().end()),
            expected.glyphs) << label;
    }
    if (expected.relativeValue) {
        EXPECT_EQ(dynamic.get<mnx::part::DynamicRelative>().relativeValue(), expected.relativeValue.value()) << label;
    }
    if (expected.accentPrefix) {
        EXPECT_EQ(dynamic.get<mnx::part::DynamicAccent>().accentPrefix(), expected.accentPrefix.value()) << label;
    }
    if (expected.accentSuffix) {
        EXPECT_EQ(dynamic.get<mnx::part::DynamicAccent>().accentSuffix(), expected.accentSuffix.value()) << label;
    }
    if (expected.wedgeType) {
        EXPECT_EQ(dynamic.get<mnx::part::DynamicGradual>().wedgeType(), expected.wedgeType.value()) << label;
    }
}

void checkMeasureDynamics(const mnx::part::Measure& measure, const std::vector<ExpectedDynamic>& expected, const std::string& label)
{
    ASSERT_TRUE(measure.dynamics().has_value()) << label << " has no dynamics";
    auto dynamics = measure.dynamics().value();
    ASSERT_EQ(dynamics.size(), expected.size()) << label;
    for (size_t index = 0; index < expected.size(); index++) {
        checkDynamic(dynamics[index], expected[index], label + " dynamic " + std::to_string(index));
    }
}

constexpr std::string_view ACCENT = mnx::part::DynamicAccent::ContentTypeValue;
constexpr std::string_view GRADUAL = mnx::part::DynamicGradual::ContentTypeValue;
constexpr std::string_view IMMEDIATE = mnx::part::DynamicImmediate::ContentTypeValue;
constexpr std::string_view RELATIVE = mnx::part::DynamicRelative::ContentTypeValue;

} // namespace

TEST(MnxDynamics, DynamicsAndHairpins)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("dynamics_hairpins.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "dynamics_hairpins.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 6);

    checkMeasureDynamics(measures[1], {
        { IMMEDIATE, mnx::DynamicValue::pppp, {}, {}, { "dynamicPPPP" }, {}, {}, {}, {} },
        { ACCENT, mnx::DynamicValue::ffff, {}, {}, { "dynamicSforzando", "dynamicFFFF", "dynamicZ" },
            {}, mnx::DynamicPrefix::s, mnx::DynamicSuffix::z, {} },
        { GRADUAL, {}, {}, {}, {}, {}, {}, {}, mnx::DynamicWedgeType::Increasing },
    }, "measure 2");

    checkMeasureDynamics(measures[2], {
        { IMMEDIATE, mnx::DynamicValue::mf, {}, {}, { "dynamicMF" }, {}, {}, {}, {} },
        { IMMEDIATE, mnx::DynamicValue::pp, {}, {}, { "dynamicPP" }, {}, {}, {}, {} },
        { GRADUAL, {}, {}, {}, {}, {}, {}, {}, mnx::DynamicWedgeType::Increasing },
        { GRADUAL, {}, {}, {}, {}, {}, {}, {}, mnx::DynamicWedgeType::Decreasing },
    }, "measure 3");

    // A dynamic whose glyph is surrounded by words is still a dynamic. The words belong in the
    // prefix or suffix, and "più"/"menos" additionally make the dynamic a relative one.
    checkMeasureDynamics(measures[3], {
        { RELATIVE, mnx::DynamicValue::f, "più", {}, { "dynamicForte" }, mnx::DynamicRelativeValue::Louder, {}, {}, {} },
        { IMMEDIATE, mnx::DynamicValue::p, "sub.", {}, { "dynamicPiano" }, {}, {}, {}, {} },
    }, "measure 4");

    checkMeasureDynamics(measures[4], {
        { IMMEDIATE, mnx::DynamicValue::ff, {}, "sempre", { "dynamicFF" }, {}, {}, {}, {} },
        { RELATIVE, mnx::DynamicValue::f, "menos", {}, { "dynamicForte" }, mnx::DynamicRelativeValue::Softer, {}, {}, {} },
    }, "measure 5");

    checkMeasureDynamics(measures[5], {
        { ACCENT, mnx::DynamicValue::ff, {}, {}, { "dynamicFF", "dynamicZ" },
            {}, mnx::DynamicPrefix::None, mnx::DynamicSuffix::z, {} },
        { IMMEDIATE, mnx::DynamicValue::ppp, {}, {}, { "dynamicPPP" }, {}, {}, {}, {} },
        { GRADUAL, {}, {}, {}, {}, {}, {}, {}, mnx::DynamicWedgeType::Decreasing },
    }, "measure 6");
}
