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

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace jump {

/// @enum Jump
/// @brief Jump or repeat-text classes recognized by the classifier.
enum class Jump
{
    None,
    Segno,
    Coda,
    ToCoda,
    Fine,
    DaCapo,
    DCAlFine,
    DCAlCoda,
    DalSegno,
    DsAlFine,
    DsAlCoda
};

} // namespace jump

/// @brief Classification of a Finale text repeat assignment.
struct JumpClassification
{
    jump::Jump visual{ jump::Jump::None };   ///< Classification from the repeat text/glyph.
    jump::Jump playback{ jump::Jump::None }; ///< Classification from Finale's playback action and target.
};

/// Classifies a Finale text repeat definition by text/glyph only.
JumpClassification classifyJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatDef>& def);

/// Classifies a Finale text repeat assignment by both visible text/glyph and playback behavior.
JumpClassification classifyJump(const musx::dom::MusxInstance<musx::dom::others::TextRepeatAssign>& assignment);

} // namespace classify
} // namespace denigma
