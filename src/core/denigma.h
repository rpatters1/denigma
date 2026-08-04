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
#include <span>
#include <unordered_set>
#include <utility>

#include "denigma/conversion.h"
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
constexpr char8_t ZIP_EXTENSION[]       = u8"zip";

/// @brief Compound extension for an EnigmaXML file stored as the sole entry of a zip archive.
constexpr char8_t ENIGMAXML_ZIP_EXTENSION[] = u8"enigmaxml.zip";

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

const char* gitCommit();

/// @brief Returns the additive offset that converts a Finale rest position to the SMuFL glyph-origin convention.
/// @return The signed offset in Finale half-space staff-position units, where negative moves downward.
constexpr int calcFinaleToSmuflRestPositionOffset(musx::dom::NoteType restType)
{
    constexpr int staffPositionsPerSpace = 2;
    return restType == musx::dom::NoteType::Whole ? staffPositionsPerSpace : 0;
}

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

// stupid omission from C++17 standard
// see https://stackoverflow.com/questions/73555606/stdunordered-setstdfilesystempath-compile-error-on-clang-and-g-below
struct PathHash
{
    auto operator()(const std::filesystem::path& p) const noexcept {
        return std::filesystem::hash_value(p);
    }
};
using PathSet = std::unordered_set<std::filesystem::path, PathHash>;

/// @brief Returns a normalized form of @p path for comparing paths that may be spelled differently.
/// Returns @p path unchanged when the filesystem cannot resolve it.
inline std::filesystem::path comparablePath(const std::filesystem::path& path)
{
    try {
        return std::filesystem::weakly_canonical(path);
    } catch (...) {}
    return path;
}

/// @brief Returns the format key used to look up an input processor for @p path.
/// A zip wrapping another extension yields the compound key ("foo.enigmaxml.zip" produces "enigmaxml.zip").
/// Every other path yields its normalized final extension.
inline std::u8string inputFormatKey(const std::filesystem::path& path)
{
    std::u8string extension = utils::normalizedPathExtension(path);
    if (extension == ZIP_EXTENSION) {
        const std::u8string wrapped = utils::normalizedExtension(path.stem().extension().u8string());
        if (!wrapped.empty()) {
            return wrapped + u8'.' + extension;
        }
    }
    return extension;
}

/// @brief Returns @p path with an outer zip wrapper removed ("foo.enigmaxml.zip" produces "foo.enigmaxml").
/// Paths without a compound archive extension are returned unchanged.
inline std::filesystem::path unwrappedInputPath(const std::filesystem::path& path)
{
    if (utils::normalizedPathExtension(path) != ZIP_EXTENSION || !path.stem().has_extension()) {
        return path;
    }
    std::filesystem::path retval = path;
    retval.replace_filename(path.stem());
    return retval;
}

/// @brief Returns the help-page annotation for @p extension given a command's default input formats.
/// The first default format is the command's default; any others are additionally scanned in directory searches.
inline std::string describeDefaultInputFormat(std::span<const std::u8string_view> defaultFormats, std::u8string_view extension)
{
    for (size_t x = 0; x < defaultFormats.size(); x++) {
        if (defaultFormats[x] == extension) {
            return x == 0 ? " (default input format)" : " (also scanned by default)";
        }
    }
    return {};
}

// Function to find the appropriate processor
template <typename Processors>
inline decltype(Processors::value_type::processor) findProcessor(const Processors& processors, std::u8string_view extension)
{
    std::u8string key = utils::normalizedExtension(std::u8string(extension));
    for (const auto& p : processors) {
        if (key == p.extension) {
            return p.processor;
        }
    }
    throw std::invalid_argument("Unsupported format: " + utils::utf8ToString(key));
}

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
    std::optional<int> cueLayer;
    std::optional<std::filesystem::path> excludeFolder;
    std::optional<std::string> partName;
    std::optional<std::filesystem::path> logFilePath;
    std::shared_ptr<std::ofstream> logFile;
    std::filesystem::path inputFilePath;
    /// @brief Every input queued for this run, in comparable form. An output that would
    /// land on one of these is skipped, because that file may not have been read yet.
    PathSet scheduledInputPaths;
    std::function<void(MessageSeverity severity, std::string_view message)> logCallback;
    ConversionResult* conversionResult{};

    // Specific options for `massage` command
    bool refloatRests{ true };
    bool extendOttavasLeft{ true };
    bool extendOttavasRight{ true };
    bool fermataWholeRests{ true };
    std::optional<std::filesystem::path> finaleFilePath;

    // Specific options for `export --mnx` command
    std::optional<int> indentSpaces{ JSON_INDENT_SPACES };
    std::optional<std::filesystem::path> mnxSchemaPath;
    std::optional<std::string> mnxSchema;
    bool includeTempoTool{};
    bool mnxSplitInstruments{};

    // Specific options for `export --svg` command
    std::vector<musx::dom::Cmper> svgShapeDefs;
    musx::util::SvgConvert::SvgUnit svgUnit{ musx::util::SvgConvert::SvgUnit::Points };
    bool svgUsePageScale{ false };
    double svgScale{ 1.0 };

    bool testOutput{}; // this may be defined on the command line by the test program

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
    void logMessage(LogMsg&& msg, MessageSeverity severity = MessageSeverity::Info) const
    {
        logMessage(std::move(msg), false, severity);
    }

    void endLogging(); ///< Ends logging if logging was requested

    bool forTestOutput() const
    {
        return testOutput;
    }

