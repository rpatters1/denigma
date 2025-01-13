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
    // musicxml -> musicxml
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
    // musicxml -> mxl
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--mxl" };
        checkStderr({ inputPath.filename().u8string() + " is not a .mxl file.", inputPath.filename().u8string() }, [&]() {
            EXPECT_NE(denigmaTestMain(args.argc(), args.argv()), 0) << "create mxl from " << inputPath.u8string() << "this should fail";
        });
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
    // mxl -> mxl
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string() };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        checkStderr(inputFile + ".massaged.mxl exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".massaged.mxl" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create from " << inputPath.u8string();
        });
        std::filesystem::path mxlFilename = utils::utf8ToPath(inputFile + ".massaged.mxl");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / mxlFilename));
    }
}

TEST(Massage, Subdirectory)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath); // inputPath points to musx file
    copyInputToOutput("musicxml/" + inputFile + ".musicxml", inputPath); // inputPath now points to musicxml file
    // musicxml -> musicxml
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / "musicxml" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "musicxml" / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "musicxml" / "-exports" / musicXmlFilename);
    }
    copyInputToOutput(inputFile + ".mxl", inputPath); // inputPath now points to mxl file
    // mxl -> mxl
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--mxl", "-exports" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        std::filesystem::path mxlFilename = utils::utf8ToPath(inputFile + ".massaged.mxl");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mxlFilename));
    }
}

TEST(Massage, OutputFilename)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath); // inputPath points to musx file
    copyInputToOutput("musicxml/" + inputFile + ".musicxml", inputPath); // inputPath now points to musicxml file
    // musicxml -> musicxml (no overwrite self)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", inputFile + ".musicxml" };
        checkStderr({ "Input and output are the same. No action taken.", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no self-overwrite " << inputPath.u8string();
        });
    }
    // musicxml -> musicxml (no ottava-left)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports/" + inputFile + ".no-ottavas-left.musicxml", "--no-extend-ottavas-left" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".no-ottavas-left.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / "musicxml" / utils::utf8ToPath(inputFile + ".no-ottavas-left.musicxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "musicxml" / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "musicxml" / "-exports" / musicXmlFilename);
    }
    // musicxml -> musicxml (no ottava-right)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports/" + inputFile + ".no-ottavas-right.musicxml", "--no-extend-ottavas-right" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".no-ottavas-right.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / "musicxml" / utils::utf8ToPath(inputFile + ".no-ottavas-right.musicxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "musicxml" / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "musicxml" / "-exports" / musicXmlFilename);
    }
    // musicxml -> musicxml (no ottava-left)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports/" + inputFile + ".no-ottavas-left.musicxml", "--no-extend-ottavas-left" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".no-ottavas-left.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / "musicxml" / utils::utf8ToPath(inputFile + ".no-ottavas-left.musicxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "musicxml" / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "musicxml" / "-exports" / musicXmlFilename);
    }
    copyInputToOutput(inputFile + ".mxl", inputPath); // inputPath now points to mxl file
    // mxl -> musicxml (no fermata-whole-rests)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports/" + inputFile + ".no-fermata-whole-rests.musicxml", "--no-fermata-whole-rests" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".no-fermata-whole-rests.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / utils::utf8ToPath(inputFile + ".no-fermata-whole-rests.musicxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / musicXmlFilename);
    }
    // mxl -> musicxml (no refloat-rests)
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports/" + inputFile + ".no-refloat-rests.musicxml", "--no-refloat-rests" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".no-refloat-rests.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / utils::utf8ToPath(inputFile + ".no-refloat-rests.musicxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / musicXmlFilename);
    }
}

TEST(Massage, Parts)
{
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath); // inputPath points to musx file
    copyInputToOutput(inputFile + ".mxl", inputPath); // inputPath now points to mxl file
    // inplace 
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "--part" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        checkStderr(inputFile + ".オボえ.massaged.musicxml exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating from " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".オボえ.massaged.musicxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".オボえ.massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / musicXmlFilename);
    }
    // explicit to subdir
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports", "--part", "オボえ" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".オボえ.massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / musicXmlFilename);
    }
    // non-existent part
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports", "--part", "Doesn't Exist" };
        checkStderr({ "No part name starting with \"Doesn't Exist\" was found", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
    }
    // all parts and score
    {
        ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--musicxml", "-exports", "--all-parts", "--force" };
        checkStderr({ "Overwriting", inputFile + ".mxl" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path musicXmlFilename = utils::utf8ToPath(inputFile + ".massaged.musicxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / musicXmlFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / musicXmlFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / musicXmlFilename);
        std::filesystem::path musicXmlFilenamePart = utils::utf8ToPath(inputFile + ".オボえ.massaged.musicxml");
        std::filesystem::path referencePathPart = getInputPath() / "reference" / musicXmlFilenamePart;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / musicXmlFilenamePart));
        compareFiles(referencePathPart, getOutputPath() / "-exports" / musicXmlFilenamePart);
    }
}

TEST(Massage, NoFinaleFile)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput("musicxml/" + inputFile + ".musicxml", inputPath); // inputPath now points to musicxml file
    ArgList args = { DENIGMA_NAME, "massage", inputPath.u8string(), "--finale-file", "." };
    checkStderr({ "Corresponding Finale document not found", inputFile + ".musicxml" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
    });
}