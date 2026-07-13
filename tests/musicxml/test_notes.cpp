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
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "formats/musicxml/musicxml.h"
#include "musx/util/Fraction.h"
#include "mx/api/ScoreData.h"
#include "mx/api/MarkDataChoice.h"
#include "pugixml.hpp"
#include "musicxml_test.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

struct ComparableNoteEvent
{
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    int voiceIndex{};
    musx::util::Fraction tickTimePosition;
    musx::util::Fraction durationTime;
    bool isRest{};
    bool isMeasureRest{};
    bool isChord{};
    bool isTieStart{};
    bool isTieStop{};
    mx::api::Step step{};
    int alter{};
    int octave{};

    bool operator==(const ComparableNoteEvent&) const = default;
};

struct ComparableStemEvent
{
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    musx::util::Fraction tickTimePosition;
    musx::util::Fraction durationTime;
    bool isChord{};
    mx::api::Step step{};
    int alter{};
    int octave{};
    mx::api::Stem stem{mx::api::Stem::unspecified};
};

struct ComparableCrossStaffEvent
{
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    int voiceIndex{};
    musx::util::Fraction tickTimePosition;
    musx::util::Fraction durationTime;
    bool isChord{};
    mx::api::Step step{};
    int alter{};
    int octave{};
    std::optional<int> crossStaffIndex;

    bool operator==(const ComparableCrossStaffEvent&) const = default;
};

struct ComparableGraceSlashEvent
{
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    int voiceIndex{};
    musx::util::Fraction tickTimePosition;
    mx::api::Step step{};
    int alter{};
    int octave{};
    mx::api::Bool graceSlash{mx::api::Bool::unspecified};

    bool operator==(const ComparableGraceSlashEvent&) const = default;
};

struct ComparableMnxCrossStaffEvent
{
    size_t measureIndex{};
    int sequenceStaff{};
    int noteStaff{};
    mx::api::Step step{};
    int alter{};
    int octave{};

    bool operator==(const ComparableMnxCrossStaffEvent&) const = default;
};

struct ComparableGraceGroup
{
    size_t partIndex{};
    size_t measureIndex{};
    size_t staffIndex{};
    int voiceIndex{};
    bool slashed{};
    std::vector<std::tuple<mx::api::Step, int, int>> pitches;

    bool operator==(const ComparableGraceGroup&) const = default;
};

struct ComparableTremoloEvent
{
    mx::api::MarkType type{};
    int marks{};
    mx::api::DurationName durationName{};
    int durationDots{};
    int timeModificationActualNotes{};
    int timeModificationNormalNotes{};

    bool operator==(const ComparableTremoloEvent&) const = default;
};

mx::api::Step parseStep(const std::string& step)
{
    if (step == "A") return mx::api::Step::a;
    if (step == "B") return mx::api::Step::b;
    if (step == "C") return mx::api::Step::c;
    if (step == "D") return mx::api::Step::d;
    if (step == "E") return mx::api::Step::e;
    if (step == "F") return mx::api::Step::f;
    if (step == "G") return mx::api::Step::g;
    throw std::logic_error("Unexpected MNX pitch step.");
}

std::filesystem::path exportMnxFixture(const std::string& musxFile)
{
    std::filesystem::path inputPath;
    copyInputToOutput(musxFile, inputPath);

    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto outputPath = inputPath;
    outputPath.replace_extension(".mnx");
    EXPECT_TRUE(std::filesystem::exists(outputPath)) << "Missing MNX output " << pathString(outputPath);
    return outputPath;
}

std::vector<ComparableTremoloEvent> createComparableTremoloEvents(const mx::api::ScoreData& score)
{
    std::vector<ComparableTremoloEvent> result;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    (void)voiceIndex;
                    for (const auto& note : voice.notes) {
                        for (const auto& mark : note.noteAttachmentData.marks) {
                            if (mark.markType != mx::api::MarkType::tremoloStart && mark.markType != mx::api::MarkType::tremoloStop) {
                                continue;
                            }
                            result.emplace_back(ComparableTremoloEvent{
                                .type = mark.markType,
                                .marks = mark.choice.tremolo().tremoloMarks.value_or(0),
                                .durationName = note.durationData.durationName,
                                .durationDots = note.durationData.durationDots,
                                .timeModificationActualNotes = note.durationData.timeModificationActualNotes,
                                .timeModificationNormalNotes = note.durationData.timeModificationNormalNotes
                            });
                        }
                    }
                }
            }
        }
    }
    return result;
}

