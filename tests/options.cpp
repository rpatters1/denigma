/*merged/
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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(Options, InsufficientOptions)
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
        ArgList args = { DENIGMA_NAME, _ARG("export") };
        checkStderr("Not enough arguments passed", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "2 arguments return error";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, _ARG("not-a-command"), _ARG("input") };
        checkStderr("Unknown command:", [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "invalid command";
        });
    }
}

TEST(Options, ParseOptions)
{
    {
        ArgList args = { DENIGMA_NAME, _ARG("--help") };
        DenigmaContext ctx(DENIGMA_NAME);
        ctx.parseOptions(args.argc(), args.argv());
        EXPECT_TRUE(ctx.showHelp);
        checkStdout(std::string("Usage: ") + ctx.programName + " <command> <input-pattern> [--options]", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "show help";
        });
    }
    {
        ArgList args = { DENIGMA_NAME, _ARG("--version") };
        DenigmaContext ctx(DENIGMA_NAME);
        ctx.parseOptions(args.argc(), args.argv());
        EXPECT_TRUE(ctx.showVersion);
        checkStdout(std::string(ctx.programName) + " " + DENIGMA_VERSION, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "show version";
        });
    }
}
