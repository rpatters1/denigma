// Copyright 2026 Robert G. Patterson.
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <ostream>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "denigma/io/random_access_reader.h"
#include "denigma/gap_report.h"
#include "core/denigma.h"
#include "core/musx_reader.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "utils/ziputils.h"

// The public converter adapters extract the MUSX archive on every call. These are
// the entry points behind those adapters, so a single extraction can serve an
// inspection and every conversion that follows. They are declared in
// formats/musicxml/musicxml.h and formats/mnx/mnx.h, but those headers pull in mx
// and mnxdom, which are private dependencies of the libraries this target links.
// Declaring them here keeps the wrapper's build off that graph; a signature change
// in either exporter surfaces as a link error.
namespace denigma::formats::musicxml::detail {
void convert(const CommandInputData& inputData,
             const DenigmaContext& denigmaContext,
             const MultiOutputCallback& outputCallback);
} // namespace denigma::formats::musicxml::detail

namespace denigma::formats::mnx::detail {
void exportJson(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext);
} // namespace denigma::formats::mnx::detail

namespace {

struct OutputFile
{
    std::string name;
    std::vector<std::uint8_t> data;
    int sourceIndex{ -1 };
};

struct PageSize
{
    double widthMm{};
    double heightMm{};
    double spatiumMm{};
    bool hasMargins{};
    double marginTopSp{};
    double marginBottomSp{};
    double marginLeftSp{};
    double marginRightSp{};
};

struct PartInfo
{
    int id{};
    std::string name;
    int outputIndex{};
    int partOrder{};
    PageSize pageSize;
};

struct OnlineResult
{
    bool success{ true };
    std::string scoreName{ "Score" };
    PageSize scorePageSize;
    std::vector<denigma::Diagnostic> diagnostics;
    std::vector<PartInfo> parts;
    std::vector<OutputFile> outputs;
    std::string gapReport;
};

enum class InputFormat
{
    Musx,
    EnigmaXml,
    ZippedEnigmaXml
};

bool endsWithCaseInsensitive(std::string_view value, std::string_view suffix)
{
    if (value.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin(), [](char lhs, char rhs) {
        return std::tolower(static_cast<unsigned char>(lhs)) == std::tolower(static_cast<unsigned char>(rhs));
    });
}

InputFormat inputFormat(const char* sourceName)
{
    const std::string_view name = sourceName ? sourceName : "browser.musx";
    if (endsWithCaseInsensitive(name, ".enigmaxml.zip")) return InputFormat::ZippedEnigmaXml;
    if (endsWithCaseInsensitive(name, ".enigmaxml")) return InputFormat::EnigmaXml;
    if (endsWithCaseInsensitive(name, ".musx")) return InputFormat::Musx;
    throw std::invalid_argument("Unsupported input filename.");
}

void addDiagnostic(OnlineResult& result, denigma::MessageSeverity severity, std::string message)
{
    result.success = result.success && severity != denigma::MessageSeverity::Error;
    result.diagnostics.push_back({ severity, std::move(message) });
}

const char* fallbackSourceName(InputFormat format)
{
    return format == InputFormat::Musx ? "browser.musx" : "browser.enigmaxml";
}

denigma::DenigmaContext makeInputContext(OnlineResult& result, const char* sourceName, const char* fallbackName)
{
    denigma::DenigmaContext context(DENIGMA_NAME);
    context.inputFilePath = sourceName ? sourceName : fallbackName;
    context.verbose = true;
    context.logCallback = [&result](denigma::MessageSeverity severity, std::string_view message) {
        addDiagnostic(result, severity, std::string(message));
    };
    return context;
}

// Mirrors makeMusicXmlContext()/makeMnxContext() in Denigma's converter adapters
// (src/formats/*/*_converter.cpp). Those adapters re-extract the archive on every
// call, so this wrapper drives the detail entry points with cached input data and
// assembles the equivalent context here. Validation stays on and quiet stays off,
// matching the CommonOptions defaults the adapters map from.
denigma::DenigmaContext makeConversionContext(OnlineResult& result,
                                              const char* sourceName,
                                              InputFormat format,
                                              denigma::ConversionResult& conversionResult)
{
    auto context = makeInputContext(result, sourceName, fallbackSourceName(format));
    context.conversionResult = &conversionResult;
    return context;
}

std::span<const std::byte> inputBytes(const std::uint8_t* data, std::size_t size)
{
    if (!data && size != 0) {
        throw std::invalid_argument("Input buffer is null.");
    }
    return { reinterpret_cast<const std::byte*>(data), size };
}

denigma::Buffer copyBytes(std::span<const std::byte> bytes)
{
    denigma::Buffer result;
    result.reserve(bytes.size());
    std::transform(bytes.begin(), bytes.end(), std::back_inserter(result), [](std::byte value) {
        return static_cast<char>(value);
    });
    return result;
}

denigma::CommandInputData readInputData(std::span<const std::byte> bytes,
                                        InputFormat format,
                                        const denigma::DenigmaContext& context)
{
    if (format == InputFormat::Musx) {
        denigma::BufferRandomAccessReader reader(bytes);
        return denigma::formats::enigmaxml::detail::extractMusxInputData(reader, context);
    }
    if (format == InputFormat::ZippedEnigmaXml) {
        denigma::BufferRandomAccessReader reader(bytes);
        const auto xml = utils::readSoleFileWithExtension(reader, ENIGMAXML_EXTENSION, context);
        return { denigma::Buffer(xml.begin(), xml.end()), std::nullopt, {} };
    }
    return { copyBytes(bytes), std::nullopt, {} };
}

// Extracting a MUSX archive inflates the zip and deobfuscates the EnigmaXML inside
// it. The browser works the same file over and over -- one inspection, then a
// conversion for every format change, part selection, and retry -- so hold the
// extracted data and reuse it until a different file arrives.
struct InputCache
{
    std::string sourceName;
    std::size_t size{};
    std::uint64_t hash{};
    denigma::CommandInputData data;
    bool valid{};
};

InputCache inputCache;

std::uint64_t contentHash(std::span<const std::byte> bytes)
{
    std::uint64_t hash = 14695981039346656037ull; // FNV-1a
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint64_t>(value);
        hash *= 1099511628211ull;
    }
    return hash;
}

