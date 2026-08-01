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
#include <algorithm>
#include <iostream>
#include <map>
#include <vector>
#include <unordered_set>
#include <optional>
#include <memory>
#include <chrono>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <clocale>

#include "core/denigma.h"
#include "export/export.h"
#include "massage/massage.h"
#include "utils/stringutils.h"

static const auto registeredCommands = []()
    {
        std::map <std::string, std::shared_ptr <denigma::ICommand>> retval;
        auto exportCmd = std::make_shared<denigma::ExportCommand>();
        retval.emplace(exportCmd->commandName(), exportCmd);
        auto massageCommand = std::make_shared<denigma::MassageCommand>();
        retval.emplace(massageCommand->commandName(), massageCommand);
        return retval;
    }();

static int showHelpPage(const std::string_view& programName)
{
    std::cout << "Usage: " << programName << " [<command>] <input-pattern> [--options]" << std::endl;
    std::cout << std::endl;

    // General options
    std::cout << "General options:" << std::endl;
    std::cout << "  --about                         Show acknowledgements and exit" << std::endl;
    std::cout << "  --exclude folder-name           Exclude the specified folder name from recursive searches" << std::endl;
    std::cout << "  --help                          Show this help message and exit" << std::endl;
    std::cout << "  --force                         Overwrite existing file(s)" << std::endl;
    std::cout << "  --part [optional-part-name]     Process named part or first part if name is omitted" << std::endl;
    std::cout << "  --recursive                     Recursively search subdirectories of the input directory" << std::endl;
    std::cout << "  --all-parts                     Process all parts and score" << std::endl;
    std::cout << "  --version                       Show program version and exit" << std::endl;
    std::cout << "  --no-validate                   Skip validation of output results (currently applies only to MNX exports)" << std::endl;
    std::cout << std::endl;
    
    for (const auto& command : registeredCommands) {
        std::string commandStr = "Command " + command.first;
        std::string sepStr(commandStr.size(), '=');
        std::cout << std::endl;
        std::cout << sepStr << std::endl;
        std::cout << commandStr << std::endl;
        std::cout << sepStr << std::endl;
        std::cout << std::endl;
        command.second->showHelpPage(programName, "    ");
    }

    std::cout << std::endl;
    std::cout << "By default, messages are sent to std::cerr." << std::endl;
    std::cout << std::endl;
    std::cout << "Logging options:" << std::endl;
    std::cout << "  --log [optional-logfile-path]   Log messages to file instead of sending them to std::cerr" << std::endl;
    std::cout << "  --no-log                        Always send messages to std::cerr (overrides any other logging options)" << std::endl;
    std::cout << "  --quiet                         Only display errors and warning messages (overrides --verbose)" << std::endl;
    std::cout << "  --verbose                       Verbose output" << std::endl;
    std::cout << std::endl;
    std::cout << "Relative input and explicit output paths are resolved from the current working directory." << std::endl;
    std::cout << "Default output files (no explicit output path) are written next to each input file." << std::endl;
    std::cout << "Relative log paths for --log are resolved from the top-level input folder." << std::endl;

    return 1;
}

static std::string fileToString(const std::filesystem::path& pathToRead)
{
    std::ifstream file;
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file.open(pathToRead, std::ios::binary | std::ios::ate);

    const std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string retval(static_cast<std::size_t>(fileSize), 0);
    file.read(retval.data(), fileSize);
    return retval;
}

using namespace denigma;

