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
#include <istream>
#include <optional>
#include <string>
#include <unordered_map>

#include "smufl_support.h"

#include "nlohmann/json.hpp"
#include "ziputils.h"

#include "musx/musx.h"
#include "smufl_mapping.h"

using namespace musx::dom;

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

static std::optional<std::string> smuflGlyphNameForFont(const std::filesystem::path& fontMetadataPath, char32_t codepoint)
{
    static std::unordered_map<std::string, const std::unordered_map<char32_t, std::string>> fontMaps;

    if (auto glyphName = smufl_mapping::getGlyphName(codepoint)) {
        return std::string(*glyphName);
    }
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
        auto mapIt = it->second.find(codepoint);
        if (mapIt != it->second.end()) {
            return mapIt->second;
        }
    }
    return std::nullopt;
}
std::optional<std::string> smuflGlyphNameForFont(const MusxInstance<FontInfo>& fontInfo, char32_t codepoint)
{
    if (auto metaDataPath = fontInfo->calcSMuFLMetaDataPath()) {
        if (auto glyphName = smufl_mapping::getGlyphName(codepoint, smufl_mapping::SmuflGlyphSource::Finale)) {
            return std::string(*glyphName);
        }
        return smuflGlyphNameForFont(metaDataPath.value(), codepoint);
    } else if (auto legacyInfo = smufl_mapping::getLegacyGlyphInfo(fontInfo->getName(), codepoint)) {
        return std::string(legacyInfo->name);
    }
    return std::nullopt;
}

std::optional<std::string> smuflGlyphNameForFont(const MusxInstance<FontInfo>& fontInfo, const std::string& text)
{
    if (auto codepoint = utf8ToCodepoint(text)) {
        return smuflGlyphNameForFont(fontInfo, codepoint.value());
    }
    return std::nullopt;
}

std::optional<EvpuFloat> smuflGlyphWidthForFont(const std::string& fontName, const std::string& glyphName)
{
    if (auto metaDataPath = FontInfo::calcSMuFLMetaDataPath(fontName)) {
        std::ifstream jsonFile;
        jsonFile.exceptions(std::ios::failbit | std::ios::badbit);
        jsonFile.open(metaDataPath.value());
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Unable to open JSON file: " + metaDataPath.value().u8string());
        }
        nlohmann::json json;
        jsonFile >> json;
        if (json.contains("glyphBBoxes") && json["glyphBBoxes"].contains(glyphName)) {
            nlohmann::json glyphBox = json["glyphBBoxes"][glyphName];
            return (glyphBox["bBoxNE"][0].get<EvpuFloat>() - glyphBox["bBoxSW"][0].get<EvpuFloat>()) * EVPU_PER_SPACE;
        } else if (json.contains("glyphAdvanceWidths") && json["glyphAdvanceWidths"].contains(glyphName)) {
            return json["glyphAdvanceWidths"][glyphName].get<EvpuFloat>() * EVPU_PER_SPACE;
        }
    }
    return std::nullopt;
}

} // namespace utils
