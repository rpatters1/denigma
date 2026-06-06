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
    <fontName cmper="2">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Maestro</name>
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

TEST(MnxFormattedText, PreservesMixedSmuflTextByDefault)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)p f");

    const auto formatted = makeFormattedText(ctx.parsingContext);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "text": "p f",
            "style": {
                "font": "Maestro",
                "size": 24.0
            }
        }
    ])json"));
}

TEST(MnxFormattedText, OmitsLegacyFontStyleFromSmuflGlyphs)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)pf");

    const auto formatted = makeFormattedText(ctx.parsingContext);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "type": "smufl",
            "glyphs": ["dynamicPiano", "dynamicForte"]
        }
    ])json"));
}

TEST(MnxFormattedText, SplitsSmuflGlyphsFromMixedTextWhenRequested)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)p f");
    MnxFormattedTextOptions options;
    options.symbolPolicy = MnxFormattedTextSymbolPolicy::SplitSmufl;

    const auto formatted = makeFormattedText(ctx.parsingContext, options);

    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "type": "smufl",
            "glyphs": ["dynamicPiano"]
        },
        {
            "text": " ",
            "style": {
                "font": "Maestro",
                "size": 24.0
            }
        },
        {
            "type": "smufl",
            "glyphs": ["dynamicForte"]
        }
    ])json"));
}

TEST(MnxFormattedText, CallsOptionalChunkCallback)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)Text ^fontid(2)^size(24)^nfx(0)pf");
    std::vector<std::string> chunks;
    std::vector<std::vector<std::string>> glyphs;
    MnxFormattedTextOptions options;
    options.onChunk = [&](const std::string& chunk, const std::vector<std::string>& chunkGlyphs) {
        chunks.push_back(chunk);
        glyphs.push_back(chunkGlyphs);
    };

    const auto formatted = makeFormattedText(ctx.parsingContext, options);

    EXPECT_EQ(chunks, (std::vector<std::string>{ "Text ", "pf" }));
    EXPECT_EQ(glyphs, (std::vector<std::vector<std::string>>{ {}, { "dynamicPiano", "dynamicForte" } }));
    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "text": "Text ",
            "style": {
                "font": "Times New Roman",
                "size": 12.0
            }
        },
        {
            "type": "smufl",
            "glyphs": ["dynamicPiano", "dynamicForte"]
        }
    ])json"));
}

TEST(MnxFormattedText, ChunkCallbackPreservesTextPolicy)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)pf");
    std::vector<std::string> chunks;
    std::vector<std::vector<std::string>> glyphs;
    MnxFormattedTextOptions options;
    options.symbolPolicy = MnxFormattedTextSymbolPolicy::PreserveText;
    options.onChunk = [&](const std::string& chunk, const std::vector<std::string>& chunkGlyphs) {
        chunks.push_back(chunk);
        glyphs.push_back(chunkGlyphs);
    };

    const auto formatted = makeFormattedText(ctx.parsingContext, options);

    EXPECT_EQ(chunks, (std::vector<std::string>{ "pf" }));
    EXPECT_EQ(glyphs, (std::vector<std::vector<std::string>>{ {} }));
    EXPECT_EQ(mnx::json::parse(formatted.dump()), mnx::json::parse(R"json([
        {
            "text": "pf",
            "style": {
                "font": "Maestro",
                "size": 24.0
            }
        }
    ])json"));
}

TEST(MnxFormattedText, ChunkCallbackSplitsSmuflPolicy)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)p f");
    std::vector<std::string> chunks;
    std::vector<std::vector<std::string>> glyphs;
    MnxFormattedTextOptions options;
    options.symbolPolicy = MnxFormattedTextSymbolPolicy::SplitSmufl;
    options.onChunk = [&](const std::string& chunk, const std::vector<std::string>& chunkGlyphs) {
        chunks.push_back(chunk);
        glyphs.push_back(chunkGlyphs);
    };

    static_cast<void>(makeFormattedText(ctx.parsingContext, options));

    EXPECT_EQ(chunks, (std::vector<std::string>{ "p", " ", "f" }));
    EXPECT_EQ(glyphs, (std::vector<std::vector<std::string>>{
        { "dynamicPiano" },
        {},
        { "dynamicForte" }
    }));
}
