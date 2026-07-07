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
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mx/api/ScoreData.h"
#include "musicxml_test.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

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

struct ComparableKeySignature
{
    int fifths{};
    mx::api::KeyMode mode{};

    auto operator<=>(const ComparableKeySignature&) const = default;
};

struct ComparableTranspositionEvent
{
    size_t partIndex{};
    size_t measureIndex{};
    int tickTimePosition{};
    int staffIndex{};
    int chromatic{};
    int diatonic{};

    auto operator<=>(const ComparableTranspositionEvent&) const = default;
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

ComparableKeySignature createComparableKeySignature(const mx::api::KeyData& key)
{
    return {
        key.fifths,
        key.mode
    };
}

std::vector<std::vector<ComparableKeySignature>> createEffectiveKeySignatureTracks(const mx::api::ScoreData& score)
{
    std::vector<std::vector<ComparableKeySignature>> result;
    for (const auto& part : score.parts) {
        size_t staffCount = 1;
        for (const auto& measure : part.measures) {
            staffCount = (std::max)(staffCount, measure.staves.size());
        }

        std::vector<std::vector<ComparableKeySignature>> partTracks(staffCount);
        std::vector<std::optional<ComparableKeySignature>> currentKeys(staffCount);
        for (const auto& measure : part.measures) {
            for (const auto& key : measure.keys) {
                const auto comparableKey = createComparableKeySignature(key);
                if (key.staffIndex < 0) {
                    for (auto& currentKey : currentKeys) {
                        currentKey = comparableKey;
                    }
                } else if (static_cast<size_t>(key.staffIndex) < currentKeys.size()) {
                    currentKeys[static_cast<size_t>(key.staffIndex)] = comparableKey;
                }
            }

            for (size_t staffIndex = 0; staffIndex < staffCount; ++staffIndex) {
                partTracks[staffIndex].push_back(currentKeys[staffIndex].value_or(ComparableKeySignature{}));
            }
        }

        result.insert(result.end(), partTracks.begin(), partTracks.end());
    }
    return result;
}

std::vector<ComparableTranspositionEvent> collectTranspositionEvents(const mx::api::ScoreData& score)
{
    std::vector<ComparableTranspositionEvent> result;
    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        const auto& part = score.parts[partIndex];
        for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
            const auto& measure = part.measures[measureIndex];
            for (const auto& transposition : measure.transpositions) {
                result.push_back({
                    partIndex,
                    measureIndex,
                    transposition.tickTimePosition,
                    transposition.staffIndex,
                    transposition.chromatic,
                    transposition.diatonic
                });
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
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

void compareCompositeTimeSignatures(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    ASSERT_EQ(actual.parts.size(), expected.parts.size()) << "part count";
    bool replacedFirstExplicitTimeSignature = false;
    for (size_t partIndex = 0; partIndex < expected.parts.size(); ++partIndex) {
        SCOPED_TRACE("part " + std::to_string(partIndex + 1));
        const auto& actualMeasures = actual.parts.at(partIndex).measures;
        const auto& expectedMeasures = expected.parts.at(partIndex).measures;
        ASSERT_EQ(actualMeasures.size(), expectedMeasures.size()) << "measure count";
        for (size_t measureIndex = 0; measureIndex < expectedMeasures.size(); ++measureIndex) {
            auto expectedTime = createComparableTimeSignature(expectedMeasures.at(measureIndex).timeSignature);
            if (!replacedFirstExplicitTimeSignature && !expectedTime.isImplicit) {
                expectedTime.beats = "133";
                expectedTime.beatType = "32";
                expectedTime.symbol = mx::api::TimeSignatureSymbol::unspecified;
                replacedFirstExplicitTimeSignature = true;
            }
            EXPECT_EQ(createComparableTimeSignature(actualMeasures.at(measureIndex).timeSignature), expectedTime)
                << "measure " << (measureIndex + 1);
        }
    }
    EXPECT_TRUE(replacedFirstExplicitTimeSignature);
}

void compareKeySignatures(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualTracks = createEffectiveKeySignatureTracks(actual);
    const auto expectedTracks = createEffectiveKeySignatureTracks(expected);
    ASSERT_EQ(actualTracks.size(), expectedTracks.size()) << "key-signature staff track count";
    for (size_t trackIndex = 0; trackIndex < expectedTracks.size(); ++trackIndex) {
        SCOPED_TRACE("key-signature staff track " + std::to_string(trackIndex + 1));
        ASSERT_EQ(actualTracks[trackIndex].size(), expectedTracks[trackIndex].size()) << "measure count";
        for (size_t measureIndex = 0; measureIndex < expectedTracks[trackIndex].size(); ++measureIndex) {
            EXPECT_EQ(actualTracks[trackIndex][measureIndex], expectedTracks[trackIndex][measureIndex])
                << "measure " << (measureIndex + 1);
        }
    }
}

void compareTranspositions(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualEvents = collectTranspositionEvents(actual);
    const auto expectedEvents = collectTranspositionEvents(expected);
    ASSERT_EQ(actualEvents.size(), expectedEvents.size()) << "transposition event count";
    for (size_t index = 0; index < expectedEvents.size(); ++index) {
        EXPECT_EQ(actualEvents[index], expectedEvents[index]) << "transposition event " << index;
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

TEST(MusicXmlParts, ShortBarlineStyleAppliesPerPart)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("barline_short_normal.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_GE(actualScore->parts.size(), 2);

    const auto& topPartMeasures = actualScore->parts.at(0).measures;
    const auto& secondPartMeasures = actualScore->parts.at(1).measures;
    ASSERT_GE(topPartMeasures.size(), 3);
    ASSERT_GE(secondPartMeasures.size(), 3);

    EXPECT_EQ(effectiveRightBarlineType(topPartMeasures.at(0)), mx::api::BarlineType::short_) << "top part measure 1";
    EXPECT_EQ(effectiveRightBarlineType(topPartMeasures.at(1)), mx::api::BarlineType::short_) << "top part measure 2";
    EXPECT_EQ(effectiveRightBarlineType(topPartMeasures.at(2)), mx::api::BarlineType::lightHeavy) << "top part measure 3";

    EXPECT_EQ(effectiveRightBarlineType(secondPartMeasures.at(0)), mx::api::BarlineType::normal) << "second part measure 1";
    EXPECT_EQ(effectiveRightBarlineType(secondPartMeasures.at(1)), mx::api::BarlineType::normal) << "second part measure 2";
    EXPECT_EQ(effectiveRightBarlineType(secondPartMeasures.at(2)), mx::api::BarlineType::lightHeavy) << "second part measure 3";
}

TEST(MusicXmlParts, RepeatsExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("repeats.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
}

TEST(MusicXmlParts, RepeatsExportJumpSound)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("repeats.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    constexpr size_t jumpMeasureIndex = 9;
    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GT(measures.size(), jumpMeasureIndex);
    ASSERT_FALSE(measures.at(jumpMeasureIndex).staves.empty());

    const auto& directions = measures.at(jumpMeasureIndex).staves.front().directions;
    const auto jumpDirectionIt = std::find_if(directions.begin(), directions.end(), [](const auto& direction) {
        return direction.isSoundDataSpecified && direction.soundData.dalsegno == "12";
    });
    ASSERT_NE(jumpDirectionIt, directions.end());
    EXPECT_EQ(jumpDirectionIt->tickTimePosition, 0);
    ASSERT_FALSE(jumpDirectionIt->words.empty());
    EXPECT_EQ(jumpDirectionIt->words.front().text, "Segno");
    EXPECT_EQ(jumpDirectionIt->words.front().positionData.horizontalAlignmnet, mx::api::HorizontalAlignment::right);
}

TEST(MusicXmlParts, RepeatsExportEndingBrackets)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("repeats.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    auto endingTypeCount = [&](mx::api::EndingType endingType) {
        size_t result = 0;
        for (const auto& part : actualScore->parts) {
            for (const auto& measure : part.measures) {
                for (const auto& barline : measure.barlines) {
                    if (barline.endingType == endingType) {
                        ++result;
                    }
                }
            }
        }
        return result;
    };

    EXPECT_GE(endingTypeCount(mx::api::EndingType::start), 2u) << "volta starts";
    EXPECT_GE(endingTypeCount(mx::api::EndingType::stop), 2u) << "closed volta stops";

    ASSERT_FALSE(actualScore->parts.empty());
    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 11u);
    const auto findBarline = [](const mx::api::MeasureData& measure, mx::api::HorizontalAlignment location) -> const mx::api::BarlineData* {
        for (const auto& barline : measure.barlines) {
            if (barline.location == location) {
                return &barline;
            }
        }
        return nullptr;
    };

    const auto* firstEndingStart = findBarline(measures.at(3), mx::api::HorizontalAlignment::left);
    ASSERT_NE(firstEndingStart, nullptr);
    EXPECT_EQ(firstEndingStart->endingType, mx::api::EndingType::start);
    EXPECT_EQ(firstEndingStart->endingNumber, 1);

    const auto* firstEndingStop = findBarline(measures.at(5), mx::api::HorizontalAlignment::right);
    ASSERT_NE(firstEndingStop, nullptr);
    EXPECT_EQ(firstEndingStop->endingType, mx::api::EndingType::stop);
    EXPECT_EQ(firstEndingStop->endingNumber, 1);

    const auto* fourthEndingStart = findBarline(measures.at(6), mx::api::HorizontalAlignment::left);
    ASSERT_NE(fourthEndingStart, nullptr);
    EXPECT_EQ(fourthEndingStart->endingType, mx::api::EndingType::start);
    EXPECT_EQ(fourthEndingStart->endingNumber, 4);

    const auto* fourthEndingStop = findBarline(measures.at(10), mx::api::HorizontalAlignment::right);
    ASSERT_NE(fourthEndingStop, nullptr);
    EXPECT_EQ(fourthEndingStop->endingType, mx::api::EndingType::stop);
    EXPECT_EQ(fourthEndingStop->endingNumber, 4);
}

TEST(MusicXmlParts, IndependentKeySignaturesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("keysigs_independent.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/keysigs_independent-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareKeySignatures(*actualScore, *expectedScore);
}

TEST(MusicXmlParts, IndependentTranspositionsMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("transposition.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/transposition-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTranspositions(*actualScore, *expectedScore);
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

TEST(MusicXmlParts, CompositeTimeSignaturesMatchFinaleExceptMultiComponent)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("timesigs_composite.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/timesigs_composite-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareCompositeTimeSignatures(*actualScore, *expectedScore);
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
}
