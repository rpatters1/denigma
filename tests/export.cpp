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

TEST(Export, InplaceExport)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // enigmaxml
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string() };
        checkStderr({ "Processing", inputFile + ".musx" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
        });
        std::filesystem::path enigmaFilename = utils::utf8ToPath(inputFile + ".enigmaxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / enigmaFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / enigmaFilename));
        compareFiles(referencePath, getOutputPath() / enigmaFilename);
    }
    // mss
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss" };
        checkStderr({ "Processing", inputFile + ".musx" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
        });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / mssFilename));
        compareFiles(referencePath, getOutputPath() / mssFilename);
    }
}
