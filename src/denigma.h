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
#pragma once

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <optional>
#include <fstream>
#include <functional>
#include <cassert>

#include "musx/musx.h"
#include "utils/stringutils.h"

constexpr char8_t MUSX_EXTENSION[]      = u8"musx";
constexpr char8_t ENIGMAXML_EXTENSION[] = u8"enigmaxml";
constexpr char8_t MNX_EXTENSION[]       = u8"mnx";
constexpr char8_t JSON_EXTENSION[]      = u8"json";
constexpr char8_t MSS_EXTENSION[]       = u8"mss";
constexpr char8_t SVG_EXTENSION[]       = u8"svg";
constexpr char8_t MXL_EXTENSION[]       = u8"mxl";
constexpr char8_t MUSICXML_EXTENSION[]  = u8"musicxml";

constexpr int JSON_INDENT_SPACES     = 4;

#ifdef _WIN32
#define _ARG(S) L##S
#define _ARG_CONV(S) (utils::wstringToString(std::wstring(S)))
#define _MAIN wmain
#else
#define _ARG(S) S
#define _ARG_CONV(S) S
#define _MAIN main
#endif

namespace denigma {

#ifdef _WIN32
using arg_view = std::wstring_view;
using arg_char = WCHAR;
struct arg_string : public std::wstring
{
    using std::wstring::wstring;
    arg_string(const std::wstring& wstr) : std::wstring(wstr) {}
    arg_string(const std::string& str) : std::wstring(utils::stringToWstring(str)) {}
    arg_string(const char* str) : std::wstring(utils::stringToWstring(str)) {}
    arg_string(std::string_view str) : std::wstring(utils::stringToWstring(std::string(str))) {}
    arg_string(const std::u8string& str) : std::wstring(utils::stringToWstring(utils::utf8ToString(str))) {}
    arg_string(std::u8string_view str) : std::wstring(utils::stringToWstring(utils::utf8ToString(str))) {}
    arg_string(const char8_t* str) : std::wstring(utils::stringToWstring(utils::utf8ToString(str ? std::u8string_view(str) : std::u8string_view{}))) {}

    operator std::string() const
    {
        return utils::wstringToString(*this);
    }

    operator std::u8string() const
    {
        return utils::stringToUtf8(static_cast<std::string>(*this));
    }
};

inline std::string operator+(const std::string& lhs, const arg_string& rhs)
{
    return lhs + static_cast<std::string>(rhs);
}
#else
using arg_view = std::string_view;
using arg_char = char;
struct arg_string : public std::string
{
    using std::string::string;
    arg_string(const std::string& str) : std::string(str) {}
    arg_string(std::string_view str) : std::string(str) {}
    arg_string(const std::u8string& str) : std::string(utils::utf8ToString(str)) {}
    arg_string(std::u8string_view str) : std::string(utils::utf8ToString(str)) {}
    arg_string(const char8_t* str) : std::string(utils::utf8ToString(str ? std::u8string_view(str) : std::u8string_view{})) {}

    operator std::u8string() const
    {
        return utils::stringToUtf8(*this);
    }
};
#endif

using MusxReader = ::musx::xml::pugi::Document;

using Buffer = std::vector<char>;
using LogMsg = std::stringstream;

struct CommandInputData
{
    struct EmbeddedGraphicFile
    {
        std::string filename;
        std::string blob;
    };

    Buffer primaryBuffer;
    std::optional<Buffer> notationMetadata;
    std::vector<EmbeddedGraphicFile> embeddedGraphics;
};

// Function to find the appropriate processor
template <typename Processors>
inline decltype(Processors::value_type::processor) findProcessor(const Processors& processors, std::u8string_view extension)
{
    std::u8string key(extension);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        return static_cast<char8_t>(std::tolower(c));
    });
    if (key.rfind(u8".", 0) == 0) {
        key = key.substr(1);
    }
    for (const auto& p : processors) {
        if (key == p.extension) {
            return p.processor;
        }
    }
    throw std::invalid_argument("Unsupported format: " + utils::utf8ToString(key));
}

/// @brief defines log message severity
enum class LogSeverity
{
    Info,       ///< No error. The message is for information.
    Warning,    ///< An event has occurred that may affect the result, but processing of output continues.
    Error,      ///< Processing of the current file has aborted. This level usually occurs in catch blocks.
    Verbose     ///< Only emit if --verbose option specified. The message is for information.
};

enum class MusicProgramPreset
{
    Unspecified,
    MuseScore,
    Dorico,
    LilyPond
};

inline MusicProgramPreset toMusicProgramPreset(const std::string& inp)
{
    const std::string lc = utils::toLowerCase(inp);
    if (lc == "musescore") return MusicProgramPreset::MuseScore;
    if (lc == "dorico") return MusicProgramPreset::Dorico;
    if (lc == "lilypond") return MusicProgramPreset::LilyPond;
    return MusicProgramPreset::Unspecified;
}

