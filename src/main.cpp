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

#include "enigmaxml/enigmaxml.h"
#include "mss/mss.h"
#include "mnx/mnx.h"

using namespace denigma;

constexpr char MUSX_EXTENSION[]		    = "musx";
constexpr char ENIGMAXML_EXTENSION[]    = "enigmaxml";
constexpr char MNX_EXTENSION[]		    = "mnx";
constexpr char MSS_EXTENSION[]		    = "mss";

// This function exists as std::to_array in C++20
template <typename T, std::size_t N>
constexpr std::array<T, N> to_array(const T(&arr)[N])
{
    std::array<T, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        result[i] = arr[i];
    }
    return result;
}

// Input format processors
constexpr auto inputProcessors = []() {
    struct InputProcessor
    {
        const char* extension;
        enigmaxml::Buffer(*processor)(const std::filesystem::path&);
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
        void(*processor)(const std::filesystem::path&, const enigmaxml::Buffer&, const std::optional<std::string>&, bool);
    };

    return to_array<OutputProcessor>({
        { ENIGMAXML_EXTENSION, enigmaxml::write },
        { MSS_EXTENSION, mss::convert },
        { MNX_EXTENSION, mnx::convert },
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

static int showHelpPage(const std::string& programName)
{
    // Print usage
    std::cerr << "Usage: " << programName << " <input file> [--output options]" << std::endl;
    std::cerr << std::endl;

    // General options
    std::cerr << "General options:" << std::endl;
    std::cerr << "  --help                      Show this help message and exit" << std::endl;
    std::cerr << "  --force                     Overwrite existing file(s)" << std::endl;
    std::cerr << "  --part [optional-part-name] Process for named part or first part if name is omitted" << std::endl;
    std::cerr << "  --all-parts                 Process for all parts and score" << std::endl;
    std::cerr << "  --version                   Show program version and exit" << std::endl;
    std::cerr << std::endl;

    // Supported input formats
    std::cerr << "Supported input formats:" << std::endl;
    for (const auto& input : inputProcessors) {
        std::cerr << "  *." << input.extension << std::endl;
    }
    std::cerr << std::endl;

    // Supported output formats
    std::cerr << "Supported output options:" << std::endl;
    for (const auto& output : outputProcessors) {
        std::cerr << "  --" << output.extension << " [optional filepath]" << std::endl;
    }
    std::cerr << std::endl;

    // Example usage
    std::cerr << "Examples:" << std::endl;
    std::cerr << "  " << programName << " input.musx --mss output.mss" << std::endl;
    std::cerr << "  " << programName << " input.musx --enigmaxml output.enigmaxml" << std::endl;
    //std::cerr << "  " << programName << " input.enigmaxml --mnx --mss" << std::endl;
    std::cerr << "  " << programName << " input.musx --mss --enigmaxml" << std::endl;

    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    std::string programName = std::filesystem::path(argv[0]).stem().string();

    if (argc < 2) {
        return showHelpPage(programName);
    }

    try {
        bool showVersion = false;
        bool showHelp = false;
        bool allowOverwrite = false;
        bool allPartsAndScore = false;
        std::optional<std::string> partName;
        std::vector<const char*> args;
        for (int x = 1; x < argc; x++) {
            const std::string_view next(argv[x]);
            if (next == "--version") {
                showVersion = true;
            } else if (next == "--help") {
                showHelp = true;
            } else if (next == "--force") {
                allowOverwrite = true;
            } else if (next == "--all-parts") {
                allPartsAndScore = true;
            } else if (next == "--part") {
                partName = "";
                std::string_view arg(argv[x + 1]);
                if (x < (argc - 1) && arg.rfind("--", 0) != 0) {
                    partName = arg;
                    x++;
                }
            } else {
                args.push_back(argv[x]);
            }
        }

        if (showVersion) {
            std::cout << programName << " " << DENIGMA_VERSION << std::endl;
            return 0;
        }
        if (showHelp) {
            showHelpPage(programName);
            return 0;
        }
        if (args.size() <= 0) {
            return showHelpPage(programName);
        }

        const std::filesystem::path inputFilePath = args[0];
        std::string inputExtension = inputFilePath.extension().string();
        if (inputExtension.empty()) {
            return showHelpPage(programName);
        }
        inputExtension = inputExtension.substr(1);
        const std::filesystem::path defaultPath = inputFilePath.parent_path();
        if (!std::filesystem::is_regular_file(inputFilePath))
        {
            std::cout << inputFilePath.string() << std::endl;
            std::cout << "Input file does not exists or is not a file." << std::endl;
            return 1;
        }
        std::cout << "Input: " << inputFilePath.string() << std::endl;

        // Find and call the input processor
        auto inputProcessor = findProcessor(inputProcessors, inputExtension);
        const enigmaxml::Buffer enigmaXml = inputProcessor(inputFilePath.string());

        auto processOutput = [&inputFilePath, &inputProcessor, &enigmaXml, partName, allPartsAndScore] (
            const std::string &outputFormat,
            const std::filesystem::path &outputPath,
            const bool allowOverwrite = false) -> void
        {
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
                if (allowOverwrite) {
                    std::cout << "Overwriting current file." << std::endl;
                }
                else {
                    std::cout << "File exists. Use --force to overwrite it." << std::endl;
                    return;
                }
            }

            auto outputProcessor = findProcessor(outputProcessors, outputFormat);
            outputProcessor(finalOutputPath, enigmaXml, partName, allPartsAndScore);
        };

        // Process output options
        bool outputFormatSpecified = false;
        for (size_t i = 0; i < args.size(); ++i) {
            std::string option = args[i];
            if (option.rfind("--", 0) == 0) {  // Options start with "--"
                std::string outputFormat = option.substr(2);
                std::filesystem::path outputFilePath = (i + 1 < args.size() && std::string(args[i + 1]).rfind("--", 0) != 0) ? args[++i] : defaultPath;
                processOutput(outputFormat, outputFilePath, allowOverwrite);
                outputFormatSpecified = true;
            }
        }
        if (!outputFormatSpecified) {
            processOutput(ENIGMAXML_EXTENSION, defaultPath, allowOverwrite);
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return showHelpPage(programName);
    }

    return 0;
}
