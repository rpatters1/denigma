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
#include <array>
#include <vector>
#include <optional>

inline constexpr char MUSX_EXTENSION[]      = "musx";
inline constexpr char ENIGMAXML_EXTENSION[] = "enigmaxml";
inline constexpr char MNX_EXTENSION[]       = "mnx";
inline constexpr char MSS_EXTENSION[]       = "mss";

namespace denigma {

// This function exists as std::to_array in C++20
template <typename T, std::size_t N>
inline constexpr std::array<T, N> to_array(const T(&arr)[N])
{
    std::array<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = arr[i];
    }
    return result;
}

using Buffer = std::vector<char>;

struct DenigmaOptions
{
    std::string_view programName;
    bool overwriteExisting{};
    bool allPartsAndScore{};
    std::optional<std::string> partName;
    std::filesystem::path inputFilePath;
};

class ICommand
{
public:
    ICommand() = default;
    virtual ~ICommand() = default;

    virtual int showHelpPage(const std::string_view& programName, const std::string& indentSpaces = {}) const = 0;

    virtual Buffer processInput(const std::filesystem::path& inputPath, const DenigmaOptions& options) const = 0;
    virtual void processOutput(const Buffer& enigmaXml, const std::filesystem::path& outputPath, const DenigmaOptions& options) const = 0;
    virtual std::optional<std::string_view> defaultOutputFormat() const = 0;
    
    virtual const std::string_view commandName() const = 0;
};

bool validatePathsAndOptions(const std::filesystem::path& outputFilePath, const denigma::DenigmaOptions& options);

}