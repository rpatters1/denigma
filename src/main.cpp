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

#include "musx/musx.h"

#include "denigma.h"
#include "export.h"
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
    std::cout << "  --help                          Show this help message and exit" << std::endl;
    std::cout << "  --force                         Overwrite existing file(s)" << std::endl;
    std::cout << "  --log [optional-logfile-path]   Log messages instead of sending them to std::cerr" << std::endl;
    std::cout << "  --part [optional-part-name]     Process named part or first part if name is omitted" << std::endl;
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

void logMessage(LogMsg&& msg, const DenigmaOptions& options, LogSeverity severity)
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
    std::string inputFile = options.inputFilePath.filename().u8string();
    if (!inputFile.empty()) {
        inputFile += ' ';
    }
    if (options.logFile && options.logFile->is_open()) {
        LogMsg prefix = LogMsg() << "[" << getTimeStamp("%Y-%m-%d %H:%M:%S") << "] " << inputFile;
        prefix.flush();
        *options.logFile << prefix.str() << getSeverityStr() << msg.str() << std::endl;
        if (severity == LogSeverity::Error) {
            *options.logFile << prefix.str() << "PROCESSING ABORTED" << std::endl;
        }
        return;
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
    std::wcerr << stringutils::stringToWstring(msg.str());
    std::wcerr << std::endl;
#else
    std::cerr << msg.str() << std::endl;
#endif
}

bool validatePathsAndOptions(const std::filesystem::path& outputFilePath, const DenigmaOptions& options)
{
    if (options.inputFilePath == outputFilePath) {
        logMessage(LogMsg() << outputFilePath.u8string() << ": " << "Input and output are the same. No action taken.", options);
        return false;
    }

    if (std::filesystem::exists(outputFilePath)) {
        if (options.overwriteExisting) {
            logMessage(LogMsg() << "Overwriting " << outputFilePath.u8string(), options);
        } else {
            logMessage(LogMsg() << outputFilePath.u8string() << " exists. Use --force to overwrite it.", options);
            return false;
        }
    } else {
        logMessage(LogMsg() << "Output: " << outputFilePath.u8string(), options);
    }

    return true;
}

void DenigmaOptions::startLogging(const std::filesystem::path& defaultLogPath, int argc, arg_char* argv[])
{
    if (logFilePath.has_value() && !logFile) {
        auto& path = logFilePath.value();
        if (path.empty()) {
            path = defaultLogPath;
        }
        if (!path.has_filename() || std::filesystem::is_directory(path)) {
            std::string logFileName = programName + "-" + getTimeStamp("%Y%m%d-%H%M%S") + ".log";
            path /= (programName + "-logs");
            std::filesystem::create_directories(path);
            path /= logFileName;
        }
        bool appending = std::filesystem::is_regular_file(path);
        logFile = std::make_shared<std::ofstream>();
        logFile->exceptions(std::ios::failbit | std::ios::badbit);
        logFile->open(path, std::ios::app);
        if (appending) {
            *logFile << std::endl;
        }
        logMessage(LogMsg() << "====== STARTED ======", *this);
        logMessage(LogMsg() << programName << " executed with the following arguments:", *this);
        LogMsg args;
        args << programName << " ";
        for (int i = 1; i < argc; i++) {
            args << argv[i] << " ";
        }
        logMessage(std::move(args), *this);
        logMessage(LogMsg(), *this);
    }
}

} // namespace denigma

using namespace denigma;

#ifdef _WIN32
#define _ARG(S) L##S
#define _ARG_CONV(S) (stringutils::wstringToString(std::wstring(S)))
#define _MAIN wmain
#else
#define _ARG(S) S
#define _ARG_CONV(S) S
#define _MAIN main
#endif
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
        } else if (next == _ARG("--all-parts")) {
            options.allPartsAndScore = true;
        } else if (next == _ARG("--part")) {
            options.partName = _ARG_CONV(getNextArg());
        } else if (next == _ARG("--log")) {
            options.logFilePath = getNextArg();
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
            logMessage(LogMsg() << msg, options, logSeverity);
        });

    bool errorOccurred = false;
    // ToDo: This code needs to process input patterns in a loop, but
    //          the try/catch needs to enclose each actual input file
    //          so that we can log results and continue
    try {
        const std::filesystem::path inputFilePath = args[1];
        const std::filesystem::path defaultPath = inputFilePath.parent_path();
        options.startLogging(defaultPath, argc, argv);
        if (!std::filesystem::is_regular_file(inputFilePath)) {
            logMessage(LogMsg() << "Input file " << inputFilePath.u8string() << " does not exist or is not a file.", options, LogSeverity::Error);
            return 1; // when this is a loop iteration, this should continue to the next iteration but maintain the return value so the error is flagged on exit
        }
        constexpr char kProcessingMessage[] = "Processing File: ";
        std::string delimiter(sizeof(kProcessingMessage) + inputFilePath.u32string().size(), '=');
        // log header for each file
        logMessage(LogMsg() << delimiter, options);
        logMessage(LogMsg() << kProcessingMessage << inputFilePath.u8string(), options);
        logMessage(LogMsg() << delimiter, options);
        options.inputFilePath = inputFilePath; // assign after logging the header

        // Find and call the input processor
        if (inputFilePath.extension().empty()) {
            return showHelpPage(options.programName);
        }
        const auto enigmaXml = currentCommand->processInput(inputFilePath, options);

        auto calcFilePath = [inputFilePath](const std::filesystem::path& path, const std::string& format) -> std::filesystem::path {
            std::filesystem::path retval = path;
            if (std::filesystem::is_directory(retval)) {
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
                                                     : defaultPath;
                currentCommand->processOutput(enigmaXml, calcFilePath(outputFilePath, outputFormat), options);
                outputFormatSpecified = true;
            }
        }
        if (!outputFormatSpecified) {
            const auto& defaultFormat = currentCommand->defaultOutputFormat();
            if (defaultFormat.has_value()) {
                currentCommand->processOutput(enigmaXml, calcFilePath(defaultPath, std::string(defaultFormat.value())), options);
            }
        }
    } catch (const musx::xml::load_error& ex) {
        logMessage(LogMsg() << "Load XML failed: " << ex.what(), options, LogSeverity::Error);
        errorOccurred = true;
    } catch (const std::exception& e) {
        logMessage(LogMsg() << e.what(), options, LogSeverity::Error);
        errorOccurred = true;
    }

    return errorOccurred;
}
