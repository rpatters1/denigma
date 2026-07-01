/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "musicxml.h"

#include <cmath>
#include <limits>
#include <string_view>

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

std::string_view musicXmlFontFamilyFallbackName(MusicXmlFontFamilyFallback fallback)
{
    switch (fallback) {
    case MusicXmlFontFamilyFallback::None:
        return {};
    case MusicXmlFontFamilyFallback::Music:
        return "music";
    case MusicXmlFontFamilyFallback::Engraved:
        return "engraved";
    case MusicXmlFontFamilyFallback::Handwritten:
        return "handwritten";
    case MusicXmlFontFamilyFallback::Text:
        return "text";
    case MusicXmlFontFamilyFallback::Serif:
        return "serif";
    case MusicXmlFontFamilyFallback::SansSerif:
        return "sans-serif";
    case MusicXmlFontFamilyFallback::Cursive:
        return "cursive";
    case MusicXmlFontFamilyFallback::Fantasy:
        return "fantasy";
    case MusicXmlFontFamilyFallback::Monospace:
        return "monospace";
    }
    throw std::invalid_argument("Unknown MusicXML font-family fallback.");
}

} // namespace

MusicXmlPitchContext pitchContextForPart(const MusicXmlMusxMapping& context, const std::string& partId)
{
    if (const auto it = context.partIdToPitchContext.find(partId); it != context.partIdToPitchContext.end()) {
        return it->second;
    }
    return MusicXmlPitchContext::Concert;
}

int MusicXmlTimingPlan::calcNearestMusicXmlDivisions(const musx::util::Fraction& wholeNoteFraction) const
{
    const auto result = wholeNoteFraction * 4 * divisions;
    if (result.denominator() == 1) {
        return result.numerator();
    }

    const auto rounded = std::llround(static_cast<double>(result.numerator()) / static_cast<double>(result.denominator()));
    ASSERT_IF(rounded < (std::numeric_limits<int>::min)() || rounded > (std::numeric_limits<int>::max)()) {
        throw std::overflow_error("MusicXML position is outside the supported integer range.");
    }
    return static_cast<int>(rounded);
}

void MusicXmlLayoutState::setStaffSize(
    mx::api::StaffData& staffData,
    musx::dom::StaffCmper staffId,
    const musx::util::Fraction& staffSize,
    const musx::util::Fraction& staffScaling)
{
    auto& current = staffLayout[staffId];
    if (current.staffSize != staffSize) {
        staffData.staffSize = staffSize.toDouble() * 100.0;
        current.staffSize = staffSize;
    }
    if (current.staffScaling != staffScaling) {
        staffData.staffScaling = staffScaling.toDouble() * 100.0;
        current.staffScaling = staffScaling;
    }
}

mx::api::FontData MusicXmlMusxMapping::musicXmlFontDataFromFontInfo(
    const musx::dom::FontInfo& fontInfo,
    MusicXmlFontFamilyFallback fallback) const
{
    mx::api::FontData result;
    const auto fontName = fontInfo.getName();
    if (!fontName.empty()) {
        result.fontFamily.emplace_back(fontName);
    }
    if (const auto fallbackName = musicXmlFontFamilyFallbackName(fallback); !fallbackName.empty()) {
        result.fontFamily.emplace_back(fallbackName);
    }

    if (!fontInfo.getSizeIsPercent() && fontInfo.fontSize > 0) {
        constexpr auto kUnscaledMmPerStaff = musx::dom::EVPU_PER_STANDARD_STAFF / musx::dom::EVPU_PER_MM;
        auto staffScaling = 1.0;
        if (!fontInfo.absolute) {
            const bool hasInitializedScaling = musicXmlScore && musicXmlScore->defaults.scalingMillimeters > 0.0;
            ASSERT_IF(!hasInitializedScaling) {
                throw std::logic_error("MusicXML font conversion requires initialized score scaling for non-absolute font sizes.");
            }
            staffScaling = musicXmlScore->defaults.scalingMillimeters / kUnscaledMmPerStaff;
        }
        result.sizeType = mx::api::FontSizeType::point;
        result.sizePoint = static_cast<double>(fontInfo.fontSize) * staffScaling;
    }

    // Finale font styles are explicit: unset bold/italic means normal, not unspecified.
    // Emit normal so MusicXML importers do not inherit style from surrounding context.
    result.style = fontInfo.italic ? mx::api::FontStyle::italic : mx::api::FontStyle::normal;
    result.weight = fontInfo.bold ? mx::api::FontWeight::bold : mx::api::FontWeight::normal;
    result.underline = fontInfo.underline ? 1 : 0;
    result.lineThrough = fontInfo.strikeout ? 1 : 0;
    return result;
}

double MusicXmlMusxMapping::musicXmlTenthsFromEvpu(double evpu, double backoutScaling) const
{
    return evpu * musicXmlScore->defaults.scalingTenths / musx::dom::EVPU_PER_STANDARD_STAFF / backoutScaling;
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
