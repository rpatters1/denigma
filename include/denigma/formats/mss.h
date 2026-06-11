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

#include <optional>
#include <string>

#include "denigma/conversion.h"

namespace denigma::formats::mss {

/// Options for MuseScore style XML converters.
struct Options final : public IOptions
{
    CommonOptions common;
    /// Emit the score plus all linked parts for multi-output conversion.
    bool allPartsAndScore{ false };
    /// Optional part-name prefix for multi-output conversion.
    std::optional<std::string> partName;
};

/// Converter adapter for Enigma XML input to MuseScore style XML output.
class EnigmaXmlToMssXmlConverter final : public IConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::EnigmaXml; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MssXml; }

    /// Converts Enigma XML from memory and writes MuseScore style XML to the provided stream.
    ConversionResult convert(std::span<const std::byte> input,
                             std::ostream& output,
                             const Options& options = {}) const;

    /// Converts Enigma XML using type-erased registry options.
    ConversionResult convert(std::span<const std::byte> input,
                             std::ostream& output,
                             const ConversionRequest& request = {}) const override;
};

/// Converter adapter for MUSX archive input to MuseScore style XML output.
class MusxToMssXmlConverter final : public IReaderConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MssXml; }

    /// Extracts a MUSX archive and writes MuseScore style XML to the provided stream.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const Options& options = {}) const;

    /// Extracts a MUSX archive using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const ConversionRequest& request = {}) const override;
};

/// Converter adapter for Enigma XML input to one or more MuseScore style XML outputs.
class EnigmaXmlToMssXmlMultiOutputConverter final : public IMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::EnigmaXml; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MssXml; }

    /// Converts Enigma XML from memory and invokes outputCallback for each MSS document.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Converts Enigma XML using type-erased registry options.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// Converter adapter for MUSX archive input to one or more MuseScore style XML outputs.
class MusxToMssXmlMultiOutputConverter final : public IReaderMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MssXml; }

    /// Extracts a MUSX archive and invokes outputCallback for each MSS document.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Extracts a MUSX archive using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// Registers all MSS format converters with the supplied registry.
void registerConverters(ConverterRegistry& registry);

} // namespace denigma::formats::mss
