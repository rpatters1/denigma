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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Characterization tests for tuplet export.
//
// These pin down what actually reaches the MusicXML file for nested tuplets, which nothing else
// covered, so that a change in that output is visible as a test change rather than as a silent
// shift.

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mx/api/ScoreData.h"
#include "musicxml_test.h"
#include "pugixml.hpp"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

struct TupletMark
{
    std::string type;
    std::string number;
};

struct TupletNote
{
    std::string id;
    std::string noteType;
    std::string actualNotes;
    std::string normalNotes;
    std::string normalType;      ///< Empty when the note carries no <normal-type>.
    size_t normalDots{};         ///< Count of <normal-dot> children.
    std::vector<TupletMark> marks;
};

// Collects every note that carries tuplet notation or a time modification, in document order.
std::vector<TupletNote> tupletNotes(const std::filesystem::path& musicXmlPath)
{
    pugi::xml_document document;
    EXPECT_TRUE(document.load_file(musicXmlPath.c_str()));

    std::vector<TupletNote> result;
    for (const auto note : document.select_nodes(".//note")) {
        const auto noteNode = note.node();
        const auto timeModification = noteNode.child("time-modification");
        const auto notations = noteNode.child("notations");
        const bool hasTuplet = notations && notations.child("tuplet");
        if (!timeModification && !hasTuplet) {
            continue;
        }

        TupletNote entry;
        entry.id = noteNode.attribute("id").value();
        entry.noteType = noteNode.child_value("type");
        entry.actualNotes = timeModification.child_value("actual-notes");
        entry.normalNotes = timeModification.child_value("normal-notes");
        entry.normalType = timeModification.child_value("normal-type");
        for (auto dot = timeModification.child("normal-dot"); dot; dot = dot.next_sibling("normal-dot")) {
            ++entry.normalDots;
        }
        for (auto tuplet = notations.child("tuplet"); tuplet; tuplet = tuplet.next_sibling("tuplet")) {
            entry.marks.push_back({ tuplet.attribute("type").value(), tuplet.attribute("number").value() });
        }
        result.push_back(std::move(entry));
    }
    return result;
}

} // namespace

TEST(MusicXmlTuplets, NestedTupletsCarryCumulativeTimeModification)
{
    setupTestDataPaths();
    const auto notes = tupletNotes(exportMusicXmlFixture("tuplets_nested.musx"));
    ASSERT_FALSE(notes.empty());

    // A 3:2 eighth tuplet inside a 3:2 quarter tuplet: nine eighths in the space of four. Every
    // note under both tuplets carries that one cumulative ratio, which is what <time-modification>
    // is for. The ratio is reduced by construction; see design-decisions.md.
    for (const auto& note : notes) {
        EXPECT_EQ(note.actualNotes, "9") << "note " << note.id;
        EXPECT_EQ(note.normalNotes, "4") << "note " << note.id;
        EXPECT_EQ(note.noteType, "eighth") << "note " << note.id;
        // Omitted because the tuplets nest: the cumulative ratio belongs to no single tuplet's
        // reference duration, so MusicXML's default of the note's own type stands.
        EXPECT_EQ(note.normalType, "") << "note " << note.id;
    }

    // The outer tuplet opens first and both open on the same note, so the nesting is visible in
    // the notation as well as in the ratio.
    ASSERT_GE(notes.front().marks.size(), 2u);
    EXPECT_EQ(notes.front().marks.at(0).type, "start");
    EXPECT_EQ(notes.front().marks.at(0).number, "1");
    EXPECT_EQ(notes.front().marks.at(1).type, "start");
    EXPECT_EQ(notes.front().marks.at(1).number, "2");

    // Every start pairs with exactly one stop at the same number.
    std::vector<std::pair<std::string, int>> openCounts;
    for (const auto& note : notes) {
        for (const auto& mark : note.marks) {
            const auto found = std::ranges::find_if(openCounts, [&mark](const auto& entry) {
                return entry.first == mark.number;
            });
            if (found == openCounts.end()) {
                openCounts.emplace_back(mark.number, mark.type == "start" ? 1 : -1);
            } else {
                found->second += mark.type == "start" ? 1 : -1;
            }
        }
    }
    for (const auto& [number, balance] : openCounts) {
        EXPECT_EQ(balance, 0) << "tuplet number " << number << " is unbalanced";
    }
}

TEST(MusicXmlTuplets, NestedSingletonTupletCarriesCumulativeRatio)
{
    setupTestDataPaths();
    const auto notes = tupletNotes(exportMusicXmlFixture("tuplet-nested-singleton.musx"));
    ASSERT_FALSE(notes.empty());

    // The fixture's inner tuplet covers exactly one note: it begins and ends on the last note of
    // the outer tuplet, which is why that note's cumulative ratio alone jumps to 12:1 while the
    // rest of the outer tuplet reads 6:1.
    const auto& lastNote = notes.back();
    EXPECT_EQ(lastNote.actualNotes, "12") << "note " << lastNote.id;
    for (const auto& note : notes) {
        if (note.id != lastNote.id && !note.actualNotes.empty()) {
            EXPECT_EQ(note.actualNotes, "6") << "note " << note.id;
        }
    }

    // Only the outer tuplet is in force on the first note, an eighth in a tuplet whose reference
    // duration is a quarter, so its <normal-type> names that quarter. The last note has both
    // tuplets in force, and a cumulative 12:1 that neither reference duration describes, so it
    // carries none; see design-decisions.md.
    EXPECT_EQ(notes.front().normalType, "quarter") << "note " << notes.front().id;
    EXPECT_EQ(lastNote.normalType, "") << "note " << lastNote.id;
    for (const auto& note : notes) {
        EXPECT_NE(note.normalType, note.noteType) << "note " << note.id;
    }

    // Numbering is sound: denigma's numberLevel is the tuplet's index in its entry frame, so it is
    // stable for a tuplet's whole extent and every start matches its own stop.
    std::vector<std::pair<std::string, int>> openCounts;
    for (const auto& note : notes) {
        for (const auto& mark : note.marks) {
            const auto found = std::ranges::find_if(openCounts, [&mark](const auto& entry) {
                return entry.first == mark.number;
            });
            if (found == openCounts.end()) {
                openCounts.emplace_back(mark.number, mark.type == "start" ? 1 : -1);
            } else {
                found->second += mark.type == "start" ? 1 : -1;
            }
        }
    }
    for (const auto& [number, balance] : openCounts) {
        EXPECT_EQ(balance, 0) << "tuplet number " << number << " is unbalanced";
    }
}

