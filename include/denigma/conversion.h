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
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace denigma {

/// Stable identifiers for converter input and output formats.
enum class FormatId
{
    /// Finale Enigma XML.
    EnigmaXml,
    /// MNX JSON as produced by mnxdom.
    MnxJson
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

private:
    std::vector<std::unique_ptr<IConverter>> m_converters;
};

} // namespace denigma
