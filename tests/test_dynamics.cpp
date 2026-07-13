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
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
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

static TextExpressionContext makeTextExpressionContext(const std::string& text)
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
    <textBlock cmper="1">
      <textID>1</textID>
      <textTag>expression</textTag>
    </textBlock>
    <textExprDef cmper="1">
      <textIDKey>1</textIDKey>
    </textExprDef>
  </others>
  <texts>
    <expression number="1">)xml";
    xml += text;
    xml += R"xml(</expression>
  </texts>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return { document, document->getOthers()->get<others::TextExpressionDef>(SCORE_PARTID, 1) };
}

static std::vector<musx::util::EnigmaTextChunk> collectChunks(const TextExpressionContext& context)
{
    auto chunks = context.def->getRawTextCtx(SCORE_PARTID).collectEnigmaTextChunks(
        musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    std::erase_if(chunks, [](const musx::util::EnigmaTextChunk& chunk) {
        return chunk.text.empty() || !chunk.styles.font || chunk.styles.font->hidden;
    });
    return chunks;
}

static std::optional<dynamics::Mark> classifyTestDynamic(const std::string& text, bool forceOther = false)
{
    const auto context = makeTextExpressionContext(text);
    const auto chunks = collectChunks(context);
    if (chunks.size() != 1) {
        ADD_FAILURE() << "Expected one chunk but got " << chunks.size();
        return std::nullopt;
    }
    return classifyDynamicRun(chunks.front(), forceOther);
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

TEST(DynamicRunClassification, ClassifiesCoreDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppppp")->dynamic, dynamics::Dynamic::pppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppppp")->dynamic, dynamics::Dynamic::ppppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pppp")->dynamic, dynamics::Dynamic::pppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ppp")->dynamic, dynamics::Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pp")->dynamic, dynamics::Dynamic::pp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)p")->dynamic, dynamics::Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mp")->dynamic, dynamics::Dynamic::mp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mf")->dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)f")->dynamic, dynamics::Dynamic::f);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ff")->dynamic, dynamics::Dynamic::ff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fff")->dynamic, dynamics::Dynamic::fff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffff")->dynamic, dynamics::Dynamic::ffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffff")->dynamic, dynamics::Dynamic::fffff);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffffff")->dynamic, dynamics::Dynamic::ffffff);
}

TEST(DynamicRunClassification, ClassifiesAccentDynamics)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fp")->dynamic, dynamics::Dynamic::fp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffp")->dynamic, dynamics::Dynamic::ffp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fz")->dynamic, dynamics::Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)ffz")->dynamic, dynamics::Dynamic::ffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)pf")->dynamic, dynamics::Dynamic::pf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sf")->dynamic, dynamics::Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfp")->dynamic, dynamics::Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfpp")->dynamic, dynamics::Dynamic::sfpp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfz")->dynamic, dynamics::Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sffz")->dynamic, dynamics::Dynamic::sffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sfzp")->dynamic, dynamics::Dynamic::sfzp);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rf")->dynamic, dynamics::Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rfz")->dynamic, dynamics::Dynamic::rfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)n")->dynamic, dynamics::Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)niente")->dynamic, dynamics::Dynamic::n);
}

TEST(DynamicRunClassification, ClassifiesAliases)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)forzando")->dynamic, dynamics::Dynamic::fz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzando")->dynamic, dynamics::Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzato")->dynamic, dynamics::Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)sforzado")->dynamic, dynamics::Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinforzando")->dynamic, dynamics::Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)rinf.")->dynamic, dynamics::Dynamic::rf);
}

TEST(DynamicRunClassification, ClassifiesLegacyGlyphs)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)p")->dynamic, dynamics::Dynamic::p);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)ppp")->dynamic, dynamics::Dynamic::ppp);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)F")->dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(classifyTestDynamic("^fontid(2)^size(24)^nfx(0)Z")->dynamic, dynamics::Dynamic::fz);
}

