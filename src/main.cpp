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
#include "export.h"
#include "util/stringutils.h"

#ifdef _WIN32
#define _ARG(S) L##S
#define _ARG_CONV(S) (stringutils::wstringToString(std::wstring(S)))
#define _MAIN wmain
#else
#define _ARG(S) S
#define _ARG_CONV(S) S
#define _MAIN main
#endif

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
    std::cout << "  --help                          Show this help message and exit" << std::endl;
    std::cout << "  --force                         Overwrite existing file(s)" << std::endl;
    std::cout << "  --log [optional-logfile-path]   Log messages instead of sending them to std::cerr" << std::endl;
    std::cout << "  --part [optional-part-name]     Process named part or first part if name is omitted" << std::endl;
    std::cout << "  --recursive                     Recursively search subdirectories of the input directory" << std::endl;
    std::cout << "  --relative                      Output directories are relative to the input file's parent directory" << std::endl;
    std::cout << "  --all-parts                     Process all parts and score" << std::endl;
    std::cout << "  --version                       Show program version and exit" << std::endl;

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

namespace denigma {

std::string getTimeStamp(const std::string& fmt)
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm localTime;
#ifdef _WIN32
    localtime_s(&localTime, &time_t_now); // Windows
#else
    localtime_r(&time_t_now, &localTime); // Linux/Unix
#endif
    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, fmt.c_str());
    return timestamp.str();
}

void DenigmaOptions::logMessage(LogMsg&& msg, LogSeverity severity) const
{
    auto getSeverityStr = [severity]() -> std::string {
            switch (severity) {
            default:
            case LogSeverity::Info: return "";
            case LogSeverity::Warning: return "[WARNING] ";
            case LogSeverity::Error: return "[***ERROR***] ";
            }
        };
    msg.flush();
    std::string inputFile = inputFilePath.filename().u8string();
    if (!inputFile.empty()) {
        inputFile += ' ';
    }
    if (logFile && logFile->is_open()) {
        LogMsg prefix = LogMsg() << "[" << getTimeStamp("%Y-%m-%d %H:%M:%S") << "] " << inputFile;
        prefix.flush();
        *logFile << prefix.str() << getSeverityStr() << msg.str() << std::endl;
        if (severity == LogSeverity::Error) {
            *logFile << prefix.str() << "PROCESSING ABORTED" << std::endl;
        }
        if (severity != LogSeverity::Error) {
            return;
        }
    }

#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_ERROR_HANDLE);
    if (hConsole && hConsole != INVALID_HANDLE_VALUE) {
        DWORD consoleMode{};
        if (::GetConsoleMode(hConsole, &consoleMode)) {
            std::wstringstream wMsg;
            wMsg << stringutils::stringToWstring(msg.str()) << std::endl;
            DWORD written{};
            if (::WriteConsoleW(hConsole, wMsg.str().data(), static_cast<DWORD>(wMsg.str().size()), &written, nullptr)) {
                return;
            }
            std::wcerr << L"Failed to write message to console: " << ::GetLastError() << std::endl;
        }
    }
    std::wcerr << stringutils::stringToWstring(msg.str()) << std::endl;
#else
    std::cerr << msg.str() << std::endl;
#endif
}

bool validatePathsAndOptions(const std::filesystem::path& outputFilePath, const DenigmaOptions& options)
{
    if (options.inputFilePath == outputFilePath) {
        options.logMessage(LogMsg() << outputFilePath.u8string() << ": " << "Input and output are the same. No action taken.");
        return false;
    }

    if (std::filesystem::exists(outputFilePath)) {
        if (options.overwriteExisting) {
            options.logMessage(LogMsg() << "Overwriting " << outputFilePath.u8string());
        } else {
            options.logMessage(LogMsg() << outputFilePath.u8string() << " exists. Use --force to overwrite it.");
            return false;
        }
    } else {
        options.logMessage(LogMsg() << "Output: " << outputFilePath.u8string());
    }

    return true;
}