const denigma::CommandInputData& cachedInputData(std::span<const std::byte> bytes,
                                                 InputFormat format,
                                                 const denigma::DenigmaContext& context,
                                                 const char* sourceName)
{
    const std::string name = sourceName ? sourceName : "";
    const auto hash = contentHash(bytes);
    if (inputCache.valid && inputCache.size == bytes.size() && inputCache.hash == hash
        && inputCache.sourceName == name) {
        return inputCache.data;
    }
    inputCache = {}; // release the previous extraction before allocating the next one
    inputCache.data = readInputData(bytes, format, context);
    inputCache.sourceName = name;
    inputCache.size = bytes.size();
    inputCache.hash = hash;
    inputCache.valid = true;
    return inputCache.data;
}

std::span<const std::byte> primaryBytes(const denigma::CommandInputData& input)
{
    return { reinterpret_cast<const std::byte*>(input.primaryBuffer.data()), input.primaryBuffer.size() };
}

void appendOutput(OnlineResult& result, std::string_view name, std::span<const std::byte> data, int sourceIndex = -1)
{
    OutputFile output;
    output.name = name;
    output.sourceIndex = sourceIndex;
    output.data.resize(data.size());
    std::transform(data.begin(), data.end(), output.data.begin(), [](std::byte value) {
        return static_cast<std::uint8_t>(value);
    });
    result.outputs.push_back(std::move(output));
}

void appendOutput(OnlineResult& result, std::string name, const std::string& data)
{
    OutputFile output;
    output.name = std::move(name);
    output.data.assign(data.begin(), data.end());
    result.outputs.push_back(std::move(output));
}

PageSize pageSizeForPart(const musx::dom::DocumentPtr& document, musx::dom::Cmper partId)
{
    try {
        const auto options = document->getOptions()->get<musx::dom::options::PageFormatOptions>();
        const auto format = options ? options->calcPageFormatForPart(partId) : nullptr;
        if (!format || format->pageWidth <= 0 || format->pageHeight <= 0) return {};
        PageSize result{
            static_cast<double>(format->pageWidth) / musx::dom::EVPU_PER_MM,
            static_cast<double>(format->pageHeight) / musx::dom::EVPU_PER_MM,
            format->calcCombinedSystemScaling().toDouble() * musx::dom::EVPU_PER_SPACE
                / musx::dom::EVPU_PER_MM
        };
        const auto firstPage = document->getOthers()->get<musx::dom::others::Page>(partId, 1);
        const auto marginScaling = firstPage && !firstPage->holdMargins
            ? format->calcSystemScaling().toDouble()
            : format->calcCombinedSystemScaling().toDouble();
        if (marginScaling <= 0) return result;
        const auto marginSp = [marginScaling](musx::dom::Evpu value) {
            return static_cast<double>(value) / (musx::dom::EVPU_PER_SPACE * marginScaling);
        };
        result.hasMargins = true;
        result.marginTopSp = marginSp(-(firstPage ? firstPage->margTop : format->leftPageMarginTop));
        result.marginBottomSp = marginSp(firstPage ? firstPage->margBottom : format->leftPageMarginBottom);
        result.marginLeftSp = marginSp(firstPage ? firstPage->margLeft : format->leftPageMarginLeft);
        result.marginRightSp = marginSp(-(firstPage ? firstPage->margRight : format->leftPageMarginRight));
        return result;
    } catch (...) {
        return {};
    }
}

