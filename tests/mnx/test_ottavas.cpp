/*
 * Copyright (C) 2025, Robert Patterson
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
#include <filesystem>
#include <iterator>
#include <fstream>
#include <memory>

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;

TEST(MnxOttavasTest, OverlappingOttavas)
{
    std::filesystem::path inputPath;
    copyInputToOutput("ottavas.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "ottavas.mnx");
    auto parts = doc.parts();
    ASSERT_FALSE(parts.empty()) << "no parts in document";
    ASSERT_TRUE(parts[0].measures().has_value());
    auto measures = parts[0].measures().value();
    ASSERT_GE(measures.size(), 2);

    // measure 1
    {
        std::vector<mnx::OttavaAmount> expectOttavaType = { mnx::OttavaAmount::TwoOctaveUp, mnx::OttavaAmount::OctaveDown };
        std::vector<int> expectOttavaEndMeasure = { 2, 2 };
        std::vector<musx::util::Fraction> expectOttaveEndPosition = { musx::util::Fraction(1, 4), musx::util::Fraction(1, 4) };
        std::vector<int> expectedNoteOctaves = { 7, 7, 6, 6 };
        auto measure = measures[0];
        size_t ottavaIndex = 0;
        size_t eventIndex = 0;
        ASSERT_GE(measure.sequences().size(), 1);
        for (const auto& content : measure.sequences()[0].content()) {
            if (content.type() == mnx::sequence::Ottava::ContentTypeValue) {
                ASSERT_LT(ottavaIndex, expectOttavaType.size());
                auto ottava = content.get<mnx::sequence::Ottava>();
                EXPECT_EQ(ottava.value(), expectOttavaType[ottavaIndex]);
                EXPECT_EQ(ottava.end().measure(), expectOttavaEndMeasure[ottavaIndex]);
                EXPECT_EQ(ottava.end().position().fraction().numerator(), expectOttaveEndPosition[ottavaIndex].numerator());
                EXPECT_EQ(ottava.end().position().fraction().denominator(), expectOttaveEndPosition[ottavaIndex].denominator());
                ottavaIndex++;
            } else if (content.type() == mnx::sequence::Event::ContentTypeValue) {
                ASSERT_LT(ottavaIndex, expectedNoteOctaves.size());
                auto event = content.get<mnx::sequence::Event>();
                ASSERT_TRUE(event.notes().has_value());
                ASSERT_GE(event.notes().value().size(), 1);
                EXPECT_EQ(event.notes().value()[0].pitch().octave(), expectedNoteOctaves[eventIndex]);
                eventIndex++;
            }
        }
    }
    // measure 2
    {
        std::vector<mnx::OttavaAmount> expectOttavaType = { };
        std::vector<int> expectOttavaEndMeasure = { };
        std::vector<musx::util::Fraction> expectOttaveEndPosition = { };
        std::vector<int> expectedNoteOctaves = { 6, 6, 5 };
        auto measure = measures[1];
        size_t ottavaIndex = 0;
        size_t eventIndex = 0;
        ASSERT_GE(measure.sequences().size(), 1);
        for (const auto& content : measure.sequences()[0].content()) {
            if (content.type() == mnx::sequence::Ottava::ContentTypeValue) {
                ASSERT_LT(ottavaIndex, expectOttavaType.size());
                auto ottava = content.get<mnx::sequence::Ottava>();
                EXPECT_EQ(ottava.value(), expectOttavaType[ottavaIndex]);
                EXPECT_EQ(ottava.end().measure(), expectOttavaEndMeasure[ottavaIndex]);
                EXPECT_EQ(ottava.end().position().fraction().numerator(), expectOttaveEndPosition[ottavaIndex].numerator());
                EXPECT_EQ(ottava.end().position().fraction().denominator(), expectOttaveEndPosition[ottavaIndex].denominator());
                ottavaIndex++;
            } else if (content.type() == mnx::sequence::Event::ContentTypeValue) {
                ASSERT_LT(ottavaIndex, expectedNoteOctaves.size());
                auto event = content.get<mnx::sequence::Event>();
                ASSERT_TRUE(event.notes().has_value());
                ASSERT_GE(event.notes().value().size(), 1);
                EXPECT_EQ(event.notes().value()[0].pitch().octave(), expectedNoteOctaves[eventIndex]);
                eventIndex++;
            }
        }
    }
}

TEST(MnxOttavasTest, EndOfBar)
{
    std::filesystem::path inputPath;
    copyInputToOutput("ottava_end_of_bar.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!was not inserted into MNX document" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "ottava_end_of_bar.mnx");
    auto parts = doc.parts();
    ASSERT_FALSE(parts.empty()) << "no parts in document";
    ASSERT_TRUE(parts[0].measures().has_value());
    auto measures = parts[0].measures().value();

    // check to see that ottava at the end of m1 in musx is now the beginning of m2 in mnx.
    ASSERT_GE(measures.size(), 2u);
    ASSERT_FALSE(measures[1].sequences().empty());
    auto sequence = measures[1].sequences()[0];
    ASSERT_FALSE(sequence.content().empty());
    auto content = sequence.content()[0];
    ASSERT_EQ(content.type(), "ottava");
    auto ottava = content.get<mnx::sequence::Ottava>();
    EXPECT_EQ(ottava.end().measure(), 2);
    EXPECT_EQ(ottava.end().position().fraction().numerator(), 0);
}