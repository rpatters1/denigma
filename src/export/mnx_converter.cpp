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
#include "denigma/formats/mnx.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "denigma.h"
#include "export/mnx.h"

namespace denigma::formats::mnx {

ConversionResult EnigmaXmlToMnxJsonConverter::convert(std::span<const std::byte> input,
                                                      std::ostream& output,
                                                      const ConversionOptions& options) const
{
    Buffer buffer;
    buffer.reserve(input.size());
    for (const std::byte value : input) {
        buffer.push_back(static_cast<char>(value));
    }

    DenigmaContext context("denigma");
    context.inputFilePath = options.sourceName.empty()
        ? std::filesystem::path("input.enigmaxml")
        : std::filesystem::path(options.sourceName);
    context.noValidate = !options.validate;
    context.indentSpaces = options.indentSpaces;

    mnxexp::exportJson(output, CommandInputData{ std::move(buffer), std::nullopt, {} }, context);
    return {};
}

void registerConverters(ConverterRegistry& registry)
{
    registry.add(std::make_unique<EnigmaXmlToMnxJsonConverter>());
}

} // namespace denigma::formats::mnx
