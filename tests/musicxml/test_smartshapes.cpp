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
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "mx/api/CurveData.h"
#include "mx/api/NoteData.h"
#include "mx/api/ScoreData.h"
#include "pugixml.hpp"
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
                                appendEvent("start", start.number.level(), start.curveOrientation, start.lineData.lineType);
                            }
                        }
                        for (const auto& stop : note.noteAttachmentData.curveStops) {
                            if (stop.curveType == mx::api::CurveType::slur) {
                                appendEvent("stop", stop.number.level(), mx::api::CurveOrientation::unspecified,
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

TEST(MusicXmlSmartShapes, SlursOverbarsMatchReference)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_overbars.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore.has_value());

    const auto referenceScore = loadScoreData(std::filesystem::path("inputs") / "musicxml" / "slurs_overbars-ref.musicxml");
    ASSERT_TRUE(referenceScore.has_value());

    EXPECT_EQ(createComparableSlurEvents(*referenceScore), createComparableSlurEvents(*actualScore));
}

/// MusicXML's number-level contract: two slurs of the same part that overlap in
/// document order must carry distinct numbers. Readers pair slur starts and stops
/// by number within the part, so a collision mispairs them (typically visible as
/// slurs jumping between the staves of a multistaff instrument). The fixture's
/// staff-1 slur (m5-m6) is open when the staff-2 slur (m6) starts, so they must
/// take different numbers. Requires mx's part-scoped spanner number pools
/// (mx PR for issue #350; see mx-api-gaps.md).
TEST(MusicXmlSmartShapes, OverlappingSlursAcrossStavesCarryDistinctNumbers)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("cross_staffs.musx");

    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(outputPath.c_str()));

    // A slur with no number attribute defaults to 1 per the MusicXML spec.
    constexpr int kDefaultSlurNumber = 1;
    bool sawDocumentOrderOverlap = false;
    for (auto part = document.child("score-partwise").child("part"); part; part = part.next_sibling("part")) {
        std::set<int> openNumbers;
        for (auto measure = part.child("measure"); measure; measure = measure.next_sibling("measure")) {
            for (auto note = measure.child("note"); note; note = note.next_sibling("note")) {
                for (auto notations = note.child("notations"); notations; notations = notations.next_sibling("notations")) {
                    for (auto slur = notations.child("slur"); slur; slur = slur.next_sibling("slur")) {
                        const int number = slur.attribute("number").as_int(kDefaultSlurNumber);
                        const std::string type = slur.attribute("type").as_string();
                        if (type == "start") {
                            if (!openNumbers.empty()) {
                                sawDocumentOrderOverlap = true;
                            }
                            EXPECT_TRUE(openNumbers.insert(number).second)
                                << "slur number " << number << " started in measure "
                                << measure.attribute("number").as_string()
                                << " while a slur with the same number was still open";
                        } else if (type == "stop") {
                            openNumbers.erase(number);
                        }
                    }
                }
            }
        }
    }
    EXPECT_TRUE(sawDocumentOrderOverlap)
        << "fixture contains no slurs that overlap in document order, so the number-level "
           "contract was not exercised";
}

