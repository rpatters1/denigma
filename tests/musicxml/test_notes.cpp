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
#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"
#include "musx/util/Fraction.h"
#include "mx/api/ScoreData.h"
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
