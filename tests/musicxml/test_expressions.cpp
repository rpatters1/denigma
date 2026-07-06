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
#include <unordered_map>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "gtest/gtest.h"
#include "mnxdom.h"
#include "musicxml_test.h"
#include "musx/musx.h"
#include "pugixml.hpp"
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

TEST(MusicXmlExpressions, TempoToolChanges)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tempo_changes.musx", inputPath);

    ArgList enigmaxmlArgs = { DENIGMA_NAME, "export", pathString(inputPath), "--enigmaxml" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(enigmaxmlArgs.argc(), enigmaxmlArgs.argv()), 0) << "export to enigmaxml: " << pathString(inputPath);
    });

    ArgList mnxArgs = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx", "--include-tempo-tool" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(mnxArgs.argc(), mnxArgs.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml", "--include-tempo-tool" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to musicxml: " << pathString(inputPath);
    });

    auto outputPath = inputPath;
    outputPath.replace_extension(".musicxml");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    pugi::xml_document musicXmlDoc;
    ASSERT_TRUE(musicXmlDoc.load_file(pathString(outputPath).c_str()));
    std::unordered_map<size_t, int> measureDivisions;
    const auto firstPart = musicXmlDoc.child("score-partwise").child("part");
    ASSERT_TRUE(firstPart);
    int currentDivisions = 0;
    size_t measureIndex = 0;
    for (const auto& measureNode : firstPart.children("measure")) {
        if (const auto divisionsNode = measureNode.child("attributes").child("divisions")) {
            currentDivisions = divisionsNode.text().as_int();
        }
        measureDivisions.emplace(measureIndex++, currentDivisions);
    }

    std::vector<char> xmlBuf;
    readFile(inputPath.parent_path() / "tempo_changes.enigmaxml", xmlBuf);
    auto musxDoc = musx::factory::DocumentFactory::create<MusxReader>(xmlBuf);
    ASSERT_TRUE(musxDoc);

    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 4u);
    std::vector<std::vector<int>> xmlSoundPositions;
    std::vector<std::vector<int>> xmlTempoValues;
    for (const auto& measureNode : firstPart.children("measure")) {
        std::vector<int> positions;
        std::vector<int> tempoValues;
        for (const auto& soundNode : measureNode.children("sound")) {
            const auto tempoAttribute = soundNode.attribute("tempo");
            if (tempoAttribute.empty()) {
                continue;
            }
            tempoValues.emplace_back(tempoAttribute.as_int());
            if (const auto offsetNode = soundNode.child("offset")) {
                positions.emplace_back(offsetNode.text().as_int());
            } else {
                positions.emplace_back(0);
            }
        }
        xmlSoundPositions.emplace_back(std::move(positions));
        xmlTempoValues.emplace_back(std::move(tempoValues));
    }

    auto mnxDoc = mnx::Document::create(inputPath.parent_path() / "tempo_changes.mnx");
    const auto mnxMeasures = mnxDoc.global().measures();
    ASSERT_EQ(mnxMeasures.size(), xmlTempoValues.size());
    for (size_t measureIndex = 0; measureIndex < xmlTempoValues.size(); ++measureIndex) {
        const auto mnxTempos = mnxMeasures[measureIndex].tempos();
        ASSERT_TRUE(mnxTempos) << "measure " << (measureIndex + 1);
        ASSERT_EQ(mnxTempos->size(), xmlTempoValues[measureIndex].size()) << "measure " << (measureIndex + 1);
        for (size_t i = 0; i < mnxTempos->size(); ++i) {
            EXPECT_EQ(mnxTempos->at(i).bpm(), xmlTempoValues[measureIndex][i]) << "measure " << (measureIndex + 1);
        }
    }

    for (size_t measureIndex = 0; measureIndex < 4; ++measureIndex) {
        const auto musxTempoChanges = musxDoc->getOthers()->getArray<musx::dom::others::TempoChange>(
            musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(measureIndex + 1));
        ASSERT_GT(musxTempoChanges.size(), 0u);

        ASSERT_LT(measureIndex, xmlSoundPositions.size());
        const auto& soundPositions = xmlSoundPositions[measureIndex];
        ASSERT_EQ(soundPositions.size(), musxTempoChanges.size()) << "measure " << (measureIndex + 1);
        for (size_t i = 0; i < musxTempoChanges.size(); ++i) {
            const auto musxDura = musx::util::Fraction::fromEdu(musxTempoChanges[i]->eduPosition);
            ASSERT_TRUE(measureDivisions.contains(measureIndex));
            const int expectedPosition = measureDivisions.at(measureIndex) * 4 * musxDura.numerator() / musxDura.denominator();
            EXPECT_EQ(soundPositions[i], expectedPosition) << "measure " << (measureIndex + 1);
        }
    }
}

} // namespace denigma::test::musicxml
