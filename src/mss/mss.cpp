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
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono> // temp

#include "mss.h"
#include "musx/musx.h"
#include "tinyxml2.h"

namespace musxconvert {
namespace mss {

// placeholder
void convert(const std::filesystem::path& file, const enigmaxml::Buffer& xmlBuffer)
{
    std::cout << "converting to " << file.string() << std::endl;

    // temp:
    try {
        auto start = std::chrono::high_resolution_clock::now();
        musx::xml::tinyxml2::Document enigmaXml;
        enigmaXml.loadFromString(xmlBuffer);
        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "tinyxml2 load time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds" << std::endl;

        start = std::chrono::high_resolution_clock::now();
        musx::xml::rapidxml::Document enigmaXmlRapid;
        enigmaXmlRapid.loadFromString(xmlBuffer);
        end = std::chrono::high_resolution_clock::now();
        std::cout << "rapidxml load time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " microseconds" << std::endl;
        tinyxml2::XMLDocument mssDoc;
    }
    catch (const musx::xml::load_error& ex) {
        std::cerr << "Load XML failed: " << ex.what() << std::endl;
        throw;
    }
    catch (const std::exception& ex) {
        std::cerr << "Unknown error: " << ex.what() << std::endl;
        throw;
    }
}

} // namespace mss
} // namespace musxconvert
