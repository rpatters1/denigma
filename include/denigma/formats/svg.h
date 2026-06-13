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
#pragma once

#include <vector>

#include "denigma/conversion.h"

namespace denigma::formats::svg {

/// Unit suffix used by SVG output dimensions.
enum class Unit
{
    None,
    Pixels,
    Points,
    Picas,
    Centimeters,
    Millimeters,
    Inches
};

/// Options for SVG converters.
struct Options final : public IOptions
{
    /// Options common to all converters.
    CommonOptions common;
    /// Unit suffix for SVG width and height output.
    Unit unit{ Unit::Points };
    /// Extra scale multiplier for SVG output when page scaling is not active.
    double scale{ 1.0 };
    /// Optional ShapeDef identifiers for SVG conversion.
    std::vector<int> shapeDefs;
    /// Use Finale page-format scaling.
    bool usePageScale{ false };
};

/// Converter adapter for Enigma XML input to zero or more SVG documents.
class EnigmaXmlToSvgConverter final : public IMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::EnigmaXml; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::Svg; }

    /// Converts Enigma XML from memory and invokes outputCallback for each SVG document.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Converts Enigma XML using type-erased registry options.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// Converter adapter for MUSX archive input to zero or more SVG documents.
class MusxToSvgConverter final : public IReaderMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::Svg; }

    /// Extracts a MUSX archive and invokes outputCallback for each SVG document.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Extracts a MUSX archive using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// Registers all SVG format converters with the supplied registry.
void registerConverters(ConverterRegistry& registry);

} // namespace denigma::formats::svg
