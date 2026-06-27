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

namespace denigma {
namespace formats {
namespace mnx {

namespace {

DenigmaContext makeMnxContext(const Options& options, const std::filesystem::path& defaultSourceName)
{
    DenigmaContext context(DENIGMA_NAME);
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
    ConversionResult result;
    Buffer buffer;
    buffer.reserve(input.size());
    for (const std::byte value : input) {
        buffer.push_back(static_cast<char>(value));
    }

    auto context = makeMnxContext(options, "input.enigmaxml");
    context.logCallback = options.common.logCallback;
    context.conversionResult = &result;
    MusxLoggerScope musxLogger(makeMusxLogCallback(context));

    try {
        detail::exportJson(output, CommandInputData{ std::move(buffer), std::nullopt, {} }, context);
    } catch (const std::exception& ex) {
        context.logMessage(LogMsg() << "unable to convert Enigma XML to MNX JSON", MessageSeverity::Error);
        context.logMessage(LogMsg() << " (exception: " << ex.what() << ")", MessageSeverity::Error);
    }
    return result;
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
    ConversionResult result;
    auto context = makeMnxContext(options, "input.musx");
    context.logCallback = options.common.logCallback;
    context.conversionResult = &result;
    MusxLoggerScope musxLogger(makeMusxLogCallback(context));

    try {
        detail::exportJson(output, formats::enigmaxml::detail::extractMusxInputData(input, context), context);
    } catch (const std::exception& ex) {
        context.logMessage(LogMsg() << "unable to convert MUSX to MNX JSON", MessageSeverity::Error);
        context.logMessage(LogMsg() << " (exception: " << ex.what() << ")", MessageSeverity::Error);
    }
    return result;
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

} // namespace mnx
} // namespace formats
} // namespace denigma
