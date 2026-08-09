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
#include <optional>
#include <unordered_map>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "formats/musicxml/musicxml.h"
#include "formats/musicxml/musicxml_formatted_text.h"
#include "gtest/gtest.h"
#include "mnxdom.h"
#include "musicxml_test.h"
#include "musx/musx.h"
#include "pugixml.hpp"
#include "test_utils.h"
#include "utils/stringutils.h"

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

/// Defined in musicxml_dynamics.cpp and declared here rather than in musicxml.h, because nothing
/// in the library needs it: it exists so the test below can hold the walk over StandardDynamic to
/// MusicXML's count of dynamic elements.
size_t musicXmlStandardDynamicCount();

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma

namespace denigma::test::musicxml {

namespace {

struct ComparableWordsDirection
{
    size_t measureIndex{};
    size_t staffIndex{};
    int tickTimePosition{};
    mx::api::Placement placement{mx::api::Placement::unspecified};
    std::vector<std::string> words;
    std::vector<mx::api::Enclosure> enclosures;
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
                const auto words = directionWords(direction);
                if (words.empty() || direction.isSoundDataSpecified) {
                    continue;
                }
                ComparableWordsDirection comparable{
                    measureIndex,
                    staffIndex,
                    directionDrawnTick(direction),
                    direction.placement,
                    {},
                    {}
                };
                for (const auto& word : words) {
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
    mx::api::Enclosure enclosure{mx::api::Enclosure::unspecified};
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
                for (const auto& rehearsal : directionRehearsals(direction)) {
                    ComparableRehearsalDirection comparable{
                        measureIndex,
                        directionDrawnTick(direction),
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
    mx::api::Enclosure enclosure{mx::api::Enclosure::unspecified};
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
                for (const auto& word : directionWords(direction)) {
                    if (predicate(word.text)) {
                        result.push_back({
                            measureIndex,
                            directionDrawnTick(direction),
                            direction.placement,
                            word.text,
                            word.enclosure
                        });
                    }
                }
                for (const auto& rehearsal : directionRehearsals(direction)) {
                    if (predicate(rehearsal.text)) {
                        result.push_back({
                            measureIndex,
                            directionDrawnTick(direction),
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

/// @brief The number of dynamic elements MusicXML defines, and so the number of values
/// mx::api::StandardDynamic has.
constexpr size_t musicXmlDynamicElementCount = 26;

/// @brief One child of a MusicXML `<dynamics>` element: either a dynamic element named after the
/// letters it draws, or an other-dynamics fallback carrying its letters and SMuFL glyph name.
struct ComparableDynamicsComponent
{
    std::optional<mx::api::StandardDynamic> standard;
    std::string text;
    std::optional<std::string> smufl;

    bool operator==(const ComparableDynamicsComponent&) const = default;
};

/// @brief Spells out the children of one `<dynamics>` mark, whether it holds a lone standard
/// symbol or several components.
std::vector<ComparableDynamicsComponent> dynamicsComponents(const mx::api::MarkData& mark)
{
    if (mark.choice.isDynamic()) {
        return { { mark.choice.dynamic(), std::string{}, std::nullopt } };
    }
    std::vector<ComparableDynamicsComponent> result;
    for (const auto& component : mark.choice.compoundDynamics().components) {
        if (component.isStandard()) {
            result.emplace_back(component.standard(), std::string{}, std::nullopt);
        } else {
            result.emplace_back(std::nullopt, component.other().text, component.other().smufl);
        }
    }
    return result;
}

std::vector<std::vector<ComparableDynamicsComponent>> collectCompoundDynamics(const mx::api::ScoreData& score)
{
    std::vector<std::vector<ComparableDynamicsComponent>> result;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& direction : staff.directions) {
                    for (const auto& mark : directionMarks(direction)) {
                        // A mark spelling itself with one standard symbol collapses to a bare
                        // dynamic, so a compound choice is exactly a marking MusicXML has no
                        // single element for.
                        if (!mark.choice.isCompoundDynamics()) {
                            continue;
                        }
                        result.emplace_back(dynamicsComponents(mark));
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
                const auto words = directionWords(direction);
                if (!words.empty()) {
                    foundVisibleTempoDirection = true;
                    EXPECT_FALSE(words.front().text.empty());
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

TEST(MusicXmlExpressions, ConvertedSymbolsCarrySizeOnlyFromEngravingFonts)
{
    const auto makeFont = [](const std::string& fontName) {
        const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
      <charsetBank>Win</charsetBank>
      <charsetVal>8191</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>)xml" + fontName + R"xml(</name>
    </fontName>
  </others>
</finale>)xml";
        std::vector<char> buffer(xml.begin(), xml.end());
        auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
        auto font = std::make_shared<musx::dom::FontInfo>(document);
        font->fontId = 1;
        return std::pair{ std::move(document), std::move(font) };
    };

    mx::api::WordsData sourceWords;
    sourceWords.fontData.sizeType = mx::api::FontSizeType::point;
    sourceWords.fontData.sizePoint = 18.0;

    const auto engravingContext = makeFont("Maestro");
    const auto engravingSymbol = formats::musicxml::detail::musicXmlSymbolFromWords(
        sourceWords, engravingContext.second, "metNoteQuarterUp");
    EXPECT_EQ(engravingSymbol.fontData.sizeType, mx::api::FontSizeType::point);
    EXPECT_DOUBLE_EQ(engravingSymbol.fontData.sizePoint, 18.0);

    // This is the spelling stored by Windows Finale; registry matching also accepts spaces.
    const auto legacyTextContext = makeFont("EngraverTextT");
    const auto legacyTextSymbol = formats::musicxml::detail::musicXmlSymbolFromWords(
        sourceWords, legacyTextContext.second, "metNoteQuarterUp");
    EXPECT_EQ(legacyTextSymbol.fontData.sizeType, mx::api::FontSizeType::unspecified);

    const auto smuflTextContext = makeFont("Finale Maestro Text");
    const auto smuflTextSymbol = formats::musicxml::detail::musicXmlSymbolFromWords(
        sourceWords, smuflTextContext.second, "metNoteQuarterUp");
    EXPECT_EQ(smuflTextSymbol.fontData.sizeType, mx::api::FontSizeType::unspecified);
}

TEST(MusicXmlExpressions, HarpPedalDiagramMapsToOrderedPedalTunings)
{
    using PedalPosition = classify::expression::HarpDiagram::PedalPosition;

    const auto harpPedals = formats::musicxml::detail::musicXmlHarpPedals({
        PedalPosition::Flat,
        PedalPosition::Natural,
        PedalPosition::Sharp,
        PedalPosition::Sharp,
        PedalPosition::Natural,
        PedalPosition::Flat,
        PedalPosition::Sharp
    });

    constexpr int FLAT_ALTERATION = -1;
    constexpr int NATURAL_ALTERATION = 0;
    constexpr int SHARP_ALTERATION = 1;
    const std::vector<mx::api::HarpPedalTuning> expected = {
        { mx::api::Step::d, FLAT_ALTERATION },
        { mx::api::Step::c, NATURAL_ALTERATION },
        { mx::api::Step::b, SHARP_ALTERATION },
        { mx::api::Step::e, SHARP_ALTERATION },
        { mx::api::Step::f, NATURAL_ALTERATION },
        { mx::api::Step::g, FLAT_ALTERATION },
        { mx::api::Step::a, SHARP_ALTERATION }
    };
    EXPECT_EQ(harpPedals.pedalTunings, expected);
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
    EXPECT_EQ(actualDirections.front().enclosures, std::vector<mx::api::Enclosure>{ mx::api::Enclosure::unspecified });
}

TEST(MusicXmlExpressions, MultimeasureRestNumbersDoNotExportAsDirections)
{
    setupTestDataPaths();

    // Measure 31 of each fixture carries its rest count in an expression rather than a multimeasure
    // rest record. Finale exports that expression as a direction - words when the number is set in a
    // text font, a SMuFL symbol when it is set in the music font - which would print the number a
    // second time on top of the measure style. denigma folds it into the measure style instead, so
    // no direction may survive.
    constexpr size_t numberMeasureIndex = 30;
    const std::array<std::pair<std::string, std::string>, 2> fixtures{ {
        { "multimeas_rests.musx", "musicxml/multimeas_rests-ref.musicxml" },
        { "multimeas_rests_musfont.musx", "musicxml/multimeas_rests_musfont-ref.musicxml" },
    } };

    for (const auto& [musxFile, referenceFile] : fixtures) {
        SCOPED_TRACE(musxFile);

        const auto expectedScore = loadScoreData(getInputPath() / referenceFile);
        ASSERT_TRUE(expectedScore);
        ASSERT_FALSE(expectedScore->parts.empty());
        const auto& expectedMeasure = expectedScore->parts.front().measures.at(numberMeasureIndex);
        ASSERT_FALSE(expectedMeasure.staves.empty());
        // Guard the premise: Finale really does emit a direction here.
        EXPECT_FALSE(expectedMeasure.staves.front().directions.empty());

        const auto actualScore = loadScoreData(exportMusicXmlFixture(musxFile));
        ASSERT_TRUE(actualScore);
        ASSERT_FALSE(actualScore->parts.empty());
        const auto& actualMeasure = actualScore->parts.front().measures.at(numberMeasureIndex);
        ASSERT_FALSE(actualMeasure.staves.empty());
        EXPECT_TRUE(actualMeasure.staves.front().directions.empty());
    }
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
                if (auto runText = directionRunText(direction); !runText.empty()) {
                    event.hasWords = true;
                    event.words = std::move(runText);
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
        { "Tempo ({metNoteQuarterUp}=120)", 120.0 },
        { "accel.", 132.0 },
        { "", 144.0 }
    };
    const std::vector<std::pair<std::string, double>> hornExpected = {
        { "Tempo ({metNoteQuarterUp}=120)", 120.0 }
    };
    const std::vector<std::pair<std::string, double>> violinExpected = {
        { "Tempo ({metNoteQuarterUp}=120)", 120.0 },
        { "accel.", 132.0 }
    };

    expectMeasureTempos(*piccolo, 0, piccoloExpected);
    expectMeasureTempos(*piccolo, 5, piccoloExpected);
    expectMeasureTempos(*horn, 0, hornExpected);
    expectMeasureTempos(*horn, 5, hornExpected);
    expectMeasureTempos(*violin, 0, violinExpected);
    expectMeasureTempos(*violin, 5, violinExpected);
}

TEST(MusicXmlExpressions, AllFontsAvailableCliPreservesSourceTextRuns)
{
    setupTestDataPaths();

    std::filesystem::path inputPath;
    copyInputToOutput("tempo_varied_staves.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--musicxml", "--all-fonts-available" };
    checkStderr({ "Processing", pathString(inputPath.filename()) }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0);
    });

    auto outputPath = inputPath;
    outputPath.replace_extension(".musicxml");
    pugi::xml_document document;
    const auto parseResult = document.load_file(outputPath.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();

    const auto tempoDirections = document.select_nodes("//direction[sound[@tempo='120']]");
    ASSERT_FALSE(tempoDirections.empty());
    for (const auto& tempoDirection : tempoDirections) {
        const auto direction = tempoDirection.node();
        EXPECT_FALSE(direction.select_node("direction-type/symbol"));
        const auto words = direction.select_node("direction-type/words").node();
        ASSERT_TRUE(words);
        EXPECT_STREQ(words.child_value(), "Tempo (∞=120)");
        EXPECT_STREQ(words.attribute("font-family").value(), "Patmm, text");
    }
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
                    // directionRunText renders a music-font glyph as {glyphName} rather than dropping
                    // it, which directionWords would do. A measure text carrying one splits into
                    // several run items, so neither the text nor the direction itself may be taken
                    // from a lone <words>.
                    const auto text = directionRunText(direction);
                    const auto position = directionRunPosition(direction);
                    if (text.empty() || !position || direction.isSoundDataSpecified) {
                        continue;
                    }
                    const auto drawnTick = directionDrawnTick(direction);
                    result.push_back({
                        measureIndex,
                        text,
                        direction.placement,
                        position->isDefaultXSpecified ? std::make_optional(position->defaultX) : std::nullopt,
                        position->isDefaultYSpecified ? std::make_optional(position->defaultY) : std::nullopt,
                        position->isRelativeXSpecified ? std::make_optional(position->relativeX) : std::nullopt,
                        drawnTick > 0 ? std::make_optional(drawnTick) : std::nullopt
                    });
                }
            }
        }
        return result;
    };

    const auto actualDirections = collectMeasureTextDirections(*actualScore);
    const auto referenceDirections = collectMeasureTextDirections(*referenceScore);

    ASSERT_EQ(actualDirections.size(), 4u);
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

    // The glyph-bearing measure text is the only fixture exercising symbol splitting on this path.
    // Finale splits it the same way and names the same glyphs, which is what the text comparison
    // above pins. The sizes agree too: Maestro spans four staff spaces to the em, the same as a
    // SMuFL font, so smufl_mapping reports a ratio of 1 and the source sizes carry across unchanged.
    // Finale rounds what it writes and Denigma does not, which is all the tolerance below absorbs.
    // The family is still dropped, since a SMuFL glyph name means nothing in Maestro, and style and
    // weight are stated outright so the glyph cannot inherit bold or italic from the run before it.
    const auto collectSymbols = [](const mx::api::ScoreData& score) {
        std::vector<mx::api::SymbolData> result;
        for (const auto& measure : score.parts.front().measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& direction : staff.directions) {
                    for (const auto& choice : direction.directionTypes) {
                        if (!choice.isWordsRun()) {
                            continue;
                        }
                        for (const auto& item : choice.wordsRun()) {
                            if (item.isSymbol()) {
                                result.emplace_back(item.symbol());
                            }
                        }
                    }
                }
            }
        }
        return result;
    };

    const auto actualSymbols = collectSymbols(*actualScore);
    const auto referenceSymbols = collectSymbols(*referenceScore);

    ASSERT_EQ(actualSymbols.size(), 2u);
    ASSERT_EQ(referenceSymbols.size(), actualSymbols.size());
    EXPECT_EQ(actualSymbols[0].smufl, "unpitchedPercussionClef1");
    EXPECT_EQ(actualSymbols[1].smufl, "repeat2Bars");

    constexpr double kFinaleRoundingTolerance = 0.05;
    for (size_t index = 0; index < actualSymbols.size(); ++index) {
        const auto& actual = actualSymbols[index];
        const auto& reference = referenceSymbols[index];
        EXPECT_EQ(actual.smufl, reference.smufl);
        EXPECT_TRUE(actual.fontData.fontFamily.empty()) << actual.smufl;
        EXPECT_EQ(actual.fontData.style, mx::api::FontStyle::normal) << actual.smufl;
        EXPECT_EQ(actual.fontData.weight, mx::api::FontWeight::normal) << actual.smufl;
        ASSERT_EQ(actual.fontData.sizeType, mx::api::FontSizeType::point) << actual.smufl;
        ASSERT_EQ(reference.fontData.sizeType, mx::api::FontSizeType::point) << actual.smufl;
        EXPECT_NEAR(actual.fontData.sizePoint, reference.fontData.sizePoint, kFinaleRoundingTolerance)
            << actual.smufl;
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
                    const auto words = directionWords(direction);
                    if (words.empty()) {
                        continue;
                    }
                    EXPECT_EQ(words.size(), 1u);
                    if (words.size() != 1u) {
                        continue;
                    }
                    ComparableTechniqueDirection comparable{
                        words.front().text,
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

    const auto outputPath = exportMusicXmlFixture("enclosures.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    const auto actualEnclosures = collectExpressionEnclosures(*actualScore, [](const std::string& text) {
        return text == "Tempo" || text == "expressive" || text == "pizz." || text == "Reh. 1";
    });

    const std::vector<ComparableExpressionEnclosure> expected = {
        { 0u, 0, mx::api::Placement::above, "Tempo", mx::api::Enclosure::hexagon },
        { 0u, 16, mx::api::Placement::below, "expressive", mx::api::Enclosure::unspecified },
        { 1u, 8, mx::api::Placement::above, "pizz.", mx::api::Enclosure::rectangle },
        { 2u, 0, mx::api::Placement::above, "Reh. 1", mx::api::Enclosure::oval }
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

    const auto outputPath = exportMusicXmlFixture("enclosures.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);

    const auto actualEnclosures = collectExpressionEnclosures(*actualScore, [](const std::string& text) {
        return text == "no enclosure" || text == "has enclosure";
    });

    const std::vector<ComparableExpressionEnclosure> expected = {
        { 2u, 16, mx::api::Placement::below, "no enclosure", mx::api::Enclosure::unspecified },
        { 3u, 3, mx::api::Placement::above, "has enclosure", mx::api::Enclosure::rectangle }
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

TEST(MusicXmlExpressions, TextExpressionsExportSourceJustification)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("enclosures.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // TextExpressionDef::horzExprJustification reaches both <words justify="..."> and
    // <rehearsal justify="...">, independently of the halign that positions the marking.
    std::unordered_map<std::string, mx::api::HorizontalAlignment> justifyByText;
    for (const auto& measure : actualScore->parts.front().measures) {
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                for (const auto& word : directionWords(direction)) {
                    justifyByText.emplace(word.text, word.justify);
                }
                for (const auto& rehearsal : directionRehearsals(direction)) {
                    justifyByText.emplace(rehearsal.text, rehearsal.justify);
                }
            }
        }
    }

    for (const auto& text : { "Tempo", "expressive", "pizz.", "Reh. 1" }) {
        const auto found = justifyByText.find(text);
        ASSERT_NE(found, justifyByText.end()) << text;
        EXPECT_EQ(found->second, mx::api::HorizontalAlignment::left) << text;
    }
}

TEST(MusicXmlExpressions, JumpTextExportsJustifyAndHalign)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("repeats.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // Finale gives text repeats one justification setting and writes it as both attributes.
    std::vector<mx::api::WordsData> jumpWords;
    for (const auto& measure : actualScore->parts.front().measures) {
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                for (const auto& word : directionWords(direction)) {
                    jumpWords.emplace_back(word);
                }
            }
        }
    }

    ASSERT_EQ(jumpWords.size(), 1u);
    EXPECT_EQ(jumpWords.front().text, "Segno");
    EXPECT_EQ(jumpWords.front().justify, mx::api::HorizontalAlignment::right);
    EXPECT_EQ(jumpWords.front().positionData.horizontalAlignment, mx::api::HorizontalAlignment::right);
}

TEST(MusicXmlExpressions, TopStaffExpressionsExportOnlyTopSystemRelation)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("rehearsal_marks.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // Every rehearsal mark in this fixture is assigned to Finale's floating TOP staff, which is
    // MusicXML system="only-top" with no staff of its own.
    size_t rehearsalCount = 0;
    for (const auto& measure : actualScore->parts.front().measures) {
        for (const auto& staff : measure.staves) {
            for (const auto& direction : staff.directions) {
                if (directionRehearsals(direction).empty()) {
                    continue;
                }
                ++rehearsalCount;
                EXPECT_EQ(direction.systemRelation, mx::api::SystemRelation::onlyTop)
                    << "rehearsal " << rehearsalCount;
                EXPECT_FALSE(direction.isStaffValueSpecified) << "rehearsal " << rehearsalCount;
            }
        }
    }
    EXPECT_GT(rehearsalCount, 0u);
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

TEST(MusicXmlExpressions, DynamicsKeepTheWordsAroundThem)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("dynamics_hairpins.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // Measures 4 and 5 spell each dynamic with a word beside its glyph, so every direction must
    // carry both the mark and the word: "più f", "sub. p", "ff sempre", "menos f".
    const std::vector<std::pair<std::string, mx::api::StandardDynamic>> expected = {
        { "più", mx::api::StandardDynamic::f },
        { "sub.", mx::api::StandardDynamic::p },
        { "sempre", mx::api::StandardDynamic::ff },
        { "menos", mx::api::StandardDynamic::f },
    };

    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 5u);
    std::vector<std::pair<std::string, mx::api::StandardDynamic>> actual;
    for (size_t measureIndex = 3; measureIndex <= 4; ++measureIndex) {
        for (const auto& staff : measures.at(measureIndex).staves) {
            for (const auto& direction : staff.directions) {
                const auto marks = directionMarks(direction);
                const auto words = directionWords(direction);
                ASSERT_EQ(marks.size(), 1u) << "measure " << (measureIndex + 1);
                ASSERT_EQ(words.size(), 1u) << "measure " << (measureIndex + 1);
                ASSERT_TRUE(marks.front().choice.isDynamic()) << "measure " << (measureIndex + 1);
                actual.emplace_back(utils::trimAscii(words.front().text), marks.front().choice.dynamic());
            }
        }
    }

    EXPECT_EQ(actual, expected);
}

TEST(MusicXmlExpressions, TheDynamicElementTableKnowsEveryDynamic)
{
    // musicXmlStandardDynamic walks a switch over mx::api::StandardDynamic, so -Wswitch fails the
    // build if mx adds a dynamic, and mx supplies the letters so they cannot drift from the element
    // names it writes. What the switch cannot state is that its chain reaches every value: a
    // mis-spliced one would skip a dynamic, or cycle. MusicXML defines 26 dynamic elements, which
    // is a fact about the format rather than a restatement of that chain.
    EXPECT_EQ(formats::musicxml::detail::musicXmlStandardDynamicCount(), musicXmlDynamicElementCount);
}

TEST(MusicXmlExpressions, CompoundDynamicsSpellOutTheirComponents)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("dynamics_hairpins.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // A marking outside MusicXML's dynamic vocabulary writes one <dynamics> element whose children
    // spell it out, instead of being flattened into one text-valued <other-dynamics>.
    const std::vector<std::vector<ComparableDynamicsComponent>> expected = {
        { // "sffffz", drawn as the glyphs s, ffff, z
            { std::nullopt, "s", "dynamicSforzando" },
            { mx::api::StandardDynamic::ffff, "", std::nullopt },
            { std::nullopt, "z", "dynamicZ" }
        },
        { // "ffz", drawn as the glyphs ff, z
            { mx::api::StandardDynamic::ff, "", std::nullopt },
            { std::nullopt, "z", "dynamicZ" }
        },
    };

    EXPECT_EQ(collectCompoundDynamics(*actualScore), expected);
}

TEST(MusicXmlExpressions, CompoundDynamicsFollowTheSourceGlyphSequence)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("ffz.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // The fixture spells "sfmp" twice, first with the composite dynamicSforzando1 ("sf") and
    // dynamicMP ("mp") glyphs and then with four separate letters. Each spelling exports as
    // itself: one component per source glyph, using a MusicXML dynamic element whenever the
    // glyph's letters name one.
    const std::vector<std::vector<ComparableDynamicsComponent>> expected = {
        { // "sfmp" as two composite glyphs
            { mx::api::StandardDynamic::sf, "", std::nullopt },
            { mx::api::StandardDynamic::mp, "", std::nullopt }
        },
        { // "sfmp" as four letter glyphs; "s" and "m" have no element of their own
            { std::nullopt, "s", "dynamicSforzando" },
            { mx::api::StandardDynamic::f, "", std::nullopt },
            { std::nullopt, "m", "dynamicMezzo" },
            { mx::api::StandardDynamic::p, "", std::nullopt }
        },
        { // "ffz" as the glyphs ff, z
            { mx::api::StandardDynamic::ff, "", std::nullopt },
            { std::nullopt, "z", "dynamicZ" }
        },
        { // "sfmp" typed as ASCII letters, which resolve to no glyphs at all
          // (see GlyphlessDynamicsFallBackToTheirLetters)
            { std::nullopt, "sfmp", std::nullopt }
        },
    };

    EXPECT_EQ(collectCompoundDynamics(*actualScore), expected);
}

TEST(MusicXmlExpressions, GlyphlessDynamicsFallBackToTheirLetters)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("ffz.musx");
    const auto actualScore = loadScoreData(outputPath);
    ASSERT_TRUE(actualScore);
    ASSERT_FALSE(actualScore->parts.empty());

    // Bar 2, beat 3 spells "sfmp" as plain ASCII in a text font, so it resolves to no SMuFL
    // glyphs and there is no glyph sequence to follow. A marking is a dynamic because of how it
    // is spelled rather than because of its font, so it still exports as a dynamic, falling back
    // to its letters with no glyph name. Finale's own export demotes this marking to
    // <words font-family="Times New Roman">sfmp</words>.
    const auto& measures = actualScore->parts.front().measures;
    ASSERT_GE(measures.size(), 2u);

    std::vector<std::pair<std::string, std::vector<ComparableDynamicsComponent>>> marks;
    size_t wordsCount = 0;
    for (const auto& staff : measures.at(1).staves) {
        for (const auto& direction : staff.directions) {
            for (const auto& mark : directionMarks(direction)) {
                ASSERT_EQ(mark.markType, mx::api::MarkType::dynamics);
                marks.emplace_back(mark.name, dynamicsComponents(mark));
            }
            wordsCount += directionWords(direction).size();
        }
    }

    const std::vector<std::pair<std::string, std::vector<ComparableDynamicsComponent>>> expected = {
        // "ffz", drawn as the glyphs ff and z, keeps its glyph sequence.
        { "ffz", { { mx::api::StandardDynamic::ff, "", std::nullopt }, { std::nullopt, "z", "dynamicZ" } } },
        // "sfmp", typed as ASCII letters, carries its letters and no glyph name.
        { "sfmp", { { std::nullopt, "sfmp", std::nullopt } } }
    };
    EXPECT_EQ(marks, expected);
    EXPECT_EQ(wordsCount, 0u);
}

} // namespace denigma::test::musicxml
