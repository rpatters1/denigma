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
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "core/denigma.h"
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
    std::vector<std::pair<std::string, std::string>> fractions;
    mx::api::ComplexTimeSymbol symbol{};
    bool isImplicit{};
    mx::api::Bool display{};

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

mx::api::ComplexTimeSymbol complexSymbolFromSimple(mx::api::TimeSignatureSymbol symbol)
{
    switch (symbol) {
    case mx::api::TimeSignatureSymbol::common: return mx::api::ComplexTimeSymbol::common;
    case mx::api::TimeSignatureSymbol::cut: return mx::api::ComplexTimeSymbol::cut;
    case mx::api::TimeSignatureSymbol::unspecified: break;
    }
    return mx::api::ComplexTimeSymbol::unspecified;
}

ComparableTimeSignature createComparableTimeSignature(const mx::api::TimeChoice& timeChoice)
{
    ComparableTimeSignature result;
    result.isImplicit = timeChoice.isImplicit;
    result.display = timeChoice.display;
    if (timeChoice.isSimple()) {
        const auto simple = timeChoice.simple();
        result.fractions.emplace_back(simple.fraction.beats, simple.fraction.beatType);
        result.symbol = complexSymbolFromSimple(simple.symbol);
    } else {
        const auto complex = timeChoice.complex();
        if (complex.isMetered()) {
            const auto metered = complex.metered();
            for (const auto& fraction : metered.fractions) {
                result.fractions.emplace_back(fraction.beats, fraction.beatType);
            }
            result.symbol = metered.symbol;
        }
    }
    return result;
}

ComparableTimeSignature simpleTime(std::string beats, std::string beatType,
    mx::api::ComplexTimeSymbol symbol = mx::api::ComplexTimeSymbol::unspecified,
    mx::api::Bool display = mx::api::Bool::unspecified)
{
    ComparableTimeSignature result;
    result.fractions.emplace_back(std::move(beats), std::move(beatType));
    result.symbol = symbol;
    result.isImplicit = false;
    result.display = display;
    return result;
}

using PerStaff = std::map<int, ComparableTimeSignature>;

struct ExpectedTimeSignatureMeasure
{
    std::optional<ComparableTimeSignature> partWide;
    PerStaff perStaff;
};

void checkTimeSignatureExpectations(const std::vector<mx::api::MeasureData>& measures,
    const std::map<size_t, ExpectedTimeSignatureMeasure>& expectedByMeasure, const char* partLabel)
{
    SCOPED_TRACE(partLabel);
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
        const auto& measure = measures[measureIndex];
        const auto expectedIt = expectedByMeasure.find(measureIndex);
        const auto* expected = expectedIt != expectedByMeasure.end() ? &expectedIt->second : nullptr;

        if (expected && expected->partWide) {
            EXPECT_EQ(createComparableTimeSignature(measure.timeSignature), *expected->partWide);
        } else {
            EXPECT_TRUE(measure.timeSignature.isImplicit) << "unexpected part-wide time signature";
        }

        PerStaff actualPerStaff;
        for (const auto& [staffIndex, staffTime] : measure.staffTimeSignatures) {
            if (!staffTime.isImplicit) { // the mx reader carries implicit per-staff entries forward
                actualPerStaff.emplace(staffIndex, createComparableTimeSignature(staffTime));
            }
        }
        const PerStaff expectedPerStaff = expected ? expected->perStaff : PerStaff{};
        EXPECT_EQ(actualPerStaff, expectedPerStaff);
    }
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
            SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
            const auto& actualMeasure = actualMeasures.at(measureIndex);
            const auto& expectedMeasure = expectedMeasures.at(measureIndex);
            EXPECT_EQ(createComparableTimeSignature(actualMeasure.timeSignature),
                createComparableTimeSignature(expectedMeasure.timeSignature)) << "part-wide time signature";

            ASSERT_EQ(actualMeasure.staffTimeSignatures.size(), expectedMeasure.staffTimeSignatures.size())
                << "per-staff time signature count";
            for (const auto& [staffIndex, expectedStaffTime] : expectedMeasure.staffTimeSignatures) {
                const auto actualStaffTimeIt = actualMeasure.staffTimeSignatures.find(staffIndex);
                ASSERT_NE(actualStaffTimeIt, actualMeasure.staffTimeSignatures.end())
                    << "missing time signature for staff index " << staffIndex;
                EXPECT_EQ(createComparableTimeSignature(actualStaffTimeIt->second),
                    createComparableTimeSignature(expectedStaffTime)) << "staff index " << staffIndex;
            }
        }
    }
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

