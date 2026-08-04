/*
 * Copyright (C) 2025, Robert Patterson
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
#include <filesystem>
#include <iterator>
#include <fstream>
#include <set>

#include "gtest/gtest.h"
#include "core/denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(MnxScores, MultiInstrumentTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("multistaff_inst.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / "multistaff_inst.mnx", mnx);

    auto scores = mnx["scores"];
    ASSERT_TRUE(scores.is_array());
    EXPECT_EQ(scores.size(), 8);

    EXPECT_EQ(scores[0]["layout"], "S0-ScrVw");
    EXPECT_EQ(scores[0]["name"], "Score");
    ASSERT_TRUE(scores[0]["pages"].is_array());
    ASSERT_GT(scores[0]["pages"].size(), 3);
    ASSERT_TRUE(scores[0]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[0]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[0]["pages"][0]["systems"][0]["measure"], "m1");
    EXPECT_EQ(scores[0]["pages"][0]["systems"][0]["layout"], "S0-Sys1");
    ASSERT_TRUE(scores[0]["pages"][1]["systems"].is_array());
    ASSERT_EQ(scores[0]["pages"][1]["systems"].size(), 1);
    EXPECT_EQ(scores[0]["pages"][1]["systems"][0]["measure"], "m4");
    EXPECT_EQ(scores[0]["pages"][1]["systems"][0]["layout"], "S0-Sys2");
    ASSERT_TRUE(scores[0]["pages"][2]["systems"].is_array());
    ASSERT_EQ(scores[0]["pages"][2]["systems"].size(), 1);
    EXPECT_EQ(scores[0]["pages"][2]["systems"][0]["measure"], "m8");
    EXPECT_EQ(scores[0]["pages"][2]["systems"][0]["layout"], "S0-Sys3");

    EXPECT_EQ(scores[1]["layout"], "S1-ScrVw");
    EXPECT_EQ(scores[1]["name"], "Organ");
    ASSERT_TRUE(scores[1]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[1]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[1]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[1]["multimeasureRests"][0]["start"], "m1");
    ASSERT_TRUE(scores[1]["pages"].is_array());
    ASSERT_EQ(scores[1]["pages"].size(), 1);
    ASSERT_TRUE(scores[1]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[1]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[1]["pages"][0]["systems"][0]["measure"], "m1");
    EXPECT_EQ(scores[1]["pages"][0]["systems"][0]["layout"], "S1-Sys1");

    EXPECT_EQ(scores[2]["layout"], "S2-ScrVw");
    EXPECT_EQ(scores[2]["name"], "RH 1");
    ASSERT_TRUE(scores[2]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[2]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[2]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[2]["multimeasureRests"][0]["start"], "m1");
    ASSERT_TRUE(scores[2]["pages"].is_array());
    ASSERT_EQ(scores[2]["pages"].size(), 1);
    ASSERT_TRUE(scores[2]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[2]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[2]["pages"][0]["systems"][0]["measure"], "m1");
    EXPECT_EQ(scores[2]["pages"][0]["systems"][0]["layout"], "S2-Sys1");


    EXPECT_EQ(scores[3]["layout"], "S3-ScrVw");
    EXPECT_EQ(scores[3]["name"], "H1LH");
    ASSERT_TRUE(scores[3]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[3]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[3]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[3]["multimeasureRests"][0]["start"], "m1");
    ASSERT_TRUE(scores[3]["pages"].is_array());
    ASSERT_EQ(scores[3]["pages"].size(), 1);
    ASSERT_TRUE(scores[3]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[3]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[3]["pages"][0]["systems"][0]["measure"], "m1");
    EXPECT_EQ(scores[3]["pages"][0]["systems"][0]["layout"], "S3-Sys1");
}

// zwei_gesange.musx has three linked parts whose page layouts Finale never calculated, leaving
// zero-valued startMeas placeholders. Building per-system layouts from those placeholders used to
// look up measure 0 and throw std::logic_error, failing the whole export. The parts now get only
// their scroll-view layout and no pages, while the score, whose layout is calculated, is unaffected.
TEST(MnxScores, UncalculatedPartLayoutOmitsPagesAndSystemLayouts)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("zwei_gesange.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / "zwei_gesange.mnx", mnx);

    auto scores = mnx["scores"];
    ASSERT_TRUE(scores.is_array());
    ASSERT_EQ(scores.size(), 4);

    // The score resolves fully and keeps its pages and per-system layouts.
    EXPECT_EQ(scores[0]["name"], "Score");
    EXPECT_EQ(scores[0]["layout"], "S0-ScrVw");
    ASSERT_TRUE(scores[0]["pages"].is_array());
    EXPECT_EQ(scores[0]["pages"].size(), 5);

    // Every score must still reference a layout that exists, including the parts that lost theirs.
    std::set<std::string> layoutIds;
    ASSERT_TRUE(mnx["layouts"].is_array());
    for (const auto& layout : mnx["layouts"]) {
        layoutIds.insert(layout["id"].get<std::string>());
    }
    for (size_t scoreIndex = 0; scoreIndex < scores.size(); ++scoreIndex) {
        EXPECT_TRUE(layoutIds.count(scores[scoreIndex]["layout"].get<std::string>()) > 0)
            << "score " << scoreIndex << " references a missing layout";
    }

    for (size_t scoreIndex = 1; scoreIndex < scores.size(); ++scoreIndex) {
        SCOPED_TRACE("score " + std::to_string(scoreIndex));
        // No pages, and no per-system layout ids, but the scroll-view layout survives.
        EXPECT_FALSE(scores[scoreIndex].contains("pages") && !scores[scoreIndex]["pages"].empty());
        const auto layoutId = scores[scoreIndex]["layout"].get<std::string>();
        EXPECT_NE(layoutId.find("-ScrVw"), std::string::npos) << "expected a scroll-view layout, got " << layoutId;
    }
}
