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
#include "tinyxml2.h"

 // NOTE: This namespace is necessary because zip_file.hpp is poorly implemented and
//          can only be included once in the entire project.
#include "zip_file.hpp"

namespace ziputils {

using namespace denigma;

std::string readFile(const std::filesystem::path& zipFilePath, const std::string& fileName, const DenigmaContext& denigmaContext)
{
    try {
        std::ifstream zipFile;
        zipFile.exceptions(std::ios::failbit | std::ios::badbit);
        zipFile.open(zipFilePath, std::ios::binary);
        miniz_cpp::zip_file zip(zipFile);
        return zip.read(fileName);
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from file " << zipFilePath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

std::string getMusicXmlRootFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext)
{
    try {
        std::ifstream zipFile;
        zipFile.exceptions(std::ios::failbit | std::ios::badbit);
        zipFile.open(zipFilePath, std::ios::binary);
        miniz_cpp::zip_file zip(zipFile);
        std::filesystem::path defaultName = zipFilePath.filename();
        defaultName.replace_extension(MUSICXML_EXTENSION);
        std::string fileName = defaultName.filename().u8string();
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
            if (fileInfo.filename == "META-INF/container.xml") {
                ::tinyxml2::XMLDocument containerXml;
                if (containerXml.Parse(zip.read(fileInfo).c_str()) != ::tinyxml2::XML_SUCCESS) {
                    throw std::runtime_error("Error parsing container.xml: " + std::string(containerXml.ErrorStr()));
                }
                ::tinyxml2::XMLHandle root(containerXml.RootElement());
                auto rootFileElement = root.FirstChildElement("rootfiles").FirstChildElement("rootfile").ToElement();
                if (rootFileElement) {
                    if (auto path = rootFileElement->FindAttribute("full-path")) {
                        fileName = path->Value();
                    }
                }
                break;
            }
		}
        return zip.read(fileName);
    } catch (const std::exception &ex) {
        denigmaContext.logMessage(LogMsg() << "unable to extract data from file " << zipFilePath.u8string(), LogSeverity::Error);
        denigmaContext.logMessage(LogMsg() << " (exception: " << ex.what() << ")", LogSeverity::Error);
        throw;
    }
}

} // namespace ziputils