std::vector<ComparableNoteEvent> createComparableNoteEvents(const mx::api::ScoreData& score)
{
    std::vector<ComparableNoteEvent> result;
    if (score.ticksPerQuarter <= 0) {
        throw std::logic_error("MusicXML test score has invalid ticksPerQuarter.");
    }
    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        const auto& part = score.parts.at(partIndex);
        for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
            const auto& measure = part.measures.at(measureIndex);
            for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
                const auto& staff = measure.staves.at(staffIndex);
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    for (const auto& note : voice.notes) {
                        result.push_back({
                            partIndex,
                            measureIndex,
                            staffIndex,
                            voiceIndex,
                            musx::util::Fraction(note.tickTimePosition, score.ticksPerQuarter),
                            musx::util::Fraction(note.durationData.durationTimeTicks, score.ticksPerQuarter),
                            note.isRest,
                            note.isMeasureRest,
                            note.isChord,
                            note.isTieStart,
                            note.isTieStop,
                            note.isRest ? mx::api::Step::unspecified : note.pitchData.step,
                            note.isRest ? 0 : note.pitchData.alter,
                            note.isRest ? 0 : note.pitchData.octave
                        });
                    }
                }
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const ComparableNoteEvent& lhs, const ComparableNoteEvent& rhs) {
        if (lhs.partIndex != rhs.partIndex) {
            return lhs.partIndex < rhs.partIndex;
        }
        if (lhs.measureIndex != rhs.measureIndex) {
            return lhs.measureIndex < rhs.measureIndex;
        }
        if (lhs.staffIndex != rhs.staffIndex) {
            return lhs.staffIndex < rhs.staffIndex;
        }
        if (lhs.voiceIndex != rhs.voiceIndex) {
            return lhs.voiceIndex < rhs.voiceIndex;
        }
        if (lhs.tickTimePosition != rhs.tickTimePosition) {
            return lhs.tickTimePosition < rhs.tickTimePosition;
        }
        if (lhs.durationTime != rhs.durationTime) {
            return lhs.durationTime < rhs.durationTime;
        }
        if (lhs.isRest != rhs.isRest) {
            return lhs.isRest < rhs.isRest;
        }
        if (lhs.isMeasureRest != rhs.isMeasureRest) {
            return lhs.isMeasureRest < rhs.isMeasureRest;
        }
        if (lhs.isChord != rhs.isChord) {
            return lhs.isChord < rhs.isChord;
        }
        if (lhs.isTieStart != rhs.isTieStart) {
            return lhs.isTieStart < rhs.isTieStart;
        }
        if (lhs.isTieStop != rhs.isTieStop) {
            return lhs.isTieStop < rhs.isTieStop;
        }
        if (lhs.step != rhs.step) {
            return lhs.step < rhs.step;
        }
        if (lhs.alter != rhs.alter) {
            return lhs.alter < rhs.alter;
        }
        return lhs.octave < rhs.octave;
    });
    return result;
}

std::vector<ComparableStemEvent> createComparableStemEvents(const mx::api::ScoreData& score)
{
    std::vector<ComparableStemEvent> result;
    if (score.ticksPerQuarter <= 0) {
        throw std::logic_error("MusicXML test score has invalid ticksPerQuarter.");
    }
    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        const auto& part = score.parts.at(partIndex);
        for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
            const auto& measure = part.measures.at(measureIndex);
            for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
                const auto& staff = measure.staves.at(staffIndex);
                for (const auto& voicePair : staff.voices) {
                    const auto& voice = voicePair.second;
                    for (const auto& note : voice.notes) {
                        if (note.isRest || note.stem == mx::api::Stem::unspecified) {
                            continue;
                        }
                        result.push_back({
                            partIndex,
                            measureIndex,
                            staffIndex,
                            musx::util::Fraction(note.tickTimePosition, score.ticksPerQuarter),
                            musx::util::Fraction(note.durationData.durationTimeTicks, score.ticksPerQuarter),
                            note.isChord,
                            note.pitchData.step,
                            note.pitchData.alter,
                            note.pitchData.octave,
                            note.stem
                        });
                    }
                }
            }
        }
    }
    return result;
}

