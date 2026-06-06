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
#include <optional>
#include <string>
#include <string_view>

namespace utils {
struct Utf8Codepoint
{
    char32_t codepoint{};
    size_t byteCount{};
};

class Utf8Iterator
{
public:
    explicit Utf8Iterator(std::string_view text);

    bool atEnd() const noexcept { return m_atEnd; }
    bool valid() const noexcept { return m_valid; }

    const Utf8Codepoint& operator*() const noexcept { return m_current; }
    const Utf8Codepoint* operator->() const noexcept { return &m_current; }

    size_t offset() const noexcept { return m_offset; }

    void next();

private:
    void decodeCurrent();

    std::string_view m_text;
    size_t m_offset{};
    Utf8Codepoint m_current{};
    bool m_atEnd{};
    bool m_valid{true};
};

/// @brief Converts a string to a utf32 codepoint if it represents exactly one codepoing.
std::optional<char32_t> utf8ToCodepoint(const std::string& utf8);
} //namespace utils
