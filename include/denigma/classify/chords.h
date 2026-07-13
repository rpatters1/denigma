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
#include <string>
#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace chord {

enum class Quality
{
    Major,
    Minor,
    Augmented,
    Diminished,
    Dominant,
    MajorSeventh,
    MinorSeventh,
    DiminishedSeventh,
    HalfDiminished,
    MajorMinor,
    MajorSixth,
    MinorSixth,
    DominantNinth,
    MajorNinth,
    MinorNinth,
    DominantEleventh,
    MajorEleventh,
    MinorEleventh,
    DominantThirteenth,
    MajorThirteenth,
    MinorThirteenth,
    SuspendedSecond,
    SuspendedFourth,
    Pedal,
    None,
    Power
};

struct Degree
{
    enum class Type { Add, Remove, Alter };

    int value{};
    int alteration{};
    Type type{ Type::Add };
    bool printObject{ true };
};

/// @struct SuffixString
/// @brief One contiguous logical text string within a Finale chord suffix.
struct SuffixString
{
    /// @brief Logical vertical position of a Finale chord-suffix string.
    enum class Position
    {
        Inline,
        Above,
        Below
    };

    std::string text;
    Position position{ Position::Inline };
};

} // namespace chord

/// @struct ChordSuffixClassification
/// @brief Classified logical strings reconstructed from Finale chord-suffix elements.
struct ChordSuffixClassification
{
    std::vector<chord::SuffixString> strings;
    std::optional<chord::Quality> quality;
    std::vector<chord::Degree> degrees;
    bool parenthesizeDegrees{};
    bool stackDegrees{};
    bool hasOuterParentheses{};
    bool hasUnrecognizedGlyphs{};

    /// Concatenates the classified strings in Finale's source order.
    std::string calcText() const;
};

/// Reconstructs a Finale chord suffix as Unicode text, resolving SMuFL and known legacy-font glyphs.
ChordSuffixClassification classifyChordSuffix(
    const musx::dom::MusxInstanceList<musx::dom::others::ChordSuffixElement>& suffix);

/// Classifies a Finale chord with no displayed suffix as a major triad.
ChordSuffixClassification classifyChordSuffix();

} // namespace classify
} // namespace denigma
