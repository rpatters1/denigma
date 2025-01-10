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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(Options, IncorrectOptions)
{
    {
        ArgList args = {};
        checkStderr("argv[0] is unavailable", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "No arguments returns error";
        });
    }

    {
        ArgList args = { DENIGMA_NAME };
        checkStderr("", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "1 argument returns help page but no message";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "export" };
        checkStderr("Not enough arguments passed", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "2 arguments return error";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "not-a-command", "input" };
        checkStderr("Unknown command:", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid command";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "--testing", "export", "input", "--xxx" };
        checkStderr("Unsupported format: ", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format (none)";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "--testing", "export", "input.yyy", "--xxx" };
        checkStderr("Unsupported format: yyy", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "--testing", "export", "input.enigmaxml", "--xxx" };
        checkStderr("Unsupported format: xxx", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid output format";
        });
    }
}

TEST(Options, ParseOptions)
{
    {
        ArgList args = { DENIGMA_NAME, "--help" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 0);
        EXPECT_TRUE(ctx.showHelp);
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStdout(std::string("Usage: ") + ctx.programName + " <command> <input-pattern> [--options]", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "show help";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "--version" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 0);
        EXPECT_TRUE(ctx.showVersion);
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStdout(std::string(ctx.programName) + " " + DENIGMA_VERSION, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "show version";
        });
    }
    {
        static const std::string fileName = "notAscii-其れ.invalid";
        ArgList args = { DENIGMA_NAME, "export", fileName };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_EQ(std::filesystem::path(newArgs[1]).u8string(), fileName) << "utf-8 encoding check";
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStderr("Input path " + fileName + " does not exist", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format";
        });
    }
    {
        static const std::string fileName = "notAscii-其れ";
        ArgList args = { DENIGMA_NAME, "--testing", "export", fileName + ".musx" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_EQ(std::filesystem::path(newArgs[1]).u8string(), fileName + ".musx") << "utf-8 encoding check";
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStderr({ "Extracting " + fileName + ".musx", "Writing", fileName + ".enigmaxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format";
        });
    }
    {
        static const std::string fileName = "notAscii-其れ";
        static const std::string subDir = "-exports";
        ArgList args = { DENIGMA_NAME, "--testing", "export", fileName + ".musx", "--enigmaxml", subDir };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 4);
        EXPECT_EQ(std::filesystem::path(newArgs[1]).u8string(), fileName + ".musx") << "utf-8 encoding check";
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStderr({ "Extracting " + fileName + ".musx", "Writing", subDir + DIRECTORY_SEP + fileName + ".enigmaxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format";
        });
    }
    {
        static const std::string fileName = "notAscii-其れ";
        static const std::string subDir = "-exports";
        ArgList args = { DENIGMA_NAME, "--testing", "export", fileName + ".musx", "--mss", subDir };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 4);
        EXPECT_EQ(std::filesystem::path(newArgs[1]).u8string(), fileName + ".musx") << "utf-8 encoding check";
        EXPECT_FALSE(ctx.logFilePath.has_value());
        checkStderr({ "Extracting " + fileName + ".musx", "Converting", subDir + DIRECTORY_SEP + fileName + ".mss" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid input format";
        });
    }
}

TEST(Options, MassageOptions)
{
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--target", "MuseScore" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--target", "doRico" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--target", "lilypond" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--target", "lilypond", "--no-refloat-rests" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-refloat-rests" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-left" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_FALSE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-right" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-refloat-rests" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-left", "--no-extend-ottavas-right", "--no-fermata-whole-rests", "--no-refloat-rests",
                            "--extend-ottavas-left" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_TRUE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_FALSE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-left", "--no-extend-ottavas-right", "--no-fermata-whole-rests", "--no-refloat-rests",
                            "--extend-ottavas-right" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_FALSE(ctx.extendOttavasLeft);
        EXPECT_TRUE(ctx.extendOttavasRight);
        EXPECT_FALSE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-left", "--no-extend-ottavas-right", "--no-fermata-whole-rests", "--no-refloat-rests",
                            "--fermata-whole-rests" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_FALSE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_TRUE(ctx.fermataWholeRests);
        EXPECT_FALSE(ctx.refloatRests);
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--no-extend-ottavas-left", "--no-extend-ottavas-right", "--no-fermata-whole-rests", "--no-refloat-rests",
                            "--refloat-rests" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_FALSE(ctx.extendOttavasLeft);
        EXPECT_FALSE(ctx.extendOttavasRight);
        EXPECT_FALSE(ctx.fermataWholeRests);
        EXPECT_TRUE(ctx.refloatRests);
        EXPECT_FALSE(ctx.finaleFilePath.has_value());
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--finale-file" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        EXPECT_FALSE(ctx.finaleFilePath.has_value());
    }
    {
        static const std::string fileName = "notAscii-其れ.mxl";
        ArgList args = { DENIGMA_NAME, "--testing", "massage", fileName, "--finale-file", "parentƒ" };
        DenigmaContext ctx(DENIGMA_NAME);
        auto newArgs = ctx.parseOptions(args.argc(), args.argv());
        EXPECT_EQ(newArgs.size(), 2);
        ASSERT_TRUE(ctx.finaleFilePath.has_value());
        EXPECT_EQ(ctx.finaleFilePath.value().u8string(), "parentƒ");
    }
}