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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(Schema, InputSchemaValid)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("notAscii-其れ.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx", "--mnx-schema", (getInputPath() / "mnx" / "mnx-schema.json").u8string() };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Schema validation errors" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "validate " << inputPath.u8string();
    });
}

TEST(Schema, InputSchemaNotValid)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("notAscii-其れ.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx", "--mnx-schema", (getInputPath() / "mnx" / "generic-schema.json").u8string() };
    checkStderr({ "Processing", inputPath.filename().u8string(), "Schema validation errors" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "wrong schema validate " << inputPath.u8string();
    });
}

TEST(Schema, EmbeddedSchemaValid)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("notAscii-其れ.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Schema validation errors" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "validate " << inputPath.u8string();
    });
}
