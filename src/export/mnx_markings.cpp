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
#include <string>

#include "mnx.h"
#include "mnx_mapping.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

std::vector<EventMarkingType> calcMarkingType(
    const details::ArticulationAssign::SelectedSymbolContext& articContext,
    std::optional<int>& numMarks)
{
    auto checkSymbol = [&](char32_t sym, const MusxInstance<FontInfo>& fontInfo) -> std::vector<EventMarkingType> {
        // First, check for standard Unicode chars
        switch (sym) {
            case 0x1D167:
            case 0x1D16A:
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            case 0x1D168:
            case 0x1D16B:
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            case 0x1D169:
            case 0x1D16C:
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            case 0x1D17B: return { EventMarkingType::Accent };
            case 0x1D17C: return { EventMarkingType::Staccato };
            case 0x1D17D: return { EventMarkingType::Tenuto };
            case 0x1D17E: return { EventMarkingType::Staccatissimo };
            case 0x1D17F: return { EventMarkingType::StrongAccent };
            default: break;                
        }

        /// @todo Spiccato marking has no SMuFl glyph!
        if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
            if (glyphName == "articAccentAbove" || glyphName == "articAccentBelow" || glyphName == "articAccentAboveLegacy") {
                return { EventMarkingType::Accent };
            } else if (glyphName == "articAccentStaccatoAbove" || glyphName == "articAccentStaccatoBelow" ||
                    glyphName == "articAccentStaccatoAboveLegacy" || glyphName == "articAccentStaccatoBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Staccato };
            } else if (glyphName == "articTenutoAccentAbove" || glyphName == "articTenutoAccentBelow" ||
                    glyphName == "articTenutoAccentAboveLegacy" || glyphName == "articTenutoAccentBelowLegacy") {
                return { EventMarkingType::Accent, EventMarkingType::Tenuto };
            } else if (glyphName == "stringsDownBow" || glyphName == "stringsDownBowTurned") {
                return { EventMarkingType::BowDirectionDown };
            } else if (glyphName == "stringsUpBow" || glyphName == "stringsUpBowTurned") {
                return { EventMarkingType::BowDirectionUp };
            } else if (glyphName == "articStaccatissimoAbove" || glyphName == "articStaccatissimoBelow") {
                return { EventMarkingType::Spiccato };
            } else if (glyphName == "articStaccatissimoStrokeAbove" || glyphName == "articStaccatissimoStrokeBelow" ||
                    glyphName == "articStaccatissimoWedgeAbove" || glyphName == "articStaccatissimoWedgeBelow") {
                return { EventMarkingType::Staccatissimo };
            } else if (glyphName == "articStaccatoAbove" || glyphName == "articStaccatoBelow") {
                return { EventMarkingType::Staccato };
            } else if (glyphName == "articMarcatoStaccatoAbove" || glyphName == "articMarcatoStaccatoBelow" ||
                    glyphName == "articMarcatoStaccatoAboveLegacy" || glyphName == "articMarcatoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::StrongAccent };
            } else if (glyphName == "articTenutoStaccatoAbove" || glyphName == "articTenutoStaccatoBelow" ||
                    glyphName == "articTenutoStaccatoAboveLegacy" || glyphName == "articTenutoStaccatoBelowLegacy") {
                return { EventMarkingType::Staccato, EventMarkingType::Tenuto };
            } else if (glyphName == "articStressAbove" || glyphName == "articStressBelow") {
                return { EventMarkingType::Stress };
            } else if (glyphName == "articUnstressAbove" || glyphName == "articUnstressBelow") {
                return { EventMarkingType::Unstress };
            } else if (glyphName == "articMarcatoAbove" || glyphName == "articMarcatoBelow") {
                return { EventMarkingType::StrongAccent };
            } else if (glyphName == "articMarcatoTenutoAbove" || glyphName == "articMarcatoTenutoBelow") {
                return { EventMarkingType::StrongAccent, EventMarkingType::Tenuto };
            } else if (glyphName == "articTenutoAbove" || glyphName == "articTenutoBelow") {
                return { EventMarkingType::Tenuto };
            } else if (glyphName == "stemPendereckiTremolo" || glyphName == "buzzRoll" ||
                    glyphName == "pendereckiTremolo" || glyphName == "unmeasuredTremolo" ||
                    glyphName == "unmeasuredTremoloSimple" || glyphName == "stockhausenTremolo") {
                numMarks = 0;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo1" || glyphName == "tremoloFingered1" || glyphName == "tremolo1Alt") {
                numMarks = 1;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo2" || glyphName == "tremoloFingered2" || glyphName == "tremolo2Alt") {
                numMarks = 2;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo3" || glyphName == "tremoloFingered3" || glyphName == "tremolo3Alt") {
                numMarks = 3;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo4" || glyphName == "tremoloFingered4" || glyphName == "tremolo4Legacy") {
                numMarks = 4;
                return { EventMarkingType::Tremolo };
            } else if (glyphName == "tremolo5" || glyphName == "tremoloFingered5" || glyphName == "tremolo5Legacy") {
                numMarks = 5;
                return { EventMarkingType::Tremolo };
            }
        }
        return {};
    };
    
    auto checkShape = [&](Cmper shapeId) -> std::vector<EventMarkingType> {
        const auto& articDef = *articContext.definition;
        if (auto shape = articDef.getDocument()->getOthers()->get<others::ShapeDef>(articDef.getRequestedPartId(), shapeId)) {
            switch (shape->recognize()) {
            case KnownShapeDefType::TenutoMark:
                return { EventMarkingType::Tenuto };
            default:
                break;
            }
        }
        return {};
    };

    auto result = articContext.symbol.isShape
                ? checkShape(articContext.symbol.shapeId)
                : checkSymbol(articContext.symbol.character, articContext.symbol.font);
    return result;
}

