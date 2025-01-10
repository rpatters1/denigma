/*
 * Copyright (C) 2024, Robert Patterson
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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(Logging, InPlace)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // default log
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--log" };
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        auto logPath = inputPath.parent_path() / (std::string(DENIGMA_NAME) + "-logs");
        assertStringsInFile({ "Processing", inputPath.filename().u8string(), "Output", inputFile + ".enigmaxml" }, logPath, ".log");
    }
}

TEST(Logging, Subdirectory)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // default log in specified subdirectory
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--log", "logs" };
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        auto logPath = inputPath.parent_path() / "logs";
        assertStringsInFile({ "Processing", inputPath.filename().u8string(), "Output", inputFile + ".enigmaxml" }, logPath, ".log");
    }
}

TEST(Logging, SpecificFile)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // specific file with appending
    {
        ArgList args1 = { DENIGMA_NAME, "export", inputPath.u8string(), "--log", "logs/mylog.log" };
        EXPECT_EQ(denigmaTestMain(args1.argc(), args1.argv()), 0) << "create from " << inputPath.u8string();
        ArgList args2 = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "--log", "logs/mylog.log" };
        EXPECT_EQ(denigmaTestMain(args2.argc(), args2.argv()), 0) << "create from " << inputPath.u8string();
        auto logPath = inputPath.parent_path() / "logs";
        assertStringsInFile({ "Processing", inputPath.filename().u8string(), "Output", inputFile + ".enigmaxml", inputFile + ".mss" }, logPath, ".log");
    }
}
