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
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "test_utils.h"
#include "xml_compare.h"

// Optional setup/teardown for test suite
class TestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Code to run before all tests
    }

    void TearDown() override {
        // Code to run after all tests
    }
};

// checks for a specific error string in the output
void checkStderr(const std::vector<std::string>& expectedMessages, std::function<void()> callback)
{
    // Redirect stderr to capture messages
    std::ostringstream nullStream; // used to suppress std::cout
    std::streambuf* originalCout = std::cout.rdbuf(nullStream.rdbuf()); // Redirect std::cout to null
    std::ostringstream errStream;
    std::streambuf* originalCerr = std::cerr.rdbuf(errStream.rdbuf());

    // do the callback
    callback();

    // Restore stderr and stdout
    std::cout.rdbuf(originalCout);
    std::cerr.rdbuf(originalCerr);

    // check for errStream for error
    std::string capturedErrors = errStream.str();
    for (const auto& expectedMessage : expectedMessages) {
        if (expectedMessage.empty()) {
            EXPECT_TRUE(capturedErrors.empty()) << "No message expected but got " << capturedErrors;
        } else if (expectedMessage[0] == '!') {
            EXPECT_EQ(capturedErrors.find(expectedMessage.substr(1)), std::string::npos)
                << "Message \"" << expectedMessage.substr(1) << "\" found but not expected" << capturedErrors;;
        } else {
            EXPECT_NE(capturedErrors.find(expectedMessage), std::string::npos)
                << "Message \"" << expectedMessage << "\" not found. Actual: " << capturedErrors;
        }
    }
};

void checkStdout(const std::vector<std::string>& expectedMessages, std::function<void()> callback)
{
    // Redirect stdout to capture messages
    std::ostringstream nullStream; // used to suppress std::cout
    std::streambuf* originalCerr = std::cerr.rdbuf(nullStream.rdbuf()); // Redirect std::cerr to null
    std::ostringstream coutStream; // used to suppress std::cout
    std::streambuf* originalCout = std::cout.rdbuf(coutStream.rdbuf()); // Redirect std::cout to null

    // do the callback
    callback();

    // Restore stderr and stdout
    std::cout.rdbuf(originalCout);
    std::cerr.rdbuf(originalCerr);

    EXPECT_TRUE(nullStream.str().empty()) << "Error occurred: " << nullStream.str();

    // check for coutStream for error
    std::string capturedMessages = coutStream.str();
    for (const auto& expectedMessage : expectedMessages) {
        if (expectedMessage.empty()) {
            EXPECT_TRUE(capturedMessages.empty()) << "No message expected but got " << capturedMessages;
        } else if (expectedMessage[0] == '!') {
            EXPECT_EQ(capturedMessages.find(expectedMessage.substr(1)), std::string::npos)
                << "Message \"" << expectedMessage.substr(1) << "\" found but not expected";
        } else {
            EXPECT_NE(capturedMessages.find(expectedMessage), std::string::npos)
                << "Message \"" << expectedMessage << "\" not found. Actual: " << capturedMessages;
        }
    }
}

void setupTestDataPaths()
{
    auto workingDir = std::filesystem::current_path();
    ASSERT_TRUE(std::filesystem::is_directory(workingDir));
    ASSERT_GE(std::distance(workingDir.begin(), workingDir.end()), 2);
    ASSERT_EQ(pathString(*(--workingDir.end())), "data");
    auto parentDir = workingDir.parent_path();
    ASSERT_EQ(pathString(*(--parentDir.end())), "tests");
    ASSERT_TRUE(std::filesystem::exists(getInputPath()));
    ASSERT_NO_THROW({
        auto exportsDir = std::filesystem::current_path() / "-exports";
        if (std::filesystem::exists(exportsDir)) {
            std::filesystem::remove_all(exportsDir);
        }
        auto outputDir = getOutputPath();
        if (std::filesystem::exists(outputDir)) {
            std::filesystem::remove_all(outputDir);
        }
        std::filesystem::create_directories(outputDir);
    });
    ASSERT_TRUE(std::filesystem::exists(getOutputPath()));
}

