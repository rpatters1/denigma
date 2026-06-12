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
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "gtest/gtest.h"

#include "denigma/classify/dynamics.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct TextExpressionContext
{
    DocumentPtr document;
    MusxInstance<others::TextExpressionDef> def;
};

static TextExpressionContext makeTextExpressionContext(const std::string& text, bool dynamicsCategory = false)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Finale Maestro</name>
    </fontName>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Times New Roman</name>
    </fontName>
    <fontName cmper="2">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Maestro</name>
    </fontName>
)xml";
    if (dynamicsCategory) {
        xml += R"xml(    <markingsCategory cmper="1">
      <categoryType>dynamics</categoryType>
    </markingsCategory>
)xml";
    }
    xml += R"xml(    <textBlock cmper="1">
      <textID>1</textID>
      <textTag>expression</textTag>
    </textBlock>
    <textExprDef cmper="1">
      <textIDKey>1</textIDKey>
)xml";
    if (dynamicsCategory) {
        xml += R"xml(      <categoryID>1</categoryID>
)xml";
    }
    xml += R"xml(    </textExprDef>
  </others>
  <texts>
    <expression number="1">)xml";
    xml += text;
    xml += R"xml(</expression>
  </texts>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
    return { document, document->getOthers()->get<others::TextExpressionDef>(SCORE_PARTID, 1) };
}

static DynamicClassification classifyTestDynamic(const std::string& text)
{
    const auto context = makeTextExpressionContext(text);
    return classifyDynamic(context.def);
}

static DynamicClassification classifyTestDynamicInDynamicsCategory(const std::string& text)
{
    const auto context = makeTextExpressionContext(text, true);
    return classifyDynamic(context.def);
}

static DocumentPtr loadPattersonDefaultDocument()
{
    const std::filesystem::path path = std::filesystem::path(MUSX_TEST_DATA_PATH) / "reference" / "PattersonDefault.enigmaxml";
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        ADD_FAILURE() << "Unable to open " << path;
        return {};
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
}

static std::string smuflGlyphText(const std::vector<std::string>& glyphs)
{
    std::string text = "^fontid(0)^size(24)^nfx(0)";
    for (const auto& glyph : glyphs) {
        if (glyph == "dynamicPiano") text += "&#xE520;";
        else if (glyph == "dynamicMezzo") text += "&#xE521;";
        else if (glyph == "dynamicForte") text += "&#xE522;";
        else if (glyph == "dynamicRinforzando") text += "&#xE523;";
        else if (glyph == "dynamicSforzando") text += "&#xE524;";
        else if (glyph == "dynamicZ") text += "&#xE525;";
        else if (glyph == "dynamicNiente") text += "&#xE526;";
        else if (glyph == "dynamicPPPPPP") text += "&#xE527;";
        else if (glyph == "dynamicPPPPP") text += "&#xE528;";
        else if (glyph == "dynamicPPPP") text += "&#xE529;";
        else if (glyph == "dynamicPPP") text += "&#xE52A;";
        else if (glyph == "dynamicPP") text += "&#xE52B;";
        else if (glyph == "dynamicMP") text += "&#xE52C;";
        else if (glyph == "dynamicMF") text += "&#xE52D;";
        else if (glyph == "dynamicPF") text += "&#xE52E;";
        else if (glyph == "dynamicFF") text += "&#xE52F;";
        else if (glyph == "dynamicFFF") text += "&#xE530;";
        else if (glyph == "dynamicFFFF") text += "&#xE531;";
        else if (glyph == "dynamicFFFFF") text += "&#xE532;";
        else if (glyph == "dynamicFFFFFF") text += "&#xE533;";
        else if (glyph == "dynamicFortePiano") text += "&#xE534;";
        else if (glyph == "dynamicForzando") text += "&#xE535;";
        else if (glyph == "dynamicSforzando1") text += "&#xE536;";
        else if (glyph == "dynamicSforzandoPiano") text += "&#xE537;";
        else if (glyph == "dynamicSforzandoPianissimo") text += "&#xE538;";
        else if (glyph == "dynamicSforzato") text += "&#xE539;";
        else if (glyph == "dynamicSforzatoPiano") text += "&#xE53A;";
        else if (glyph == "dynamicSforzatoFF") text += "&#xE53B;";
        else if (glyph == "dynamicRinforzando1") text += "&#xE53C;";
        else if (glyph == "dynamicRinforzando2") text += "&#xE53D;";
        else ADD_FAILURE() << "Unhandled SMuFL glyph in test: " << glyph;
    }
    return text;
}

} // namespace

