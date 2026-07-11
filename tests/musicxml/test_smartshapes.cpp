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

#include "gtest/gtest.h"
#include "musicxml_test.h"

#include <filesystem>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "mx/api/CurveData.h"
#include "mx/api/NoteData.h"
#include "mx/api/ScoreData.h"
#include "test_utils.h"

using namespace denigma::test::musicxml;

namespace {

struct ComparableSlurEvent
{
    std::string endpoint;
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    size_t noteIndex{};
    bool isChord{};
    mx::api::Step step{};
    int alter{};
    int octave{};
    int numberLevel{};
    mx::api::CurveOrientation curveOrientation{};
    mx::api::LineType lineType{};

    bool operator==(const ComparableSlurEvent&) const = default;
};

std::ostream& operator<<(std::ostream& os, const ComparableSlurEvent& event)
{
    return os << event.endpoint
              << " part=" << event.partIndex
              << " measure=" << event.measureIndex
              << " staff=" << event.staffIndex
              << " note=" << event.noteIndex
              << " chord=" << event.isChord
              << " step=" << static_cast<int>(event.step)
              << " alter=" << event.alter
              << " octave=" << event.octave
              << " number=" << event.numberLevel
              << " orientation=" << static_cast<int>(event.curveOrientation)
              << " lineType=" << static_cast<int>(event.lineType);
}

std::vector<ComparableSlurEvent> createComparableSlurEvents(const mx::api::ScoreData& score)
{
    std::vector<ComparableSlurEvent> result;
    std::unordered_map<int, int> normalizedNumberLevels;
    int nextNumberLevel = 1;
    const auto normalizeNumberLevel = [&](int numberLevel) {
        if (numberLevel <= 0) {
            return numberLevel;
        }
        const auto [numberIt, inserted] = normalizedNumberLevels.emplace(numberLevel, nextNumberLevel);
        if (inserted) {
            ++nextNumberLevel;
        }
        return numberIt->second;
    };

    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        const auto& part = score.parts.at(partIndex);
        for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
            const auto& measure = part.measures.at(measureIndex);
            for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
                const auto& staff = measure.staves.at(staffIndex);
                for (const auto& voicePair : staff.voices) {
                    const auto& voice = voicePair.second;
                    for (size_t noteIndex = 0; noteIndex < voice.notes.size(); ++noteIndex) {
                        const auto& note = voice.notes.at(noteIndex);
                        const auto appendEvent = [&](std::string endpoint, int numberLevel, mx::api::CurveOrientation curveOrientation,
                                                     mx::api::LineType lineType) {
                            result.push_back({
                                std::move(endpoint),
                                partIndex,
                                measureIndex,
                                staffIndex,
                                noteIndex,
                                note.isChord,
                                note.isRest ? mx::api::Step::unspecified : note.pitchData.step,
                                note.isRest ? 0 : note.pitchData.alter,
                                note.isRest ? 0 : note.pitchData.octave,
                                normalizeNumberLevel(numberLevel),
                                curveOrientation,
                                lineType
                            });
                        };

                        for (const auto& start : note.noteAttachmentData.curveStarts) {
                            if (start.curveType == mx::api::CurveType::slur) {
                                appendEvent("start", start.numberLevel, start.curveOrientation, start.lineData.lineType);
                            }
                        }
                        for (const auto& stop : note.noteAttachmentData.curveStops) {
                            if (stop.curveType == mx::api::CurveType::slur) {
                                appendEvent("stop", stop.numberLevel, mx::api::CurveOrientation::unspecified,
                                            mx::api::LineType::unspecified);
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

} // namespace

TEST(MusicXmlSmartShapes, SlursTwoStavesExportsSmoke)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_2staves.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
}

TEST(MusicXmlSmartShapes, SlursTwoStavesMatchReference)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_2staves.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore.has_value());

    const auto referenceScore = loadScoreData(std::filesystem::path("inputs") / "musicxml" / "slurs_2staves-ref.musicxml");
    ASSERT_TRUE(referenceScore.has_value());

    EXPECT_EQ(createComparableSlurEvents(*referenceScore), createComparableSlurEvents(*actualScore));
}

TEST(MusicXmlSmartShapes, SlursTwoVoicesMatchReference)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_2voices.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore.has_value());

    const auto referenceScore = loadScoreData(std::filesystem::path("inputs") / "musicxml" / "slurs_2voices-ref.musicxml");
    ASSERT_TRUE(referenceScore.has_value());