private:
    void logMessage(LogMsg&& msg, bool alwaysShow, MessageSeverity severity = MessageSeverity::Info) const;
};

/// @brief Returns a musx logging callback that forwards messages into the supplied denigma context.
musx::util::Logger::LogCallback makeMusxLogCallback(const DenigmaContext& denigmaContext);

/// @class MusxLoggerScope
/// @brief Installs a musx logging callback for the lifetime of the scope and restores the previous one on exit.
class MusxLoggerScope
{
public:
    explicit MusxLoggerScope(musx::util::Logger::LogCallback callback);
    ~MusxLoggerScope();

    MusxLoggerScope(const MusxLoggerScope&) = delete;
    MusxLoggerScope& operator=(const MusxLoggerScope&) = delete;
    MusxLoggerScope(MusxLoggerScope&&) = delete;
    MusxLoggerScope& operator=(MusxLoggerScope&&) = delete;
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
    /// @brief The input formats searched when the input pattern names a bare directory.
    /// The first entry is reported as the command's default input format. An empty span
    /// means every file in the directory is considered.
    virtual std::span<const std::u8string_view> defaultInputFormats() const { return {}; }
    virtual std::optional<std::u8string> defaultOutputFormat(const std::filesystem::path&) const { return std::nullopt; }

    virtual const std::string_view commandName() const = 0;
};

std::string getTimeStamp(const std::string& fmt);

bool createDirectoryIfNeeded(const std::filesystem::path& path);
void showAboutPage();
bool isFontSMuFL(const std::shared_ptr<musx::dom::FontInfo>& font);

// createMusxDocument is implemented as a template to avoid promoting pugixml to being a core dependency
template <typename Reader>
musx::dom::DocumentPtr createMusxDocument(
    const CommandInputData& inputData,
    const DenigmaContext& denigmaContext,
    musx::dom::PartVoicingPolicy partVoicingPolicy = musx::dom::PartVoicingPolicy::Ignore)
{
    musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFiles embeddedGraphicFiles;
    embeddedGraphicFiles.reserve(inputData.embeddedGraphics.size());
    for (const auto& graphic : inputData.embeddedGraphics) {
        musx::factory::DocumentFactory::CreateOptions::EmbeddedGraphicFile file;
        file.filename = graphic.filename;
        file.bytes.assign(graphic.blob.begin(), graphic.blob.end());
        embeddedGraphicFiles.emplace_back(std::move(file));
    }

    musx::factory::DocumentFactory::CreateOptions createOptions(
        denigmaContext.inputFilePath,
        inputData.notationMetadata.value_or(Buffer{}),
        std::move(embeddedGraphicFiles),
        partVoicingPolicy);

    return musx::factory::DocumentFactory::create<Reader>(inputData.primaryBuffer, std::move(createOptions));
}

template <typename T>
musx::dom::MusxInstance<T> getDocOptions(const musx::dom::DocumentPtr& document, const std::string& prefsName)
{
    auto retval = document->getOptions()->get<T>();
    if (!retval) {
        throw std::invalid_argument("document contains no default " + prefsName + " denigmaContext");
    }
    return retval;
}

/// @brief Returns a human-readable name for a linked part.
/// @details The single source of naming for linked parts: MNX and MusicXML score names, the part
/// suffix on exported filenames, and diagnostics all resolve through here. Finale allows a part to
/// have no name at all, so fall back to identifying it by cmper.
/// @note A null instance yields an empty string rather than "Score", so callers exporting the score
/// itself pass null and get an unsuffixed filename.
/// @note Accidentals are rendered ASCII, as everywhere else in Denigma's output. There is
/// deliberately no way to ask for Unicode accidentals here.
/// @note Defined in denigma.cpp rather than inline, because this header is included nearly everywhere.
std::string calcLinkedPartDisplayName(const musx::dom::MusxInstance<musx::dom::others::PartDefinition>& linkedPart);

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