void copyInputToOutput(const std::string& fileName, std::filesystem::path& outputPath)
{
    auto inputPath = getInputPath() / utils::utf8ToPath(fileName);
    ASSERT_TRUE(std::filesystem::exists(inputPath));
    outputPath = getOutputPath() / utils::utf8ToPath(fileName);
    ASSERT_NO_THROW({
        std::filesystem::create_directories(outputPath.parent_path());
        std::filesystem::copy(inputPath, outputPath, std::filesystem::copy_options::overwrite_existing);
    });
    ASSERT_TRUE(std::filesystem::exists(outputPath));
}

void readFile(const std::filesystem::path& filePath, std::vector<char>& contents)
{
    ASSERT_TRUE(std::filesystem::is_regular_file(filePath)) << pathString(filePath) << " is not a file";
    std::ifstream file(filePath, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "failed to open file: " << pathString(filePath);
    contents = std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void compareFiles(const std::filesystem::path& path1, const std::filesystem::path& path2)
{
    ASSERT_TRUE(std::filesystem::is_regular_file(path1)) << "unable to find " << pathString(path1);
    ASSERT_TRUE(std::filesystem::is_regular_file(path2)) << "unable to find " << pathString(path2);
    const bool compareAsXml = shouldUseXmlComparison(path1) && shouldUseXmlComparison(path2);
    if (compareAsXml) {
        std::string message;
        ASSERT_TRUE(compareXmlFiles(path1, path2, message)) << message;
        return;
    }

    std::ifstream file1(path1, std::ios::binary);
    ASSERT_TRUE(file1);
    std::ifstream file2(path2, std::ios::binary);
    ASSERT_TRUE(file2);
    char c1, c2;
    while (file1.get(c1)) {
        ASSERT_TRUE(file2.get(c2));
        ASSERT_EQ(c1, c2) << "comparing " << pathString(path1) << " and " << pathString(path2);
    }
    EXPECT_FALSE(file2.get(c2));
}

void assertStringsInFile(const std::vector<std::string>& targets, const std::filesystem::path& filePath, const std::filesystem::path& extension)
{
    std::filesystem::path actualFilePath;
    if (std::filesystem::is_regular_file(filePath)) {
        actualFilePath = filePath;
    } else if (std::filesystem::is_directory(filePath)) {
        auto it = std::filesystem::directory_iterator(filePath);
        auto matchingFile = std::find_if(begin(it), end(it), [&extension](const std::filesystem::directory_entry& entry) {
            return entry.is_regular_file() && entry.path().extension() == extension;
        });
        ASSERT_NE(matchingFile, std::filesystem::end(it)) << "No file with extension " << pathString(extension) << " found in directory: " << pathString(filePath);
        actualFilePath = matchingFile->path();
    } else {
        FAIL() << "Path is neither a regular file nor a directory: " << pathString(filePath);
    }
    std::ifstream file(actualFilePath, std::ios::binary);
    ASSERT_TRUE(file.is_open()) << "failed to open file: " << pathString(actualFilePath);
    std::string fileContents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    for (const auto& target : targets) {
        EXPECT_NE(fileContents.find(target), std::string::npos)
            << "String \"" << target << "\" not found in file: " << pathString(actualFilePath);
    }
}

void openJson(const std::filesystem::path& path, nlohmann::json& result)
{
    std::ifstream file(path);
    ASSERT_TRUE(file) << "opening " << pathString(path);
    nlohmann::json mnx;
    ASSERT_NO_THROW(file >> mnx) << "parsing JSON file " << pathString(path);
    result = mnx;
}

int main(int argc, char** argv)
{
#ifndef MUSX_TEST_DATA_PATH
#error MUSX_TEST_DATA_PATH is not defined. Please set it via target_compile_definitions.
#else
    musx::util::TestConfiguration::setTestDataPath(MUSX_TEST_DATA_PATH);
#endif

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new TestEnvironment);
    return RUN_ALL_TESTS();
}