std::vector<ComparableMnxCrossStaffEvent> createComparableMnxCrossStaffEvents(const nlohmann::json& mnx)
{
    std::vector<ComparableMnxCrossStaffEvent> result;
    if (!mnx.contains("parts") || mnx["parts"].empty()) {
        return result;
    }
    const auto& measures = mnx["parts"][0]["measures"];
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        const auto& measure = measures.at(measureIndex);
        if (!measure.contains("sequences")) {
            continue;
        }
        for (const auto& sequence : measure["sequences"]) {
            if (!sequence.contains("staff") || !sequence.contains("content")) {
                continue;
            }
            const int sequenceStaff = sequence["staff"];
            for (const auto& item : sequence["content"]) {
                if (!item.contains("notes")) {
                    continue;
                }
                for (const auto& note : item["notes"]) {
                    if (!note.contains("staff")) {
                        continue;
                    }
                    const auto& pitch = note["pitch"];
                    result.push_back({
                        measureIndex,
                        sequenceStaff,
                        note["staff"],
                        parseStep(pitch["step"]),
                        pitch.value("alter", 0),
                        pitch["octave"]
                    });
                }
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const ComparableMnxCrossStaffEvent& lhs, const ComparableMnxCrossStaffEvent& rhs) {
        if (lhs.measureIndex != rhs.measureIndex) return lhs.measureIndex < rhs.measureIndex;
        if (lhs.sequenceStaff != rhs.sequenceStaff) return lhs.sequenceStaff < rhs.sequenceStaff;
        if (lhs.noteStaff != rhs.noteStaff) return lhs.noteStaff < rhs.noteStaff;
        if (lhs.step != rhs.step) return lhs.step < rhs.step;
        if (lhs.alter != rhs.alter) return lhs.alter < rhs.alter;
        return lhs.octave < rhs.octave;
    });
    return result;
}

std::vector<ComparableMnxCrossStaffEvent> createComparableMusicXmlCrossStaffEvents(const std::filesystem::path& path)
{
    pugi::xml_document document;
    const auto loadResult = document.load_file(path.c_str());
    if (!loadResult) {
        throw std::logic_error("Unable to load MusicXML output for cross-staff comparison.");
    }

    std::vector<ComparableMnxCrossStaffEvent> result;
    const auto part = document.child("score-partwise").child("part");
    size_t measureIndex = 0;
    for (auto measure = part.child("measure"); measure; measure = measure.next_sibling("measure"), ++measureIndex) {
        std::vector<pugi::xml_node> eventNotes;
        const auto flushEvent = [&]() {
            if (eventNotes.empty()) {
                return;
            }
            std::vector<std::pair<int, int>> noteStaffCounts;
            for (const auto& note : eventNotes) {
                const auto pitch = note.child("pitch");
                if (!pitch) {
                    continue;
                }
                const int staffNumber = note.child("staff").text().as_int();
                const auto countIt = std::find_if(noteStaffCounts.begin(), noteStaffCounts.end(), [staffNumber](const auto& pair) {
                    return pair.first == staffNumber;
                });
                if (countIt == noteStaffCounts.end()) {
                    noteStaffCounts.emplace_back(staffNumber, 1);
                } else {
                    ++countIt->second;
                }
            }
            if (noteStaffCounts.size() <= 1) {
                eventNotes.clear();
                return;
            }
            const auto homeStaffIt = std::max_element(noteStaffCounts.begin(), noteStaffCounts.end(), [](const auto& lhs, const auto& rhs) {
                if (lhs.second != rhs.second) return lhs.second < rhs.second;
                return lhs.first > rhs.first;
            });
            const int homeStaff = homeStaffIt->first;
            for (const auto& note : eventNotes) {
                const auto pitch = note.child("pitch");
                if (!pitch) {
                    continue;
                }
                const int noteStaff = note.child("staff").text().as_int();
                if (noteStaff == homeStaff) {
                    continue;
                }
                result.push_back({
                    measureIndex,
                    homeStaff,
                    noteStaff,
                    parseStep(pitch.child_value("step")),
                    pitch.child("alter").text().as_int(),
                    pitch.child("octave").text().as_int()
                });
            }
            eventNotes.clear();
        };

        for (auto note = measure.child("note"); note; note = note.next_sibling("note")) {
            if (!note.child("pitch")) {
                flushEvent();
                continue;
            }
            if (!note.child("chord")) {
                flushEvent();
            }
            eventNotes.emplace_back(note);
        }
        flushEvent();
    }
    std::sort(result.begin(), result.end(), [](const ComparableMnxCrossStaffEvent& lhs, const ComparableMnxCrossStaffEvent& rhs) {
        if (lhs.measureIndex != rhs.measureIndex) return lhs.measureIndex < rhs.measureIndex;
        if (lhs.sequenceStaff != rhs.sequenceStaff) return lhs.sequenceStaff < rhs.sequenceStaff;
        if (lhs.noteStaff != rhs.noteStaff) return lhs.noteStaff < rhs.noteStaff;
        if (lhs.step != rhs.step) return lhs.step < rhs.step;
        if (lhs.alter != rhs.alter) return lhs.alter < rhs.alter;
        return lhs.octave < rhs.octave;
    });
    return result;
}

