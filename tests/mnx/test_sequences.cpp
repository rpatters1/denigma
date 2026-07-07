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
#include "denigma/classify/articulations.h"
#include "formats/mnx/mnx_articulations.h"
#include "test_utils.h"

using namespace denigma;

namespace {

nlohmann::json exportMnxFixture(const std::string& fileName)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput(fileName, inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    nlohmann::json mnx;
    openJson(inputPath.parent_path() / (inputPath.stem().string() + ".mnx"), mnx);
    return mnx;
}

} // namespace

TEST(MnxSequences, CalcPointing)
{
    using formats::mnx::detail::calcPointing;

    EXPECT_EQ(calcPointing(classify::GlyphStyle{ VerticalPlacement::Above }), mnxdom::MarkingUpDownAuto::Up);
    EXPECT_EQ(calcPointing(classify::GlyphStyle{ VerticalPlacement::Below }), mnxdom::MarkingUpDownAuto::Down);
    EXPECT_EQ(calcPointing(classify::GlyphStyle{ VerticalPlacement::Float }), mnxdom::MarkingUpDownAuto::Auto);
    EXPECT_EQ(calcPointing(classify::GlyphStyle{}), mnxdom::MarkingUpDownAuto::Auto);
}

TEST(MnxSequences, Voice2TripletAtEnd)
{
    // test file contains voice 2 triplet at end of bar. The triplet was being overfilled.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2triplet_at_end.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "v2triplet_at_end.mnx");
}

TEST(MnxSequences, Voice2TupletIncomplete)
{
    // test file contains incomplete v2 triplet that should be reported without additional adding compensationg spacer to main sequence
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("v2tuplet_incomplete.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "less than the expected", "!more than the expected" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "v2tuplet_incomplete.mnx");
}

TEST(MnxSequences, NestedTuplets)
{
    // test file contains a nested tuplets ending on the same note.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplets_nested.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "tuplets_nested.mnx");
}

TEST(MnxSequences, NestedTrailingSingletonTuplet)
{
    // test file contains a trailing singleton tuplet nested inside another tuplet.
    
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("tuplet-nested-singleton.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", pathString(inputPath), "--mnx" };
    checkStderr({ "Processing", pathString(inputPath.filename()), "!validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << pathString(inputPath);
    });

    auto doc = mnxdom::Document::create(inputPath.parent_path() / "tuplet-nested-singleton.mnx");
}

TEST(MnxSequences, CrossStaffNotesUseNoteLevelStaffOverride)
{
    const auto mnx = exportMnxFixture("cross_staffs.musx");

    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_FALSE(mnx["parts"].empty());
    const auto& measures = mnx["parts"][0]["measures"];

    int crossedEventCount = 0;
    for (const auto& measure : measures) {
        if (!measure.contains("sequences")) {
            continue;
        }
        for (const auto& sequence : measure["sequences"]) {
            if (!sequence.contains("content")) {
                continue;
            }
            for (const auto& item : sequence["content"]) {
                if (!item.contains("notes")) {
                    continue;
                }
                int crossedNotesInEvent = 0;
                for (const auto& note : item["notes"]) {
                    if (note.contains("staff")) {
                        ++crossedNotesInEvent;
                        EXPECT_EQ(note["staff"], 2) << item.dump(4);
                    }
                }
                if (crossedNotesInEvent > 0) {
                    ++crossedEventCount;
                    ASSERT_TRUE(sequence.contains("staff")) << sequence.dump(4);
                    EXPECT_EQ(sequence["staff"], 1) << sequence.dump(4);
                    EXPECT_FALSE(item.contains("staff")) << item.dump(4);
                    EXPECT_EQ(crossedNotesInEvent, 1) << item.dump(4);
                }
            }
        }
    }

    EXPECT_EQ(crossedEventCount, 4);
}

TEST(MnxSequences, GraceBeamsRetainSlashState)
{
    const auto mnx = exportMnxFixture("grace_beamed.musx");

    ASSERT_TRUE(mnx.contains("parts"));
    ASSERT_FALSE(mnx["parts"].empty());
    const auto& content = mnx["parts"][0]["measures"][0]["sequences"][0]["content"];

    std::vector<std::reference_wrapper<const nlohmann::json>> graceGroups;
    for (const auto& item : content) {
        if (item.value("type", "") == "grace") {
            graceGroups.emplace_back(std::cref(item));
        }
    }

    ASSERT_EQ(graceGroups.size(), 3u);

    const auto& firstGrace = graceGroups[0].get();
    const auto& beamedGrace = graceGroups[1].get();
    const auto& lastGrace = graceGroups[2].get();

    EXPECT_FALSE(firstGrace.contains("slash")) << firstGrace.dump(4); // omitted means slashed
    EXPECT_EQ(firstGrace["content"].size(), 1u);

    ASSERT_TRUE(beamedGrace.contains("slash")) << beamedGrace.dump(4);
    EXPECT_FALSE(beamedGrace["slash"]) << beamedGrace.dump(4);
    EXPECT_EQ(beamedGrace["content"].size(), 2u);

    EXPECT_FALSE(lastGrace.contains("slash")) << lastGrace.dump(4); // omitted means slashed
    EXPECT_EQ(lastGrace["content"].size(), 1u);
}
