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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(MnxScores, MultiInstrumentTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("multistaff_inst.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Invalid argument" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
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
    EXPECT_EQ(scores[0]["pages"][0]["systems"][0]["measure"], 1);
    EXPECT_EQ(scores[0]["pages"][0]["systems"][0]["layout"], "S0-Sys1");
    ASSERT_TRUE(scores[0]["pages"][1]["systems"].is_array());
    ASSERT_EQ(scores[0]["pages"][1]["systems"].size(), 1);
    EXPECT_EQ(scores[0]["pages"][1]["systems"][0]["measure"], 4);
    EXPECT_EQ(scores[0]["pages"][1]["systems"][0]["layout"], "S0-Sys2");
    ASSERT_TRUE(scores[0]["pages"][2]["systems"].is_array());
    ASSERT_EQ(scores[0]["pages"][2]["systems"].size(), 1);
    EXPECT_EQ(scores[0]["pages"][2]["systems"][0]["measure"], 8);
    EXPECT_EQ(scores[0]["pages"][2]["systems"][0]["layout"], "S0-Sys3");

    EXPECT_EQ(scores[1]["layout"], "S1-ScrVw");
    EXPECT_EQ(scores[1]["name"], "Organ");
    ASSERT_TRUE(scores[1]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[1]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[1]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[1]["multimeasureRests"][0]["start"], 1);
    ASSERT_TRUE(scores[1]["pages"].is_array());
    ASSERT_EQ(scores[1]["pages"].size(), 1);
    ASSERT_TRUE(scores[1]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[1]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[1]["pages"][0]["systems"][0]["measure"], 1);
    EXPECT_EQ(scores[1]["pages"][0]["systems"][0]["layout"], "S1-Sys1");

    EXPECT_EQ(scores[2]["layout"], "S2-ScrVw");
    EXPECT_EQ(scores[2]["name"], "RH 1");
    ASSERT_TRUE(scores[2]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[2]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[2]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[2]["multimeasureRests"][0]["start"], 1);
    ASSERT_TRUE(scores[2]["pages"].is_array());
    ASSERT_EQ(scores[2]["pages"].size(), 1);
    ASSERT_TRUE(scores[2]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[2]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[2]["pages"][0]["systems"][0]["measure"], 1);
    EXPECT_EQ(scores[2]["pages"][0]["systems"][0]["layout"], "S2-Sys1");


    EXPECT_EQ(scores[3]["layout"], "S3-ScrVw");
    EXPECT_EQ(scores[3]["name"], "H1LH");
    ASSERT_TRUE(scores[3]["multimeasureRests"].is_array());
    ASSERT_EQ(scores[3]["multimeasureRests"].size(), 1);
    EXPECT_EQ(scores[3]["multimeasureRests"][0]["duration"], 101);
    EXPECT_EQ(scores[3]["multimeasureRests"][0]["start"], 1);
    ASSERT_TRUE(scores[3]["pages"].is_array());
    ASSERT_EQ(scores[3]["pages"].size(), 1);
    ASSERT_TRUE(scores[3]["pages"][0]["systems"].is_array());
    ASSERT_EQ(scores[3]["pages"][0]["systems"].size(), 1);
    EXPECT_EQ(scores[3]["pages"][0]["systems"][0]["measure"], 1);
    EXPECT_EQ(scores[3]["pages"][0]["systems"][0]["layout"], "S3-Sys1");
}