TEST(MusicXmlParts, CompositeTimeSignaturesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("timesigs_composite.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/timesigs_composite-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTimeSignatures(*actualScore, *expectedScore);

    ASSERT_EQ(actualScore->parts.size(), 1u);
    ASSERT_EQ(expectedScore->parts.size(), 1u);
    EXPECT_EQ(actualScore->parts.front().groups, expectedScore->parts.front().groups);

    const auto& actualFirstMeasure = actualScore->parts.front().measures.front();
    const auto& expectedFirstMeasure = expectedScore->parts.front().measures.front();
    EXPECT_EQ(actualFirstMeasure.measureNumbering, expectedFirstMeasure.measureNumbering);
    EXPECT_EQ(actualFirstMeasure.measureNumberingMultipleRestAlways, expectedFirstMeasure.measureNumberingMultipleRestAlways);
    EXPECT_EQ(actualFirstMeasure.measureNumberingMultipleRestRange, expectedFirstMeasure.measureNumberingMultipleRestRange);
    // Finale emits only-top for this visible staff. Denigma preserves the staff's own number and
    // adds the top-system number, which MusicXML models as also-top.
    EXPECT_EQ(actualFirstMeasure.measureNumberingSystemRelation, mx::api::SystemRelation::alsoTop);
}

TEST(MusicXmlParts, MeasureNumberStaffOverrideUsesScrollViewPositions)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("measnums_staffoverride.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_EQ(actualScore->parts.size(), 4u);

    struct Expectation
    {
        mx::api::MeasureNumbering numbering;
        mx::api::SystemRelation relation;
        mx::api::Bool multipleRestAlways;
        mx::api::Bool multipleRestRange;
    };
    const std::array<Expectation, 4> expected = {{
        { mx::api::MeasureNumbering::none, mx::api::SystemRelation::unspecified, mx::api::Bool::unspecified, mx::api::Bool::unspecified },
        { mx::api::MeasureNumbering::measure, mx::api::SystemRelation::alsoBottom, mx::api::Bool::yes, mx::api::Bool::yes },
        { mx::api::MeasureNumbering::none, mx::api::SystemRelation::unspecified, mx::api::Bool::unspecified, mx::api::Bool::unspecified },
        { mx::api::MeasureNumbering::measure, mx::api::SystemRelation::alsoBottom, mx::api::Bool::yes, mx::api::Bool::yes }
    }};
    for (size_t partIndex = 0; partIndex < expected.size(); ++partIndex) {
        const auto& measure = actualScore->parts[partIndex].measures.front();
        EXPECT_EQ(measure.measureNumbering, expected[partIndex].numbering) << "part " << partIndex + 1;
        EXPECT_EQ(measure.measureNumberingSystemRelation, expected[partIndex].relation) << "part " << partIndex + 1;
        EXPECT_EQ(measure.measureNumberingMultipleRestAlways, expected[partIndex].multipleRestAlways) << "part " << partIndex + 1;
        EXPECT_EQ(measure.measureNumberingMultipleRestRange, expected[partIndex].multipleRestRange) << "part " << partIndex + 1;
    }
}

TEST(MusicXmlParts, PerStaffTimeSignaturesAndVisibility)
{
    setupTestDataPaths();

    // Finale's own export of this fixture splits the independent-time staves into separate
    // parts (it cannot write <time number=>) and drops hidden meter changes entirely, so the
    // -ref file is not directly comparable. The expectations here assert Denigma's per-staff
    // and visibility semantics explicitly.
    const auto outputPath = exportMusicXmlFixture("timesigs_independent.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_EQ(actualScore->parts.size(), 2u);

    constexpr auto common = mx::api::ComplexTimeSymbol::common;
    constexpr auto cut = mx::api::ComplexTimeSymbol::cut;
    constexpr auto hidden = mx::api::Bool::no;
    constexpr auto forcedShown = mx::api::Bool::yes;

    const auto& fluteMeasures = actualScore->parts.at(0).measures;
    const auto& pianoMeasures = actualScore->parts.at(1).measures;
    ASSERT_GE(fluteMeasures.size(), 13u);
    ASSERT_GE(pianoMeasures.size(), 13u);

    // Flute: staff-level "Display Time Signatures in Score" is off, so every emitted time
    // signature is print-object="no", and the measure-13 "always show" does not override it.
    const std::map<size_t, ExpectedTimeSignatureMeasure> expectedFlute = {
        { 0, { simpleTime("4", "4", {}, hidden), {} } },
        { 2, { simpleTime("3", "4", {}, hidden), {} } },
        { 5, { simpleTime("4", "4", common, hidden), {} } },
        { 6, { simpleTime("2", "2", cut, hidden), {} } },
        { 7, { simpleTime("2", "4", {}, hidden), {} } },
        { 8, { simpleTime("5", "8", {}, hidden), {} } },
        { 10, { simpleTime("3", "4", {}, hidden), {} } },
    };
    checkTimeSignatureExpectations(fluteMeasures, expectedFlute, "flute");

    // Piano: staff 2 has an independent time signature. Polymeter at measure 3 goes per-staff,
    // convergence at measure 5 resets with one unnumbered signature, the measure-9 meter change
    // is set to never show, measure 10 changes only the independent staff while a staff style
    // hides it, and measure 13 forces a restatement of the unchanged meter.
    const std::map<size_t, ExpectedTimeSignatureMeasure> expectedPiano = {
        { 0, { simpleTime("4", "4"), {} } },
        { 2, { std::nullopt, PerStaff{
            { 0, simpleTime("3", "4") },
            { 1, simpleTime("6", "8") } } } },
        { 4, { simpleTime("3", "4"), {} } },
        { 5, { simpleTime("4", "4", common), {} } },
        { 6, { simpleTime("2", "2", cut), {} } },
        { 7, { simpleTime("2", "4"), {} } },
        { 8, { simpleTime("5", "8", {}, hidden), {} } },
        { 9, { std::nullopt, PerStaff{ { 1, simpleTime("7", "8", {}, hidden) } } } },
        { 10, { simpleTime("3", "4"), {} } },
        { 12, { simpleTime("3", "4", {}, forcedShown), {} } },
    };
    checkTimeSignatureExpectations(pianoMeasures, expectedPiano, "piano");
}

TEST(MusicXmlParts, TimeSignaturesVisibleInLinkedPart)
{
    setupTestDataPaths();

    // The flute staff hides time signatures in the score but not in parts, so the linked part
    // must show them. The measure-9 "never show" still hides, and the measure-13 "always show"
    // now restates, because staff-level hiding no longer suppresses it in the part.
    std::filesystem::path inputPath;
    copyInputToOutput("timesigs_independent.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml", "--part", "Flute", "--force" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export flute part: " << pathString(inputPath);
    });

    const auto outputPath = inputPath.parent_path() / "timesigs_independent.Flute.musicxml";
    ASSERT_TRUE(std::filesystem::exists(outputPath)) << "Missing MusicXML output " << pathString(outputPath);
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_EQ(actualScore->parts.size(), 1u);
    EXPECT_EQ(actualScore->parts.front().groups, std::vector<std::string>{ "part" });

    const auto& fluteMeasures = actualScore->parts.at(0).measures;
    ASSERT_GE(fluteMeasures.size(), 13u);

    const std::map<size_t, ExpectedTimeSignatureMeasure> expectedFlute = {
        { 0, { simpleTime("4", "4"), {} } },
        { 2, { simpleTime("3", "4"), {} } },
        { 5, { simpleTime("4", "4", mx::api::ComplexTimeSymbol::common), {} } },
        { 6, { simpleTime("2", "2", mx::api::ComplexTimeSymbol::cut), {} } },
        { 7, { simpleTime("2", "4"), {} } },
        { 8, { simpleTime("5", "8", {}, mx::api::Bool::no), {} } },
        { 10, { simpleTime("3", "4"), {} } },
        { 12, { simpleTime("3", "4", {}, mx::api::Bool::yes), {} } },
    };
    checkTimeSignatureExpectations(fluteMeasures, expectedFlute, "flute part");
}

TEST(MusicXmlParts, VoicedPartsApplyVoicingToLinkedParts)
{
    setupTestDataPaths();

    std::filesystem::path inputPath;
    copyInputToOutput("voiced_parts.musx", inputPath);

    // Per measure: the sounding note groups (chords or single notes) after part voicing is
    // applied; rests are skipped, so an empty measure means voicing excluded every entry.
    // The expected values mirror musxdom's PartVoicing.EntryDetection ground truth.
    struct VoicedPartExpectation
    {
        std::string partName;
        std::vector<std::vector<std::vector<std::string>>> measures;
    };
    const std::vector<VoicedPartExpectation> expectations = {
        { "Layer 1", { {{"C5"}}, {{"C5"}}, {{"C5"}}, {{"A4", "C5"}}, {{"A4"}, {"C5"}} } },
        { "Layer 2", { {{"F4"}}, {{"F4"}}, {{"F4"}}, {{"F4"}}, {{"A4"}} } },
        { "Layer 3", { {{"A4"}}, {{"A4"}}, {{"A4"}}, {}, {{"A4"}, {"A4"}} } },
        { "Layer 2 Only", { {{"F4"}}, {{"F4", "A4", "C5"}}, {}, {{"F4"}}, {} } },
    };

    const auto pitchName = [](const mx::api::PitchData& pitch) {
        constexpr std::array<char, 7> stepLetters = { 'C', 'D', 'E', 'F', 'G', 'A', 'B' };
        std::string result(1, stepLetters.at(static_cast<size_t>(pitch.step)));
        if (pitch.alter > 0) {
            result += std::string(static_cast<size_t>(pitch.alter), '#');
        } else if (pitch.alter < 0) {
            result += std::string(static_cast<size_t>(-pitch.alter), 'b');
        }
        result += std::to_string(pitch.octave);
        return result;
    };

    for (const auto& expectation : expectations) {
        SCOPED_TRACE(expectation.partName);
        ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml", "--part", expectation.partName, "--force" };
        checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export part " << expectation.partName;
        });

        const auto outputPath = inputPath.parent_path() / ("voiced_parts." + expectation.partName + ".musicxml");
        ASSERT_TRUE(std::filesystem::exists(outputPath)) << "Missing MusicXML output " << pathString(outputPath);
        const auto actualScore = loadScoreData(outputPath);
        ASSERT_TRUE(actualScore);
        ASSERT_EQ(actualScore->parts.size(), 1u);

        const auto& measures = actualScore->parts.at(0).measures;
        ASSERT_EQ(measures.size(), expectation.measures.size());
        for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
            SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
            std::vector<std::vector<std::string>> actualGroups;
            for (const auto& staff : measures[measureIndex].staves) {
                for (const auto& voice : staff.voices) {
                    std::optional<int> lastGroupTick;
                    for (const auto& note : voice.second.notes) {
                        if (note.isRest) {
                            lastGroupTick = std::nullopt;
                            continue;
                        }
                        if (note.isChord && lastGroupTick == note.tickTimePosition && !actualGroups.empty()) {
                            actualGroups.back().emplace_back(pitchName(note.pitchData));
                        } else {
                            actualGroups.emplace_back(std::vector<std::string>{ pitchName(note.pitchData) });
                            lastGroupTick = note.tickTimePosition;
                        }
                    }
                }
            }
            EXPECT_EQ(actualGroups, expectation.measures[measureIndex]);
        }
    }
}

TEST(MusicXmlParts, IndependentTimeSignaturesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("indtime_clefchange.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/indtime_clefchange-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTimeSignatures(*actualScore, *expectedScore);
}