class ICommand;
struct DenigmaContext
{
public:
    DenigmaContext(const arg_string& progName)
        : programName(std::string(progName))
    {
    }

    mutable bool errorOccurred{};
    bool outputIsFilename;

    std::string programName;
    bool showVersion{};
    bool showHelp{};
    bool showAbout{};
    bool overwriteExisting{};
    bool allPartsAndScore{};
    bool recursiveSearch{};
    bool noLog{};
    bool verbose{};
    bool quiet{};
    bool noValidate{};
    std::optional<std::filesystem::path> excludeFolder;
    std::optional<std::string> partName;
    std::optional<std::filesystem::path> logFilePath;
    std::shared_ptr<std::ofstream> logFile;
    std::filesystem::path inputFilePath;

    // Specific options for `massage` command
    bool refloatRests{ true };
    bool extendOttavasLeft{ true };
    bool extendOttavasRight{ true };
    bool fermataWholeRests{ true };
    std::optional<std::filesystem::path> finaleFilePath;

    // Specic options for `export --mnx` command
    std::optional<int> indentSpaces{ JSON_INDENT_SPACES };
    std::optional<std::filesystem::path> mnxSchemaPath;
    std::optional<std::string> mnxSchema;
    bool includeTempoTool{};

    // Specific options for `export --svg` command
    std::vector<musx::dom::Cmper> svgShapeDefs;
    musx::util::SvgConvert::SvgUnit svgUnit{ musx::util::SvgConvert::SvgUnit::Points };
    bool svgUsePageScale{ false };
    double svgScale{ 1.0 };

#ifdef DENIGMA_TEST
    bool testOutput{}; // this may be defined on the command line by the test program
#endif

    void setMassageTarget(const std::string& opt)
    {
        auto preset = toMusicProgramPreset(opt);
        if (preset == MusicProgramPreset::Unspecified) return;
        refloatRests = extendOttavasLeft = fermataWholeRests = true;
        extendOttavasRight = (preset != MusicProgramPreset::LilyPond);
    }

    // Parse general options and return remaining options
    std::vector<const arg_char*> parseOptions(int argc, arg_char* argv[]);

    // validate paths
    bool validatePathsAndOptions(const std::filesystem::path& outputFilePath) const;

    void processFile(const std::shared_ptr<ICommand>& currentCommand, const std::filesystem::path inpFilePath, const std::vector<const arg_char*>& args);

    // Logging methods
    void startLogging(const std::filesystem::path& defaultLogPath, int argc, arg_char* argv[]); ///< Starts logging if logging was requested

    /**
     * @brief logs a message using the denigmaContext or outputs to std::cerr
     * @param msg a utf-8 encoded message.
     * @param severity the message severity
    */
    void logMessage(LogMsg&& msg, LogSeverity severity = LogSeverity::Info) const
    {
        logMessage(std::move(msg), false, severity);
    }

    void endLogging(); ///< Ends logging if logging was requested

    bool forTestOutput() const
    {
#ifdef DENIGMA_TEST
        return testOutput;
#else
        return false;
#endif
    }

private:
    void logMessage(LogMsg&& msg, bool alwaysShow, LogSeverity severity = LogSeverity::Info) const;
};

class ICommand
{
public:
    ICommand() = default;
    virtual ~ICommand() = default;

    virtual int showHelpPage(const std::string_view& programName, const std::string& indentSpaces = {}) const = 0;

    virtual bool canProcess(const std::filesystem::path& inputPath) const = 0;
    virtual CommandInputData processInput(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const = 0;
    virtual void processOutput(const CommandInputData& inputData, const std::filesystem::path& outputPath, const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const = 0;
    virtual std::optional<std::u8string_view> defaultInputFormat() const { return std::nullopt; }
    virtual std::optional<std::u8string> defaultOutputFormat(const std::filesystem::path&) const { return std::nullopt; }

    virtual const std::string_view commandName() const = 0;
};

std::string getTimeStamp(const std::string& fmt);

bool createDirectoryIfNeeded(const std::filesystem::path& path);
void showAboutPage();
bool isFontSMuFL(const std::shared_ptr<musx::dom::FontInfo>& font);

} // namespace denigma

#define ASSERT_IF(TEST) \
assert(!(TEST)); \
if (TEST)

#ifdef DENIGMA_TEST // this is defined on the command line by the test program
#undef _MAIN
#define _MAIN denigmaTestMain
int denigmaTestMain(int argc, denigma::arg_char* argv[]);
#ifdef DENIGMA_VERSION
#undef DENIGMA_VERSION
#define DENIGMA_VERSION "TEST"
#endif
#define DENIGMA_TEST_CODE(C) C
#else
#define DENIGMA_TEST_CODE(C)
#endif
