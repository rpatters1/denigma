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

#include "denigma/classify/clefs.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct ClefDefContext
{
    DocumentPtr document;
    MusxInstance<options::ClefOptions::ClefDef> def;
    MusxInstance<others::Staff> staff;
};

static ClefDefContext makeClefDefContext(int middleCPos, char32_t clefChar, int staffPosition, bool percussionStaff = false)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <options>
    <fontOptions>
      <font type="clef">
        <fontSize>24</fontSize>
      </font>
    </fontOptions>
    <clefOptions>
      <clefDef index="0">
        <adjust>)xml";
    xml += std::to_string(middleCPos);
    xml += R"xml(</adjust>
        <clefChar>)xml";
    xml += std::to_string(static_cast<uint32_t>(clefChar));
    xml += R"xml(</clefChar>
        <clefYDisp>)xml";
    xml += std::to_string(staffPosition);
    xml += R"xml(</clefYDisp>
      </clefDef>
    </clefOptions>
  </options>
  <others>
)xml";
    if (percussionStaff) {
        xml += R"xml(    <staffSpec cmper="1">
      <notationStyle>percussion</notationStyle>
      <staffLines>5</staffLines>
    </staffSpec>
    <drumStaff cmper="1">
      <whichDrumLib>1</whichDrumLib>
    </drumStaff>
)xml";
    }
    xml += R"xml(    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Maestro</name>
    </fontName>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(buffer);
    return {
        document,
        document->getOptions()->get<options::ClefOptions>()->getClefDef(0),
        percussionStaff ? document->getOthers()->get<others::Staff>(SCORE_PARTID, 1) : nullptr
    };
}

} // namespace

TEST(ClefClassification, ClassifiesStandardClefInfo)
{
    const auto context = makeClefDefContext(-10, 38, -6);
    const auto classification = classifyClef(context.def);

    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.type, music_theory::ClefType::G);
    EXPECT_EQ(classification.octave, 0);
    EXPECT_FALSE(classification.isBlank);
    EXPECT_TRUE(classification.showOctave);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "gClef");
}

TEST(ClefClassification, ClassifiesBlankClefInfo)
{
    const auto context = makeClefDefContext(-10, 0, -6);
    const auto classification = classifyClef(context.def);

    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.type, music_theory::ClefType::G);
    EXPECT_EQ(classification.octave, 0);
    EXPECT_TRUE(classification.isBlank);
    EXPECT_TRUE(classification.showOctave);
    EXPECT_FALSE(classification.glyphName);
}

TEST(ClefClassification, ClassifiesBlankPercussionClefInfo)
{
    const auto context = makeClefDefContext(-10, 0, -6, true);
    const auto classification = classifyClef(context.def, context.staff);

    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.type, music_theory::ClefType::Percussion1);
    EXPECT_EQ(classification.octave, 0);
    EXPECT_TRUE(classification.isBlank);
    EXPECT_TRUE(classification.showOctave);
    EXPECT_FALSE(classification.glyphName);
}

TEST(ClefClassification, HidesOctaveForTransposingPlainClefGlyph)
{
    const auto context = makeClefDefContext(-3, 38, -6);
    const auto classification = classifyClef(context.def);

    EXPECT_EQ(classification.type, music_theory::ClefType::G);
    EXPECT_EQ(classification.octave, -1);
    EXPECT_FALSE(classification.showOctave);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "gClef");
}

TEST(ClefClassification, ShowsOctaveForTransposingOctaveClefGlyph)
{
    const auto context = makeClefDefContext(-3, 86, -6);
    const auto classification = classifyClef(context.def);

    EXPECT_EQ(classification.type, music_theory::ClefType::G);
    EXPECT_EQ(classification.octave, -1);
    EXPECT_TRUE(classification.showOctave);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_NE(classification.glyphName.value(), "gClef");
}
