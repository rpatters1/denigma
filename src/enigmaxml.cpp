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
#include <vector>
#include <iostream>

#include "zip_file.hpp"		// miniz submodule (for zip)
#include "ezgz.hpp"			// ezgz submodule (for gzip)
#include "musx/musx.h"

constexpr char SCORE_DAT_NAME[] = "score.dat";

std::vector<char> readEnigmaXml(const std::string& inputPath)
{
    try {
        std::ifstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(inputPath, std::ios::binary);
        return std::vector<char>((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());
    } catch (const std::ios_base::failure& ex) {
        std::cout << "unable to read " << inputPath << std::endl
                  << "message: " << ex.what() << std::endl
                  << "details: " << std::strerror(ex.code().value()) << std::endl;
        throw;
    };
}

std::vector<char> extractEnigmaXml(const std::string& inputPath)
{
    try
    {
        miniz_cpp::zip_file zip(inputPath.c_str());
        std::string buffer = zip.read(SCORE_DAT_NAME);
        musx::util::ScoreFileEncoder::recodeBuffer(buffer);
        return EzGz::IGzFile<>({reinterpret_cast<uint8_t *>(buffer.data()), buffer.size()}).readAll();
    }
    catch (const std::exception &ex)
    {
        std::cout << "Error: unable to extract enigmaxml from file " << inputPath << " (exception: " << ex.what() << ")" << std::endl;
        throw;
    }
}

void writeEnigmaXml(const std::string& outputPath, const std::vector<char>& xmlBuffer)
{
    std::cout << "extracting to " << outputPath << std::endl;

    try	{
        std::ifstream inFile;

        size_t uncompressedSize = xmlBuffer.size();
        std::cout << "decompressed size of enigmaxml: " << uncompressedSize << std::endl;

        std::ofstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(outputPath, std::ios::binary);
        xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
    } catch (const std::ios_base::failure& ex) {
        std::cout << "unable to write " << outputPath << std::endl
                  << "message: " << ex.what() << std::endl
                  << "details: " << std::strerror(ex.code().value()) << std::endl;
        throw;
    } catch (const std::exception &ex) {
        std::cout << "unable to write " << outputPath << " (exception: " << ex.what() << ")" << std::endl;
        throw;
    }
}
