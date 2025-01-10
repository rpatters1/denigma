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

TEST(Massage, InPlace)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath); // inputPath points to musx file
    copyInputToOutput("musicxml/" + inputFile + ".musicxml", inputPath); // inputPath now points to musicxml file
    // musicxml
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string() };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        checkStderr(inputFile + ".massaged.musicxml exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".massaged.musicxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / "musicxml" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "musicxml" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "musicxml" / musicXmlFilename);
    }
    copyInputToOutput(inputFile + ".mxl", inputPath); // inputPath now points to mxl file
    // mxl -> musicxml
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        checkStderr(inputFile + ".massaged.musicxml exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".massaged.musicxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / musicXmlFilename);
    }
}
