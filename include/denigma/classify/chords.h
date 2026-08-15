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

/// @enum Quality
/// @brief Harmonic quality recognized in a Finale chord suffix.
///
/// These are the qualities MusicXML models as chord kinds. Anything the suffix adds beyond the
/// quality is reported separately as a @ref chord::Degree.
enum class Quality
{
    Major,              ///< Major triad. Also the quality of a chord with no displayed suffix.
    Minor,              ///< Minor triad.
    Augmented,          ///< Augmented triad.
    Diminished,         ///< Diminished triad.
    Dominant,           ///< Dominant (minor) seventh.
    AugmentedSeventh,   ///< Augmented triad with a minor seventh.
    MajorSeventh,       ///< Major seventh.
    MinorSeventh,       ///< Minor seventh.
    DiminishedSeventh,  ///< Diminished seventh.
    HalfDiminished,     ///< Half-diminished seventh.
    MajorMinor,         ///< Minor triad with a major seventh.
    MajorSixth,         ///< Major sixth.
    MinorSixth,         ///< Minor sixth.
    DominantNinth,      ///< Dominant ninth.
    MajorNinth,         ///< Major ninth.
    MinorNinth,         ///< Minor ninth.
    DominantEleventh,   ///< Dominant eleventh.
    MajorEleventh,      ///< Major eleventh.
    MinorEleventh,      ///< Minor eleventh.
    DominantThirteenth, ///< Dominant thirteenth.
    MajorThirteenth,    ///< Major thirteenth.
    MinorThirteenth,    ///< Minor thirteenth.
    SuspendedSecond,    ///< Suspended second.
    SuspendedFourth,    ///< Suspended fourth.
    Pedal,              ///< Pedal point.
    None,               ///< No chord sounds ("N.C.").
    Power               ///< Power chord (root and fifth).
};

/// @struct Degree
/// @brief One degree alteration that a chord suffix applies on top of its #chord::Quality.
struct Degree
{
    /// @enum Type
    /// @brief How the degree modifies the chord.
    enum class Type
    {
        Add,    ///< The degree is added to the chord.
        Remove, ///< The degree is omitted from the chord.
        Alter   ///< A degree already present in the chord is chromatically altered.
    };

    int value{};                ///< Degree number relative to the root, such as 9, 11, or 13.
    int alteration{};           ///< Chromatic alteration in semitones: -1 for flat, 1 for sharp, 0 for none.
    Type type{ Type::Add };     ///< How the degree modifies the chord.
    bool impliedByText{};       ///< True when the suffix text already spells this degree out, such as
                                ///< the seventh in "7sus4". The chord sounds it, but a consumer that
                                ///< renders the suffix text must not draw the degree a second time.
};

/// @struct SuffixString
/// @brief One contiguous logical text string within a Finale chord suffix.
///
/// A Finale suffix is a sequence of individually positioned glyphs. Consecutive glyphs that share a
/// vertical offset form one string, so a suffix that stacks text produces more than one of these.
struct SuffixString
{
    /// @enum Position
    /// @brief Logical vertical position of a Finale chord-suffix string.
    ///
    /// Finale nudges parentheses and accidentals slightly off the baseline within a single line of
    /// text. Only a shift of at least half the em counts as a separately stacked line.
    enum class Position
    {
        Inline, ///< On the suffix baseline.
        Above,  ///< Stacked above the suffix baseline.
        Below   ///< Stacked below the suffix baseline.
    };

    std::string text;                       ///< The string's text as Unicode.
    Position position{ Position::Inline };  ///< The string's position relative to the suffix baseline.
};

} // namespace chord

/// @struct ChordSuffixClassification
/// @brief Classified logical strings reconstructed from Finale chord-suffix elements.
struct ChordSuffixClassification
{
    /// The suffix's logical strings in Finale's source order. (See @ref chord::SuffixString.)
    std::vector<chord::SuffixString> strings;
    /// The recognized harmonic quality, or std::nullopt when the suffix text could not be parsed.
    std::optional<chord::Quality> quality;
    /// Degrees the suffix applies on top of #quality, in source order.
    std::vector<chord::Degree> degrees;
    /// True when the suffix shows @ref degrees inside parentheses.
    bool parenthesizeDegrees{};
    /// True when the suffix shows @ref degrees stacked vertically instead of running left to right.
    bool stackDegrees{};
    /// True when a single pair of parentheses encloses the entire suffix text.
    bool hasOuterParentheses{};
    /// True when the suffix contains a private-use character that no known font mapping resolved.
    /// The character is preserved in #strings, but it may not render outside its original font.
    bool hasUnrecognizedGlyphs{};

    /// @brief Concatenates the classified strings in Finale's source order.
    /// @return The suffix as a single line of Unicode text, with any stacking flattened.
    std::string calcText() const;
};

/// @brief Reconstructs a Finale chord suffix as Unicode text, resolving SMuFL and known legacy-font glyphs.
/// @param suffix The chord suffix elements, normally from `details::ChordAssign::getChordSuffix`.
ChordSuffixClassification classifyChordSuffix(
    const musx::dom::MusxInstanceList<musx::dom::others::ChordSuffixElement>& suffix);

/// @brief Classifies a Finale chord with no displayed suffix as a major triad.
ChordSuffixClassification classifyChordSuffix();

} // namespace classify
} // namespace denigma
