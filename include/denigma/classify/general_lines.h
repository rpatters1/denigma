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

#include <memory>
#include <optional>
#include <string>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace smartshape {

/// @struct LineCap
/// @brief Describes the cap (hook or arrowhead) at one end of a line-type smart shape.
struct LineCap
{
    /// @enum Type
    /// @brief The kind of cap at this end of the line.
    enum class Type
    {
        None,               ///< No cap.
        Hook,               ///< A perpendicular hook. (See #hookLength.)
        ArrowheadPreset,    ///< One of Finale's preset arrowheads. (See #preset.)
        ArrowheadCustom     ///< A Shape Designer arrowhead. (See #customArrowhead and #customArrowheadType.)
    };

    Type type{};                                    ///< The cap type.
    musx::dom::Efix hookLength{};                   ///< Signed hook length: positive extends up. (Only for Type::Hook.)
                                                    ///< Built-in line shape types encode direction in the type and length
                                                    ///< in @ref musx::dom::options::SmartShapeOptions::hookLength; both are
                                                    ///< normalized into this value.
    std::optional<musx::dom::ArrowheadPreset> preset; ///< The preset arrowhead, when the preset id is valid. (Only for Type::ArrowheadPreset.)
    musx::dom::MusxInstance<musx::dom::others::ShapeDef> customArrowhead; ///< The arrowhead shape, when resolvable. (Only for Type::ArrowheadCustom.)
    musx::dom::KnownShapeDefType customArrowheadType{ musx::dom::KnownShapeDefType::Unrecognized }; ///< Recognized type of #customArrowhead. (Only for Type::ArrowheadCustom.)
};

/// @struct GeneralLine
/// @brief Describes the appearance of a line-type smart shape in musxdom terms.
///
/// Both built-in solid/dashed line shape types and custom lines are normalized into this payload,
/// giving consumers a single fallback representation for any line they cannot handle semantically.
/// Built-in line shape types have no #customLine and no texts.
struct GeneralLine
{
    /// The line body style. (Custom lines and built-ins share the musxdom custom-line enum.)
    using LineStyle = musx::dom::others::SmartShapeCustomLine::LineStyle;

    LineStyle lineStyle{};                          ///< Line body style.
    bool lineVisible{};                             ///< False when the line body draws nothing (zero width or a blank character).
    musx::dom::Efix lineWidth{};                    ///< Line width. (Solid and dashed styles.)
    musx::dom::Efix dashOn{};                       ///< Dash length. (Dashed style only.)
    musx::dom::Efix dashOff{};                      ///< Length of gap between dashes. (Dashed style only.)
    char32_t lineChar{};                            ///< The repeated line character. (Char style only.)
    std::shared_ptr<musx::dom::FontInfo> lineCharFont; ///< The font of #lineChar. (Char style only.)
    std::optional<std::string> lineCharGlyphName;   ///< SMuFL glyph name of #lineChar, when resolvable. (Char style only.)
    bool horizontal{};                              ///< True when the line is forced horizontal ("Horizontal" in the custom line
                                                    ///< dialog). Built-in line shape types follow their endpoints and report false.
    LineCap startCap;                               ///< Cap at the left/start end.
    LineCap endCap;                                 ///< Cap at the right/end end.

    musx::util::EnigmaParsingContext startText;         ///< Left-start text, if any. (Custom lines only.)
    musx::util::EnigmaParsingContext continuationText;  ///< Left-continuation text, if any. (Custom lines only.)
    musx::util::EnigmaParsingContext endText;           ///< Right-end text, if any. (Custom lines only.)
    musx::util::EnigmaParsingContext centerFullText;    ///< Center full text, if any. (Custom lines only.)
    musx::util::EnigmaParsingContext centerAbbrText;    ///< Center abbreviated text, if any. (Custom lines only.)

    /// The custom line style, or null when the shape is a built-in line type. Positioning
    /// minutiae (text offsets, line adjustments) remain available here.
    musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine> customLine;
};

} // namespace smartshape

/// @brief Describes a custom line style as a @ref smartshape::GeneralLine.
/// @return std::nullopt when @p customLine is null.
[[nodiscard]]
std::optional<smartshape::GeneralLine> classifyGeneralLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine);

/// @brief Describes a line-type smart shape (built-in or custom) as a @ref smartshape::GeneralLine.
///
/// Entry-attached shapes are never classified as general lines: they carry specific
/// meanings and are classified separately.
/// @return std::nullopt when the shape is entry-attached, is not a line type, or its
/// custom line style cannot be resolved.
[[nodiscard]]
std::optional<smartshape::GeneralLine> classifyGeneralLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape);

} // namespace classify
} // namespace denigma
