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
#include <ostream>
#include <sstream>
#include <span>
#include <vector>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "denigma/formats/mnx.h"
#include "denigma/formats/mss.h"
#include "denigma/formats/svg.h"
#include "test_utils.h"

namespace {

template <typename OptionsT>
concept MnxJsonOptionsForConcreteConverter = requires(const denigma::formats::mnx::EnigmaXmlToMnxJsonConverter& converter,
                                                      std::span<const std::byte> input,
                                                      std::ostream& output,
                                                      const OptionsT& options) {
    converter.convert(input, output, options);
};

static_assert(MnxJsonOptionsForConcreteConverter<denigma::formats::mnx::Options>);
static_assert(!MnxJsonOptionsForConcreteConverter<denigma::formats::svg::Options>);
static_assert(!MnxJsonOptionsForConcreteConverter<denigma::formats::mss::Options>);

} // namespace

TEST(ConverterApi, ConcreteConverterAcceptsTypedOptions)
{
    setupTestDataPaths();

    std::vector<char> input;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), input);

    denigma::formats::mnx::EnigmaXmlToMnxJsonConverter converter;
    denigma::formats::mnx::Options options;
    options.common.sourceName = "notAscii-其れ.enigmaxml";
    options.common.validate = false;
    options.indentSpaces = 2;

    std::ostringstream output;
    const auto result = converter.convert(std::as_bytes(std::span<const char>(input.data(), input.size())),
                                          output,
                                          options);

    EXPECT_TRUE(result.diagnostics().empty());

    const auto json = nlohmann::json::parse(output.str());
    EXPECT_TRUE(json.contains("mnx"));
}
