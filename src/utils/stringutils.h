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
#include <cctype>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#endif

#if !defined(CP_UTF8) && !defined(CP_ACP) && !defined(STRINGUTILS_DEFINED_CPS)
#define CP_UTF8 65001
#define CP_ACP  0
#define STRINGUTILS_DEFINED_CPS
#endif

namespace utils {

namespace detail {

template <typename T>
using EnableIfEightBitCharacter = std::enable_if_t<
    std::is_integral_v<T> && sizeof(T) == 1 && !std::is_same_v<std::remove_cv_t<T>, bool>, int>;

} // namespace detail

template <typename T, detail::EnableIfEightBitCharacter<T> = 0>
inline bool isSpace(T c)
{
    const auto byte = static_cast<unsigned char>(c);
    return byte <= 0x7f && std::isspace(byte) != 0;
}

template <typename T, detail::EnableIfEightBitCharacter<T> = 0>
inline bool isPunctuation(T c)
{
    const auto byte = static_cast<unsigned char>(c);
    return byte <= 0x7f && std::ispunct(byte) != 0;
}

template <typename T, detail::EnableIfEightBitCharacter<T> = 0>
inline bool isAlphaNumeric(T c)
{
    const auto byte = static_cast<unsigned char>(c);
    return byte <= 0x7f && std::isalnum(byte) != 0;
}

template <typename T, detail::EnableIfEightBitCharacter<T> = 0>
inline T toLowerCase(T c)
{
    const auto byte = static_cast<unsigned char>(c);
    return byte <= 0x7f ? static_cast<T>(std::tolower(byte)) : c;
}

inline std::string utf8ToString(std::u8string_view utf8)
{
    return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

inline std::string trimNewLineFromString(const std::string& src)
{
    const size_t pos = src.find('\n');
    if (pos != std::string::npos) {
        return src.substr(0, pos);
    }
    return src;
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

inline std::u8string normalizedExtension(std::u8string extension)
{
    if (!extension.empty() && extension.front() == u8'.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char8_t>(toLowerCase(c));
    });
    return extension;
}

inline std::u8string normalizedPathExtension(const std::filesystem::path& path)
{
    return normalizedExtension(path.extension().u8string());
}

inline bool pathExtensionEquals(const std::filesystem::path& path, std::u8string_view extensionWithoutDot)
{
    return normalizedPathExtension(path) == normalizedExtension(std::u8string(extensionWithoutDot));
}

inline std::string toLowerCase(const std::string& inp)
{
    std::string s = inp;
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return toLowerCase(c); });
    return s;
}

inline std::string trimAscii(std::string_view text)
{
    while (!text.empty() && isSpace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && isSpace(text.back())) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

#if defined(STRINGUTILS_DEFINED_CPS)
#undef CP_UTF8
#undef CP_ACP
#undef STRINGUTILS_DEFINED_CPS
#endif

} // namespace utils
