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

namespace {

struct ComparableWordsDirection
{
    size_t measureIndex{};
    size_t staffIndex{};
    int tickTimePosition{};
    mx::api::Placement placement{mx::api::Placement::unspecified};
    std::vector<std::string> words;
    std::vector<mx::api::RehearsalEnclosure> enclosures;
};

std::vector<ComparableWordsDirection> collectWordsOnlyDirections(
    const mx::api::ScoreData& score,
    const std::function<bool(const ComparableWordsDirection&)>& predicate)
{
    std::vector<ComparableWordsDirection> result;
    if (score.parts.empty()) {
        return result;
    }
    const auto& measures = score.parts.front().measures;
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        const auto& measure = measures.at(measureIndex);
        for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
            const auto& staff = measure.staves.at(staffIndex);
            for (const auto& direction : staff.directions) {
                if (direction.words.empty() || direction.isSoundDataSpecified) {
                    continue;
                }
                ComparableWordsDirection comparable{
                    measureIndex,
                    staffIndex,
                    direction.tickTimePosition,
                    direction.placement,
                    {},
                    {}
                };
                for (const auto& word : direction.words) {
                    comparable.words.emplace_back(word.text);
                    comparable.enclosures.emplace_back(word.enclosure);
                }
                if (predicate(comparable)) {
                    result.emplace_back(std::move(comparable));
                }
            }
        }
    }
    return result;
}

struct ComparableRehearsalDirection
{
    size_t measureIndex{};
    int tickTimePosition{};
    mx::api::Placement placement{mx::api::Placement::unspecified};
    std::string text;
    mx::api::RehearsalEnclosure enclosure{mx::api::RehearsalEnclosure::unspecified};
    std::vector<std::string> fontFamilies;
    mx::api::FontStyle fontStyle{mx::api::FontStyle::unspecified};
    mx::api::FontWeight fontWeight{mx::api::FontWeight::unspecified};
    mx::api::FontSizeType fontSizeType{mx::api::FontSizeType::unspecified};
    mx::api::CssSize fontSizeCss{mx::api::CssSize::unspecified};
    std::optional<double> fontSizePoint;
    int underline{};
    int overline{};
    int lineThrough{};
};

std::string normalizeRehearsalFontFamily(std::string value)
{
    auto trim = [](std::string& text) {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
            text.erase(text.begin());
        }
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
            text.pop_back();
        }
    };

    trim(value);
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    constexpr std::string_view suffix = ", text";
    if (lowered.size() >= suffix.size() && lowered.substr(lowered.size() - suffix.size()) == suffix) {
        value.erase(value.size() - suffix.size());
        trim(value);
    }
    return value;
}

bool isTextFallbackFontFamily(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered == "text";
}

std::vector<ComparableRehearsalDirection> collectRehearsalDirections(const mx::api::ScoreData& score)
{
    std::vector<ComparableRehearsalDirection> result;
    if (score.parts.empty()) {
        return result;
    }

    const auto& measures = score.parts.front().measures;
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        const auto& measure = measures.at(measureIndex);
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                for (const auto& rehearsal : direction.rehearsals) {
                    ComparableRehearsalDirection comparable{
                        measureIndex,
                        direction.tickTimePosition,
                        direction.placement,
                        rehearsal.text,
                        rehearsal.enclosure,
                        {},
                        rehearsal.fontData.style,
                        rehearsal.fontData.weight,
                        rehearsal.fontData.sizeType,
                        rehearsal.fontData.sizeCss,
                        rehearsal.fontData.sizeType == mx::api::FontSizeType::point &&
                                rehearsal.fontData.sizePoint != mx::api::DOUBLE_UNSPECIFIED
                            ? std::make_optional(rehearsal.fontData.sizePoint)
                            : std::nullopt,
                        rehearsal.fontData.underline,
                        rehearsal.fontData.overline,
                        rehearsal.fontData.lineThrough
                    };
                    comparable.fontFamilies.reserve(rehearsal.fontData.fontFamily.size());
                    for (const auto& family : rehearsal.fontData.fontFamily) {
                        auto normalized = normalizeRehearsalFontFamily(family);
                        if (!isTextFallbackFontFamily(normalized)) {
                            comparable.fontFamilies.emplace_back(std::move(normalized));
                        }
                    }
                    result.emplace_back(std::move(comparable));
                }
            }
        }
    }
    return result;
}

struct ComparableExpressionEnclosure
{
    size_t measureIndex{};
    int tickTimePosition{};
    mx::api::Placement placement{mx::api::Placement::unspecified};
    std::string text;
    mx::api::RehearsalEnclosure enclosure{mx::api::RehearsalEnclosure::unspecified};
};

