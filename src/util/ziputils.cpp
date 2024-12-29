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
#include "pugixml.hpp"

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
        std::ifstream zipFile;
        zipFile.exceptions(std::ios::failbit | std::ios::badbit);
        zipFile.open(zipFilePath, std::ios::binary);
        return miniz_cpp::zip_file(zipFile);
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

static void iterateFiles(miniz_cpp::zip_file& zip, const denigma::DenigmaContext& denigmaContext,
    std::optional<std::string> searchForFile, std::function<bool(const miniz_cpp::zip_info&)> iterator)
{
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
        if (!iterator(fileInfo)) {
            break;
        }
    }
}

std::string getMusicXmlRootFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext)
{
    std::filesystem::path defaultName = zipFilePath.filename();
    defaultName.replace_extension(MUSICXML_EXTENSION);
    std::string fileName = defaultName.filename().u8string();
    auto zip = openZip(zipFilePath, denigmaContext);
    try {
        iterateFiles(zip, denigmaContext, std::nullopt, [&](const miniz_cpp::zip_info& fileInfo) {
            if (fileInfo.filename == "META-INF/container.xml") {
                pugi::xml_document containerXml;
                auto parseResult = containerXml.load_string(zip.read(fileInfo).c_str());
                if (!parseResult) {
                    throw std::runtime_error("Error parsing container.xml: " + std::string(parseResult.description()));
                }
                pugi::xml_node root = containerXml.child("rootfiles").child("rootfile");
                if (root) {
                    pugi::xml_attribute path = root.attribute("full-path");
                    if (path) {
                        fileName = path.value();
                    }
                }
                return false;
            }
            return true;
        });
        return zip.read(fileName);
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from file " << zipFilePath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

void iterateFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, std::function<bool(const std::string&)> iterator)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    iterateFiles(zip, denigmaContext, std::nullopt, [&](const miniz_cpp::zip_info& fileInfo) {
        return iterator(zip.read(fileInfo.filename));
    });
}

void iterateFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, const std::string& searchFileName, std::function<bool(const std::string&)> iterator)
{
    auto zip = openZip(zipFilePath, denigmaContext);
    iterateFiles(zip, denigmaContext, searchFileName, [&](const miniz_cpp::zip_info& fileInfo) {
        return iterator(zip.read(fileInfo.filename));
    });
}

} // namespace ziputils
