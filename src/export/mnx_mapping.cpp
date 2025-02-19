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
#include <unordered_map>

#include "mnx.h"
#include "mnx_mapping.h"

namespace denigma {
namespace mnxexp {
    
JumpType convertTextToJump(const std::string& text, FontType fontType)
{
    static const std::unordered_map<std::string, JumpType> utf8Lookup = {
        {"segno", JumpType::Segno}, {"§", JumpType::Segno}, {"𝄋", JumpType::Segno},
        {"d.s.", JumpType::DalSegno}, {"dal segno", JumpType::DalSegno},
        {"d.c.", JumpType::DaCapo}, {"da capo", JumpType::DaCapo},
        {"coda", JumpType::Coda}, {"𝄌", JumpType::Coda}, {"fine", JumpType::Fine}
    };
    
    static const std::unordered_map<std::string, JumpType> maestroLookup = {
        {"%", JumpType::Segno},
        {"U+00DE", JumpType::Coda},
        {"U+009F", JumpType::Segno}
    };
    
    static const std::unordered_map<std::string, JumpType> smuflLookup = {
        {"U+E047", JumpType::Segno}, {"U+E04A", JumpType::Segno}, {"U+E04B", JumpType::Segno}, {"U+F404", JumpType::Segno},
        {"U+E045", JumpType::DalSegno}, {"U+E046", JumpType::DaCapo},
        {"U+E048", JumpType::Coda}, {"U+E049", JumpType::Coda}, {"U+F405", JumpType::Coda}
    };

    std::string lowerText = utils::toLowerCase(text); // Convert input text to lowercase    

    auto checkTable = [lowerText](const std::unordered_map<std::string, JumpType>& table) -> std::optional<JumpType> {
        for (const auto& [key, value] : table) {
            if (lowerText.find(utils::toLowerCase(key)) != std::string::npos) { // Check if key is in text
                return value;
            }
        }
        return std::nullopt;
    };
    
    std::optional<JumpType> result;
    switch (fontType) {
        case FontType::SMuFL:
            if ((result = checkTable(smuflLookup)) != std::nullopt) {
                return result.value();
            }
            // fall thru on purpose
        default:
        case FontType::Unicode:
            result = checkTable(utf8Lookup);
            if (!result) {
                if (lowerText == "ds" || lowerText.rfind("ds ", 0) == 0) {
                    return JumpType::DalSegno;
                }
                if (lowerText == "dc" || lowerText.rfind("dc ", 0) == 0) {
                    return JumpType::DaCapo;
                }
            }
            break;
        case FontType::LegacyMusic:
            result = checkTable(maestroLookup);
            break;
    }

    return result.value_or(JumpType::None);
}

} // namespace mnxexp
} // namespace denigma
