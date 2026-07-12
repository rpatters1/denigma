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