void finishConversion(OnlineResult& result, const denigma::ConversionResult& conversionResult)
{
    if (conversionResult.hasError()) {
        result.success = false;
    }
    if (result.outputs.empty() && result.success) {
        addDiagnostic(result, denigma::MessageSeverity::Error, "Conversion produced no output.");
    } else if (result.success && std::any_of(result.outputs.begin(), result.outputs.end(), [](const OutputFile& output) {
        return output.data.empty();
    })) {
        addDiagnostic(result, denigma::MessageSeverity::Error, "Conversion produced an empty output file.");
    }
}

void inspectInput(OnlineResult& result, std::span<const std::byte> bytes, const char* sourceName, InputFormat format)
{
    auto context = makeInputContext(result, sourceName, fallbackSourceName(format));

    const auto& input = cachedInputData(bytes, format, context, sourceName);
    auto document = denigma::createMusxDocument<denigma::MusxReader>(input, context);
    auto parts = document->getOthers()->getArray<musx::dom::others::PartDefinition>(musx::dom::SCORE_PARTID);
    result.scorePageSize = pageSizeForPart(document, musx::dom::SCORE_PARTID);

    int outputIndex = 1; // MusicXML callback zero is the score.
    for (const auto& part : parts) {
        if (part->isScore()) {
            auto name = part->getName(musx::util::EnigmaString::AccidentalStyle::Unicode);
            if (!name.empty()) {
                result.scoreName = std::move(name);
            }
            continue;
        }
        auto name = part->getName(musx::util::EnigmaString::AccidentalStyle::Unicode);
        if (name.empty()) {
            name = "Part " + std::to_string(part->getCmper());
        }
        result.parts.push_back({
            part->getCmper(),
            std::move(name),
            outputIndex++,
            part->partOrder,
            pageSizeForPart(document, part->getCmper())
        });
    }
    std::stable_sort(result.parts.begin(), result.parts.end(), [](const PartInfo& lhs, const PartInfo& rhs) {
        return lhs.partOrder < rhs.partOrder;
    });
}

void convertMusicXml(OnlineResult& result,
                     std::span<const std::byte> bytes,
                     const char* sourceName,
                     InputFormat inputFormat,
                     bool includeTempo,
                     bool allFontsAvailable,
                     bool useFinaleRestPosition,
                     int cueLayer,
                     const int* selectedOutputs,
                     std::size_t selectedCount)
{
    denigma::ConversionResult conversionResult;
    auto context = makeConversionContext(result, sourceName, inputFormat, conversionResult);
    context.includeTempoTool = includeTempo;
    context.allFontsAvailable = allFontsAvailable;
    context.useFinaleRestPosition = useFinaleRestPosition;
    context.allPartsAndScore = true;
    if (cueLayer > 0) {
        context.cueLayer = cueLayer;
    }

    const std::set<int> selected(selectedOutputs, selectedOutputs + selectedCount);
    int outputIndex = 0;
    const auto outputCallback = [&](std::string_view suggestedName, std::span<const std::byte> data) {
        if (selected.contains(outputIndex)) {
            appendOutput(result, suggestedName, data, outputIndex);
        }
        ++outputIndex;
    };
    const auto& input = cachedInputData(bytes, inputFormat, context, sourceName);
    denigma::formats::musicxml::detail::convert(input, context, outputCallback);
    finishConversion(result, conversionResult);
}

