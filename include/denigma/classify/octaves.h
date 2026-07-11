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

namespace octave {

/// @enum Direction
/// @brief The octave-displacement direction stated by a marking.
enum class Direction
{
    Unknown,    ///< The marking does not state a direction (e.g., a bare "8" glyph).
    Up,         ///< Notes sound above written pitch (alta).
    Down        ///< Notes sound below written pitch (bassa).
};

} // namespace octave

/// @struct OctaveMarkingClassification
/// @brief An octave-displacement marking ("8va", "15mb", the SMuFL ottava glyphs, etc.)
/// recognized in Finale text.
struct OctaveMarkingClassification
{
    int magnitude{};                                        ///< Number of octaves displaced (1, 2, or 3).
    octave::Direction direction{ octave::Direction::Unknown }; ///< The stated direction, when the marking states one.
    bool directionIsExplicit{};                             ///< True when the direction is unmistakable (e.g., "8vb", "bassa").
                                                            ///< "va"/"ma" spellings imply alta only weakly: engravers also use
                                                            ///< them below the staff to mean bassa.
    bool fromGlyph{};                                       ///< True when recognition relied on SMuFL glyphs.
};

/// @brief Classifies plain text as an octave-displacement marking.
///
/// Only unmistakable spellings are recognized (e.g., "8va", "8vb", "15ma", "22mb",
/// "8va bassa"). A bare ASCII number is never classified.
[[nodiscard]]
std::optional<OctaveMarkingClassification> classifyOctaveMarking(std::string_view text);

/// @brief Classifies Finale text as an octave-displacement marking, resolving SMuFL
/// ottava glyphs (including parenthesized and letter-composed forms).
[[nodiscard]]
std::optional<OctaveMarkingClassification> classifyOctaveMarking(
    const musx::util::EnigmaParsingContext& textContext);

} // namespace denigma::classify
