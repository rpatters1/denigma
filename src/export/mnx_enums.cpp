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

namespace denigma {
namespace mnxexp {

// Primary template definition (if needed)
template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum) {
    static_assert(sizeof(FromEnum) == 0, "No specialization exists for this conversion");
    return {};
}

template<>
mnx::NoteValueBase enumConvert(musx::dom::NoteType value)
{
    switch (value) {
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
        default:
            throw std::invalid_argument("Unknown note type: " + std::to_string(int(value)));
    }
}

} // namespace mnxexp
} // namespace denigma