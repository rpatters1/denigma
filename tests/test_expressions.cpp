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
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "denigma/classify/expressions.h"
#include "musx/musx.h"
#include "utils/stringutils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct TextExpressionContext
{
    DocumentPtr document;
    MusxInstance<others::TextExpressionDef> def;
    MusxInstance<others::MeasureExprAssign> assignment;
};

struct ShapeExpressionContext
{
    DocumentPtr document;
    MusxInstance<others::ShapeExpressionDef> def;
    MusxInstance<others::MeasureExprAssign> assignment;
};

static const dynamics::DynamicMark& firstDynamicMark(const ExpressionClassification& classification)
{
    const auto dynamicIt = std::find_if(classification.runs.begin(), classification.runs.end(), [](const expression::ExpressionRun& run) {
        return run.as<dynamics::DynamicMark>() != nullptr;
    });
    EXPECT_NE(dynamicIt, classification.runs.end());
    return *dynamicIt->as<dynamics::DynamicMark>();
}

static const expression::DynamicQualifier& firstDynamicQualifier(const ExpressionClassification& classification)
{
    const auto qualifierIt = std::find_if(classification.runs.begin(), classification.runs.end(), [](const expression::ExpressionRun& run) {
        return run.as<expression::DynamicQualifier>() != nullptr;
    });
    EXPECT_NE(qualifierIt, classification.runs.end());
    return *qualifierIt->as<expression::DynamicQualifier>();
}

static std::string categoryXmlName(ExpressionCategoryType categoryType)
{
    switch (categoryType) {
    case ExpressionCategoryType::Invalid: break;
    case ExpressionCategoryType::Dynamics: return "dynamics";
    case ExpressionCategoryType::TempoMarks: return "tempoMarks";
    case ExpressionCategoryType::TempoAlterations: return "tempoAlts";
    case ExpressionCategoryType::ExpressiveText: return "expressiveText";
    case ExpressionCategoryType::TechniqueText: return "techniqueText";
    case ExpressionCategoryType::RehearsalMarks: return "rehearsalMarks";
    case ExpressionCategoryType::Misc: return "misc";
    }
    return {};
}

static std::string categoryXml(ExpressionCategoryType categoryType)
{
    const std::string xmlName = categoryXmlName(categoryType);
    if (xmlName.empty()) {
        return {};
    }
    return "    <markingsCategory cmper=\"" + std::to_string(static_cast<int>(categoryType)) + "\">\n"
           "      <categoryType>" + xmlName + "</categoryType>\n"
           "    </markingsCategory>\n";
}

static TextExpressionContext makeTextExpressionContext(
    const std::string& text,
    ExpressionCategoryType categoryType = ExpressionCategoryType::Invalid,
    const std::string& playbackXml = {},
    bool assignmentTopStaff = false,
    const std::string& fontName = "Times New Roman",
    int charsetVal = 0,
    const std::string& expressionXml = {})
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>)xml";
    xml += std::to_string(charsetVal);
    xml += R"xml(</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>)xml";
    xml += fontName;
    xml += R"xml(</name>
    </fontName>
)xml";
    xml += categoryXml(categoryType);
    xml += R"xml(    <textBlock cmper="1">
      <textID>1</textID>
      <textTag>expression</textTag>
    </textBlock>
    <textExprDef cmper="1">
      <textIDKey>1</textIDKey>
)xml";
    if (categoryType != ExpressionCategoryType::Invalid) {
        xml += "      <categoryID>" + std::to_string(static_cast<int>(categoryType)) + "</categoryID>\n";
    }
    xml += playbackXml;
    xml += expressionXml;
    xml += R"xml(    </textExprDef>
)xml";
    xml += R"xml(  </others>
)xml";
    xml += R"xml(  <texts>
    <expression number="1">)xml";
    xml += text;
    xml += R"xml(</expression>
  </texts>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    MusxInstance<others::MeasureExprAssign> assignment;
    if (assignmentTopStaff) {
        auto mutableAssignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper{ 1 }, Inci{ 0 });
        mutableAssignment->textExprId = 1;
        mutableAssignment->staffAssign = static_cast<StaffCmper>(others::StaffList::FloatingValues::TopStaff);
        assignment = mutableAssignment;
    }
    return {
        document,
        document->getOthers()->get<others::TextExpressionDef>(SCORE_PARTID, 1),
        assignment
    };
}

