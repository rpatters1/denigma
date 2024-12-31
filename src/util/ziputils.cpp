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
#include "util/ziputils.h"

 // NOTE: This namespace is necessary because zip_file.hpp is poorly implemented and
//          can only be included once in the entire project.
#include "zip_file.hpp"

namespace ziputils {

using namespace denigma;

/// @brief Opens zip file using std::ifstream, which works with utf16 paths on Windows
/// @param zipFilePath zip archive
/// @return zip_file instance
static miniz_cpp::zip_file openZip(const std::filesystem::path& zipFilePath, const DenigmaContext& denigmaContext)
{
    try {
        std::ifstream zipReader;
        zipReader.exceptions(std::ios::failbit | std::ios::badbit);
        zipReader.open(zipFilePath, std::ios::binary);
        return miniz_cpp::zip_file(zipReader);
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from file " << zipFilePath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

std::string readFile(const std::filesystem::path& zipFilePath, const std::string& fileName, const DenigmaContext& denigmaContext)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    return zip.read(fileName);
}

static bool iterateFiles(miniz_cpp::zip_file& zip, const denigma::DenigmaContext& denigmaContext,
    std::optional<std::string> searchForFile, std::function<bool(const miniz_cpp::zip_info&)> iterator)
{
    bool calledIterator = false;
    for (auto& fileInfo : zip.infolist()) {
        /*
        std::cout << "Unzipping " << outputDir << "/" << fileInfo.filename << " ";
        std::cout << std::setfill('0')
                    << fileInfo.date_time.year << '-'
                    << std::setw(2) << fileInfo.date_time.month << '-'
                    << std::setw(2) << fileInfo.date_time.day << ' '
                    << std::setw(2) << fileInfo.date_time.hours << ':'
                    << std::setw(2) << fileInfo.date_time.minutes << ':'
                    << std::setw(2) << fileInfo.date_time.seconds << std::endl
                    << std::setfill(' ');
        std::cout << "-----------\n create_version " << fileInfo.create_version
                    << "\n extract_version " << fileInfo.extract_version
                    << "\n flag " << fileInfo.flag_bits
                    << "\n compressed_size " << fileInfo.compress_size
                    << "\n crc " << fileInfo.crc
                    << "\n compress_size " << fileInfo.compress_size
                    << "\n file_size " << fileInfo.file_size
                    << "\n extra " << fileInfo.extra
                    << "\n comment " << fileInfo.comment
                    << "\n header_offset " << fileInfo.header_offset;
        std::cout << std::showbase << std::hex
                    << "\n internal_attr " << fileInfo.internal_attr
                    << "\n external_attr " << fileInfo.external_attr << "\n\n"
                    << std::noshowbase << std::dec;
        */
        if (searchForFile.has_value() && searchForFile.value() != fileInfo.filename) {
            continue;
        }
        calledIterator = true;
        if (!iterator(fileInfo)) {
            break;
        }
    }
    return calledIterator;
}

static std::string getMusicXmlScoreName(const std::filesystem::path& zipFilePath, miniz_cpp::zip_file& zip, const denigma::DenigmaContext& denigmaContext)
{
    std::filesystem::path defaultName = zipFilePath.filename();
    defaultName.replace_extension(MUSICXML_EXTENSION);
    std::string fileName = defaultName.filename().u8string();
    try {
        iterateFiles(zip, denigmaContext, std::nullopt, [&](const miniz_cpp::zip_info& fileInfo) {
            if (fileInfo.filename == "META-INF/container.xml") {
                pugi::xml_document containerXml;
                auto parseResult = containerXml.load_string(zip.read(fileInfo).c_str());
                if (!parseResult) {
                    throw std::runtime_error("Error parsing container.xml: " + std::string(parseResult.description()));
                }
                if (auto path = containerXml.child("container").child("rootfiles").child("rootfile").attribute("full-path")) {
                    fileName = path.value();
                }
                return false;
            }
            return true;
        });
        return fileName;
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract META-INF/container.xml from file " << zipFilePath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

std::string getMusicXmlScoreFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    return zip.read(getMusicXmlScoreName(zipFilePath, zip, denigmaContext));
}

bool iterateMusicXmlPartFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, const std::optional<std::string>& fileName, IteratorFunc iterator)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    std::string scoreName = getMusicXmlScoreName(zipFilePath, zip, denigmaContext);
    return iterateFiles(zip, denigmaContext, std::nullopt, [&](const miniz_cpp::zip_info& fileInfo) {
        if (scoreName == fileInfo.filename) {
            return true; // skip score
        }
        if (fileName.has_value() && fileName.value() != fileInfo.filename) {
            return true; // skip parts that aren't the one we are looking for
        }
        std::filesystem::path nextPath = stringutils::utf8ToPath(fileInfo.filename);
        if (nextPath.extension().u8string() == std::string(".") + MUSICXML_EXTENSION) {
            return iterator(nextPath, zip.read(fileInfo.filename));
        }
        return true;
    });
}

bool iterateModifyFilesInPlace(const std::filesystem::path& zipFilePath, const std::filesystem::path& outputPath, const denigma::DenigmaContext& denigmaContext, ModifyIteratorFunc iterator)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    miniz_cpp::zip_file outputZip;
    std::string scoreName = getMusicXmlScoreName(zipFilePath, zip, denigmaContext);
    bool retval = iterateFiles(zip, denigmaContext, std::nullopt, [&](const miniz_cpp::zip_info& fileInfo) {
        std::filesystem::path nextPath = stringutils::utf8ToPath(fileInfo.filename);
        if (nextPath.has_filename()) {
            std::string buffer = zip.read(fileInfo);
            if (iterator(nextPath, buffer, scoreName == fileInfo.filename)) {
                outputZip.writestr(fileInfo, buffer);
            }
        }
        return true;
    });
    try {
        std::ofstream zipWriter;
        zipWriter.exceptions(std::ios::failbit | std::ios::badbit);
        zipWriter.open(outputPath, std::ios::binary);
        outputZip.save(zipWriter);
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to save data to file " << outputPath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
    return retval;
}

} // namespace ziputils
