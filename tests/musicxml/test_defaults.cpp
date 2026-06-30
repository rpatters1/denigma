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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <map>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mx/api/ScoreData.h"
#include "musicxml_test.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

struct LayoutFixture
{
    std::string musxFile;
    std::string referenceMusicXmlFile;
    // Finale appears to emit an incorrect defaults-level top-system-distance; compare against Denigma's value instead.
    long expectedTopSystemDistance;
};

const std::vector<LayoutFixture>& layoutFixtures()
{
    static const std::vector<LayoutFixture> fixtures{
        { "musicxml/page70-staff82ev.musx", "musicxml/page70-staff82ev-ref.musicxml", 211 },
        { "musicxml/page70nohold-staff82ev.musx", "musicxml/page70nohold-staff82ev-ref.musicxml", 211 }
    };
    return fixtures;
}

void compareMargins(const mx::api::MarginsData& actual, const mx::api::MarginsData& expected, const char* label)
{
    expectRoundedValue(actual.left, expected.left, (std::string(label) + ".left").c_str());
    expectRoundedValue(actual.right, expected.right, (std::string(label) + ".right").c_str());
    expectRoundedValue(actual.top, expected.top, (std::string(label) + ".top").c_str());
    expectRoundedValue(actual.bottom, expected.bottom, (std::string(label) + ".bottom").c_str());
}

void comparePageLayout(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected)
{
    ASSERT_EQ(actual.pageLayout.size.has_value(), expected.pageLayout.size.has_value()) << "Presence mismatch for page size";
    if (expected.pageLayout.size) {
        expectRoundedValue(actual.pageLayout.size->height, expected.pageLayout.size->height, "page-layout.size.height");
        expectRoundedValue(actual.pageLayout.size->width, expected.pageLayout.size->width, "page-layout.size.width");
    }

    ASSERT_EQ(actual.pageLayout.margins.odd.has_value(), expected.pageLayout.margins.odd.has_value())
        << "Presence mismatch for odd page margins";
    if (expected.pageLayout.margins.odd) {
        compareMargins(*actual.pageLayout.margins.odd, *expected.pageLayout.margins.odd, "page-layout.margins.odd");
    }

    ASSERT_EQ(actual.pageLayout.margins.even.has_value(), expected.pageLayout.margins.even.has_value())
        << "Presence mismatch for even page margins";
    if (expected.pageLayout.margins.even) {
        compareMargins(*actual.pageLayout.margins.even, *expected.pageLayout.margins.even, "page-layout.margins.even");
    }
}

void compareSystemLayout(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected, long expectedTopSystemDistance)
{
    ASSERT_EQ(actual.systemLayout.margins.has_value(), expected.systemLayout.margins.has_value())
        << "Presence mismatch for system margins";
    if (expected.systemLayout.margins) {
        expectRoundedValue(actual.systemLayout.margins->left, expected.systemLayout.margins->left, "system-layout.margins.left");
        expectRoundedValue(actual.systemLayout.margins->right, expected.systemLayout.margins->right, "system-layout.margins.right");
    }

    expectOptionalValue(actual.systemLayout.systemDistance, expected.systemLayout.systemDistance, "system-layout.system-distance");
    ASSERT_TRUE(actual.systemLayout.topSystemDistance) << "Missing system-layout.top-system-distance";
    expectRoundedValue(*actual.systemLayout.topSystemDistance, expectedTopSystemDistance, "system-layout.top-system-distance");
}

using AppearanceKey = std::pair<mx::api::AppearanceType, std::string>;
using AppearanceMap = std::map<AppearanceKey, double>;
constexpr double APPEARANCE_VALUE_TOLERANCE = 0.00005;
constexpr double FONT_SIZE_ONE_DECIMAL_TOLERANCE = 0.05;

AppearanceMap createAppearanceMap(const std::vector<mx::api::AppearanceData>& appearance)
{
    AppearanceMap result;
    for (const auto& data : appearance) {
        const auto [it, inserted] = result.emplace(AppearanceKey{ data.appearanceType, data.appearanceSubType }, data.value);
        EXPECT_TRUE(inserted) << "Duplicate appearance entry: " << data.appearanceSubType;
    }
    return result;
}

void compareAppearance(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected)
{
    const auto actualAppearance = createAppearanceMap(actual.appearance);
    const auto expectedAppearance = createAppearanceMap(expected.appearance);
    EXPECT_GE(actualAppearance.size(), expectedAppearance.size()) << "Missing appearance entries";
    for (const auto& [key, expectedValue] : expectedAppearance) {
        const auto actualIt = actualAppearance.find(key);
        ASSERT_NE(actualIt, actualAppearance.end()) << "Missing appearance entry: " << key.second;
        EXPECT_NEAR(actualIt->second, expectedValue, APPEARANCE_VALUE_TOLERANCE) << "Mismatch for " << key.second;
    }
}

bool isGenericFontFamilyFallback(const std::string& fontFamily)
{
    return fontFamily == "music"
        || fontFamily == "engraved"
        || fontFamily == "handwritten"
        || fontFamily == "text"
        || fontFamily == "serif"
        || fontFamily == "sans-serif"
        || fontFamily == "cursive"
        || fontFamily == "fantasy"
        || fontFamily == "monospace";
}

