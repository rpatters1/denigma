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
#include "denigma/classify/general_lines.h"

#include "classify.h"
#include "smufl_mapping.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace classify {

using namespace smartshape;

namespace detail {

std::optional<std::string> glyphNameForFont(
    const std::shared_ptr<musx::dom::FontInfo>& font,
    char32_t codepoint)
{
    if (!font) {
        return std::nullopt;
    }
    if (font->calcIsSMuFL()) {
        if (const auto* name = smufl_mapping::getGlyphName(codepoint)) {
            return std::string(*name);
        }
        return std::nullopt;
    }
    return utils::smuflGlyphNameForFont(font, codepoint);
}

} // namespace detail

namespace {

LineCap classifyLineCap(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine,
    bool atStart)
{
    using CustomLine = musx::dom::others::SmartShapeCustomLine;
    LineCap result;
    const auto capType = atStart ? customLine->lineCapStartType : customLine->lineCapEndType;
    const auto arrowId = atStart ? customLine->lineCapStartArrowId : customLine->lineCapEndArrowId;
    switch (capType) {
    case CustomLine::LineCapType::None:
        break;
    case CustomLine::LineCapType::Hook:
        result.type = LineCap::Type::Hook;
        result.hookLength = atStart ? customLine->lineCapStartHookLength : customLine->lineCapEndHookLength;
        break;
    case CustomLine::LineCapType::ArrowheadPreset:
        result.type = LineCap::Type::ArrowheadPreset;
        if (arrowId >= static_cast<musx::dom::Cmper>(musx::dom::ArrowheadPreset::SmallFilled)
            && arrowId <= static_cast<musx::dom::Cmper>(musx::dom::ArrowheadPreset::MediumCurved)) {
            result.preset = static_cast<musx::dom::ArrowheadPreset>(arrowId);
        }
        break;
    case CustomLine::LineCapType::ArrowheadCustom:
        result.type = LineCap::Type::ArrowheadCustom;
        result.customArrowhead = customLine->getDocument()->getOthers()->get<musx::dom::others::ShapeDef>(
            customLine->getRequestedPartId(), arrowId);
        if (result.customArrowhead) {
            result.customArrowheadType = result.customArrowhead->recognize();
        }
        break;
    }
    return result;
}

struct BuiltInLineSpec
{
    bool dashed{};
    int startHookDirection{}; ///< +1 up, -1 down, 0 none
    int endHookDirection{};   ///< +1 up, -1 down, 0 none
};

std::optional<BuiltInLineSpec> builtInLineSpec(musx::dom::others::SmartShape::ShapeType shapeType)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    switch (shapeType) {
    case ShapeType::SolidLine:          return BuiltInLineSpec{ false, 0, 0 };
    case ShapeType::SolidLineDown:      return BuiltInLineSpec{ false, 0, -1 };
    case ShapeType::SolidLineUp:        return BuiltInLineSpec{ false, 0, +1 };
    case ShapeType::SolidLineDownBoth:  return BuiltInLineSpec{ false, -1, -1 };
    case ShapeType::SolidLineUpBoth:    return BuiltInLineSpec{ false, +1, +1 };
    case ShapeType::SolidLineUpLeft:    return BuiltInLineSpec{ false, +1, 0 };
    case ShapeType::SolidLineDownLeft:  return BuiltInLineSpec{ false, -1, 0 };
    case ShapeType::SolidLineUpDown:    return BuiltInLineSpec{ false, +1, -1 };
    case ShapeType::SolidLineDownUp:    return BuiltInLineSpec{ false, -1, +1 };
    case ShapeType::DashLine:           return BuiltInLineSpec{ true, 0, 0 };
    case ShapeType::DashLineDown:       return BuiltInLineSpec{ true, 0, -1 };
    case ShapeType::DashLineUp:         return BuiltInLineSpec{ true, 0, +1 };
    case ShapeType::DashLineDownBoth:   return BuiltInLineSpec{ true, -1, -1 };
    case ShapeType::DashLineUpBoth:     return BuiltInLineSpec{ true, +1, +1 };
    case ShapeType::DashLineUpLeft:     return BuiltInLineSpec{ true, +1, 0 };
    case ShapeType::DashLineDownLeft:   return BuiltInLineSpec{ true, -1, 0 };
    case ShapeType::DashLineUpDown:     return BuiltInLineSpec{ true, +1, -1 };
    case ShapeType::DashLineDownUp:     return BuiltInLineSpec{ true, -1, +1 };
    default:
        return std::nullopt;
    }
}

