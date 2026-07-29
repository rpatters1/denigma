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

#include <cstddef>
#include <vector>

#include "gtest/gtest.h"
#include "mx/api/ScoreData.h"
#include "musicxml_test.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

std::vector<mx::api::ClefData> clefsInMeasure(const mx::api::PartData& part, size_t measureIndex)
{
    if (measureIndex >= part.measures.size()) {
        return {};
    }
    std::vector<mx::api::ClefData> result;
    for (const auto& staff : part.measures[measureIndex].staves) {
        for (const auto& clef : staff.clefs) {
            result.emplace_back(clef);
        }
    }
    return result;
}

} // namespace

// clef_forced_after_barline.musx changes to bass clef in measure 5 with Finale's
// "Place Clef After Barline" option, then restates the same bass clef in measure 6
// with Finale's forced ("Always") clef display.
TEST(MusicXmlClefs, AfterBarlineAndForcedRestatement)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("clef_forced_after_barline.musx");
    ASSERT_TRUE(score);
    ASSERT_FALSE(score->parts.empty());
    const auto& part = score->parts.front();
    ASSERT_GE(part.measures.size(), 6u);

    constexpr size_t afterBarlineMeasureIndex = 4;
    const auto afterBarlineClefs = clefsInMeasure(part, afterBarlineMeasureIndex);
    ASSERT_EQ(afterBarlineClefs.size(), 1u);
    EXPECT_TRUE(afterBarlineClefs.front().isBass());
    EXPECT_EQ(afterBarlineClefs.front().location, mx::api::ClefLocation::afterBarline);
    EXPECT_EQ(afterBarlineClefs.front().additional, mx::api::Bool::unspecified);

    constexpr size_t forcedMeasureIndex = 5;
    const auto forcedClefs = clefsInMeasure(part, forcedMeasureIndex);
    ASSERT_EQ(forcedClefs.size(), 1u);
    EXPECT_TRUE(forcedClefs.front().isBass());
    EXPECT_EQ(forcedClefs.front().additional, mx::api::Bool::yes);
    // The restated clef is not itself placed after the barline.
    EXPECT_EQ(forcedClefs.front().location, mx::api::ClefLocation::unspecified);
    EXPECT_EQ(forcedClefs.front().printObject, mx::api::Bool::unspecified);
}

// Ordinary clef changes carry neither attribute.
TEST(MusicXmlClefs, PlainClefChangesOmitAfterBarlineAndAdditional)
{
    setupTestDataPaths();

    const auto score = createScoreDataFromMusicXmlFixture("clef_changes.musx");
    ASSERT_TRUE(score);
    ASSERT_FALSE(score->parts.empty());
    const auto& part = score->parts.front();

    size_t clefCount = 0;
    for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
        SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
        for (const auto& clef : clefsInMeasure(part, measureIndex)) {
            ++clefCount;
            EXPECT_EQ(clef.additional, mx::api::Bool::unspecified);
            EXPECT_NE(clef.location, mx::api::ClefLocation::afterBarline);
        }
    }
    EXPECT_GT(clefCount, 0u);
}
