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

namespace denigma {
namespace formats {

/// @namespace denigma::formats::musicxml
/// @brief Conversion adapters for generating MusicXML.
namespace musicxml {

/// @struct Options
/// @brief Options for MusicXML converters.
struct Options final : public IOptions
{
    /// Options common to all converters.
    CommonOptions common;
    bool includeTempoTool{ false };
    /// Emit the score plus all linked parts for multi-output conversion.
    bool allPartsAndScore{ false };
    /// Optional part-name prefix for multi-output conversion.
    std::optional<std::string> partName;
};

/// @class EnigmaXmlToMusicXmlMultiOutputConverter
/// @brief Converter adapter for Enigma XML input to one or more MusicXML outputs (score and/or parts).
class EnigmaXmlToMusicXmlMultiOutputConverter final : public IMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::EnigmaXml; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MusicXml; }

    /// Converts Enigma XML from memory and invokes outputCallback for each MusicXML document.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Converts Enigma XML using type-erased registry options.
    ConversionResult convert(std::span<const std::byte> input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// @class MusxToMusicXmlMultiOutputConverter
/// @brief Converter adapter for MUSX archive input to one or more MusicXML outputs (score and/or parts).
class MusxToMusicXmlMultiOutputConverter final : public IReaderMultiOutputConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::MusicXml; }

    /// Extracts a MUSX archive and invokes outputCallback for each MusicXML document.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const Options& options = {}) const;

    /// Extracts a MUSX archive using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             const MultiOutputCallback& outputCallback,
                             const ConversionRequest& request = {}) const override;
};

/// Registers all MusicXML format converters with the supplied registry.
void registerConverters(ConverterRegistry& registry);

} // namespace musicxml
} // namespace formats
} // namespace denigma
