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
#include <span>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "denigma/formats/svg.h"
#include "test_utils.h"

TEST(ConverterApi, EnigmaXmlToSvgInvokesOutputCallback)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / "notAscii-其れ.enigmaxml", input);

    denigma::ConverterRegistry registry;
    denigma::formats::svg::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(denigma::FormatId::EnigmaXml, denigma::FormatId::Svg);
    ASSERT_NE(converter, nullptr);

    denigma::ConversionOptions options;
    options.sourceName = "notAscii-其れ.enigmaxml";
    options.svgShapeDefs = { 3 };

    std::vector<std::pair<std::string, std::string>> outputs;
    const auto result = converter->convert(
        std::as_bytes(std::span<const char>(input.data(), input.size())),
        [&](std::string_view suggestedName, std::span<const std::byte> data) {
            std::string svgText;
            svgText.resize(data.size());
            std::memcpy(svgText.data(), data.data(), data.size());
            outputs.emplace_back(std::string(suggestedName), std::move(svgText));
        },
        options);

    EXPECT_TRUE(result.diagnostics.empty());
    ASSERT_EQ(outputs.size(), 1u);
    EXPECT_EQ(outputs.front().first, "shape-3.svg");
    EXPECT_NE(outputs.front().second.find("<svg"), std::string::npos);
}
