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

TEST(MnxSequences, Voice2TripletAtEnd)
{
    // test file contains voice 2 triplet at end of bar. The triplet was being overfilled.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2triplet_at_end.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "v2triplet_at_end.mnx");
}

TEST(MnxSequences, Voice2TupletIncomplete)
{
    // test file contains incomplete v2 triplet that should be reported without additional adding compensationg spacer to main sequence
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2tuplet_incomplete.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "less than the expected", "!more than the expected" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "v2tuplet_incomplete.mnx");
}

TEST(MnxSequences, NestedTuplets)
{
    // test file contains a nested tuplets ending on the same note.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplets_nested.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "tuplets_nested.mnx");
}

TEST(MnxSequences, NestedTrailingSingletonTuplet)
{
    // test file contains a trailing singleton tuplet nested inside another tuplet.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplet-nested-singleton.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "tuplet-nested-singleton.mnx");
}
