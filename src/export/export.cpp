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
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <optional>

#include "denigma/formats/mnx.h"
#include "export/export.h"
#include "export/enigmaxml.h"
#include "export/mss.h"
#include "export/svg.h"

namespace denigma {

namespace {

ConversionOptions makeMnxConversionOptions(const DenigmaContext& denigmaContext)
{
    ConversionOptions options;
    options.sourceName = denigmaContext.inputFilePath.string();
    options.validate = !denigmaContext.noValidate;
    options.indentSpaces = denigmaContext.indentSpaces;
    options.cueLayer = denigmaContext.cueLayer;
    options.mnxSchema = denigmaContext.mnxSchema;
    options.mnxIncludeTempoTool = denigmaContext.includeTempoTool;
    options.mnxSplitInstruments = denigmaContext.mnxSplitInstruments;
    return options;
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

    const auto input = std::as_bytes(std::span(inputData.primaryBuffer.data(), inputData.primaryBuffer.size()));
    converter->convert(input, output, makeMnxConversionOptions(denigmaContext));
    output.close();
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
            { MSS_EXTENSION, mss::convert },
            { SVG_EXTENSION, svgexp::convert },
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
