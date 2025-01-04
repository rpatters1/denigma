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
#include "denigma.h"

namespace denigma {

std::vector<const arg_char*> DenigmaContext::parseOptions(int argc, arg_char* argv[])
{
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
            overwriteExisting = true;
        } else if (next == _ARG("--log")) {
            logFilePath = getNextArg();
        } else if (next == _ARG("--no-log")) {
            noLog = true;
        } else if (next == _ARG("--part")) {
            partName = _ARG_CONV(getNextArg());
        } else if (next == _ARG("--all-parts")) {
            allPartsAndScore = true;
        } else if (next == _ARG("--recursive")) {
            recursiveSearch = true;
        } else if (next == _ARG("--exclude-folder")) {
            auto option = getNextArg();
            if (!option.empty()) {
                excludeFolder = option;
            }
        } else if (next == _ARG("--verbose")) {
            verbose = true;
        // Specific options for `massage` command
        } else if (next == _ARG("--finale-file")) {
            auto option = getNextArg();
            if (!option.empty()) {
                finaleFilePath = option;
            }
        } else if (next == _ARG("--target")) {
            setMassageTarget(std::string(_ARG_CONV(getNextArg())));
        } else if (next == _ARG("--refloat-rests")) {
            refloatRests = true;
        } else if (next == _ARG("--no-refloat-rests")) {
            refloatRests = false;
        } else if (next == _ARG("--extend-ottavas-left")) {
            extendOttavasLeft = true;
        } else if (next == _ARG("--no-extend-ottavas-left")) {
            extendOttavasLeft = false;
        } else if (next == _ARG("--extend-ottavas-right")) {
            extendOttavasRight = true;
        } else if (next == _ARG("--no-extend-ottavas-right")) {
            extendOttavasRight = false;
        } else if (next == _ARG("--fermata-whole-rests")) {
            fermataWholeRests = true;
        } else if (next == _ARG("--no-fermata-whole-rests")) {
            fermataWholeRests = false;
        } else {
            args.push_back(argv[x]);
        }
    }
    return args;
}

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

void DenigmaContext::logMessage(LogMsg&& msg, LogSeverity severity) const
{
    auto getSeverityStr = [severity]() -> std::string {
            switch (severity) {
            default:
            case LogSeverity::Info: return "";
            case LogSeverity::Warning: return "[WARNING] ";
            case LogSeverity::Error: return "[***ERROR***] ";
            }
        };
    if (severity == LogSeverity::Verbose && !verbose) {
        return;
    }
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
            wMsg << utils::stringToWstring(msg.str()) << std::endl;
            DWORD written{};
            if (::WriteConsoleW(hConsole, wMsg.str().data(), static_cast<DWORD>(wMsg.str().size()), &written, nullptr)) {
                return;
            }
            std::wcerr << L"Failed to write message to console: " << ::GetLastError() << std::endl;
        }
    }
    std::wcerr << utils::stringToWstring(msg.str()) << std::endl;
#else
    std::cerr << msg.str() << std::endl;
#endif
}

bool DenigmaContext::validatePathsAndOptions(const std::filesystem::path& outputFilePath) const
{
    if (inputFilePath == outputFilePath) {
        logMessage(LogMsg() << outputFilePath.u8string() << ": " << "Input and output are the same. No action taken.");
        return false;
    }

    if (std::filesystem::exists(outputFilePath)) {
        if (overwriteExisting) {
            logMessage(LogMsg() << "Overwriting " << outputFilePath.u8string());
        } else {
            logMessage(LogMsg() << outputFilePath.u8string() << " exists. Use --force to overwrite it.");
            return false;
        }
    } else {
        logMessage(LogMsg() << "Output: " << outputFilePath.u8string());
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

void DenigmaContext::startLogging(const std::filesystem::path& defaultLogPath, int argc, arg_char* argv[])
{
    if (!noLog && logFilePath.has_value() && !logFile) {
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

void DenigmaContext::endLogging()
{
    if (!noLog && logFilePath.has_value()) {
        inputFilePath = "";
        logMessage(LogMsg());
        logMessage(LogMsg() << programName << " processing complete");
        logMessage(LogMsg() << "======== END ========");
        logFile.reset();
    }
}

bool processFile(const std::shared_ptr<ICommand>& currentCommand, const std::filesystem::path inputFilePath, const std::vector<const arg_char*>& args, DenigmaContext& denigmaContext)
{
    try {
        if (!std::filesystem::is_regular_file(inputFilePath)) {
            throw std::runtime_error("Input file " + inputFilePath.u8string() + " does not exist or is not a file.");
        }
        constexpr char kProcessingMessage[] = "Processing File: ";
        constexpr size_t kProcessingMessageSize = sizeof(kProcessingMessage) - 1; // account for null terminator.
        std::string delimiter(kProcessingMessageSize + inputFilePath.u32string().size(), '='); // use u32string().size to get actual number of characters displayed
        // log header for each file
        denigmaContext.logMessage(LogMsg());
        denigmaContext.logMessage(LogMsg() << delimiter);
        denigmaContext.logMessage(LogMsg() << kProcessingMessage << inputFilePath.u8string());
        denigmaContext.logMessage(LogMsg() << delimiter);
        denigmaContext.inputFilePath = inputFilePath; // assign after logging the header

        const auto xmlBuffer = currentCommand->processInput(inputFilePath, denigmaContext);

        auto calcOutpuFilePath = [inputFilePath, &denigmaContext](const std::filesystem::path& path, const std::string& format) -> std::filesystem::path {
            std::filesystem::path retval = path;
            if (retval.is_relative()) {
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
                currentCommand->processOutput(xmlBuffer, calcOutpuFilePath(outputFilePath, outputFormat), inputFilePath, denigmaContext);
                outputFormatSpecified = true;
            }
        }
        if (!outputFormatSpecified) {
            const auto& defaultFormat = currentCommand->defaultOutputFormat(inputFilePath);
            if (defaultFormat.has_value()) {
                currentCommand->processOutput(xmlBuffer, calcOutpuFilePath(inputFilePath.parent_path(), std::string(defaultFormat.value())), inputFilePath, denigmaContext);
            }
        }
        return true;
    } catch (const musx::xml::load_error& ex) {
        denigmaContext.logMessage(LogMsg() << "Load XML failed: " << ex.what(), LogSeverity::Error);
    } catch (const std::exception& e) {
        denigmaContext.logMessage(LogMsg() << e.what(), LogSeverity::Error);
    }

    return false;
}

} // namespace denigma
