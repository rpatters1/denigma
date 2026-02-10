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
#include <limits>

namespace denigma {

namespace {

musx::util::SvgConvert::SvgUnit parseSvgUnitOption(const std::string& input)
{
    const std::string value = utils::toLowerCase(input);
    if (value == "none") return musx::util::SvgConvert::SvgUnit::None;
    if (value == "px") return musx::util::SvgConvert::SvgUnit::Pixels;
    if (value == "pt") return musx::util::SvgConvert::SvgUnit::Points;
    if (value == "pc") return musx::util::SvgConvert::SvgUnit::Picas;
    if (value == "cm") return musx::util::SvgConvert::SvgUnit::Centimeters;
    if (value == "mm") return musx::util::SvgConvert::SvgUnit::Millimeters;
    if (value == "in") return musx::util::SvgConvert::SvgUnit::Inches;
    throw std::invalid_argument("Invalid value for --svg-unit: " + input + ". Expected one of: none, px, pt, pc, cm, mm, in.");
}

void appendShapeDefIds(const std::string& list, std::vector<musx::dom::Cmper>& out)
{
    if (list.empty()) {
        throw std::invalid_argument("Missing value for --shape-def");
    }
    size_t start = 0;
    while (start <= list.size()) {
        const size_t comma = list.find(',', start);
        const std::string token = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (token.empty()) {
            throw std::invalid_argument("Invalid --shape-def list: \"" + list + "\"");
        }
        long long parsed = 0;
        try {
            parsed = std::stoll(token);
        } catch (...) {
            throw std::invalid_argument("Invalid --shape-def value: \"" + token + "\"");
        }
        if (parsed <= 0 || parsed > std::numeric_limits<musx::dom::Cmper>::max()) {
            throw std::invalid_argument("Invalid --shape-def value: \"" + token + "\"");
        }
        const auto id = static_cast<musx::dom::Cmper>(parsed);
        bool seen = false;
        for (const auto existing : out) {
            if (existing == id) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            out.push_back(id);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
}

} // namespace

std::vector<const arg_char*> DenigmaContext::parseOptions(int argc, arg_char* argv[])
{
    std::vector<const arg_char*> args;
    bool svgScaleSpecified = false;
    bool svgPageScaleExplicitlyEnabled = false;
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
        } else if (next == _ARG("--about")) {
            showAbout = true;
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
        } else if (next == _ARG("--quiet")) {
            quiet = true;
        } else if (next == _ARG("--verbose")) {
            verbose = true;
        } else if (next == _ARG("--no-validate")) {
            noValidate = true;
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
        } else if (next == _ARG("--include-tempo-tool")) {
            includeTempoTool = true;
        } else if (next == _ARG("--no-include-tempo-tool")) {
            includeTempoTool = false;
        } else if (next == _ARG("--pretty-print")) {
            try {
                int value = std::stoi(std::string(_ARG_CONV(getNextArg())));
                indentSpaces = (value >= 0) ? value : JSON_INDENT_SPACES;
            } catch (...) {
                indentSpaces = JSON_INDENT_SPACES;
            }
        } else if (next == _ARG("--no-pretty-print")) {
            indentSpaces = std::nullopt;
        } else if (next == _ARG("--mnx-schema")) {
            std::filesystem::path schemaPath = getNextArg();
            if (!schemaPath.empty()) {
                mnxSchemaPath = schemaPath;
            }
        } else if (next == _ARG("--shape-def")) {
            const std::string shapeDefList = std::string(_ARG_CONV(getNextArg()));
            appendShapeDefIds(shapeDefList, svgShapeDefs);
        } else if (next == _ARG("--svg-unit")) {
            const std::string unitValue = std::string(_ARG_CONV(getNextArg()));
            if (unitValue.empty()) {
                throw std::invalid_argument("Missing value for --svg-unit");
            }
            svgUnit = parseSvgUnitOption(unitValue);
        } else if (next == _ARG("--svg-page-scale")) {
            svgUsePageScale = true;
            svgPageScaleExplicitlyEnabled = true;
        } else if (next == _ARG("--no-svg-page-scale")) {
            svgUsePageScale = false;
        } else if (next == _ARG("--svg-scale")) {
            const std::string scaleValue = std::string(_ARG_CONV(getNextArg()));
            if (scaleValue.empty()) {
                throw std::invalid_argument("Missing value for --svg-scale");
            }
            try {
                svgScale = std::stod(scaleValue);
            } catch (...) {
                throw std::invalid_argument("Invalid value for --svg-scale: " + scaleValue);
            }
            if (svgScale <= 0.0) {
                throw std::invalid_argument("Invalid value for --svg-scale: " + scaleValue + " (must be > 0)");
            }
            svgScaleSpecified = true;
#ifdef DENIGMA_TEST // this is defined on the command line by the test program
        } else if (next == _ARG("--testing")) {
            testOutput = true;
#endif
        } else {
            args.push_back(argv[x]);
        }
    }
    if (svgScaleSpecified && !svgPageScaleExplicitlyEnabled) {
        svgUsePageScale = false;
    }
    if (svgScaleSpecified && svgPageScaleExplicitlyEnabled) {
        throw std::invalid_argument("Cannot combine --svg-scale with page-format scaling. Use --no-svg-page-scale with --svg-scale.");
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

void DenigmaContext::logMessage(LogMsg&& msg, bool alwaysShow, LogSeverity severity) const
{
    auto getSeverityStr = [severity]() -> std::string {
            switch (severity) {
            default:
            case LogSeverity::Info: return "";
            case LogSeverity::Warning: return "[WARNING] ";
            case LogSeverity::Error: return "[***ERROR***] ";
            }
        };
    if (!alwaysShow) {
        if (severity == LogSeverity::Verbose && (!verbose || quiet)) {
            return;
        }
        if (severity == LogSeverity::Info && quiet) {
            return;
        }
    }
    if (severity == LogSeverity::Error) {
        errorOccurred = true;
    }
    msg.flush();
    const auto inputFileUtf8 = inputFilePath.filename().u8string();
    std::string inputFile;
    utils::appendUtf8(inputFile, inputFileUtf8);
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

#if defined(_WIN32) && !defined(DENIGMA_TEST)
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
        logMessage(LogMsg() << utils::asUtf8Bytes(outputFilePath) << ": " << "Input and output are the same. No action taken.");
        return false;
    }

    if (std::filesystem::exists(outputFilePath)) {
        if (overwriteExisting) {
            logMessage(LogMsg() << "Overwriting " << utils::asUtf8Bytes(outputFilePath));
        } else {
            logMessage(LogMsg() << utils::asUtf8Bytes(outputFilePath) << " exists. Use --force to overwrite it.", LogSeverity::Warning);
            return false;
        }
    } else {
        logMessage(LogMsg() << "Output: " << utils::asUtf8Bytes(outputFilePath));
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
    if (!exists && !path.parent_path().empty()) {
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
    errorOccurred = false;
    if (!noLog && logFilePath.has_value() && !logFile) {
        if (forTestOutput()) {
            std::cout << "Logging to " << utils::asUtf8Bytes(logFilePath.value()) << std::endl;
            return;
        }
        auto& path = logFilePath.value();
        if (path.empty()) {
            path = programName + "-logs";
        }
        if (path.is_relative()) {
            path = defaultLogPath / path;
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
        logMessage(LogMsg() << "======= START =======", true);
        logMessage(LogMsg() << programName << " executed with the following arguments:", true);
        LogMsg args;
        args << programName << " ";
        for (int i = 1; i < argc; i++) {
            args << std::string(arg_string(argv[i])) << " ";
        }
        logMessage(std::move(args), true);
    }   
}

void DenigmaContext::endLogging()
{
    if (!noLog && logFilePath.has_value() && !forTestOutput()) {
        inputFilePath = "";
        logMessage(LogMsg(), true);
        logMessage(LogMsg() << programName << " processing complete", true);
        logMessage(LogMsg() << "======== END ========", true);
        logFile.reset();
    }
}

void DenigmaContext::processFile(const std::shared_ptr<ICommand>& currentCommand, const std::filesystem::path inpFilePath, const std::vector<const arg_char*>& args)
{
    try {
        if (!std::filesystem::is_regular_file(inpFilePath) && !forTestOutput()) {
            const auto inputPathUtf8 = inpFilePath.u8string();
            throw std::runtime_error("Input path " + utils::utf8ToString(inputPathUtf8) + " does not exist or is not a file or directory.");
        }
//        if (!currentCommand->canProcess(inpFilePath)) {
//            logMessage(LogMsg() << "Invalid input format for command: " << inpFilePath.u8string(), LogSeverity::Error);
//            return;
//        }
        constexpr char kProcessingMessage[] = "Processing File: ";
        constexpr size_t kProcessingMessageSize = sizeof(kProcessingMessage) - 1; // account for null terminator.
        std::string delimiter(kProcessingMessageSize + inpFilePath.u32string().size(), '='); // use u32string().size to get actual number of characters displayed
        // log header for each file
        logMessage(LogMsg(), true);
        logMessage(LogMsg() << delimiter, true);
        logMessage(LogMsg() << kProcessingMessage << utils::asUtf8Bytes(inpFilePath), true);
        logMessage(LogMsg() << delimiter, true);
        this->inputFilePath = inpFilePath; // assign after logging the header

        const auto inputData = currentCommand->processInput(inputFilePath, *this);

        auto calcOutpuFilePath = [&](const std::filesystem::path& path, const std::string& format) -> std::filesystem::path {
            std::filesystem::path retval = path;
            if (retval.is_relative()) {
                retval = inputFilePath.parent_path() / retval;
            }
            if (retval.empty()) {
                retval = std::filesystem::current_path();
            }
            if (createDirectoryIfNeeded(retval)) {
                outputIsFilename = false;
                std::filesystem::path outputFileName = inputFilePath.filename();
                outputFileName.replace_extension(format);
                retval = retval / outputFileName;
            } else {
                outputIsFilename = true;
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
                currentCommand->processOutput(inputData, calcOutpuFilePath(outputFilePath, outputFormat), inputFilePath, *this);
                outputFormatSpecified = true;
            }
        }
        if (!outputFormatSpecified) {
            const auto& defaultFormat = currentCommand->defaultOutputFormat(inputFilePath);
            if (defaultFormat.has_value()) {
                currentCommand->processOutput(inputData, calcOutpuFilePath(inputFilePath.parent_path(), std::string(defaultFormat.value())), inputFilePath, *this);
            }
        }
    } catch (const musx::xml::load_error& ex) {
        logMessage(LogMsg() << "Load XML failed: " << ex.what(), true, LogSeverity::Error);
    } catch (const std::exception& e) {
        logMessage(LogMsg() << e.what(), true, LogSeverity::Error);
    }
}

} // namespace denigma
