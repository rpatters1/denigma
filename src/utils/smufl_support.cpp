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
#include <array>
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

namespace {

struct SmuflFontMetadata
{
    std::unordered_map<char32_t, std::string> optionalGlyphNames;
    std::unordered_map<std::string, EvpuFloat> glyphAdvanceWidths;
    std::unordered_map<std::string, std::array<EvpuFloat, 4>> glyphBBoxes;
};

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

static SmuflFontMetadata parseSmuflMetadata(std::istream& jsonFile)
{
    nlohmann::json json;
    jsonFile >> json;

    SmuflFontMetadata metadata;
    if (json.contains("optionalGlyphs")) {
        metadata.optionalGlyphNames = createGlyphMap(json["optionalGlyphs"]);
    }
    if (json.contains("glyphAdvanceWidths") && json["glyphAdvanceWidths"].is_object()) {
        for (const auto& item : json["glyphAdvanceWidths"].items()) {
            if (item.value().is_number()) {
                metadata.glyphAdvanceWidths.emplace(item.key(), item.value().get<EvpuFloat>());
            }
        }
    }
    if (json.contains("glyphBBoxes") && json["glyphBBoxes"].is_object()) {
        for (const auto& item : json["glyphBBoxes"].items()) {
            const auto& bbox = item.value();
            if (!bbox.contains("bBoxSW") || !bbox.contains("bBoxNE")) {
                continue;
            }
            const auto& sw = bbox["bBoxSW"];
            const auto& ne = bbox["bBoxNE"];
            if (!sw.is_array() || !ne.is_array() || sw.size() < 2 || ne.size() < 2) {
                continue;
            }
            metadata.glyphBBoxes.emplace(item.key(),
                                         std::array<EvpuFloat, 4>{ sw[0].get<EvpuFloat>(),
                                                                   sw[1].get<EvpuFloat>(),
                                                                   ne[0].get<EvpuFloat>(),
                                                                   ne[1].get<EvpuFloat>() });
        }
    }
    return metadata;
}

static const SmuflFontMetadata* metadataForFont(const std::filesystem::path& fontMetadataPath)
{
    static std::unordered_map<std::string, SmuflFontMetadata> metadataCache;
    const std::string cacheKey = fontMetadataPath.u8string();
    auto it = metadataCache.find(cacheKey);
    if (it != metadataCache.end()) {
        return &it->second;
    }

    std::ifstream jsonFile;
    jsonFile.exceptions(std::ios::failbit | std::ios::badbit);
    jsonFile.open(fontMetadataPath);
    if (!jsonFile.is_open()) {
        throw std::runtime_error("Unable to open JSON file: " + fontMetadataPath.u8string());
    }
    auto [newIt, _] = metadataCache.emplace(cacheKey, parseSmuflMetadata(jsonFile));
    return &newIt->second;
}

} // namespace

static std::optional<std::string> smuflGlyphNameForFont(const std::filesystem::path& fontMetadataPath, char32_t codepoint)
{
    if (auto glyphName = smufl_mapping::getGlyphName(codepoint)) {
        return std::string(*glyphName);
    }

    if (const auto* metadata = metadataForFont(fontMetadataPath)) {
        auto it = metadata->optionalGlyphNames.find(codepoint);
        if (it != metadata->optionalGlyphNames.end()) {
            return it->second;
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
        if (const auto* metadata = metadataForFont(metaDataPath.value())) {
            auto bboxIt = metadata->glyphBBoxes.find(glyphName);
            if (bboxIt != metadata->glyphBBoxes.end()) {
                const auto& bbox = bboxIt->second;
                return (bbox[2] - bbox[0]) * EVPU_PER_SPACE;
            }
            auto advIt = metadata->glyphAdvanceWidths.find(glyphName);
            if (advIt != metadata->glyphAdvanceWidths.end()) {
                return advIt->second * EVPU_PER_SPACE;
            }
        }
    }
    return std::nullopt;
}

std::optional<SmuflGlyphMetricsEvpu> smuflGlyphMetricsForFont(const FontInfo& fontInfo, char32_t codepoint)
{
    auto metadataPath = fontInfo.calcSMuFLMetaDataPath();
    if (!metadataPath) {
        return std::nullopt;
    }

    auto glyphName = smuflGlyphNameForFont(metadataPath.value(), codepoint);
    if (!glyphName) {
        return std::nullopt;
    }

    const auto* metadata = metadataForFont(metadataPath.value());
    if (!metadata) {
        return std::nullopt;
    }
    auto bboxIt = metadata->glyphBBoxes.find(glyphName.value());
    if (bboxIt == metadata->glyphBBoxes.end()) {
        return std::nullopt;
    }

    const EvpuFloat pointSize = fontInfo.fontSize > 0 ? static_cast<EvpuFloat>(fontInfo.fontSize) : 12.0;
    const EvpuFloat evpuPerSpaceAtSize = pointSize * EVPU_PER_POINT / 4.0;
    const auto& bbox = bboxIt->second;

    SmuflGlyphMetricsEvpu result;
    auto advanceIt = metadata->glyphAdvanceWidths.find(glyphName.value());
    const EvpuFloat advanceInSpaces = (advanceIt != metadata->glyphAdvanceWidths.end())
                                      ? advanceIt->second
                                      : (bbox[2] - bbox[0]);
    result.advance = advanceInSpaces * evpuPerSpaceAtSize;
    result.top = bbox[3] * evpuPerSpaceAtSize;
    result.bottom = bbox[1] * evpuPerSpaceAtSize;
    return result;
}

} // namespace utils
