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
#include "musx/musx.h"
#include "export/mnx.h"

using namespace denigma;
using namespace musx::dom;

TEST(MnxGlobal, Tempos)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tempo_text_shape.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "tempo_text_shape.mnx");
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
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--enigmaxml" };
        checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to enigmaxml: " << inputPath.u8string();
        });
    }
    {
        ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx", "--include-tempo-tool" };
            checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
            EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
        });
    }

    std::vector<char> xmlBuf;
    readFile(inputPath.parent_path() / "tempo_changes.enigmaxml", xmlBuf);
    auto musxDoc = musx::factory::DocumentFactory::create<musx::xml::pugi::Document>(xmlBuf);
    ASSERT_TRUE(musxDoc);

    auto mnxDoc = mnx::Document::create(inputPath.parent_path() / "tempo_changes.mnx");
    auto measures = mnxDoc.global().measures();
    ASSERT_GE(measures.size(), 4) << "should be at least 4 measures";

    for (size_t x = 0; x < 4; x++)
    {
        auto musxTempoChanges = musxDoc->getOthers()->getArray<others::TempoChange>(SCORE_PARTID, static_cast<MeasCmper>(x) + 1);
        auto mnxTempoChanges = measures[x].tempos();
        ASSERT_GT(musxTempoChanges.size(), 0);
        ASSERT_TRUE(mnxTempoChanges);
        ASSERT_EQ(musxTempoChanges.size(), mnxTempoChanges->size());
        for (size_t y = 0; y < musxTempoChanges.size(); y++) {
            auto musxDura = musx::util::Fraction::fromEdu(musxTempoChanges[y]->eduPosition);
            musx::util::Fraction mnxDura;
            if (const auto location = mnxTempoChanges->at(y).location()) {
                mnxDura = mnxexp::fractionFromMnxFraction(location->fraction());
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
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "timesigs_composite.mnx");
    auto measures = doc.global().measures();
    ASSERT_GE(measures.size(), 1) << "should be at least one measure";

    auto time = measures[0].time();
    ASSERT_TRUE(time);
    EXPECT_EQ(time.value().count(), 133);
    EXPECT_EQ(time.value().unit(), mnx::TimeSignatureUnit::Value32nd);
}