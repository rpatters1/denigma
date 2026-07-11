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
#include "denigma/classify/barlines.h"
#include "denigma/classify/classifier_common.h"
#include "formats/enum_conversion_macros.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

DEFINE_ENUM_CONVERT_TEMPLATE

BEGIN_ENUM_CONVERSION(details::TupletDef::AutoBracketStyle, mnxdom::AutoYesNo)
    case details::TupletDef::AutoBracketStyle::Always: return mnxdom::AutoYesNo::Yes;
    case details::TupletDef::AutoBracketStyle::NeverBeamSide: return mnxdom::AutoYesNo::Yes; // currently there is no exact analog for this Finale option in Mnx.
    case details::TupletDef::AutoBracketStyle::UnbeamedOnly: return mnxdom::AutoYesNo::Auto;
END_ENUM_CONVERSION

using BarlineType = musx::dom::others::Measure::BarlineType;
BEGIN_ENUM_CONVERSION(BarlineType, mnxdom::BarlineType)
    case BarlineType::None: return mnxdom::BarlineType::NoBarline;
    case BarlineType::Normal: return mnxdom::BarlineType::Regular;
    case BarlineType::Double: return mnxdom::BarlineType::Double;
    case BarlineType::Final: return mnxdom::BarlineType::Final;
    case BarlineType::Solid: return mnxdom::BarlineType::Heavy;
    case BarlineType::Dashed: return mnxdom::BarlineType::Dashed;
    case BarlineType::Tick: return mnxdom::BarlineType::Tick;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::barline::Type, mnxdom::BarlineType)
    case classify::barline::Type::NoBarline: return mnxdom::BarlineType::NoBarline;
    case classify::barline::Type::Regular: return mnxdom::BarlineType::Regular;
    case classify::barline::Type::Double: return mnxdom::BarlineType::Double;
    case classify::barline::Type::Final: return mnxdom::BarlineType::Final;
    case classify::barline::Type::Heavy: return mnxdom::BarlineType::Heavy;
    case classify::barline::Type::Dashed: return mnxdom::BarlineType::Dashed;
    case classify::barline::Type::Tick: return mnxdom::BarlineType::Tick;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::GlyphStyle::Placement, mnxdom::MarkingUpDownAuto)
    case classify::GlyphStyle::Placement::Automatic: return mnxdom::MarkingUpDownAuto::Auto;
    case classify::GlyphStyle::Placement::Above: return mnxdom::MarkingUpDownAuto::Up;
    case classify::GlyphStyle::Placement::Below: return mnxdom::MarkingUpDownAuto::Down;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::NoteName, mnxdom::NoteStep)
    case music_theory::NoteName::C: return mnxdom::NoteStep::C;
    case music_theory::NoteName::D: return mnxdom::NoteStep::D;
    case music_theory::NoteName::E: return mnxdom::NoteStep::E;
    case music_theory::NoteName::F: return mnxdom::NoteStep::F;
    case music_theory::NoteName::G: return mnxdom::NoteStep::G;
    case music_theory::NoteName::A: return mnxdom::NoteStep::A;
    case music_theory::NoteName::B: return mnxdom::NoteStep::B;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(musx::dom::NoteType, mnxdom::NoteValueBase)
    case NoteType::Note2048th: return mnxdom::NoteValueBase::Note2048th;
    case NoteType::Note1024th: return mnxdom::NoteValueBase::Note1024th;
    case NoteType::Note512th: return mnxdom::NoteValueBase::Note512th;
    case NoteType::Note128th: return mnxdom::NoteValueBase::Note128th;
    case NoteType::Note64th: return mnxdom::NoteValueBase::Note64th;
    case NoteType::Note32nd: return mnxdom::NoteValueBase::Note32nd;
    case NoteType::Note16th: return mnxdom::NoteValueBase::Note16th;
    case NoteType::Eighth: return mnxdom::NoteValueBase::Eighth;
    case NoteType::Quarter: return mnxdom::NoteValueBase::Quarter;
    case NoteType::Half: return mnxdom::NoteValueBase::Half;
    case NoteType::Whole: return mnxdom::NoteValueBase::Whole;
    case NoteType::Breve: return mnxdom::NoteValueBase::Breve;
    case NoteType::Longa: return mnxdom::NoteValueBase::Longa;
    case NoteType::Maxima: return mnxdom::NoteValueBase::Maxima;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(musx::dom::NoteType, mnxdom::TimeSignatureUnit)
    case NoteType::Note128th: return mnxdom::TimeSignatureUnit::Value128th;
    case NoteType::Note64th: return mnxdom::TimeSignatureUnit::Value64th;
    case NoteType::Note32nd: return mnxdom::TimeSignatureUnit::Value32nd;
    case NoteType::Note16th: return mnxdom::TimeSignatureUnit::Value16th;
    case NoteType::Eighth: return mnxdom::TimeSignatureUnit::Eighth;
    case NoteType::Quarter: return mnxdom::TimeSignatureUnit::Quarter;
    case NoteType::Half: return mnxdom::TimeSignatureUnit::Half;
    case NoteType::Whole: return mnxdom::TimeSignatureUnit::Whole;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(others::SmartShape::ShapeType, mnxdom::OttavaAmount)
    case others::SmartShape::ShapeType::OctaveDown: return mnxdom::OttavaAmount::OctaveDown;
    case others::SmartShape::ShapeType::OctaveUp: return mnxdom::OttavaAmount::OctaveUp;
    case others::SmartShape::ShapeType::TwoOctaveDown: return mnxdom::OttavaAmount::TwoOctavesDown;
    case others::SmartShape::ShapeType::TwoOctaveUp: return mnxdom::OttavaAmount::TwoOctavesUp;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(VerticalPlacement, mnxdom::Orientation)
    case VerticalPlacement::NotApplicable: return mnxdom::Orientation::Auto;
    case VerticalPlacement::Float: return mnxdom::Orientation::Auto;
    case VerticalPlacement::Above: return mnxdom::Orientation::Above;
    case VerticalPlacement::Below: return mnxdom::Orientation::Below;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(VerticalPlacement, mnxdom::MultiStaffOrientation)
    case VerticalPlacement::NotApplicable: return mnxdom::MultiStaffOrientation::Auto;
    case VerticalPlacement::Float: return mnxdom::MultiStaffOrientation::Auto;
    case VerticalPlacement::Above: return mnxdom::MultiStaffOrientation::Above;
    case VerticalPlacement::Below: return mnxdom::MultiStaffOrientation::Below;
END_ENUM_CONVERSION

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