/// Beat-attached slurs whose endpoints coincide with no entry anchor to synthesized
/// hidden rests at the endpoint tick (in the reserved anchor voice), because MusicXML
/// curve notations can only hang off a <note>. Short same-position hooks that coincide
/// with an entry classify as pseudo lv ties and export as <tied type="let-ring">.
TEST(MusicXmlSmartShapes, BeatAttachedSlursAnchorToHiddenRests)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("slurs_beatattached.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_GE(score->parts.size(), 2u);

    struct SlurEndpointCounts
    {
        int onHiddenRestAnchors{};
        int onVisibleNotes{};
        int onHiddenNotes{};
        int letRingTies{};
    };
    const auto countPart = [](const mx::api::PartData& part) {
        SlurEndpointCounts counts;
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& voicePair : staff.voices) {
                    for (const auto& note : voicePair.second.notes) {
                        if (note.tieLetRing) {
                            ++counts.letRingTies;
                        }
                        int slurEndpoints = 0;
                        for (const auto& start : note.noteAttachmentData.curveStarts) {
                            slurEndpoints += start.curveType == mx::api::CurveType::slur ? 1 : 0;
                        }
                        for (const auto& stop : note.noteAttachmentData.curveStops) {
                            slurEndpoints += stop.curveType == mx::api::CurveType::slur ? 1 : 0;
                        }
                        if (slurEndpoints == 0) {
                            continue;
                        }
                        if (note.isRest && note.printData.printObject == mx::api::Bool::no) {
                            counts.onHiddenRestAnchors += slurEndpoints;
                        } else if (note.printData.printObject == mx::api::Bool::no) {
                            counts.onHiddenNotes += slurEndpoints;
                        } else {
                            counts.onVisibleNotes += slurEndpoints;
                        }
                    }
                }
            }
        }
        return counts;
    };

    // Part 1: three chained slur pairs across empty measures (m1-m2, m2-m3, m3-m4),
    // stacked three high, all floating; plus three same-position hooks on the m1
    // notes that classify as lv ties.
    const auto part1 = countPart(score->parts.at(0));
    EXPECT_EQ(part1.onHiddenRestAnchors, 18);
    EXPECT_EQ(part1.onVisibleNotes, 0);
    EXPECT_EQ(part1.onHiddenNotes, 0);
    EXPECT_EQ(part1.letRingTies, 3);

    // Part 2: one slur attached to hidden entries in m1 (hidden entries export with
    // print-object="no" and remain valid attachment points), one real note-attached
    // slur in m2, one floating slur inside m2, and two floating slurs spanning m3-m4.
    const auto part2 = countPart(score->parts.at(1));
    EXPECT_EQ(part2.onHiddenRestAnchors, 6);
    EXPECT_EQ(part2.onVisibleNotes, 2);
    EXPECT_EQ(part2.onHiddenNotes, 2);
    EXPECT_EQ(part2.letRingTies, 0);
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

namespace {

size_t countOttavaStarts(const mx::api::StaffData& staff)
{
    size_t count = 0;
    for (const auto& direction : staff.directions) {
        count += direction.ottavaStarts.size();
    }
    return count;
}

const mx::api::OttavaStart* firstOttavaStart(const mx::api::StaffData& staff)
{
    for (const auto& direction : staff.directions) {
        if (!direction.ottavaStarts.empty()) {
            return &direction.ottavaStarts.front();
        }
    }
    return nullptr;
}

const mx::api::OttavaStop* firstOttavaStop(const mx::api::StaffData& staff)
{
    for (const auto& direction : staff.directions) {
        if (!direction.ottavaStops.empty()) {
            return &direction.ottavaStops.front();
        }
    }
    return nullptr;
}

const mx::api::SpannerStart* firstBracketStart(const mx::api::StaffData& staff)
{
    for (const auto& direction : staff.directions) {
        if (!direction.bracketStarts.empty()) {
            return &direction.bracketStarts.front();
        }
    }
    return nullptr;
}

const mx::api::SpannerStop* firstBracketStop(const mx::api::StaffData& staff)
{
    for (const auto& direction : staff.directions) {
        if (!direction.bracketStops.empty()) {
            return &direction.bracketStops.front();
        }
    }
    return nullptr;
}

std::vector<int> noteOctaves(const mx::api::StaffData& staff)
{
    std::vector<int> result;
    const auto voiceIt = staff.voices.find(0);
    if (voiceIt == staff.voices.end()) {
        return result;
    }
    for (const auto& note : voiceIt->second.notes) {
        result.push_back(note.pitchData.octave);
    }
    return result;
}

} // namespace

