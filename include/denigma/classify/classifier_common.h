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

/// @struct GlyphStyle
/// @brief Visual style encoded by the source glyph variant.
struct GlyphStyle
{
    /// @enum Placement
    /// @brief Above/below/automatic style encoded in the source glyph variant.
    enum class Placement
    {
        Automatic,
        Above,
        Below
    };

    /// Above/below style encoded in the source glyph variant, when applicable.
    Placement placement{ Placement::Automatic };
};

/// @struct PseudoTie
/// @brief A source shape used as a stand-in for a laissez-vibrer tie or tie end.
struct PseudoTie
{
    /// @enum Type
    /// @brief The tie behavior represented by the source shape.
    enum class Type
    {
        LaissezVibrer,
        TieEnd
    };

    /// The tie behavior represented by the source shape.
    Type type{};
    /// The source shape's resolved contour direction.
    musx::dom::CurveContourDirection contour{ musx::dom::CurveContourDirection::Unspecified };
};

} // namespace classify
} // namespace denigma
