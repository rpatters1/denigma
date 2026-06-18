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
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "denigma/classify/expressions.h"
#include "musx/musx.h"

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
    return "    <markingsCategory cmper=\"1\">\n"
           "      <categoryType>" + xmlName + "</categoryType>\n"
           "    </markingsCategory>\n";
}

static TextExpressionContext makeTextExpressionContext(
    const std::string& text,
    ExpressionCategoryType categoryType = ExpressionCategoryType::Invalid,
    const std::string& playbackXml = {},
    bool assignmentTopStaff = false)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Times New Roman</name>
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
        xml += "      <categoryID>1</categoryID>\n";
    }
    xml += playbackXml;
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
        auto mutableAssignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, 1, Inci{ 0 });
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
        xml += "      <categoryID>1</categoryID>\n";
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
        auto mutableAssignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, 1, Inci{ 0 });
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
    auto assignment = std::make_shared<others::MeasureExprAssign>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, 1, inci);
    assignment->textExprId = textExpressionId;
    assignment->staffAssign = staff;
    return assignment;
}

} // namespace

TEST(ExpressionClassification, ClassifiesConcreteDynamicsAndCarriesSurroundingText)
{
    const auto result = classifyExpression(
        "sub. pp possibile",
        ExpressionCategoryType::Dynamics);

    EXPECT_EQ(result.type, ExpressionType::Dynamic);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(result.dynamic, Dynamic::pp);
    EXPECT_EQ(result.dynamicPrefixText, "sub.");
    EXPECT_EQ(result.dynamicSuffixText, "possibile");
}

TEST(ExpressionClassification, ClassifiesDynamicCategoryTextAsDynamicOther)
{
    const auto result = classifyExpression(
        "dolce",
        ExpressionCategoryType::Dynamics);

    EXPECT_EQ(result.type, ExpressionType::Dynamic);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(result.dynamic, Dynamic::Other);
    EXPECT_EQ(result.text, "dolce");
}

TEST(ExpressionClassification, UsesCategoryForTempoTechniqueAndRehearsalText)
{
    const auto tempo = classifyExpression("Allegro", ExpressionCategoryType::TempoMarks);
    EXPECT_EQ(tempo.type, ExpressionType::TempoMark);
    EXPECT_EQ(tempo.basis, ClassificationBasis::FinaleCategoryConfirmed);

    const auto tempoAlteration = classifyExpression("poco rit.", ExpressionCategoryType::TempoAlterations);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FinaleCategory);

    const auto technique = classifyExpression("sul pont.", ExpressionCategoryType::TechniqueText);
    EXPECT_EQ(technique.type, ExpressionType::TechniqueText);
    EXPECT_EQ(technique.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(technique.technique.type, TechniqueType::SulPonticello);

    const auto rehearsal = classifyExpression("A", ExpressionCategoryType::RehearsalMarks);
    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::FinaleCategory);
}

TEST(ExpressionClassification, StrongCategoriesOverrideTextHeuristics)
{
    const auto rehearsal = classifyExpression("p", ExpressionCategoryType::RehearsalMarks);
    EXPECT_EQ(rehearsal.type, ExpressionType::RehearsalMark);
    EXPECT_EQ(rehearsal.basis, ClassificationBasis::FinaleCategory);

    const auto dynamic = classifyExpression("Allegro", ExpressionCategoryType::Dynamics);
    EXPECT_EQ(dynamic.type, ExpressionType::Dynamic);
    EXPECT_EQ(dynamic.basis, ClassificationBasis::FinaleCategory);
    EXPECT_EQ(dynamic.dynamic, Dynamic::Other);

    const auto tempoMark = classifyExpression("rit.", ExpressionCategoryType::TempoMarks);
    EXPECT_EQ(tempoMark.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoMark.basis, ClassificationBasis::FinaleCategoryCorrected);

    const auto tempoAlteration = classifyExpression("Allegro", ExpressionCategoryType::TempoAlterations);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoMark);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FinaleCategoryCorrected);
}

TEST(ExpressionClassification, CorrectsWeakCategoriesWithHeuristics)
{
    const auto technique = classifyExpression("pizz.", ExpressionCategoryType::ExpressiveText);
    EXPECT_EQ(technique.type, ExpressionType::TechniqueText);
    EXPECT_EQ(technique.basis, ClassificationBasis::FinaleCategoryCorrected);
    EXPECT_EQ(technique.technique.type, TechniqueType::Pizzicato);

    const auto tempoAlteration = classifyExpression("rit.", ExpressionCategoryType::Invalid);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::GenericText);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::FallbackToGenericText);

    const auto tempoMark = classifyExpression("Allegro", ExpressionCategoryType::Invalid);
    EXPECT_EQ(tempoMark.type, ExpressionType::GenericText);
    EXPECT_EQ(tempoMark.basis, ClassificationBasis::FallbackToGenericText);
}

