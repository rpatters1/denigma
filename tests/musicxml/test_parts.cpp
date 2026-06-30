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
#include <array>
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
        { "large_orchestra.musx", "musicxml/large_orchestra-ref.musicxml" }
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

void expectInitialTransposition(const mx::api::PartData& part, int chromatic, int diatonic, const char* label)
{
    ASSERT_TRUE(part.transposition) << label << " transposition";
    EXPECT_EQ(part.transposition->chromatic, chromatic) << label << ".chromatic";
    EXPECT_EQ(part.transposition->diatonic, diatonic) << label << ".diatonic";
}

mx::api::BarlineType effectiveRightBarlineType(const mx::api::MeasureData& measure)
{
    for (const auto& barline : measure.barlines) {
        if (barline.location == mx::api::HorizontalAlignment::right || barline.tickTimePosition == mx::api::TICK_TIME_INFINITY) {
            return barline.barlineType;
        }
    }
    return mx::api::BarlineType::normal;
}

struct ComparableTimeSignature
{
    std::string beats;
    std::string beatType;
    mx::api::TimeSignatureSymbol symbol{};
    bool isImplicit{};

    auto operator<=>(const ComparableTimeSignature&) const = default;
};

ComparableTimeSignature createComparableTimeSignature(const mx::api::TimeSignatureData& timeSignature)
{
    return {
        timeSignature.beats,
        timeSignature.beatType,
        timeSignature.symbol,
        timeSignature.isImplicit
    };
}

void compareTimeSignatures(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    ASSERT_EQ(actual.parts.size(), expected.parts.size()) << "part count";
    for (size_t partIndex = 0; partIndex < expected.parts.size(); ++partIndex) {
        SCOPED_TRACE("part " + std::to_string(partIndex + 1));
        const auto& actualMeasures = actual.parts.at(partIndex).measures;
        const auto& expectedMeasures = expected.parts.at(partIndex).measures;
        ASSERT_EQ(actualMeasures.size(), expectedMeasures.size()) << "measure count";
        for (size_t measureIndex = 0; measureIndex < expectedMeasures.size(); ++measureIndex) {
            const auto& actualTime = actualMeasures.at(measureIndex).timeSignature;
            const auto& expectedTime = expectedMeasures.at(measureIndex).timeSignature;
            EXPECT_EQ(createComparableTimeSignature(actualTime), createComparableTimeSignature(expectedTime))
                << "measure " << (measureIndex + 1);
        }
    }
}

void expectAllMeasureDurationsEqual(const mx::api::ScoreData& score)
{
    auto measureDuration = [](const mx::api::MeasureData& measure) -> std::optional<int> {
        if (measure.staves.empty() || measure.staves.front().voices.empty()) {
            return std::nullopt;
        }
        const auto& voice = measure.staves.front().voices.begin()->second;
        if (voice.notes.empty()) {
            return std::nullopt;
        }
        return voice.notes.front().durationData.durationTimeTicks;
    };

    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        SCOPED_TRACE("part " + std::to_string(partIndex + 1));
        const auto& measures = score.parts.at(partIndex).measures;
        ASSERT_FALSE(measures.empty());
        const auto referenceDuration = measureDuration(measures.front());
        ASSERT_TRUE(referenceDuration);
        for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
            SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
            const auto duration = measureDuration(measures.at(measureIndex));
            ASSERT_TRUE(duration);
            EXPECT_EQ(*duration, *referenceDuration);
        }
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

TEST(MusicXmlParts, PipeOrganExportsThreeStaffPartWithManualsBrace)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("musicxml/pipe-organ.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_EQ(actualScore->parts.size(), 1);

    const auto& organPart = actualScore->parts.front();
    ASSERT_EQ(organPart.measures.size(), 1);
    const auto& firstMeasure = organPart.measures.front();
    ASSERT_EQ(firstMeasure.staves.size(), 3);
    ASSERT_TRUE(firstMeasure.partSymbol);
    EXPECT_EQ(firstMeasure.partSymbol->value, mx::api::BracketType::brace);
    EXPECT_EQ(firstMeasure.partSymbol->topStaff, 1);
    EXPECT_EQ(firstMeasure.partSymbol->bottomStaff, 2);
}

TEST(MusicXmlParts, LargeOrchestraExportsInitialTranspositions)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("large_orchestra.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_GT(actualScore->parts.size(), 15);

    expectInitialTransposition(actualScore->parts.at(0), 12, 7, "piccolo");
    expectInitialTransposition(actualScore->parts.at(9), -2, -1, "clarinet in Bb");
    expectInitialTransposition(actualScore->parts.at(15), -7, -4, "horn in F");
}

TEST(MusicXmlParts, BarlinesOverrideCorrectTypes)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("barline_types.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 10) << "should be at least ten measures";

    constexpr std::array<mx::api::BarlineType, 10> expected = {
        mx::api::BarlineType::lightLight,
        mx::api::BarlineType::normal,
        mx::api::BarlineType::lightHeavy,
        mx::api::BarlineType::heavy,
        mx::api::BarlineType::dashed,
        mx::api::BarlineType::dashed,
        mx::api::BarlineType::none,
        mx::api::BarlineType::short_,
        mx::api::BarlineType::tick,
        mx::api::BarlineType::lightLight
    };

    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(effectiveRightBarlineType(measures[i]), expected[i]) << "measure " << (i + 1);
    }
}

TEST(MusicXmlParts, ChangingTimeSignaturesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("timesigs_changing.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/timesigs_changing-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTimeSignatures(*actualScore, *expectedScore);
}

TEST(MusicXmlParts, IndependentTimeSignaturesExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("indtime_clefchange.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/indtime_clefchange-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTimeSignatures(*actualScore, *expectedScore);
    expectAllMeasureDurationsEqual(*actualScore);
}