void convertMnx(OnlineResult& result,
                std::span<const std::byte> bytes,
                const char* sourceName,
                InputFormat inputFormat,
                bool includeTempo,
                bool splitInstruments,
                int indentSpaces,
                int cueLayer)
{
    denigma::ConversionResult conversionResult;
    auto context = makeConversionContext(result, sourceName, inputFormat, conversionResult);
    context.includeTempoTool = includeTempo;
    context.mnxSplitInstruments = splitInstruments;
    context.indentSpaces = indentSpaces < 0 ? std::nullopt : std::optional<int>(indentSpaces);
    if (cueLayer > 0) {
        context.cueLayer = cueLayer;
    }

    std::ostringstream output;
    const denigma::MusxLoggerScope musxLogger(denigma::makeMusxLogCallback(context));
    const auto& input = cachedInputData(bytes, inputFormat, context, sourceName);
    denigma::formats::mnx::detail::exportJson(output, input, context);
    if (!conversionResult.gaps().empty()) {
        conversionResult.setSourceEvidence({ denigma::FormatId::EnigmaXml,
            std::string(input.primaryBuffer.begin(), input.primaryBuffer.end()) });
    }
    const auto sourceFormat = inputFormat == InputFormat::Musx
        ? denigma::FormatId::Musx
        : denigma::FormatId::EnigmaXml;
    result.gapReport = denigma::serializeGapReport(conversionResult, sourceFormat, denigma::FormatId::MnxJson,
        { DENIGMA_NAME, DENIGMA_VERSION, denigma::gitCommit() });
    if (!conversionResult.hasError()) {
        appendOutput(result, {}, output.str());
    }
    finishConversion(result, conversionResult);
}

void convertEnigmaXml(OnlineResult& result,
                      std::span<const std::byte> bytes,
                      const char* sourceName,
                      InputFormat inputFormat)
{
    // Denigma's MUSX-to-EnigmaXML converter writes exactly the primary buffer that
    // extraction already produced, so both input formats reduce to the cached data.
    auto context = makeInputContext(result, sourceName, fallbackSourceName(inputFormat));
    const auto& input = cachedInputData(bytes, inputFormat, context, sourceName);
    appendOutput(result, {}, primaryBytes(input));
    if (input.primaryBuffer.empty()) {
        addDiagnostic(result, denigma::MessageSeverity::Error, "The EnigmaXML input is empty.");
    }
}

template <typename Callback>
OnlineResult* makeResult(Callback&& callback)
{
    auto result = std::make_unique<OnlineResult>();
    try {
        callback(*result);
    } catch (const std::exception& ex) {
        addDiagnostic(*result, denigma::MessageSeverity::Error, ex.what());
    } catch (...) {
        addDiagnostic(*result, denigma::MessageSeverity::Error, "Unknown Denigma error.");
    }
    return result.release();
}

template <typename Collection>
const typename Collection::value_type* itemAt(const Collection& collection, std::size_t index)
{
    return index < collection.size() ? &collection[index] : nullptr;
}

} // namespace

extern "C" {

void* denigma_malloc(std::size_t size) { return ::operator new(size, std::nothrow); }
void denigma_free(void* pointer) { ::operator delete(pointer); }

OnlineResult* denigma_inspect(const std::uint8_t* data, std::size_t size, const char* sourceName)
{
    return makeResult([&](OnlineResult& result) {
        inspectInput(result, inputBytes(data, size), sourceName, inputFormat(sourceName));
    });
}

OnlineResult* denigma_convert(const std::uint8_t* data,
                           std::size_t size,
                           const char* sourceName,
                           int format,
                           int includeTempo,
                           int allFontsAvailable,
                           int useFinaleRestPosition,
                           int splitInstruments,
                           int indentSpaces,
                           int cueLayer,
                           const int* selectedOutputs,
                           std::size_t selectedCount)
{
    return makeResult([&](OnlineResult& result) {
        const auto bytes = inputBytes(data, size);
        const auto sourceFormat = inputFormat(sourceName);
        switch (format) {
        case 0:
            if (!selectedOutputs || selectedCount == 0) {
                throw std::invalid_argument("Select the score or at least one linked part.");
            }
            convertMusicXml(result, bytes, sourceName, sourceFormat, includeTempo != 0,
                            allFontsAvailable != 0, useFinaleRestPosition != 0, cueLayer, selectedOutputs, selectedCount);
            break;
        case 1:
            convertMnx(result, bytes, sourceName, sourceFormat, includeTempo != 0, splitInstruments != 0, indentSpaces, cueLayer);
            break;
        case 2:
            convertEnigmaXml(result, bytes, sourceName, sourceFormat);
            break;
        default:
            throw std::invalid_argument("Unknown output format.");
        }
    });
}

void denigma_result_destroy(OnlineResult* result) { delete result; }
int denigma_result_success(const OnlineResult* result) { return result && result->success ? 1 : 0; }

std::size_t denigma_result_diagnostic_count(const OnlineResult* result)
{
    return result ? result->diagnostics.size() : 0;
}

int denigma_result_diagnostic_severity(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->diagnostics, index) : nullptr;
    return item ? static_cast<int>(item->severity) : static_cast<int>(denigma::MessageSeverity::Error);
}