std::vector<ComparableGraceGroup> createComparableMnxGraceGroups(const nlohmann::json& mnx)
{
    std::vector<ComparableGraceGroup> result;
    if (!mnx.contains("parts") || mnx["parts"].empty()) {
        return result;
    }
    const auto& measures = mnx["parts"][0]["measures"];
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        const auto& measure = measures.at(measureIndex);
        if (!measure.contains("sequences")) {
            continue;
        }
        for (const auto& sequence : measure["sequences"]) {
            if (!sequence.contains("content")) {
                continue;
            }
            const size_t staffIndex = static_cast<size_t>(sequence.value("staff", 1) - 1);
            const int voiceIndex = 0;
            for (const auto& item : sequence["content"]) {
                if (item.value("type", "") != "grace") {
                    continue;
                }
                auto group = ComparableGraceGroup{ 0, measureIndex, staffIndex, voiceIndex, !item.contains("slash"), {} };
                for (const auto& event : item["content"]) {
                    if (!event.contains("notes")) {
                        continue;
                    }
                    for (const auto& note : event["notes"]) {
                        const auto& pitch = note["pitch"];
                        group.pitches.emplace_back(parseStep(pitch["step"]), pitch.value("alter", 0), pitch["octave"]);
                    }
                }
                result.emplace_back(std::move(group));
            }
        }
    }
    return result;
}

std::vector<ComparableGraceGroup> createComparableMusicXmlGraceGroups(const std::filesystem::path& path)
{
    pugi::xml_document document;
    const auto loadResult = document.load_file(path.c_str());
    if (!loadResult) {
        throw std::logic_error("Unable to load MusicXML output for grace comparison.");
    }

    std::vector<ComparableGraceGroup> result;
    const auto part = document.child("score-partwise").child("part");
    size_t measureIndex = 0;
    for (auto measure = part.child("measure"); measure; measure = measure.next_sibling("measure"), ++measureIndex) {
        std::optional<bool> previousSlashed;
        for (auto note = measure.child("note"); note; note = note.next_sibling("note")) {
            const auto grace = note.child("grace");
            if (!grace) {
                previousSlashed.reset();
                continue;
            }
            const auto pitch = note.child("pitch");
            if (!pitch) {
                previousSlashed.reset();
                continue;
            }
            const bool slashed = std::string_view(grace.attribute("slash").as_string()) == "yes";
            if (!previousSlashed || *previousSlashed != slashed) {
                const auto staffNode = note.child("staff");
                const int staffNumber = staffNode ? staffNode.text().as_int() : 1;
                result.push_back({ 0, measureIndex, static_cast<size_t>(staffNumber - 1), 0, slashed, {} });
            }
            result.back().pitches.emplace_back(
                parseStep(pitch.child_value("step")),
                pitch.child("alter").text().as_int(),
                pitch.child("octave").text().as_int());
            previousSlashed = slashed;
        }
    }
    return result;
}

void compareNoteEvents(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualEvents = createComparableNoteEvents(actual);
    const auto expectedEvents = createComparableNoteEvents(expected);
    ASSERT_EQ(actualEvents.size(), expectedEvents.size()) << "note/rest event count";
    for (size_t eventIndex = 0; eventIndex < expectedEvents.size(); ++eventIndex) {
        EXPECT_EQ(actualEvents[eventIndex], expectedEvents[eventIndex]) << "note/rest event " << eventIndex;
    }
}

void compareStemEventsSetByExporter(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualEvents = createComparableStemEvents(actual);
    auto expectedEvents = createComparableStemEvents(expected);
    ASSERT_FALSE(actualEvents.empty()) << "expected Denigma to export at least one explicit stem";
    for (const auto& actualEvent : actualEvents) {
        const auto expectedIt = std::find_if(expectedEvents.begin(), expectedEvents.end(), [&actualEvent](const ComparableStemEvent& expectedEvent) {
            return actualEvent.partIndex == expectedEvent.partIndex
                && actualEvent.measureIndex == expectedEvent.measureIndex
                && actualEvent.staffIndex == expectedEvent.staffIndex
                && actualEvent.tickTimePosition == expectedEvent.tickTimePosition
                && actualEvent.durationTime == expectedEvent.durationTime
                && actualEvent.isChord == expectedEvent.isChord
                && actualEvent.step == expectedEvent.step
                && actualEvent.alter == expectedEvent.alter
                && actualEvent.octave == expectedEvent.octave;
        });
        ASSERT_NE(expectedIt, expectedEvents.end()) << "missing reference note for exported stem";
        EXPECT_EQ(actualEvent.stem, expectedIt->stem);
        expectedEvents.erase(expectedIt);
    }
}

void compareCrossStaffEventsToMnx(const std::filesystem::path& musicXmlPath, const nlohmann::json& mnx)
{
    const auto musicXmlEvents = createComparableMusicXmlCrossStaffEvents(musicXmlPath);
    const auto mnxEvents = createComparableMnxCrossStaffEvents(mnx);
    ASSERT_EQ(musicXmlEvents.size(), mnxEvents.size()) << "cross-staff event count vs MNX";
    for (size_t eventIndex = 0; eventIndex < mnxEvents.size(); ++eventIndex) {
        EXPECT_EQ(musicXmlEvents[eventIndex], mnxEvents[eventIndex]) << "cross-staff event vs MNX " << eventIndex;
    }
}

