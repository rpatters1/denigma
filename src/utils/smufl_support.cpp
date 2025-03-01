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
#include <unordered_map>
#include <istream>

#include "smufl_support.h"
#include "smufl_glyphnames.xxd"

#include "nlohmann/json.hpp"
#include "ziputils.h"

namespace utils {

static char32_t ucodeToCodePoint(const std::string& ucode)
{
    return std::stoul(ucode.substr(2), nullptr, 16);
}

static std::unordered_map<char32_t, std::string> createGlyphMap(const nlohmann::json& json)
{
    std::unordered_map<char32_t, std::string> result;
    for (auto [glyphName, glyphData] : json.items()) {
        if (glyphData.contains("codepoint")) {
            result.emplace(ucodeToCodePoint(glyphData["codepoint"]), glyphName);
        }
    }
    return result;
}

std::optional<std::string> smuflGlyphName(char32_t codePoint, const denigma::DenigmaContext& denigmaContext)
{
    static const auto glyphMap = [&]() -> std::unordered_map<char32_t, std::string> {
        std::vector<unsigned char> zipFile(smufl_glyphnames_zip, smufl_glyphnames_zip + smufl_glyphnames_zip_len);
        nlohmann::json json = nlohmann::json::parse(readFile(zipFile, "glyphnames.json", denigmaContext));
        return createGlyphMap(json);
    }();

    const auto it = glyphMap.find(codePoint);
    if (it != glyphMap.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::string> smuflGlyphNameForFont(const std::filesystem::path& fontMetadataPath, char32_t codePoint, const denigma::DenigmaContext& denigmaContext)
{
    static std::unordered_map<std::string, const std::unordered_map<char32_t, std::string>> fontMaps;

    auto result = smuflGlyphName(codePoint, denigmaContext);
    if (!result) {
        auto it = fontMaps.find(fontMetadataPath.u8string());
        if (it == fontMaps.end()) {
            auto [newIt, _] = fontMaps.emplace(fontMetadataPath.u8string(), [&]() {
                std::ifstream jsonFile;
                jsonFile.exceptions(std::ios::failbit | std::ios::badbit);
                jsonFile.open(fontMetadataPath);
                if (!jsonFile.is_open()) {
                    throw std::runtime_error("Unable to open JSON file: " + fontMetadataPath.u8string());
                }
                nlohmann::json json;
                jsonFile >> json;
                if (json.contains("optionalGlyphs")) {
                    return createGlyphMap(json["optionalGlyphs"]);
                }
                return std::unordered_map<char32_t, std::string>();
            }());
            it = newIt;
        }
        if (it != fontMaps.end()) {
            auto mapIt = it->second.find(codePoint);
            if (mapIt != it->second.end()) {
                result = mapIt->second;
            }
        }
    }
    return result;
}

std::optional<std::string> smuflGlyphNameForFont(const std::shared_ptr<musx::dom::FontInfo>& fontInfo, const std::string& text, const denigma::DenigmaContext& denigmaContext)
{
    if (auto metaDataPath = fontInfo->calcSMuFLMetaDataPath()) {
        if (auto codePoint = utf8ToCodepoint(text)) {
            return smuflGlyphNameForFont(metaDataPath.value(), codePoint.value(), denigmaContext);
        }
    }
    return std::nullopt;
}

} // namespace utils