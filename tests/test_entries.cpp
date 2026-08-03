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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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

#include <cstddef>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "denigma/classify/entries.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::classify;
using namespace musx::dom;

namespace {

DocumentPtr loadFixture(const std::string& fileName)
{
    const auto inputPath = getInputPath() / fileName;
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

// Collects every artificial-harmonic pair the classifier finds, in score order.
std::vector<entry::ArtificialHarmonic> classifyFixtureHarmonics(const DocumentPtr& document)
{
    std::vector<entry::ArtificialHarmonic> result;
    document->iterateEntries(SCORE_PARTID, [&](const EntryInfoPtr& entryInfo) -> bool {
        const auto classification = classifyEntryNoteheads(entryInfo);
        if (const auto* harmonics = classification.as<entry::ArtificialHarmonics>()) {
            result.insert(result.end(), harmonics->harmonics.begin(), harmonics->harmonics.end());
        }
        return true;
    });
    return result;
}

struct ExpectedPitch
{
    music_theory::NoteName noteName;
    int octave;
    int alteration;
};

void expectPitch(const NoteInfoPtr& note, const ExpectedPitch& expected, size_t index)
{
    const auto properties = note.calcNoteProperties();
    EXPECT_EQ(std::get<0>(properties), expected.noteName) << "harmonic " << index;
    EXPECT_EQ(std::get<1>(properties), expected.octave) << "harmonic " << index;
    EXPECT_EQ(std::get<2>(properties), expected.alteration) << "harmonic " << index;
}

} // namespace

TEST(RestPosition, FinaleToSmuflOffset)
{
    EXPECT_EQ(calcFinaleToSmuflRestPositionOffset(NoteType::Whole), 2);
    EXPECT_EQ(calcFinaleToSmuflRestPositionOffset(NoteType::Half), 0);
}

TEST(EntryNoteheadClassification, ReturnsUnrecognizedForInvalidEntry)
{
    EXPECT_FALSE(classifyEntryNoteheads(EntryInfoPtr()));
}

// The fixture's three chords cover every touch interval the classifier recognizes: Eb3 under a G3
// diamond (major third), B3 under an E4 diamond (fourth), and F3 under a C4 diamond (fifth). None of
// them writes the theoretical sounding pitch, so soundingNote is checked here on its negative path.
// ClassifiesWrittenSoundingPitch covers the positive path.
TEST(EntryNoteheadClassification, ClassifiesArtificialHarmonicChords)
{
    setupTestDataPaths();

    const auto document = loadFixture("harmonics_artificial.musx");
    ASSERT_TRUE(document);
    const auto found = classifyFixtureHarmonics(document);

    using TouchInterval = entry::ArtificialHarmonic::TouchInterval;
    using NoteName = music_theory::NoteName;
    struct ExpectedHarmonic
    {
        TouchInterval interval;
        ExpectedPitch stopped;
        ExpectedPitch touched;
    };
    const std::vector<ExpectedHarmonic> expectedHarmonics = {
        { TouchInterval::MajorThird, { NoteName::E, 3, -1 }, { NoteName::G, 3, 0 } },
        { TouchInterval::Fourth,     { NoteName::B, 3, 0 },  { NoteName::E, 4, 0 } },
        { TouchInterval::Fifth,      { NoteName::F, 3, 0 },  { NoteName::C, 4, 0 } },
    };

    ASSERT_EQ(found.size(), expectedHarmonics.size());
    for (size_t index = 0; index < expectedHarmonics.size(); ++index) {
        const auto& harmonic = found[index];
        const auto& expected = expectedHarmonics[index];
        EXPECT_EQ(harmonic.interval, expected.interval) << "harmonic " << index;
        EXPECT_EQ(harmonic.stoppedNotehead.shape, notehead::Shape::Regular) << "harmonic " << index;
        EXPECT_EQ(harmonic.touchedNotehead.shape, notehead::Shape::Diamond) << "harmonic " << index;
        EXPECT_FALSE(harmonic.soundingNote) << "harmonic " << index;
        expectPitch(harmonic.stoppedNote, expected.stopped, index);
        expectPitch(harmonic.touchedNote, expected.touched, index);
    }
}

// Both chords in this fixture write the theoretical sounding pitch as an explicit third note, which the
// classifier should bind to soundingNote rather than mistake for another stopped or touched note.
// Touching a fourth above excites the 4th partial, two octaves above the stopped note (G4 -> G6);
// touching a fifth above excites the 3rd partial, an octave and a fifth above it (G4 -> D6).
TEST(EntryNoteheadClassification, ClassifiesWrittenSoundingPitch)
{
    setupTestDataPaths();

    const auto document = loadFixture("harmonics_sounding.musx");
    ASSERT_TRUE(document);
    const auto found = classifyFixtureHarmonics(document);

    using TouchInterval = entry::ArtificialHarmonic::TouchInterval;
    using NoteName = music_theory::NoteName;
    struct ExpectedHarmonic
    {
        TouchInterval interval;
        ExpectedPitch stopped;
        ExpectedPitch touched;
        ExpectedPitch sounding;
    };
    const std::vector<ExpectedHarmonic> expectedHarmonics = {
        { TouchInterval::Fourth, { NoteName::G, 4, 0 }, { NoteName::C, 5, 0 }, { NoteName::G, 6, 0 } },
        { TouchInterval::Fifth,  { NoteName::G, 4, 0 }, { NoteName::D, 5, 0 }, { NoteName::D, 6, 0 } },
    };

    ASSERT_EQ(found.size(), expectedHarmonics.size());
    for (size_t index = 0; index < expectedHarmonics.size(); ++index) {
        const auto& harmonic = found[index];
        const auto& expected = expectedHarmonics[index];
        EXPECT_EQ(harmonic.interval, expected.interval) << "harmonic " << index;
        EXPECT_EQ(harmonic.stoppedNotehead.shape, notehead::Shape::Regular) << "harmonic " << index;
        EXPECT_EQ(harmonic.touchedNotehead.shape, notehead::Shape::Diamond) << "harmonic " << index;
        expectPitch(harmonic.stoppedNote, expected.stopped, index);
        expectPitch(harmonic.touchedNote, expected.touched, index);
        ASSERT_TRUE(harmonic.soundingNote) << "harmonic " << index;
        expectPitch(harmonic.soundingNote, expected.sounding, index);
    }
}
