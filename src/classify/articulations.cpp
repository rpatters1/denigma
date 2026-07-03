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
#include <unordered_map>
#include <utility>

#include "smufl_mapping.h"

namespace denigma::classify {

namespace {

using ArticulationType = ArticulationMark::Type;

struct AmbiguousMark
{
    enum class Shape
    {
        Unspecified,
        SmallCircle,
        Plus
    };

    Shape shape{};
    OtherMark::Category category{};
    GlyphStyle glyphStyle{};
    std::optional<std::string> glyphName;
};

using PrivateClassification = std::variant<ArticulationClassification, AmbiguousMark>;

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

static GlyphStyle glyphStyleFromKnownShapeDefType(musx::dom::KnownShapeDefType shapeDefType)
{
    using ST = musx::dom::KnownShapeDefType;
    switch (shapeDefType) {
    case ST::SnapPizzicatoAbove:
        return { musx::dom::VerticalPlacement::Above };
    case ST::SnapPizzicatoBelow:
        return { musx::dom::VerticalPlacement::Below };
    default:
        break;
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

static AmbiguousMark makeAmbiguousMark(
    AmbiguousMark::Shape shape, OtherMark::Category category, std::optional<std::string> glyphName)
{
    return { shape, category, glyphStyleFromGlyphName(glyphName), std::move(glyphName) };
}

static ArticulationClassification makeArticulationMark(
    ArticulationType type, const GlyphStyle& glyphStyle, std::optional<std::string> glyphName = std::nullopt)
{
    ArticulationClassification result;
    result.value = ArticulationMarks{ { { type, glyphStyle } } };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static ArticulationClassification makeOtherMark(
    OtherMark::Category category, const GlyphStyle& glyphStyle, std::optional<std::string> glyphName = std::nullopt)
{
    ArticulationClassification result;
    result.value = OtherMark{ category, glyphStyle };
    setGlyphMetadata(result, std::move(glyphName));
    return result;
}

static musx::dom::InstrumentFamily instrumentFamilyForEntry(const musx::dom::EntryInfoPtr& entryInfo)
{
    if (const auto staff = entryInfo.createCurrentStaff()) {
        return musx::dom::instrumentFamilyFromUuid(staff->instUuid);
    }
    return musx::dom::InstrumentFamily::Unspecified;
}

static ArticulationClassification resolveAmbiguousMark(AmbiguousMark mark, const musx::dom::EntryInfoPtr& entryInfo)
{
    ArticulationClassification result;
    const auto instrumentFamily = instrumentFamilyForEntry(entryInfo);
    switch (mark.shape) {
    case AmbiguousMark::Shape::SmallCircle:
        if (instrumentFamily == musx::dom::InstrumentFamily::Brass) {
            result = makeArticulationMark(ArticulationType::BrassOpen, mark.glyphStyle, std::move(mark.glyphName));
        } else if (instrumentFamily == musx::dom::InstrumentFamily::Strings
            || instrumentFamily == musx::dom::InstrumentFamily::PluckedStrings) {
            result = makeArticulationMark(ArticulationType::StringHarmonic, mark.glyphStyle, std::move(mark.glyphName));
        } else {
            result = makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName));
        }
        break;
    case AmbiguousMark::Shape::Plus:
        result = instrumentFamily == musx::dom::InstrumentFamily::Brass
            ? makeArticulationMark(ArticulationType::BrassStopped, mark.glyphStyle, std::move(mark.glyphName))
            : makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName));
        break;
    default:
        result = makeOtherMark(mark.category, mark.glyphStyle, std::move(mark.glyphName));
        break;
    }
    return result;
}

static std::vector<Ornament::AccidentalMark> accidentalAbove(Ornament::Accidental accidental)
{
    return { { accidental, musx::dom::VerticalPlacement::Above } };
}

static std::vector<Ornament::AccidentalMark> accidentalBelow(Ornament::Accidental accidental)
{
    return { { accidental, musx::dom::VerticalPlacement::Below } };
}

static std::vector<Ornament::AccidentalMark> accidentalsAboveBelow(Ornament::Accidental above, Ornament::Accidental below)
{
    return {
        { above, musx::dom::VerticalPlacement::Above },
        { below, musx::dom::VerticalPlacement::Below }
    };
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
    ArticulationClassification result{};
    if (auto shape = def->getDocument()->getOthers()->get<musx::dom::others::ShapeDef>(def->getRequestedPartId(), shapeId)) {
        using ST = musx::dom::KnownShapeDefType;
        const auto recognizedShapeType = shape->recognize();
        switch (recognizedShapeType) {
        case ST::TenutoMark:
            result = makeArticulationMark({ ArticulationType::Tenuto }, std::nullopt);
            break;
        case ST::VerticalLineRightHooks:
            result = makeVerticalEntryBracket();
            break;
        case ST::SnapPizzicatoAbove:
            result = makeArticulationMark({ ArticulationType::SnapPizzicato }, std::nullopt);
            break;
        case ST::SnapPizzicatoBelow:
            result = makeArticulationMark({ ArticulationType::SnapPizzicato }, std::nullopt);
            break;
        case ST::BuzzPizzicato:
            result = makeArticulationMark({ ArticulationType::BuzzPizzicato }, std::nullopt);
            break;
        default:
            break;
        }
        std::visit([&](auto& value) {
                using T = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<T, ArticulationMarks>) {
                    for (auto& mark : value.marks) {
                        mark.glyphStyle = glyphStyleFromKnownShapeDefType(recognizedShapeType);
                    }
                } else if constexpr (!std::is_same_v<T, std::monostate>) {
                    value.glyphStyle = glyphStyleFromKnownShapeDefType(recognizedShapeType);
                }
            },
            result.value);
    }
    return result;
}

