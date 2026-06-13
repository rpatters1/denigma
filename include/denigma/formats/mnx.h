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
#pragma once

#include <optional>
#include <string>

#include "denigma/conversion.h"

namespace denigma {
namespace formats {

/// @namespace denigma::formats::mnx
/// @brief Conversion adapters for generating MNX JSON.
namespace mnx {

/// @struct Options
/// @brief Options for MNX JSON converters.
struct Options final : public IOptions
{
    /// Options common to all converters.
    CommonOptions common;
    /// Number of spaces used for formatted JSON output, or std::nullopt for compact output.
    std::optional<int> indentSpaces{ 4 };
    /// Optional cue layer to omit because MNX does not currently support cues.
    std::optional<int> cueLayer;
    /// Optional MNX JSON schema contents used for validation.
    std::optional<std::string> schema;
    /// Include Finale Tempo Tool changes.
    bool includeTempoTool{ false };
    /// Split Finale instruments into separate MNX parts.
    bool splitInstruments{ false };
};

/// @class EnigmaXmlToMnxJsonConverter
/// @brief Converter adapter for Enigma XML input to MNX JSON output.
class EnigmaXmlToMnxJsonConverter final : public IConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::EnigmaXml; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MnxJson; }

    /// Converts Enigma XML from memory and writes MNX JSON to the provided stream.
    ConversionResult convert(std::span<const std::byte> input,
                             std::ostream& output,
                             const Options& options = {}) const;

    /// Converts Enigma XML using type-erased registry options.
    ConversionResult convert(std::span<const std::byte> input,
                             std::ostream& output,
                             const ConversionRequest& request = {}) const override;
};

/// @class MusxToMnxJsonConverter
/// @brief Converter adapter for MUSX archive input to MNX JSON output.
class MusxToMnxJsonConverter final : public IReaderConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MnxJson; }

    /// Extracts a MUSX archive and writes MNX JSON to the provided stream.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const Options& options = {}) const;

    /// Extracts a MUSX archive using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const ConversionRequest& request = {}) const override;
};

/// Registers all MNX format converters with the supplied registry.
void registerConverters(ConverterRegistry& registry);

} // namespace mnx
} // namespace formats
} // namespace denigma
