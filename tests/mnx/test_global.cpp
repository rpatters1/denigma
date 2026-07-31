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
#include <array>
#include <filesystem>
#include <iterator>
#include <fstream>

#include "gtest/gtest.h"
#include "core/denigma.h"
#include "core/musx_reader.h"
#include "mnxdom.h"

namespace mnxdom = ::mnx;
#include "test_utils.h"
#include "musx/musx.h"
#include "formats/mnx/mnx.h"

using namespace denigma;
using namespace musx::dom;

TEST(MnxGlobal, Tempos)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tempo_text_shape.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "tempo_text_shape.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 1) << "should be at least one measure";
    auto tempos = measures[0].tempos();
    ASSERT_TRUE(tempos.has_value()) << "should have tempos in the first measure";
    ASSERT_EQ(tempos.value().size(), 2) << "should have 2 tempos in the first measure";

    auto testTempo = [&](auto tempo, int bpm, int numerator, int denominator) {
        EXPECT_EQ(tempo.bpm(), bpm);
        if (tempo.location().has_value()) {
            EXPECT_EQ(tempo.location().value().fraction().numerator(), numerator);
            EXPECT_EQ(tempo.location().value().fraction().denominator(), denominator);
        } else {
            EXPECT_EQ(numerator, 0);
            EXPECT_EQ(denominator, 1);
        }
    };
    {
        auto tempo = tempos.value()[0];
        EXPECT_FALSE(tempo.location().has_value());
        testTempo(tempo, 73, 0, 1);
    }
    {
        auto tempo = tempos.value()[1];
        EXPECT_TRUE(tempo.location().has_value());
        testTempo(tempo, 71, 1, 2);
    }
}

TEST(MnxGlobal, TempoToolChanges)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tempo_changes.musx", inputPath);
    {
        ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--enigmaxml" };
        checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to enigmaxml: " << pathString(inputPath);
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx", "--include-tempo-tool" };
            checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
        });
    }

    std::vector<char> xmlBuf;
    readFile(inputPath.parent_path() / "tempo_changes.enigmaxml", xmlBuf);
    auto musxDoc = musx::factory::DocumentFactory::create<MusxReader>(xmlBuf);
    ASSERT_TRUE(musxDoc);

    auto mnxDoc = mnxdom::Document::create(inputPath.parent_path() / "tempo_changes.mnx");
    auto measures = mnxDoc.global().measures();
    ASSERT_GE(measures.size(), 4) << "should be at least 4 measures";

    for (size_t x = 0; x < 4; x++)
    {
        auto musxTempoChanges = musxDoc->getOthers()->getArray<others::TempoChange>(SCORE_PARTID, static_cast<Cmper>(x + 1));
        auto mnxTempoChanges = measures[x].tempos();
        ASSERT_GT(musxTempoChanges.size(), 0);
        ASSERT_TRUE(mnxTempoChanges);
        ASSERT_EQ(musxTempoChanges.size(), mnxTempoChanges->size());
        for (size_t y = 0; y < musxTempoChanges.size(); y++) {
            auto musxDura = musx::util::Fraction::fromEdu(musxTempoChanges[y]->eduPosition);
            musx::util::Fraction mnxDura;
            if (const auto location = mnxTempoChanges->at(y).location()) {
                mnxDura = formats::mnx::detail::fractionFromMnxFraction(location->fraction());
            }
            EXPECT_EQ(musxDura, mnxDura) << "measure positions are not the same";
        }
    }
}

TEST(MnxGlobal, CompositeTime)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("timesigs_composite.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "timesigs_composite.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 1) << "should be at least one measure";

    auto time = measures[0].time();
    ASSERT_TRUE(time);
    EXPECT_EQ(time.value().count(), 133);
    EXPECT_EQ(time.value().unit(), mnxdom::TimeSignatureUnit::Value32nd);
}

