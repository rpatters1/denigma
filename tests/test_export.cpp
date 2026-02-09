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
#include <ctime>
#include <array>
#include <filesystem>
#include <iterator>

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"
#include "unzip.h"
#include "utils/ziputils.h"

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

TEST(Export, SvgOutput)
{
    setupTestDataPaths();
    std::string inputFile = "notAscii-其れ";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--svg", "--shape-def", "3" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "default svg output";
        });
        auto outputFilename = utils::utf8ToPath(inputFile + ".shape-3.svg");
        auto outputPath = getOutputPath() / outputFilename;
        EXPECT_TRUE(std::filesystem::exists(outputPath));
        assertStringInFile("<svg", outputPath);
    }
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--svg", "-exports/output.svg", "--shape-def", "3" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "explicit svg output filename";
        });
        auto explicitFile = getOutputPath() / utils::utf8ToPath("-exports/output.svg");
        auto suffixedFile = getOutputPath() / utils::utf8ToPath("-exports/output.shape-3.svg");
        EXPECT_TRUE(std::filesystem::exists(explicitFile));
        EXPECT_FALSE(std::filesystem::exists(suffixedFile));
        assertStringInFile("<svg", explicitFile);
    }
}

TEST(Export, SvgPageScaledShapeRefs)
{
    setupTestDataPaths();
#if defined(MUSX_RUNNING_ON_MACOS) || defined(MUSX_RUNNING_ON_WINDOWS)
    std::string inputFile = "pageDiffThanOpts";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);

    const std::filesystem::path referenceDir = getInputPath() / "pageDiffThanOpts_page_scaled";

    const std::array<int, 6> shapeCmpers{ 2, 3, 4, 5, 6, 7 };
    for (const int shapeCmper : shapeCmpers) {
        const std::string cmperText = std::to_string(shapeCmper);
        const std::filesystem::path outputSvg = getOutputPath() / utils::utf8ToPath(cmperText + "-test.svg");
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--svg", outputSvg.u8string(),
                         "--shape-def", cmperText, "--svg-page-scale", "--force" };
        checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "svg export for shape cmper " << cmperText;
        });

        const std::filesystem::path referenceSvg = referenceDir / utils::utf8ToPath(cmperText + ".svg");
        EXPECT_TRUE(std::filesystem::exists(referenceSvg));
        EXPECT_TRUE(std::filesystem::exists(outputSvg));
        assertStringInFile("<svg", outputSvg);
    }
#else
    GTEST_SKIP() << "SVG reference set is currently validated on macOS and Windows only.";
#endif
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

TEST(Export, NoCommandSimplestForm)
{
    setupTestDataPaths();
    std::string inputFile = "secbeams";
    std::filesystem::path inputPath;
    copyInputToOutput(inputFile + ".musx", inputPath);
    ArgList args = { DENIGMA_NAME, inputPath.u8string() };
    checkStderr({ "Processing", inputPath.filename().u8string() }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputPath.u8string();
    });
}

TEST(Export, ReverseMusxTimestamp)
{
    setupTestDataPaths();
    std::filesystem::path inputMusxPath;
    copyInputToOutput("pageDiffThanOpts.musx", inputMusxPath);

    std::filesystem::path enigmaxmlPath = getOutputPath() / "pageDiffThanOpts.enigmaxml";
    {
        ArgList args = { DENIGMA_NAME, "export", inputMusxPath.u8string(), "--enigmaxml", "--force" };
        checkStderr({ "Processing", inputMusxPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << inputMusxPath.u8string();
        });
    }

    std::filesystem::path reverseMusxPath = getOutputPath() / "-exports" / "pageDiffThanOpts.rev.musx";
    {
        ArgList args = { DENIGMA_NAME, "export", enigmaxmlPath.u8string(), "--musx", "-exports/pageDiffThanOpts.rev.musx", "--force" };
        checkStderr({ "Processing", enigmaxmlPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "create " << reverseMusxPath.u8string();
        });
    }

    ASSERT_TRUE(std::filesystem::exists(reverseMusxPath));

    const std::string zipPath = reverseMusxPath.u8string();
    unzFile zip = unzOpen64(zipPath.c_str());
    ASSERT_NE(zip, nullptr) << "unable to open " << zipPath;

    int rc = unzLocateFile(zip, "score.dat", 1);
    ASSERT_EQ(rc, UNZ_OK) << "unable to locate score.dat in " << zipPath;

    unz_file_info64 fileInfo{};
    rc = unzGetCurrentFileInfo64(zip, &fileInfo, nullptr, 0, nullptr, 0, nullptr, 0);
    ASSERT_EQ(rc, UNZ_OK) << "unable to read score.dat metadata in " << zipPath;
    unzClose(zip);

    std::time_t now = std::time(nullptr);
    std::tm localNow{};
#ifdef _WIN32
    localtime_s(&localNow, &now);
#else
    localtime_r(&now, &localNow);
#endif

    EXPECT_GE(fileInfo.tmu_date.tm_year, localNow.tm_year + 1900 - 1);
    EXPECT_LE(fileInfo.tmu_date.tm_year, localNow.tm_year + 1900 + 1);
    EXPECT_GE(fileInfo.tmu_date.tm_mon, 0);
    EXPECT_LE(fileInfo.tmu_date.tm_mon, 11);
    EXPECT_GE(fileInfo.tmu_date.tm_mday, 1);
    EXPECT_LE(fileInfo.tmu_date.tm_mday, 31);
}

