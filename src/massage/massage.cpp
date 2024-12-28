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
#include <functional>

#include "massage/massage.h"
#include "massage/musicxml.h"

namespace denigma {

static Buffer nullFunc(const std::filesystem::path&, const DenigmaContext&)
{
    return {};
}

// Input format processors
constexpr auto inputProcessors = []() {
    struct InputProcessor
    {
        const char* extension;
        Buffer(*processor)(const std::filesystem::path&, const DenigmaContext&);
    };

    return to_array<InputProcessor>({
            { MXL_EXTENSION, nullFunc },
            { MUSICXML_EXTENSION, musicxml::read },
        });
    }();

// Output format processors
constexpr auto outputProcessors = []() {
    struct OutputProcessor
    {
        const char* extension;
        void(*processor)(const std::filesystem::path&, const std::filesystem::path&, const Buffer&, const DenigmaContext&);
    };

    return to_array<OutputProcessor>({
            { MUSICXML_EXTENSION, musicxml::massage },
        });
    }();

int MassageCommand::showHelpPage(const std::string_view& programName, const std::string& indentSpaces) const
{
    std::string fullCommand = std::string(programName) + " " + std::string(commandName());
    // Print usage
    std::cout << indentSpaces << "Usage: " << fullCommand << " <input-pattern> [--output options]" << std::endl;
    std::cout << indentSpaces << std::endl;

    // Supported input formats
    std::cout << indentSpaces << "Supported input formats:" << std::endl;
    for (const auto& input : inputProcessors) {
        std::cout << indentSpaces << "  *." << input.extension;
        if (input.extension == defaultInputFormat()) {
            std::cout << " (default input format)";
        }
        std::cout << std::endl;
    }
    std::cout << indentSpaces << std::endl;

    // Supported output formats
    std::cout << indentSpaces << "Supported output options:" << std::endl;
    for (const auto& output : outputProcessors) {
        std::cout << indentSpaces << "  --" << output.extension << " [optional filepath]" << std::endl;
    }
    std::cout << indentSpaces << std::endl;

    // Example usage
    //std::cout << indentSpaces << "Examples:" << std::endl;
    //std::cout << indentSpaces << "  " << fullCommand << " myfolder --mss exports/mss --all-parts --recursive --relative" << std::endl;
    //std::cout << indentSpaces << "  " << "  exports .musx files in myfolder and all subfolders to directory exports/mss within the same folder as each file found" << std::endl;
    //std::cout << indentSpaces << "  " << fullCommand << " input.musx --enigmaxml output.enigmaxml" << std::endl;
    //std::cout << indentSpaces << "  " << "  exports the enigmaxml in input.musx to output.enigmaxml in the same folder" << std::endl;
    //std::cout << indentSpaces << "  " << programName << " input.enigmaxml --mnx --mss" << std::endl;
    //std::cout << indentSpaces << "  " << fullCommand << " myfile.mxl" << std::endl;
    std::cout << indentSpaces << "  " << "  massages the score from myfile.enigmaxml to myfile.massaged.musicxml in the same folder" << std::endl;

    return 1;
}

bool MassageCommand::canProcess(const std::filesystem::path& inputPath) const
{
    try {
        findProcessor(inputProcessors, inputPath.extension().u8string());
        return true;
    } catch (...) {}
    return false;
}

Buffer MassageCommand::processInput(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const
{
    auto inputProcessor = findProcessor(inputProcessors, inputPath.extension().u8string());
    return inputProcessor(inputPath, denigmaContext);
}

void MassageCommand::processOutput(const Buffer& musicXml, const std::filesystem::path& outputPath, const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const
{
    auto outputProcessor = findProcessor(outputProcessors, outputPath.extension().u8string());
    outputProcessor(inputPath, outputPath, musicXml, denigmaContext);
}

} // namespace denigma