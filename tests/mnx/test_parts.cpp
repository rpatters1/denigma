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
#include "core/denigma.h"
#include "mnxdom.h"
#include "test_utils.h"

using namespace denigma;

namespace {

std::optional<unsigned> dynamicGraceIndex(const mnx::part::DynamicGroupBase& dynamic)
{
    if (dynamic.type() == mnx::part::DynamicImmediate::ContentTypeValue) {
        return dynamic.get<mnx::part::DynamicImmediate>().position().graceIndex();
    }
    if (dynamic.type() == mnx::part::DynamicRelative::ContentTypeValue) {
        return dynamic.get<mnx::part::DynamicRelative>().position().graceIndex();
    }
    if (dynamic.type() == mnx::part::DynamicAccent::ContentTypeValue) {
        return dynamic.get<mnx::part::DynamicAccent>().position().graceIndex();
    }
    ADD_FAILURE() << "Unexpected dynamic type: " << dynamic.type();
    return std::nullopt;
}

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

// clef_forced_after_barline.musx changes to bass clef in measure 5, then restates the same bass
// clef in measure 6 with Finale's forced ("Always") clef display. MNX has no counterpart to
// MusicXML's clef@additional, so the restatement can only be carried by emitting the clef again.
TEST(MnxParts, ForcedClefRestatesPrevailingClef)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("clef_forced_after_barline.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "clef_forced_after_barline.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);

    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 6);

    // Measure 5 is the clef change itself; measure 6 is the forced restatement of that clef.
    for (size_t measureIndex : { size_t(4), size_t(5) }) {
        SCOPED_TRACE("measure " + std::to_string(measureIndex + 1));
        auto measure = measures[measureIndex];
        ASSERT_TRUE(measure.clefs().has_value()) << measure.dump(4);
        auto clefs = measure.clefs().value();
        ASSERT_EQ(clefs.size(), 1);
        auto clef = clefs[0];
        EXPECT_EQ(clef.clef().sign(), mnx::ClefSign::FClef);
        EXPECT_EQ(clef.clef().octave(), mnx::OttavaAmountOrZero::NoTransposition);
        EXPECT_FALSE(clef.position().has_value());
    }
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

TEST(MnxParts, CueLayer)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("forced_bass_clef.musx", inputPath);
    // The cue notifications are verbose-only, so --verbose is required to observe them.
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx", "--cue-layer", "1", "--no-validate", "--verbose" };
    checkStderr(std::vector<std::string>{
        "discarded cue material detected by --cue-layer in measure 2, staff 1, layer 1; MNX does not currently support cues.",
        "discarded 1 cue frames because MNX does not currently support cues."
    }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx with cue layer: " << pathString(inputPath);
    });
}

TEST(MnxParts, MeasureRepeats)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("measure_repeats.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({
        "Processing", pathString(inputPath.filename()), "!validation error",
        // A 2-bar repeat in measure 2 would repeat measures 0 and 1, and one in measure 14 would
        // occupy a measure 15 that does not exist. Both are semantic violations in MNX.
        "has a 2-bar repeat in measure 2 that would reach back before the first measure",
        "has a 2-bar repeat in measure 14 that would extend past the last measure",
        // Measure 8 asks for a 1-bar repeat while already covered by the 2-bar repeat in measure 7.
        "has a 1-bar repeat in measure 8 that falls inside the 2-bar repeat beginning in measure 7"
    }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "measure_repeats.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    auto measures = parts[0].measures();
    ASSERT_EQ(measures.size(), 14);

    // Finale flags every measure a repeat region covers, but MNX declares a repeat only on the first
    // measure of each group. The 2-bar region over measures 2..4 is an odd number of measures, so its
    // second group begins in measure 4 and takes measure 5, which carries no alternate notation, as
    // its second measure.
    const std::array<std::optional<int>, 14> expectedRepeats = {
        std::nullopt,       // m1
        std::nullopt,       // m2: dropped, reaches back before the start
        std::nullopt,       // m3: second measure of the group beginning in m2
        2,                  // m4
        std::nullopt,       // m5: second measure of the m4 group, unflagged in Finale
        std::nullopt,       // m6
        2,                  // m7
        std::nullopt,       // m8: its own 1-bar repeat is dropped, see above
        1,                  // m9
        2,                  // m10
        std::nullopt,       // m11
        2,                  // m12
        std::nullopt,       // m13
        std::nullopt        // m14: dropped, extends past the end
    };
    for (size_t x = 0; x < expectedRepeats.size(); x++) {
        const auto measureRepeat = measures[x].measureRepeat();
        const std::string label = "measure " + std::to_string(x + 1);
        if (!expectedRepeats[x]) {
            EXPECT_FALSE(measureRepeat.has_value()) << label << " should have no measure repeat";
            continue;
        }
        ASSERT_TRUE(measureRepeat.has_value()) << label << " should have a measure repeat";
        EXPECT_EQ(measureRepeat->number(), expectedRepeats[x].value()) << label;
    }

    // Alternate notation replaces the entries of the layers it hides, so those layers are not
    // exported. The fixture applies repeat styles both with and without "hide other layers", so a
    // measure is either emptied entirely or keeps the layers the style leaves alone. Measures 1
    // through 3 hold no entries in the source at all.
    const std::array<size_t, 14> expectedSequenceCounts = { 0, 0, 0, 0, 1, 2, 1, 1, 0, 1, 1, 0, 0, 0 };
    for (size_t x = 0; x < expectedSequenceCounts.size(); x++) {
        EXPECT_EQ(measures[x].sequences().size(), expectedSequenceCounts[x])
            << "measure " << (x + 1) << " sequence count";
    }
}