static ShapeExpressionContext makeShapeExpressionContext(
    ExpressionCategoryType categoryType = ExpressionCategoryType::Invalid,
    const std::string& playbackXml = {},
    bool assignmentTopStaff = false)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
)xml";
    xml += categoryXml(categoryType);
    xml += R"xml(    <shapeExprDef cmper="1">
)xml";
    if (categoryType != ExpressionCategoryType::Invalid) {
        xml += "      <categoryID>" + std::to_string(static_cast<int>(categoryType)) + "</categoryID>\n";
    }
    xml += playbackXml;
    xml += R"xml(    </shapeExprDef>
)xml";
    xml += R"xml(  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    MusxInstance<others::MeasureExprAssign> assignment;
    if (assignmentTopStaff) {
        auto mutableAssignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper{ 1 }, Inci{ 0 });
        mutableAssignment->shapeExprId = 1;
        mutableAssignment->staffAssign = static_cast<StaffCmper>(others::StaffList::FloatingValues::TopStaff);
        assignment = mutableAssignment;
    }
    return {
        document,
        document->getOthers()->get<others::ShapeExpressionDef>(SCORE_PARTID, 1),
        assignment
    };
}

static ExpressionClassification classifyTextExpression(
    const std::string& text,
    ExpressionCategoryType categoryType = ExpressionCategoryType::Invalid)
{
    return classifyExpression(makeTextExpressionContext(text, categoryType).def);
}

static std::string tempoPlaybackXml(int bpm = 120, int beatUnitEdu = 1024)
{
    return "      <playType>time</playType>\n"
           "      <value>" + std::to_string(bpm) + "</value>\n"
           "      <auxdata1>" + std::to_string(beatUnitEdu) + "</auxdata1>\n";
}

static MusxInstance<others::MeasureExprAssign> makeStaffTextAssignment(
    const DocumentPtr& document,
    Cmper textExpressionId,
    StaffCmper staff,
    Inci inci = Inci{ 1 })
{
    auto assignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper{ 1 }, inci);
    assignment->textExprId = textExpressionId;
    assignment->staffAssign = staff;
    return assignment;
}

static std::string makeHarpPedalDiagramText(std::u8string_view glyphs)
{
    std::string result;
    utils::appendUtf8(result, glyphs);
    return result;
}

} // namespace

TEST(ExpressionClassification, ClassifiesConcreteDynamicsAndCarriesSurroundingText)
{
    const auto result = classifyTextExpression("sub. pp possibile", ExpressionCategoryType::Dynamics);

    EXPECT_EQ(result.type, ExpressionType::Dynamic);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryConfirmed);
    const auto dynamicIt = std::find_if(result.runs.begin(), result.runs.end(), [](const expression::ExpressionRun& run) {
        return run.as<dynamics::DynamicMark>() != nullptr;
    });
    ASSERT_NE(dynamicIt, result.runs.end());
    EXPECT_EQ(dynamicIt->as<dynamics::DynamicMark>()->dynamic, dynamics::Dynamic::pp);
    ASSERT_EQ(result.runs.size(), 3u);
    ASSERT_NE(result.runs[0].as<expression::GenericText>(), nullptr);
    EXPECT_EQ(result.runs[0].chunk.text, "sub. ");
    ASSERT_NE(result.runs[2].as<expression::GenericText>(), nullptr);
    EXPECT_EQ(result.runs[2].chunk.text, " possibile");
}