TEST(DynamicClassification, ClassifiesCoreDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppppp").dynamic, Dynamic::pppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppppp").dynamic, Dynamic::ppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppp").dynamic, Dynamic::pppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppp").dynamic, Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pp").dynamic, Dynamic::pp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)p").dynamic, Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mp").dynamic, Dynamic::mp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mf").dynamic, Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)f").dynamic, Dynamic::f);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ff").dynamic, Dynamic::ff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fff").dynamic, Dynamic::fff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffff").dynamic, Dynamic::ffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffff").dynamic, Dynamic::fffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffffff").dynamic, Dynamic::ffffff);
}

TEST(DynamicClassification, ClassifiesAccentDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fp").dynamic, Dynamic::fp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffp").dynamic, Dynamic::ffp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fz").dynamic, Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffz").dynamic, Dynamic::ffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pf").dynamic, Dynamic::pf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sf").dynamic, Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfp").dynamic, Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfpp").dynamic, Dynamic::sfpp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfz").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sffz").dynamic, Dynamic::sffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfzp").dynamic, Dynamic::sfzp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rf").dynamic, Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rfz").dynamic, Dynamic::rfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)n").dynamic, Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)niente").dynamic, Dynamic::n);
}

TEST(DynamicClassification, ClassifiesAliases)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)forzando").dynamic, Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzando").dynamic, Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzato").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzado").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinforzando").dynamic, Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinf.").dynamic, Dynamic::rf);
}

TEST(DynamicClassification, ClassifiesLegacyGlyphs)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)p").dynamic, Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)ppp").dynamic, Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)F").dynamic, Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)Z").dynamic, Dynamic::fz);
}

TEST(DynamicClassification, ClassifiesSmuflGlyphs)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52B;").dynamic, Dynamic::pp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52E;").dynamic, Dynamic::pf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53A;").dynamic, Dynamic::sfzp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE526;").dynamic, Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE541;").dynamic, Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE520;").dynamic, Dynamic::ffp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE525;").dynamic, Dynamic::ffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE524;&#xE522;").dynamic, Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE524;&#xE522;&#xE520;").dynamic, Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE536;").dynamic, Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE539;").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53C;").dynamic, Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53D;").dynamic, Dynamic::rfz);
}

TEST(DynamicClassification, ClassifiesPattersonDefaultDynamicsCategory)
{
    const auto document = loadPattersonDefaultDocument();
    ASSERT_TRUE(document);

    const auto category = document->getOthers()->get<others::MarkingCategory>(SCORE_PARTID, 1);
    ASSERT_TRUE(category);
    ASSERT_EQ(category->categoryType, others::MarkingCategory::CategoryType::Dynamics);

    struct ExpectedDynamic
    {
        Cmper cmper{};
        Dynamic dynamic{};
        bool hasAdditionalText{};
    };

    const std::vector<ExpectedDynamic> expected = {
        { 1, Dynamic::ffff }, { 2, Dynamic::fff }, { 3, Dynamic::ff }, { 4, Dynamic::f },
        { 5, Dynamic::mf }, { 6, Dynamic::mp }, { 7, Dynamic::p }, { 8, Dynamic::pp },
        { 9, Dynamic::ppp }, { 10, Dynamic::pppp }, { 23, Dynamic::n }, { 11, Dynamic::fp },
        { 12, Dynamic::fz }, { 50, Dynamic::ffz }, { 13, Dynamic::sf }, { 14, Dynamic::sfz },
        { 15, Dynamic::sffz }, { 16, Dynamic::sfzp }, { 17, Dynamic::sfpp }, { 18, Dynamic::sfp },
        { 19, Dynamic::rfz }, { 20, Dynamic::rf }, { 21, Dynamic::p, true }, { 22, Dynamic::p, true },
        { 51, Dynamic::f, true }, { 52, Dynamic::ff, true }, { 53, Dynamic::pp, true }
    };

    ASSERT_EQ(category->textExpressions.size(), expected.size());
    for (const auto& item : expected) {
        ASSERT_TRUE(category->textExpressions.contains(item.cmper)) << item.cmper;
        const auto def = document->getOthers()->get<others::TextExpressionDef>(SCORE_PARTID, item.cmper);
        ASSERT_TRUE(def) << item.cmper;
        ASSERT_EQ(def->categoryId, 1) << item.cmper;

        const auto classification = classifyDynamic(def);
        EXPECT_EQ(classification.dynamic, item.dynamic) << item.cmper;
        EXPECT_EQ(classification.hasAdditionalText, item.hasAdditionalText) << item.cmper;
    }
}

