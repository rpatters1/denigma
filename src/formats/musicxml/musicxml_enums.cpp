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

#include "mx/api/PartGroupData.h"

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

BEGIN_ENUM_CONVERSION(details::Bracket::BracketStyle, mx::api::BracketType)
    case details::Bracket::BracketStyle::None: return mx::api::BracketType::none;
    case details::Bracket::BracketStyle::ThickLine: return mx::api::BracketType::line;
    case details::Bracket::BracketStyle::BracketStraightHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::PianoBrace: return mx::api::BracketType::brace;
    case details::Bracket::BracketStyle::BracketCurvedHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::DeskBracket: return mx::api::BracketType::square;
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