TEST(DynamicRunClassification, ClassifiesSmuflGlyphs)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52B;")->dynamic, dynamics::Dynamic::pp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52E;")->dynamic, dynamics::Dynamic::pf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53A;")->dynamic, dynamics::Dynamic::sfzp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE526;")->dynamic, dynamics::Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE541;")->dynamic, dynamics::Dynamic::n);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE520;")->dynamic, dynamics::Dynamic::ffp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE525;")->dynamic, dynamics::Dynamic::ffz);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE524;&#xE522;")->dynamic, dynamics::Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE524;&#xE522;&#xE520;")->dynamic, dynamics::Dynamic::sfp);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE536;")->dynamic, dynamics::Dynamic::sf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE539;")->dynamic, dynamics::Dynamic::sfz);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53C;")->dynamic, dynamics::Dynamic::rf);
    EXPECT_EQ(classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE53D;")->dynamic, dynamics::Dynamic::rfz);
}

TEST(DynamicRunClassification, ReturnsMatchedGlyphNamesWhenAllMatchedCharactersResolveToGlyphs)
{
    const auto smuflSingle = classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52D;");
    ASSERT_TRUE(smuflSingle);
    EXPECT_EQ(smuflSingle->dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(smuflSingle->glyphs, (std::vector<std::string>{ "dynamicMF" }));

    const auto smuflCompound = classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE520;");
    ASSERT_TRUE(smuflCompound);
    EXPECT_EQ(smuflCompound->dynamic, dynamics::Dynamic::ffp);
    EXPECT_EQ(smuflCompound->glyphs, (std::vector<std::string>{ "dynamicFF", "dynamicPiano" }));

    const auto smuflAlias = classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE524;&#xE522;");
    ASSERT_TRUE(smuflAlias);
    EXPECT_EQ(smuflAlias->dynamic, dynamics::Dynamic::sf);
    EXPECT_EQ(smuflAlias->glyphs, (std::vector<std::string>{ "dynamicSforzando", "dynamicForte" }));

    const auto ascii = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)mf");
    ASSERT_TRUE(ascii);
    EXPECT_EQ(ascii->dynamic, dynamics::Dynamic::mf);
    EXPECT_TRUE(ascii->glyphs.empty());

    const auto legacy = classifyTestDynamic("^fontid(2)^size(24)^nfx(0)F");
    ASSERT_TRUE(legacy);
    EXPECT_EQ(legacy->dynamic, dynamics::Dynamic::mf);
    EXPECT_EQ(legacy->glyphs, (std::vector<std::string>{ "dynamicMF" }));
}

TEST(DynamicRunClassification, DistinguishesOtherAndNone)
{
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)fffffff")->dynamic, dynamics::Dynamic::Other);
    EXPECT_FALSE(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)nn"));
    EXPECT_FALSE(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce"));
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce", true)->dynamic, dynamics::Dynamic::Other);
}

TEST(DynamicRunClassification, PreservesGlyphsForOtherDynamics)
{
    const auto glyphOther = classifyTestDynamic("^fontid(0)^size(24)^nfx(0)&#xE52F;&#xE52F;&#xE52F;&#xE522;");
    ASSERT_TRUE(glyphOther);
    EXPECT_EQ(glyphOther->dynamic, dynamics::Dynamic::Other);
    EXPECT_EQ(glyphOther->glyphs, (std::vector<std::string>{ "dynamicFF", "dynamicFF", "dynamicFF", "dynamicForte" }));

    const auto categoryOther = classifyTestDynamic("^fontid(1)^size(12)^nfx(0)dolce", true);
    ASSERT_TRUE(categoryOther);
    EXPECT_EQ(categoryOther->dynamic, dynamics::Dynamic::Other);
    EXPECT_TRUE(categoryOther->glyphs.empty());
}

TEST(DynamicRunClassification, RequiresSpaceDelimitersForNonGlyphTokens)
{
    EXPECT_FALSE(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)G.P."));
    EXPECT_EQ(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)G.P.", true)->dynamic, dynamics::Dynamic::Other);
    EXPECT_FALSE(classifyTestDynamic("^fontid(1)^size(12)^nfx(0)cresc.mf"));
}

