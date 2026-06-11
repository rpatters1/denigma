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
#include <cstddef>
#include <sstream>
#include <span>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "denigma/formats/mnx.h"
#include "denigma/io/random_access_reader.h"
#include "test_utils.h"

TEST(ConverterApi, EnigmaXmlToMnxJsonWritesToStream)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / "notAscii-其れ.enigmaxml", input);

    denigma::ConverterRegistry registry;
    denigma::formats::mnx::registerConverters(registry);
    const auto* converter = registry.find(denigma::FormatId::EnigmaXml, denigma::FormatId::MnxJson);
    ASSERT_NE(converter, nullptr);

    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "notAscii-其れ.enigmaxml";
    options.common.validate = false;
    options.indentSpaces = 2;
    const auto result = converter->convert(std::as_bytes(std::span<const char>(input.data(), input.size())),
                                           output,
                                           denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics.empty());

    const std::string jsonText = output.str();
    ASSERT_FALSE(jsonText.empty());
    const auto json = nlohmann::json::parse(jsonText);
    ASSERT_TRUE(json.contains("mnx"));
    ASSERT_TRUE(json.contains("global"));
    ASSERT_TRUE(json.contains("parts"));
}

TEST(ConverterApi, MusxToMnxJsonWritesToStream)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::mnx::registerConverters(registry);
    const auto* converter = registry.findReader(denigma::FormatId::Musx, denigma::FormatId::MnxJson);
    ASSERT_NE(converter, nullptr);

    denigma::FileRandomAccessReader input(getInputPath() / "notAscii-其れ.musx");
    std::ostringstream output;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "notAscii-其れ.musx";
    options.common.validate = false;
    options.indentSpaces = 2;
    const auto result = converter->convert(input, output, denigma::ConversionRequest{ &options });

    EXPECT_TRUE(result.diagnostics.empty());

    const std::string jsonText = output.str();
    ASSERT_FALSE(jsonText.empty());
    const auto json = nlohmann::json::parse(jsonText);
    ASSERT_TRUE(json.contains("mnx"));
    ASSERT_TRUE(json.contains("global"));
    ASSERT_TRUE(json.contains("parts"));
}
