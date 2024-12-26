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
#include <iostream>
#include <map>
#include <optional>
#include <memory>
#include <chrono>
#include <regex>

#include "musx/musx.h"

#include "denigma.h"
#include "export/export.h"
#include "util/stringutils.h"

static const auto registeredCommands = []()
    {
        std::map <std::string, std::shared_ptr <denigma::ICommand>> retval;
        auto exportCmd = std::make_shared<denigma::ExportCommand>();
        retval.emplace(exportCmd->commandName(), exportCmd);
        return retval;
    }();

static int showHelpPage(const std::string_view& programName)
{
    std::cout << "Usage: " << programName << " <command> <input-pattern> [--options]" << std::endl;
    std::cout << std::endl;

    // General options
    std::cout << "General options:" << std::endl;
    std::cout << "  --exclude folder-name           Exclude the specified folder from recursive searches" << std::endl;
    std::cout << "  --help                          Show this help message and exit" << std::endl;
    std::cout << "  --force                         Overwrite existing file(s)" << std::endl;
    std::cout << "  --part [optional-part-name]     Process named part or first part if name is omitted" << std::endl;
    std::cout << "  --recursive                     Recursively search subdirectories of the input directory" << std::endl;
    std::cout << "  --relative                      Output directories are relative to the input file's parent directory" << std::endl;
    std::cout << "  --all-parts                     Process all parts and score" << std::endl;
    std::cout << "  --version                       Show program version and exit" << std::endl;
    std::cout << std::endl;
    std::cout << "By default, if the input is a single file, messages are sent to std::cerr." << std::endl;
    std::cout << "If the input is a multiple files, messages are logged in `" << programName << "-logs` in the top-level input directory." << std::endl;
    std::cout << std::endl;
    std::cout << "Logging options:" << std::endl;
    std::cout << "  --log [optional-logfile-path]   Always log messages instead of sending them to std::cerr" << std::endl;
    std::cout << "  --no-log                        Always send messages to std::cerr (overrides any other logging options)" << std::endl;
    std::cout << "  --verbose                       Verbose output" << std::endl;

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
    return 1;
}

using namespace denigma;

