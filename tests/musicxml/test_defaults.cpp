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

struct LayoutFixture
{
    std::string musxFile;
    std::string referenceMusicXmlFile;
    // Finale appears to emit an incorrect defaults-level top-system-distance; compare against Denigma's value instead.
    long expectedTopSystemDistance;
};

const std::vector<LayoutFixture>& layoutFixtures()
{
    static const std::vector<LayoutFixture> fixtures{
        { "musicxml/page70-staff82ev.musx", "musicxml/page70-staff82ev-ref.musicxml", 211 },
        { "musicxml/page70nohold-staff82ev.musx", "musicxml/page70nohold-staff82ev-ref.musicxml", 211 }
    };
    return fixtures;
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

void compareMargins(const mx::api::MarginsData& actual, const mx::api::MarginsData& expected, const char* label)
{
    expectRoundedValue(actual.left, expected.left, (std::string(label) + ".left").c_str());
    expectRoundedValue(actual.right, expected.right, (std::string(label) + ".right").c_str());
    expectRoundedValue(actual.top, expected.top, (std::string(label) + ".top").c_str());
    expectRoundedValue(actual.bottom, expected.bottom, (std::string(label) + ".bottom").c_str());
}

void comparePageLayout(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected)
{
    ASSERT_EQ(actual.pageLayout.size.has_value(), expected.pageLayout.size.has_value()) << "Presence mismatch for page size";
    if (expected.pageLayout.size) {
        expectRoundedValue(actual.pageLayout.size->height, expected.pageLayout.size->height, "page-layout.size.height");
        expectRoundedValue(actual.pageLayout.size->width, expected.pageLayout.size->width, "page-layout.size.width");
    }

    ASSERT_EQ(actual.pageLayout.margins.odd.has_value(), expected.pageLayout.margins.odd.has_value())
        << "Presence mismatch for odd page margins";
    if (expected.pageLayout.margins.odd) {
        compareMargins(*actual.pageLayout.margins.odd, *expected.pageLayout.margins.odd, "page-layout.margins.odd");
    }

    ASSERT_EQ(actual.pageLayout.margins.even.has_value(), expected.pageLayout.margins.even.has_value())
        << "Presence mismatch for even page margins";
    if (expected.pageLayout.margins.even) {
        compareMargins(*actual.pageLayout.margins.even, *expected.pageLayout.margins.even, "page-layout.margins.even");
    }
}

void compareSystemLayout(const mx::api::DefaultsData& actual, const mx::api::DefaultsData& expected, long expectedTopSystemDistance)
{
    ASSERT_EQ(actual.systemLayout.margins.has_value(), expected.systemLayout.margins.has_value())
        << "Presence mismatch for system margins";
    if (expected.systemLayout.margins) {
        expectRoundedValue(actual.systemLayout.margins->left, expected.systemLayout.margins->left, "system-layout.margins.left");
        expectRoundedValue(actual.systemLayout.margins->right, expected.systemLayout.margins->right, "system-layout.margins.right");
    }

    expectOptionalValue(actual.systemLayout.systemDistance, expected.systemLayout.systemDistance, "system-layout.system-distance");
    ASSERT_TRUE(actual.systemLayout.topSystemDistance) << "Missing system-layout.top-system-distance";
    expectRoundedValue(*actual.systemLayout.topSystemDistance, expectedTopSystemDistance, "system-layout.top-system-distance");
}

} // namespace

TEST(MusicXmlDefaults, PageAndSystemLayoutMatchFinaleRoundedTenths)
{
    setupTestDataPaths();

    for (const auto& fixture : layoutFixtures()) {
        std::filesystem::path inputPath;
        copyInputToOutput(fixture.musxFile, inputPath);

        ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml" };
        checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to musicxml: " << pathString(inputPath);
        });

        auto outputPath = inputPath;
        outputPath.replace_extension(".musicxml");
        ASSERT_TRUE(std::filesystem::exists(outputPath)) << "Missing MusicXML output " << pathString(outputPath);

        const auto actualScore = loadScoreData(outputPath);
        const auto expectedScore = loadScoreData(getInputPath() / fixture.referenceMusicXmlFile);
        ASSERT_TRUE(actualScore);
        ASSERT_TRUE(expectedScore);

        SCOPED_TRACE(fixture.musxFile);
        comparePageLayout(actualScore->defaults, expectedScore->defaults);
        compareSystemLayout(actualScore->defaults, expectedScore->defaults, fixture.expectedTopSystemDistance);
    }
}
