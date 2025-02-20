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
#include <exception>
#include <filesystem>
#include <algorithm>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(CP_UTF8) && !defined(CP_ACP) && !defined(STRINGUTILS_DEFINED_CPS)
#define CP_UTF8 65001
#define CP_ACP  0
#define STRINGUTILS_DEFINED_CPS
#endif

namespace utils {

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

inline std::string utf8ToAcp(const std::string& utf8)
{
#ifdef _WIN32
    if (::GetACP() != CP_UTF8) {
        return wstringToString(stringToWstring(utf8), CP_ACP);
    }   
#endif
    return utf8;
}

inline std::filesystem::path utf8ToPath(const std::string& str)
{
#ifdef _WIN32
    if (::GetACP() != CP_UTF8) {
        return stringToWstring(str, CP_UTF8);
    }
#endif
    return str;
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
