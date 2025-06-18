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

#include "gtest/gtest.h"
#include "denigma.h"
#include "test_utils.h"

using namespace denigma;


TEST(MnxParts, MultiInstrumentTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("piano3staff.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "piano3staff.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 3);

    EXPECT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0].name(), "Piccolo");
    EXPECT_EQ(parts[1].name(), "Cello");
    EXPECT_EQ(parts[2].name(), "Piano");
}

TEST(MnxParts, ForcedClef)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("forced_bass_clef_smufl.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "forced_bass_clef_smufl.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    ASSERT_TRUE(parts[0].measures().has_value());

    auto measures = parts[0].measures().value();
    ASSERT_GE(measures.size(), 2);
    auto measure2 = measures[1];

    /// check whole rest position
    auto m2seqs = measure2.sequences();
    ASSERT_GE(m2seqs.size(), 2);
    auto m2contentLayer1 = m2seqs[0].content();
    ASSERT_GE(m2contentLayer1.size(), 1);
    EXPECT_EQ(m2contentLayer1.size(), 1);
    auto measureRest = m2contentLayer1[0];
    ASSERT_EQ(measureRest.type(), mnx::sequence::Event::ContentTypeValue);
    auto measureRestAsEvent = m2contentLayer1[0].get<mnx::sequence::Event>();
    EXPECT_TRUE(measureRestAsEvent.measure());
    ASSERT_TRUE(measureRestAsEvent.rest().has_value());
    auto rest = measureRestAsEvent.rest().value();
    ASSERT_TRUE(rest.staffPosition().has_value());
    EXPECT_EQ(rest.staffPosition().value(), 4);

    /// check clefs
    ASSERT_TRUE(measure2.clefs().has_value()) << measure2.dump(4);
    auto clefs = measure2.clefs().value();
    ASSERT_GE(clefs.size(), 1);
    EXPECT_EQ(clefs.size(), 1);
    auto clef = clefs[0];
    EXPECT_EQ(clef.clef().sign(), mnx::ClefSign::FClef);
    EXPECT_EQ(clef.clef().octave(), mnx::OttavaAmountOrZero::NoTransposition);
    EXPECT_FALSE(clef.position().has_value());
}