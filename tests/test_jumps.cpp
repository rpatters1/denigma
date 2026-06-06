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
#include "gtest/gtest.h"

#include "classify/jumps.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct TextRepeatContext
{
    DocumentPtr document;
    MusxInstance<others::TextRepeatDef> def;
};

static TextRepeatContext makeTextRepeatContext(const std::string& text, const std::string& fontName = "Times New Roman", int charsetVal = 0)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
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
    <textRepeatDef cmper="1">
      <fontID>1</fontID>
      <fontSize>12</fontSize>
      <efx>0</efx>
    </textRepeatDef>
    <textRepeatText cmper="1">
      <rptText>)xml";
    xml += text;
    xml += R"xml(</rptText>
    </textRepeatText>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
    return { document, document->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, 1) };
}

static Jump classifyTestJump(const std::string& text)
{
    const auto context = makeTextRepeatContext(text);
    return classifyJump(context.def);
}

static Jump classifyTestJumpWithMaestro(const std::string& text)
{
    const auto context = makeTextRepeatContext(text, "Maestro", 4095);
    return classifyJump(context.def);
}

} // namespace

TEST(JumpClassification, ClassifiesText)
{
    EXPECT_EQ(classifyTestJump("D.C. al Fine"), Jump::DCAlFine);
    EXPECT_EQ(classifyTestJump("D.C. al Coda"), Jump::DCAlCoda);
    EXPECT_EQ(classifyTestJump("D.S. al Fine"), Jump::DsAlFine);
    EXPECT_EQ(classifyTestJump("D.S. al Coda"), Jump::DsAlCoda);
    EXPECT_EQ(classifyTestJump("to coda #"), Jump::ToCoda);
    EXPECT_EQ(classifyTestJump("coda"), Jump::Coda);
    EXPECT_EQ(classifyTestJump("to coda"), Jump::ToCoda);
    EXPECT_EQ(classifyTestJump("Fine"), Jump::Fine);
}

TEST(JumpClassification, ClassifiesUnicodeSymbols)
{
    EXPECT_EQ(classifyTestJump("§"), Jump::Segno);
    EXPECT_EQ(classifyTestJump("𝄋"), Jump::Segno);
    EXPECT_EQ(classifyTestJump("𝄌"), Jump::Coda);
}

TEST(JumpClassification, ClassifiesLegacyGlyphs)
{
    EXPECT_EQ(classifyTestJumpWithMaestro("%"), Jump::Segno);
    EXPECT_EQ(classifyTestJumpWithMaestro("\xC3\x9E"), Jump::Coda);
}

TEST(JumpClassification, UnknownTextReturnsNone)
{
    EXPECT_EQ(classifyTestJump("Allegro"), Jump::None);
}