bool isDynamicExpression(const MusxInstance<others::TextExpressionDef>& expr)
{
    /// @todo Do not base dynamics detection (or any other expression type detection) on category.
    auto cat = expr->getDocument()->getOthers()->get<others::MarkingCategory>(expr->getRequestedPartId(), expr->categoryId);
    return cat && cat->categoryType == others::MarkingCategory::CategoryType::Dynamics;
}

std::optional<mnx::Fermata> calcFermata(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement)
{
    if (placement == VerticalPlacement::NotApplicable) {
        return std::nullopt;
    }

    std::optional<mnx::Fermata> result;

    if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
        auto calcOrient = [&]() {
            auto orient = enumConvert<mnx::Orientation>(placement);
            if (orient == mnx::Orientation::Auto) {
                if (glyphName->size() >= 5 && glyphName->compare(glyphName->size() - 5, 5, "Above") == 0) {
                    return mnx::Orientation::Above;
                }
                if (glyphName->size() >= 5 && glyphName->compare(glyphName->size() - 5, 5, "Below") == 0) {
                    return mnx::Orientation::Below;
                }
            }
            return orient;
        };
        auto makeFermata = [&](mnx::FermataSymbol symbol, mnx::FermataDuration duration = mnx::FermataDuration::Auto) {
            auto& fermata = result.emplace();
            fermata.set_or_clear_symbol(symbol);
            fermata.set_or_clear_duration(duration);
            fermata.set_or_clear_orient(calcOrient());
        };

        if (glyphName == "fermataAbove" || glyphName == "fermataBelow") {
            makeFermata(mnx::FermataSymbol::Normal);
        } else if (glyphName == "fermataVeryShortAbove" || glyphName == "fermataVeryShortBelow") {
            makeFermata(mnx::FermataSymbol::DoubleAngled, mnx::FermataDuration::VeryShort);
        } else if (glyphName == "fermataShortAbove" || glyphName == "fermataShortBelow") {
            makeFermata(mnx::FermataSymbol::Angled, mnx::FermataDuration::Short);
        } else if (glyphName == "fermataLongAbove" || glyphName == "fermataLongBelow") {
            makeFermata(mnx::FermataSymbol::Square, mnx::FermataDuration::Long);
        } else if (glyphName == "fermataVeryLongAbove" || glyphName == "fermataVeryLongBelow") {
            makeFermata(mnx::FermataSymbol::DoubleSquare, mnx::FermataDuration::VeryLong);
        } else if (glyphName == "fermataLongHenzeAbove" || glyphName == "fermataLongHenzeBelow") {
            makeFermata(mnx::FermataSymbol::DoubleDot, mnx::FermataDuration::Long);
        } else if (glyphName == "fermataShortHenzeAbove" || glyphName == "fermataShortHenzeBelow") {
            makeFermata(mnx::FermataSymbol::HalfCurve, mnx::FermataDuration::Short);
        } else if (glyphName == "curlewSign") {
            makeFermata(mnx::FermataSymbol::Curlew);
        }
    }

    return result;
}

std::optional<mnx::Fermata> calcFermata(const MusxInstance<FontInfo>& fontInfo, const std::string& symStr, VerticalPlacement placement)
{
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        return calcFermata(fontInfo, sym.value(), placement);
    }
    return std::nullopt;
}

std::optional<mnx::sequence::BreathMark> calcBreathMark(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement)
{
    std::optional<mnx::sequence::BreathMark> result;

    if (auto glyphName = utils::smuflGlyphNameForFont(fontInfo, sym)) {
        auto makeBreathMark = [&](mnx::BreathMarkSymbol breathSym) {
            auto& breathMark = result.emplace();
            breathMark.set_symbol(breathSym);
            breathMark.set_or_clear_orient(enumConvert<mnx::Orientation>(placement));
        };
        if (glyphName == "breathMarkComma" || glyphName == "breathMarkCommaLegacy") {
            makeBreathMark(mnx::BreathMarkSymbol::Comma);
        } else if (glyphName == "breathMarkTick") {
            makeBreathMark(mnx::BreathMarkSymbol::Tick);
        } else if (glyphName == "breathMarkUpbow") {
            makeBreathMark(mnx::BreathMarkSymbol::Upbow);
        } else if (glyphName == "breathMarkSalzedo") {
            makeBreathMark(mnx::BreathMarkSymbol::Salzedo);
        }
    }

    return result;
}

std::optional<mnx::sequence::BreathMark> calcBreathMark(const MusxInstance<FontInfo>& fontInfo, const std::string& symStr, VerticalPlacement placement)
{
    if (const auto sym = utils::utf8ToCodepoint(symStr)) {
        return calcBreathMark(fontInfo, sym.value(), placement);
    }
    return std::nullopt;
}

} // namespace mnxexp
} // namespace denigma