namespace {

struct ExpectedTime
{
    int count;
    mnxdom::TimeSignatureUnit unit;
    std::optional<mnxdom::TimeSignatureDisplay> display;
};

/// @brief Verifies the `time` object of each global measure. A `std::nullopt` entry means the measure
/// carries the previous measure's time signature forward and must not emit a `time` object of its own.
/// @param expectCleanValidation Pass false for a document with a known, unrelated semantic validation error.
void checkGlobalTimes(const std::string& fileName, const std::vector<std::optional<ExpectedTime>>& expected,
    bool expectCleanValidation = true)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput(fileName + ".musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    std::vector<std::string> expectedMessages = { "Processing", pathString(inputPath.filename()) };
    if (expectCleanValidation) {
        expectedMessages.emplace_back("!validation error");
    }
    checkStderr(expectedMessages, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto displayName = [](const std::optional<mnxdom::TimeSignatureDisplay>& display) -> std::string {
        if (!display) {
            return "(none)";
        }
        return mnxdom::EnumStringMapping<mnxdom::TimeSignatureDisplay>::enumToString().at(display.value());
    };

    auto doc = mnxdom::Document::create(inputPath.parent_path() / (fileName + ".mnx"));
    auto measures = doc.global().measures();
    ASSERT_EQ(measures.size(), expected.size()) << fileName << ": unexpected number of global measures";

    for (size_t x = 0; x < expected.size(); x++) {
        const std::string label = fileName + " measure " + std::to_string(x + 1);
        const auto time = measures[x].time();
        if (!expected[x]) {
            EXPECT_FALSE(time.has_value()) << label << ": should not repeat the previous time signature";
            continue;
        }
        ASSERT_TRUE(time.has_value()) << label << ": should have a time signature";
        EXPECT_EQ(time->count(), expected[x]->count) << label << ": count";
        EXPECT_EQ(time->unit(), expected[x]->unit) << label << ": unit";
        EXPECT_EQ(displayName(time->display()), displayName(expected[x]->display)) << label << ": display";
    }
}

} // namespace

TEST(MnxGlobal, TimeSignatureDisplaySymbols)
{
    using Unit = mnxdom::TimeSignatureUnit;
    using Display = mnxdom::TimeSignatureDisplay;

    // In this document "Abbreviate Cut Time" is on and "Abbreviate Common Time" is off, so only a measure
    // that abbreviates its own display time signature can produce a common-time symbol.
    //
    // MNX gives a measure exactly one time signature, shared by every part, so it cannot represent the
    // independent (per-staff) time signatures this document uses. The expectations below are therefore all
    // score-level meters, and the export emits a semantic validation error unrelated to time signature
    // display: entries on a staff with its own meter overfill the global measure duration.
    checkGlobalTimes("timesigs_independent", {
        ExpectedTime{ 4, Unit::Quarter, std::nullopt },  // 4/4, but common time is not abbreviated document-wide
        std::nullopt,
        ExpectedTime{ 3, Unit::Quarter, std::nullopt },
        std::nullopt,
        std::nullopt,
        ExpectedTime{ 4, Unit::Quarter, Display::Common }, // abbreviated 4/4 display time signature
        ExpectedTime{ 2, Unit::Half, Display::Cut },       // 2/2 abbreviated by the document-wide cut time option
        ExpectedTime{ 6, Unit::Eighth, std::nullopt },     // 2/4 display time signature: not representable in MNX
        ExpectedTime{ 5, Unit::Eighth, std::nullopt },
        std::nullopt,
        ExpectedTime{ 3, Unit::Quarter, std::nullopt },
        std::nullopt,
        std::nullopt
    }, /*expectCleanValidation*/ false);
}

TEST(MnxGlobal, TimeSignatureDisplayNotAbbreviated)
{
    using Unit = mnxdom::TimeSignatureUnit;

    // Measure 1 is a 1/4 pickup whose display time signature is 4/4, and measure 2 is an actual 4/4.
    // Neither may emit a common-time symbol, because "Abbreviate Common Time" is off in this document.
    checkGlobalTimes("timesigs_changing", {
        ExpectedTime{ 1, Unit::Quarter, std::nullopt },
        ExpectedTime{ 4, Unit::Quarter, std::nullopt },
        ExpectedTime{ 5, Unit::Eighth, std::nullopt },
        ExpectedTime{ 7, Unit::Eighth, std::nullopt },
        ExpectedTime{ 3, Unit::Quarter, std::nullopt }
    });
}

TEST(MnxGlobal, TimeSignatureDisplayChange)
{
    using Unit = mnxdom::TimeSignatureUnit;
    using Display = mnxdom::TimeSignatureDisplay;

    // Every measure is 4/4, but measure 4 turns on an abbreviated display time signature. A new `time`
    // object must be emitted there even though the count and unit are unchanged.
    constexpr size_t numMeasures = 29;
    constexpr size_t measureThatAbbreviates = 3; // zero-based index of measure 4
    std::vector<std::optional<ExpectedTime>> expected(numMeasures, std::nullopt);
    expected.front() = ExpectedTime{ 4, Unit::Quarter, std::nullopt };
    expected[measureThatAbbreviates] = ExpectedTime{ 4, Unit::Quarter, Display::Common };
    checkGlobalTimes("repeats", expected);
}

TEST(MnxGlobal, BarlineTypes)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("barline_types.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "barline_types.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 10) << "should be at least ten measures";

    constexpr std::array<mnxdom::BarlineType, 10> expected = {
        mnxdom::BarlineType::Double,
        mnxdom::BarlineType::Regular,
        mnxdom::BarlineType::Final,
        mnxdom::BarlineType::Heavy,
        mnxdom::BarlineType::Dashed,
        mnxdom::BarlineType::Dashed,
        mnxdom::BarlineType::NoBarline,
        mnxdom::BarlineType::Short,
        mnxdom::BarlineType::Tick,
        mnxdom::BarlineType::Double
    };

    for (size_t i = 0; i < expected.size(); ++i) {
        const auto barline = measures[i].barline();
        if (i == 1 && !barline) {
            continue;
        }
        ASSERT_TRUE(barline) << "measure " << (i + 1) << " should have a barline";
        EXPECT_EQ(barline->type(), expected[i]) << "measure " << (i + 1);
    }
}

TEST(MnxGlobal, FinalMeasureWithNormalBarlineUsesImplicitFinal)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("barline_short_normal.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "barline_short_normal.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 3) << "should be at least three measures";

    for (size_t i = 0; i + 1 < measures.size(); ++i) {
        const auto barline = measures[i].barline();
        ASSERT_TRUE(barline) << "measure " << (i + 1) << " should have a barline";
        EXPECT_EQ(barline->type(), mnxdom::BarlineType::Short) << "measure " << (i + 1);
    }

    // The final measure has a normal barline in the musx document, but Finale's
    // drawFinalBarlineOnLastMeas option promotes it to a final barline, which is
    // the MNX default for the last measure and therefore must not be emitted explicitly.
    EXPECT_FALSE(measures[measures.size() - 1].barline()) << "final measure should rely on the implicit final barline";
}
