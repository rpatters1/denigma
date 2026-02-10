/*
 * Copyright (C) 2024, Robert Patterson
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

#include <string>
#include <string_view>
#include <ostream>
#include <exception>
#include <filesystem>
#include <algorithm>
#include <optional>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(CP_UTF8) && !defined(CP_ACP) && !defined(STRINGUTILS_DEFINED_CPS)
#define CP_UTF8 65001
#define CP_ACP  0
#define STRINGUTILS_DEFINED_CPS
#endif

namespace utils {

inline std::string utf8ToString(std::u8string_view utf8)
{
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

inline void appendUtf8(std::string& target, std::u8string_view utf8)
{
    target.append(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

inline std::u8string stringToUtf8(std::string_view str)
{
    return std::u8string(reinterpret_cast<const char8_t*>(str.data()), str.size());
}

struct Utf8Bytes
{
    std::u8string value;
};

inline Utf8Bytes asUtf8Bytes(const std::filesystem::path& path)
{
    return Utf8Bytes{ path.u8string() };
}

inline Utf8Bytes asUtf8Bytes(std::u8string value)
{
    return Utf8Bytes{ std::move(value) };
}

inline std::ostream& operator<<(std::ostream& os, const Utf8Bytes& utf8)
{
    os.write(reinterpret_cast<const char*>(utf8.value.data()), static_cast<std::streamsize>(utf8.value.size()));
    return os;
}

class encoding_error : public std::exception
{
public:
    encoding_error(const std::string& msg, int codepage) :
        m_msg(msg + " (codepage " + std::to_string(codepage) + ")"),
        m_codepage(codepage) {}

    const char * what() const noexcept override
    {
        return m_msg.c_str();
    }

    int codepage() const { return m_codepage; }

private:
    std::string m_msg;
    int m_codepage;
};

inline std::wstring stringToWstring([[maybe_unused]]const std::string& acp, [[maybe_unused]]int codepage = CP_UTF8)
{
#ifdef _WIN32
    int wlen = MultiByteToWideChar(codepage, 0, acp.c_str(), -1, nullptr, 0);
    if (wlen == 0) throw encoding_error("string to wstring conversion failed for value: " + acp, codepage);
    std::wstring wide(wlen, L'\0');
    MultiByteToWideChar(codepage, 0, acp.c_str(), -1, &wide[0], wlen);
    wide.resize(wlen - 1); // Remove null terminator
    return wide;
#else
    throw std::runtime_error("stringToWstring is not supported on this platform");
#endif
}

inline std::string wstringToString([[maybe_unused]]const std::wstring& wide, [[maybe_unused]]int codepage = CP_UTF8)
{
#ifdef _WIN32
    BOOL usedDefaultChar{};
    DWORD flags = (codepage != CP_UTF8) ? WC_NO_BEST_FIT_CHARS : WC_ERR_INVALID_CHARS;
    int clen = WideCharToMultiByte(codepage, flags, wide.c_str(), -1, nullptr, 0, nullptr, &usedDefaultChar);
    if (clen == 0 || usedDefaultChar) {
        throw encoding_error("wstring to string conversion failed because no Unicode mapping exists in the target codepage", codepage);
    }
    std::string acp(clen, '\0');
    WideCharToMultiByte(codepage, 0, wide.c_str(), -1, &acp[0], clen, nullptr, nullptr);
    acp.resize(clen - 1); // Remove null terminator
    return acp;
#else
    throw std::runtime_error("wstringToString is not supported on this platform");
#endif
}

inline std::optional<std::string> getEnvironmentValue(const char* name)
{
    if (!name || !*name) {
        return std::nullopt;
    }
#ifdef _WIN32
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || !value || len == 0) {
        if (value) {
            std::free(value);
        }
        return std::nullopt;
    }
    std::string result(value);
    std::free(value);
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
#else
    if (const char* value = std::getenv(name)) {
        return std::string(value);
    }
    return std::nullopt;
#endif
}

inline std::filesystem::path utf8ToPath(std::string_view str)
{
    return std::filesystem::path(stringToUtf8(str));
}

inline std::string pathToString(const std::filesystem::path& path)
{
    return utf8ToString(path.u8string());
}

inline bool pathExtensionEquals(const std::filesystem::path& path, std::u8string_view extensionWithoutDot)
{
    const auto extension = path.extension().u8string();
    if (extension.size() != extensionWithoutDot.size() + 1 || extension.empty() || extension.front() != u8'.') {
        return false;
    }
    return std::equal(extensionWithoutDot.begin(), extensionWithoutDot.end(), extension.begin() + 1);
}

inline std::optional<char32_t> utf8ToCodepoint(const std::string& utf8) {
    size_t len = utf8.size();
    if (len == 0 || len > 4) {
        return std::nullopt; // Invalid UTF-8 (must be 1-4 bytes)
    }

    const unsigned char* data = reinterpret_cast<const unsigned char*>(utf8.data());
    char32_t codepoint = 0;

    if (len == 1) { 
        // ASCII (0x00 - 0x7F)
        if (data[0] <= 0x7F) {
            codepoint = data[0];
        } else {
            return std::nullopt; // Invalid single-byte character
        }
    } else if (len == 2) { 
        // 2-byte sequence (U+0080 - U+07FF)
        if ((data[0] & 0xE0) == 0xC0 && (data[1] & 0xC0) == 0x80) {
            codepoint = ((data[0] & 0x1F) << 6) | (data[1] & 0x3F);
            if (codepoint < 0x80) return std::nullopt; // Overlong encoding
        } else {
            return std::nullopt;
        }
    } else if (len == 3) { 
        // 3-byte sequence (U+0800 - U+FFFF)
        if ((data[0] & 0xF0) == 0xE0 &&
            (data[1] & 0xC0) == 0x80 &&
            (data[2] & 0xC0) == 0x80) {
            codepoint = ((data[0] & 0x0F) << 12) |
                        ((data[1] & 0x3F) << 6) |
                        (data[2] & 0x3F);
            if (codepoint < 0x800) return std::nullopt; // Overlong encoding
        } else {
            return std::nullopt;
        }
    } else if (len == 4) { 
        // 4-byte sequence (U+10000 - U+10FFFF)
        if ((data[0] & 0xF8) == 0xF0 &&
            (data[1] & 0xC0) == 0x80 &&
            (data[2] & 0xC0) == 0x80 &&
            (data[3] & 0xC0) == 0x80) {
            codepoint = ((data[0] & 0x07) << 18) |
                        ((data[1] & 0x3F) << 12) |
                        ((data[2] & 0x3F) << 6) |
                        (data[3] & 0x3F);
            if (codepoint < 0x10000 || codepoint > 0x10FFFF) return std::nullopt; // Overlong encoding or out of range
        } else {
            return std::nullopt;
        }
    } else {
        return std::nullopt; // Invalid size
    }

    return codepoint;
}

inline std::string toLowerCase(const std::string& inp)
{
    std::string s = inp;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

inline std::string fileToString(const std::filesystem::path& pathToRead)
{
    std::ifstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(pathToRead, std::ios::binary | std::ios::ate);

    // Preallocate retval based on file size
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string retval(static_cast<std::size_t>(fileSize), 0);
    file.read(retval.data(), fileSize);
    return retval;
}

#if defined(STRINGUTILS_DEFINED_CPS)
#undef CP_UTF8
#undef CP_ACP
#undef STRINGUTILS_DEFINED_CPS
#endif

} // namespace utils