TEST(DynamicRunClassification, ProvidesCanonicalText)
{
    EXPECT_EQ(dynamicCanonicalText(dynamics::Dynamic::mf), "mf");
    EXPECT_EQ(dynamicCanonicalText(dynamics::Dynamic::sfz), "sfz");
    EXPECT_EQ(dynamicCanonicalText(dynamics::Dynamic::sfzp), "sfzp");
    EXPECT_EQ(dynamicCanonicalText(dynamics::Dynamic::n), "n");
    EXPECT_TRUE(dynamicCanonicalText(dynamics::Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalText(dynamics::Dynamic::None).empty());
}

TEST(DynamicRunClassification, ProvidesCanonicalGlyphs)
{
    const std::vector<dynamics::Dynamic> dynamics = {
        dynamics::Dynamic::pppppp, dynamics::Dynamic::ppppp, dynamics::Dynamic::pppp, dynamics::Dynamic::ppp, dynamics::Dynamic::pp, dynamics::Dynamic::p,
        dynamics::Dynamic::mp, dynamics::Dynamic::mf,
        dynamics::Dynamic::f, dynamics::Dynamic::ff, dynamics::Dynamic::fff, dynamics::Dynamic::ffff, dynamics::Dynamic::fffff, dynamics::Dynamic::ffffff,
        dynamics::Dynamic::fp, dynamics::Dynamic::ffp, dynamics::Dynamic::fz, dynamics::Dynamic::ffz, dynamics::Dynamic::pf,
        dynamics::Dynamic::sf, dynamics::Dynamic::sfp, dynamics::Dynamic::sfpp, dynamics::Dynamic::sfz, dynamics::Dynamic::sffz, dynamics::Dynamic::sfzp,
        dynamics::Dynamic::rf, dynamics::Dynamic::rfz, dynamics::Dynamic::n
    };

    for (const auto dynamic : dynamics) {
        const auto glyphs = dynamicCanonicalGlyphs(dynamic);
        EXPECT_FALSE(glyphs.empty()) << dynamicCanonicalText(dynamic);
        EXPECT_EQ(classifyTestDynamic(smuflGlyphText(glyphs))->dynamic, dynamic)
            << dynamicCanonicalText(dynamic);
    }

    EXPECT_TRUE(dynamicCanonicalGlyphs(dynamics::Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalGlyphs(dynamics::Dynamic::None).empty());
}

TEST(DynamicRunClassification, ProvidesCanonicalLetterGlyphs)
{
    EXPECT_EQ(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::mf), (std::vector<std::string>{ "dynamicMezzo", "dynamicForte" }));
    EXPECT_EQ(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::fffff), (std::vector<std::string>{
        "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte", "dynamicForte"
    }));
    EXPECT_EQ(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::sfpp), (std::vector<std::string>{
        "dynamicSforzando", "dynamicForte", "dynamicPiano", "dynamicPiano"
    }));
    EXPECT_EQ(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::rfz), (std::vector<std::string>{
        "dynamicRinforzando", "dynamicForte", "dynamicZ"
    }));
    EXPECT_TRUE(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::Other).empty());
    EXPECT_TRUE(dynamicCanonicalLetterGlyphs(dynamics::Dynamic::None).empty());
}

TEST(DynamicRunClassification, ConvertsDynamicGlyphsToLetters)
{
    EXPECT_EQ(dynamicGlyphsToLetters({ "dynamicFF" }), "ff");
    EXPECT_EQ(dynamicGlyphsToLetters({ "dynamicMF" }), "mf");
    EXPECT_EQ(dynamicGlyphsToLetters({ "dynamicSforzatoPiano" }), "sfzp");
    EXPECT_EQ(dynamicGlyphsToLetters({ "dynamicPPPP", "dynamicFortePiano" }), "ppppfp");
    EXPECT_EQ(dynamicGlyphsToLetters({ "dynamicForteSmall" }), "f");
    EXPECT_TRUE(dynamicGlyphsToLetters({ "unknownGlyph", "dynamicForte" }).empty());
}

TEST(DynamicRunClassification, ConvertsDynamicLettersToLetterGlyphs)
{
    EXPECT_EQ(dynamicLettersToLetterGlyphs("mfsrzn"), (std::vector<std::string>{
        "dynamicMezzo", "dynamicForte", "dynamicSforzando",
        "dynamicRinforzando", "dynamicZ", "dynamicNiente"
    }));
    EXPECT_EQ(dynamicLettersToLetterGlyphs("PPx F!"), (std::vector<std::string>{
        "dynamicPiano", "dynamicPiano", "dynamicForte"
    }));
    EXPECT_TRUE(dynamicLettersToLetterGlyphs("abc").empty());
}
