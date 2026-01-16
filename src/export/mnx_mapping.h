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

#include "denigma.h"
#include "musx/musx.h"
#include "mnxdom.h"

#include "mnx.h"

 //placeholder function

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

struct MnxMusxMapping;

enum class JumpType
{
    None,
    Segno,
    DalSegno,
    DsAlFine,
    DaCapo,
    DCAlFine,
    Coda,
    Fine
};

enum class EventMarkingType
{
    Accent,
    Breath,
    SoftAccent,
    Spiccato,
    Staccatissimo,
    Staccato,
    Stress,
    StrongAccent,
    Tenuto,
    Tremolo,
    Unstress
};

JumpType convertTextToJump(const std::string& text, const std::optional<std::string>& glyphName);
std::vector<EventMarkingType> calcMarkingType(const MusxInstance<others::ArticulationDef>& artic,
    std::optional<int>& numMarks,
    std::optional<mnx::BreathMarkSymbol>& breathMark);

mnx::NoteValue::Required mnxNoteValueFromEdu(Edu duration);
mnx::NoteValueQuantity::Required mnxNoteValueQuantityFromFraction(const std::shared_ptr<MnxMusxMapping>& context, musx::util::Fraction duration);
mnx::LyricLineType mnxLineTypeFromLyric(const MusxInstance<LyricsSyllableInfo>& syl);
std::optional<std::tuple<mnx::ClefSign, mnx::OttavaAmountOrZero, bool>> mnxClefInfoFromClefDef(
    const MusxInstance<options::ClefOptions::ClefDef>& clefDef,
    const MusxInstance<others::Staff>& staff, std::optional<std::string_view> glyphName);

musx::util::Fraction fractionFromMnxFraction(const mnx::FractionValue& mnxFraction);
mnx::FractionValue mnxFractionFromFraction(const musx::util::Fraction& fraction);
mnx::FractionValue mnxFractionFromEdu(Edu eduValue);
mnx::FractionValue mnxFractionFromSmartShapeEndPoint(const MusxInstance<musx::dom::smartshape::EndPoint>& smartShape);

int mnxStaffPosition(const MusxInstance<others::Staff>& staff, int musxStaffPosition);

} // namespace mnxexp
} // namespace denigma
