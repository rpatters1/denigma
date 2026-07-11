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
#pragma once

#include <optional>
#include <string_view>

#include "musx/musx.h"

namespace denigma::classify {

namespace keyboardpedal {

/// @brief Keyboard-pedal markings recognized in Finale text and custom lines.
enum class Type
{
    PedalOne,       ///< Sustain pedal, normally the rightmost pedal.
    PedalTwo,       ///< Sostenuto pedal, normally the middle pedal.
    PedalThree,     ///< Una corda or soft pedal, normally the leftmost pedal.
    PedalUp,        ///< Release the currently engaged keyboard pedal.
    HalfPedal,      ///< A half-pedal indication.
    PedalUpNotch,   ///< A notched pedal-release indication.
    PedalUpSpecial, ///< The special pedal-release indication.
    HookStart,      ///< A glyph forming the beginning of a pedal line.
    HookEnd,        ///< A glyph forming the end of a pedal line.
    Hyphen,         ///< A glyph used to extend a pedal line.
    PedalChange     ///< Release and immediately re-engage the pedal.
};

} // namespace keyboardpedal

struct KeyboardPedalClassification
{
    keyboardpedal::Type type{};
    bool fromGlyph{};
};

[[nodiscard]]
std::optional<KeyboardPedalClassification> classifyKeyboardPedal(std::string_view text);

[[nodiscard]]
std::optional<KeyboardPedalClassification> classifyKeyboardPedal(
    const musx::util::EnigmaParsingContext& textContext);

} // namespace denigma::classify
