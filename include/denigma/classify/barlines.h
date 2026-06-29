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

/// Barline types recognized by Denigma's classifier.
enum class BarlineType
{
    Unsupported,
    NoBarline,
    Regular,
    Double,
    Final,
    Heavy,
    Dashed,
    Tick
};

/// @struct BarlineClassification
/// @brief Result returned by barline classification.
struct BarlineClassification
{
    /// Classified barline type.
    BarlineType type{ BarlineType::Unsupported };
    /// True when the source barline is a short barline.
    bool isShort{};
};

/// Classifies the effective right barline for a Finale measure in staff context.
BarlineClassification classifyBarline(
    const musx::dom::MusxInstance<musx::dom::others::Staff>& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& measure,
    bool isFinalMeasure,
    const musx::dom::MusxInstance<musx::dom::options::BarlineOptions>& barlineOptions);

} // namespace classify
} // namespace denigma
