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
#include "denigma/formats/musicxml.h"

#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "core/denigma.h"
#include "musicxml.h"

namespace denigma {
namespace formats {
namespace musicxml {

namespace {

Buffer copyBytes(std::span<const std::byte> input)
{
    Buffer buffer;
    buffer.reserve(input.size());
    for (const std::byte value : input) {
        buffer.push_back(static_cast<char>(value));
    }
    return buffer;
}

DenigmaContext makeMusicXmlContext(const Options& options, const std::filesystem::path& defaultSourceName)
{
    DenigmaContext context("denigma");
    context.inputFilePath = options.common.sourceName.empty()
        ? defaultSourceName
        : utils::utf8ToPath(options.common.sourceName);
    context.noValidate = !options.common.validate;
    context.verbose = options.common.verbose;
    context.quiet = options.common.quiet;
    context.includeTempoTool = options.includeTempoTool;
    return context;
}

} // namespace

ConversionResult EnigmaXmlToMusicXmlConverter::convert(std::span<const std::byte> input,
                                                       std::ostream& output,
                                                       const Options& options) const
{
    ConversionResult result;
    auto buffer = copyBytes(input);
    auto context = makeMusicXmlContext(options, "input.enigmaxml");
    context.logCallback = options.common.logCallback;
    context.conversionResult = &result;

    try {
        detail::exportMusicXml(output, CommandInputData{ std::move(buffer), std::nullopt, {} }, context);
    } catch (const std::exception& ex) {
        context.logMessage(LogMsg() << "unable to convert Enigma XML to MusicXML", MessageSeverity::Error);
        context.logMessage(LogMsg() << " (exception: " << ex.what() << ")", MessageSeverity::Error);
    }
    return result;
}

ConversionResult EnigmaXmlToMusicXmlConverter::convert(std::span<const std::byte> input,
                                                       std::ostream& output,
                                                       const ConversionRequest& request) const
{
    return convert(input, output, optionsFromRequest<Options>(request, "EnigmaXmlToMusicXmlConverter"));
}

void registerConverters(ConverterRegistry& registry)
{
    registry.add(std::make_unique<EnigmaXmlToMusicXmlConverter>());
}

} // namespace musicxml
} // namespace formats
} // namespace denigma
