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

static Dynamic classifyTestDynamic(const std::string& text)
{
    const auto context = makeTextExpressionContext(text);
    return classifyDynamic(context.def);
}

static Dynamic classifyTestDynamicInDynamicsCategory(const std::string& text)
{
    const auto context = makeTextExpressionContext(text, true);
    return classifyDynamic(context.def);
}

} // namespace

TEST(DynamicClassification, ClassifiesCoreDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppppp"), Dynamic::pppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppppp"), Dynamic::ppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppp"), Dynamic::pppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppp"), Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pp"), Dynamic::pp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)p"), Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mp"), Dynamic::mp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mf"), Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)f"), Dynamic::f);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ff"), Dynamic::ff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fff"), Dynamic::fff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffff"), Dynamic::ffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffff"), Dynamic::fffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffffff"), Dynamic::ffffff);
}

TEST(DynamicClassification, ClassifiesAccentDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fp"), Dynamic::fp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fz"), Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sf"), Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfp"), Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfpp"), Dynamic::sfpp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfz"), Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sffz"), Dynamic::sffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rf"), Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rfz"), Dynamic::rfz);
}

TEST(DynamicClassification, ClassifiesAliases)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)forzando"), Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzando"), Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzato"), Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinforzando"), Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinf."), Dynamic::rf);
}

TEST(DynamicClassification, ClassifiesLegacyGlyphs)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)p"), Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)ppp"), Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)F"), Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)Z"), Dynamic::fz);
}

TEST(DynamicClassification, DistinguishesOtherAndNone)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffffff"), Dynamic::Other);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce"), Dynamic::None);
    EXPECT_EQ(classifyTestDynamicInDynamicsCategory("^fontid(1)^size(12)^nfx(0)dolce"), Dynamic::Other);
}

TEST(DynamicClassification, ProvidesCanonicalText)
{
    EXPECT_EQ(dynamicCanonicalText(Dynamic::mf), "mf");
    EXPECT_EQ(dynamicCanonicalText(Dynamic::sfz), "sfz");
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalText(Dynamic::None).empty());
}
