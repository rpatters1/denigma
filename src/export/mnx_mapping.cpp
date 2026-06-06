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
#include "mnx.h"

namespace denigma {
namespace mnxexp {

mnx::NoteValue::Required mnxNoteValueFromEdu(Edu duration)
{
    auto [base, dots] = calcDurationInfoFromEdu(duration);
    return mnx::NoteValue::make(enumConvert<mnx::NoteValueBase>(base), dots);
}

mnx::NoteValueQuantity::Required mnxNoteValueQuantityFromFraction(const MnxMusxMappingPtr& context, musx::util::Fraction duration)
{
    if (duration <= 0 || (duration.denominator() & (duration.denominator() - 1)) != 0) {
        auto newValue = musx::util::Fraction(duration.calcEduDuration(), Edu(musx::dom::NoteType::Whole));
        context->logMessage(LogMsg() << "Value " << duration << " cannot be exactly converted to a note value quantity. Using closest approximation. ("
            << newValue << ")", LogSeverity::Warning);
        duration = newValue;
    }

    return mnx::NoteValueQuantity::make(
        static_cast<unsigned>(duration.numerator()),
        mnxNoteValueFromEdu(musx::util::Fraction(1, duration.denominator()).calcEduDuration()));
}

musx::util::Fraction fractionFromMnxFraction(const mnx::FractionValue& mnxFraction)
{
    return musx::util::Fraction(mnxFraction.numerator(), mnxFraction.denominator());
}

mnx::FractionValue mnxFractionFromFraction(const musx::util::Fraction& fraction)
{
    return mnx::FractionValue(fraction.numerator(), fraction.denominator());
}

mnx::FractionValue mnxFractionFromEdu(Edu eduValue)
{
    return mnxFractionFromFraction(musx::util::Fraction::fromEdu(eduValue));
}

mnx::FractionValue mnxFractionFromSmartShapeEndPoint(const MusxInstance<smartshape::EndPoint>& endPoint)
{
    // findExact true needed for ottavas. revisit as we add other items.
    if (auto entryInfo = endPoint->calcAssociatedEntry(/*findExact*/ true)) {
        return mnxFractionFromFraction(entryInfo->elapsedDuration);
    }
    return mnxFractionFromFraction(endPoint->calcPosition()); 
}

int mnxStaffPosition(const MusxInstance<others::Staff>& staff, int musxStaffPosition)
{
    return musxStaffPosition - staff->calcMiddleStaffPosition();
}

mnx::LyricLineType mnxLineTypeFromLyric(const MusxInstance<LyricsSyllableInfo>& syl)
{
    if (syl->hasHyphenBefore && syl->hasHyphenAfter) {
        return mnx::LyricLineType::Middle;
    } else if (syl->hasHyphenBefore && !syl->hasHyphenAfter) {
        return mnx::LyricLineType::End;
    } else if (!syl->hasHyphenBefore && syl->hasHyphenAfter) {
        return mnx::LyricLineType::Start;
    }
    return mnx::LyricLineType::Whole;
}

} // namespace mnxexp
} // namespace denigma
