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

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

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

double MusicXmlMusxMapping::musicXmlTenthsFromEvpu(double evpu, double backoutScaling) const
{
    return evpu * musicXmlScore->defaults.scalingTenths / musx::dom::EVPU_PER_STANDARD_STAFF / backoutScaling;
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