int _MAIN(int argc, arg_char* argv[])
{
#ifndef DENIGMA_TEST
    setlocale(LC_ALL, "");
#endif
    
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    DenigmaContext denigmaContext(std::filesystem::path(*argv).stem().native());
    
    if (argc < 2) {
        return showHelpPage(denigmaContext.programName);
    }

    std::vector<const arg_char*> args;
    try {
        args = denigmaContext.parseOptions(argc, argv);
    } catch (const std::exception& e) {
        denigmaContext.logMessage(LogMsg() << e.what(), MessageSeverity::Error);
        return 1;
    }

    if (denigmaContext.showVersion) {
        std::cout << denigmaContext.programName << " " << DENIGMA_VERSION << std::endl;
        return 0;
    }
    if (denigmaContext.showHelp) {
        showHelpPage(denigmaContext.programName);
        return 0;
    }
    if (denigmaContext.showAbout) {
        showAboutPage();
        return 0;
    }

    const auto currentCommand = [&]() -> std::shared_ptr<ICommand> {
        if (args.empty()) return nullptr;
        auto it = registeredCommands.find(arg_string(args[0]));
        if (it != registeredCommands.end()) {
            args.erase(args.begin());
            return it->second;
        }
        it = registeredCommands.find(arg_string(std::string(denigma::ExportCommand().commandName())));
        ASSERT_IF(it == registeredCommands.end()) {
            std::cerr << "Export command is missing!" << std::endl;
            return nullptr;
        }
        return it->second;
    }();
    if (!currentCommand || args.empty()) {
        std::cerr << "Not enough arguments passed" << std::endl;
        return showHelpPage(denigmaContext.programName);
    }

    MusxLoggerScope musxLogger(makeMusxLogCallback(denigmaContext));

    // collect files to process first
    // this avoids potential infinite recursion if input and output are the same format
    PathSet pathsToProcess;
    std::optional<std::filesystem::path> defaultLogPath;

    try {
        std::optional<std::string> optionBeforeInput;
        for (size_t x = 0; x < args.size(); x++) {
            arg_view option = args[x];
            if (option.rfind(_ARG("--"), 0) == 0) {  // Options start with "--"
                optionBeforeInput = std::string(arg_string(option));
                break;
            }
            const std::filesystem::path rawInputPattern = option;
            std::filesystem::path inputFilePattern = rawInputPattern;

            // collect inputs
            const auto inputPatternUtf8 = inputFilePattern.u8string();
            const bool isSpecificFileOrDirectory = inputPatternUtf8.find(u8'*') == std::u8string::npos && inputPatternUtf8.find(u8'?') == std::u8string::npos;
            bool isSpecificFile = isSpecificFileOrDirectory && inputFilePattern.has_filename();
            std::vector<std::filesystem::path> wildcardPatterns;
            if (std::filesystem::is_directory(inputFilePattern)) {
                isSpecificFile = false;
                for (const auto& format : currentCommand->defaultInputFormats()) {
                    std::u8string wildcardPattern = u8"*.";
                    wildcardPattern.append(format);
                    wildcardPatterns.emplace_back(wildcardPattern);
                }
                if (wildcardPatterns.empty()) {
                    wildcardPatterns.emplace_back(std::u8string(u8"*"));
                }
                inputFilePattern /= ""; // assure parent_path returns inputFilePattern
            } else {
                wildcardPatterns.emplace_back(inputFilePattern.filename());
            }
            std::filesystem::path inputDir = inputFilePattern.parent_path();
            if (inputDir.is_relative()) {
                inputDir = std::filesystem::current_path() / inputDir;
            }
            const bool inputDirectoryExists = std::filesystem::is_directory(inputDir);
            if (inputDirectoryExists && !defaultLogPath.has_value()) {
                defaultLogPath = inputDir;
            }
            // convert each wildcard pattern to regex
#ifdef _WIN32
            std::vector<std::wregex> regexes;
#else
            std::vector<std::regex> regexes;
#endif
            for (const auto& pattern : wildcardPatterns) {
                auto wildcardPattern = pattern.native(); // native format avoids encoding issues
#ifdef _WIN32
                auto regexPattern = std::regex_replace(wildcardPattern, std::wregex(LR"(\*)"), L".*");
                regexPattern = std::regex_replace(regexPattern, std::wregex(LR"(\?)"), L".");
#else
                auto regexPattern = std::regex_replace(wildcardPattern, std::regex(R"(\*)"), R"(.*)");
                regexPattern = std::regex_replace(regexPattern, std::regex(R"(\?)"), R"(.)");
#endif
                regexes.emplace_back(regexPattern);
            }
            const auto matchesAnyPattern = [&regexes](const std::filesystem::path& path) {
                return std::any_of(regexes.begin(), regexes.end(), [&path](const auto& regex) {
                    return std::regex_match(path.filename().native(), regex);
                });
            };

            auto iterate = [&](auto& iterator) {
                for (auto it = iterator; it != std::filesystem::end(iterator); ++it) {
                    const auto& entry = *it;
                    if constexpr (std::is_same_v<std::remove_reference_t<decltype(iterator)>, std::filesystem::recursive_directory_iterator>) {
                        if (entry.is_directory()) {
                            if (entry.path().filename() == denigmaContext.excludeFolder) {
                                it.disable_recursion_pending(); // Skip this folder and its subdirectories
                                continue;
                            }
                        }
                    }
                    if (!entry.is_directory()) {
                        denigmaContext.logMessage(LogMsg() << "considered file " << utils::asUtf8Bytes(entry.path()), MessageSeverity::Verbose);
                    }
                    if (entry.is_regular_file() && matchesAnyPattern(entry.path())) {
                        auto inputFilePath = entry.path();
                        if (currentCommand->canProcess(inputFilePath)) {
                            pathsToProcess.emplace(inputFilePath);
                        }
                    }
                }
            };
            if (inputDirectoryExists && !isSpecificFile) {
                if (denigmaContext.recursiveSearch) {
                    std::filesystem::recursive_directory_iterator it(inputDir);
                    iterate(it);
                } else {
                    std::filesystem::directory_iterator it(inputDir);
                    iterate(it);
                }
            } else {
                pathsToProcess.emplace(inputFilePattern);
            }
        }
        if (pathsToProcess.empty() && optionBeforeInput.has_value()) {
            throw std::invalid_argument("Unknown or misplaced option: " + optionBeforeInput.value());
        }
        denigmaContext.startLogging(defaultLogPath.value_or(std::filesystem::current_path()), argc, argv);
        if (denigmaContext.mnxSchemaPath.has_value() && !denigmaContext.mnxSchema.has_value()) {
            denigmaContext.mnxSchema = fileToString(denigmaContext.mnxSchemaPath.value());
        }
        // no conversion may overwrite a file that another conversion in this run still has to read
        for (const auto& path : pathsToProcess) {
            denigmaContext.scheduledInputPaths.emplace(comparablePath(path));
        }
        // process files
        for (const auto& path : pathsToProcess) {
            denigmaContext.inputFilePath = "";
            denigmaContext.processFile(currentCommand, path, args);
        }
    } catch (const std::exception& e) {
        denigmaContext.logMessage(LogMsg() << e.what(), MessageSeverity::Error);
    }

    denigmaContext.endLogging();

    return denigmaContext.errorOccurred;
}
