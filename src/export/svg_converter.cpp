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
#include "denigma/formats/svg.h"

#include <filesystem>
#include <memory>
#include <vector>

#include "denigma.h"
#include "export/enigmaxml.h"
#include "export/svg.h"

namespace denigma::formats::svg {

namespace {

musx::util::SvgConvert::SvgUnit toMusxSvgUnit(SvgUnit unit)
{
    switch (unit) {
    case SvgUnit::None:
        return musx::util::SvgConvert::SvgUnit::None;
    case SvgUnit::Pixels:
        return musx::util::SvgConvert::SvgUnit::Pixels;
    case SvgUnit::Points:
        return musx::util::SvgConvert::SvgUnit::Points;
    case SvgUnit::Picas:
        return musx::util::SvgConvert::SvgUnit::Picas;
    case SvgUnit::Centimeters:
        return musx::util::SvgConvert::SvgUnit::Centimeters;
    case SvgUnit::Millimeters:
        return musx::util::SvgConvert::SvgUnit::Millimeters;
    case SvgUnit::Inches:
        return musx::util::SvgConvert::SvgUnit::Inches;
    }
    return musx::util::SvgConvert::SvgUnit::Points;
}

void applySvgOptions(DenigmaContext& context, const ConversionOptions& options)
{
    context.noValidate = !options.validate;
    context.svgUnit = toMusxSvgUnit(options.svgUnit);
    context.svgScale = options.svgScale;
    context.svgUsePageScale = options.svgUsePageScale;
    context.svgShapeDefs.reserve(options.svgShapeDefs.size());
    for (const int shapeDef : options.svgShapeDefs) {
        context.svgShapeDefs.push_back(static_cast<musx::dom::Cmper>(shapeDef));
    }
}

} // namespace

ConversionResult EnigmaXmlToSvgConverter::convert(std::span<const std::byte> input,
                                                  const MultiOutputCallback& outputCallback,
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
    applySvgOptions(context, options);

    svgexp::convert(CommandInputData{ std::move(buffer), std::nullopt, {} }, context, outputCallback);
    return {};
}

ConversionResult MusxToSvgConverter::convert(const IRandomAccessReader& input,
                                             const MultiOutputCallback& outputCallback,
                                             const ConversionOptions& options) const
{
    DenigmaContext context("denigma");
    context.inputFilePath = options.sourceName.empty()
        ? std::filesystem::path("input.musx")
        : std::filesystem::path(options.sourceName);
    applySvgOptions(context, options);

    svgexp::convert(enigmaxml::extractInputData(input, context), context, outputCallback);
    return {};
}

void registerConverters(ConverterRegistry& registry)
{
    registry.add(std::make_unique<EnigmaXmlToSvgConverter>());
    registry.add(std::make_unique<MusxToSvgConverter>());
}

} // namespace denigma::formats::svg