static PrivateClassification classifyGlyphName(std::string glyphName)
{
    using GlyphClassifier = PrivateClassification (*)(std::string);
    static const std::unordered_map<std::string_view, GlyphClassifier> glyphClassifiers = {
        // Arpeggio
        { "arpeggiato", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Normal, std::move(glyphName)); } },
        { "arpeggiatoDown", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Down, std::move(glyphName)); } },
        { "arpeggiatoUp", [](std::string glyphName) -> PrivateClassification { return makeArpeggio(Arpeggio::Type::Up, std::move(glyphName)); } },
        { "arpeggioVerticalSegment", [](std::string glyphName) -> PrivateClassification {
            return makeArpeggio(Arpeggio::Type::VerticalSegment, std::move(glyphName));
        } },

        // ArticulationMarks
        { "articAccentAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent }, std::move(glyphName));
        } },
        { "articAccentAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent }, std::move(glyphName));
        } },
        { "articAccentBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent }, std::move(glyphName));
        } },
        { "articAccentStaccatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articAccentStaccatoAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articAccentStaccatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articAccentStaccatoBelowLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articMarcatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoStaccatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoStaccatoAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoStaccatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoStaccatoBelowLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::StrongAccent }, std::move(glyphName));
        } },
        { "articMarcatoTenutoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::StrongAccent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articMarcatoTenutoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::StrongAccent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articStaccatissimoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Spiccato }, std::move(glyphName));
        } },
        { "articStaccatissimoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Spiccato }, std::move(glyphName));
        } },
        { "articStaccatissimoStrokeAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccatissimo }, std::move(glyphName));
        } },
        { "articStaccatissimoStrokeBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccatissimo }, std::move(glyphName));
        } },
        { "articStaccatissimoWedgeAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccatissimo }, std::move(glyphName));
        } },
        { "articStaccatissimoWedgeBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccatissimo }, std::move(glyphName));
        } },
        { "articStaccatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articStaccatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato }, std::move(glyphName));
        } },
        { "articStressAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Stress }, std::move(glyphName));
        } },
        { "articStressBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Stress }, std::move(glyphName));
        } },
        { "articTenutoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoAccentAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoAccentAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoAccentBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoAccentBelowLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Accent, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoStaccatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoStaccatoAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoStaccatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articTenutoStaccatoBelowLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Staccato, ArticulationType::Tenuto }, std::move(glyphName));
        } },
        { "articUnstressAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Unstress }, std::move(glyphName));
        } },
        { "articUnstressBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Unstress }, std::move(glyphName));
        } },
        { "brassBend", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassBend }, std::move(glyphName));
        } },
        { "brassDoitLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassDoit }, std::move(glyphName));
        } },
        { "brassDoitMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassDoit }, std::move(glyphName));
        } },
        { "brassDoitShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassDoit }, std::move(glyphName));
        } },
        { "brassFallLipLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallLipMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallLipShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallSmoothLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallSmoothMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallSmoothShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughShortMedDecline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughShortSlightDecline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughVeryShortFastDecline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughVeryShortMedDecline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallRoughVeryShortSlightDecilne", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallSlight", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFallSmoothVeryLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFalloff }, std::move(glyphName));
        } },
        { "brassFlip", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassFlip }, std::move(glyphName));
        } },
        { "brassLiftLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftSmoothLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftSmoothMedium", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftSmoothShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftShortMedIncline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftShortSlightIncline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftSlight", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftSmoothVeryLong", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftVeryShortMedIncline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassLiftVeryShortSlightIncline", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "brassPlop", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassPlop }, std::move(glyphName));
        } },
        { "brassScoop", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassScoop }, std::move(glyphName));
        } },
        { "brassSmear", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassSmear }, std::move(glyphName));
        } },
        { "brassVeryLiftShort", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BrassLift }, std::move(glyphName));
        } },
        { "pluckedBuzzPizzicato", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::BuzzPizzicato }, std::move(glyphName));
        } },
        { "pluckedFingernailFlick", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Fingernails }, std::move(glyphName));
        } },
        { "pluckedSnapPizzicatoAbove", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::SnapPizzicato }, std::move(glyphName));
        } },
        { "pluckedSnapPizzicatoBelow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::SnapPizzicato }, std::move(glyphName));
        } },
        { "pluckedSnapPizzicatoAboveGerman", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::SnapPizzicato }, std::move(glyphName));
        } },
        { "pluckedSnapPizzicatoBelowGerman", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::SnapPizzicato }, std::move(glyphName));
        } },
        { "pluckedWithFingernails", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::Fingernails }, std::move(glyphName));
        } },
        { "stringsDownBow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::DownBow }, std::move(glyphName));
        } },
        { "stringsDownBowTurned", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::DownBow }, std::move(glyphName));
        } },
        { "stringsUpBow", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::UpBow }, std::move(glyphName));
        } },
        { "stringsUpBowTurned", [](std::string glyphName) -> PrivateClassification {
            return makeArticulationMark({ ArticulationType::UpBow }, std::move(glyphName));
        } },

        // AmbiguousMark
        { "brassMuteClosed", [](std::string glyphName) -> PrivateClassification {
            return makeAmbiguousMark(
                AmbiguousMark::Shape::Plus, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
        } },
        { "brassMuteOpen", [](std::string glyphName) -> PrivateClassification {
            return makeAmbiguousMark(
                AmbiguousMark::Shape::SmallCircle, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
        } },
        { "stringsHarmonic", [](std::string glyphName) -> PrivateClassification {
            return makeAmbiguousMark(
                AmbiguousMark::Shape::SmallCircle, OtherMark::Category::PerformanceTechnique, std::move(glyphName));
        } },

        // BreathMark
        { "breathMarkComma", [](std::string glyphName) -> PrivateClassification {
            return makeBreathMark(BreathMark::Type::Comma, std::move(glyphName));
        } },
        { "breathMarkCommaLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeBreathMark(BreathMark::Type::Comma, std::move(glyphName));
        } },
        { "breathMarkSalzedo", [](std::string glyphName) -> PrivateClassification {
            return makeBreathMark(BreathMark::Type::Salzedo, std::move(glyphName));
        } },
        { "breathMarkTick", [](std::string glyphName) -> PrivateClassification {
            return makeBreathMark(BreathMark::Type::Tick, std::move(glyphName));
        } },
        { "breathMarkUpbow", [](std::string glyphName) -> PrivateClassification {
            return makeBreathMark(BreathMark::Type::Upbow, std::move(glyphName));
        } },

        // Caesura
        { "caesura", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Normal, std::move(glyphName)); } },
        { "caesuraCurved", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Curved, std::move(glyphName)); } },
        { "caesuraShort", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Short, std::move(glyphName)); } },
        { "caesuraSingleStroke", [](std::string glyphName) -> PrivateClassification {
            return makeCaesura(Caesura::Type::SingleStroke, std::move(glyphName));
        } },
        { "caesuraThick", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Thick, std::move(glyphName)); } },
        { "chantCaesura", [](std::string glyphName) -> PrivateClassification { return makeCaesura(Caesura::Type::Chant, std::move(glyphName)); } },

        // Fermata
        { "curlewSign", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Curlew, Fermata::Duration::Auto, std::move(glyphName));
        } },
        { "fermataAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Normal, Fermata::Duration::Auto, std::move(glyphName));
        } },
        { "fermataBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Normal, Fermata::Duration::Auto, std::move(glyphName));
        } },
        { "fermataLongAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Square, Fermata::Duration::Long, std::move(glyphName));
        } },
        { "fermataLongBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Square, Fermata::Duration::Long, std::move(glyphName));
        } },
        { "fermataLongHenzeAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleDot, Fermata::Duration::Long, std::move(glyphName));
        } },
        { "fermataLongHenzeBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleDot, Fermata::Duration::Long, std::move(glyphName));
        } },
        { "fermataShortAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Angled, Fermata::Duration::Short, std::move(glyphName));
        } },
        { "fermataShortBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::Angled, Fermata::Duration::Short, std::move(glyphName));
        } },
        { "fermataShortHenzeAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::HalfCurve, Fermata::Duration::Short, std::move(glyphName));
        } },
        { "fermataShortHenzeBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::HalfCurve, Fermata::Duration::Short, std::move(glyphName));
        } },
        { "fermataVeryLongAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleSquare, Fermata::Duration::VeryLong, std::move(glyphName));
        } },
        { "fermataVeryLongBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleSquare, Fermata::Duration::VeryLong, std::move(glyphName));
        } },
        { "fermataVeryShortAbove", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleAngled, Fermata::Duration::VeryShort, std::move(glyphName));
        } },
        { "fermataVeryShortBelow", [](std::string glyphName) -> PrivateClassification {
            return makeFermata(Fermata::Shape::DoubleAngled, Fermata::Duration::VeryShort, std::move(glyphName));
        } },

        // Ornament
        { "ornamentHaydn", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); } },
        { "ornamentMordent", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Mordent, std::move(glyphName)); } },
        { "ornamentMordentInverted", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::InvertedMordent, std::move(glyphName));
        } },
        { "ornamentShake3", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Shake, std::move(glyphName)); } },
        { "ornamentShakeMuffat1", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Shake, std::move(glyphName));
        } },
        { "ornamentShortTrill", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName));
        } },
        { "ornamentTremblement", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName));
        } },
        { "ornamentTremblementCouperin", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName));
        } },
        { "ornamentTrill", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Trill, std::move(glyphName)); } },
        { "ornamentTrillFlatAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
        } },
        { "ornamentTrillFlatAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
        } },
        { "ornamentTrillNaturalAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
        } },
        { "ornamentTrillNaturalAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
        } },
        { "ornamentTrillSharpAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
        } },
        { "ornamentTrillSharpAboveLegacy", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Trill, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
        } },
        { "ornamentTurn", [](std::string glyphName) -> PrivateClassification { return makeOrnament(Ornament::Type::Turn, std::move(glyphName)); } },
        { "ornamentTurnFlatAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Flat));
        } },
        { "ornamentTurnFlatAboveSharpBelow", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName),
                accidentalsAboveBelow(Ornament::Accidental::Flat, Ornament::Accidental::Sharp));
        } },
        { "ornamentTurnFlatBelow", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Flat));
        } },
        { "ornamentTurnInverted", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::InvertedTurn, std::move(glyphName));
        } },
        { "ornamentTurnNaturalAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Natural));
        } },
        { "ornamentTurnNaturalBelow", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Natural));
        } },
        { "ornamentTurnSharpAbove", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalAbove(Ornament::Accidental::Sharp));
        } },
        { "ornamentTurnSharpAboveFlatBelow", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName),
                accidentalsAboveBelow(Ornament::Accidental::Sharp, Ornament::Accidental::Flat));
        } },
        { "ornamentTurnSharpBelow", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::Turn, std::move(glyphName), accidentalBelow(Ornament::Accidental::Sharp));
        } },
        { "ornamentTurnSlash", [](std::string glyphName) -> PrivateClassification {
            return makeOrnament(Ornament::Type::InvertedTurn, std::move(glyphName));
        } },

        // Tremolo
        { "buzzRoll", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } },
        { "pendereckiTremolo", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } },
        { "stemPendereckiTremolo", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } },
        { "stockhausenTremolo", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } },
        { "tremolo1", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName)); } },
        { "tremolo1Alt", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName)); } },
        { "tremoloFingered1", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 1, std::move(glyphName));
        } },
        { "tremolo2", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName)); } },
        { "tremolo2Alt", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName)); } },
        { "tremoloFingered2", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 2, std::move(glyphName));
        } },
        { "tremolo3", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName)); } },
        { "tremolo3Alt", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName)); } },
        { "tremoloFingered3", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 3, std::move(glyphName));
        } },
        { "tremolo4", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName)); } },
        { "tremolo4Legacy", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName));
        } },
        { "tremoloFingered4", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 4, std::move(glyphName));
        } },
        { "tremolo5", [](std::string glyphName) -> PrivateClassification { return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName)); } },
        { "tremolo5Legacy", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName));
        } },
        { "tremoloFingered5", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Measured, 5, std::move(glyphName));
        } },
        { "unmeasuredTremolo", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } },
        { "unmeasuredTremoloSimple", [](std::string glyphName) -> PrivateClassification {
            return makeTremolo(Tremolo::Style::Unmeasured, 0, std::move(glyphName));
        } }
    };

    const std::string_view glyph = glyphName;
    if (const auto it = glyphClassifiers.find(glyph); it != glyphClassifiers.end()) {
        return it->second(std::move(glyphName));
    }
    return {};
}

} // namespace

