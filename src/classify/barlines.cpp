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
#include "denigma/classify/barlines.h"

namespace denigma::classify {

namespace {

using MusxBarlineType = musx::dom::others::Measure::BarlineType;

static bool classifyIsShortBarline(const musx::dom::MusxInstance<musx::dom::others::Staff>& staff)
{
    if (!staff) {
        return false;
    }

    constexpr musx::dom::Evpu minExtension = 12;
    constexpr musx::dom::Evpu maxExtension = 36;
    const auto [topFromCenter, bottomFromCenter] = staff->calcBarlineOffsetsFromCenter();
    return topFromCenter == -bottomFromCenter
        && topFromCenter >= minExtension
        && topFromCenter <= maxExtension;
}

static BarlineType classifyMeasureBarlineType(MusxBarlineType type)
{
    switch (type) {
    case MusxBarlineType::None:
        return BarlineType::NoBarline;
    case MusxBarlineType::Normal:
        return BarlineType::Regular;
    case MusxBarlineType::Double:
        return BarlineType::Double;
    case MusxBarlineType::Final:
        return BarlineType::Final;
    case MusxBarlineType::Solid:
        return BarlineType::Heavy;
    case MusxBarlineType::Dashed:
        return BarlineType::Dashed;
    case MusxBarlineType::Tick:
        return BarlineType::Tick;
    case MusxBarlineType::OptionsDefault:
    case MusxBarlineType::Custom:
        return BarlineType::Unsupported;
    }

    return BarlineType::Unsupported;
}

} // namespace

BarlineClassification classifyBarline(
    const musx::dom::MusxInstance<musx::dom::others::Staff>& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& measure,
    bool isFinalMeasure,
    const musx::dom::MusxInstance<musx::dom::options::BarlineOptions>& barlineOptions)
{
    if (!measure) {
        return {};
    }

    if ((barlineOptions && !barlineOptions->drawBarlines) || (staff && staff->hideBarlines)) {
        return { BarlineType::NoBarline, false };
    }

    const bool isShort = classifyIsShortBarline(staff);
    const auto type = measure->barlineType;
    if (type == MusxBarlineType::Normal) {
        if (isFinalMeasure && barlineOptions && !barlineOptions->drawFinalBarlineOnLastMeas) {
            return { BarlineType::Regular, isShort };
        }

        if (!isFinalMeasure && barlineOptions && barlineOptions->drawDoubleBarlineBeforeKeyChanges) {
            if (const auto& nextMeasure = measure->getDocument()->getOthers()->get<musx::dom::others::Measure>(
                    musx::dom::SCORE_PARTID, static_cast<musx::dom::Cmper>(measure->getCmper() + 1))) {
                if (!measure->createKeySignature()->isSame(*nextMeasure->createKeySignature().get())) {
                    return { BarlineType::Double, isShort };
                }
            }
        }
    }

    return { classifyMeasureBarlineType(type), isShort };
}

} // namespace denigma::classify