TEST(MusicXmlSmartShapes, SmartShapeLinesOttavaCarriers)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("smartshape_lines.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_GE(score->parts.size(), 3u);

    const auto& staff1 = score->parts.at(0);
    ASSERT_GE(staff1.measures.size(), 28u);

    // The hidden quindicesima (m14-m23) is the semantic carrier; its two visual
    // segments (m14-m15 and m16-m18) must not emit additional octave-shifts.
    {
        const auto& m14 = staff1.measures.at(13).staves.at(0);
        ASSERT_EQ(countOttavaStarts(m14), 1u);
        const auto* start = firstOttavaStart(m14);
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(m14.directions.front().placement, mx::api::Placement::above);
        EXPECT_EQ(start->ottavaType, mx::api::OttavaType::o15ma);
        EXPECT_EQ(countOttavaStarts(staff1.measures.at(14).staves.at(0)), 0u);
        EXPECT_EQ(countOttavaStarts(staff1.measures.at(15).staves.at(0)), 0u);
        const auto* stop = firstOttavaStop(staff1.measures.at(22).staves.at(0));
        ASSERT_NE(stop, nullptr);
        ASSERT_TRUE(stop->size.has_value());
        EXPECT_EQ(*stop->size, 15);
        EXPECT_EQ(noteOctaves(m14), (std::vector<int>{ 6, 6, 6, 7 }));
    }

    // The unpaired "8vb" line inside the quindicesima is its own carrier; where the
    // two overlap, the displacements sum.
    {
        const auto* start = firstOttavaStart(staff1.measures.at(18).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->ottavaType, mx::api::OttavaType::o8vb);
        EXPECT_EQ(noteOctaves(staff1.measures.at(18).staves.at(0)), (std::vector<int>{ 6, 6, 5, 6 }));
        EXPECT_EQ(noteOctaves(staff1.measures.at(19).staves.at(0)), (std::vector<int>{ 5, 5, 5, 6 }));
    }

    // The canonical pair: the hidden octaveUp emits despite hidden; its visible
    // custom line emits nothing.
    {
        const auto& m26 = staff1.measures.at(25).staves.at(0);
        ASSERT_EQ(countOttavaStarts(m26), 1u);
        const auto* start = firstOttavaStart(m26);
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->ottavaType, mx::api::OttavaType::o8va);
        const auto* stop = firstOttavaStop(staff1.measures.at(27).staves.at(0));
        ASSERT_NE(stop, nullptr);
        ASSERT_TRUE(stop->size.has_value());
        EXPECT_EQ(*stop->size, 8);
        EXPECT_EQ(noteOctaves(m26), (std::vector<int>{ 5, 5, 5, 6 }));
        EXPECT_EQ(noteOctaves(staff1.measures.at(27).staves.at(0)), (std::vector<int>{ 5, 5, 4, 5 }));
    }

    // The unpaired "8vb" line on staff 3 is a carrier.
    {
        const auto& staff3 = score->parts.at(2);
        ASSERT_GE(staff3.measures.size(), 23u);
        const auto* start = firstOttavaStart(staff3.measures.at(20).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->ottavaType, mx::api::OttavaType::o8vb);
        // Staff 3 is written A4 B4 C5 D5 per measure; m22 sounds an octave lower.
        EXPECT_EQ(noteOctaves(staff3.measures.at(21).staves.at(0)), (std::vector<int>{ 3, 3, 4, 4 }));
    }
}

TEST(MusicXmlSmartShapes, SmartShapeLinesOttavaNumberLevels)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("smartshape_lines.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_GE(score->parts.size(), 3u);

    const auto expectNumberLevels = [](const mx::api::StaffData& startStaff, const mx::api::StaffData& stopStaff,
                                       int expectedNumberLevel, const char* which) {
        const auto* start = firstOttavaStart(startStaff);
        ASSERT_NE(start, nullptr) << which;
        EXPECT_EQ(start->spannerStart.number.level(), expectedNumberLevel) << which << " start";
        const auto* stop = firstOttavaStop(stopStaff);
        ASSERT_NE(stop, nullptr) << which;
        EXPECT_EQ(stop->spannerStop.number.level(), expectedNumberLevel) << which << " stop";
    };

    const auto& staff1 = score->parts.at(0);
    ASSERT_GE(staff1.measures.size(), 28u);
    const auto staff1At = [&](size_t measureIndex) -> const mx::api::StaffData& {
        return staff1.measures.at(measureIndex).staves.at(0);
    };
    expectNumberLevels(staff1At(13), staff1At(22), 1, "hidden quindicesima m14-m23");
    expectNumberLevels(staff1At(18), staff1At(19), 2, "8vb line m19-m20");
    // The suppressed visual proxy of this pair emits nothing, so it holds no number
    // and the writer-side assigner reuses 1 once the quindicesima has closed.
    expectNumberLevels(staff1At(25), staff1At(27), 1, "paired ottava m26-m28");

    const auto& staff3 = score->parts.at(2);
    ASSERT_GE(staff3.measures.size(), 23u);
    expectNumberLevels(staff3.measures.at(20).staves.at(0), staff3.measures.at(22).staves.at(0), 1,
                       "8vb line staff 3 m21-m23");
}