musx::dom::Efix efixFromEvpu(musx::dom::Evpu value)
{
    return static_cast<musx::dom::Efix>(value * musx::dom::EFIX_PER_EVPU);
}

GeneralLine classifyBuiltInLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape,
    const BuiltInLineSpec& spec)
{
    using LineStyle = musx::dom::others::SmartShapeCustomLine::LineStyle;
    GeneralLine result;
    result.lineStyle = spec.dashed ? LineStyle::Dashed : LineStyle::Solid;
    const auto options = shape->getDocument()->getOptions()->get<musx::dom::options::SmartShapeOptions>();
    if (options) {
        result.lineWidth = options->smartLineWidth;
        if (spec.dashed) {
            result.dashOn = efixFromEvpu(options->smartDashOn);
            result.dashOff = efixFromEvpu(options->smartDashOff);
        }
    }
    result.lineVisible = result.lineWidth != 0;
    const musx::dom::Efix hookLength = options ? efixFromEvpu(options->hookLength) : 0;
    if (spec.startHookDirection != 0) {
        result.startCap.type = LineCap::Type::Hook;
        result.startCap.hookLength = spec.startHookDirection * hookLength;
    }
    if (spec.endHookDirection != 0) {
        result.endCap.type = LineCap::Type::Hook;
        result.endCap.hookLength = spec.endHookDirection * hookLength;
    }
    return result;
}

} // namespace

std::optional<GeneralLine> classifyGeneralLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShapeCustomLine>& customLine)
{
    using LineStyle = musx::dom::others::SmartShapeCustomLine::LineStyle;
    if (!customLine) {
        return std::nullopt;
    }

    GeneralLine result;
    result.customLine = customLine;
    result.lineStyle = customLine->lineStyle;
    switch (customLine->lineStyle) {
    case LineStyle::Char:
        if (customLine->charParams) {
            result.lineChar = customLine->charParams->lineChar;
            result.lineCharFont = customLine->charParams->font;
            result.lineCharGlyphName = detail::glyphNameForFont(result.lineCharFont, result.lineChar);
        }
        result.lineVisible = result.lineChar != 0 && result.lineChar != U' ';
        break;
    case LineStyle::Solid:
        if (customLine->solidParams) {
            result.lineWidth = customLine->solidParams->lineWidth;
        }
        result.lineVisible = result.lineWidth != 0;
        break;
    case LineStyle::Dashed:
        if (customLine->dashedParams) {
            result.lineWidth = customLine->dashedParams->lineWidth;
            result.dashOn = customLine->dashedParams->dashOn;
            result.dashOff = customLine->dashedParams->dashOff;
        }
        result.lineVisible = result.lineWidth != 0;
        break;
    }
    result.horizontal = customLine->makeHorz;
    result.startCap = classifyLineCap(customLine, true);
    result.endCap = classifyLineCap(customLine, false);

    const auto partId = customLine->getRequestedPartId();
    result.startText = customLine->getLeftStartRawTextCtx(partId);
    result.continuationText = customLine->getLeftContRawTextCtx(partId);
    result.endText = customLine->getRightEndRawTextCtx(partId);
    result.centerFullText = customLine->getCenterFullRawTextCtx(partId);
    result.centerAbbrText = customLine->getCenterAbbrRawTextCtx(partId);
    return result;
}

std::optional<GeneralLine> classifyGeneralLine(
    const musx::dom::MusxInstance<musx::dom::others::SmartShape>& shape)
{
    using ShapeType = musx::dom::others::SmartShape::ShapeType;
    if (!shape || shape->entryBased) {
        // Entry-attached shapes carry specific meanings (e.g., glissandi, bends)
        // and are classified separately.
        return std::nullopt;
    }
    if (shape->shapeType == ShapeType::CustomLine) {
        if (shape->lineStyleId == 0) {
            return std::nullopt;
        }
        return classifyGeneralLine(shape->getDocument()->getOthers()->get<musx::dom::others::SmartShapeCustomLine>(
            shape->getRequestedPartId(), shape->lineStyleId));
    }
    if (const auto spec = builtInLineSpec(shape->shapeType)) {
        return classifyBuiltInLine(shape, *spec);
    }
    return std::nullopt;
}

} // namespace classify
} // namespace denigma