    EXPECT_EQ(createComparableSlurEvents(*referenceScore), createComparableSlurEvents(*actualScore));
}

TEST(MusicXmlSmartShapes, DISABLED_SlursOverbarsMatchReference)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_overbars.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore.has_value());

    const auto referenceScore = loadScoreData(std::filesystem::path("inputs") / "musicxml" / "slurs_overbars-ref.musicxml");
    ASSERT_TRUE(referenceScore.has_value());

    EXPECT_EQ(createComparableSlurEvents(*referenceScore), createComparableSlurEvents(*actualScore));
}

TEST(MusicXmlSmartShapes, OverlappingOttavas)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("ottavas.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_FALSE(score->parts.empty());

    const auto& part = score->parts.at(0);
    ASSERT_GE(part.measures.size(), 2);
    ASSERT_FALSE(part.measures.at(0).staves.empty());
    ASSERT_FALSE(part.measures.at(1).staves.empty());

    const auto& measure1Staff = part.measures.at(0).staves.at(0);
    ASSERT_GE(measure1Staff.directions.size(), 2);
    ASSERT_EQ(measure1Staff.directions.at(0).ottavaStarts.size(), 1);
    EXPECT_EQ(measure1Staff.directions.at(0).placement, mx::api::Placement::above);
    EXPECT_EQ(measure1Staff.directions.at(0).ottavaStarts.front().ottavaType, mx::api::OttavaType::o15ma);
    ASSERT_EQ(measure1Staff.directions.at(1).ottavaStarts.size(), 1);
    EXPECT_EQ(measure1Staff.directions.at(1).placement, mx::api::Placement::below);
    EXPECT_EQ(measure1Staff.directions.at(1).ottavaStarts.front().ottavaType, mx::api::OttavaType::o8vb);

    const auto& measure2Staff = part.measures.at(1).staves.at(0);
    ASSERT_GE(measure2Staff.directions.size(), 2);
    ASSERT_EQ(measure2Staff.directions.at(0).ottavaStops.size(), 1);
    ASSERT_EQ(measure2Staff.directions.at(1).ottavaStops.size(), 1);

    const auto& voice2 = measure2Staff.voices.at(0);
    ASSERT_GE(voice2.notes.size(), 3);
    const auto secondMeasure2NoteEnd = voice2.notes.at(1).tickTimePosition + voice2.notes.at(1).durationData.durationTimeTicks;
    EXPECT_EQ(measure2Staff.directions.at(0).tickTimePosition, secondMeasure2NoteEnd);
    EXPECT_EQ(measure2Staff.directions.at(1).tickTimePosition, secondMeasure2NoteEnd);

    const auto& voice1 = measure1Staff.voices.at(0);
    ASSERT_GE(voice1.notes.size(), 4);
    EXPECT_EQ(voice1.notes.at(0).pitchData.octave, 7);
    EXPECT_EQ(voice1.notes.at(1).pitchData.octave, 7);
    EXPECT_EQ(voice1.notes.at(2).pitchData.octave, 6);
    EXPECT_EQ(voice1.notes.at(3).pitchData.octave, 6);
    EXPECT_EQ(voice2.notes.at(0).pitchData.octave, 6);
    EXPECT_EQ(voice2.notes.at(1).pitchData.octave, 6);
    EXPECT_EQ(voice2.notes.at(2).pitchData.octave, 5);
}

TEST(MusicXmlSmartShapes, OttavaEndOfBar)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("ottava_end_of_bar.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_FALSE(score->parts.empty());

    const auto& part = score->parts.at(0);
    ASSERT_GE(part.measures.size(), 2);
    ASSERT_FALSE(part.measures.at(0).staves.empty());
    ASSERT_FALSE(part.measures.at(1).staves.empty());

    const auto& measure1Staff = part.measures.at(0).staves.at(0);
    ASSERT_FALSE(measure1Staff.directions.empty());
    ASSERT_EQ(measure1Staff.directions.back().ottavaStarts.size(), 1);
    EXPECT_EQ(measure1Staff.directions.back().ottavaStarts.front().ottavaType, mx::api::OttavaType::o8va);

    const auto& measure2Staff = part.measures.at(1).staves.at(0);
    ASSERT_FALSE(measure2Staff.directions.empty());
    ASSERT_EQ(measure2Staff.directions.front().ottavaStops.size(), 1);
    const auto& voice = measure2Staff.voices.at(0);
    ASSERT_FALSE(voice.notes.empty());
    const auto firstNoteEnd = voice.notes.front().tickTimePosition + voice.notes.front().durationData.durationTimeTicks;
    EXPECT_EQ(measure2Staff.directions.front().tickTimePosition, firstNoteEnd);
}