TEST(MusicXmlSmartShapes, SmartShapeLinesGeneralLineBrackets)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("smartshape_lines.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_GE(score->parts.size(), 5u);

    // Built-in lines take their hook length from the document's smart shape options
    // (20 Evpu in this fixture). The comparison tolerance absorbs the decimal
    // precision of the written XML attribute.
    const double expectedHookTenths = 20.0 * (40.0 / 96.0);
    constexpr double kEndLengthTolerance = 0.0001;

    // Built-in solidLineDown2 on staff 1, m1-m2: solid bracket with down hooks at
    // both ends.
    {
        const auto& staff1 = score->parts.at(0);
        const auto* start = firstBracketStart(staff1.measures.at(0).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->lineData.lineType, mx::api::LineType::solid);
        EXPECT_EQ(start->lineData.lineHook, mx::api::LineHook::down);
        ASSERT_TRUE(start->lineData.isStopLengthSpecified);
        EXPECT_NEAR(start->lineData.endLength, expectedHookTenths, kEndLengthTolerance);
        const auto* stop = firstBracketStop(staff1.measures.at(1).staves.at(0));
        ASSERT_NE(stop, nullptr);
        EXPECT_EQ(stop->lineData.lineHook, mx::api::LineHook::down);
        ASSERT_TRUE(stop->lineData.isStopLengthSpecified);
        EXPECT_NEAR(stop->lineData.endLength, expectedHookTenths, kEndLengthTolerance);
    }

    // Custom line with an end hook on staff 2, m1-m2: no start hook, up hook at the
    // end with its length expressed in tenths (1536 Efix = 24 Evpu = 10 tenths).
    {
        const auto& staff2 = score->parts.at(1);
        const auto* start = firstBracketStart(staff2.measures.at(0).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->lineData.lineType, mx::api::LineType::solid);
        EXPECT_EQ(start->lineData.lineHook, mx::api::LineHook::none);
        const auto* stop = firstBracketStop(staff2.measures.at(1).staves.at(0));
        ASSERT_NE(stop, nullptr);
        EXPECT_EQ(stop->lineData.lineHook, mx::api::LineHook::up);
        ASSERT_TRUE(stop->lineData.isStopLengthSpecified);
        EXPECT_DOUBLE_EQ(stop->lineData.endLength, 10.0);
    }

    // Built-in dashLineDown2 on staff 4, m1-m3: dashed bracket with dash lengths from
    // the document's smart shape options (18 Evpu = 7.5 tenths).
    {
        const auto& staff4 = score->parts.at(3);
        const auto* start = firstBracketStart(staff4.measures.at(0).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->lineData.lineType, mx::api::LineType::dashed);
        EXPECT_EQ(start->lineData.lineHook, mx::api::LineHook::down);
        ASSERT_TRUE(start->lineData.isStopLengthSpecified);
        EXPECT_NEAR(start->lineData.endLength, expectedHookTenths, kEndLengthTolerance);
        ASSERT_TRUE(start->lineData.isDashLengthSpecified);
        EXPECT_DOUBLE_EQ(start->lineData.dashLength, 7.5);
    }

    // Neutral wiggle char line on staff 5, m9-m11: wavy bracket with no hooks.
    {
        const auto& staff5 = score->parts.at(4);
        const auto* start = firstBracketStart(staff5.measures.at(8).staves.at(0));
        ASSERT_NE(start, nullptr);
        EXPECT_EQ(start->lineData.lineType, mx::api::LineType::wavy);
        EXPECT_EQ(start->lineData.lineHook, mx::api::LineHook::none);
    }
}

TEST(MusicXmlSmartShapes, SmartShapeLinesTrillMark)
{
    setupTestDataPaths();
    const auto outputPath = exportMusicXmlFixture("smartshape_lines.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_GE(score->parts.size(), 3u);

    // Built-in trill on staff 3 starting at beat 2 of m11 emits a trill-mark on the
    // associated note.
    const auto& staff3 = score->parts.at(2);
    ASSERT_GE(staff3.measures.size(), 11u);
    const auto& m11 = staff3.measures.at(10).staves.at(0);
    const auto voiceIt = m11.voices.find(0);
    ASSERT_NE(voiceIt, m11.voices.end());
    ASSERT_GE(voiceIt->second.notes.size(), 2u);
    const auto& note = voiceIt->second.notes.at(1);
    const auto trillIt = std::ranges::find_if(note.noteAttachmentData.marks, [](const mx::api::MarkData& mark) {
        return mark.markType == mx::api::MarkType::trillMark;
    });
    EXPECT_NE(trillIt, note.noteAttachmentData.marks.end());

    // The built-in trill extension (staff 3, m3-m6) is omitted: mx::api cannot pair
    // wavy-line start/stop. (See mx-api-gaps.md.)
    const auto& m3 = staff3.measures.at(2).staves.at(0);
    const auto m3VoiceIt = m3.voices.find(0);
    if (m3VoiceIt != m3.voices.end()) {
        for (const auto& m3Note : m3VoiceIt->second.notes) {
            for (const auto& mark : m3Note.noteAttachmentData.marks) {
                EXPECT_NE(mark.markType, mx::api::MarkType::wavyLine);
            }
        }
    }
}
