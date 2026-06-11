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
#include "classify/articulations.h"

#include <string_view>
#include <utility>

#include "smufl_mapping.h"

namespace denigma::classify {

namespace {

using ArticulationType = StandardArticulation::Type;

static ArticulationClassification makeStandard(std::vector<ArticulationType> types, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = StandardArticulation{ std::move(types) };
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeTremolo(Tremolo::Style style, int marks, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Tremolo{ style, marks };
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeFermata(Fermata::Shape shape, Fermata::Duration duration, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Fermata{ shape, duration };
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeBreathMark(BreathMark::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = BreathMark{ type };
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeArpeggio(Arpeggio::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Arpeggio{ type };
    result.glyphName = std::move(glyphName);
    return result;
}

static ArticulationClassification makeVerticalEntryBracket()
{
    ArticulationClassification result;
    result.value = VerticalEntryBracket{};
    return result;
}

static ArticulationClassification classifyUnicodeSymbol(char32_t symbol)
{
    switch (symbol) {
    case 0x1D167:
    case 0x1D16A:
        return makeTremolo(Tremolo::Style::Measured, 1, std::nullopt);
    case 0x1D168:
    case 0x1D16B:
        return makeTremolo(Tremolo::Style::Measured, 2, std::nullopt);
    case 0x1D169:
    case 0x1D16C:
        return makeTremolo(Tremolo::Style::Measured, 3, std::nullopt);
    case 0x1D17B:
        return makeStandard({ ArticulationType::Accent }, std::nullopt);
    case 0x1D17C:
        return makeStandard({ ArticulationType::Staccato }, std::nullopt);
    case 0x1D17D:
        return makeStandard({ ArticulationType::Tenuto }, std::nullopt);
    case 0x1D17E:
        return makeStandard({ ArticulationType::Staccatissimo }, std::nullopt);
    case 0x1D17F:
        return makeStandard({ ArticulationType::StrongAccent }, std::nullopt);
    default:
        return {};
    }
}

static ArticulationClassification classifyShape(
    const musx::dom::MusxInstance<musx::dom::others::ArticulationDef>& def, musx::dom::Cmper shapeId)
{
    if (!def || !shapeId) {
        return {};
    }
    if (auto shape = def->getDocument()->getOthers()->get<musx::dom::others::ShapeDef>(def->getRequestedPartId(), shapeId)) {
        switch (shape->recognize()) {
        case musx::dom::KnownShapeDefType::TenutoMark:
            return makeStandard({ ArticulationType::Tenuto }, std::nullopt);
        case musx::dom::KnownShapeDefType::VerticalLineRightHooks:
            return makeVerticalEntryBracket();
        default:
            break;
        }
    }
    return {};
}

static ArticulationClassification classifyGlyphName(std::string glyphName)
{
    const std::string_view glyph = glyphName;
    if (glyph == "articAccentAbove" || glyph == "articAccentBelow" || glyph == "articAccentAboveLegacy") {
        return makeStandard({ ArticulationType::Accent }, std::move(glyphName));
    }
    if (glyph == "articAccentStaccatoAbove" || glyph == "articAccentStaccatoBelow"
        || glyph == "articAccentStaccatoAboveLegacy" || glyph == "articAccentStaccatoBelowLegacy") {
        return makeStandard({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
    }
    if (glyph == "articTenutoAccentAbove" || glyph == "articTenutoAccentBelow"
        || glyph == "articTenutoAccentAboveLegacy" || glyph == "articTenutoAccentBelowLegacy") {
        return makeStandard({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "stringsDownBow" || glyph == "stringsDownBowTurned") {
        return makeStandard({ ArticulationType::BowDirectionDown }, std::move(glyphName));
    }
    if (glyph == "stringsUpBow" || glyph == "stringsUpBowTurned") {
        return makeStandard({ ArticulationType::BowDirectionUp }, std::move(glyphName));
    }
    if (glyph == "articStaccatissimoAbove" || glyph == "articStaccatissimoBelow") {
        return makeStandard({ ArticulationType::Spiccato }, std::move(glyphName));
    }
    if (glyph == "articStaccatissimoStrokeAbove" || glyph == "articStaccatissimoStrokeBelow"
        || glyph == "articStaccatissimoWedgeAbove" || glyph == "articStaccatissimoWedgeBelow") {
        return makeStandard({ ArticulationType::Staccatissimo }, std::move(glyphName));
    }
    if (glyph == "articStaccatoAbove" || glyph == "articStaccatoBelow") {
        return makeStandard({ ArticulationType::Staccato }, std::move(glyphName));
    }
    if (glyph == "articMarcatoStaccatoAbove" || glyph == "articMarcatoStaccatoBelow"
        || glyph == "articMarcatoStaccatoAboveLegacy" || glyph == "articMarcatoStaccatoBelowLegacy") {
        return makeStandard({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
    }
    if (glyph == "articTenutoStaccatoAbove" || glyph == "articTenutoStaccatoBelow"
        || glyph == "articTenutoStaccatoAboveLegacy" || glyph == "articTenutoStaccatoBelowLegacy") {
        return makeStandard({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "articStressAbove" || glyph == "articStressBelow") {
        return makeStandard({ ArticulationType::Stress }, std::move(glyphName));
    }
    if (glyph == "articUnstressAbove" || glyph == "articUnstressBelow") {
        return makeStandard({ ArticulationType::Unstress }, std::move(glyphName));
    }
    if (glyph == "articMarcatoAbove" || glyph == "articMarcatoBelow") {
        return makeStandard({ ArticulationType::StrongAccent }, std::move(glyphName));
    }
    if (glyph == "articMarcatoTenutoAbove" || glyph == "articMarcatoTenutoBelow") {
        return makeStandard({ ArticulationType::StrongAccent, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "articTenutoAbove" || glyph == "articTenutoBelow") {
        return makeStandard({ ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "stemPendereckiTremolo" || glyph == "buzzRoll" || glyph == "pendereckiTremolo"
        || glyph == "unmeasuredTremolo" || glyph == "unmeasuredTremoloSimple" || glyph == "stockhausenTremolo") {
        return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
    }
    if (glyph == "tremolo1" || glyph == "tremoloFingered1" || glyph == "tremolo1Alt") {
        return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName));
    }
    if (glyph == "tremolo2" || glyph == "tremoloFingered2" || glyph == "tremolo2Alt") {
        return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName));
    }
    if (glyph == "tremolo3" || glyph == "tremoloFingered3" || glyph == "tremolo3Alt") {
        return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName));
    }
    if (glyph == "tremolo4" || glyph == "tremoloFingered4" || glyph == "tremolo4Legacy") {
        return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName));
    }
    if (glyph == "tremolo5" || glyph == "tremoloFingered5" || glyph == "tremolo5Legacy") {
        return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName));
    }
    if (glyph == "fermataAbove" || glyph == "fermataBelow") {
        return makeFermata(Fermata::Shape::Normal, Fermata::Duration::Auto, std::move(glyphName));
    }
    if (glyph == "fermataVeryShortAbove" || glyph == "fermataVeryShortBelow") {
        return makeFermata(Fermata::Shape::DoubleAngled, Fermata::Duration::VeryShort, std::move(glyphName));
    }
    if (glyph == "fermataShortAbove" || glyph == "fermataShortBelow") {
        return makeFermata(Fermata::Shape::Angled, Fermata::Duration::Short, std::move(glyphName));
    }
    if (glyph == "fermataLongAbove" || glyph == "fermataLongBelow") {
        return makeFermata(Fermata::Shape::Square, Fermata::Duration::Long, std::move(glyphName));
    }
    if (glyph == "fermataVeryLongAbove" || glyph == "fermataVeryLongBelow") {
        return makeFermata(Fermata::Shape::DoubleSquare, Fermata::Duration::VeryLong, std::move(glyphName));
    }
    if (glyph == "fermataLongHenzeAbove" || glyph == "fermataLongHenzeBelow") {
        return makeFermata(Fermata::Shape::DoubleDot, Fermata::Duration::Long, std::move(glyphName));
    }
    if (glyph == "fermataShortHenzeAbove" || glyph == "fermataShortHenzeBelow") {
        return makeFermata(Fermata::Shape::HalfCurve, Fermata::Duration::Short, std::move(glyphName));
    }
    if (glyph == "curlewSign") {
        return makeFermata(Fermata::Shape::Curlew, Fermata::Duration::Auto, std::move(glyphName));
    }
    if (glyph == "breathMarkComma" || glyph == "breathMarkCommaLegacy") {
        return makeBreathMark(BreathMark::Type::Comma, std::move(glyphName));
    }
    if (glyph == "breathMarkTick") {
        return makeBreathMark(BreathMark::Type::Tick, std::move(glyphName));
    }
    if (glyph == "breathMarkUpbow") {
        return makeBreathMark(BreathMark::Type::Upbow, std::move(glyphName));
    }
    if (glyph == "breathMarkSalzedo") {
        return makeBreathMark(BreathMark::Type::Salzedo, std::move(glyphName));
    }
    if (glyph == "caesura") {
        return makeBreathMark(BreathMark::Type::Caesura, std::move(glyphName));
    }
    if (glyph == "caesuraCurved") {
        return makeBreathMark(BreathMark::Type::CaesuraCurved, std::move(glyphName));
    }
    if (glyph == "caesuraShort") {
        return makeBreathMark(BreathMark::Type::CaesuraShort, std::move(glyphName));
    }
    if (glyph == "caesuraThick") {
        return makeBreathMark(BreathMark::Type::CaesuraThick, std::move(glyphName));
    }
    if (glyph == "chantCaesura") {
        return makeBreathMark(BreathMark::Type::ChantCaesura, std::move(glyphName));
    }
    if (glyph == "caesuraSingleStroke") {
        return makeBreathMark(BreathMark::Type::CaesuraSingleStroke, std::move(glyphName));
    }
    if (glyph == "arpeggioVerticalSegment") {
        return makeArpeggio(Arpeggio::Type::VerticalSegment, std::move(glyphName));
    }
    if (glyph == "arpeggiato") {
        return makeArpeggio(Arpeggio::Type::Normal, std::move(glyphName));
    }
    if (glyph == "arpeggiatoUp") {
        return makeArpeggio(Arpeggio::Type::Up, std::move(glyphName));
    }
    if (glyph == "arpeggiatoDown") {
        return makeArpeggio(Arpeggio::Type::Down, std::move(glyphName));
    }

    return {};
}

} // namespace

ArticulationClassification classifyArticulationSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    if (auto unicodeClassification = classifyUnicodeSymbol(symbol)) {
        return unicodeClassification;
    }
    if (fontInfo) {
        if (const auto* glyphName = smufl_mapping::getGlyphNameForFont(
                fontInfo->getName(),
                symbol,
                fontInfo->calcIsSMuFL(),
                smufl_mapping::SmuflGlyphSource::Finale)) {
            return classifyGlyphName(std::string(*glyphName));
        }
    }
    return {};
}

ArticulationClassification classifyArticulation(
    const musx::dom::details::ArticulationAssign::SelectedSymbolContext& context)
{
    if (context.symbol.isShape) {
        return classifyShape(context.definition, context.symbol.shapeId);
    }
    return classifyArticulationSymbol(context.symbol.font, context.symbol.character);
}

ArticulationClassification classifyArticulation(
    const musx::dom::MusxInstance<musx::dom::others::ArticulationDef>& def)
{
    if (!def) {
        return {};
    }
    if (def->mainIsShape) {
        return classifyShape(def, def->mainShape);
    }
    if (def->charMain) {
        return classifyArticulationSymbol(def->fontMain, def->charMain);
    }
    if (def->altIsShape) {
        return classifyShape(def, def->altShape);
    }
    if (def->charAlt) {
        return classifyArticulationSymbol(def->fontAlt, def->charAlt);
    }
    return {};
}

} // namespace denigma::classify
