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

#include "musicxml.h"

#include <stdexcept>
#include <string>

#include "denigma/classify/barlines.h"

#include "mx/api/BarlineData.h"
#include "mx/api/ClefData.h"
#include "mx/api/DurationData.h"
#include "mx/api/PartGroupData.h"
#include "mx/api/PitchData.h"

#define BEGIN_ENUM_CONVERSION(FromEnum, ToEnum) \
template<> \
ToEnum enumConvert(FromEnum value) { \
    switch(value) {

#define END_ENUM_CONVERSION \
    default: \
        throw std::invalid_argument("Unable to convert enum value: " + std::to_string(int(value))); \
    } \
}

using namespace musx::dom;
using BarlineType = musx::dom::others::Measure::BarlineType;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

// Primary template definition (if needed)
template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum)
{
    static_assert(sizeof(FromEnum) == 0, "No specialization exists for this conversion");
    return {};
}

BEGIN_ENUM_CONVERSION(BarlineType, mx::api::BarlineType)
    case BarlineType::None: return mx::api::BarlineType::none;
    case BarlineType::OptionsDefault: return mx::api::BarlineType::unspecified;
    case BarlineType::Normal: return mx::api::BarlineType::normal;
    case BarlineType::Double: return mx::api::BarlineType::lightLight;
    case BarlineType::Final: return mx::api::BarlineType::lightHeavy;
    case BarlineType::Solid: return mx::api::BarlineType::heavy;
    case BarlineType::Dashed: return mx::api::BarlineType::dashed;
    case BarlineType::Tick: return mx::api::BarlineType::tick;
    case BarlineType::Custom: return mx::api::BarlineType::unsupported;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::BarlineType, mx::api::BarlineType)
    case classify::BarlineType::Unsupported: return mx::api::BarlineType::unsupported;
    case classify::BarlineType::NoBarline: return mx::api::BarlineType::none;
    case classify::BarlineType::Regular: return mx::api::BarlineType::normal;
    case classify::BarlineType::Double: return mx::api::BarlineType::lightLight;
    case classify::BarlineType::Final: return mx::api::BarlineType::lightHeavy;
    case classify::BarlineType::Heavy: return mx::api::BarlineType::heavy;
    case classify::BarlineType::Dashed: return mx::api::BarlineType::dashed;
    case classify::BarlineType::Tick: return mx::api::BarlineType::tick;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(details::Bracket::BracketStyle, mx::api::BracketType)
    case details::Bracket::BracketStyle::None: return mx::api::BracketType::none;
    case details::Bracket::BracketStyle::ThickLine: return mx::api::BracketType::line;
    case details::Bracket::BracketStyle::BracketStraightHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::PianoBrace: return mx::api::BracketType::brace;
    case details::Bracket::BracketStyle::BracketCurvedHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::DeskBracket: return mx::api::BracketType::square;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::ClefType, mx::api::ClefSymbol)
    case music_theory::ClefType::Unknown: return mx::api::ClefSymbol::g;
    case music_theory::ClefType::G: return mx::api::ClefSymbol::g;
    case music_theory::ClefType::F: return mx::api::ClefSymbol::f;
    case music_theory::ClefType::C: return mx::api::ClefSymbol::c;
    case music_theory::ClefType::Percussion1: return mx::api::ClefSymbol::percussion;
    case music_theory::ClefType::Percussion2: return mx::api::ClefSymbol::percussion;
    case music_theory::ClefType::Tab: return mx::api::ClefSymbol::tab;
    case music_theory::ClefType::TabSerif: return mx::api::ClefSymbol::tab;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(MusicXmlPitchContext, KeySignature::KeyContext)
    case MusicXmlPitchContext::Concert: return KeySignature::KeyContext::Concert;
    case MusicXmlPitchContext::Written: return KeySignature::KeyContext::Written;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::NoteName, mx::api::Step)
    case music_theory::NoteName::A: return mx::api::Step::a;
    case music_theory::NoteName::B: return mx::api::Step::b;
    case music_theory::NoteName::C: return mx::api::Step::c;
    case music_theory::NoteName::D: return mx::api::Step::d;
    case music_theory::NoteName::E: return mx::api::Step::e;
    case music_theory::NoteName::F: return mx::api::Step::f;
    case music_theory::NoteName::G: return mx::api::Step::g;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(NoteType, mx::api::DurationName)
    case NoteType::Maxima: return mx::api::DurationName::maxima;
    case NoteType::Longa: return mx::api::DurationName::longa;
    case NoteType::Breve: return mx::api::DurationName::breve;
    case NoteType::Whole: return mx::api::DurationName::whole;
    case NoteType::Half: return mx::api::DurationName::half;
    case NoteType::Quarter: return mx::api::DurationName::quarter;
    case NoteType::Eighth: return mx::api::DurationName::eighth;
    case NoteType::Note16th: return mx::api::DurationName::dur16th;
    case NoteType::Note32nd: return mx::api::DurationName::dur32nd;
    case NoteType::Note64th: return mx::api::DurationName::dur64th;
    case NoteType::Note128th: return mx::api::DurationName::dur128th;
    case NoteType::Note256th: return mx::api::DurationName::dur256th;
    case NoteType::Note512th: return mx::api::DurationName::dur512th;
    case NoteType::Note1024th: return mx::api::DurationName::dur1024th;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(details::StaffGroup::DrawBarlineStyle, mx::api::GroupBarline)
    case details::StaffGroup::DrawBarlineStyle::OnlyOnStaves: return mx::api::GroupBarline::no;
    case details::StaffGroup::DrawBarlineStyle::ThroughStaves: return mx::api::GroupBarline::yes;
    case details::StaffGroup::DrawBarlineStyle::Mensurstriche: return mx::api::GroupBarline::mensurstrich;
END_ENUM_CONVERSION

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
