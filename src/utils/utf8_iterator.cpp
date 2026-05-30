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
#include "utf8_iterator.h"

namespace utils {
namespace {
std::optional<Utf8Codepoint> decodeUtf8Codepoint(const char* bytes, size_t remaining)
{
    if (remaining == 0) {
        return std::nullopt;
    }

    const auto* data = reinterpret_cast<const unsigned char*>(bytes);

    char32_t codepoint = 0;
    size_t byteCount = 0;

    if (data[0] <= 0x7F) {
        codepoint = data[0];
        byteCount = 1;
    } else if ((data[0] & 0xE0) == 0xC0) {
        byteCount = 2;
        if (remaining < byteCount || (data[1] & 0xC0) != 0x80) {
            return std::nullopt;
        }

        codepoint = ((data[0] & 0x1F) << 6)
                  |  (data[1] & 0x3F);

        if (codepoint < 0x80) {
            return std::nullopt;
        }
    } else if ((data[0] & 0xF0) == 0xE0) {
        byteCount = 3;
        if (remaining < byteCount ||
            (data[1] & 0xC0) != 0x80 ||
            (data[2] & 0xC0) != 0x80) {
            return std::nullopt;
        }

        codepoint = ((data[0] & 0x0F) << 12)
                  | ((data[1] & 0x3F) << 6)
                  |  (data[2] & 0x3F);

        if (codepoint < 0x800) {
            return std::nullopt;
        }
    } else if ((data[0] & 0xF8) == 0xF0) {
        byteCount = 4;
        if (remaining < byteCount ||
            (data[1] & 0xC0) != 0x80 ||
            (data[2] & 0xC0) != 0x80 ||
            (data[3] & 0xC0) != 0x80) {
            return std::nullopt;
        }

        codepoint = ((data[0] & 0x07) << 18)
                  | ((data[1] & 0x3F) << 12)
                  | ((data[2] & 0x3F) << 6)
                  |  (data[3] & 0x3F);

        if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
            return std::nullopt;
        }
    } else {
        return std::nullopt;
    }

    return Utf8Codepoint{ codepoint, byteCount };
}
} // namespace

Utf8Iterator::Utf8Iterator(std::string_view text)
    : m_text(text)
{
    decodeCurrent();
}

void Utf8Iterator::next()
{
    if (m_atEnd) {
        return;
    }

    m_offset += m_current.byteCount;
    decodeCurrent();
}

void Utf8Iterator::decodeCurrent()
{
    if (m_offset >= m_text.size()) {
        m_atEnd = true;
        return;
    }

    auto decoded = decodeUtf8Codepoint(
        m_text.data() + m_offset,
        m_text.size() - m_offset
    );

    if (!decoded) {
        m_valid = false;
        m_atEnd = true;
        return;
    }

    m_current = *decoded;
    m_atEnd = false;
}

std::optional<char32_t> utf8ToCodepoint(const std::string& utf8)
{
    Utf8Iterator iter(utf8);

    if (iter.atEnd() || !iter.valid()) {
        return std::nullopt;
    }

    const auto result = iter->codepoint;

    iter.next();

    if (!iter.atEnd()) {
        return std::nullopt; // more than one codepoint
    }

    return result;
}} // namespace utils
