/*
 * Copyright (C) 2026, Robert Patterson
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
#include "export/svg.h"

#include <fstream>
#include <vector>

#include "musx/musx.h"
#include "utils/stringutils.h"
#include "utils/textmetrics.h"

namespace denigma {
namespace svgexp {

using namespace musx::dom;
using namespace musx::factory;

namespace {

DocumentFactory::CreateOptions::EmbeddedGraphicFiles createEmbeddedGraphicFiles(const CommandInputData& inputData)
{
    DocumentFactory::CreateOptions::EmbeddedGraphicFiles files;
    if (inputData.embeddedGraphics.empty()) {
        return files;
    }
    files.reserve(inputData.embeddedGraphics.size());
    for (const auto& graphic : inputData.embeddedGraphics) {
        DocumentFactory::CreateOptions::EmbeddedGraphicFile file;
        file.filename = graphic.filename;
        file.bytes.assign(graphic.blob.begin(), graphic.blob.end());
        files.emplace_back(std::move(file));
    }
    return files;
}

DocumentPtr createDocument(const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
    DocumentFactory::CreateOptions options(
        denigmaContext.inputFilePath,
        inputData.notationMetadata.value_or(Buffer{}),
        createEmbeddedGraphicFiles(inputData));
    return DocumentFactory::create<MusxReader>(inputData.primaryBuffer, std::move(options));
}

std::filesystem::path appendShapeSuffix(const std::filesystem::path& outputPath, Cmper shapeCmper)
{
    std::filesystem::path result = outputPath;
    std::u8string extension = outputPath.extension().u8string();
    if (extension.empty()) {
        extension = std::u8string(u8".") + utils::stringToUtf8(SVG_EXTENSION);
    }
    const std::u8string stem = outputPath.stem().u8string();
    result.replace_filename(utils::utf8ToPath(stem + utils::stringToUtf8(".shape-" + std::to_string(shapeCmper)) + extension));
    return result;
}

std::filesystem::path resolveOutputPath(const std::filesystem::path& outputPath,
                                        Cmper shapeCmper,
                                        bool outputIsFilename,
                                        bool multipleShapes)
{
    if (!outputIsFilename || multipleShapes) {
        return appendShapeSuffix(outputPath, shapeCmper);
    }
    return outputPath;
}

std::vector<MusxInstance<others::ShapeDef>> selectShapes(const DocumentPtr& document, const DenigmaContext& denigmaContext)
{
    std::vector<MusxInstance<others::ShapeDef>> result;
    if (denigmaContext.svgShapeDefs.empty()) {
        auto allShapes = document->getOthers()->getArray<others::ShapeDef>(SCORE_PARTID);
        result.reserve(allShapes.size());
        for (const auto& shape : allShapes) {
            if (shape && !shape->isBlank()) {
                result.push_back(shape);
            }
        }
        return result;
    }

    result.reserve(denigmaContext.svgShapeDefs.size());
    for (Cmper shapeId : denigmaContext.svgShapeDefs) {
        auto shape = document->getOthers()->get<others::ShapeDef>(SCORE_PARTID, shapeId);
        if (!shape) {
            denigmaContext.logMessage(LogMsg() << "Requested ShapeDef cmper " << shapeId << " was not found.", LogSeverity::Warning);
            continue;
        }
        if (shape->isBlank()) {
            denigmaContext.logMessage(LogMsg() << "Requested ShapeDef cmper " << shapeId << " is blank and was skipped.", LogSeverity::Warning);
            continue;
        }
        result.push_back(shape);
    }
    return result;
}

} // namespace

void convert(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
#endif

    auto document = createDocument(inputData, denigmaContext);
    const auto shapes = selectShapes(document, denigmaContext);
    if (shapes.empty()) {
        denigmaContext.logMessage(LogMsg() << "No ShapeDef entries matched the SVG export filters.", LogSeverity::Warning);
        return;
    }

    const bool usePageFormatScaling = denigmaContext.svgUsePageScale;
    const double svgScale = denigmaContext.svgScale;
    const bool multipleShapes = shapes.size() > 1;
    const auto glyphMetrics = textmetrics::makeSvgGlyphMetricsCallback(denigmaContext);
    denigmaContext.logMessage(LogMsg() << "SVG scaling pageScale=" << (usePageFormatScaling ? "on" : "off")
                                       << " user=" << svgScale
                                       << " path=" << (usePageFormatScaling ? "toSvgWithPageFormatScaling" : "toSvg"),
                              LogSeverity::Verbose);

    size_t generatedCount = 0;
    for (const auto& shape : shapes) {
        const auto resolvedOutputPath = resolveOutputPath(outputPath, shape->getCmper(), denigmaContext.outputIsFilename, multipleShapes);
        if (!denigmaContext.validatePathsAndOptions(resolvedOutputPath)) {
            continue;
        }
        const std::string svgData = usePageFormatScaling
            ? musx::util::SvgConvert::toSvgWithPageFormatScaling(*shape, denigmaContext.svgUnit, glyphMetrics)
            : musx::util::SvgConvert::toSvg(*shape, svgScale, denigmaContext.svgUnit, glyphMetrics);
        if (svgData.empty()) {
            denigmaContext.logMessage(LogMsg() << "ShapeDef cmper " << shape->getCmper()
                                               << " could not be converted to SVG (likely unresolved external graphic).",
                                      LogSeverity::Warning);
            continue;
        }
        std::ofstream out;
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out.open(resolvedOutputPath, std::ios::out | std::ios::binary);
        out.write(svgData.data(), static_cast<std::streamsize>(svgData.size()));
        out.close();
        ++generatedCount;
    }

    if (generatedCount == 0) {
        denigmaContext.logMessage(LogMsg() << "No SVG files were written.", LogSeverity::Warning);
    }
}

} // namespace svgexp
} // namespace denigma
