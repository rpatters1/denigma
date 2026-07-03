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
#include <variant>
#include <vector>

#include "musx/musx.h"

namespace denigma {

/// @namespace denigma::classify
/// @brief Music classification helpers for Finale symbol extraction.
namespace classify {

/// @struct GlyphStyle
/// @brief Visual style encoded by the source glyph variant.
struct GlyphStyle
{
    /// Above/below style encoded in the source glyph variant, when applicable.
    musx::dom::VerticalPlacement placement{ musx::dom::VerticalPlacement::NotApplicable };
};

/// @struct ArticulationMark
/// @brief Classification for one articulation mark represented by a musx articulation symbol.
struct ArticulationMark
{
    /// @enum Type
    /// @brief Articulation mark type recognized by the classifier.
    enum class Type
    {
        Accent,
        BrassBend,
        BrassDoit,
        BrassFalloff,
        BrassFlip,
        BrassLift,
        BrassOpen,
        BrassPlop,
        BrassScoop,
        BrassSmear,
        BrassStopped,
        BuzzPizzicato,
        Fingernails,
        SnapPizzicato,
        UpBow,
        DownBow,
        SoftAccent,
        Spiccato,
        Staccatissimo,
        Staccato,
        Stress,
        StringHarmonic,
        StrongAccent,
        Tenuto,
        Unstress
    };

    /// Articulation mark type.
    Type type{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct ArticulationMarks
/// @brief Classification for one or more articulation marks represented by a musx articulation symbol.
struct ArticulationMarks
{
    /// One or more articulation marks represented by the source symbol.
    std::vector<ArticulationMark> marks;
};

/// @struct Tremolo
/// @brief Classification for tremolo articulation marks.
struct Tremolo
{
    /// @enum Style
    /// @brief Distinguishes measured from unmeasured tremolo marks.
    enum class Style
    {
        Measured,
        Unmeasured
    };

    /// Tremolo style.
    Style style{};
    /// Number of tremolo strokes.
    int marks{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct Fermata
/// @brief Classification for fermata articulation marks.
struct Fermata
{
    /// @enum Shape
    /// @brief Visual fermata shape.
    enum class Shape
    {
        Normal,
        Angled,
        DoubleAngled,
        Square,
        DoubleSquare,
        HalfCurve,
        DoubleDot,
        Curlew
    };

    /// @enum Duration
    /// @brief Finale playback-duration class associated with the fermata.
    enum class Duration
    {
        Auto,
        VeryShort,
        Short,
        Long,
        VeryLong
    };

    /// Visual fermata shape.
    Shape shape{};
    /// Playback-duration class.
    Duration duration{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct BreathMark
/// @brief Classification for breath marks.
struct BreathMark
{
    /// @enum Type
    /// @brief Breath mark type.
    enum class Type
    {
        Comma,
        Tick,
        Upbow,
        Salzedo
    };

    /// Classified breath mark type.
    Type type{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct Caesura
/// @brief Classification for caesura articulation marks.
struct Caesura
{
    /// @enum Type
    /// @brief Caesura type.
    enum class Type
    {
        Normal,
        Curved,
        Short,
        Thick,
        Chant,
        SingleStroke
    };

    /// Classified caesura type.
    Type type{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct Arpeggio
/// @brief Classification for arpeggio articulation marks.
struct Arpeggio
{
    /// @enum Type
    /// @brief Arpeggio type.
    enum class Type
    {
        VerticalSegment,
        Normal,
        Up,
        Down
    };

    /// Classified arpeggio type.
    Type type{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct Ornament
/// @brief Classification for ornament articulation marks.
struct Ornament
{
    /// @enum Accidental
    /// @brief Accidentals attached to an ornament sign.
    enum class Accidental
    {
        Unspecified,
        Flat,
        Natural,
        Sharp
    };

    /// @enum Type
    /// @brief Ornament types recognized by the classifier.
    enum class Type
    {
        InvertedMordent,
        InvertedTurn,
        Mordent,
        Shake,
        Trill,
        Turn
    };

    /// @struct AccidentalMark
    /// @brief Accidental sign attached to an ornament.
    struct AccidentalMark
    {
        /// Accidental sign shown with the ornament.
        Accidental accidental{};
        /// Placement of the accidental relative to the ornament sign.
        musx::dom::VerticalPlacement placement{};
    };

    /// Classified ornament type.
    Type type{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
    /// Accidentals attached to the ornament sign.
    std::vector<AccidentalMark> accidentals;
};

/// @struct VerticalEntryBracket
/// @brief Classification for Finale vertical entry bracket shapes.
struct VerticalEntryBracket
{
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// @struct OtherMark
/// @brief Classification for recognized articulation symbols that have no more specific Denigma classification.
struct OtherMark
{
    /// @enum Category
    /// @brief Broad notation category for preserving a known but unclassified mark.
    enum class Category
    {
        Articulation,
        PerformanceTechnique
    };

    /// Broad notation category for the mark.
    Category category{};
    /// Visual style encoded by the source glyph variant.
    GlyphStyle glyphStyle{};
};

/// Variant payload for articulation classification.
using ArticulationValue = std::variant<std::monostate, ArticulationMarks, Tremolo, Fermata, BreathMark, Caesura, Arpeggio, Ornament,
    VerticalEntryBracket, OtherMark>;

/// @struct ArticulationClassification
/// @brief Result returned by articulation classification.
struct ArticulationClassification
{
    /// Classified articulation payload, or std::monostate when no articulation was recognized.
    ArticulationValue value{};
    /// Resolved placement of the assigned articulation, when classified from an assignment.
    musx::dom::VerticalPlacement placement{ musx::dom::VerticalPlacement::NotApplicable };
    /// SMuFL glyph name associated with the recognized symbol, when available.
    std::optional<std::string> glyphName;

    /// Returns true when the source was recognized as an articulation.
    bool isArticulation() const noexcept
    { return !std::holds_alternative<std::monostate>(value); }

    /// Returns true when the source was recognized as an articulation.
    explicit operator bool() const noexcept
    { return isArticulation(); }

    /// Returns the classified payload as T, or nullptr when it has another type.
    template <typename T>
    const T* as() const noexcept
    { return std::get_if<T>(&value); }

    /// Returns true when the classified payload has type T.
    template <typename T>
    bool is() const noexcept
    { return std::holds_alternative<T>(value); }
};

/// Classifies an articulation assignment for an entry.
ArticulationClassification classifyArticulation(
    const musx::dom::MusxInstance<musx::dom::details::ArticulationAssign>& assignment,
    const musx::dom::EntryInfoPtr& entryInfo);
/// Classifies an articulation symbol from a font and character code.
ArticulationClassification classifyArticulationSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol);

} // namespace classify
} // namespace denigma
