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
#include <unordered_set>
#include <optional>
#include <memory>
#include <chrono>
#include <regex>
#include <clocale>

#include "musx/musx.h"

#include "denigma.h"
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
    std::cout << "By default, if the input is a single file, messages are sent to std::cerr." << std::endl;
    std::cout << "If the input is multiple files, messages are logged in `" << programName << "-logs` in the top-level input directory." << std::endl;
    std::cout << std::endl;
    std::cout << "Logging options:" << std::endl;
    std::cout << "  --log [optional-logfile-path]   Always log messages instead of sending them to std::cerr" << std::endl;
    std::cout << "  --no-log                        Always send messages to std::cerr (overrides any other logging options)" << std::endl;
    std::cout << "  --quiet                         Only display errors and warning messages (overrides --verbose)" << std::endl;
    std::cout << "  --verbose                       Verbose output" << std::endl;
    std::cout << std::endl;
    std::cout << "Any relative path is relative to the parent path of the input file or (for log files) to the top-level input folder." << std::endl;

    return 1;
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

    std::vector<const arg_char*> args = denigmaContext.parseOptions(argc, argv);

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

    // stupid omission from C++17 standard
    // see https://stackoverflow.com/questions/73555606/stdunordered-setstdfilesystempath-compile-error-on-clang-and-g-below
    struct PathHash
    {
        auto operator()(const std::filesystem::path& p) const noexcept {
            return std::filesystem::hash_value(p);
        }
    };
    // collect files to process first
    // this avoids potential infinite recursion if input and output are the same format
    std::unordered_set<std::filesystem::path, PathHash> pathsToProcess;
    std::optional<std::filesystem::path> defaultLogPath;

    try {
        for (size_t x = 0; x < args.size(); x++) {
            arg_view option = args[x];
            if (option.rfind(_ARG("--"), 0) == 0) {  // Options start with "--"
                break;
            }
            const std::filesystem::path rawInputPattern = option;
            std::filesystem::path inputFilePattern = rawInputPattern;

            // collect inputs
            const bool isSpecificFileOrDirectory = inputFilePattern.u8string().find('*') == std::string::npos && inputFilePattern.u8string().find('?') == std::string::npos;
            bool isSpecificFile = isSpecificFileOrDirectory && inputFilePattern.has_filename();
            if (std::filesystem::is_directory(inputFilePattern)) {
                isSpecificFile = false;
                if (currentCommand->defaultInputFormat().has_value()) {
                    inputFilePattern /= "*." + std::string(currentCommand->defaultInputFormat().value());
                } else {
                    inputFilePattern /= ""; // assure parent_path returns inputFilePattern
                }
            }
            std::filesystem::path inputDir = inputFilePattern.parent_path();
            if (inputDir.is_relative()) {
                inputDir = std::filesystem::current_path() / inputDir;
            }
            const bool inputDirectoryExists = std::filesystem::is_directory(inputDir);
            if (inputDirectoryExists && !defaultLogPath.has_value()) {
                defaultLogPath = inputDir;
            }
            if (!std::filesystem::is_regular_file(inputFilePattern) && !isSpecificFile && !denigmaContext.logFilePath.has_value()) {
                denigmaContext.logFilePath = "";
            }

            // convert wildcard pattern to regex
            auto wildcardPattern = inputFilePattern.filename().native(); // native format avoids encoding issues
#ifdef _WIN32
            auto regexPattern = std::regex_replace(wildcardPattern, std::wregex(LR"(\*)"), L".*");
            regexPattern = std::regex_replace(regexPattern, std::wregex(LR"(\?)"), L".");
            std::wregex regex(regexPattern);
#else
            auto regexPattern = std::regex_replace(wildcardPattern, std::regex(R"(\*)"), R"(.*)");
            regexPattern = std::regex_replace(regexPattern, std::regex(R"(\?)"), R"(.)");
            std::regex regex(regexPattern);
#endif

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
                    if (entry.is_regular_file() && std::regex_match(entry.path().filename().native(), regex)) {
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
        if (pathsToProcess.size() > 1 && !denigmaContext.logFilePath.has_value()) {
            denigmaContext.logFilePath = "";
        }
        denigmaContext.startLogging(defaultLogPath.value_or(std::filesystem::current_path()), argc, argv);
        if (denigmaContext.mnxSchemaPath.has_value() && !denigmaContext.mnxSchema.has_value()) {
            denigmaContext.mnxSchema = utils::fileToString(denigmaContext.mnxSchemaPath.value());
        }
        // process files
        for (const auto& path : pathsToProcess) {
            denigmaContext.inputFilePath = "";
            denigmaContext.processFile(currentCommand, path, args);
        }
    } catch (const std::exception& e) {
        denigmaContext.logMessage(LogMsg() << e.what(), LogSeverity::Error);
    }

    denigmaContext.endLogging();

    return denigmaContext.errorOccurred;
}
