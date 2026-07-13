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

#include <string>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/noteheads.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct FontContext
{
    DocumentPtr document;
    MusxInstance<FontInfo> fontInfo;
};

static FontContext makeFontContext(const std::string& fontName = "Maestro", int charsetVal = 4095)
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
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    auto fontInfo = std::make_shared<FontInfo>(document);
    fontInfo->fontId = 1;
    fontInfo->fontSize = 24;
    return { document, fontInfo };
}

} // namespace

TEST(NoteheadClassification, ClassifiesRegularNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A4); // noteheadBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Regular);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);
    ASSERT_TRUE(filled.glyphName);
    EXPECT_EQ(filled.glyphName.value(), "noteheadBlack");

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A3); // noteheadHalf
    EXPECT_EQ(unfilled.shape, notehead::Shape::Regular);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesNullNoteheads)
{
    const auto smuflContext = makeFontContext("Finale Maestro");
    auto smuflNull = classifyNoteheadSymbol(smuflContext.fontInfo, 0xE0A5); // noteheadNull
    EXPECT_EQ(smuflNull.shape, notehead::Shape::Null);
    EXPECT_EQ(smuflNull.fill, notehead::Fill::Unspecified);
    ASSERT_TRUE(smuflNull.glyphName);
    EXPECT_EQ(smuflNull.glyphName.value(), "noteheadNull");
    EXPECT_EQ(smuflNull.noteheadInfo.font, smuflContext.fontInfo);
    EXPECT_EQ(smuflNull.noteheadInfo.character, char32_t(0xE0A5));

    const auto textContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);
    auto space = classifyNoteheadSymbol(textContext.fontInfo, U' ');
    EXPECT_EQ(space.shape, notehead::Shape::Null);
    EXPECT_EQ(space.fill, notehead::Fill::Unspecified);
    EXPECT_FALSE(space.glyphName);
    EXPECT_EQ(space.noteheadInfo.font, textContext.fontInfo);
    EXPECT_EQ(space.noteheadInfo.character, U' ');
}

TEST(NoteheadClassification, ClassifiesDiamondNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0DB); // noteheadDiamondBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Diamond);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0DD); // noteheadDiamondWhite
    EXPECT_EQ(unfilled.shape, notehead::Shape::Diamond);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesXNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A9); // noteheadXBlack
    EXPECT_EQ(filled.shape, notehead::Shape::X);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A8); // noteheadXHalf
    EXPECT_EQ(unfilled.shape, notehead::Shape::X);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

#ifdef small
#undef small
#endif

TEST(NoteheadClassification, ClassifiesSlashNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto small = classifyNoteheadSymbol(fontContext.fontInfo, 0xE105); // noteheadSlashVerticalEndsSmall
    EXPECT_EQ(small.shape, notehead::Shape::SmallSlash);
    EXPECT_EQ(small.fill, notehead::Fill::Filled);

    auto large = classifyNoteheadSymbol(fontContext.fontInfo, 0xE100); // noteheadSlashVerticalEnds
    EXPECT_EQ(large.shape, notehead::Shape::LargeSlash);
    EXPECT_EQ(large.fill, notehead::Fill::Filled);

    auto largeUnfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE102); // noteheadSlashWhiteWhole
    EXPECT_EQ(largeUnfilled.shape, notehead::Shape::LargeSlash);
    EXPECT_EQ(largeUnfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesCircledNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0E4); // noteheadCircledBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Circled);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0E6); // noteheadCircledWhole
    EXPECT_EQ(unfilled.shape, notehead::Shape::Circled);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesLegacyMaestroGlyphs)
{
    // Default font context is "Maestro" with the Mac symbol charset -- a legacy symbol font.
    const auto fontContext = makeFontContext();

    auto regular = classifyNoteheadSymbol(fontContext.fontInfo, 207); // -> noteheadBlack
    EXPECT_EQ(regular.shape, notehead::Shape::Regular);
    EXPECT_EQ(regular.fill, notehead::Fill::Filled);

    auto diamond = classifyNoteheadSymbol(fontContext.fontInfo, 226); // -> noteheadDiamondBlack
    EXPECT_EQ(diamond.shape, notehead::Shape::Diamond);
    EXPECT_EQ(diamond.fill, notehead::Fill::Filled);

    auto crossNotehead = classifyNoteheadSymbol(fontContext.fontInfo, 192); // -> noteheadXBlack
    EXPECT_EQ(crossNotehead.shape, notehead::Shape::X);
    EXPECT_EQ(crossNotehead.fill, notehead::Fill::Filled);
}

TEST(NoteheadClassification, ClassifiesAsciiXInNonSymbolFontOnly)
{
    const auto nonSymbolContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);
    auto lower = classifyNoteheadSymbol(nonSymbolContext.fontInfo, U'x');
    EXPECT_EQ(lower.shape, notehead::Shape::X);
    EXPECT_EQ(lower.fill, notehead::Fill::Filled);
    EXPECT_FALSE(lower.glyphName);

    auto upper = classifyNoteheadSymbol(nonSymbolContext.fontInfo, U'X');
    EXPECT_EQ(upper.shape, notehead::Shape::X);

    // In a symbol font, codepoint 'X' (0x58) is a glyph index, not literal text -- must not match.
    const auto symbolContext = makeFontContext();
    auto notX = classifyNoteheadSymbol(symbolContext.fontInfo, U'X');
    EXPECT_FALSE(notX);
}

TEST(NoteheadClassification, ReturnsUnclassifiedForUnknownSymbol)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0x41); // 'A', not mapped
    EXPECT_FALSE(classification);
    EXPECT_EQ(classification.shape, notehead::Shape::Unclassified);
    EXPECT_EQ(classification.fill, notehead::Fill::Unspecified);
}

TEST(NoteheadClassification, ReturnsOtherForRecognizedButUncatalogedNoteheadGlyph)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    // noteheadSquareBlack is a real SMuFL notehead glyph, but not one of the shapes Denigma
    // specifically classifies.
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0B9);
    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.shape, notehead::Shape::Other);
    EXPECT_EQ(classification.fill, notehead::Fill::Unspecified);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "noteheadSquareBlack");
}

TEST(NoteheadClassification, ClassifyNoteheadSymbolPopulatesNoteheadInfo)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A4); // noteheadBlack
    EXPECT_EQ(classification.noteheadInfo.font, fontContext.fontInfo);
    EXPECT_EQ(classification.noteheadInfo.character, char32_t(0xE0A4));
    EXPECT_EQ(classification.noteheadInfo.percent, 100);
    EXPECT_EQ(classification.noteheadInfo.horzOffset, 0);
    EXPECT_EQ(classification.noteheadInfo.vertOffset, 0);
}

TEST(NoteheadClassification, ClassifyNoteheadReturnsUnclassifiedForInvalidNote)
{
    EXPECT_FALSE(classifyNotehead(NoteInfoPtr()));
}