const char* denigma_result_diagnostic_message(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->diagnostics, index) : nullptr;
    return item ? item->message.c_str() : "";
}

const char* denigma_result_score_name(const OnlineResult* result)
{
    return result ? result->scoreName.c_str() : "Score";
}

double denigma_result_score_page_width_mm(const OnlineResult* result)
{
    return result ? result->scorePageSize.widthMm : 0.0;
}

double denigma_result_score_page_height_mm(const OnlineResult* result)
{
    return result ? result->scorePageSize.heightMm : 0.0;
}

double denigma_result_score_spatium_mm(const OnlineResult* result)
{
    return result ? result->scorePageSize.spatiumMm : 0.0;
}

int denigma_result_score_has_page_margins(const OnlineResult* result)
{
    return result && result->scorePageSize.hasMargins ? 1 : 0;
}

double denigma_result_score_page_margin_top_sp(const OnlineResult* result)
{
    return result ? result->scorePageSize.marginTopSp : 0.0;
}

double denigma_result_score_page_margin_bottom_sp(const OnlineResult* result)
{
    return result ? result->scorePageSize.marginBottomSp : 0.0;
}

double denigma_result_score_page_margin_left_sp(const OnlineResult* result)
{
    return result ? result->scorePageSize.marginLeftSp : 0.0;
}

double denigma_result_score_page_margin_right_sp(const OnlineResult* result)
{
    return result ? result->scorePageSize.marginRightSp : 0.0;
}

std::size_t denigma_result_part_count(const OnlineResult* result) { return result ? result->parts.size() : 0; }

int denigma_result_part_id(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->id : 0;
}

const char* denigma_result_part_name(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->name.c_str() : "";
}

int denigma_result_part_output_index(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->outputIndex : -1;
}

double denigma_result_part_page_width_mm(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.widthMm : 0.0;
}

double denigma_result_part_page_height_mm(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.heightMm : 0.0;
}

double denigma_result_part_spatium_mm(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.spatiumMm : 0.0;
}

int denigma_result_part_has_page_margins(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item && item->pageSize.hasMargins ? 1 : 0;
}

double denigma_result_part_page_margin_top_sp(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.marginTopSp : 0.0;
}

double denigma_result_part_page_margin_bottom_sp(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.marginBottomSp : 0.0;
}

double denigma_result_part_page_margin_left_sp(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.marginLeftSp : 0.0;
}

double denigma_result_part_page_margin_right_sp(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->parts, index) : nullptr;
    return item ? item->pageSize.marginRightSp : 0.0;
}

std::size_t denigma_result_output_count(const OnlineResult* result) { return result ? result->outputs.size() : 0; }

const char* denigma_result_output_name(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->outputs, index) : nullptr;
    return item ? item->name.c_str() : "";
}

const std::uint8_t* denigma_result_output_data(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->outputs, index) : nullptr;
    return item && !item->data.empty() ? item->data.data() : nullptr;
}

std::size_t denigma_result_output_size(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->outputs, index) : nullptr;
    return item ? item->data.size() : 0;
}

int denigma_result_output_index(const OnlineResult* result, std::size_t index)
{
    const auto* item = result ? itemAt(result->outputs, index) : nullptr;
    return item ? item->sourceIndex : -1;
}

const std::uint8_t* denigma_result_gap_report_data(const OnlineResult* result)
{
    return result && !result->gapReport.empty()
        ? reinterpret_cast<const std::uint8_t*>(result->gapReport.data())
        : nullptr;
}

std::size_t denigma_result_gap_report_size(const OnlineResult* result)
{
    return result ? result->gapReport.size() : 0;
}

const char* denigma_version() { return DENIGMA_VERSION; }
const char* denigma_commit() { return denigma::gitCommit(); }

} // extern "C"
