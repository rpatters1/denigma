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

#define BEGIN_ENUM_CONVERSION(FromEnum, ToEnum) \
template<> \
ToEnum enumConvert(FromEnum value) { \
    switch(value) {

#define END_ENUM_CONVERSION \
    default: \
        throw std::invalid_argument("Unable to convert enum value: " + std::to_string(int(value))); \
    } \
}

namespace denigma {
namespace mnxexp {

// Primary template definition (if needed)
template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum) {
    static_assert(sizeof(FromEnum) == 0, "No specialization exists for this conversion");
    return {};
}

using BarlineType = musx::dom::others::Measure::BarlineType;
BEGIN_ENUM_CONVERSION(BarlineType, mnx::BarlineType)
    case BarlineType::None: return mnx::BarlineType::NoBarline;
    case BarlineType::Normal: return mnx::BarlineType::Regular;
    case BarlineType::Double: return mnx::BarlineType::Double;
    case BarlineType::Final: return mnx::BarlineType::Final;
    case BarlineType::Solid: return mnx::BarlineType::Heavy;
    case BarlineType::Dashed: return mnx::BarlineType::Dashed;
    case BarlineType::Tick: return mnx::BarlineType::Tick;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(musx::dom::NoteType, mnx::NoteValueBase)
    case NoteType::Note2048th: return mnx::NoteValueBase::Note2048th;
    case NoteType::Note1024th: return mnx::NoteValueBase::Note1024th;
    case NoteType::Note512th: return mnx::NoteValueBase::Note512th;
    case NoteType::Note128th: return mnx::NoteValueBase::Note128th;
    case NoteType::Note64th: return mnx::NoteValueBase::Note64th;
    case NoteType::Note32nd: return mnx::NoteValueBase::Note32nd;
    case NoteType::Note16th: return mnx::NoteValueBase::Note16th;
    case NoteType::Eighth: return mnx::NoteValueBase::Eighth;
    case NoteType::Quarter: return mnx::NoteValueBase::Quarter;
    case NoteType::Half: return mnx::NoteValueBase::Half;
    case NoteType::Whole: return mnx::NoteValueBase::Whole;
    case NoteType::Breve: return mnx::NoteValueBase::Breve;
    case NoteType::Longa: return mnx::NoteValueBase::Longa;
    case NoteType::Maxima: return mnx::NoteValueBase::Maxima;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(musx::dom::NoteType, mnx::TimeSignatureUnit)
    case NoteType::Note128th: return mnx::TimeSignatureUnit::Value128th;
    case NoteType::Note64th: return mnx::TimeSignatureUnit::Value64th;
    case NoteType::Note32nd: return mnx::TimeSignatureUnit::Value32nd;
    case NoteType::Note16th: return mnx::TimeSignatureUnit::Value16th;
    case NoteType::Eighth: return mnx::TimeSignatureUnit::Eighth;
    case NoteType::Quarter: return mnx::TimeSignatureUnit::Quarter;
    case NoteType::Half: return mnx::TimeSignatureUnit::Half;
    case NoteType::Whole: return mnx::TimeSignatureUnit::Whole;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(details::TupletDef::AutoBracketStyle, mnx::AutoYesNo)
    case details::TupletDef::AutoBracketStyle::Always: return mnx::AutoYesNo::Yes;
    case details::TupletDef::AutoBracketStyle::NeverBeamSide: return mnx::AutoYesNo::Yes; // currently there is no exact analog for this Finale option in Mnx.
    case details::TupletDef::AutoBracketStyle::UnbeamedOnly: return mnx::AutoYesNo::Auto;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::NoteName, mnx::NoteStep)
    case music_theory::NoteName::C: return mnx::NoteStep::C;
    case music_theory::NoteName::D: return mnx::NoteStep::D;
    case music_theory::NoteName::E: return mnx::NoteStep::E;
    case music_theory::NoteName::F: return mnx::NoteStep::F;
    case music_theory::NoteName::G: return mnx::NoteStep::G;
    case music_theory::NoteName::A: return mnx::NoteStep::A;
    case music_theory::NoteName::B: return mnx::NoteStep::B;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(others::SmartShape::ShapeType, mnx::OttavaAmount)
    case others::SmartShape::ShapeType::OctaveDown: return mnx::OttavaAmount::OctaveDown;
    case others::SmartShape::ShapeType::OctaveUp: return mnx::OttavaAmount::OctaveUp;
    case others::SmartShape::ShapeType::TwoOctaveDown: return mnx::OttavaAmount::TwoOctavesDown;
    case others::SmartShape::ShapeType::TwoOctaveUp: return mnx::OttavaAmount::TwoOctavesUp;
END_ENUM_CONVERSION

} // namespace mnxexp
} // namespace denigma
