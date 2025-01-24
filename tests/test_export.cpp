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

TEST(Export, InPlace)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // enigmaxml
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string() };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        ArgList quietArgs = args;
        quietArgs.add(_ARG("--quiet"));
        checkStderr(inputFile + ".enigmaxml exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(quietArgs.argc(), quietArgs.argv()), 0) << "no force options when creating " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".enigmaxml" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create " << inputPath.u8string();
        });
        std::filesystem::path enigmaFilename = utils::utf8ToPath(inputFile + ".enigmaxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / enigmaFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / enigmaFilename));
        compareFiles(referencePath, getOutputPath() / enigmaFilename);
    }
    // mss
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        checkStderr(inputFile + ".mss exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating " << inputPath.u8string();
        });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".mss" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create " << inputPath.u8string();
        });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / mssFilename));
        compareFiles(referencePath, getOutputPath() / mssFilename);
    }
}

TEST(Export, Subdirectory)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // enigmaxml
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--enigmaxml", "-exports" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        std::filesystem::path enigmaFilename = utils::utf8ToPath(inputFile + ".enigmaxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / enigmaFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / enigmaFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / enigmaFilename);
    }
    // mss
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "-exports" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mssFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / mssFilename);
    }
}

TEST(Export, OutputFilename)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // enigmaxml
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--enigmaxml", "-exports/output.enigmaxml" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        std::filesystem::path enigmaFilename = utils::utf8ToPath("output.enigmaxml");
        std::filesystem::path referencePath = getInputPath() / "reference" / utils::utf8ToPath(inputFile + ".enigmaxml");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / enigmaFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / enigmaFilename);
    }
    // mss
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "-exports/output.mss" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
        });
        std::filesystem::path mssFilename = utils::utf8ToPath("output.mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / utils::utf8ToPath(inputFile + ".mss");
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mssFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / mssFilename);
    }
}

TEST(Export, Parts)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    // inplace 
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "--part" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
            });
        checkStderr(inputFile + ".オボえ.mss exists. Use --force to overwrite it.", [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "no force options when creating from " << inputPath.u8string();
            });
        args.add(_ARG("--force"));
        checkStderr({ "Overwriting", inputFile + ".オボえ.mss" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "force create from " << inputPath.u8string();
            });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".オボえ.mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / mssFilename));
        compareFiles(referencePath, getOutputPath() / mssFilename);
    }
    // explicit to subdir
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "-exports", "--part", "オボえ" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
            });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".オボえ.mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mssFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / mssFilename);
    }
    // non-existent part
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "-exports", "--part", "Doesn't Exist" };
        checkStderr({ "No part name starting with \"Doesn't Exist\" was found", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
    }
    // all parts and score
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss", "-exports", "--all-parts", "--force" };
        checkStderr({ "Overwriting", inputFile + ".mss" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
        });
        std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".mss");
        std::filesystem::path referencePath = getInputPath() / "reference" / mssFilename;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mssFilename));
        compareFiles(referencePath, getOutputPath() / "-exports" / mssFilename);
        std::filesystem::path mssFilenamePart = utils::utf8ToPath(inputFile + ".オボえ.mss");
        std::filesystem::path referencePathPart = getInputPath() / "reference" / mssFilenamePart;
        EXPECT_TRUE(std::filesystem::exists(getOutputPath() / "-exports" / mssFilenamePart));
        compareFiles(referencePathPart, getOutputPath() / "-exports" / mssFilenamePart);
    }
}

TEST(Export, CalcPageFormat)
{
    setupTestDataPaths();
    std::string inputFile = "pageDiffThanOpts";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mss" };
    checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << inputPath.u8string();
    });
    std::filesystem::path mssFilename = utils::utf8ToPath(inputFile + ".mss");
    std::filesystem::path referencePath = getInputPath() / "reference" / utils::utf8ToPath(inputFile + ".mss");
    EXPECT_TRUE(std::filesystem::exists(getOutputPath() / mssFilename));
    compareFiles(referencePath, getOutputPath() / mssFilename);
}

TEST(Export, CurrentDirectoryPattern)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("notAscii-其れ.musx", inputPath);
    copyInputToOutput("pageDiffThanOpts.musx", inputPath);
    copyInputToOutput("tremolos.musx", inputPath);
    auto currentPath = std::filesystem::current_path();
    std::filesystem::current_path(inputPath.parent_path());
    ArgList args = { DENIGMA_NAME, "export", "*.musx", "--no-log" };
    checkStderr({ "Processing", "pageDiffThanOpts.musx", "tremolos.musx", "notAscii-" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << "*.musx";
    });
    std::filesystem::current_path(currentPath);
}

TEST(Export, AutoGlobSimulation)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("notAscii-其れ.musx", inputPath);
    copyInputToOutput("pageDiffThanOpts.musx", inputPath);
    copyInputToOutput("tremolos.musx", inputPath);
    auto currentPath = std::filesystem::current_path();
    std::filesystem::current_path(inputPath.parent_path());
    ArgList args = { DENIGMA_NAME, "export", "notAscii-其れ.musx", "pageDiffThanOpts.musx", "tremolos.musx", "--no-log" };
    checkStderr({ "Processing", "pageDiffThanOpts.musx", "tremolos.musx", "notAscii-" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create from " << "*.musx";
    });
    std::filesystem::current_path(currentPath);
}