std::vector<ComparableExpressionEnclosure> collectExpressionEnclosures(
    const mx::api::ScoreData& score,
    const std::function<bool(const std::string&)>& predicate)
{
    std::vector<ComparableExpressionEnclosure> result;
    if (score.parts.empty()) {
        return result;
    }

    const auto& measures = score.parts.front().measures;
    for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
        const auto& measure = measures.at(measureIndex);
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                for (const auto& word : direction.words) {
                    if (predicate(word.text)) {
                        result.push_back({
                            measureIndex,
                            direction.tickTimePosition,
                            direction.placement,
                            word.text,
                            word.enclosure
                        });
                    }
                }
                for (const auto& rehearsal : direction.rehearsals) {
                    if (predicate(rehearsal.text)) {
                        result.push_back({
                            measureIndex,
                            direction.tickTimePosition,
                            direction.placement,
                            rehearsal.text,
                            rehearsal.enclosure
                        });
                    }
                }
            }
        }
    }
    return result;
}

} // namespace

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

TEST(MusicXmlExpressions, GenericTextDirectionsMatchReference)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("slurs_2staves.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    const auto isWarmDirection = [](const ComparableWordsDirection& direction) {
        return direction.words.size() == 1 && direction.words.front() == "warm";
    };
    const auto actualDirections = collectWordsOnlyDirections(*actualScore, isWarmDirection);

    ASSERT_EQ(actualDirections.size(), 1u);
    EXPECT_EQ(actualDirections.front().measureIndex, 2u);
    EXPECT_EQ(actualDirections.front().staffIndex, 0u);
    EXPECT_EQ(actualDirections.front().placement, mx::api::Placement::below);
    EXPECT_EQ(actualDirections.front().words, std::vector<std::string>{ "warm" });
    EXPECT_EQ(actualDirections.front().enclosures, std::vector<mx::api::RehearsalEnclosure>{ mx::api::RehearsalEnclosure::none });
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

TEST(MusicXmlExpressions, MeasureTextSmoke)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("measure_text.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto referenceScore = loadScoreData(getInputPath() / "musicxml/measure_text-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(referenceScore);
    ASSERT_FALSE(actualScore->parts.empty());
    ASSERT_FALSE(referenceScore->parts.empty());

    struct MeasureTextDirection
    {
        size_t measureIndex{};
        std::string text;
        mx::api::Placement placement{mx::api::Placement::unspecified};
        std::optional<double> defaultX;
        std::optional<double> defaultY;
        std::optional<double> relativeX;
        std::optional<int> offset;
    };

    const auto collectMeasureTextDirections = [](const mx::api::ScoreData& score) {
        std::vector<MeasureTextDirection> result;
        for (size_t measureIndex = 0; measureIndex < score.parts.front().measures.size(); ++measureIndex) {
            const auto& measure = score.parts.front().measures.at(measureIndex);
            for (const auto& staff : measure.staves) {
                for (const auto& direction : staff.directions) {
                    if (direction.words.size() != 1 || direction.isSoundDataSpecified) {
                        continue;
                    }
                    const auto& words = direction.words.front();
                    result.push_back({
                        measureIndex,
                        words.text,
                        direction.placement,
                        words.positionData.isDefaultXSpecified ? std::make_optional(words.positionData.defaultX) : std::nullopt,
                        words.positionData.isDefaultYSpecified ? std::make_optional(words.positionData.defaultY) : std::nullopt,
                        words.positionData.isRelativeXSpecified ? std::make_optional(words.positionData.relativeX) : std::nullopt,
                        direction.tickTimePosition > 0 ? std::make_optional(direction.tickTimePosition) : std::nullopt
                    });
                }
            }
        }
        return result;
    };

    const auto actualDirections = collectMeasureTextDirections(*actualScore);
    const auto referenceDirections = collectMeasureTextDirections(*referenceScore);

    ASSERT_EQ(actualDirections.size(), 3u);
    ASSERT_EQ(referenceDirections.size(), actualDirections.size());

    constexpr double kDefaultYTolerance = 1.0;
    for (size_t index = 0; index < actualDirections.size(); ++index) {
        const auto& actual = actualDirections[index];
        const auto& reference = referenceDirections[index];

        EXPECT_EQ(actual.measureIndex, reference.measureIndex);
        EXPECT_EQ(actual.text, reference.text);
        EXPECT_EQ(actual.placement, reference.placement);
        ASSERT_EQ(actual.defaultY.has_value(), reference.defaultY.has_value());
        if (actual.defaultY && reference.defaultY) {
            EXPECT_NEAR(*actual.defaultY, *reference.defaultY, kDefaultYTolerance);
        }
        EXPECT_EQ(actual.offset, reference.offset);
        if (reference.relativeX && *reference.relativeX < 0.0) {
            ASSERT_TRUE(actual.defaultX.has_value());
            EXPECT_NEAR(*actual.defaultX, *reference.relativeX, kDefaultYTolerance);
        }
    }
}

