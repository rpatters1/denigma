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

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/denigma.h"
#include "gtest/gtest.h"
#include "mx/api/DocumentManager.h"
#include "mx/api/ScoreData.h"
#include "test_utils.h"

using namespace denigma;

namespace {

struct PartGroupFixture
{
    std::string musxFile;
    std::string referenceMusicXmlFile;
};

struct ComparablePartGroup
{
    int firstPartIndex{};
    int lastPartIndex{};
    mx::api::BracketType bracketType{};
    mx::api::GroupBarline groupBarline{};

    auto operator<=>(const ComparablePartGroup&) const = default;
};

const std::vector<PartGroupFixture>& partGroupFixtures()
{
    static const std::vector<PartGroupFixture> fixtures{
        { "musicxml/large-orchestra.musx", "musicxml/large-orchestra-ref.musicxml" }
    };
    return fixtures;
}

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

std::vector<ComparablePartGroup> createComparablePartGroups(const std::vector<mx::api::PartGroupData>& partGroups)
{
    std::vector<ComparablePartGroup> result;
    result.reserve(partGroups.size());
    for (const auto& group : partGroups) {
        result.push_back({
            group.firstPartIndex,
            group.lastPartIndex,
            group.bracketType,
            group.groupBarline
        });
    }
    std::sort(result.begin(), result.end());
    return result;
}

void comparePartGroups(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualGroups = createComparablePartGroups(actual.partGroups);
    const auto expectedGroups = createComparablePartGroups(expected.partGroups);
    ASSERT_EQ(actualGroups.size(), expectedGroups.size()) << "part-group count";

    for (size_t i = 0; i < expectedGroups.size(); ++i) {
        EXPECT_EQ(actualGroups[i].firstPartIndex, expectedGroups[i].firstPartIndex) << "part-group[" << i << "].firstPartIndex";
        EXPECT_EQ(actualGroups[i].lastPartIndex, expectedGroups[i].lastPartIndex) << "part-group[" << i << "].lastPartIndex";
        EXPECT_EQ(actualGroups[i].bracketType, expectedGroups[i].bracketType) << "part-group[" << i << "].bracketType";
        EXPECT_EQ(actualGroups[i].groupBarline, expectedGroups[i].groupBarline) << "part-group[" << i << "].groupBarline";
    }
}

} // namespace

TEST(MusicXmlParts, PartGroupBracketsMatchFinaleSpans)
{
    setupTestDataPaths();

    for (const auto& fixture : partGroupFixtures()) {
        const auto outputPath = exportMusicXmlFixture(fixture.musxFile);
        const auto actualScore = loadScoreData(outputPath);
        const auto expectedScore = loadScoreData(getInputPath() / fixture.referenceMusicXmlFile);
        ASSERT_TRUE(actualScore);
        ASSERT_TRUE(expectedScore);

        SCOPED_TRACE(fixture.musxFile);
        comparePartGroups(*actualScore, *expectedScore);
    }
}

TEST(MusicXmlParts, PipeOrganExportsLoadableMusicXml)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("musicxml/pipe-organ.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
}