TEST(Export, ReverseDefaultOutputMusx)
{
    setupTestDataPaths();
    std::filesystem::path inputEnigmaxmlPath;
    copyInputToOutput("reference/notAscii-其れ.enigmaxml", inputEnigmaxmlPath);

    std::filesystem::path defaultMusxOutputPath = inputEnigmaxmlPath;
    defaultMusxOutputPath.replace_extension(".musx");
    {
        ArgList args = { DENIGMA_NAME, inputEnigmaxmlPath.u8string(), "--force" };
        checkStderr({ "Processing", inputEnigmaxmlPath.filename().u8string(), defaultMusxOutputPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "default reverse output from " << inputEnigmaxmlPath.u8string();
        });
    }
    ASSERT_TRUE(std::filesystem::exists(defaultMusxOutputPath));
    {
        ArgList args = { DENIGMA_NAME, defaultMusxOutputPath.u8string(), "--force" };
        checkStderr({ "Processing", defaultMusxOutputPath.filename().u8string(), inputEnigmaxmlPath.filename().u8string() }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "default forward output from " << defaultMusxOutputPath.u8string();
        });
    }
    ASSERT_TRUE(std::filesystem::exists(inputEnigmaxmlPath));
}

TEST(ZipUtils, ReadMusxArchiveFiles)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("pageDiffThanOpts.musx", inputPath);

    auto archiveFiles = utils::readMusxArchiveFiles(inputPath, DenigmaContext(DENIGMA_NAME));
    EXPECT_FALSE(archiveFiles.scoreDat.empty());
    ASSERT_TRUE(archiveFiles.notationMetadata.has_value());
    EXPECT_NE(archiveFiles.notationMetadata->find("<metadata"), std::string::npos);
}

TEST(Export, MnxFromEnigmaxmlNoMetadataStillWorks)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("reference/notAscii-其れ.enigmaxml", inputPath);

    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    std::filesystem::path mnxOutput = inputPath;
    mnxOutput.replace_extension(".mnx");
    ASSERT_TRUE(std::filesystem::exists(mnxOutput));
    nlohmann::json mnxJson;
    openJson(mnxOutput, mnxJson);
    ASSERT_FALSE(mnxJson.empty());
}

TEST(Export, MultiOutputMnxAndMssFromMusx)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("pageDiffThanOpts.musx", inputPath);

    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx", "--mss" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx and mss: " << inputPath.u8string();
    });

    std::filesystem::path mnxOutput = inputPath;
    mnxOutput.replace_extension(".mnx");
    ASSERT_TRUE(std::filesystem::exists(mnxOutput));
    nlohmann::json mnxJson;
    openJson(mnxOutput, mnxJson);
    ASSERT_FALSE(mnxJson.empty());

    std::filesystem::path mssOutput = inputPath;
    mssOutput.replace_extension(".mss");
    std::filesystem::path mssReference = getInputPath() / "reference" / "pageDiffThanOpts.mss";
    ASSERT_TRUE(std::filesystem::exists(mssOutput));
    compareFiles(mssReference, mssOutput);
}
