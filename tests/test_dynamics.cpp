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
#include <vector>

#include "gtest/gtest.h"

#include "classify/dynamics.h"
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
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fz").dynamic, Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sf").dynamic, Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfp").dynamic, Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfpp").dynamic, Dynamic::sfpp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfz").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sffz").dynamic, Dynamic::sffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rf").dynamic, Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rfz").dynamic, Dynamic::rfz);
}

TEST(DynamicClassification, ClassifiesAliases)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)forzando").dynamic, Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzando").dynamic, Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzato").dynamic, Dynamic::sfz);
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

TEST(DynamicClassification, DistinguishesOtherAndNone)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffffff").dynamic, Dynamic::Other);
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
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::None).empty());
}