TEST(MusicXmlExpressions, RehearsalMarksMatchReference)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("rehearsal_marks.musx");
    const auto actualScore = loadScoreData(outputPath);
    const auto referenceScore = loadScoreData(getInputPath() / "musicxml/rehearsal_marks-ref.musicxml");
    ASSERT_TRUE(actualScore);
    ASSERT_TRUE(referenceScore);

    const auto actualRehearsals = collectRehearsalDirections(*actualScore);
    const auto referenceRehearsals = collectRehearsalDirections(*referenceScore);

    ASSERT_EQ(actualRehearsals.size(), referenceRehearsals.size());
    ASSERT_FALSE(actualRehearsals.empty());

    for (size_t index = 0; index < actualRehearsals.size(); ++index) {
        const auto& actual = actualRehearsals[index];
        const auto& reference = referenceRehearsals[index];

        EXPECT_EQ(actual.measureIndex, reference.measureIndex) << "rehearsal " << index;
        EXPECT_EQ(actual.tickTimePosition, reference.tickTimePosition) << "rehearsal " << index;
        EXPECT_EQ(actual.placement, reference.placement) << "rehearsal " << index;
        EXPECT_EQ(actual.text, reference.text) << "rehearsal " << index;
        EXPECT_EQ(actual.enclosure, reference.enclosure) << "rehearsal " << index;
        EXPECT_EQ(actual.fontFamilies, reference.fontFamilies) << "rehearsal " << index;
        if (reference.fontStyle != mx::api::FontStyle::unspecified) {
            EXPECT_EQ(actual.fontStyle, reference.fontStyle) << "rehearsal " << index;
        } else {
            EXPECT_EQ(actual.fontStyle, mx::api::FontStyle::normal) << "rehearsal " << index;
        }
        EXPECT_EQ(actual.fontWeight, reference.fontWeight) << "rehearsal " << index;
        EXPECT_EQ(actual.fontSizeType, reference.fontSizeType) << "rehearsal " << index;
        if (reference.fontSizeType == mx::api::FontSizeType::css) {
            EXPECT_EQ(actual.fontSizeCss, reference.fontSizeCss) << "rehearsal " << index;
        }
        ASSERT_EQ(actual.fontSizePoint.has_value(), reference.fontSizePoint.has_value()) << "rehearsal " << index;
        if (actual.fontSizePoint && reference.fontSizePoint) {
            EXPECT_DOUBLE_EQ(*actual.fontSizePoint, *reference.fontSizePoint) << "rehearsal " << index;
        }
        EXPECT_EQ(actual.underline, reference.underline) << "rehearsal " << index;
        EXPECT_EQ(actual.overline, reference.overline) << "rehearsal " << index;
        EXPECT_EQ(actual.lineThrough, reference.lineThrough) << "rehearsal " << index;
    }
}

TEST(MusicXmlExpressions, TechniquesMatchReference)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("techniques.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    struct ComparableTechniqueDirection
    {
        std::string text;
        std::optional<mx::api::Bool> pizzicato;
    };

    const auto collectTechniqueDirections = [](const mx::api::ScoreData& score) {
        std::vector<ComparableTechniqueDirection> result;
        if (score.parts.empty()) {
            return result;
        }
        const auto& measures = score.parts.front().measures;
        for (size_t measureIndex = 0; measureIndex < measures.size(); ++measureIndex) {
            const auto& measure = measures.at(measureIndex);
            for (size_t staffIndex = 0; staffIndex < measure.staves.size(); ++staffIndex) {
                const auto& staff = measure.staves.at(staffIndex);
                for (const auto& direction : staff.directions) {
                    if (direction.words.empty()) {
                        continue;
                    }
                    EXPECT_EQ(direction.words.size(), 1u);
                    if (direction.words.size() != 1u) {
                        continue;
                    }
                    ComparableTechniqueDirection comparable{
                        direction.words.front().text,
                        std::nullopt
                    };
                    if (direction.isSoundDataSpecified && direction.soundData.pizzicato != mx::api::Bool::unspecified) {
                        comparable.pizzicato = direction.soundData.pizzicato;
                    }
                    result.emplace_back(std::move(comparable));
                }
            }
        }
        return result;
    };

    const auto actualDirections = collectTechniqueDirections(*actualScore);

    const std::vector<ComparableTechniqueDirection> expected = {
        { "pizz.", mx::api::Bool::yes },
        { "arco", mx::api::Bool::no },
        { "mute", std::nullopt },
        { "arco", mx::api::Bool::no },
        { "senza sord.", std::nullopt },
        { "mute", std::nullopt },
        { "open", std::nullopt },
        { "harmon mute", std::nullopt },
        { "stopped", std::nullopt },
        { "open", std::nullopt }
    };

    ASSERT_EQ(actualDirections.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(actualDirections[index].text, expected[index].text) << "direction " << index;
        EXPECT_EQ(actualDirections[index].pizzicato, expected[index].pizzicato) << "direction " << index;
    }
}

