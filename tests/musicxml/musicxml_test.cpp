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
#include "formats/enigmaxml/enigmaxml.h"
#include "formats/musicxml/musicxml.h"
#include "gtest/gtest.h"
#include "mx/api/MusicXml.h"
#include "test_utils.h"

namespace denigma::test::musicxml {

std::filesystem::path exportMusicXmlFixture(const std::string& musxFile, bool useFinaleRestPosition)
{
    std::filesystem::path inputPath;
    copyInputToOutput(musxFile, inputPath);

    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml" };
    if (useFinaleRestPosition) {
        args.add("--finale-rest-position");
    }
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to musicxml: " << pathString(inputPath);
    });

    auto outputPath = inputPath;
    outputPath.replace_extension(".musicxml");
    EXPECT_TRUE(std::filesystem::exists(outputPath)) << "Missing MusicXML output " << pathString(outputPath);
    return outputPath;
}

std::optional<mx::api::ScoreData> createScoreDataFromMusicXmlFixture(const std::string& musxFile)
{
    std::filesystem::path inputPath;
    copyInputToOutput(musxFile, inputPath);

    return createScoreDataFromMusxPath(inputPath);
}

std::optional<mx::api::ScoreData> createScoreDataFromMusxPath(const std::filesystem::path& musxPath)
{
    DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = musxPath;
    MusxLoggerScope musxLogger(makeMusxLogCallback(denigmaContext));

    try {
        const auto inputData = formats::enigmaxml::detail::extractMusxInputData(musxPath, denigmaContext);
        return formats::musicxml::detail::createMusicXmlDocument(inputData, denigmaContext);
    } catch (const std::exception& ex) {
        ADD_FAILURE() << "Unable to create MusicXML ScoreData from " << pathString(musxPath) << ": " << ex.what();
    }
    return std::nullopt;
}

std::optional<mx::api::ScoreData> loadScoreData(const std::filesystem::path& path)
{
    const auto documentResult = mx::api::MusicXml::fromFile(pathString(path));
    EXPECT_TRUE(documentResult.ok()) << "Unable to load " << pathString(path)
        << ": " << mx::api::formatError(documentResult.error());
    if (!documentResult.ok()) {
        return std::nullopt;
    }

    const auto scoreDataResult = mx::api::getScore(documentResult.value());
    EXPECT_TRUE(scoreDataResult.ok()) << "Unable to read ScoreData from " << pathString(path)
        << ": " << mx::api::formatError(scoreDataResult.error());
    if (!scoreDataResult.ok()) {
        return std::nullopt;
    }
    return scoreDataResult.value();
}

} // namespace denigma::test::musicxml
