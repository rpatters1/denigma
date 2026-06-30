/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "musicxml_test.h"

#include "core/denigma.h"
#include "gtest/gtest.h"
#include "mx/api/DocumentManager.h"
#include "test_utils.h"

namespace denigma::test::musicxml {

std::filesystem::path exportMusicXmlFixture(const std::string& musxFile)
{
    std::filesystem::path inputPath;
    copyInputToOutput(musxFile, inputPath);

    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to musicxml: " << pathString(inputPath);
    });

    auto outputPath = inputPath;
    outputPath.replace_extension(".musicxml");
    EXPECT_TRUE(std::filesystem::exists(outputPath)) << "Missing MusicXML output " << pathString(outputPath);
    return outputPath;
}

std::optional<mx::api::ScoreData> loadScoreData(const std::filesystem::path& path)
{
    auto& documentManager = mx::api::DocumentManager::getInstance();
    const auto documentIdResult = documentManager.createFromFile(pathString(path));
    EXPECT_TRUE(documentIdResult.ok()) << "Unable to load " << pathString(path) << ": " << documentIdResult.error().message;
    if (!documentIdResult.ok()) {
        return std::nullopt;
    }

    const auto documentId = documentIdResult.value();
    const auto scoreDataResult = documentManager.getData(documentId);
    documentManager.destroyDocument(documentId);
    EXPECT_TRUE(scoreDataResult.ok()) << "Unable to read ScoreData from " << pathString(path) << ": " << scoreDataResult.error().message;
    if (!scoreDataResult.ok()) {
        return std::nullopt;
    }
    return scoreDataResult.value();
}

} // namespace denigma::test::musicxml
