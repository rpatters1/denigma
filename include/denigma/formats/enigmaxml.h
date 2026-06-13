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

#include "denigma/conversion.h"

namespace denigma::formats::enigmaxml {

/// Options for MUSX to Enigma XML conversion.
struct Options final : public IOptions
{
    /// Options common to all converters.
    CommonOptions common;
};

/// Converter adapter for MUSX archive input to Enigma XML output.
class MusxToEnigmaXmlConverter final : public IReaderConverter
{
public:
    [[nodiscard]] FormatId sourceFormat() const override { return FormatId::Musx; }
    [[nodiscard]] FormatId targetFormat() const override { return FormatId::EnigmaXml; }

    /// Extracts Enigma XML from a MUSX random-access reader and writes it to the provided stream.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const Options& options = {}) const;

    /// Extracts Enigma XML using type-erased registry options.
    ConversionResult convert(const IRandomAccessReader& input,
                             std::ostream& output,
                             const ConversionRequest& request = {}) const override;
};

/// Registers all Enigma XML format converters with the supplied registry.
void registerConverters(ConverterRegistry& registry);

} // namespace denigma::formats::enigmaxml
