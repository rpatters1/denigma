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

TEST(MnxGlobal, Tempos)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tempo_text_shape.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "tempo_text_shape.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 1) << "should be at least one measure";
    auto tempos = measures[0].tempos();
    ASSERT_TRUE(tempos.has_value()) << "should have tempos in the first measure";
    ASSERT_EQ(tempos.value().size(), 2) << "should have 2 tempos in the first measure";

    auto testTempo = [&](auto tempo, int bpm, int numerator, int denominator) {
        EXPECT_EQ(tempo.bpm(), bpm);
        if (tempo.location().has_value()) {
            EXPECT_EQ(tempo.location().value().fraction().numerator(), numerator);
            EXPECT_EQ(tempo.location().value().fraction().denominator(), denominator);
        } else {
            EXPECT_EQ(numerator, 0);
            EXPECT_EQ(denominator, 1);
        }
    };
    {
        auto tempo = tempos.value()[0];
        EXPECT_FALSE(tempo.location().has_value());
        testTempo(tempo, 73, 0, 1);
    }
    {
        auto tempo = tempos.value()[1];
        EXPECT_TRUE(tempo.location().has_value());
        testTempo(tempo, 71, 1, 2);
    }
}