void compareGraceGroupsToMnx(const std::filesystem::path& musicXmlPath, const nlohmann::json& mnx)
{
    const auto musicXmlGroups = createComparableMusicXmlGraceGroups(musicXmlPath);
    const auto mnxGroups = createComparableMnxGraceGroups(mnx);
    ASSERT_EQ(musicXmlGroups.size(), mnxGroups.size()) << "grace group count vs MNX";
    for (size_t groupIndex = 0; groupIndex < mnxGroups.size(); ++groupIndex) {
        EXPECT_EQ(musicXmlGroups[groupIndex], mnxGroups[groupIndex]) << "grace group vs MNX " << groupIndex;
    }
}

std::vector<ComparableNoteEvent> createComparableTieEvents(const mx::api::ScoreData& score)
{
    auto events = createComparableNoteEvents(score);
    events.erase(std::remove_if(events.begin(), events.end(), [](const ComparableNoteEvent& event) {
        return !event.isTieStart && !event.isTieStop;
    }), events.end());
    return events;
}

std::vector<ComparableNoteEvent> createComparablePairedTieEvents(const mx::api::ScoreData& score)
{
    struct TieKey
    {
        size_t partIndex{};
        size_t staffIndex{};
        int voiceIndex{};
        mx::api::Step step{};
        int alter{};
        int octave{};

        bool operator==(const TieKey&) const = default;
    };

    const auto makeKey = [](const ComparableNoteEvent& event) {
        return TieKey{event.partIndex, event.staffIndex, event.voiceIndex, event.step, event.alter, event.octave};
    };

    const auto allTieEvents = createComparableTieEvents(score);
    std::vector<ComparableNoteEvent> result;
    std::vector<std::pair<TieKey, ComparableNoteEvent>> pendingStarts;
    for (const auto& event : allTieEvents) {
        if (event.isTieStop) {
            const auto key = makeKey(event);
            const auto startIt = std::find_if(pendingStarts.begin(), pendingStarts.end(), [&key](const auto& pendingStart) {
                return pendingStart.first == key;
            });
            if (startIt != pendingStarts.end()) {
                result.push_back(startIt->second);
                result.push_back(event);
                pendingStarts.erase(startIt);
            }
        }
        if (event.isTieStart) {
            pendingStarts.emplace_back(makeKey(event), event);
        }
    }
    return result;
}

void compareTieEvents(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualEvents = createComparablePairedTieEvents(actual);
    const auto expectedEvents = createComparablePairedTieEvents(expected);
    ASSERT_EQ(actualEvents.size(), createComparableTieEvents(actual).size()) << "exported tie events should all be paired";
    ASSERT_EQ(actualEvents.size(), expectedEvents.size()) << "tie event count";
    for (size_t eventIndex = 0; eventIndex < expectedEvents.size(); ++eventIndex) {
        EXPECT_EQ(actualEvents[eventIndex], expectedEvents[eventIndex]) << "tie event " << eventIndex;
    }
}

void expectNotesUseStaffQualifiedVoiceNumbers(const mx::api::ScoreData& score)
{
    for (size_t partIndex = 0; partIndex < score.parts.size(); ++partIndex) {
        const auto& part = score.parts.at(partIndex);
        for (size_t measureIndex = 0; measureIndex < part.measures.size(); ++measureIndex) {
            const auto& measure = part.measures.at(measureIndex);
            for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
                const auto& staff = measure.staves.at(staffIndex);
                const int firstVoiceIndex = denigma::formats::musicxml::detail::musicXmlVoiceNumber(staffIndex, 0, 1) - 1;
                const int nextStaffFirstVoiceIndex = denigma::formats::musicxml::detail::musicXmlVoiceNumber(staffIndex + 1, 0, 1) - 1;
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    for (const auto& note : voice.notes) {
                        (void)note;
                        EXPECT_GE(voiceIndex, firstVoiceIndex)
                            << "part " << partIndex << " measure " << measureIndex << " staff " << staffIndex;
                        EXPECT_LT(voiceIndex, nextStaffFirstVoiceIndex)
                            << "part " << partIndex << " measure " << measureIndex << " staff " << staffIndex;
                    }
                }
            }
        }
    }
}

