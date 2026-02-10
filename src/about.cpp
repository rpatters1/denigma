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
#include "denigma.h"

namespace denigma {
#include "denigma_license.xxd"
}

namespace zlib {
#include "zlib_license.xxd"
}

namespace mnxdom {
#include "mnxdom_license.xxd"
}

namespace musx {
#include "musx_license.xxd"
}

namespace smufl_mapping {
#include "smufl_mapping_license.xxd"
}

namespace pugixml {
#include "pugixml_license.xxd"
}

namespace freetype {
#include "freetype_license.xxd"
}

constexpr std::basic_string_view<unsigned char> denigmaLicense(denigma::LICENSE, sizeof(denigma::LICENSE));
constexpr std::basic_string_view<unsigned char> zlibLicense(zlib::LICENSE, sizeof(zlib::LICENSE));
constexpr std::basic_string_view<unsigned char> mnxLicense(mnxdom::LICENSE, sizeof(mnxdom::LICENSE));
constexpr std::basic_string_view<unsigned char> musxLicense(musx::LICENSE, sizeof(musx::LICENSE));
constexpr std::basic_string_view<unsigned char> smuflLicense(smufl_mapping::LICENSE, sizeof(smufl_mapping::LICENSE));
constexpr std::basic_string_view<unsigned char> pugixmlLicense(pugixml::LICENSE_md, sizeof(pugixml::LICENSE_md));
constexpr std::basic_string_view<unsigned char> freetypeLicense(freetype::FTL_TXT, sizeof(freetype::FTL_TXT));

constexpr auto licenses = std::to_array<std::pair<std::string_view, std::basic_string_view<unsigned char>>>({
    { "denigma", denigmaLicense },
    { "musx object model", musxLicense },
    { "mnx object model", mnxLicense },
    { "SMuFL mapping", smuflLicense },
    { "zlib", zlibLicense },
    { "pugixml", pugixmlLicense },
    { "FreeType (FTL)", freetypeLicense }
});

namespace denigma {

void showAboutPage()
{
    for (const auto& next : licenses) {
        std::cout << "=================" << std::endl;
        std::cout << next.first << std::endl;
        std::cout << "=================" << std::endl;
        std::string cstrBuffer(next.second.begin(), next.second.end());
        std::cout << cstrBuffer << std::endl;
    }
}

} // namespace denigma
