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

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "denigma/io/random_access_reader.h"

namespace denigma {

/// Stable identifiers for converter input and output formats.
enum class FormatId
{
    /// Finale MUSX archive.
    Musx,
    /// Finale Enigma XML.
    EnigmaXml,
    /// MNX JSON as produced by mnxdom.
    MnxJson,
    /// MuseScore style XML.
    MssXml,
    /// Scalable Vector Graphics XML.
    Svg
};

/// Unit suffix used by SVG output dimensions.
enum class SvgUnit
{
    None,
    Pixels,
    Points,
    Picas,
    Centimeters,
    Millimeters,
    Inches
};

/// A non-fatal message emitted by a converter.
struct Diagnostic
{
    /// Diagnostic severity.
    enum class Severity
    {
        Info,
        Warning,
        Error
    };

    Severity severity{ Severity::Info }; ///< Message severity.
    std::string message;                 ///< Human-readable diagnostic text.
};

/// Common options accepted by public converter adapters.
struct ConversionOptions
{
    /// Caller-supplied source name used for diagnostics and metadata.
    std::string sourceName;
    /// Enables converter-specific output validation when supported.
    bool validate{ true };
    /// Number of spaces used for formatted text output, or std::nullopt for compact output.
    std::optional<int> indentSpaces{ 4 };
    /// Optional cue layer to omit when converting to formats that do not support cues.
    std::optional<int> cueLayer;
    /// Optional MNX JSON schema contents used for MNX validation.
    std::optional<std::string> mnxSchema;
    /// Include Finale Tempo Tool changes in MNX output when supported.
    bool mnxIncludeTempoTool{ false };
    /// Split Finale instruments into separate MNX parts when supported.
    bool mnxSplitInstruments{ false };
    /// Unit suffix for SVG width and height output.
    SvgUnit svgUnit{ SvgUnit::Points };
    /// Extra scale multiplier for SVG output when page scaling is not active.
    double svgScale{ 1.0 };
    /// Optional ShapeDef identifiers for SVG conversion.
    std::vector<int> svgShapeDefs;
    /// Use Finale page-format scaling for SVG conversion when supported.
    bool svgUsePageScale{ false };
};

/// Result metadata returned after a conversion completes.
struct ConversionResult
{
    std::vector<Diagnostic> diagnostics; ///< Non-fatal diagnostics emitted during conversion.
};

/// Public interface implemented by each conversion adapter.
class IConverter
{
public:
    virtual ~IConverter() = default;

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input memory buffer and writes the converted output to the provided stream.
    virtual ConversionResult convert(std::span<const std::byte> input,
                                     std::ostream& output,
                                     const ConversionOptions& options = {}) const = 0;
};

/// Public interface implemented by adapters whose input is a random-access container.
class IReaderConverter
{
public:
    virtual ~IReaderConverter() = default;

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input reader and writes the converted output to the provided stream.
    virtual ConversionResult convert(const IRandomAccessReader& input,
                                     std::ostream& output,
                                     const ConversionOptions& options = {}) const = 0;
};

/// Callback used by converters that may emit zero, one, or many output buffers.
using MultiOutputCallback = std::function<void(std::string_view suggestedName, std::span<const std::byte> data)>;

/// Public interface implemented by adapters that may produce multiple output documents.
class IMultiOutputConverter
{
public:
    virtual ~IMultiOutputConverter() = default;

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input memory buffer and invokes outputCallback once for each generated output.
    virtual ConversionResult convert(std::span<const std::byte> input,
                                     const MultiOutputCallback& outputCallback,
                                     const ConversionOptions& options = {}) const = 0;
};

/// Public interface implemented by reader-backed adapters that may produce multiple output documents.
class IReaderMultiOutputConverter
{
public:
    virtual ~IReaderMultiOutputConverter() = default;

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input reader and invokes outputCallback once for each generated output.
    virtual ConversionResult convert(const IRandomAccessReader& input,
                                     const MultiOutputCallback& outputCallback,
                                     const ConversionOptions& options = {}) const = 0;
};

/// Lightweight registry for locating converters by source and target format.
class ConverterRegistry
{
public:
    /// Adds a converter to the registry.
    void add(std::unique_ptr<IConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_converters.emplace_back(std::move(converter));
    }

    /// Adds a multi-output converter to the registry.
    void add(std::unique_ptr<IMultiOutputConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_multiOutputConverters.emplace_back(std::move(converter));
    }

    /// Adds a reader-backed converter to the registry.
    void add(std::unique_ptr<IReaderConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_readerConverters.emplace_back(std::move(converter));
    }

    /// Adds a reader-backed multi-output converter to the registry.
    void add(std::unique_ptr<IReaderMultiOutputConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_readerMultiOutputConverters.emplace_back(std::move(converter));
    }

    /// Returns the first registered converter matching the requested formats, or nullptr.
    [[nodiscard]] const IConverter* find(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_converters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered multi-output converter matching the requested formats, or nullptr.
    [[nodiscard]] const IMultiOutputConverter* findMultiOutput(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_multiOutputConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered reader-backed converter matching the requested formats, or nullptr.
    [[nodiscard]] const IReaderConverter* findReader(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_readerConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered reader-backed multi-output converter matching the requested formats, or nullptr.
    [[nodiscard]] const IReaderMultiOutputConverter* findReaderMultiOutput(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_readerMultiOutputConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

private:
    std::vector<std::unique_ptr<IConverter>> m_converters;
    std::vector<std::unique_ptr<IMultiOutputConverter>> m_multiOutputConverters;
    std::vector<std::unique_ptr<IReaderConverter>> m_readerConverters;
    std::vector<std::unique_ptr<IReaderMultiOutputConverter>> m_readerMultiOutputConverters;
};

} // namespace denigma