TEST(MusicXmlTuplets, SingleNoteTupletsCarryCumulativeRatio)
{
    setupTestDataPaths();
    const auto notes = tupletNotes(exportMusicXmlFixture("tuplet_singletons.musx"));

    // Two tuplets, each covering exactly one note, so each note carries that tuplet's whole ratio
    // and both of its notation ends.
    ASSERT_EQ(notes.size(), 2u);
    for (const auto& note : notes) {
        EXPECT_EQ(note.actualNotes, "3") << "note " << note.id;
        EXPECT_EQ(note.normalNotes, "2") << "note " << note.id;
        ASSERT_EQ(note.marks.size(), 2u) << "note " << note.id;
        EXPECT_EQ(note.marks.at(0).number, note.marks.at(1).number) << "note " << note.id;
        const bool oneOfEach = note.marks.at(0).type != note.marks.at(1).type;
        EXPECT_TRUE(oneOfEach) << "note " << note.id << ": expected one start and one stop";
    }
}

// A tuplet's reference duration can be dotted, and MusicXML then spells it as a <normal-type> plus
// one <normal-dot> per dot. No Finale-authored fixture reaches that path. tuplet_dotted_reference.musx
// is tuplet_singletons.musx with both tupletDef elements restated as three dotted eighths in the
// space of one dotted quarter, edited in the enigmaxml and exported back to musx: the same 3:2
// sounding ratio and the same durations, over a dotted reference duration.
TEST(MusicXmlTuplets, DottedReferenceDurationCarriesNormalDot)
{
    setupTestDataPaths();
    const auto notes = tupletNotes(exportMusicXmlFixture("tuplet_dotted_reference.musx"));
    ASSERT_EQ(notes.size(), 2u);

    for (const auto& note : notes) {
        EXPECT_EQ(note.noteType, "half") << "note " << note.id;
        EXPECT_EQ(note.actualNotes, "3") << "note " << note.id;
        EXPECT_EQ(note.normalNotes, "2") << "note " << note.id;
        EXPECT_EQ(note.normalType, "quarter") << "note " << note.id;
        EXPECT_EQ(note.normalDots, 1) << "note " << note.id;
    }
}

// A tuplet may cover exactly one note, and MusicXML then puts both of its ends on that note, the
// start before the stop. Finale's own export of this fixture
// (inputs/musicxml/tuplet_singletons-ref.musicxml) writes exactly that, twice, and mx has done the
// same since webern/mx#429.
TEST(MusicXmlTuplets, SingleNoteTupletWritesStartBeforeStop)
{
    setupTestDataPaths();
    const auto notes = tupletNotes(exportMusicXmlFixture("tuplet_singletons.musx"));
    ASSERT_EQ(notes.size(), 2u);

    for (const auto& note : notes) {
        ASSERT_EQ(note.marks.size(), 2u) << "note " << note.id;
        EXPECT_EQ(note.marks.at(0).type, "start") << "note " << note.id;
        EXPECT_EQ(note.marks.at(1).type, "stop") << "note " << note.id;
    }
}

// Denigma sets DurationData::timeModificationNormalType only where the tuplet's reference duration
// differs from the note's own type, because MusicXML reads an absent <normal-type> as the note
// type. Since webern/mx#428, mx::impl::NoteWriter writes that field rather than inferring the
// element from sibling notes, so the written set is exactly the requested set and no note carries
// a <normal-type> that merely repeats its own <type>.
TEST(MusicXmlTuplets, NormalTypeIsWrittenOnlyWhereRequested)
{
    setupTestDataPaths();

    const auto requested = createScoreDataFromMusxPath(
        std::filesystem::path("inputs") / "zwei_gesange.musx");
    ASSERT_TRUE(requested.has_value());

    size_t requestedCount = 0;
    for (const auto& part : requested->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        if (note.durationData.timeModificationNormalType != mx::api::DurationName::unspecified) {
                            ++requestedCount;
                        }
                    }
                }
            }
        }
    }

    const auto notes = tupletNotes(exportMusicXmlFixture("zwei_gesange.musx"));
    size_t writtenCount = 0;
    size_t redundantCount = 0;
    for (const auto& note : notes) {
        if (note.normalType.empty()) {
            continue;
        }
        ++writtenCount;
        if (note.normalType == note.noteType) {
            ++redundantCount;
        }
    }

    EXPECT_EQ(writtenCount, requestedCount);
    EXPECT_EQ(redundantCount, 0u) << "<normal-type> should never merely repeat <type>";
}
