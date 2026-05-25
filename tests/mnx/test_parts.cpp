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

namespace {

void checkMeasure1MidMeasureClefs(const mnx::Part& part)
{
    auto measures = part.measures();
    ASSERT_GE(measures.size(), 1);

    for (size_t i = 1; i < measures.size(); ++i) {
        EXPECT_FALSE(measures[i].clefs().has_value()) << measures[i].dump(4);
    }

    auto measure1 = measures[0];
    ASSERT_TRUE(measure1.clefs().has_value()) << measure1.dump(4);

    auto clefs = measure1.clefs().value();
    std::vector<size_t> staff1Clefs;
    for (size_t i = 0; i < clefs.size(); ++i) {
        if (clefs[i].staff() == 1) {
            staff1Clefs.push_back(i);
        }
    }
    ASSERT_EQ(staff1Clefs.size(), 3) << measure1.dump(4);

    auto clef0 = clefs[staff1Clefs[0]];
    EXPECT_EQ(clef0.clef().sign(), mnx::ClefSign::GClef);
    EXPECT_FALSE(clef0.position().has_value());

    auto clef1 = clefs[staff1Clefs[1]];
    EXPECT_EQ(clef1.clef().sign(), mnx::ClefSign::FClef);
    ASSERT_TRUE(clef1.position().has_value());
    EXPECT_EQ(clef1.position()->fraction().numerator(), 1);
    EXPECT_EQ(clef1.position()->fraction().denominator(), 8);

    auto clef2 = clefs[staff1Clefs[2]];
    EXPECT_EQ(clef2.clef().sign(), mnx::ClefSign::CClef);
    ASSERT_TRUE(clef2.position().has_value());
    EXPECT_EQ(clef2.position()->fraction().numerator(), 1);
    EXPECT_EQ(clef2.position()->fraction().denominator(), 2);

    std::vector<size_t> staff2Clefs;
    for (size_t i = 0; i < clefs.size(); ++i) {
        if (clefs[i].staff() == 2) {
            staff2Clefs.push_back(i);
        }
    }
    ASSERT_EQ(staff2Clefs.size(), 1) << measure1.dump(4);

    auto staff2Clef = clefs[staff2Clefs[0]];
    EXPECT_FALSE(staff2Clef.position().has_value());
}

void checkMeas1MidMeasureClefExport(bool splitInstruments)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("meas1_midmeasureclef.musx", inputPath);

    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    if (splitInstruments) {
        args.add("--split-instruments");
    }
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "meas1_midmeasureclef.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    checkMeasure1MidMeasureClefs(parts[0]);

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / "meas1_midmeasureclef.mnx", mnx);
    if (splitInstruments) {
        EXPECT_FALSE(mnx.contains("scores")) << mnx.dump(4);
        EXPECT_FALSE(mnx.contains("layouts")) << mnx.dump(4);
    } else {
        EXPECT_TRUE(mnx.contains("scores")) << mnx.dump(4);
        EXPECT_TRUE(mnx.contains("layouts")) << mnx.dump(4);
    }
}

} // namespace


TEST(MnxParts, MultiInstrumentTest)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("piano3staff.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
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
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "forced_bass_clef_smufl.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);

    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 2);
    auto measure2 = measures[1];

    /// check whole rest position
    auto m2seqs = measure2.sequences();
    ASSERT_GE(m2seqs.size(), 2);
    auto layer1 = m2seqs[0];
    EXPECT_EQ(layer1.content().size(), 0u);
    ASSERT_TRUE(layer1.fullMeasure().has_value());
    auto fullMeasure = layer1.fullMeasure().value();
    ASSERT_TRUE(fullMeasure.staffPosition().has_value());
    EXPECT_EQ(fullMeasure.staffPosition().value(), 4);

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

TEST(MnxParts, Measure1MidMeasureClefs)
{
    checkMeas1MidMeasureClefExport(false);
}

TEST(MnxParts, Measure1MidMeasureClefsSplitInstruments)
{
    checkMeas1MidMeasureClefExport(true);
}

TEST(MnxParts, PartiallyHiddenCue)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("multimeas_cue.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error", "!Semantic validation errors" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });
}
