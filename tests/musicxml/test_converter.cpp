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
#include <cstddef>
#include <cstring>
#include <sstream>
#include <span>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "pugixml.hpp"

#include "denigma/formats/musicxml.h"
#include "denigma/io/random_access_reader.h"
#include "musicxml_test.h"
#include "test_utils.h"

TEST(ConverterApi, EnigmaXmlToMusicXmlWritesToStream)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(denigma::FormatId::EnigmaXml, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.enigmaxml";
    const auto result = converter->convert(std::as_bytes(std::span<const char>(input.data(), input.size())),
                                           [&](std::string_view, std::span<const std::byte> data) {
                                               xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
                                           },
                                           denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_TRUE(document.child("score-partwise"));
}

TEST(ConverterApi, MusxToMusicXmlWritesToStream)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / utils::utf8ToPath("notAscii-其れ.musx"));
    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.musx";
    const auto result = converter->convert(input,
                                           [&](std::string_view, std::span<const std::byte> data) {
                                               xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
                                           },
                                           denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_TRUE(document.child("score-partwise"));
}

TEST(ConverterApi, MusxToMusicXmlPreservesTextWhenAllFontsAreAvailable)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / "tempo_varied_staves.musx");
    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "tempo_varied_staves.musx";
    options.common.allFontsAvailable = true;
    const auto result = converter->convert(input, [&](std::string_view, std::span<const std::byte> data) {
        xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
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

TEST(ConverterApi, MusxToMusicXmlInvokesOutputCallbackForParts)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    struct Output
    {
        std::string suggestedName;
        std::string data;
    };

    std::vector<Output> outputs;
    denigma::FileRandomAccessReader input(getInputPath() / utils::utf8ToPath("notAscii-其れ.musx"));
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "notAscii-其れ.musx";
    options.allPartsAndScore = true;
    const auto result = converter->convert(input, [&](std::string_view suggestedName, std::span<const std::byte> data) {
        std::string outputData;
        outputData.resize(data.size());
        std::memcpy(outputData.data(), data.data(), data.size());
        outputs.push_back(Output{ std::string(suggestedName), std::move(outputData) });
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_GE(outputs.size(), 2);
    EXPECT_EQ(outputs.front().suggestedName, "");

    bool foundNamedPart = false;
    for (const auto& output : outputs) {
        pugi::xml_document document;
        const auto parseResult = document.load_string(output.data.c_str());
        ASSERT_TRUE(parseResult) << parseResult.description();
        EXPECT_TRUE(document.child("score-partwise"));
        foundNamedPart = foundNamedPart || output.suggestedName == "オボえ";
    }
    EXPECT_TRUE(foundNamedPart);
}

TEST(ConverterApi, MusxToMusicXmlEmitsCuesOnlyWhereVisible)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::vector<std::string> outputs;
    denigma::FileRandomAccessReader input(getInputPath() / "multimeas_cue.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "multimeas_cue.musx";
    options.allPartsAndScore = true;
    const auto result = converter->convert(input, [&](std::string_view, std::span<const std::byte> data) {
        outputs.emplace_back(reinterpret_cast<const char*>(data.data()), data.size());
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    ASSERT_EQ(outputs.size(), 3u);

    size_t outputsWithCues{};
    for (size_t outputIndex = 0; outputIndex < outputs.size(); ++outputIndex) {
        pugi::xml_document document;
        const auto parseResult = document.load_string(outputs[outputIndex].c_str());
        ASSERT_TRUE(parseResult) << parseResult.description();
        const auto cueNotes = document.select_nodes("//note[cue]");
        if (outputIndex == 0) {
            EXPECT_TRUE(cueNotes.empty()) << "Cues hidden in the score context must be suppressed.";
        }
        if (!cueNotes.empty()) {
            ++outputsWithCues;
            EXPECT_EQ(cueNotes.size(), 8u);
            EXPECT_EQ(document.select_nodes("//note[cue and rest]").size(), 1u);
            EXPECT_EQ(document.select_nodes("//note[cue and pitch]").size(), 7u);
            const auto cueNoteSize = document.select_node("//defaults/appearance/note-size[@type='cue']").node();
            ASSERT_TRUE(cueNoteSize);
            EXPECT_EQ(cueNoteSize.text().as_int(), 81);
        }
    }
    EXPECT_EQ(outputsWithCues, 1u);
}

TEST(ConverterApi, MusxToMusicXmlPlumbsForcedCueLayer)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "forced_bass_clef.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "forced_bass_clef.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(input, [&](std::string_view, std::span<const std::byte> data) {
        xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[cue]").size(), 1u);
    EXPECT_EQ(document.select_nodes("//note[cue and rest]").size(), 1u);
}

TEST(ConverterApi, MusxToMusicXmlRetainsForcedCueLayerExpressions)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "techniques.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "techniques.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(input, [&](std::string_view, std::span<const std::byte> data) {
        xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[cue]").size(), 27u);
    EXPECT_EQ(document.select_nodes("//direction[voice='1']").size(), 5u);
    EXPECT_EQ(document.select_nodes("//direction[voice='1']/direction-type/words").size(), 5u);
}

TEST(ConverterApi, MusxToMusicXmlRetainsForcedCueLayerTiesAsNotationOnly)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::FileRandomAccessReader input(getInputPath() / "tie_target_types.musx");
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "tie_target_types.musx";
    options.cueLayer = 1;
    const auto result = converter->convert(input, [&](std::string_view, std::span<const std::byte> data) {
        xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
    }, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_FALSE(document.select_nodes("//note[cue]/notations/tied[@type='start']").empty());
    EXPECT_FALSE(document.select_nodes("//note[cue]/notations/tied[@type='stop']").empty());
    EXPECT_TRUE(document.select_nodes("//note[cue]/tie").empty());
}

TEST(ConverterApi, EnigmaXmlToMusicXmlRetainsForcedGraceCueTieAsNotationOnly)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);

    pugi::xml_document inputDocument;
    const auto inputParseResult = inputDocument.load_buffer(input.data(), input.size());
    ASSERT_TRUE(inputParseResult) << inputParseResult.description();
    auto graceTieStartNote = inputDocument.select_node("//entry[@entnum='17']/note").node();
    auto tieEndNote = inputDocument.select_node("//entry[@entnum='5']/note").node();
    ASSERT_TRUE(graceTieStartNote);
    ASSERT_TRUE(tieEndNote);
    graceTieStartNote.append_child("tieStart");
    tieEndNote.append_child("tieEnd");

    std::ostringstream modifiedInput;
    inputDocument.save(modifiedInput);
    const auto modifiedInputText = modifiedInput.str();

    denigma::ConverterRegistry registry;
    denigma::formats::musicxml::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(denigma::FormatId::EnigmaXml, denigma::FormatId::MusicXml);
    ASSERT_NE(converter, nullptr);

    std::string xmlText;
    denigma::formats::musicxml::Options options;
    options.common.sourceName = "grace-cue-tie.enigmaxml";
    options.cueLayer = 1;
    const auto result = converter->convert(std::as_bytes(std::span<const char>(modifiedInputText.data(), modifiedInputText.size())),
                                           [&](std::string_view, std::span<const std::byte> data) {
                                               xmlText.assign(reinterpret_cast<const char*>(data.data()), data.size());
                                           },
                                           denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics().empty());
    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    EXPECT_EQ(document.select_nodes("//note[grace and cue]/notations/tied[@type='start']").size(), 1u);
    EXPECT_TRUE(document.select_nodes("//note[grace and cue]/tie").empty());
}

TEST(MusicXmlChordFixture, ExportsChordsForInspection)
{
    setupTestDataPaths();

    const auto outputPath = denigma::test::musicxml::exportMusicXmlFixture("chords.musx");
    EXPECT_TRUE(std::filesystem::exists(outputPath));

    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(outputPath.c_str()));
    const auto firstKind = document.select_node("//harmony[1]/kind").node();
    ASSERT_TRUE(firstKind);
    EXPECT_STREQ(firstKind.child_value(), "major");
    EXPECT_FALSE(firstKind.attribute("text"));
}

TEST(MusicXmlChordFixture, WritesDegreeLayoutAttributesFromTheSuffix)
{
    setupTestDataPaths();

    const auto outputPath = denigma::test::musicxml::exportMusicXmlFixture("chords.musx");
    pugi::xml_document document;
    ASSERT_TRUE(document.load_file(outputPath.c_str()));

    // The parentheses in "m(maj7)" belong to the chord kind, not to a degree group, so the suffix
    // must not claim parenthesized degrees. Finale's own export of this fixture agrees.
    const auto majorMinor = document.select_node("//kind[@text='m(maj7)']").node();
    ASSERT_TRUE(majorMinor);
    EXPECT_FALSE(majorMinor.attribute("parentheses-degrees"));
    EXPECT_FALSE(majorMinor.attribute("stack-degrees"));
    EXPECT_FALSE(majorMinor.parent().child("degree"));

    // A parenthesized alteration is a degree group, whether or not the parentheses wrap the whole
    // suffix, but only vertically offset degrees are stacked.
    const auto parenthesized = document.select_node("//kind[@parentheses-degrees='yes']").node();
    ASSERT_TRUE(parenthesized);
    EXPECT_TRUE(parenthesized.parent().child("degree"));

    const auto stacked = document.select_node("//kind[@stack-degrees='yes']").node();
    ASSERT_TRUE(stacked);
    EXPECT_STREQ(stacked.attribute("parentheses-degrees").value(), "yes");
    EXPECT_TRUE(stacked.parent().child("degree"));

    const auto inlineDegrees = document.select_node("//kind[@text='+(add♯9add♭9)']").node();
    ASSERT_TRUE(inlineDegrees);
    EXPECT_STREQ(inlineDegrees.attribute("parentheses-degrees").value(), "yes");
    EXPECT_FALSE(inlineDegrees.attribute("stack-degrees"));

    // The seventh of "7sus4" sounds but is already spelled out by the kind text, so it must not
    // print a second time. Degrees the suffix does show carry no print-object at all.
    const auto suspended = document.select_node("//kind[@text='7sus4-3']").node();
    ASSERT_TRUE(suspended);
    const auto impliedDegree = suspended.parent().child("degree");
    ASSERT_TRUE(impliedDegree);
    EXPECT_STREQ(impliedDegree.child_value("degree-value"), "7");
    EXPECT_STREQ(impliedDegree.attribute("print-object").value(), "no");
    EXPECT_FALSE(parenthesized.parent().child("degree").attribute("print-object"));
}
