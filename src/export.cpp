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
        void(*processor)(const std::filesystem::path&, const Buffer&, const std::optional<std::string>&, bool);
    };

    return to_array<OutputProcessor>({
        { ENIGMAXML_EXTENSION, enigmaxml::write },
        { MSS_EXTENSION, mss::convert },
        // { MNX_EXTENSION, mnx::convert },
        });
    }();

// Function to find the appropriate processor
template <typename Processors>
decltype(Processors::value_type::processor) findProcessor(const Processors& processors, const std::string& key)
{
    for (const auto& p : processors) {
        if (key == p.extension) {
            return p.processor;
        }
    }
    throw std::invalid_argument("Unsupported format: " + key);
}

int ExportCommand::showHelpPage(const std::string_view& programName, const std::string& indentSpaces)
{
    std::string fullCommand = std::string(programName) + " " + std::string(commandName());
    // Print usage
    std::cerr << indentSpaces << "Usage: " << fullCommand << " <input-pattern> [--output options]" << std::endl;
    std::cerr << indentSpaces << std::endl;

    // Supported input formats
    std::cerr << indentSpaces << "Supported input formats:" << std::endl;
    for (const auto& input : inputProcessors) {
        std::cerr << indentSpaces << "  *." << input.extension << std::endl;
    }
    std::cerr << indentSpaces << std::endl;

    // Supported output formats
    std::cerr << indentSpaces << "Supported output options:" << std::endl;
    for (const auto& output : outputProcessors) {
        std::cerr << indentSpaces << "  --" << output.extension << " [optional filepath]" << std::endl;
    }
    std::cerr << indentSpaces << std::endl;

    // Example usage
    std::cerr << indentSpaces << "Examples:" << std::endl;
    std::cerr << indentSpaces << "  " << fullCommand << " input.musx --mss output.mss" << std::endl;
    std::cerr << indentSpaces << "  " << fullCommand << " input.musx --enigmaxml output.enigmaxml" << std::endl;
    //std::cerr << indentSpaces << "  " << programName << " input.enigmaxml --mnx --mss" << std::endl;
    std::cerr << indentSpaces << "  " << fullCommand << " input.musx --mss --enigmaxml" << std::endl;

    return 1;
}

int ExportCommand::doCommand(const std::vector<const char*>& args, const DenigmaOptions& options)
{
    if (args.empty()) {
        return showHelpPage(options.programName);
    }

    const std::filesystem::path inputFilePath = args[0];
    std::string inputExtension = inputFilePath.extension().string();
    if (inputExtension.empty()) {
        return showHelpPage(options.programName);
    }
    inputExtension = inputExtension.substr(1);
    const std::filesystem::path defaultPath = inputFilePath.parent_path();
    if (!std::filesystem::is_regular_file(inputFilePath)) {
        std::cout << inputFilePath.string() << std::endl;
        std::cout << "Input file does not exists or is not a file." << std::endl;
        return 1;
    }
    std::cout << "Input: " << inputFilePath.string() << std::endl;

    // Find and call the input processor
    auto inputProcessor = findProcessor(inputProcessors, inputExtension);
    const Buffer enigmaXml = inputProcessor(inputFilePath.string());

    auto processOutput = [&inputFilePath, &inputProcessor, &enigmaXml, options] (
        const std::string& outputFormat,
        const std::filesystem::path& outputPath) -> void {
            std::filesystem::path finalOutputPath = outputPath;

            if (std::filesystem::is_directory(finalOutputPath)) {
                std::filesystem::path outputFileName = inputFilePath.filename();
                outputFileName.replace_extension(outputFormat);
                finalOutputPath.append(outputFileName.string());
            }

            if (inputFilePath == finalOutputPath) {
                std::cout << "Input and output are the same. No action taken." << std::endl;
                return;
            }

            if (std::filesystem::exists(finalOutputPath)) {
                std::cout << "Output: " << finalOutputPath.string() << std::endl;
                if (options.overwriteExisting) {
                    std::cout << "Overwriting current file(s)." << std::endl;
                }
                else {
                    std::cout << "File exists. Use --force to overwrite it." << std::endl;
                    return;
                }
            }

            auto outputProcessor = findProcessor(outputProcessors, outputFormat);
            outputProcessor(finalOutputPath, enigmaXml, options.partName, options.allPartsAndScore);
        };

    // Process output options
    bool outputFormatSpecified = false;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string option = args[i];
        if (option.rfind("--", 0) == 0) {  // Options start with "--"
            std::string outputFormat = option.substr(2);
            std::filesystem::path outputFilePath = (i + 1 < args.size() && std::string(args[i + 1]).rfind("--", 0) != 0) ? args[++i] : defaultPath;
            processOutput(outputFormat, outputFilePath);
            outputFormatSpecified = true;
        }
    }
    if (!outputFormatSpecified) {
        processOutput(ENIGMAXML_EXTENSION, defaultPath);
    }

    return 0;
}

} // namespace denigma