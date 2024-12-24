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
#include <filesystem>
#include <iostream>
#include <string>
#include <array>
#include <unordered_map>
#include <optional>

#include "export.h"

#include "enigmaxml/enigmaxml.h"
#include "mss/mss.h"
#include "mnx/mnx.h"

namespace denigma {

// Input format processors
constexpr auto inputProcessors = []() {
    struct InputProcessor
    {
        const char* extension;
        Buffer(*processor)(const std::filesystem::path&);
    };

    return to_array<InputProcessor>({
        { MUSX_EXTENSION, enigmaxml::extract },
        { ENIGMAXML_EXTENSION, enigmaxml::read },
        });
    }();

// Output format processors
constexpr auto outputProcessors = []() {
    struct OutputProcessor
    {
        const char* extension;
        void(*processor)(const std::filesystem::path&, const Buffer&, const DenigmaOptions&);
    };

    return to_array<OutputProcessor>({
        { ENIGMAXML_EXTENSION, enigmaxml::write },
        { MSS_EXTENSION, mss::convert },
        // { MNX_EXTENSION, mnx::convert },
        });
    }();

// Function to find the appropriate processor
template <typename Processors>
decltype(Processors::value_type::processor) findProcessor(const Processors& processors, const std::string& extension)
{
    std::string key = extension;
    if (key.rfind(".", 0) == 0) {
        key = extension.substr(1);
    }
    for (const auto& p : processors) {
        if (key == p.extension) {
            return p.processor;
        }
    }
    throw std::invalid_argument("Unsupported format: " + key);
}

int ExportCommand::showHelpPage(const std::string_view& programName, const std::string& indentSpaces) const
{
    std::string fullCommand = std::string(programName) + " " + std::string(commandName());
    // Print usage
    std::cout << indentSpaces << "Usage: " << fullCommand << " <input-pattern> [--output options]" << std::endl;
    std::cout << indentSpaces << std::endl;

    // Supported input formats
    std::cout << indentSpaces << "Supported input formats:" << std::endl;
    for (const auto& input : inputProcessors) {
        std::cout << indentSpaces << "  *." << input.extension << std::endl;
    }
    std::cout << indentSpaces << std::endl;

    // Supported output formats
    std::cout << indentSpaces << "Supported output options:" << std::endl;
    for (const auto& output : outputProcessors) {
        std::cout << indentSpaces << "  --" << output.extension << " [optional filepath]" << std::endl;
    }
    std::cout << indentSpaces << std::endl;

    // Example usage
    std::cout << indentSpaces << "Examples:" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx --mss output.mss" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx --enigmaxml output.enigmaxml" << std::endl;
    //std::cout << indentSpaces << "  " << programName << " input.enigmaxml --mnx --mss" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx --mss --enigmaxml" << std::endl;

    return 1;
}

Buffer ExportCommand::processInput(const std::filesystem::path& inputPath, const DenigmaOptions&) const
{
    auto inputProcessor = findProcessor(inputProcessors, inputPath.extension().string());
    return inputProcessor(inputPath);
}

void ExportCommand::processOutput(const Buffer& enigmaXml, const std::filesystem::path& outputPath, const DenigmaOptions& options) const
{
    auto outputProcessor = findProcessor(outputProcessors, outputPath.extension().string());
    outputProcessor(outputPath, enigmaXml, options);
}

} // namespace denigma