/** returns true if input path is a directory */
bool createDirectoryIfNeeded(const std::filesystem::path& path)
{
    // std::filesystem::path::exists() somethimes erroneously throws on Windows network drives.
    // this works around that issue.
    bool exists = false;
    try {
        exists = std::filesystem::exists(path);
    } catch(...) {}
    if (!exists) {
        std::filesystem::create_directories(path.parent_path());
    }
    if (std::filesystem::is_directory(path) || (!exists && !path.has_extension())) {
        std::filesystem::create_directories(path);
        return true;
    }
    return false;
}

void DenigmaOptions::startLogging(const std::filesystem::path& defaultLogPath, int argc, arg_char* argv[])
{
    if (logFilePath.has_value() && !logFile) {
        auto& path = logFilePath.value();
        if (path.empty()) {
            path = defaultLogPath / (programName + "-logs");
        }
        if (createDirectoryIfNeeded(path)) {
            std::string logFileName = programName + "-" + getTimeStamp("%Y%m%d-%H%M%S") + ".log";
            path /= logFileName;
        }
        bool appending = std::filesystem::is_regular_file(path);
        logFile = std::make_shared<std::ofstream>();
        logFile->exceptions(std::ios::failbit | std::ios::badbit);
        logFile->open(path, std::ios::app);
        if (appending) {
            *logFile << std::endl;
        }
        logMessage(LogMsg() << "======= START =======");
        logMessage(LogMsg() << programName << " executed with the following arguments:");
        LogMsg args;
        args << programName << " ";
        for (int i = 1; i < argc; i++) {
            args << std::string(arg_string(argv[i])) << " ";
        }
        logMessage(std::move(args));
    }   
}

void DenigmaOptions::endLogging()
{
    if (logFilePath.has_value()) {
        inputFilePath = "";
        logMessage(LogMsg());
        logMessage(LogMsg() << programName << " processing complete");
        logMessage(LogMsg() << "======== END ========");
        logFile.reset();
    }
}

