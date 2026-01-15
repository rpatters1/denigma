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
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>
#include <sstream>

#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wc++20-extensions"
#elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wattributes"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 5051)
#endif

#include "ezgz.hpp"			// ezgz submodule (for gzip)

#if defined(__clang__)
    #pragma clang diagnostic pop
#elif defined(__GNUC__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif

#include "musx/musx.h"

#include "denigma.h"
#include "export/enigmaxml.h"
#include "utils/ziputils.h"
#include "score_encoder/score_encoder.h"

constexpr char SCORE_DAT_NAME[] = "score.dat";

namespace denigma {
namespace enigmaxml {

Buffer read(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Reading " << inputPath.u8string());
        return {};
    }
#endif
    try {
        std::ifstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(inputPath, std::ios::binary | std::ios::ate);

        // Preallocate buffer based on file size
        std::streamsize fileSize = xmlFile.tellg();
        xmlFile.seekg(0, std::ios::beg);

        Buffer buffer(static_cast<std::size_t>(fileSize));
        xmlFile.read(buffer.data(), fileSize);
        return buffer;
    } catch (const std::ios_base::failure& ex) {
        denigmaContext.logMessage(LogMsg() << "unable to read " << inputPath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << "message: " << ex.what(), LogSeverity::Error);
        if (const auto* ec = dynamic_cast<const std::error_code*>(&ex.code())) {
            denigmaContext.logMessage(LogMsg() << "details: " << ec->message(), LogSeverity::Error);
        }
        throw;
    };
}

Buffer extract(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Extracting " << inputPath.u8string());
        return {};
    }
#endif
    try {
        std::string buffer = utils::readFile(inputPath, SCORE_DAT_NAME, denigmaContext);
        musx::encoder::ScoreFileEncoder::recodeBuffer(buffer);
        return EzGz::IGzFile<>({ reinterpret_cast<uint8_t*>(buffer.data()), buffer.size() }).readAll();
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract enigmaxml from file " << inputPath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

void write(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Writing " << outputPath.u8string());
        return;
    }
#endif

    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    try	{
        std::ifstream inFile;

        size_t uncompressedSize = xmlBuffer.size();
        denigmaContext.logMessage(LogMsg() << "decompressed size of enigmaxml: " << uncompressedSize);

        std::ofstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(outputPath, std::ios::binary);
        xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
    } catch (const std::ios_base::failure& ex) {
        std::stringstream sst;
        denigmaContext.logMessage(LogMsg() << "unable to write " << outputPath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << "message: " << ex.what(), LogSeverity::Error);
        if (const auto* ec = dynamic_cast<const std::error_code*>(&ex.code())) {
            denigmaContext.logMessage(LogMsg() << "details: " << ec->message(), LogSeverity::Error);
        }
        throw;
    }
}

} // namespace enigmaxml
} // namespace denigma
