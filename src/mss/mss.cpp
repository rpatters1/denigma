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

#include "mss.h"
#include "musx/musx.h"
#include "tinyxml2.h"

namespace musxconvert {
namespace mss {

constexpr static char MSS_VERSION[] = "4.50";

void convert(const std::filesystem::path& outputPath, const enigmaxml::Buffer& xmlBuffer)
{
    // ToDo: lots
    try {
        // construct source instance and release input memory
        musx::dom::Document enigmaDocument = musx::factory::DocumentFactory::create<musx::xml::rapidxml::Document>(xmlBuffer);
        //musx::dom::Document enigmaDocument = musx::factory::DocumentFactory::create<musx::xml::tinyxml2::Document>(xmlBuffer);

        // extract document to mss
        tinyxml2::XMLDocument mssDoc; // output
        mssDoc.InsertEndChild(mssDoc.NewDeclaration());
        auto museScoreElement = mssDoc.NewElement("museScore");
        museScoreElement->SetAttribute("version", MSS_VERSION);
        mssDoc.InsertEndChild(museScoreElement);
        auto styleElement = museScoreElement->InsertNewChildElement("Style");
        if (mssDoc.SaveFile(outputPath.string().c_str()) != ::tinyxml2::XML_SUCCESS) {
            throw std::runtime_error(mssDoc.ErrorStr());
        }
        std::cout << "converted to " << outputPath.string() << std::endl;
    } catch (const musx::xml::load_error& ex) {
        std::cerr << "Load XML failed: " << ex.what() << std::endl;
        throw;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        throw;
    }
}

} // namespace mss
} // namespace musxconvert
