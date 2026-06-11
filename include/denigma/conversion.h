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

enum class FormatId
{
    EnigmaXml,
    MnxJson
};

struct Diagnostic
{
    enum class Severity
    {
        Info,
        Warning,
        Error
    };

    Severity severity{ Severity::Info };
    std::string message;
};

struct ConversionOptions
{
    std::string sourceName;
    bool validate{ true };
    std::optional<int> indentSpaces{ 4 };
};

struct ConversionResult
{
    std::vector<Diagnostic> diagnostics;
};

class IConverter
{
public:
    virtual ~IConverter() = default;

    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    virtual ConversionResult convert(std::span<const std::byte> input,
                                     std::ostream& output,
                                     const ConversionOptions& options = {}) const = 0;
};

class ConverterRegistry
{
public:
    void add(std::unique_ptr<IConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_converters.emplace_back(std::move(converter));
    }

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