mx::api::FontStyle explicitFontStyle(mx::api::FontStyle style)
{
    return style == mx::api::FontStyle::unspecified ? mx::api::FontStyle::normal : style;
}

mx::api::FontWeight explicitFontWeight(mx::api::FontWeight weight)
{
    return weight == mx::api::FontWeight::unspecified ? mx::api::FontWeight::normal : weight;
}

void compareFontData(const mx::api::FontData& actual, const mx::api::FontData& expected, const char* label)
{
    ASSERT_GE(actual.fontFamily.size(), expected.fontFamily.size()) << label << ".font-family";
    for (size_t i = 0; i < expected.fontFamily.size(); ++i) {
        EXPECT_EQ(actual.fontFamily[i], expected.fontFamily[i]) << label << ".font-family[" << i << "]";
    }
    for (size_t i = expected.fontFamily.size(); i < actual.fontFamily.size(); ++i) {
        EXPECT_TRUE(isGenericFontFamilyFallback(actual.fontFamily[i])) << label << ".font-family[" << i << "]";
    }
    EXPECT_EQ(actual.sizeType, expected.sizeType) << label << ".font-size-type";
    if (expected.sizeType == mx::api::FontSizeType::point) {
        EXPECT_NEAR(actual.sizePoint, expected.sizePoint, FONT_SIZE_ONE_DECIMAL_TOLERANCE) << label << ".font-size";
    } else {
        EXPECT_EQ(actual.sizePoint, expected.sizePoint) << label << ".font-size";
    }
    EXPECT_EQ(actual.sizeCss, expected.sizeCss) << label << ".font-size-css";
    EXPECT_EQ(explicitFontStyle(actual.style), explicitFontStyle(expected.style)) << label << ".font-style";
    EXPECT_EQ(explicitFontWeight(actual.weight), explicitFontWeight(expected.weight)) << label << ".font-weight";
    EXPECT_EQ(actual.underline, expected.underline) << label << ".underline";
    EXPECT_EQ(actual.overline, expected.overline) << label << ".overline";
    EXPECT_EQ(actual.lineThrough, expected.lineThrough) << label << ".line-through";
}

void compareOptionalFontData(
    const std::optional<mx::api::FontData>& actual,
    const std::optional<mx::api::FontData>& expected,
    const char* label)
{
    ASSERT_EQ(actual.has_value(), expected.has_value()) << "Presence mismatch for " << label;
    if (expected) {
        compareFontData(*actual, *expected, label);
    }
}

void compareDefaultFonts(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected)
{
    compareOptionalFontData(actual.musicFont, expected.musicFont, "music-font");
    compareOptionalFontData(actual.wordFont, expected.wordFont, "word-font");

    ASSERT_EQ(actual.lyricFonts.size(), expected.lyricFonts.size()) << "lyric-font count";
    for (size_t i = 0; i < expected.lyricFonts.size(); ++i) {
        const auto label = "lyric-font[" + std::to_string(i) + "]";
        EXPECT_EQ(actual.lyricFonts[i].number, expected.lyricFonts[i].number) << label << ".number";
        EXPECT_EQ(actual.lyricFonts[i].name, expected.lyricFonts[i].name) << label << ".name";
        compareFontData(actual.lyricFonts[i].font, expected.lyricFonts[i].font, label.c_str());
    }
}

} // namespace

TEST(MusicXmlDefaults, PageAndSystemLayoutMatchFinaleRoundedTenths)
{
    setupTestDataPaths();

    for (const auto& fixture : layoutFixtures()) {
        const auto outputPath = exportMusicXmlFixture(fixture.musxFile);
        const auto actualScore = loadScoreData(outputPath);
        const auto expectedScore = loadScoreData(getInputPath() / fixture.referenceMusicXmlFile);
        ASSERT_TRUE(actualScore);
        ASSERT_TRUE(expectedScore);

        SCOPED_TRACE(fixture.musxFile);
        comparePageLayout(actualScore->defaults, expectedScore->defaults);
        compareSystemLayout(actualScore->defaults, expectedScore->defaults, fixture.expectedTopSystemDistance);
    }
}

TEST(MusicXmlDefaults, AppearanceMatchesFinaleToFourDecimals)
{
    setupTestDataPaths();

    for (const auto& fixture : layoutFixtures()) {
        const auto outputPath = exportMusicXmlFixture(fixture.musxFile);
        const auto actualScore = loadScoreData(outputPath);
        const auto expectedScore = loadScoreData(getInputPath() / fixture.referenceMusicXmlFile);
        ASSERT_TRUE(actualScore);
        ASSERT_TRUE(expectedScore);

        SCOPED_TRACE(fixture.musxFile);
        compareAppearance(actualScore->defaults, expectedScore->defaults);
    }
}

TEST(MusicXmlDefaults, FontsMatchFinaleWithinOneDecimalPlace)
{
    setupTestDataPaths();

    for (const auto& fixture : layoutFixtures()) {
        const auto outputPath = exportMusicXmlFixture(fixture.musxFile);
        const auto actualScore = loadScoreData(outputPath);
        const auto expectedScore = loadScoreData(getInputPath() / fixture.referenceMusicXmlFile);
        ASSERT_TRUE(actualScore);
        ASSERT_TRUE(expectedScore);

        SCOPED_TRACE(fixture.musxFile);
        compareDefaultFonts(actualScore->defaults, expectedScore->defaults);
    }
}
