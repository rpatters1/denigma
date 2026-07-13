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
#include "denigma/classify/noteheads.h"

#include <string_view>
#include <unordered_map>

#include "smufl_mapping.h"

namespace denigma::classify {

using namespace notehead;

namespace {

NoteheadClassification makeNotehead(Shape shape, Fill fill, std::optional<std::string> glyphName)
{
    NoteheadClassification result;
    result.shape = shape;
    result.fill = fill;
    result.glyphName = std::move(glyphName);
    return result;
}

NoteheadClassification classifyGlyphName(std::string glyphName)
{
    static const std::unordered_map<std::string_view, std::pair<Shape, Fill>> glyphTable = {
        // Null
        { "noteheadNull", { Shape::Null, Fill::Unspecified } },

        // Regular
        { "noteheadBlack", { Shape::Regular, Fill::Filled } },
        { "noteheadHalfFilled", { Shape::Regular, Fill::Filled } },
        { "noteheadWholeFilled", { Shape::Regular, Fill::Filled } },
        { "noteheadHalf", { Shape::Regular, Fill::Unfilled } },
        { "noteheadWhole", { Shape::Regular, Fill::Unfilled } },
        { "noteheadDoubleWhole", { Shape::Regular, Fill::Unfilled } },
        { "noteheadDoubleWholeSquare", { Shape::Regular, Fill::Unfilled } },

        // X
        { "noteheadXBlack", { Shape::X, Fill::Filled } },
        { "noteheadHeavyX", { Shape::X, Fill::Filled } },
        { "noteheadHeavyXHat", { Shape::X, Fill::Filled } },
        { "noteheadXHalf", { Shape::X, Fill::Unfilled } },
        { "noteheadXWhole", { Shape::X, Fill::Unfilled } },
        { "noteheadXDoubleWhole", { Shape::X, Fill::Unfilled } },

        // Diamond
        { "noteheadDiamondBlack", { Shape::Diamond, Fill::Filled } },
        { "noteheadDiamondBlackOld", { Shape::Diamond, Fill::Filled } },
        { "noteheadDiamondBlackWide", { Shape::Diamond, Fill::Filled } },
        { "noteheadDiamondHalfFilled", { Shape::Diamond, Fill::Filled } },
        { "noteheadDiamondHalf", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondHalfOld", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondHalfWide", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondWhite", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondWhiteWide", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondWhole", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondWholeOld", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondDoubleWhole", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondDoubleWholeOld", { Shape::Diamond, Fill::Unfilled } },
        { "noteheadDiamondOpen", { Shape::Diamond, Fill::Unfilled } },

        // SmallSlash
        { "noteheadSlashVerticalEndsSmall", { Shape::SmallSlash, Fill::Filled } },

        // LargeSlash
        { "noteheadSlashVerticalEnds", { Shape::LargeSlash, Fill::Filled } },
        { "noteheadSlashHorizontalEnds", { Shape::LargeSlash, Fill::Filled } },
        { "noteheadSlashWhiteWhole", { Shape::LargeSlash, Fill::Unfilled } },
        { "noteheadSlashWhiteHalf", { Shape::LargeSlash, Fill::Unfilled } },
        { "noteheadSlashWhiteDoubleWhole", { Shape::LargeSlash, Fill::Unfilled } },

        // Circled
        { "noteheadCircledBlack", { Shape::Circled, Fill::Filled } },
        { "noteheadCircledBlackLarge", { Shape::Circled, Fill::Filled } },
        { "noteheadCircledHalf", { Shape::Circled, Fill::Unfilled } },
        { "noteheadCircledHalfLarge", { Shape::Circled, Fill::Unfilled } },
        { "noteheadCircledWhole", { Shape::Circled, Fill::Unfilled } },
        { "noteheadCircledWholeLarge", { Shape::Circled, Fill::Unfilled } },
        { "noteheadCircledDoubleWhole", { Shape::Circled, Fill::Unfilled } },
        { "noteheadCircledDoubleWholeLarge", { Shape::Circled, Fill::Unfilled } },
    };

    const std::string_view glyph = glyphName;
    if (const auto it = glyphTable.find(glyph); it != glyphTable.end()) {
        return makeNotehead(it->second.first, it->second.second, std::move(glyphName));
    }
    // A real SMuFL notehead-family glyph was resolved, just not one of the specifically recognized
    // shapes above (e.g. a shape-note, cluster, or arrow notehead). This is a known alternate
    // notehead, so it should not be reported the same as finding nothing at all.
    if (glyph.rfind("notehead", 0) == 0) {
        return makeNotehead(Shape::Other, Fill::Unspecified, std::move(glyphName));
    }
    return {};
}

/// Recognizes a literal ASCII 'x'/'X' as a cross notehead, but only in a font that is not a symbol
/// font (where such a codepoint would otherwise be a direct glyph-index reference to something else).
NoteheadClassification classifyAsciiX(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    if (!fontInfo || fontInfo->calcIsSymbolFont()) {
        return {};
    }
    if (symbol == U'x' || symbol == U'X') {
        return makeNotehead(Shape::X, Fill::Filled, std::nullopt);
    }
    return {};
}

} // namespace

NoteheadClassification classifyNoteheadSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    NoteheadClassification result;
    if (symbol == U' ') {
        result = makeNotehead(Shape::Null, Fill::Unspecified, std::nullopt);
    } else if (auto asciiClassification = classifyAsciiX(fontInfo, symbol)) {
        result = std::move(asciiClassification);
    } else if (fontInfo) {
        if (const auto* glyphName = smufl_mapping::getGlyphNameForFont(
                fontInfo->getName(),
                symbol,
                fontInfo->calcIsSMuFL(),
                smufl_mapping::SmuflGlyphSource::Finale)) {
            result = classifyGlyphName(std::string(*glyphName));
        }
    }
    result.noteheadInfo.font = fontInfo;
    result.noteheadInfo.character = symbol;
    return result;
}

NoteheadClassification classifyNotehead(const musx::dom::NoteInfoPtr& note)
{
    if (!note) {
        return {};
    }
    const auto noteheadInfo = note.calcNoteheadInfo();
    auto result = classifyNoteheadSymbol(noteheadInfo.font, noteheadInfo.character);
    result.noteheadInfo = noteheadInfo;
    return result;
}

} // namespace denigma::classify
