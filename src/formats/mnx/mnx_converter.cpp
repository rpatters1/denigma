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

#include "core/denigma.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "mnx.h"
#include "utils/stringutils.h"

namespace denigma::formats::mnx {

namespace {

DenigmaContext makeMnxContext(const Options& options, const std::filesystem::path& defaultSourceName)
{
    DenigmaContext context("denigma");
    context.inputFilePath = options.common.sourceName.empty()
        ? defaultSourceName
        : utils::utf8ToPath(options.common.sourceName);
    context.noValidate = !options.common.validate;
    context.verbose = options.common.verbose;
    context.quiet = options.common.quiet;
    context.indentSpaces = options.indentSpaces;
    context.cueLayer = options.cueLayer;
    context.mnxSchema = options.schema;
    context.includeTempoTool = options.includeTempoTool;
    context.mnxSplitInstruments = options.splitInstruments;
    return context;
}

} // namespace

ConversionResult EnigmaXmlToMnxJsonConverter::convert(std::span<const std::byte> input,
                                                      std::ostream& output,
                                                      const Options& options) const
{
    Buffer buffer;
    buffer.reserve(input.size());
    for (const std::byte value : input) {
        buffer.push_back(static_cast<char>(value));
    }

    auto context = makeMnxContext(options, "input.enigmaxml");
    context.logCallback = options.common.logCallback;
    MusxLoggerScope musxLogger(makeMusxLogCallback(context));

    mnxexp::exportJson(output, CommandInputData{ std::move(buffer), std::nullopt, {} }, context);
    return {};
}

ConversionResult EnigmaXmlToMnxJsonConverter::convert(std::span<const std::byte> input,
                                                      std::ostream& output,
                                                      const ConversionRequest& request) const
{
    return convert(input, output, optionsFromRequest<Options>(request, "EnigmaXmlToMnxJsonConverter"));
}

ConversionResult MusxToMnxJsonConverter::convert(const IRandomAccessReader& input,
                                                 std::ostream& output,
                                                 const Options& options) const
{
    auto context = makeMnxContext(options, "input.musx");
    context.logCallback = options.common.logCallback;
    MusxLoggerScope musxLogger(makeMusxLogCallback(context));

    mnxexp::exportJson(output, enigmaxml::extractMusxInputData(input, context), context);
    return {};
}

ConversionResult MusxToMnxJsonConverter::convert(const IRandomAccessReader& input,
                                                 std::ostream& output,
                                                 const ConversionRequest& request) const
{
    return convert(input, output, optionsFromRequest<Options>(request, "MusxToMnxJsonConverter"));
}

void registerConverters(ConverterRegistry& registry)
{
    registry.add(std::make_unique<EnigmaXmlToMnxJsonConverter>());
    registry.add(std::make_unique<MusxToMnxJsonConverter>());
}

} // namespace denigma::formats::mnx
