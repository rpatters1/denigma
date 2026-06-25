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
#pragma once

#include <filesystem>
#include <optional>

#include "core/denigma.h"
#include "musx/musx.h"
#include "mnxdom.h"

 //placeholder function

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

struct MnxMusxMapping;

mnxdom::NoteValue::Required mnxNoteValueFromEdu(Edu duration);
mnxdom::NoteValueQuantity::Required mnxNoteValueQuantityFromFraction(const std::shared_ptr<MnxMusxMapping>& context, musx::util::Fraction duration);
mnxdom::LyricLineType mnxLineTypeFromLyric(const MusxInstance<LyricsSyllableInfo>& syl);

musx::util::Fraction fractionFromMnxFraction(const mnxdom::FractionValue& mnxFraction);
mnxdom::FractionValue mnxFractionFromFraction(const musx::util::Fraction& fraction);
mnxdom::FractionValue mnxFractionFromEdu(Edu eduValue);
mnxdom::FractionValue mnxFractionFromSmartShapeEndPoint(const MusxInstance<smartshape::EndPoint>& smartShape);

int mnxStaffPosition(const MusxInstance<others::Staff>& staff, int musxStaffPosition);

mnxdom::MultiStaffOrientation mnxMultiStaffOrientFromVerticalPlacement(const std::optional<int>& mnxStaffNumber, VerticalPlacement placement);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
