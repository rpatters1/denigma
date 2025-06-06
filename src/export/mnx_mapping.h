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

 //placeholder function

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

enum class FontType
{
    Unicode,        // Standard text
    Symbol,         // Non-SMuFL music fonts based on Adobe Sonata (e.g., Maestro, Petrucci, Sonata)
    GraceNotes,     // GraceNotes (mainly used for tremolos before SMuFL)
    SMuFL           // Modern music fonts following SMuFL
};

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

FontType convertFontToType(const std::shared_ptr<FontInfo>& fontInfo);
JumpType convertTextToJump(const std::string& text, FontType fontType);
std::optional<std::pair<mnx::ClefSign, mnx::OttavaAmountOrZero>> convertCharToClef(const char32_t sym, FontType fontType);
std::vector<EventMarkingType> calcMarkingType(const std::shared_ptr<const others::ArticulationDef>& artic,
    std::optional<int>& numMarks,
    std::optional<mnx::BreathMarkSymbol>& breathMark);

} // namespace mnxexp
} // namespace denigma