int _MAIN(int argc, arg_char* argv[])
{
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    std::string programName = std::filesystem::path(argv[0]).stem().u8string();
    
    if (argc < 2) {
        return showHelpPage(programName);
    }

    DenigmaContext denigmaContext;
    denigmaContext.programName = programName;
    bool showVersion = false;
    bool showHelp = false;
    std::vector<const arg_char*> args;
    for (int x = 1; x < argc; x++) {
        auto getNextArg = [&]() -> arg_view {
                if (x + 1 < argc) {
                    arg_view arg(argv[x + 1]);
                    if (x < (argc - 1) && arg.rfind(_ARG("--"), 0) != 0) {
                        x++;
                        return arg;
                    }
                }
                return {};
            };
        const arg_view next(argv[x]);
        if (next == _ARG("--version")) {
            showVersion = true;
        } else if (next == _ARG("--help")) {
            showHelp = true;
        } else if (next == _ARG("--force")) {
            denigmaContext.overwriteExisting = true;
        } else if (next == _ARG("--log")) {
            denigmaContext.logFilePath = getNextArg();
        } else if (next == _ARG("--no-log")) {
            denigmaContext.noLog = true;
        } else if (next == _ARG("--part")) {
            denigmaContext.partName = _ARG_CONV(getNextArg());
        } else if (next == _ARG("--all-parts")) {
            denigmaContext.allPartsAndScore = true;
        } else if (next == _ARG("--recursive")) {
            denigmaContext.recursiveSearch = true;
        } else if (next == _ARG("--exclude-folder")) {
            auto option = getNextArg();
            if (!option.empty()) {
                denigmaContext.excludeFolder = option;
            }
        } else if (next == _ARG("--relative")) {
            denigmaContext.relativeOutputDirs = true;
        } else if (next == _ARG("--verbose")) {
            denigmaContext.verbose = true;
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
    if (args.size() < 1) {
        std::cerr << "Not enough arguments passed" << std::endl;
        return showHelpPage(programName);
    }

    const auto currentCommand = [args]() -> std::shared_ptr<ICommand> {
            auto it = registeredCommands.find(arg_string(args[0]));
            if (it != registeredCommands.end()) {
                return it->second;
            } else {
                std::cerr << "Unknown command: " << args[0] << std::endl;
                return nullptr;
            }
        }();
    if (!currentCommand) {
        return showHelpPage(programName);
    }

    musx::util::Logger::setCallback([&denigmaContext](musx::util::Logger::LogLevel logLevel, const std::string& msg) {
            LogSeverity logSeverity = [logLevel]() {
                    switch (logLevel) {
                        default:
                        case musx::util::Logger::LogLevel::Info: return LogSeverity::Info;
                        case musx::util::Logger::LogLevel::Warning: return LogSeverity::Warning;
                        case musx::util::Logger::LogLevel::Error: return LogSeverity::Error;
                        case musx::util::Logger::LogLevel::Verbose: return LogSeverity::Verbose;
                    }
                }();
            denigmaContext.logMessage(LogMsg() << msg, logSeverity);
        });

    bool errorOccurred = false;
    try {
        const std::filesystem::path rawInputPattern = args[1];
        std::filesystem::path inputFilePattern = rawInputPattern;

        // collect inputs
        bool isSpecificFileOrDirectory = inputFilePattern.u8string().find('*') == std::string::npos && inputFilePattern.u8string().find('?') == std::string::npos;
        if (std::filesystem::is_directory(inputFilePattern)) {
            if (currentCommand->defaultInputFormat().has_value()) {
                inputFilePattern /= "*." + std::string(currentCommand->defaultInputFormat().value());
            } else {
                inputFilePattern /= ""; // assure parent_path returns inputFilePattern
            }
        }
        std::filesystem::path inputDir = inputFilePattern.parent_path();
        auto wildcardPattern = inputFilePattern.filename().u8string();
        bool inputIsOneFile = std::filesystem::is_regular_file(inputFilePattern);
        
        if (!inputIsOneFile && !denigmaContext.logFilePath.has_value()) {
            denigmaContext.logFilePath = "";
        }
        denigmaContext.startLogging(inputDir, argc, argv);

        if (isSpecificFileOrDirectory && !std::filesystem::exists(rawInputPattern)) {
            throw std::runtime_error("Input path " + inputFilePattern.u8string() + " does not exist or is not a file or directory.");
        }

        // convert wildcard pattern to regex
        auto regexPattern = std::regex_replace(wildcardPattern, std::regex(R"(\*)"), R"(.*)");
        regexPattern = std::regex_replace(regexPattern, std::regex(R"(\?)"), R"(.)");
        std::regex regex(regexPattern);

        // collect files to process first
        // this avoids potential infinite recursion if input and output are the same format
        std::vector<std::filesystem::path> pathsToProcess;
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
                    denigmaContext.logMessage(LogMsg() << "considered file " << entry.path().u8string(), LogSeverity::Verbose);
                }
                if (entry.is_regular_file() && std::regex_match(entry.path().filename().u8string(), regex)) {
                    auto inputFilePath = entry.path();
                    if (currentCommand->canProcess(inputFilePath)) {
                        pathsToProcess.push_back(inputFilePath);
                    } else if (inputIsOneFile) {
                        denigmaContext.logMessage(LogMsg() << "Invalid input format.", LogSeverity::Error);
                        showHelpPage(denigmaContext.programName);
                        errorOccurred = true;
                        break;
                    }
                }    
            }
        };
        if (denigmaContext.recursiveSearch) {
            std::filesystem::recursive_directory_iterator it(inputDir);
            iterate(it);
        } else {
            std::filesystem::directory_iterator it(inputDir);
            iterate(it);
        }
        for (const auto& path : pathsToProcess) {
            denigmaContext.inputFilePath = "";
            if (!processFile(currentCommand, path, args, denigmaContext)) {
                errorOccurred = true;
            }
        }
    }
    catch (const std::exception& e) {
        denigmaContext.logMessage(LogMsg() << e.what(), LogSeverity::Error);
        errorOccurred = true;
    }

    denigmaContext.endLogging();

    return errorOccurred;
}
