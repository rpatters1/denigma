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

#include "core/musx_reader.h"
#include "denigma/classify/jumps.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct TextRepeatContext
{
    DocumentPtr document;
    MusxInstance<others::TextRepeatDef> def;
    MusxInstance<others::TextRepeatAssign> assignment;
};

static TextRepeatContext makeTextRepeatContext(
    const std::string& text,
    const std::string& fontName = "Times New Roman",
    int charsetVal = 0,
    const std::string& action = "",
    int target = 0)
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
)xml";
    if (!action.empty()) {
        xml += R"xml(    <textRepeatAssign cmper="1" inci="0">
      <target>)xml";
        xml += std::to_string(target);
        xml += R"xml(</target>
      <repnum>1</repnum>
      <action>)xml";
        xml += action;
        xml += R"xml(</action>
    </textRepeatAssign>
)xml";
    }
    xml += R"xml(
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, 1),
        document->getOthers()->get<others::TextRepeatAssign>(SCORE_PARTID, 1, 0) };
}

static TextRepeatContext makeJumpToMarkContext()
{
    const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Times New Roman</name>
    </fontName>
    <textRepeatDef cmper="1">
      <fontID>1</fontID>
      <fontSize>12</fontSize>
      <efx>0</efx>
    </textRepeatDef>
    <textRepeatText cmper="1">
      <rptText>Go</rptText>
    </textRepeatText>
    <textRepeatDef cmper="2">
      <fontID>1</fontID>
      <fontSize>12</fontSize>
      <efx>0</efx>
    </textRepeatDef>
    <textRepeatText cmper="2">
      <rptText>𝄋</rptText>
    </textRepeatText>
    <textRepeatAssign cmper="1" inci="0">
      <target>2</target>
      <repnum>1</repnum>
      <action>jumpToMark</action>
    </textRepeatAssign>
    <textRepeatAssign cmper="2" inci="0">
      <repnum>2</repnum>
      <action>noJump</action>
    </textRepeatAssign>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, 1),
        document->getOthers()->get<others::TextRepeatAssign>(SCORE_PARTID, 1, 0) };
}

static jump::Jump classifyTestJump(const std::string& text)
{
    const auto context = makeTextRepeatContext(text);
    return classifyJump(context.def).visual;
}

static jump::Jump classifyTestJumpWithMaestro(const std::string& text)
{
    const auto context = makeTextRepeatContext(text, "Maestro", 4095);
    return classifyJump(context.def).visual;
}

} // namespace

TEST(JumpClassification, ClassifiesText)
{
    EXPECT_EQ(classifyTestJump("D.C. al Fine"), jump::Jump::DCAlFine);
    EXPECT_EQ(classifyTestJump("D.C. al Coda"), jump::Jump::DCAlCoda);
    EXPECT_EQ(classifyTestJump("D.S. al Fine"), jump::Jump::DsAlFine);
    EXPECT_EQ(classifyTestJump("D.S. al Coda"), jump::Jump::DsAlCoda);
    EXPECT_EQ(classifyTestJump("to coda #"), jump::Jump::ToCoda);
    EXPECT_EQ(classifyTestJump("segno"), jump::Jump::Segno);
    EXPECT_EQ(classifyTestJump("coda"), jump::Jump::Coda);
    EXPECT_EQ(classifyTestJump("to coda"), jump::Jump::ToCoda);
    EXPECT_EQ(classifyTestJump("Fine"), jump::Jump::Fine);
}

TEST(JumpClassification, ClassifiesUnicodeSymbols)
{
    EXPECT_EQ(classifyTestJump("§"), jump::Jump::Segno);
    EXPECT_EQ(classifyTestJump("𝄋"), jump::Jump::Segno);
    EXPECT_EQ(classifyTestJump("𝄌"), jump::Jump::Coda);
}

TEST(JumpClassification, ClassifiesLegacyGlyphs)
{
    EXPECT_EQ(classifyTestJumpWithMaestro("%"), jump::Jump::Segno);
    EXPECT_EQ(classifyTestJumpWithMaestro("\xC3\x9E"), jump::Jump::Coda);
}

TEST(JumpClassification, UnknownTextReturnsNone)
{
    EXPECT_EQ(classifyTestJump("Allegro"), jump::Jump::None);
}

TEST(JumpClassification, ClassifiesPlaybackFromRepeatAction)
{
    const auto stopContext = makeTextRepeatContext("Coda", "Times New Roman", 0, "stop");
    const auto stopClassification = classifyJump(stopContext.assignment);
    EXPECT_EQ(stopClassification.visual, jump::Jump::Coda);
    EXPECT_EQ(stopClassification.playback, jump::Jump::Fine);

    const auto jumpToMarkContext = makeJumpToMarkContext();
    const auto jumpToMarkClassification = classifyJump(jumpToMarkContext.assignment);
    EXPECT_EQ(jumpToMarkClassification.visual, jump::Jump::None);
    EXPECT_EQ(jumpToMarkClassification.playback, jump::Jump::DalSegno);
}
