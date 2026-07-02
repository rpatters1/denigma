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
#include "denigma/classify/articulations.h"

#include <string_view>
#include <utility>

#include "smufl_mapping.h"

namespace denigma::classify {

namespace {

using ArticulationType = ArticulationMark::Type;

static GlyphStyle glyphStyleFromGlyphName(const std::string_view glyphName)
{
    const auto endsWith = [&](const std::string_view suffix) {
        return glyphName.size() >= suffix.size()
            && glyphName.compare(glyphName.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (endsWith("Above") || endsWith("AboveLegacy")) {
        return { musx::dom::VerticalPlacement::Above };
    }
    if (endsWith("Below") || endsWith("BelowLegacy")) {
        return { musx::dom::VerticalPlacement::Below };
    }
    return {};
}

static GlyphStyle glyphStyleFromGlyphName(const std::optional<std::string>& glyphName)
{
    if (glyphName) {
        return glyphStyleFromGlyphName(std::string_view(glyphName.value()));
    }
    return {};
}

static void setGlyphMetadata(ArticulationClassification& result, std::optional<std::string> glyphName)
{
    result.glyphName = std::move(glyphName);
}

static ArticulationClassification makeArticulationMark(std::vector<ArticulationType> types, std::optional<std::string> glyphName)
{
    const auto glyphStyle = glyphStyleFromGlyphName(glyphName);
    std::vector<ArticulationMark> marks;
    marks.reserve(types.size());
    for (auto type : types) {
        marks.push_back({ type, glyphStyle });
    }
    ArticulationClassification result;
    result.value = ArticulationMarks{ std::move(marks) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeTremolo(Tremolo::Style style, int marks, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Tremolo{ style, marks, glyphStyleFromGlyphName(glyphName) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeFermata(Fermata::Shape shape, Fermata::Duration duration, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Fermata{ shape, duration, glyphStyleFromGlyphName(glyphName) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeBreathMark(BreathMark::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = BreathMark{ type, glyphStyleFromGlyphName(glyphName) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeCaesura(Caesura::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Caesura{ type, glyphStyleFromGlyphName(glyphName) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeArpeggio(Arpeggio::Type type, std::optional<std::string> glyphName)
{
    ArticulationClassification result;
    result.value = Arpeggio{ type, glyphStyleFromGlyphName(glyphName) };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeOrnament(
    Ornament::Type type,
    std::optional<std::string> glyphName,
    std::vector<Ornament::AccidentalMark> accidentals = {})
{
    ArticulationClassification result;
    result.value = Ornament{ type, glyphStyleFromGlyphName(glyphName), std::move(accidentals) };
    setGlyphMetadata(result, std::move(glyphName));
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
        return makeArticulationMark({ ArticulationType::Accent }, std::nullopt);
    case 0x1D17C:
        return makeArticulationMark({ ArticulationType::Staccato }, std::nullopt);
    case 0x1D17D:
        return makeArticulationMark({ ArticulationType::Tenuto }, std::nullopt);
    case 0x1D17E:
        return makeArticulationMark({ ArticulationType::Staccatissimo }, std::nullopt);
    case 0x1D17F:
        return makeArticulationMark({ ArticulationType::StrongAccent }, std::nullopt);
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
            return makeArticulationMark({ ArticulationType::Tenuto }, std::nullopt);
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
    const auto accidentalAbove = [](Ornament::Accidental accidental) {
        return std::vector<Ornament::AccidentalMark>{ { accidental, musx::dom::VerticalPlacement::Above } };
    };
    const auto accidentalBelow = [](Ornament::Accidental accidental) {
        return std::vector<Ornament::AccidentalMark>{ { accidental, musx::dom::VerticalPlacement::Below } };
    };
    const auto accidentalsAboveBelow = [](Ornament::Accidental above, Ornament::Accidental below) {
        return std::vector<Ornament::AccidentalMark>{
            { above, musx::dom::VerticalPlacement::Above },
            { below, musx::dom::VerticalPlacement::Below }
        };
    };

    if (glyph == "articAccentAbove" || glyph == "articAccentBelow" || glyph == "articAccentAboveLegacy") {
        return makeArticulationMark({ ArticulationType::Accent }, std::move(glyphName));
    }
    if (glyph == "articAccentStaccatoAbove" || glyph == "articAccentStaccatoBelow"
        || glyph == "articAccentStaccatoAboveLegacy" || glyph == "articAccentStaccatoBelowLegacy") {
        return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
    }
    if (glyph == "articTenutoAccentAbove" || glyph == "articTenutoAccentBelow"
        || glyph == "articTenutoAccentAboveLegacy" || glyph == "articTenutoAccentBelowLegacy") {
        return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "stringsDownBow" || glyph == "stringsDownBowTurned") {
        return makeArticulationMark({ ArticulationType::DownBow }, std::move(glyphName));
    }
    if (glyph == "stringsUpBow" || glyph == "stringsUpBowTurned") {
        return makeArticulationMark({ ArticulationType::UpBow }, std::move(glyphName));
    }
    if (glyph == "articStaccatissimoAbove" || glyph == "articStaccatissimoBelow") {
        return makeArticulationMark({ ArticulationType::Spiccato }, std::move(glyphName));
    }
    if (glyph == "articStaccatissimoStrokeAbove" || glyph == "articStaccatissimoStrokeBelow"
        || glyph == "articStaccatissimoWedgeAbove" || glyph == "articStaccatissimoWedgeBelow") {
        return makeArticulationMark({ ArticulationType::Staccatissimo }, std::move(glyphName));
    }
    if (glyph == "articStaccatoAbove" || glyph == "articStaccatoBelow") {
        return makeArticulationMark({ ArticulationType::Staccato }, std::move(glyphName));
    }
    if (glyph == "articMarcatoStaccatoAbove" || glyph == "articMarcatoStaccatoBelow"
        || glyph == "articMarcatoStaccatoAboveLegacy" || glyph == "articMarcatoStaccatoBelowLegacy") {
        return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
    }
    if (glyph == "articTenutoStaccatoAbove" || glyph == "articTenutoStaccatoBelow"
        || glyph == "articTenutoStaccatoAboveLegacy" || glyph == "articTenutoStaccatoBelowLegacy") {
        return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "articStressAbove" || glyph == "articStressBelow") {
        return makeArticulationMark({ ArticulationType::Stress }, std::move(glyphName));
    }
    if (glyph == "articUnstressAbove" || glyph == "articUnstressBelow") {
        return makeArticulationMark({ ArticulationType::Unstress }, std::move(glyphName));
    }
    if (glyph == "articMarcatoAbove" || glyph == "articMarcatoBelow") {
        return makeArticulationMark({ ArticulationType::StrongAccent }, std::move(glyphName));
    }
    if (glyph == "articMarcatoTenutoAbove" || glyph == "articMarcatoTenutoBelow") {
        return makeArticulationMark({ ArticulationType::StrongAccent, ArticulationType::Tenuto }, std::move(glyphName));
    }
    if (glyph == "articTenutoAbove" || glyph == "articTenutoBelow") {
        return makeArticulationMark({ ArticulationType::Tenuto }, std::move(glyphName));
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
        return makeCaesura(Caesura::Type::Normal, std::move(glyphName));
    }
    if (glyph == "caesuraCurved") {
        return makeCaesura(Caesura::Type::Curved, std::move(glyphName));
    }
    if (glyph == "caesuraShort") {
        return makeCaesura(Caesura::Type::Short, std::move(glyphName));
    }
    if (glyph == "caesuraThick") {
        return makeCaesura(Caesura::Type::Thick, std::move(glyphName));
    }
    if (glyph == "chantCaesura") {
        return makeCaesura(Caesura::Type::Chant, std::move(glyphName));
    }
    if (glyph == "caesuraSingleStroke") {
        return makeCaesura(Caesura::Type::SingleStroke, std::move(glyphName));
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
    if (glyph == "ornamentMordentInverted") {
        return makeOrnament(Ornament::Type::InvertedMordent, std::move(glyphName));
    }
    if (glyph == "ornamentTurnInverted" || glyph == "ornamentTurnSlash") {
        return makeOrnament(Ornament::Type::InvertedTurn, std::move(glyphName));
    }
    if (glyph == "ornamentMordent") {
        return makeOrnament(Ornament::Type::Mordent, std::move(glyphName));
    }
    if (glyph == "ornamentShake3" || glyph == "ornamentShakeMuffat1") {
        return makeOrnament(Ornament::Type::Shake, std::move(glyphName));
    }
    if (glyph == "ornamentTrill" || glyph == "ornamentShortTrill" || glyph == "ornamentTremblement"
        || glyph == "ornamentTremblementCouperin" || glyph == "ornamentHaydn") {
        return makeOrnament(Ornament::Type::Trill, std::move(glyphName));
    }
    if (glyph == "ornamentTrillFlatAbove" || glyph == "ornamentTrillFlatAboveLegacy") {
        return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
    }
    if (glyph == "ornamentTrillNaturalAbove" || glyph == "ornamentTrillNaturalAboveLegacy") {
        return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
    }
    if (glyph == "ornamentTrillSharpAbove" || glyph == "ornamentTrillSharpAboveLegacy") {
        return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
    }
    if (glyph == "ornamentTurn") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName));
    }
    if (glyph == "ornamentTurnFlatAbove") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
    }
    if (glyph == "ornamentTurnFlatAboveSharpBelow") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName),
            accidentalsAboveBelow(Ornament::Accidental::Flat, Ornament::Accidental::Sharp));
    }
    if (glyph == "ornamentTurnFlatBelow") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Flat));
    }
    if (glyph == "ornamentTurnNaturalAbove") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
    }
    if (glyph == "ornamentTurnNaturalBelow") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Natural));
    }
    if (glyph == "ornamentTurnSharpAbove") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
    }
    if (glyph == "ornamentTurnSharpAboveFlatBelow") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName),
            accidentalsAboveBelow(Ornament::Accidental::Sharp, Ornament::Accidental::Flat));
    }
    if (glyph == "ornamentTurnSharpBelow") {
        return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Sharp));
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