TEST(DynamicClassification, DistinguishesOtherAndNone)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffffff").dynamic, Dynamic::Other);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)nn").dynamic, Dynamic::None);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce").dynamic, Dynamic::None);
    EXPECT_EQ(classifyTestDynamicInDynamicsCategory("^fontid(1)^size(12)^nfx(0)dolce").dynamic, Dynamic::Other);
}

TEST(DynamicClassification, FlagsAdditionalText)
{
    const auto plain = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pp");
    EXPECT_TRUE(plain);
    EXPECT_TRUE(plain.isDynamic());
    EXPECT_EQ(plain.dynamic, Dynamic::pp);
    EXPECT_FALSE(plain.hasAdditionalText);

    const auto subito = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sub. pp");
    EXPECT_TRUE(subito);
    EXPECT_EQ(subito.dynamic, Dynamic::pp);
    EXPECT_TRUE(subito.hasAdditionalText);

    const auto none = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce");
    EXPECT_FALSE(none);
    EXPECT_FALSE(none.isDynamic());
}

TEST(DynamicClassification, RequiresSpaceDelimitersForNonGlyphTokens)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)G.P.").dynamic, Dynamic::None);
    EXPECT_EQ(classifyTestDynamicInDynamicsCategory("^fontid(1)^size(12)^nfx(0)G.P.").dynamic, Dynamic::Other);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)cresc.mf").dynamic, Dynamic::None);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)cresc mf").dynamic, Dynamic::mf);

    const auto glyphDynamic = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)cresc.^fontid(2)^size(24)^nfx(0)F");
    EXPECT_EQ(glyphDynamic.dynamic, Dynamic::mf);
    EXPECT_TRUE(glyphDynamic.hasAdditionalText);
}

TEST(DynamicClassification, ProvidesCanonicalText)
{
    EXPECT_EQ(dynamicCanonicalText(Dynamic::mf), "mf");
    EXPECT_EQ(dynamicCanonicalText(Dynamic::sfz), "sfz");
    EXPECT_EQ(dynamicCanonicalText(Dynamic::sfzp), "sfzp");
    EXPECT_EQ(dynamicCanonicalText(Dynamic::n), "n");
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::None).empty());
}

TEST(DynamicClassification, ProvidesCanonicalGlyphs)
{
    const std::vector<Dynamic> dynamics = {
        Dynamic::pppppp, Dynamic::ppppp, Dynamic::pppp, Dynamic::ppp, Dynamic::pp, Dynamic::p,
        Dynamic::mp, Dynamic::mf,
        Dynamic::f, Dynamic::ff, Dynamic::fff, Dynamic::ffff, Dynamic::fffff, Dynamic::ffffff,
        Dynamic::fp, Dynamic::ffp, Dynamic::fz, Dynamic::ffz, Dynamic::pf,
        Dynamic::sf, Dynamic::sfp, Dynamic::sfpp, Dynamic::sfz, Dynamic::sffz, Dynamic::sfzp,
        Dynamic::rf, Dynamic::rfz, Dynamic::n
    };

    for (const auto dynamic : dynamics) {
        const auto glyphs = dynamicCanonicalGlyphs(dynamic);
        EXPECT_FALSE(glyphs.empty()) << dynamicCanonicalText(dynamic);
        EXPECT_EQ(classifyTestDynamic(smuflGlyphText(glyphs)).dynamic, dynamic)
            << dynamicCanonicalText(dynamic);
    }

    EXPECT_TRUE(dynamicCanonicalGlyphs(Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalGlyphs(Dynamic::None).empty());
}
