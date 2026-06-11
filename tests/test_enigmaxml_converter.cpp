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
#include <sstream>
#include <span>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "denigma/formats/enigmaxml.h"
#include "denigma/io/random_access_reader.h"
#include "test_utils.h"

TEST(ConverterApi, MusxToEnigmaXmlWritesToStream)
{
    setupTestDataPaths();

    denigma::ConverterRegistry registry;
    denigma::formats::enigmaxml::registerConverters(registry);
    const auto* converter = registry.findReader(denigma::FormatId::Musx, denigma::FormatId::EnigmaXml);
    ASSERT_NE(converter, nullptr);

    denigma::ConversionOptions options;
    options.sourceName = "notAscii-其れ.musx";

    denigma::FileRandomAccessReader fileReader(getInputPath() / "notAscii-其れ.musx");
    std::ostringstream fileOutput;
    const auto fileResult = converter->convert(fileReader, fileOutput, options);
    EXPECT_TRUE(fileResult.diagnostics.empty());

    std::vector<char> musxInput;
    readFile(getInputPath() / "notAscii-其れ.musx", musxInput);
    denigma::BufferRandomAccessReader bufferReader(std::as_bytes(std::span<const char>(musxInput.data(), musxInput.size())));
    std::ostringstream bufferOutput;
    const auto bufferResult = converter->convert(bufferReader, bufferOutput, options);
    EXPECT_TRUE(bufferResult.diagnostics.empty());

    std::vector<char> reference;
    readFile(getInputPath() / "reference" / "notAscii-其れ.enigmaxml", reference);
    const std::string referenceText(reference.begin(), reference.end());

    EXPECT_EQ(fileOutput.str(), referenceText);
    EXPECT_EQ(bufferOutput.str(), referenceText);
}
