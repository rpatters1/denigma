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
#include <array>
#include <filesystem>
#include <vector>
#include <iostream>
#include <sstream>
#include <limits>
#include <stdexcept>

#include "zlib.h"

#include "musx/musx.h"

#include "denigma.h"
#include "export/enigmaxml.h"
#include "utils/ziputils.h"
#include "score_encoder/score_encoder.h"

constexpr char SCORE_DAT_NAME[] = "score.dat";

namespace denigma {
namespace enigmaxml {

static Buffer gunzipBuffer(const std::string& compressedData)
{
    z_stream stream{};
    int rc = inflateInit2(&stream, 16 + MAX_WBITS); // 16 + MAX_WBITS = gzip stream
    if (rc != Z_OK) {
        throw std::runtime_error("unable to initialize zlib inflate");
    }

    Buffer output;
    output.reserve(compressedData.size());
    std::array<char, 16384> chunk{};

    const auto* nextInput = reinterpret_cast<const Bytef*>(compressedData.data());
    std::size_t remainingInput = compressedData.size();
    int inflateRc = Z_OK;
    while (true) {
        if (stream.avail_in == 0 && remainingInput > 0) {
            const std::size_t inputChunk = std::min<std::size_t>(remainingInput, std::numeric_limits<uInt>::max());
            stream.next_in = const_cast<Bytef*>(nextInput);
            stream.avail_in = static_cast<uInt>(inputChunk);
            nextInput += inputChunk;
            remainingInput -= inputChunk;
        }

        stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        inflateRc = inflate(&stream, Z_NO_FLUSH);

        if (inflateRc != Z_OK && inflateRc != Z_STREAM_END) {
            inflateEnd(&stream);
            throw std::runtime_error("unable to decompress gzip stream");
        }

        const auto written = static_cast<std::size_t>(chunk.size() - stream.avail_out);
        output.insert(output.end(), chunk.data(), chunk.data() + written);

        if (inflateRc == Z_STREAM_END) {
            break;
        }

        if (stream.avail_in == 0 && remainingInput == 0 && written == 0) {
            inflateEnd(&stream);
            throw std::runtime_error("unexpected end of gzip stream");
        }
    }

    inflateEnd(&stream);
    return output;
}

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
        return gunzipBuffer(buffer);
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
