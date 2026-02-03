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
#include <algorithm>
#include <array>
#include <filesystem>
#include <vector>
#include <iostream>
#include <sstream>
#include <ctime>
#include <regex>
#include <limits>
#include <stdexcept>

#include "zlib.h"
#include "zip.h"

#ifdef _WIN32
#include "iowin32.h"
#endif

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

static std::string gzipBuffer(const Buffer& uncompressedData)
{
    z_stream stream{};
    int rc = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (rc != Z_OK) {
        throw std::runtime_error("unable to initialize zlib deflate");
    }

    std::string output;
    output.reserve(uncompressedData.size() / 2);
    std::array<char, 16384> chunk{};

    const auto* nextInput = reinterpret_cast<const Bytef*>(uncompressedData.data());
    std::size_t remainingInput = uncompressedData.size();
    int deflateRc = Z_OK;
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
        deflateRc = deflate(&stream, (remainingInput == 0 && stream.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH);

        if (deflateRc != Z_OK && deflateRc != Z_STREAM_END) {
            deflateEnd(&stream);
            throw std::runtime_error("unable to compress gzip stream");
        }

        const auto written = static_cast<std::size_t>(chunk.size() - stream.avail_out);
        output.append(chunk.data(), written);

        if (deflateRc == Z_STREAM_END) {
            break;
        }
    }

    deflateEnd(&stream);
    return output;
}

static zipFile openZipForWrite(const std::filesystem::path& outputPath)
{
#ifdef _WIN32
    zlib_filefunc64_def fileFuncs{};
    fill_win32_filefunc64W(&fileFuncs);
    std::wstring widePath = outputPath.wstring();
    return zipOpen2_64(widePath.c_str(), APPEND_STATUS_CREATE, nullptr, &fileFuncs);
#else
    std::string utf8Path = outputPath.u8string();
    return zipOpen64(utf8Path.c_str(), APPEND_STATUS_CREATE);
#endif
}

static zip_fileinfo makeCurrentZipFileInfo()
{
    zip_fileinfo info{};
    const std::time_t now = std::time(nullptr);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    info.tmz_date.tm_sec = localTime.tm_sec;
    info.tmz_date.tm_min = localTime.tm_min;
    info.tmz_date.tm_hour = localTime.tm_hour;
    info.tmz_date.tm_mday = localTime.tm_mday;
    info.tmz_date.tm_mon = localTime.tm_mon;
    info.tmz_date.tm_year = localTime.tm_year + 1900;
    return info;
}

static void writeZipEntry(zipFile outputZip, const char* name, const std::string& content, int method, int level)
{
    zip_fileinfo info = makeCurrentZipFileInfo();
    int rc = zipOpenNewFileInZip64(
        outputZip,
        name,
        &info,
        nullptr,
        0,
        nullptr,
        0,
        nullptr,
        method,
        level,
        content.size() >= 0xffffffffULL ? 1 : 0
    );
    if (rc != ZIP_OK) {
        throw std::runtime_error(std::string("unable to create ") + name + " in musx archive");
    }

    std::size_t offset = 0;
    while (offset < content.size()) {
        const std::size_t chunkSize = std::min<std::size_t>(content.size() - offset, std::numeric_limits<unsigned>::max());
        rc = zipWriteInFileInZip(outputZip, content.data() + offset, static_cast<unsigned>(chunkSize));
        if (rc < 0) {
            zipCloseFileInZip(outputZip);
            throw std::runtime_error(std::string("unable to write ") + name + " to musx archive");
        }
        offset += chunkSize;
    }

    rc = zipCloseFileInZip(outputZip);
    if (rc != ZIP_OK) {
        throw std::runtime_error(std::string("unable to finalize ") + name + " in musx archive");
    }
}

static std::pair<int, int> extractFileVersionFromEnigmaXml(const Buffer& xmlBuffer)
{
    const std::string xmlText(xmlBuffer.begin(), xmlBuffer.end());
    std::smatch match;
    const std::regex modifiedFileVersion(
        R"(<modified>[\s\S]*?<fileVersion>[\s\S]*?<major>(\d+)</major>[\s\S]*?<minor>(\d+)</minor>)"
    );
    if (std::regex_search(xmlText, match, modifiedFileVersion) && match.size() >= 3) {
        return { std::stoi(match[1].str()), std::stoi(match[2].str()) };
    }

    const std::regex createdFileVersion(
        R"(<created>[\s\S]*?<fileVersion>[\s\S]*?<major>(\d+)</major>[\s\S]*?<minor>(\d+)</minor>)"
    );
    if (std::regex_search(xmlText, match, createdFileVersion) && match.size() >= 3) {
        return { std::stoi(match[1].str()), std::stoi(match[2].str()) };
    }

    return { 27, 4 };
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

void writeMusx(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Writing " << outputPath.u8string());
        return;
    }
#endif

    if (!denigmaContext.validatePathsAndOptions(outputPath)) return;

    try {
        std::string encodedBuffer = gzipBuffer(xmlBuffer);
        musx::encoder::ScoreFileEncoder::recodeBuffer(encodedBuffer);
        const auto [fileVersionMajor, fileVersionMinor] = extractFileVersionFromEnigmaXml(xmlBuffer);

        zipFile outputZip = openZipForWrite(outputPath);
        if (!outputZip) {
            throw std::runtime_error("unable to create output musx archive");
        }

        static const std::string kMimetype = "application/vnd.makemusic.notation";
        const std::string kContainerXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<container version=\"" + std::to_string(fileVersionMajor) + "\" xmlns=\"http://www.makemusic.com/2012/container\">\n"
            "  <rootfiles>\n"
            "    <rootfile full-path=\"score.dat\" media-type=\"application/vnd.makemusic.notation.dat.1\"/>\n"
            "  </rootfiles>\n"
            "</container>\n";
        const std::string kNotationMetadataXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<metadata version=\"" + std::to_string(fileVersionMajor) + "." + std::to_string(fileVersionMinor) + "\" xmlns=\"http://www.makemusic.com/2012/NotationMetadata\">\n"
            "  <fileInfo>\n"
            "    <keySignature>C</keySignature>\n"
            "    <initialTempo>96</initialTempo>\n"
            "    <scoreDuration>0</scoreDuration>\n"
            "    <creatorString>denigma " DENIGMA_VERSION " reverse export</creatorString>\n"
            "  </fileInfo>\n"
            "</metadata>\n";

        writeZipEntry(outputZip, "mimetype", kMimetype, 0, 0);
        writeZipEntry(outputZip, "META-INF/container.xml", kContainerXml, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
        writeZipEntry(outputZip, "NotationMetadata.xml", kNotationMetadataXml, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
        writeZipEntry(outputZip, SCORE_DAT_NAME, encodedBuffer, Z_DEFLATED, Z_DEFAULT_COMPRESSION);

        int rc = zipClose(outputZip, nullptr);
        if (rc != ZIP_OK) {
            throw std::runtime_error("unable to finalize musx archive");
        }
    } catch (const std::exception& ex) {
        denigmaContext.logMessage(LogMsg() << "unable to write musx to " << outputPath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

} // namespace enigmaxml
} // namespace denigma