std::vector<const mx::api::NoteData*> measureRestsInBar(const mx::api::ScoreData& score, size_t barNumber)
{
    constexpr size_t firstBarNumber = 1;
    std::vector<const mx::api::NoteData*> result;
    if (score.parts.empty()) {
        ADD_FAILURE() << "score has no parts";
        return result;
    }
    const auto& measures = score.parts.front().measures;
    if (measures.size() < barNumber) {
        ADD_FAILURE() << "score has " << measures.size() << " measures, expected at least " << barNumber;
        return result;
    }
    const auto& measure = measures.at(barNumber - firstBarNumber);
    for (const auto& staff : measure.staves) {
        for (const auto& [voiceIndex, voice] : staff.voices) {
            (void)voiceIndex;
            for (const auto& note : voice.notes) {
                if (note.isMeasureRest) {
                    result.emplace_back(&note);
                }
            }
        }
    }
    return result;
}

void expectMeasureRestPosition(const mx::api::NoteData& note, std::optional<std::pair<mx::api::Step, int>> expectedPosition)
{
    EXPECT_EQ(note.isDisplayStepOctaveSpecified, expectedPosition.has_value());
    if (expectedPosition) {
        EXPECT_EQ(note.pitchData.step, expectedPosition->first);
        EXPECT_EQ(note.pitchData.octave, expectedPosition->second);
    }
}

} // namespace

TEST(MusicXmlNotes, ChangingTimeSignaturesNotesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("timesigs_changing.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/timesigs_changing-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareNoteEvents(*actualScore, *expectedScore);
}

TEST(MusicXmlNotes, ArtificialHarmonicsExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("harmonics_artificial.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/harmonics_artificial-ref.musicxml");
    ASSERT_TRUE(expectedScore);

    // Notated pitches, durations, and chord structure should match Finale's own reference export exactly.
    compareNoteEvents(*actualScore, *expectedScore);

    // Each touched (diamond) note should carry a <harmonic> technical mark. Finale's own MusicXML export
    // (the reference file) does not emit this -- it relies on notehead shape and interval alone -- but
    // Denigma's classifier identifies the pattern, so Denigma adds the mark as a semantic enrichment
    // beyond what Finale itself exports. See "Artificial-harmonic technical detail" in mx-api-gaps.md
    // for what mx::api still cannot express on it (natural/artificial, base/touching/sounding pitch).
    struct ExpectedNote
    {
        mx::api::Step step;
        int alter;
        int octave;
        mx::api::Notehead notehead;
        bool expectHarmonicMark;
    };
    const std::vector<ExpectedNote> expectedNotes = {
        { mx::api::Step::e, -1, 3, mx::api::Notehead::normal, false },  // stopped (major third touch)
        { mx::api::Step::g, 0, 3, mx::api::Notehead::diamond, true },   // touched
        { mx::api::Step::b, 0, 3, mx::api::Notehead::normal, false },   // stopped (fourth touch)
        { mx::api::Step::e, 0, 4, mx::api::Notehead::diamond, true },   // touched
        { mx::api::Step::f, 0, 3, mx::api::Notehead::normal, false },   // stopped (fifth touch)
        { mx::api::Step::c, 0, 4, mx::api::Notehead::diamond, true },   // touched
    };

    ASSERT_FALSE(actualScore->parts.empty());
    ASSERT_FALSE(actualScore->parts.front().measures.empty());
    const auto& measure = actualScore->parts.front().measures.front();
    ASSERT_FALSE(measure.staves.empty());
    ASSERT_FALSE(measure.staves.front().voices.empty());
    const auto& notes = measure.staves.front().voices.begin()->second.notes;

    ASSERT_GE(notes.size(), expectedNotes.size());
    for (size_t noteIndex = 0; noteIndex < expectedNotes.size(); ++noteIndex) {
        const auto& expected = expectedNotes[noteIndex];
        const auto& note = notes[noteIndex];
        EXPECT_EQ(note.pitchData.step, expected.step) << "note " << noteIndex;
        EXPECT_EQ(note.pitchData.alter, expected.alter) << "note " << noteIndex;
        EXPECT_EQ(note.pitchData.octave, expected.octave) << "note " << noteIndex;
        EXPECT_EQ(note.notehead, expected.notehead) << "note " << noteIndex;
        const bool hasHarmonicMark = std::any_of(note.noteAttachmentData.marks.begin(), note.noteAttachmentData.marks.end(),
            [](const auto& mark) { return mark.markType == mx::api::MarkType::harmonic; });
        EXPECT_EQ(hasHarmonicMark, expected.expectHarmonicMark) << "note " << noteIndex;
    }
}

TEST(MusicXmlNotes, LargeOrchestraTiesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("large_orchestra.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/large_orchestra-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareTieEvents(*actualScore, *expectedScore);
}

TEST(MusicXmlNotes, TieTargetTypesExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("tie_target_types.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    const auto findBarlineIndex = [](const mx::api::MeasureData& measure, mx::api::HorizontalAlignment location) -> std::optional<size_t> {
        for (size_t barlineIndex = 0; barlineIndex < measure.barlines.size(); ++barlineIndex) {
            if (measure.barlines.at(barlineIndex).location == location) {
                return barlineIndex;
            }
        }
        return std::nullopt;
    };

    ASSERT_FALSE(actualScore->parts.empty());
    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 4u);

    const auto& firstEndingMeasure = measures.at(2);
    const auto firstEndingStartIndex = findBarlineIndex(firstEndingMeasure, mx::api::HorizontalAlignment::left);
    const auto firstEndingStopIndex = findBarlineIndex(firstEndingMeasure, mx::api::HorizontalAlignment::right);
    ASSERT_TRUE(firstEndingStartIndex);
    ASSERT_TRUE(firstEndingStopIndex);
    EXPECT_LT(*firstEndingStartIndex, *firstEndingStopIndex);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStartIndex).barlineType, mx::api::BarlineType::unspecified);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStartIndex).endingType, mx::api::EndingType::start);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStartIndex).endingNumber, 1);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStopIndex).barlineType, mx::api::BarlineType::lightHeavy);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStopIndex).endingType, mx::api::EndingType::stop);
    EXPECT_EQ(firstEndingMeasure.barlines.at(*firstEndingStopIndex).endingNumber, 1);

    const auto& secondEndingMeasure = measures.at(3);
    const auto secondEndingStartIndex = findBarlineIndex(secondEndingMeasure, mx::api::HorizontalAlignment::left);
    const auto secondEndingStopIndex = findBarlineIndex(secondEndingMeasure, mx::api::HorizontalAlignment::right);
    ASSERT_TRUE(secondEndingStartIndex);
    ASSERT_TRUE(secondEndingStopIndex);
    EXPECT_LT(*secondEndingStartIndex, *secondEndingStopIndex);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStartIndex).barlineType, mx::api::BarlineType::unspecified);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStartIndex).endingType, mx::api::EndingType::start);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStartIndex).endingNumber, 2);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStopIndex).barlineType, mx::api::BarlineType::unspecified);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStopIndex).endingType, mx::api::EndingType::discontinue);
    EXPECT_EQ(secondEndingMeasure.barlines.at(*secondEndingStopIndex).endingNumber, 2);
}

