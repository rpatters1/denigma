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

namespace ezgz {
#include "ezgz_license.xxd"
}

namespace miniz_cpp {
#include "miniz_cpp_license.xxd"
}

namespace musx {
#include "musx_license.xxd"
}

namespace pugixml {
#include "pugixml_license.xxd"
}

static const std::basic_string_view<unsigned char> denigmaLicense(denigma::LICENSE, denigma::LICENSE_len);
static const std::basic_string_view<unsigned char> ezgzLicense(ezgz::LICENSE, ezgz::LICENSE_len);
static const std::basic_string_view<unsigned char> minizLicense(miniz_cpp::LICENSE_md, miniz_cpp::LICENSE_md_len);
static const std::basic_string_view<unsigned char> musxLicense(musx::LICENSE, musx::LICENSE_len);
static const std::basic_string_view<unsigned char> pugixmlLicense(pugixml::LICENSE_md, pugixml::LICENSE_md_len);

static const std::array<std::pair<std::string_view, std::basic_string_view<unsigned char>>, 5> licenses = {{
    { "denigma", denigmaLicense },
    { "musx object model", musxLicense },
    { "EzGz", ezgzLicense },
    { "miniz-cpp", minizLicense },
    { "pugixml", pugixmlLicense }
}};

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