static PrivateClassification classifySymbol(
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
    return ArticulationClassification{};
}

ArticulationClassification classifyArticulationSymbol(
    const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    auto classification = classifySymbol(fontInfo, symbol);
    if (auto* ambiguousMark = std::get_if<AmbiguousMark>(&classification)) {
        return makeOtherMark(ambiguousMark->category, ambiguousMark->glyphStyle, std::move(ambiguousMark->glyphName));
    }
    return std::get<ArticulationClassification>(std::move(classification));
}

static PrivateClassification classifySelectedSymbolContext(
    const musx::dom::details::ArticulationAssign::SelectedSymbolContext& context)
{
    if (context.symbol.isShape) {
        return classifyShape(context.definition, context.symbol.shapeId);
    }
    return classifySymbol(context.symbol.font, context.symbol.character);
}

ArticulationClassification classifyArticulation(
    const musx::dom::MusxInstance<musx::dom::details::ArticulationAssign>& assignment,
    const musx::dom::EntryInfoPtr& entryInfo)
{
    if (!assignment || !entryInfo) {
        return {};
    }
    if (const auto symbolContext = assignment->calcSelectedSymbolContext(entryInfo)) {
        auto classification = classifySelectedSymbolContext(symbolContext.value());
        ArticulationClassification result = std::holds_alternative<AmbiguousMark>(classification)
            ? resolveAmbiguousMark(std::get<AmbiguousMark>(std::move(classification)), entryInfo)
            : std::get<ArticulationClassification>(std::move(classification));
        result.placement = symbolContext->placement;
        return result;
    }
    return {};
}

} // namespace denigma::classify