static bool processFile(const std::shared_ptr<ICommand>& currentCommand, const std::filesystem::path inputFilePath, const std::vector<const arg_char*>& args, DenigmaOptions& options)
{
    try {
        if (!std::filesystem::is_regular_file(inputFilePath)) {
            throw std::runtime_error("Input file " + inputFilePath.u8string() + " does not exist or is not a file.");
        }
        constexpr char kProcessingMessage[] = "Processing File: ";
        constexpr size_t kProcessingMessageSize = sizeof(kProcessingMessage) - 1; // account for null terminator.
        std::string delimiter(kProcessingMessageSize + inputFilePath.u32string().size(), '='); // use u32string().size to get actual number of characters displayed
        // log header for each file
        options.logMessage(LogMsg());
        options.logMessage(LogMsg() << delimiter);
        options.logMessage(LogMsg() << kProcessingMessage << inputFilePath.u8string());
        options.logMessage(LogMsg() << delimiter);
        options.inputFilePath = inputFilePath; // assign after logging the header

        const auto enigmaXml = currentCommand->processInput(inputFilePath, options);

        auto calcOutpuFilePath = [inputFilePath, &options](const std::filesystem::path& path, const std::string& format) -> std::filesystem::path {
            std::filesystem::path retval = path;
            if (options.relativeOutputDirs) {
                retval = inputFilePath.parent_path() / retval;
            }
            if (createDirectoryIfNeeded(retval)) {
                std::filesystem::path outputFileName = inputFilePath.filename();
                outputFileName.replace_extension(format);
                retval = retval / outputFileName;
            }
            return retval;
        };

        // Process output options
        bool outputFormatSpecified = false;
        for (size_t i = 0; i < args.size(); ++i) {
            arg_string option = args[i];
            if (option.rfind(_ARG("--"), 0) == 0) {  // Options start with "--"
                arg_string outputFormat = option.substr(2);
                std::filesystem::path outputFilePath = (i + 1 < args.size() && arg_string(args[i + 1]).rfind(_ARG("--"), 0) != 0)
                                                     ? std::filesystem::path(args[++i])
                                                     : inputFilePath.parent_path();
                currentCommand->processOutput(enigmaXml, calcOutpuFilePath(outputFilePath, outputFormat), options);
                outputFormatSpecified = true;
            }
        }
        if (!outputFormatSpecified) {
            const auto& defaultFormat = currentCommand->defaultOutputFormat();
            if (defaultFormat.has_value()) {
                currentCommand->processOutput(enigmaXml, calcOutpuFilePath(inputFilePath.parent_path(), std::string(defaultFormat.value())), options);
            }
        }
        return true;
    } catch (const musx::xml::load_error& ex) {
        options.logMessage(LogMsg() << "Load XML failed: " << ex.what(), LogSeverity::Error);
    } catch (const std::exception& e) {
        options.logMessage(LogMsg() << e.what(), LogSeverity::Error);
    }

    return false;
}

} // namespace denigma

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

    DenigmaOptions options;
    options.programName = programName;
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
            options.overwriteExisting = true;
        } else if (next == _ARG("--log")) {
            options.logFilePath = getNextArg();
        } else if (next == _ARG("--part")) {
            options.partName = _ARG_CONV(getNextArg());
        } else if (next == _ARG("--all-parts")) {
            options.allPartsAndScore = true;
        } else if (next == _ARG("--recursive")) {
            options.recursiveSearch = true;
        } else if (next == _ARG("--relative")) {
            options.relativeOutputDirs = true;
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

    musx::util::Logger::setCallback([&options](musx::util::Logger::LogLevel logLevel, const std::string& msg) {
            LogSeverity logSeverity = [logLevel]() {
                    switch (logLevel) {
                        default:
                        case musx::util::Logger::LogLevel::Info: return LogSeverity::Info;
                        case musx::util::Logger::LogLevel::Warning: return LogSeverity::Warning;
                        case musx::util::Logger::LogLevel::Error: return LogSeverity::Error;
                    }
                }();
            options.logMessage(LogMsg() << msg, logSeverity);
        });

    bool errorOccurred = false;
    try {
        std::filesystem::path inputFilePattern = args[1];

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
        options.startLogging(inputDir, argc, argv);
        auto wildcardPattern = inputFilePattern.filename().u8string();

        if (isSpecificFileOrDirectory && !std::filesystem::exists(args[1])) {
            throw std::runtime_error("Input path " + inputFilePattern.u8string() + " does not exist or is not a file or directory.");
        }
        bool inputIsOneFile = std::filesystem::is_regular_file(inputFilePattern);

        // convert wildcard pattern to regex
        auto regexPattern = std::regex_replace(wildcardPattern, std::regex(R"(\*)"), R"(.*)");
        regexPattern = std::regex_replace(regexPattern, std::regex(R"(\?)"), R"(.)");
        std::regex regex(regexPattern);

        auto iterate = [&](auto& iterator) {
            for (const auto& entry : iterator) {
                if (entry.is_regular_file() && std::regex_match(entry.path().filename().u8string(), regex)) {
                    auto inputFilePath = entry.path();
                    if (currentCommand->canProcess(inputFilePath)) {
                        options.inputFilePath = "";
                        if (!processFile(currentCommand, inputFilePath, args, options)) {
                            errorOccurred = true;
                        }
                    } else if (inputIsOneFile) {
                            std::cerr << "Invalid input format." << std::endl;
                            showHelpPage(options.programName);
                            errorOccurred = true;
                            break;
                    }
                }
            }
        };
        if (options.recursiveSearch) {
            std::filesystem::recursive_directory_iterator it(inputDir);
            iterate(it);
        } else {
            std::filesystem::directory_iterator it(inputDir);
            iterate(it);
        }        
    }
    catch (const std::exception& e) {
        options.logMessage(LogMsg() << e.what(), LogSeverity::Error);
        errorOccurred = true;
    }

    options.endLogging();

    return errorOccurred;
}