TEST(ExpressionClassification, ClassifiesTextExpressionFermataAndBreathMarkSymbols)
{
    const auto fermataContext = makeTextExpressionContext("^fontid(0)^size(24)^nfx(0)U", ExpressionCategoryType::Misc, {}, false, "Maestro", 4095);
    const auto fermata = classifyExpression(fermataContext.def);
    EXPECT_EQ(fermata.type, ExpressionType::Fermata);
    EXPECT_EQ(fermata.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(fermata.fermata().fermata.shape, articulation::Fermata::Shape::Normal);
    EXPECT_EQ(fermata.fermata().fermata.duration, articulation::Fermata::Duration::Auto);
    ASSERT_TRUE(fermata.fermata().glyphName.has_value());
    EXPECT_EQ(*fermata.fermata().glyphName, "fermataAbove");

    const auto breathContext = makeTextExpressionContext("^fontid(0)^size(24)^nfx(0),", ExpressionCategoryType::Misc, {}, false, "Maestro", 4095);
    const auto breath = classifyExpression(breathContext.def);
    EXPECT_EQ(breath.type, ExpressionType::BreathMark);
    EXPECT_EQ(breath.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(breath.breathMark().breathMark.type, articulation::BreathMark::Type::Comma);
    ASSERT_TRUE(breath.breathMark().glyphName.has_value());
    EXPECT_EQ(*breath.breathMark().glyphName, "breathMarkComma");
}

TEST(ExpressionClassification, ClassifiesHarpPedalDiagramGlyphSequence)
{
    const auto context = makeTextExpressionContext(
        "^fontid(0)^size(24)^nfx(0)" + makeHarpPedalDiagramText(u8"\uE680\uE681\uE682\uE683\uE682\uE681\uE680\uE682"),
        ExpressionCategoryType::Misc,
        {},
        false,
        "Bravura");
    const auto result = classifyExpression(context.def);

    ASSERT_EQ(result.type, ExpressionType::HarpDiagram);
    EXPECT_EQ(result.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(result.harpDiagram().d, expression::HarpDiagram::PedalPosition::Flat);
    EXPECT_EQ(result.harpDiagram().c, expression::HarpDiagram::PedalPosition::Natural);
    EXPECT_EQ(result.harpDiagram().b, expression::HarpDiagram::PedalPosition::Sharp);
    EXPECT_EQ(result.harpDiagram().e, expression::HarpDiagram::PedalPosition::Sharp);
    EXPECT_EQ(result.harpDiagram().f, expression::HarpDiagram::PedalPosition::Natural);
    EXPECT_EQ(result.harpDiagram().g, expression::HarpDiagram::PedalPosition::Flat);
    EXPECT_EQ(result.harpDiagram().a, expression::HarpDiagram::PedalPosition::Sharp);
}

TEST(ExpressionClassification, RejectsIncompleteHarpPedalDiagramGlyphSequence)
{
    const auto context = makeTextExpressionContext(
        "^fontid(0)^size(24)^nfx(0)" + makeHarpPedalDiagramText(u8"\uE680\uE681\uE682\uE683\uE682\uE681\uE680"),
        ExpressionCategoryType::Misc,
        {},
        false,
        "Bravura");
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::GenericText);
}

TEST(ExpressionClassification, RequiresSingleCodepointForTextExpressionSymbolClassification)
{
    const auto fermataContext = makeTextExpressionContext("^fontid(0)^size(24)^nfx(0)UU", ExpressionCategoryType::Misc, {}, false, "Maestro", 4095);
    const auto fermata = classifyExpression(fermataContext.def);
    EXPECT_EQ(fermata.type, ExpressionType::GenericText);
    EXPECT_EQ(fermata.basis, ClassificationBasis::FinaleCategory);
}

TEST(ExpressionClassification, ClassifiesRelativeDynamicQualifiers)
{
    const auto increase = classifyTextExpression("piu mf", ExpressionCategoryType::Dynamics);

    EXPECT_EQ(increase.type, ExpressionType::Dynamic);
    EXPECT_EQ(firstDynamicMark(increase).dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(firstDynamicQualifier(increase).change, dynamics::DynamicChange::RelativeIncrease);

    const auto decrease = classifyTextExpression("menos mf", ExpressionCategoryType::Dynamics);

    EXPECT_EQ(decrease.type, ExpressionType::Dynamic);
    EXPECT_EQ(firstDynamicMark(decrease).dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(firstDynamicQualifier(decrease).change, dynamics::DynamicChange::RelativeDecrease);

    const auto gradual = classifyTextExpression("cresc. mf", ExpressionCategoryType::Dynamics);

    EXPECT_EQ(gradual.type, ExpressionType::Dynamic);
    EXPECT_EQ(firstDynamicMark(gradual).dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(std::none_of(gradual.runs.begin(), gradual.runs.end(), [](const expression::ExpressionRun& run) {
        return run.as<expression::DynamicQualifier>() != nullptr;
    }), true);
}

TEST(ExpressionClassification, ClassifiesDynamicCategoryTextAsDynamicOther)
{
    const auto result = classifyTextExpression("dolce", ExpressionCategoryType::Dynamics);

    EXPECT_EQ(result.type, ExpressionType::Dynamic);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(firstDynamicMark(result).dynamic, dynamics::Dynamic::Other);
}

TEST(ExpressionClassification, UsesCategoryForTempoTechniqueAndRehearsalText)
{
    const auto tempo = classifyTextExpression("Allegro", ExpressionCategoryType::TempoMarks);
    EXPECT_EQ(tempo.type, ExpressionType::TempoMark);
    EXPECT_EQ(tempo.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(tempo.tempoText().tempo.text, "Allegro");

    const auto tempoAlteration = classifyTextExpression("poco rit.", ExpressionCategoryType::TempoAlterations);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(tempoAlteration.tempoAlteration().tempo.text, "poco rit.");

    const auto technique = classifyTextExpression("sul pont.", ExpressionCategoryType::TechniqueText);
    EXPECT_EQ(technique.type, ExpressionType::TechniqueText);
    EXPECT_EQ(technique.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(technique.techniqueText().type, expression::TechniqueText::Type::SulPonticello);
    EXPECT_EQ(technique.techniqueText().text, "sul pont.");

    const auto rehearsal = classifyTextExpression("A", ExpressionCategoryType::RehearsalMarks);
    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(rehearsal.rehearsalMark().text, "A");
}

TEST(ExpressionClassification, StrongCategoriesOverrideTextHeuristics)
{
    const auto rehearsal = classifyTextExpression("p", ExpressionCategoryType::RehearsalMarks);
    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::FinaleCategory);

    const auto dynamic = classifyTextExpression("Allegro", ExpressionCategoryType::Dynamics);
    EXPECT_EQ(dynamic.type, ExpressionType::Dynamic);
    EXPECT_EQ(dynamic.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(firstDynamicMark(dynamic).dynamic, dynamics::Dynamic::Other);

    const auto tempoMark = classifyTextExpression("rit.", ExpressionCategoryType::TempoMarks);
    EXPECT_EQ(tempoMark.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoMark.basis, ClassificationBasis::FinaleCategoryCorrected);

    const auto tempoAlteration = classifyTextExpression("Allegro", ExpressionCategoryType::TempoAlterations);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoMark);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FinaleCategoryCorrected);
}

TEST(ExpressionClassification, CorrectsWeakCategoriesWithHeuristics)
{
    const auto technique = classifyTextExpression("Pizz.", ExpressionCategoryType::ExpressiveText);
    EXPECT_EQ(technique.type, ExpressionType::TechniqueText);
    EXPECT_EQ(technique.basis, ClassificationBasis::FinaleCategoryCorrected);
    EXPECT_EQ(technique.techniqueText().type, expression::TechniqueText::Type::Pizzicato);
    EXPECT_EQ(technique.techniqueText().text, "Pizz.");

    const auto tempoAlteration = classifyTextExpression("Rit.", ExpressionCategoryType::Invalid);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::GenericText);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FallbackToGenericText);
    EXPECT_EQ(tempoAlteration.genericText().text, "Rit.");

    const auto tempoMark = classifyTextExpression("Allegro", ExpressionCategoryType::Invalid);
    EXPECT_EQ(tempoMark.type, ExpressionType::GenericText);
    EXPECT_EQ(tempoMark.basis, ClassificationBasis::FallbackToGenericText);
    EXPECT_EQ(tempoMark.genericText().text, "Allegro");
}

TEST(ExpressionClassification, ClassifiesStringTechniqueTokens)
{
    struct ExpectedTechnique
    {
        const char* text{};
        expression::TechniqueText::Type type{};
    };

    const std::vector<ExpectedTechnique> expected = {
        { "arco", expression::TechniqueText::Type::Arco },
        { "pizz", expression::TechniqueText::Type::Pizzicato },
        { "pizz.", expression::TechniqueText::Type::Pizzicato },
        { "col legno", expression::TechniqueText::Type::ColLegno },
        { "col legno.", expression::TechniqueText::Type::ColLegno },
        { "c. legno.", expression::TechniqueText::Type::ColLegno },
        { "col legno batt.", expression::TechniqueText::Type::ColLegnoBattuto },
        { "c. legno battuto", expression::TechniqueText::Type::ColLegnoBattuto },
        { "col legno tratt.", expression::TechniqueText::Type::ColLegnoTratto },
        { "c. legno tratto", expression::TechniqueText::Type::ColLegnoTratto },
        { "sul pont.", expression::TechniqueText::Type::SulPonticello },
        { "s. pont", expression::TechniqueText::Type::SulPonticello },
        { "sul tasto", expression::TechniqueText::Type::SulTasto },
        { "s. tasto.", expression::TechniqueText::Type::SulTasto },
        { "flaut", expression::TechniqueText::Type::Flautando },
        { "flaut.", expression::TechniqueText::Type::Flautando },
        { "flautando", expression::TechniqueText::Type::Flautando },
        { "ordinario", expression::TechniqueText::Type::Ordinario },
        { "ord", expression::TechniqueText::Type::Ordinario },
        { "ord.", expression::TechniqueText::Type::Ordinario },
        { "straight mute", expression::TechniqueText::Type::StraightMute },
        { "straight", expression::TechniqueText::Type::StraightMute },
        { "metal mute", expression::TechniqueText::Type::StraightMute },
        { "wood mute", expression::TechniqueText::Type::StraightMute },
        { "fiber mute", expression::TechniqueText::Type::StraightMute },
        { "fibre mute", expression::TechniqueText::Type::StraightMute },
        { "cup mute", expression::TechniqueText::Type::CupMute },
        { "cup", expression::TechniqueText::Type::CupMute },
        { "harmon mute", expression::TechniqueText::Type::HarmonMute },
        { "wah-wah", expression::TechniqueText::Type::HarmonMute },
        { "plunger mute", expression::TechniqueText::Type::PlungerMute },
        { "bucket mute", expression::TechniqueText::Type::BucketMute },
        { "solotone mute", expression::TechniqueText::Type::SolotoneMute },
        { "stop mute", expression::TechniqueText::Type::StopMute },
        { "brass mute", expression::TechniqueText::Type::StopMute },
        { "stopped", expression::TechniqueText::Type::Stopped },
        { "stop", expression::TechniqueText::Type::Stopped },
        { "con sord.", expression::TechniqueText::Type::Mute }
    };

    for (const auto& item : expected) {
        const auto result = classifyTextExpression(item.text, ExpressionCategoryType::ExpressiveText);
        EXPECT_EQ(result.type, ExpressionType::TechniqueText) << item.text;
        EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryCorrected) << item.text;
        EXPECT_EQ(result.techniqueText().type, item.type) << item.text;
        EXPECT_EQ(result.techniqueText().text, item.text) << item.text;
    }
}

TEST(ExpressionClassification, DoesNotClassifyTremoloTextAsTechnique)
{
    const auto result = classifyTextExpression("tremolo", ExpressionCategoryType::ExpressiveText);
    EXPECT_EQ(result.type, ExpressionType::GenericText);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(result.genericText().text, "tremolo");
}

TEST(ExpressionClassification, ClassifiesTempoPrimoAsTempoAlteration)
{
    for (const auto* text : { "Tempo I", "tempo primo", "Tempo Iº" }) {
        const auto result = classifyTextExpression(text, ExpressionCategoryType::TempoAlterations);
        EXPECT_EQ(result.type, ExpressionType::TempoAlteration) << text;
        EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryConfirmed) << text;

        const auto context = makeTextExpressionContext(text, ExpressionCategoryType::Misc, {}, true);
        const auto assignedResult = classifyExpression(context.assignment);
        EXPECT_EQ(assignedResult.type, ExpressionType::TempoAlteration) << text;
        EXPECT_EQ(assignedResult.basis, ClassificationBasis::Heuristic) << text;
    }
}

TEST(ExpressionClassification, TopStaffAssignmentForcesSystemTextClassification)
{
    const auto tempoMarkContext = makeTextExpressionContext("Andante", ExpressionCategoryType::Misc, {}, true);
    const auto tempoMark = classifyExpression(tempoMarkContext.assignment);
    EXPECT_EQ(tempoMark.type, ExpressionType::TempoMark);
    EXPECT_EQ(tempoMark.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(tempoMark.tempoText().tempo.text, "Andante");

    const auto tempoAlterationContext = makeTextExpressionContext("rit.", ExpressionCategoryType::Misc, {}, true);
    const auto tempoAlteration = classifyExpression(tempoAlterationContext.assignment);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(tempoAlteration.tempoAlteration().tempo.text, "rit.");

    const auto dynamicContext = makeTextExpressionContext("mf", ExpressionCategoryType::Misc, {}, true);
    const auto dynamic = classifyExpression(dynamicContext.assignment);
    EXPECT_EQ(dynamic.type, ExpressionType::Dynamic);
    EXPECT_EQ(dynamic.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(firstDynamicMark(dynamic).dynamic, dynamics::Dynamic::mf);

    const auto rehearsalContext = makeTextExpressionContext("AA", ExpressionCategoryType::Misc, {}, true);
    const auto rehearsal = classifyExpression(rehearsalContext.assignment);
    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::Heuristic);

    const auto numericRehearsalContext = makeTextExpressionContext("27", ExpressionCategoryType::Misc, {}, true);
    const auto numericRehearsal = classifyExpression(numericRehearsalContext.assignment);
    EXPECT_EQ(numericRehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(numericRehearsal.basis, ClassificationBasis::Heuristic);

    const auto fallbackContext = makeTextExpressionContext("cantabile", ExpressionCategoryType::Misc, {}, true);
    const auto fallback = classifyExpression(fallbackContext.assignment);
    EXPECT_EQ(fallback.type, ExpressionType::TempoMark);
    EXPECT_EQ(fallback.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(fallback.tempoText().tempo.text, "cantabile");
}

TEST(ExpressionClassification, NumericTextWithoutTopStaffRemainsGeneric)
{
    const auto result = classifyTextExpression("27", ExpressionCategoryType::Misc);
    EXPECT_EQ(result.type, ExpressionType::GenericText);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(result.genericText().text, "27");
}

TEST(ExpressionClassification, BatchClassificationPropagatesTopStaffSystemTextByDefinition)
{
    const auto tempoAlterationContext = makeTextExpressionContext("rit.", ExpressionCategoryType::Misc, {}, true);
    const auto realStaffTempoAlteration = makeStaffTextAssignment(tempoAlterationContext.document, 1, 1);
    MusxInstanceList<others::MeasureExprAssign> tempoAlterationAssignments(tempoAlterationContext.document, SCORE_PARTID);
    tempoAlterationAssignments.push_back(realStaffTempoAlteration);
    tempoAlterationAssignments.push_back(tempoAlterationContext.assignment);

    const auto singleRealStaffResult = classifyExpression(realStaffTempoAlteration);
    EXPECT_EQ(singleRealStaffResult.type, ExpressionType::GenericText);

    const auto tempoAlterationResults = classifyExpressionAssignments(tempoAlterationAssignments);
    ASSERT_EQ(tempoAlterationResults.size(), 2);
    EXPECT_EQ(tempoAlterationResults[0].classification.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlterationResults[0].classification.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(tempoAlterationResults[1].classification.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlterationResults[1].classification.basis, ClassificationBasis::Heuristic);

    const auto rehearsalContext = makeTextExpressionContext("27", ExpressionCategoryType::Misc, {}, true);
    const auto realStaffRehearsal = makeStaffTextAssignment(rehearsalContext.document, 1, 1);
    MusxInstanceList<others::MeasureExprAssign> rehearsalAssignments(rehearsalContext.document, SCORE_PARTID);
    rehearsalAssignments.push_back(realStaffRehearsal);
    rehearsalAssignments.push_back(rehearsalContext.assignment);

    const auto rehearsalResults = classifyExpressionAssignments(rehearsalAssignments);
    ASSERT_EQ(rehearsalResults.size(), 2);
    EXPECT_EQ(rehearsalResults[0].classification.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsalResults[0].classification.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(rehearsalResults[1].classification.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsalResults[1].classification.basis, ClassificationBasis::Heuristic);
}

TEST(ExpressionClassification, ClassifiesSystemExpressionWithRehearsalMarkStyleAsRehearsalMark)
{
    const auto rehearsalContext = makeTextExpressionContext(
        "Dolce",
        ExpressionCategoryType::Misc,
        {},
        true,
        "Times New Roman",
        0,
        "      <rehearsalMarkStyle>letters</rehearsalMarkStyle>\n");
    const auto rehearsal = classifyExpression(rehearsalContext.assignment);

    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(rehearsal.rehearsalMark().text, "Dolce");
}

TEST(ExpressionClassification, ExtractsTextExpressionTempoPlayback)
{
    const auto context = makeTextExpressionContext(
        "Allegro",
        ExpressionCategoryType::Misc,
        tempoPlaybackXml(132, 1024));
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::TempoMark);
    EXPECT_EQ(result.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(result.tempoText().tempo.text, "Allegro");
    EXPECT_EQ(result.tempoText().tempo.beatsPerMinute, 132);
    EXPECT_EQ(result.tempoText().tempo.beatUnitEdu, 1024);
}

TEST(ExpressionClassification, TempoPlaybackOverridesTextExpressionCategory)
{
    const auto context = makeTextExpressionContext(
        "dolce",
        ExpressionCategoryType::Dynamics,
        tempoPlaybackXml(88, 1024));
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::TempoMark);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryCorrected);
    EXPECT_EQ(result.tempoText().tempo.text, "dolce");
    EXPECT_EQ(result.tempoText().tempo.beatsPerMinute, 88);
    EXPECT_EQ(result.tempoText().tempo.beatUnitEdu, 1024);
}

TEST(ExpressionClassification, ClassifiesShapeExpressionTempoPlayback)
{
    const auto context = makeShapeExpressionContext(
        ExpressionCategoryType::TempoMarks,
        tempoPlaybackXml(96, 512));
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::TempoMark);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(result.tempoText().tempo.beatsPerMinute, 96);
    EXPECT_EQ(result.tempoText().tempo.beatUnitEdu, 512);
}

TEST(ExpressionClassification, SuppressesUnrecognizedShapeExpression)
{
    const auto context = makeShapeExpressionContext();
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::Suppress);
}
