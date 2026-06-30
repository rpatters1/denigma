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
    mx::api::Step step{};
    int alter{};
    int octave{};

    bool operator==(const ComparableNoteEvent&) const = default;
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

void compareNoteEvents(const mx::api::ScoreData& actual, const mx::api::ScoreData& expected)
{
    const auto actualEvents = createComparableNoteEvents(actual);
    const auto expectedEvents = createComparableNoteEvents(expected);
    ASSERT_EQ(actualEvents.size(), expectedEvents.size()) << "note/rest event count";
    for (size_t eventIndex = 0; eventIndex < expectedEvents.size(); ++eventIndex) {
        EXPECT_EQ(actualEvents[eventIndex], expectedEvents[eventIndex]) << "note/rest event " << eventIndex;
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