static void expectOttava(const mx::api::PartData& part, size_t startMeasureIdx, size_t startNoteIdx, mx::api::OttavaType ottavaType,
    mx::api::Placement placement, size_t endMeasureIdx, size_t endNoteIdx, int expectedStopSize)
{
    {
        const auto& staff = part.measures.at(startMeasureIdx).staves.at(0);
        ASSERT_FALSE(staff.directions.empty());
        ASSERT_EQ(staff.directions.front().ottavaStarts.size(), 1);
        EXPECT_EQ(staff.directions.front().placement, placement);
        EXPECT_EQ(staff.directions.front().ottavaStarts.front().ottavaType, ottavaType);
        const auto& voice = staff.voices.at(0);
        ASSERT_GT(voice.notes.size(), startNoteIdx);        
        const auto expectedStartTick = voice.notes.at(startNoteIdx).tickTimePosition;
        EXPECT_EQ(staff.directions.front().tickTimePosition, expectedStartTick);
    }

    {
        const auto& staff = part.measures.at(endMeasureIdx).staves.at(0);
        ASSERT_FALSE(staff.directions.empty());
        ASSERT_FALSE(staff.directions.back().ottavaStops.empty());
        ASSERT_TRUE(staff.directions.back().ottavaStops.front().size.has_value());
        EXPECT_EQ(*staff.directions.back().ottavaStops.front().size, expectedStopSize);
        EXPECT_EQ(staff.directions.back().placement, placement);
        const auto& voice = staff.voices.at(0);
        ASSERT_GT(voice.notes.size(), endNoteIdx);
        const auto expectedStopTick = voice.notes.at(endNoteIdx).tickTimePosition
            + voice.notes.at(endNoteIdx).durationData.durationTimeTicks;
        EXPECT_EQ(staff.directions.back().tickTimePosition, expectedStopTick);
    }
};

TEST(MusicXmlSmartShapes, OttavasSimpleMatchesExpectedOttavas)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("ottavas_simple.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_FALSE(score->parts.empty());

    const auto& part = score->parts.at(0);
    ASSERT_GE(part.measures.size(), 4);

    expectOttava(part, 0, 0, mx::api::OttavaType::o8va, mx::api::Placement::above, 0, 2, 8);
    expectOttava(part, 1, 0, mx::api::OttavaType::o8vb, mx::api::Placement::below, 1, 2, 8);
    expectOttava(part, 2, 0, mx::api::OttavaType::o15ma, mx::api::Placement::above, 2, 1, 15);
    expectOttava(part, 3, 0, mx::api::OttavaType::o15mb, mx::api::Placement::below, 3, 1, 15);
}

TEST(MusicXmlSmartShapes, OttavasEdgeMatchesExpectedOttavas)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("ottavas_edge.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_FALSE(score->parts.empty());

    const auto& part = score->parts.at(0);
    ASSERT_GE(part.measures.size(), 4);

    expectOttava(part, 0, 1, mx::api::OttavaType::o8va, mx::api::Placement::above, 1, 0, 8);
    expectOttava(part, 2, 1, mx::api::OttavaType::o8va, mx::api::Placement::above, 2, 2, 8);
    expectOttava(part, 4, 0, mx::api::OttavaType::o8va, mx::api::Placement::above, 4, 3, 8);
    expectOttava(part, 5, 1, mx::api::OttavaType::o15mb, mx::api::Placement::below, 5, 4, 15);
}

TEST(MusicXmlSmartShapes, BlankLinePedalExportsAsSignOnlyMarks)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("pedal_custom_lines.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());

    bool foundPedalStart = false;
    bool foundPedalStop = false;
    for (const auto& part : score->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& direction : staff.directions) {
                    for (const auto& mark : direction.marks) {
                        foundPedalStart = foundPedalStart || mark.markType == mx::api::MarkType::pedal;
                        foundPedalStop = foundPedalStop || mark.markType == mx::api::MarkType::damp;
                    }
                }
            }
        }
    }
    EXPECT_TRUE(foundPedalStart);
    EXPECT_TRUE(foundPedalStop);
}