TEST(ExpressionClassification, ClassifiesStringTechniqueTokens)
{
    struct ExpectedTechnique
    {
        const char* text{};
        TechniqueType type{};
    };

    const std::vector<ExpectedTechnique> expected = {
        { "arco", TechniqueType::Arco },
        { "pizz", TechniqueType::Pizzicato },
        { "pizz.", TechniqueType::Pizzicato },
        { "col legno", TechniqueType::ColLegno },
        { "col legno.", TechniqueType::ColLegno },
        { "c. legno.", TechniqueType::ColLegno },
        { "col legno batt.", TechniqueType::ColLegnoBattuto },
        { "c. legno battuto", TechniqueType::ColLegnoBattuto },
        { "col legno tratt.", TechniqueType::ColLegnoTratto },
        { "c. legno tratto", TechniqueType::ColLegnoTratto },
        { "sul pont.", TechniqueType::SulPonticello },
        { "s. pont", TechniqueType::SulPonticello },
        { "sul tasto", TechniqueType::SulTasto },
        { "s. tasto.", TechniqueType::SulTasto },
        { "flaut", TechniqueType::Flautando },
        { "flaut.", TechniqueType::Flautando },
        { "flautando", TechniqueType::Flautando },
        { "ordinario", TechniqueType::Ordinario },
        { "ord", TechniqueType::Ordinario },
        { "ord.", TechniqueType::Ordinario },
        { "straight mute", TechniqueType::StraightMute },
        { "straight", TechniqueType::StraightMute },
        { "metal mute", TechniqueType::StraightMute },
        { "wood mute", TechniqueType::StraightMute },
        { "fiber mute", TechniqueType::StraightMute },
        { "fibre mute", TechniqueType::StraightMute },
        { "cup mute", TechniqueType::CupMute },
        { "cup", TechniqueType::CupMute },
        { "harmon mute", TechniqueType::HarmonMute },
        { "wah-wah", TechniqueType::HarmonMute },
        { "plunger mute", TechniqueType::PlungerMute },
        { "bucket mute", TechniqueType::BucketMute },
        { "solotone mute", TechniqueType::SolotoneMute },
        { "stop mute", TechniqueType::StopMute },
        { "brass mute", TechniqueType::StopMute },
        { "stopped", TechniqueType::Stopped },
        { "stop", TechniqueType::Stopped },
        { "con sord.", TechniqueType::Mute }
    };

    for (const auto& item : expected) {
        const auto result = classifyExpression(item.text, ExpressionCategoryType::ExpressiveText);
        EXPECT_EQ(result.type, ExpressionType::TechniqueText) << item.text;
        EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryCorrected) << item.text;
        EXPECT_EQ(result.technique.type, item.type) << item.text;
    }
}

TEST(ExpressionClassification, ClassifiesTempoPrimoAsTempoAlteration)
{
    for (const auto* text : { "Tempo I", "tempo primo", "Tempo Iº" }) {
        const auto result = classifyExpression(text, ExpressionCategoryType::TempoAlterations);
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

    const auto tempoAlterationContext = makeTextExpressionContext("rit.", ExpressionCategoryType::Misc, {}, true);
    const auto tempoAlteration = classifyExpression(tempoAlterationContext.assignment);
    EXPECT_EQ(tempoAlteration.type, ExpressionType::TempoAlteration);
    EXPECT_EQ(tempoAlteration.basis, ClassificationBasis::Heuristic);

    const auto dynamicContext = makeTextExpressionContext("mf", ExpressionCategoryType::Misc, {}, true);
    const auto dynamic = classifyExpression(dynamicContext.assignment);
    EXPECT_EQ(dynamic.type, ExpressionType::Dynamic);
    EXPECT_EQ(dynamic.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(dynamic.dynamic, Dynamic::mf);

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
}

TEST(ExpressionClassification, NumericTextWithoutTopStaffRemainsGeneric)
{
    const auto result = classifyExpression("27", ExpressionCategoryType::Misc);
    EXPECT_EQ(result.type, ExpressionType::GenericText);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategory);
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

TEST(ExpressionClassification, ExtractsTextExpressionTempoPlayback)
{
    const auto context = makeTextExpressionContext(
        "Allegro",
        ExpressionCategoryType::Misc,
        tempoPlaybackXml(132, 1024));
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::TempoMark);
    EXPECT_EQ(result.basis, ClassificationBasis::Heuristic);
    EXPECT_EQ(result.tempo.text, "allegro");
    EXPECT_EQ(result.tempo.beatsPerMinute, 132);
    EXPECT_EQ(result.tempo.beatUnitEdu, 1024);
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
    EXPECT_EQ(result.tempo.text, "dolce");
    EXPECT_EQ(result.tempo.beatsPerMinute, 88);
    EXPECT_EQ(result.tempo.beatUnitEdu, 1024);
}

TEST(ExpressionClassification, ClassifiesShapeExpressionTempoPlayback)
{
    const auto context = makeShapeExpressionContext(
        ExpressionCategoryType::TempoMarks,
        tempoPlaybackXml(96, 512));
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::TempoMark);
    EXPECT_EQ(result.basis, ClassificationBasis::FinaleCategoryConfirmed);
    EXPECT_EQ(result.tempo.beatsPerMinute, 96);
    EXPECT_EQ(result.tempo.beatUnitEdu, 512);
}

TEST(ExpressionClassification, SuppressesUnrecognizedShapeExpression)
{
    const auto context = makeShapeExpressionContext();
    const auto result = classifyExpression(context.def);

    EXPECT_EQ(result.type, ExpressionType::Suppress);
}
