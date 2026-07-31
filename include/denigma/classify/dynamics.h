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
#include <string_view>
#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace dynamics {

/// @enum Dynamic
/// @brief Dynamic marking classes recognized by the classifier.
enum class Dynamic
{
    None,
    Other,
    pppppp,
    ppppp,
    pppp,
    ppp,
    pp,
    p,
    mp,
    mf,
    f,
    ff,
    fff,
    ffff,
    fffff,
    ffffff,
    fp,
    ffp,
    fz,
    ffz,
    pf,
    sf,
    sfp,
    sfpp,
    sfz,
    sffz,
    sfzp,
    rf,
    rfz,
    n
};

/// @enum Change
/// @brief Whether a classified dynamic is absolute or indicates relative motion.
enum class Change
{
    Absolute,
    RelativeIncrease,
    RelativeDecrease
};

/// @enum Reinforcement
/// @brief Leading syllable that reinforces a dynamic attack.
enum class Reinforcement
{
    None,           ///< no leading syllable, as in "fz"
    Sforzando,      ///< "s", as in "sf", "sfz", "sfp"
    Rinforzando     ///< "r", as in "rf", "rfz"
};

/// @enum Level
/// @brief An absolute dynamic level, i.e. a dynamic marking carrying none of the affixes in
/// #Composition. Unlike #Dynamic, this enumerates only levels, so it is a total description of
/// what #Composition::level and #Composition::subsequent can hold.
enum class Level
{
    None,           ///< no level is present
    Other,          ///< a level is present but is louder or softer than this enum can name
    pppppp,
    ppppp,
    pppp,
    ppp,
    pp,
    p,
    mp,
    mf,
    f,
    ff,
    fff,
    ffff,
    fffff,
    ffffff,
    n
};

/// @struct Composition
/// @brief Structural decomposition of a dynamic marking.
///
/// A dynamic marking is built from an optional reinforcement syllable, a sounding level, an
/// optional forzato "z", and an optional level the passage settles to after the attack. A plain
/// level such as "ff" decomposes to itself with no affixes.
///
/// A marking decomposes whether or not it is one of the spellings #Dynamic enumerates. "sffffffz"
/// has no #Dynamic of its own and classifies as Dynamic::Other, but it still decomposes to a
/// sforzando reinforcement, a level of "ffffff", and a forzato.
///
/// The affixes are reported even when the level itself cannot be named. "sffffffffz" yields a
/// sforzando reinforcement and a forzato with #level of Level::Other, so a consumer can still act
/// on the structure it does have and take the level from dynamics::Mark::glyphs.
///
/// A default-constructed Composition means the marking has no structure the classifier recognizes.
/// #level is therefore Level::None only in that empty Composition; a marking that decomposes at all
/// has a level, even if that level can only be reported as Level::Other.
struct Composition
{
    /// @brief Leading reinforcement syllable, if any.
    Reinforcement reinforcement{ Reinforcement::None };
    /// @brief The sounding level, e.g. Level::f for "sfzp".
    Level level{ Level::None };
    /// @brief Whether the marking carries the forzato "z", as in "fz", "sfz", "sffz", "rfz".
    bool forzato{};
    /// @brief The level after the attack, e.g. Level::p for "sfzp". Level::None if the marking
    /// does not fall back to another level.
    Level subsequent{ Level::None };
};

/// @struct Mark
/// @brief Dynamic metadata attached to a source text run.
struct Mark
{
    /// Classified dynamic.
    Dynamic dynamic{};
    /// Structural decomposition of the marking as it is spelled in the source.
    /// Populated even when #dynamic is Dynamic::Other, so that a marking outside the #Dynamic
    /// vocabulary can still be exported in full by a format able to express it.
    Composition composition;
    /// Glyph names that participated in the matched dynamic, in source order.
    /// Empty when any matched character does not resolve to a glyph name.
    std::vector<std::string> glyphs;
};

} // namespace dynamics

/// Classifies a source text chunk as a single plain dynamic mark.
std::optional<dynamics::Mark> classifyDynamicRun(const musx::util::EnigmaTextChunk& chunk, bool forceOther = false);
/// Decomposes a dynamic into its constituent parts. Returns an empty dynamics::Composition for
/// dynamics::Dynamic::None and dynamics::Dynamic::Other, whose spelling is not known from the
/// enumerator alone; use dynamicCompositionFromLetters or dynamics::Mark::composition for those.
dynamics::Composition dynamicComposition(dynamics::Dynamic dynamic);
/// Decomposes dynamic letters such as "sffz" into their constituent parts, whether or not the
/// spelling is one that dynamics::Dynamic enumerates. Returns an empty dynamics::Composition if
/// the letters do not spell a dynamic.
dynamics::Composition dynamicCompositionFromLetters(std::string_view letters);
/// Returns the dynamics::Dynamic that names a level, so that a level can be passed to the
/// functions keyed on dynamics::Dynamic. Returns dynamics::Dynamic::None for Level::None.
dynamics::Dynamic dynamicFromLevel(dynamics::Level level);
/// Returns the canonical text spelling for a dynamic.
std::string dynamicCanonicalText(dynamics::Dynamic dynamic);
/// Returns canonical SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalGlyphs(dynamics::Dynamic dynamic);
/// Returns canonical per-letter SMuFL glyph names for a dynamic.
std::vector<std::string> dynamicCanonicalLetterGlyphs(dynamics::Dynamic dynamic);
/// Returns per-letter SMuFL glyph names for recognized dynamic letters, omitting other characters.
std::vector<std::string> dynamicLettersToLetterGlyphs(std::string_view letters);
/// Returns the dynamic letters represented by known dynamic glyph names.
std::string dynamicGlyphsToLetters(const std::vector<std::string>& glyphs);

} // namespace classify
} // namespace denigma
