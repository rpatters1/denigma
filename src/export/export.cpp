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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <array>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <optional>
#include <vector>

#include "denigma/formats/mss.h"
#include "denigma/formats/mnx.h"
#include "denigma/formats/svg.h"
#include "export/export.h"
#include "export/enigmaxml.h"

namespace denigma {

namespace {

ConversionOptions makeConversionOptions(const DenigmaContext& denigmaContext)
{
    ConversionOptions options;
    options.sourceName = denigmaContext.inputFilePath.string();
    options.validate = !denigmaContext.noValidate;
    options.indentSpaces = denigmaContext.indentSpaces;
    options.cueLayer = denigmaContext.cueLayer;
    options.mnxSchema = denigmaContext.mnxSchema;
    options.mnxIncludeTempoTool = denigmaContext.includeTempoTool;
    options.mnxSplitInstruments = denigmaContext.mnxSplitInstruments;
    options.mssAllPartsAndScore = denigmaContext.allPartsAndScore;
    options.mssPartName = denigmaContext.partName;
    switch (denigmaContext.svgUnit) {
    case musx::util::SvgConvert::SvgUnit::None:
        options.svgUnit = SvgUnit::None;
        break;
    case musx::util::SvgConvert::SvgUnit::Pixels:
        options.svgUnit = SvgUnit::Pixels;
        break;
    case musx::util::SvgConvert::SvgUnit::Points:
        options.svgUnit = SvgUnit::Points;
        break;
    case musx::util::SvgConvert::SvgUnit::Picas:
        options.svgUnit = SvgUnit::Picas;
        break;
    case musx::util::SvgConvert::SvgUnit::Centimeters:
        options.svgUnit = SvgUnit::Centimeters;
        break;
    case musx::util::SvgConvert::SvgUnit::Millimeters:
        options.svgUnit = SvgUnit::Millimeters;
        break;
    case musx::util::SvgConvert::SvgUnit::Inches:
        options.svgUnit = SvgUnit::Inches;
        break;
    }
    options.svgScale = denigmaContext.svgScale;
    options.svgUsePageScale = denigmaContext.svgUsePageScale;
    options.svgShapeDefs.reserve(denigmaContext.svgShapeDefs.size());
    for (const auto shapeDef : denigmaContext.svgShapeDefs) {
        options.svgShapeDefs.push_back(static_cast<int>(shapeDef));
    }
    return options;
}

std::span<const std::byte> enigmaXmlBytes(const CommandInputData& inputData)
{
    return std::as_bytes(std::span(inputData.primaryBuffer.data(), inputData.primaryBuffer.size()));
}

void exportMnxJsonWithAdapter(const std::filesystem::path& outputPath,
                              const CommandInputData& inputData,
                              const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
#endif
    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    ConverterRegistry registry;
    formats::mnx::registerConverters(registry);
    const auto* converter = registry.find(FormatId::EnigmaXml, FormatId::MnxJson);
    if (!converter) {
        throw std::logic_error("MNX JSON converter is not registered.");
    }

    std::ofstream output;
    output.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    output.open(outputPath, std::ios::out | std::ios::binary);

    converter->convert(enigmaXmlBytes(inputData), output, makeConversionOptions(denigmaContext));
    output.close();
}

void exportMssWithAdapter(const std::filesystem::path& outputPath,
                          const CommandInputData& inputData,
                          const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
#endif

    ConverterRegistry registry;
    formats::mss::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(FormatId::EnigmaXml, FormatId::MssXml);
    if (!converter) {
        throw std::logic_error("MSS converter is not registered.");
    }

    size_t generatedCount = 0;
    converter->convert(enigmaXmlBytes(inputData), [&](std::string_view suggestedName, std::span<const std::byte> mssData) {
        std::filesystem::path qualifiedOutputPath = outputPath;
        if (!suggestedName.empty()) {
            auto currExtension = qualifiedOutputPath.extension();
            qualifiedOutputPath.replace_extension(utils::stringToUtf8(suggestedName) + currExtension.u8string());
        }
        if (!denigmaContext.validatePathsAndOptions(qualifiedOutputPath)) {
            return;
        }
        std::ofstream output;
        output.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        output.open(qualifiedOutputPath, std::ios::out | std::ios::binary);
        output.write(reinterpret_cast<const char*>(mssData.data()), static_cast<std::streamsize>(mssData.size()));
        output.close();
        ++generatedCount;
    }, makeConversionOptions(denigmaContext));

    if (generatedCount == 0) {
        denigmaContext.logMessage(LogMsg() << "No MSS files were written.", LogSeverity::Warning);
    }
}

std::filesystem::path appendSvgShapeSuffix(const std::filesystem::path& outputPath, int shapeCmper)
{
    std::filesystem::path result = outputPath;
    auto extension = outputPath.extension().u8string();
    if (extension.empty()) {
        extension.push_back(u8'.');
        extension.append(SVG_EXTENSION);
    }
    const auto stem = outputPath.stem().u8string();
    result.replace_filename(std::filesystem::path(stem + utils::stringToUtf8(".shape-" + std::to_string(shapeCmper)) + extension));
    return result;
}

std::filesystem::path resolveSvgOutputPath(const std::filesystem::path& outputPath,
                                           int shapeCmper,
                                           bool outputIsFilename,
                                           bool multipleShapes)
{
    if (!outputIsFilename || multipleShapes) {
        return appendSvgShapeSuffix(outputPath, shapeCmper);
    }
    return outputPath;
}

int shapeCmperFromSuggestedSvgName(std::string_view suggestedName)
{
    const std::string suggestedNameString(suggestedName);
    constexpr std::string_view prefix = "shape-";
    constexpr std::string_view suffix = ".svg";
    if (suggestedNameString.rfind(prefix, 0) != 0
        || suggestedNameString.size() <= prefix.size() + suffix.size()
        || !suggestedNameString.ends_with(suffix)) {
        return 0;
    }
    return std::stoi(suggestedNameString.substr(prefix.size(), suggestedNameString.size() - prefix.size() - suffix.size()));
}

void exportSvgWithAdapter(const std::filesystem::path& outputPath,
                          const CommandInputData& inputData,
                          const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
#endif

    ConverterRegistry registry;
    formats::svg::registerConverters(registry);
    const auto* converter = registry.findMultiOutput(FormatId::EnigmaXml, FormatId::Svg);
    if (!converter) {
        throw std::logic_error("SVG converter is not registered.");
    }

    struct PendingSvg
    {
        int shapeCmper{};
        std::string data;
    };

    std::vector<PendingSvg> pendingSvgs;
    converter->convert(enigmaXmlBytes(inputData), [&](std::string_view suggestedName, std::span<const std::byte> svgData) {
        std::string data;
        data.resize(svgData.size());
        std::memcpy(data.data(), svgData.data(), svgData.size());
        pendingSvgs.push_back(PendingSvg{ shapeCmperFromSuggestedSvgName(suggestedName), std::move(data) });
    }, makeConversionOptions(denigmaContext));

    const bool multipleShapes = pendingSvgs.size() > 1;
    size_t generatedCount = 0;
    for (const auto& pendingSvg : pendingSvgs) {
        const auto resolvedOutputPath = resolveSvgOutputPath(
            outputPath, pendingSvg.shapeCmper, denigmaContext.outputIsFilename, multipleShapes);
        if (!denigmaContext.validatePathsAndOptions(resolvedOutputPath)) {
            continue;
        }
        std::ofstream output;
        output.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        output.open(resolvedOutputPath, std::ios::out | std::ios::binary);
        output.write(pendingSvg.data.data(), static_cast<std::streamsize>(pendingSvg.data.size()));
        output.close();
        ++generatedCount;
    }

    if (generatedCount == 0) {
        denigmaContext.logMessage(LogMsg() << "No SVG files were written.", LogSeverity::Warning);
    }
}

} // namespace

// Input format processors
constexpr auto inputProcessors = []() {
    struct InputProcessor
    {
        std::u8string_view extension;
        CommandInputData(*processor)(const std::filesystem::path&, const DenigmaContext&);
    };

    return std::to_array<InputProcessor>({
            { MUSX_EXTENSION, enigmaxml::extractInputData },
            { ENIGMAXML_EXTENSION, enigmaxml::readInputData },
        });
    }();

// Output format processors
constexpr auto outputProcessors = []() {
    struct OutputProcessor
    {
        std::u8string_view extension;
        void(*processor)(const std::filesystem::path&, const CommandInputData&, const DenigmaContext&);
    };

    return std::to_array<OutputProcessor>({
            { MUSX_EXTENSION, enigmaxml::writeMusx },
            { ENIGMAXML_EXTENSION, enigmaxml::write },
            { MSS_EXTENSION, exportMssWithAdapter },
            { SVG_EXTENSION, exportSvgWithAdapter },
            { MNX_EXTENSION, exportMnxJsonWithAdapter },
            { JSON_EXTENSION, exportMnxJsonWithAdapter },
        });
    }();

int ExportCommand::showHelpPage(const std::string_view& programName, const std::string& indentSpaces) const
{
    std::string fullCommand = std::string(programName) + " " + std::string(commandName());
    // Print usage
    std::cout << indentSpaces << "Exports other formats from Finale files. This is the default command." << std::endl;
    std::cout << indentSpaces << "Currently it can export" << std::endl;
    std::cout << indentSpaces << "  musx:       Finale-readable musx file from enigmaxml" << std::endl;
    std::cout << indentSpaces << "  enigmaxml:  the internal xml representation of musx" << std::endl;
    std::cout << indentSpaces << "  mss:        the Styles format for MuseScore" << std::endl;
    std::cout << indentSpaces << "  svg:        Shape Designer shapes as SVG files" << std::endl;
    std::cout << indentSpaces << "  mnx:        MNX open standard files (currently in development)" << std::endl;
    std::cout << indentSpaces << "Note: reverse export to musx is intended for small test cases" << std::endl;
    std::cout << indentSpaces << "      and does not restore ancillary files (for example embedded graphics or audio)." << std::endl;
    std::cout << std::endl;
    std::cout << indentSpaces << "Usage: " << fullCommand << " <input-pattern> [--output options]" << std::endl;
    std::cout << std::endl;
    std::cout << indentSpaces << "Specific options:" << std::endl;
    std::cout << indentSpaces << "  --cue-layer <1..4>              Treat entries in this Finale layer as cue material." << std::endl;
    std::cout << indentSpaces << "  --mnx-schema [file-path]        Validate against this json schema file rather than the embedded one." << std::endl;
    std::cout << indentSpaces << "  --include-tempo-tool            Include tempo changes created with the Tempo Tool." << std::endl;
    std::cout << indentSpaces << "  --no-include-tempo-tool         Exclude tempo changes created with the Tempo Tool (default: exclude)." << std::endl;
    std::cout << indentSpaces << "  --pretty-print [indent-spaces]  Print human readable format (default: on, " << JSON_INDENT_SPACES << " indent spaces)." << std::endl;
    std::cout << indentSpaces << "  --no-pretty-print               Print compact json with no indentions or new lines." << std::endl;
    std::cout << indentSpaces << "  --shape-def <id[,id...]>        Export only specific ShapeDef cmper IDs (repeatable)." << std::endl;
    std::cout << indentSpaces << "  --svg-unit <none|px|pt|pc|cm|mm|in>  Unit suffix for SVG width/height (default: pt)." << std::endl;
    std::cout << indentSpaces << "  --svg-page-scale                Use page-format scaling for SVG output (default: off)." << std::endl;
    std::cout << indentSpaces << "  --no-svg-page-scale             Disable page-format scaling for SVG output (default)." << std::endl;
    std::cout << indentSpaces << "  --svg-scale <positive-float>    Extra SVG scaling multiplier (default: 1.0)." << std::endl;

    // Supported input formats
    std::cout << indentSpaces << "Supported input formats:" << std::endl;
    for (const auto& input : inputProcessors) {
        std::cout << indentSpaces << "  *." << utils::utf8ToString(input.extension);
        if (input.extension == defaultInputFormat()) {
            std::cout << " (default input format)";
        }
        std::cout << std::endl;
    }
    std::cout << indentSpaces << std::endl;

    // Supported output formats
    std::cout << indentSpaces << "Supported output options:" << std::endl;
    for (const auto& output : outputProcessors) {
        std::cout << indentSpaces << "  --" << utils::utf8ToString(output.extension) << " [optional filepath, relative to current directory]";
        if (output.extension == defaultOutputFormat({})) {
            std::cout << " (default output format)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    // Example usage
    std::cout << indentSpaces << "Examples:" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx --enigmaxml output.enigmaxml -mss" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.enigmaxml --mss --part" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " myfolder --mss exports/mss --all-parts --recursive" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.enigmaxml --mnx --mss" << std::endl;
    std::cout << indentSpaces << "  " << fullCommand << " input.musx --svg --shape-def 3,7 --svg-unit px" << std::endl;

    return 1;
}

bool ExportCommand::canProcess(const std::filesystem::path& inputPath) const
{
    try {
        findProcessor(inputProcessors, inputPath.extension().u8string());
        return true;
    } catch (...) {}
    return false;
}

CommandInputData ExportCommand::processInput(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const
{
    auto inputProcessor = findProcessor(inputProcessors, inputPath.extension().u8string());
    return inputProcessor(inputPath, denigmaContext);
}

void ExportCommand::processOutput(const CommandInputData& inputData, const std::filesystem::path& outputPath, const std::filesystem::path&, const DenigmaContext& denigmaContext) const
{
    auto outputProcessor = findProcessor(outputProcessors, outputPath.extension().u8string());
    outputProcessor(outputPath, inputData, denigmaContext);
}

} // namespace denigma
