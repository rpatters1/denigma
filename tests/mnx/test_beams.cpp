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

static void getNote(std::shared_ptr<mnx::sequence::Note>& note, const std::optional<mnx::Array<mnx::part::Measure>>& measures,
    size_t measureIndex, size_t contentIndex, size_t expectedContentSize)
{
    ASSERT_TRUE(measures.has_value()) << "no measure array in first part";
    ASSERT_GT(measures.value().size(), measureIndex) << "fewer than " << measureIndex + 1 << " measures in measure array of part";

    auto sequences = measures.value()[measureIndex].sequences();
    ASSERT_FALSE(sequences.empty());
    auto content = sequences[0].content();
    ASSERT_EQ(content.size(), expectedContentSize);

    auto contentItem = content[contentIndex];
    ASSERT_EQ(contentItem.type(), mnx::sequence::Event::ContentTypeValue);
    auto event = contentItem.get<mnx::sequence::Event>();
    ASSERT_TRUE(event.notes().has_value()) << "event at m1 index 3 has no notes array";
    ASSERT_FALSE(event.notes().value().empty()) << "event at m1 index 3 has no notes";
    note = std::make_shared<mnx::sequence::Note>(event.notes().value()[0]); // no chords are in the test file.
}

static void testNote(const mnx::sequence::Note& note, mnx::NoteStep step, int octave, int alter)
{
    EXPECT_EQ(note.pitch().step(), step);
    EXPECT_EQ(note.pitch().octave(), octave);
    EXPECT_EQ(note.pitch().alter().value_or(0), alter);
}

TEST(MnxBeams, MultiMeasureBeams)
{
    setupTestDataPaths();
    std::filesystem::path inputPath;
    copyInputToOutput("multimeas_beam.musx", inputPath);
    ArgList args = { DENIGMA_NAME, "export", inputPath.u8string(), "--mnx" };
    checkStderr({ "Processing", inputPath.filename().u8string(), "!Validation error" }, [&]() {
        EXPECT_EQ(denigmaTestMain(args.argc(), args.argv()), 0) << "export to mnx: " << inputPath.u8string();
    });

    auto doc = mnx::Document::create(inputPath.parent_path() / "multimeas_beam.mnx");
    auto parts = doc.parts();
    ASSERT_FALSE(parts.empty()) << "no parts in document";
    auto measures = parts[0].measures();

    {
        std::shared_ptr<mnx::sequence::Note> note;
        getNote(note, measures, 0, 3, 4);
        ASSERT_TRUE(note);
        testNote(*note.get(), mnx::NoteStep::G, 4, 0);
    }

    {
        std::shared_ptr<mnx::sequence::Note> note;
        getNote(note, measures, 1, 1, 8);
        ASSERT_TRUE(note);
        testNote(*note.get(), mnx::NoteStep::E, 4, 0);
    }

    {
        std::shared_ptr<mnx::sequence::Note> note;
        getNote(note, measures, 2, 1, 4);
        ASSERT_TRUE(note);
        testNote(*note.get(), mnx::NoteStep::G, 4, 0);
    }
}