TEST(MnxParts, MeasureRepeatCounters)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("measure_repeat_counters.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({
        "Processing", pathString(inputPath.filename()), "!validation error",
        // Every counter in the fixture belongs to a measure that declares a repeat, so none is dropped.
        "!measure repeat counter"
    }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "measure_repeat_counters.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    auto measures = parts[0].measures();
    ASSERT_EQ(measures.size(), 31);

    // Finale supplies no counter of its own, so the fixture numbers the iterations with text
    // expressions centered over the repeated measures. Two runs of one-bar repeats, each counting
    // from 2 because the measure holding the music being repeated is iteration 1.
    const auto expectedCount = [](size_t measureNumber) -> std::optional<int> {
        if (measureNumber >= 2 && measureNumber <= 11) {
            return static_cast<int>(measureNumber);         // m1 is the music, m2..m11 count 2..11
        }
        if (measureNumber >= 13 && measureNumber <= 19) {
            return static_cast<int>(measureNumber) - 11;    // m12 is the music, m13..m19 count 2..8
        }
        return std::nullopt;
    };
    for (size_t x = 0; x < measures.size(); x++) {
        const size_t measureNumber = x + 1;
        const auto expected = expectedCount(measureNumber);
        const auto measureRepeat = measures[x].measureRepeat();
        const std::string label = "measure " + std::to_string(measureNumber);
        if (!expected) {
            EXPECT_FALSE(measureRepeat.has_value()) << label << " should have no measure repeat";
            continue;
        }
        ASSERT_TRUE(measureRepeat.has_value()) << label << " should have a measure repeat";
        EXPECT_EQ(measureRepeat->number(), 1) << label;
        const auto counter = measureRepeat->counter();
        ASSERT_TRUE(counter.has_value()) << label << " should have a counter";
        EXPECT_EQ(counter->count(), expected.value()) << label;
    }
}

TEST(MnxParts, DynamicAccentAffixes)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("slurs_2voices.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "slurs_2voices.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);
    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 2);

    // Measure 2 carries an "fz". MNX defaults accentPrefix to "s" and accentSuffix to "z", so an
    // accent that leaves them out reads as "sfz". Both must be written for this to say "fz".
    ASSERT_TRUE(measures[1].dynamics().has_value());
    auto dynamics = measures[1].dynamics().value();
    ASSERT_GE(dynamics.size(), 1);
    ASSERT_EQ(dynamics[0].type(), mnx::part::DynamicAccent::ContentTypeValue) << "the fz should export as an accent";
    auto accent = dynamics[0].get<mnx::part::DynamicAccent>();
    EXPECT_EQ(accent.value(), mnx::DynamicValue::f);
    EXPECT_EQ(accent.accentPrefix(), mnx::DynamicPrefix::None);
    EXPECT_EQ(accent.accentSuffix(), mnx::DynamicSuffix::z);
    EXPECT_FALSE(accent.residualValue().has_value());
}

TEST(MnxParts, DynamicsGraceIndices)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("grace_indices.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "grace_indices.mnx");
    auto parts = doc.parts();
    ASSERT_GE(parts.size(), 1);

    auto measures = parts[0].measures();
    ASSERT_GE(measures.size(), 4);

    {
        auto measure = measures[1];
        ASSERT_TRUE(measure.dynamics().has_value());
        auto dynamics = measure.dynamics().value();
        ASSERT_GE(dynamics.size(), 2);
        EXPECT_EQ(dynamicGraceIndex(dynamics[0]), std::nullopt);
        EXPECT_EQ(dynamicGraceIndex(dynamics[1]), unsigned{0});
    }

    {
        auto measure = measures[2];
        ASSERT_TRUE(measure.dynamics().has_value());
        auto dynamics = measure.dynamics().value();
        ASSERT_GE(dynamics.size(), 4);
        EXPECT_EQ(dynamicGraceIndex(dynamics[0]), unsigned{3});
        EXPECT_EQ(dynamicGraceIndex(dynamics[1]), unsigned{2});
        EXPECT_EQ(dynamicGraceIndex(dynamics[2]), unsigned{1});
        EXPECT_EQ(dynamicGraceIndex(dynamics[3]), unsigned{0});
    }

    {
        auto measure = measures[3];
        ASSERT_TRUE(measure.dynamics().has_value());
        auto dynamics = measure.dynamics().value();
        ASSERT_GE(dynamics.size(), 1);
        EXPECT_EQ(dynamicGraceIndex(dynamics[0]), unsigned{0});
    }
}
