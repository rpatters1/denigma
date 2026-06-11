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

#include "denigma/formats/mss.h"
#include "denigma/io/random_access_reader.h"
#include "test_utils.h"

TEST(ConverterApi, EnigmaXmlToMssXmlWritesToStream)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / "notAscii-其れ.enigmaxml", input);

    denigma::ConverterRegistry registry;
    denigma::formats::mss::registerConverters(registry);
    const auto* converter = registry.find(denigma::FormatId::EnigmaXml, denigma::FormatId::MssXml);
    ASSERT_NE(converter, nullptr);

    std::ostringstream output;
    denigma::ConversionOptions options;
    options.sourceName = "notAscii-其れ.enigmaxml";
    const auto result = converter->convert(std::as_bytes(std::span<const char>(input.data(), input.size())),
                                           output,
                                           options);

    EXPECT_TRUE(result.diagnostics.empty());

    const std::string xmlText = output.str();
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    const auto museScore = document.child("museScore");
    ASSERT_TRUE(museScore);
    EXPECT_STREQ(museScore.attribute("version").value(), "4.60");
    EXPECT_TRUE(museScore.child("Style"));
}

TEST(ConverterApi, MusxToMssXmlWritesToStream)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::mss::registerConverters(registry);
    const auto* converter = registry.findReader(denigma::FormatId::Musx, denigma::FormatId::MssXml);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / "notAscii-其れ.musx");
    std::ostringstream output;
    denigma::ConversionOptions options;
    options.sourceName = "notAscii-其れ.musx";
    const auto result = converter->convert(input, output, options);

    EXPECT_TRUE(result.diagnostics.empty());

    const std::string xmlText = output.str();
    ASSERT_FALSE(xmlText.empty());

    pugi::xml_document document;
    const auto parseResult = document.load_string(xmlText.c_str());
    ASSERT_TRUE(parseResult) << parseResult.description();
    const auto museScore = document.child("museScore");
    ASSERT_TRUE(museScore);
    EXPECT_STREQ(museScore.attribute("version").value(), "4.60");
    EXPECT_TRUE(museScore.child("Style"));
}

TEST(ConverterApi, MusxToMssXmlInvokesOutputCallbackForParts)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::mss::registerConverters(registry);
    const auto* converter = registry.findReaderMultiOutput(denigma::FormatId::Musx, denigma::FormatId::MssXml);
    ASSERT_NE(converter, nullptr);

    struct Output
    {
        std::string suggestedName;
        std::string data;
    };

    std::vector<Output> outputs;
    denigma::FileRandomAccessReader input(getInputPath() / "notAscii-其れ.musx");
    denigma::ConversionOptions options;
    options.sourceName = "notAscii-其れ.musx";
    options.mssAllPartsAndScore = true;
    const auto result = converter->convert(input, [&](std::string_view suggestedName, std::span<const std::byte> data) {
        std::string outputData;
        outputData.resize(data.size());
        std::memcpy(outputData.data(), data.data(), data.size());
        outputs.push_back(Output{ std::string(suggestedName), std::move(outputData) });
    }, options);

    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_GE(outputs.size(), 2);
    EXPECT_EQ(outputs.front().suggestedName, "");

    bool foundNamedPart = false;
    for (const auto& output : outputs) {
        pugi::xml_document document;
        const auto parseResult = document.load_string(output.data.c_str());
        ASSERT_TRUE(parseResult) << parseResult.description();
        const auto museScore = document.child("museScore");
        ASSERT_TRUE(museScore);
        EXPECT_STREQ(museScore.attribute("version").value(), "4.60");
        EXPECT_TRUE(museScore.child("Style"));
        foundNamedPart = foundNamedPart || output.suggestedName == "オボえ";
    }
    EXPECT_TRUE(foundNamedPart);
}
