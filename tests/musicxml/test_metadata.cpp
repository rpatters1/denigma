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

#include <algorithm>

#include "gtest/gtest.h"
#include "musicxml_test.h"

using namespace denigma::test::musicxml;

TEST(MusicXmlMetadata, exportsPageTextCredits)
{
    const auto score = createScoreDataFromMusicXmlFixture("page_text.musx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->pageTextItems.size(), 6u);

    const auto findCredit = [&](const std::string& creditType) {
        return std::find_if(score->pageTextItems.begin(), score->pageTextItems.end(), [&](const auto& credit) {
            return std::find(credit.creditTypes.begin(), credit.creditTypes.end(), creditType) != credit.creditTypes.end();
        });
    };

    const auto pageNumber = findCredit("page number");
    ASSERT_NE(pageNumber, score->pageTextItems.end());
    EXPECT_EQ(pageNumber->text, "1");
    EXPECT_EQ(pageNumber->pageNumber, 1);
    EXPECT_EQ(pageNumber->positionData.horizontalAlignment, mx::api::HorizontalAlignment::right);
    EXPECT_EQ(pageNumber->positionData.verticalAlignment, mx::api::VerticalAlignment::top);

    const auto title = findCredit("title");
    ASSERT_NE(title, score->pageTextItems.end());
    EXPECT_EQ(title->text, "\nfor");
    EXPECT_EQ(title->justify, mx::api::HorizontalAlignment::center);

    const auto composer = findCredit("composer");
    ASSERT_NE(composer, score->pageTextItems.end());
    EXPECT_EQ(composer->text, "R. G. PATTERSON (2012)");
    EXPECT_EQ(composer->creditTypes, (std::vector<std::string>{ "composer", "rights" }));
}

// Page text sits on the page, so only Finale's page scaling reduces its point size. The system and
// staff scaling that governs in-score text never applies to it, and a font carrying Finale's
// fixed-size effect is exempt from scaling altogether. page_percent.musx separates all three: its
// page scaling is 90% (taken from the page record, not the page format options, which say 100%),
// its system scaling is 82/96, and their product is neither. Finale's own export of this document
// agrees with both expected values; see musicxml/page_percent-ref.musicxml.
TEST(MusicXmlMetadata, pageTextFontSizeScalesToPageScalingOnly)
{
    const auto score = createScoreDataFromMusicXmlFixture("page_percent.musx");
    ASSERT_TRUE(score);
    ASSERT_EQ(score->pageTextItems.size(), 2u);

    const auto findCredit = [&](const std::string& text) {
        return std::find_if(score->pageTextItems.begin(), score->pageTextItems.end(), [&](const auto& credit) {
            return credit.text == text;
        });
    };

    constexpr double kPageScaling = 0.90;
    constexpr double kTolerance = 0.001;
    // Finale writes whole tenths, so the reference positions below carry its rounding.
    constexpr double kPositionTolerance = 0.5;

    // Nominally 18 point and scalable: 18 * 0.90. Scaling by the combined page and system factor
    // would give 13.84, and not scaling at all would leave 18.
    const auto scalable = findCredit("Unfixed Title");
    ASSERT_NE(scalable, score->pageTextItems.end());
    EXPECT_EQ(scalable->fontData.sizeType, mx::api::FontSizeType::point);
    EXPECT_NEAR(scalable->fontData.sizePoint, 18.0 * kPageScaling, kTolerance);

    // Nominally 12 point with Finale's fixed-size effect, so no scaling of any kind applies.
    const auto fixed = findCredit("Fixed Title");
    ASSERT_NE(fixed, score->pageTextItems.end());
    EXPECT_EQ(fixed->fontData.sizeType, mx::api::FontSizeType::point);
    EXPECT_NEAR(fixed->fontData.sizePoint, 12.0, kTolerance);

    // Both anchors match Finale's own export of this document, on both axes. The two assignments
    // carry offsets in opposite directions (xDisp -380 and +519, yDisp -23 and -229) so that a
    // wrong conversion cannot cancel out. The page box is in page tenths, which back out the
    // combined scaling, but an assignment's offset positions content on the page and so carries the
    // page scaling, leaving only the system scaling to back out. For the fixed credit, backing out
    // the combined scaling instead would give x 944.72 and y 1495.39, and backing out nothing would
    // give x 879.66 and y 1524.10.
    EXPECT_TRUE(scalable->positionData.isDefaultXSpecified);
    EXPECT_TRUE(scalable->positionData.isDefaultYSpecified);
    EXPECT_NEAR(scalable->positionData.defaultX, 478.0, kPositionTolerance);
    EXPECT_NEAR(scalable->positionData.defaultY, 1608.0, kPositionTolerance);

    EXPECT_TRUE(fixed->positionData.isDefaultXSpecified);
    EXPECT_TRUE(fixed->positionData.isDefaultYSpecified);
    EXPECT_NEAR(fixed->positionData.defaultX, 917.0, kPositionTolerance);
    EXPECT_NEAR(fixed->positionData.defaultY, 1508.0, kPositionTolerance);
}
