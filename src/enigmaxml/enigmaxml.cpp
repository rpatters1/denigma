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

#include "musx/musx.h"
#include "enigmaxml.h"
#include "zip_file.hpp"		// miniz submodule (for zip)
#include "ezgz.hpp"			// ezgz submodule (for gzip)

#include "denigma.h"

constexpr char SCORE_DAT_NAME[] = "score.dat";

namespace denigma {
namespace enigmaxml {

Buffer read(const std::filesystem::path& inputPath, const DenigmaOptions& options)
{
    try {
        std::ifstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(inputPath, std::ios::binary);
        return Buffer((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());
    } catch (const std::ios_base::failure& ex) {
        logMessage(LogMsg() << "unable to read " << inputPath.u8string(), options, LogSeverity::Error);
        logMessage(LogMsg() << "message: " << ex.what(), options, LogSeverity::Error);
        logMessage(LogMsg() << "details: " << std::strerror(ex.code().value()), options, LogSeverity::Error);
        throw;
    };
}

Buffer extract(const std::filesystem::path& inputPath, const DenigmaOptions& options)
{
    try {
        miniz_cpp::zip_file zip(inputPath.string());
        std::string buffer = zip.read(SCORE_DAT_NAME);
        musx::util::ScoreFileEncoder::recodeBuffer(buffer);
        return EzGz::IGzFile<>({ reinterpret_cast<uint8_t*>(buffer.data()), buffer.size() }).readAll();
    } catch (const std::exception &ex) {
        logMessage(LogMsg() << "unable to extract enigmaxml from file " << inputPath.u8string(), options, LogSeverity::Error);
        logMessage(LogMsg() << " (exception: " << ex.what() << ")", options, LogSeverity::Error);
        throw;
    }
}

void write(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaOptions& options)
{
    if (!denigma::validatePathsAndOptions(outputPath, options)) return;

    try	{
        std::ifstream inFile;

        size_t uncompressedSize = xmlBuffer.size();
        logMessage(LogMsg() << "decompressed size of enigmaxml: " << uncompressedSize, options);

        std::ofstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(outputPath, std::ios::binary);
        xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
    } catch (const std::ios_base::failure& ex) {
        std::stringstream sst;
        logMessage(LogMsg() << "unable to write " << outputPath.u8string(), options, LogSeverity::Error);
        logMessage(LogMsg() << "message: " << ex.what(), options, LogSeverity::Error);
        logMessage(LogMsg() << "details: " << std::strerror(ex.code().value()), options, LogSeverity::Error);
        throw;
    }
}

} // namespace enigmaxml
} // namespace denigma