TEST(MusicXmlNotes, TieTargetTypesArpeggiatedTiesExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("tie_target_types.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    const auto tieEvents = createComparableTieEvents(*actualScore);
    const auto findTieEvent = [&](size_t measureIndex, mx::api::Step step, int octave, bool isTieStart) -> const ComparableNoteEvent* {
        for (const auto& event : tieEvents) {
            if (event.measureIndex == measureIndex && event.step == step && event.octave == octave
                && event.isTieStart == isTieStart && event.isTieStop == !isTieStart) {
                return &event;
            }
        }
        return nullptr;
    };

    // The E4/G4 chord tones in measure 3 (first ending) are separate single-note entries stacked
    // into a chord across layers, so Finale cannot connect them to measure 2 with an ordinary
    // entry-based tie. It uses arpeggiated-tie smart shapes instead. Measure indices below are
    // zero-based (measure "2" and "3" in the 1-based MusicXML output).
    constexpr size_t arpeggiatedTieStartMeasureIndex = 1;
    constexpr size_t firstEndingMeasureIndex = 2;

    EXPECT_TRUE(findTieEvent(arpeggiatedTieStartMeasureIndex, mx::api::Step::e, 4, true))
        << "expected an E4 arpeggiated tie start in measure 2";
    EXPECT_TRUE(findTieEvent(arpeggiatedTieStartMeasureIndex, mx::api::Step::g, 4, true))
        << "expected a G4 arpeggiated tie start in measure 2";
    EXPECT_TRUE(findTieEvent(firstEndingMeasureIndex, mx::api::Step::e, 4, false))
        << "expected an E4 arpeggiated tie stop in measure 3 (first ending)";
    EXPECT_TRUE(findTieEvent(firstEndingMeasureIndex, mx::api::Step::g, 4, false))
        << "expected a G4 arpeggiated tie stop in measure 3 (first ending)";
}

TEST(MusicXmlNotes, NonArpeggioMarksUseTopAndBottomEndpoints)
{
    const auto score = createScoreDataFromMusxPath(std::filesystem::path(MUSX_TEST_DATA_PATH) / "nonArpeggios.musx");
    ASSERT_TRUE(score);

    std::vector<mx::api::NonArpeggiatePlacement> placements;
    for (const auto& part : score->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        for (const auto& mark : note.noteAttachmentData.marks) {
                            if (mark.markType == mx::api::MarkType::nonArpeggiate) {
                                ASSERT_TRUE(mark.choice.isNonArpeggiate());
                                placements.emplace_back(mark.choice.nonArpeggiate().placement);
                            }
                        }
                    }
                }
            }
        }
    }

    ASSERT_FALSE(placements.empty());
    EXPECT_EQ(std::count(placements.begin(), placements.end(), mx::api::NonArpeggiatePlacement::top),
        std::count(placements.begin(), placements.end(), mx::api::NonArpeggiatePlacement::bottom));
}

