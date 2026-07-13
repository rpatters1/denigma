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
#include "core/denigma.h"
#include "mnxdom.h"
#include "test_utils.h"

using namespace denigma;

TEST(MnxOttavasTest, OverlappingOttavas)
{
    std::filesystem::path inputPath;
    copyInputToOutput("ottavas.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "ottavas.mnx");
    auto parts = doc.parts();
    ASSERT_FALSE(parts.empty()) << "no parts in document";
    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 2);

    // measure 1
    {
        std::vector<mnx::OttavaAmount> expectOttavaType = { mnx::OttavaAmount::TwoOctavesUp, mnx::OttavaAmount::OctaveDown };
        std::vector<std::string> expectOttavaEndMeasure = { "m2", "m2" };
        std::vector<musx::util::Fraction> expectOttaveEndPosition = { musx::util::Fraction(1, 4), musx::util::Fraction(1, 4) };
        std::vector<int> expectedNoteOctaves = { 7, 7, 6, 6 };
        auto measure = measures[0];
        size_t ottavaIndex = 0;
        size_t eventIndex = 0;
        ASSERT_GE(measure.sequences().size(), 1);
        ASSERT_TRUE(measure.ottavas().has_value());
        ASSERT_GE(measure.ottavas().value().size(), 2);
        auto ottavas = measure.ottavas().value();
        for (const auto& ottava : ottavas) {
            ASSERT_LT(ottavaIndex, expectOttavaType.size());
            EXPECT_EQ(ottava.value(), expectOttavaType[ottavaIndex]);
            EXPECT_EQ(ottava.end().measure(), expectOttavaEndMeasure[ottavaIndex]);
            EXPECT_EQ(ottava.end().position().fraction().numerator(), expectOttaveEndPosition[ottavaIndex].numerator());
            EXPECT_EQ(ottava.end().position().fraction().denominator(), expectOttaveEndPosition[ottavaIndex].denominator());
            ottavaIndex++;
        }
        for (const auto& content : measure.sequences()[0].content()) {
            if (content.type() == mnx::sequence::Event::ContentTypeValue) {
                ASSERT_LT(eventIndex, expectedNoteOctaves.size());
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
        size_t eventIndex = 0;
        ASSERT_GE(measure.sequences().size(), 1);
        EXPECT_FALSE(measure.ottavas());
        for (const auto& content : measure.sequences()[0].content()) {
            if (content.type() == mnx::sequence::Event::ContentTypeValue) {
                ASSERT_LT(eventIndex, expectedNoteOctaves.size());
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
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!was not inserted into MNX document" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "ottava_end_of_bar.mnx");
    auto parts = doc.parts();
    ASSERT_FALSE(parts.empty()) << "no parts in document";
    auto measures = parts[0].measures();

    // check to see that ottava at the end of m1 in musx is at end of m1 and ends at beginning of m2
    ASSERT_GE(measures.size(), 2);
    ASSERT_TRUE(measures[0].ottavas().has_value());
    ASSERT_GE(measures[0].ottavas().value().size(), 1);
    auto ottava = measures[0].ottavas().value()[0];
    EXPECT_EQ(ottava.position().fraction().numerator(), 1);
    EXPECT_EQ(ottava.position().fraction().denominator(), 1);
    EXPECT_EQ(ottava.end().measure(), "m2");
    EXPECT_EQ(ottava.end().position().fraction().numerator(), 0);
    EXPECT_EQ(ottava.end().position().fraction().denominator(), 1);
}

TEST(MnxOttavasTest, ClassifiedOttavaCarriers)
{
    std::filesystem::path inputPath;
    copyInputToOutput("smartshape_lines.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "smartshape_lines.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 3u);

    auto expectSingleOttava = [](auto measure, mnx::OttavaAmount amount,
                                 const std::string& endMeasure, int endNumerator, int endDenominator) {
        ASSERT_TRUE(measure.ottavas().has_value());
        ASSERT_EQ(measure.ottavas().value().size(), 1u);
        auto ottava = measure.ottavas().value()[0];
        EXPECT_EQ(ottava.value(), amount);
        EXPECT_EQ(ottava.end().measure(), endMeasure);
        EXPECT_EQ(ottava.end().position().fraction().numerator(), endNumerator);
        EXPECT_EQ(ottava.end().position().fraction().denominator(), endDenominator);
    };
    auto noteOctaves = [](auto measure) {
        std::vector<int> result;
        for (const auto& content : measure.sequences()[0].content()) {
            if (content.type() == mnx::sequence::Event::ContentTypeValue) {
                auto event = content.template get<mnx::sequence::Event>();
                if (event.notes().has_value()) {
                    auto notes = event.notes().value();
                    for (const auto& note : notes) {
                        result.push_back(note.pitch().octave());
                    }
                }
            }
        }
        return result;
    };

    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 28u);

    // The hidden quindicesima (m14-m23) is the semantic carrier. Its two visual
    // segments are appearance-only and must not emit additional ottavas.
    expectSingleOttava(measures[13], mnx::OttavaAmount::TwoOctavesUp, "m23", 1, 1);
    EXPECT_FALSE(measures[14].ottavas().has_value());
    EXPECT_FALSE(measures[15].ottavas().has_value());
    EXPECT_EQ(noteOctaves(measures[13]), (std::vector<int>{ 6, 6, 6, 7 }));
    EXPECT_EQ(noteOctaves(measures[17]), (std::vector<int>{ 6, 6, 6, 7 }));

    // The unpaired "8vb" line inside the quindicesima is its own carrier; where the
    // two overlap, their displacements sum.
    expectSingleOttava(measures[18], mnx::OttavaAmount::OctaveDown, "m20", 3, 4);
    EXPECT_EQ(noteOctaves(measures[19]), (std::vector<int>{ 5, 5, 5, 6 }));

    // The canonical pair: the hidden octaveUp emits (despite hidden); its visible
    // custom line does not.
    expectSingleOttava(measures[25], mnx::OttavaAmount::OctaveUp, "m28", 1, 4);
    EXPECT_EQ(noteOctaves(measures[26]), (std::vector<int>{ 5, 5, 5, 6 }));
    EXPECT_EQ(noteOctaves(measures[27]), (std::vector<int>{ 5, 5, 4, 5 })); // ottava ends after beat 1

    // The unpaired "8vb" line on staff 3 is a carrier.
    auto staff3Measures = parts[2].measures();
    ASSERT_GE(staff3Measures.size(), 23u);
    expectSingleOttava(staff3Measures[20], mnx::OttavaAmount::OctaveDown, "m23", 1, 4);
    EXPECT_EQ(noteOctaves(staff3Measures[21]), (std::vector<int>{ 3, 3, 3, 4 }));
}
