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

#include "gtest/gtest.h"
#include "musicxml_test.h"
#include "test_utils.h"

namespace denigma::test::musicxml {

TEST(MusicXmlExpressions, TempoMarksExportDirectionAndSound)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("tempo_text_shape.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    size_t tempoDirectionCount = 0;
    bool foundVisibleTempoDirection = false;
    bool foundSoundOnlyTempo = false;
    for (const auto& measure : actualScore->parts.front().measures) {
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                if (!direction.isSoundDataSpecified || direction.soundData.tempo < 0.0) {
                    continue;
                }
                ++tempoDirectionCount;
                if (!direction.words.empty()) {
                    foundVisibleTempoDirection = true;
                    EXPECT_FALSE(direction.words.front().text.empty());
                } else {
                    foundSoundOnlyTempo = true;
                }
            }
        }
    }

    EXPECT_EQ(tempoDirectionCount, 2u);
    EXPECT_TRUE(foundVisibleTempoDirection);
    EXPECT_TRUE(foundSoundOnlyTempo);
}

TEST(MusicXmlExpressions, TempoVariedStavesSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("tempo_varied_staves.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    auto findPart = [&](const std::string& partName) -> const mx::api::PartData* {
        const auto it = std::find_if(actualScore->parts.begin(), actualScore->parts.end(), [&](const auto& part) {
            return part.name == partName;
        });
        return it != actualScore->parts.end() ? &*it : nullptr;
    };

    auto collectTempoEvents = [](const mx::api::MeasureData& measure) {
        struct TempoEvent
        {
            std::string words;
            double tempo{};
            bool hasWords{};
            bool hasSound{};
        };

        std::vector<TempoEvent> result;
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                if (!direction.isSoundDataSpecified || direction.soundData.tempo < 0.0) {
                    continue;
                }
                TempoEvent event;
                event.tempo = direction.soundData.tempo;
                event.hasSound = true;
                if (!direction.words.empty()) {
                    event.hasWords = true;
                    event.words = direction.words.front().text;
                }
                result.emplace_back(std::move(event));
            }
        }
        return result;
    };

    auto expectMeasureTempos = [&](const mx::api::PartData& part, size_t measureIndex, const std::vector<std::pair<std::string, double>>& expected) {
        ASSERT_GT(part.measures.size(), measureIndex) << part.name << " measure index " << measureIndex;
        const auto actual = collectTempoEvents(part.measures[measureIndex]);
        ASSERT_EQ(actual.size(), expected.size()) << part.name << " measure " << (measureIndex + 1);
        for (size_t i = 0; i < expected.size(); ++i) {
            EXPECT_EQ(actual[i].hasWords ? actual[i].words : std::string{}, expected[i].first)
                << part.name << " measure " << (measureIndex + 1) << " event " << i;
            EXPECT_DOUBLE_EQ(actual[i].tempo, expected[i].second)
                << part.name << " measure " << (measureIndex + 1) << " event " << i;
        }
    };

    const auto* piccolo = findPart("Piccolo");
    const auto* horn = findPart("Horn in F 1");
    const auto* violin = findPart("Violin I");
    ASSERT_TRUE(piccolo);
    ASSERT_TRUE(horn);
    ASSERT_TRUE(violin);

    const std::vector<std::pair<std::string, double>> piccoloExpected = {
        { "Tempo (∞=120)", 120.0 },
        { "accel.", 132.0 },
        { "", 144.0 }
    };
    const std::vector<std::pair<std::string, double>> hornExpected = {
        { "Tempo (∞=120)", 120.0 }
    };
    const std::vector<std::pair<std::string, double>> violinExpected = {
        { "Tempo (∞=120)", 120.0 },
        { "accel.", 132.0 }
    };

    expectMeasureTempos(*piccolo, 0, piccoloExpected);
    expectMeasureTempos(*piccolo, 5, piccoloExpected);
    expectMeasureTempos(*horn, 0, hornExpected);
    expectMeasureTempos(*horn, 5, hornExpected);
    expectMeasureTempos(*violin, 0, violinExpected);
    expectMeasureTempos(*violin, 5, violinExpected);
}

} // namespace denigma::test::musicxml