TEST(MusicXmlNotes, VoicesStemsMatchFinaleWhereExported)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("voices.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto expectedScore = loadScoreData(getInputPath() / "musicxml/voices-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(expectedScore);

    compareStemEventsSetByExporter(*actualScore, *expectedScore);
}

TEST(MusicXmlNotes, CrossStaffNotesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("cross_staffs.musx");
    const auto mnxPath = exportMnxFixture("cross_staffs.musx");
    nlohmann::json mnx;
    openJson(mnxPath, mnx);

    compareCrossStaffEventsToMnx(outputPath, mnx);
}

TEST(MusicXmlNotes, GraceSlashStatesMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("grace_beamed.musx");
    const auto mnxPath = exportMnxFixture("grace_beamed.musx");
    nlohmann::json mnx;
    openJson(mnxPath, mnx);

    compareGraceGroupsToMnx(outputPath, mnx);
}

TEST(MusicXmlNotes, VoicesKeyboardUsesStaffQualifiedVoiceNumbers)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("voices_keyboard.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    expectNotesUseStaffQualifiedVoiceNumbers(*actualScore);

    constexpr size_t unpositionedMeasureRestBar = 3;
    const auto bar3Rests = measureRestsInBar(*actualScore, unpositionedMeasureRestBar);
    ASSERT_EQ(bar3Rests.size(), 2);
    expectMeasureRestPosition(*bar3Rests[0], std::nullopt);
    expectMeasureRestPosition(*bar3Rests[1], std::nullopt);

    constexpr size_t positionedMeasureRestBar = 6;
    const auto bar6Rests = measureRestsInBar(*actualScore, positionedMeasureRestBar);
    ASSERT_EQ(bar6Rests.size(), 2);
    expectMeasureRestPosition(*bar6Rests[0], std::pair{mx::api::Step::b, 5});
    expectMeasureRestPosition(*bar6Rests[1], std::nullopt);
}

TEST(MusicXmlNotes, ShapeNoteNoteheadsExportSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("note_shapes.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    // note_shapes.musx assigns Finale shape-note noteheads (square, triangle, ...) to some staves.
    // None of those shapes are in Denigma's specifically-recognized set (Regular/X/Diamond/Slash/Circle),
    // so they should come through as mx::api::Notehead::other rather than being silently dropped.
    size_t otherNoteheadCount = 0;
    for (const auto& part : actualScore->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    (void)voiceIndex;
                    for (const auto& note : voice.notes) {
                        if (note.notehead == mx::api::Notehead::other) {
                            ++otherNoteheadCount;
                        }
                    }
                }
            }
        }
    }
    EXPECT_GT(otherNoteheadCount, 0u) << "expected at least one shape-note notehead to export as mx::api::Notehead::other";
}

TEST(MusicXmlNotes, MeasuredTremolosExportStartStopMarks)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("tremolos.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    std::vector<std::pair<mx::api::MarkType, int>> tremoloMarks;
    for (const auto& part : actualScore->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    (void)voiceIndex;
                    for (const auto& note : voice.notes) {
                        for (const auto& mark : note.noteAttachmentData.marks) {
                            if (mark.markType != mx::api::MarkType::tremoloStart && mark.markType != mx::api::MarkType::tremoloStop) {
                                continue;
                            }
                            ASSERT_TRUE(mark.choice.isTremolo());
                            ASSERT_TRUE(mark.choice.tremolo().tremoloMarks);
                            tremoloMarks.emplace_back(mark.markType, *mark.choice.tremolo().tremoloMarks);
                        }
                    }
                }
            }
        }
    }

    EXPECT_EQ(tremoloMarks, (std::vector<std::pair<mx::api::MarkType, int>>{
                                { mx::api::MarkType::tremoloStart, 3 },
                                { mx::api::MarkType::tremoloStop, 3 },
                                { mx::api::MarkType::tremoloStart, 2 },
                                { mx::api::MarkType::tremoloStop, 2 } }));
}

TEST(MusicXmlNotes, TwoNoteTremolosMatchFinaleReference)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("termolos_twonotes.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto referenceScore = loadScoreData(getInputPath() / "musicxml/termolos_twonotes-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(referenceScore);

    EXPECT_EQ(createComparableTremoloEvents(*actualScore), createComparableTremoloEvents(*referenceScore));
}
