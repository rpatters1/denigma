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

#include "export/mnx_formatted_text.h"
#include "musx/musx.h"

using namespace denigma::mnxexp;
using namespace musx::dom;

namespace {

struct RawTextContext
{
    DocumentPtr document;
    MusxInstance<texts::BlockText> text;
    musx::util::EnigmaParsingContext parsingContext;
};

static RawTextContext makeRawTextContext(const std::string& enigmaText)
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
  </others>
  <texts>
    <blockText number="1">)xml";
    xml += enigmaText;
    xml += R"xml(</blockText>
  </texts>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
    auto text = document->getTexts()->get<texts::BlockText>(1);
    return { document, text, text->getRawTextCtx(text, SCORE_PARTID) };
}

} // namespace

TEST(MnxFormattedText, ConvertsStyledText)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(14)^nfx(3)Bold Italic");

    const auto formatted = makeFormattedText(ctx.parsingContext);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "text": "Bold Italic",
            "style": {
                "font": "Times New Roman",
                "size": 14.0,
                "fontStyle": "italic",
                "weight": "bold"
            }
        }
    ])json"));
}

TEST(MnxFormattedText, ReplacesExistingContent)
{
    auto firstCtx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)First");
    auto secondCtx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)Second");
    auto formatted = makeFormattedText(firstCtx.parsingContext);

    setFormattedText(formatted, secondCtx.parsingContext);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "text": "Second",
            "style": {
                "font": "Times New Roman",
                "size": 12.0
            }
        }
    ])json"));
}

TEST(MnxFormattedText, ConvertsSmuflGlyphs)
{
    auto ctx = makeRawTextContext("^fontid(0)^size(24)^nfx(0)&#xE520;");

    const auto formatted = makeFormattedText(ctx.parsingContext);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "type": "smufl",
            "glyphs": ["dynamicPiano"],
            "style": {
                "font": "Finale Maestro",
                "size": 24.0
            }
        }
    ])json"));
}