TEST(MusicXmlExpressions, ExpressionEnclosuresExportExpectedShapes)
{
    setupTestDataPaths();

    /// @todo Switch this back to post-write MusicXML verification once MX preserves `WordsData.enclosure`.
    const auto outputPath = exportMusicXmlFixture("enclosures.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    const auto actualScore = createScoreDataFromMusicXmlFixture("enclosures.musx");
    ASSERT_TRUE(actualScore);

    const auto actualEnclosures = collectExpressionEnclosures(*actualScore, [](const std::string& text) {
        return text == "Tempo" || text == "expressive" || text == "pizz." || text == "Reh. 1";
    });

    const std::vector<ComparableExpressionEnclosure> expected = {
        { 0u, 0, mx::api::Placement::above, "Tempo", mx::api::RehearsalEnclosure::square },
        { 0u, 16, mx::api::Placement::below, "expressive", mx::api::RehearsalEnclosure::none },
        { 1u, 8, mx::api::Placement::above, "pizz.", mx::api::RehearsalEnclosure::rectangle },
        { 2u, 0, mx::api::Placement::above, "Reh. 1", mx::api::RehearsalEnclosure::oval }
    };

    ASSERT_EQ(actualEnclosures.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(actualEnclosures[index].measureIndex, expected[index].measureIndex) << "expression " << index;
        EXPECT_EQ(actualEnclosures[index].tickTimePosition, expected[index].tickTimePosition) << "expression " << index;
        EXPECT_EQ(actualEnclosures[index].placement, expected[index].placement) << "expression " << index;
        EXPECT_EQ(actualEnclosures[index].text, expected[index].text) << "expression " << index;
        EXPECT_EQ(actualEnclosures[index].enclosure, expected[index].enclosure) << "expression " << index;
    }
}

TEST(MusicXmlExpressions, MeasureTextEnclosuresUseStandardFrameRule)
{
    setupTestDataPaths();

    /// @todo Switch this back to post-write MusicXML verification once MX preserves `WordsData.enclosure`.
    const auto outputPath = exportMusicXmlFixture("enclosures.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    const auto actualScore = createScoreDataFromMusicXmlFixture("enclosures.musx");
    ASSERT_TRUE(actualScore);

    const auto actualEnclosures = collectExpressionEnclosures(*actualScore, [](const std::string& text) {
        return text == "no enclosure" || text == "has enclosure";
    });

    const std::vector<ComparableExpressionEnclosure> expected = {
        { 2u, 16, mx::api::Placement::below, "no enclosure", mx::api::RehearsalEnclosure::none },
        { 3u, 3, mx::api::Placement::above, "has enclosure", mx::api::RehearsalEnclosure::rectangle }
    };

    ASSERT_EQ(actualEnclosures.size(), expected.size());
    for (size_t index = 0; index < expected.size(); ++index) {
        EXPECT_EQ(actualEnclosures[index].measureIndex, expected[index].measureIndex) << "measure text " << index;
        EXPECT_EQ(actualEnclosures[index].tickTimePosition, expected[index].tickTimePosition) << "measure text " << index;
        EXPECT_EQ(actualEnclosures[index].placement, expected[index].placement) << "measure text " << index;
        EXPECT_EQ(actualEnclosures[index].text, expected[index].text) << "measure text " << index;
        EXPECT_EQ(actualEnclosures[index].enclosure, expected[index].enclosure) << "measure text " << index;
    }
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
    {
        size_t measureIndex = 0;
        for (const auto& measureNode : firstPart.children("measure")) {
            if (const auto divisionsNode = measureNode.child("attributes").child("divisions")) {
                currentDivisions = divisionsNode.text().as_int();
            }
            measureDivisions.emplace(measureIndex++, currentDivisions);
        }
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
