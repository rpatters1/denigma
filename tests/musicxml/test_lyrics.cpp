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

#include <algorithm>
#include <filesystem>
#include <map>
#include <ostream>
#include <utility>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "gtest/gtest.h"
#include "mx/api/ScoreData.h"
#include "musicxml_test.h"
#include "test_utils.h"

using namespace denigma;
using namespace denigma::test::musicxml;

namespace {

// A lyric line is identified by its name and number together, since each Finale block numbers
// from 1 and so Verse 1 and Chorus 1 share the number. Lyrics do not necessarily iterate in the
// order Finale's own exporter declared them, so tests look one up by that pair rather than by
// vector position.
const mx::api::LyricData& findLyric(
    const std::vector<mx::api::LyricData>& lyrics, std::string_view verseName, std::string_view verseNumber)
{
    const auto it = std::find_if(lyrics.begin(), lyrics.end(), [&](const mx::api::LyricData& lyric) {
        return lyric.verseName == verseName && lyric.verseNumber == verseNumber;
    });
    if (it == lyrics.end()) {
        throw std::out_of_range(
            "No lyric found with name " + std::string(verseName) + " number " + std::string(verseNumber));
    }
    return *it;
}

const mx::api::NoteData& firstNoteAt(const mx::api::ScoreData& score, size_t measureIdx, size_t noteIdx)
{
    const auto& staff = score.parts.at(0).measures.at(measureIdx).staves.at(0);
    return staff.voices.at(0).notes.at(noteIdx);
}

} // namespace

// Exercises the "for_health_and_strength" fixture, which the user hand-edited in Finale to probe
// exactly which characters trigger a MusicXML <elision> split (U+00A0 NBSP and U+203F undertie;
// not non-breaking hyphen, en dash, em dash, or a literal SMuFL elision glyph typed as text) and
// how displayVerseNum's auto-generated verse number composes with that split. See musxdom
// LyricAssign::calcDisplayNumberText().
TEST(MusicXmlLyrics, DisplayVerseNumberAndElisionMatchFinale)
{
    setupTestDataPaths();

    const auto outputPath = exportMusicXmlFixture("for_health_and_strength.musx");
    const auto score = loadScoreData(outputPath);
    ASSERT_TRUE(score.has_value());
    ASSERT_FALSE(score->parts.empty());

    // Verse 1's first syllable ("For") gets Finale's auto-generated "1." number, composed as a
    // leading run elided to the syllable. Verse 2 has no displayVerseNum override, but its raw
    // syllable text is the literal, author-typed "(2.<NBSP>For)".
    {
        const auto& note = firstNoteAt(*score, 0, 0);
        ASSERT_EQ(note.lyrics.size(), 2u);

        const auto& verse1 = findLyric(note.lyrics, "verse", "1");
        EXPECT_EQ(verse1.text, "1.");
        EXPECT_EQ(verse1.syllabic, mx::api::LyricSyllabic::single);
        ASSERT_EQ(verse1.continuations.size(), 1u);
        EXPECT_EQ(verse1.continuations.at(0).text, "For");
        EXPECT_EQ(verse1.continuations.at(0).syllabic, mx::api::LyricSyllabic::single);
        ASSERT_TRUE(verse1.continuations.at(0).elisionText.has_value());
        EXPECT_EQ(*verse1.continuations.at(0).elisionText, "\xC2\xA0");

        const auto& verse2 = findLyric(note.lyrics, "verse", "2");
        EXPECT_EQ(verse2.text, "(2.");
        ASSERT_EQ(verse2.continuations.size(), 1u);
        EXPECT_EQ(verse2.continuations.at(0).text, "For)");
        ASSERT_TRUE(verse2.continuations.at(0).elisionText.has_value());
        EXPECT_EQ(*verse2.continuations.at(0).elisionText, "\xC2\xA0");
    }

    // "and<undertie>a": an embedded undertie elides two otherwise separate words into one
    // syllable, with the elision content carrying the undertie glyph itself, not a space.
    {
        const auto& note = firstNoteAt(*score, 1, 1);
        const auto& verse1 = findLyric(note.lyrics, "verse", "1");
        EXPECT_EQ(verse1.text, "and");
        ASSERT_EQ(verse1.continuations.size(), 1u);
        EXPECT_EQ(verse1.continuations.at(0).text, "a");
        ASSERT_TRUE(verse1.continuations.at(0).elisionText.has_value());
        EXPECT_EQ(*verse1.continuations.at(0).elisionText, "\xE2\x80\xBF");

        // No edit was made to verse 2's "and", which stays a single, unsplit run.
        const auto& verse2 = findLyric(note.lyrics, "verse", "2");
        EXPECT_EQ(verse2.text, "and");
        EXPECT_TRUE(verse2.continuations.empty());
    }

    // "strength": verse 1 is untouched; verse 2 was edited to embed two NBSPs
    // ("str<NBSP>en<NBSP>gth"), proving the split is a mechanical N-segment rule.
    {
        const auto& note = firstNoteAt(*score, 1, 2);

        const auto& verse1 = findLyric(note.lyrics, "verse", "1");
        EXPECT_EQ(verse1.text, "stre\xE2\x80\x91ngth"); // non-breaking hyphen: not an elision trigger
        EXPECT_TRUE(verse1.continuations.empty());

        const auto& verse2 = findLyric(note.lyrics, "verse", "2");
        EXPECT_EQ(verse2.text, "str");
        EXPECT_EQ(verse2.syllabic, mx::api::LyricSyllabic::single);
        ASSERT_EQ(verse2.continuations.size(), 2u);
        EXPECT_EQ(verse2.continuations.at(0).text, "en");
        EXPECT_EQ(verse2.continuations.at(0).syllabic, mx::api::LyricSyllabic::single);
        EXPECT_EQ(verse2.continuations.at(1).text, "gth");
        EXPECT_EQ(verse2.continuations.at(1).syllabic, mx::api::LyricSyllabic::single);
    }

    // "dai"/"ly"/"food,": a three-note hyphen chain, each note's own syllable additionally split
    // by an internal NBSP in verse 2. Confirms the hyphen-edge-distribution rule: a syllable's
    // real leading hyphen can only land on the first run, its real trailing hyphen only on the
    // last, and a true `middle` syllable ("ly") never emits a literal `middle` on either run.
    {
        const auto& daiNote = firstNoteAt(*score, 2, 0);
        const auto& daiVerse2 = findLyric(daiNote.lyrics, "verse", "2");
        EXPECT_EQ(daiVerse2.text, "d");
        EXPECT_EQ(daiVerse2.syllabic, mx::api::LyricSyllabic::single);
        ASSERT_EQ(daiVerse2.continuations.size(), 1u);
        EXPECT_EQ(daiVerse2.continuations.at(0).text, "ai");
        EXPECT_EQ(daiVerse2.continuations.at(0).syllabic, mx::api::LyricSyllabic::begin);

        const auto& lyNote = firstNoteAt(*score, 2, 1);
        const auto& lyVerse2 = findLyric(lyNote.lyrics, "verse", "2");
        EXPECT_EQ(lyVerse2.text, "l");
        EXPECT_EQ(lyVerse2.syllabic, mx::api::LyricSyllabic::end);
        ASSERT_EQ(lyVerse2.continuations.size(), 1u);
        EXPECT_EQ(lyVerse2.continuations.at(0).text, "y");
        EXPECT_EQ(lyVerse2.continuations.at(0).syllabic, mx::api::LyricSyllabic::begin);

        const auto& foodNote = firstNoteAt(*score, 2, 2);
        const auto& foodVerse1 = findLyric(foodNote.lyrics, "verse", "1");
        // U+E551 (SMuFL lyricsElision, typed literally as text rather than as a structured
        // <elision smufl="..."> value): not an elision trigger either, just like non-breaking
        // hyphen, en dash, and em dash.
        EXPECT_EQ(foodVerse1.text, "fo\xEE\x95\x91od,");
        EXPECT_TRUE(foodVerse1.continuations.empty());

        const auto& foodVerse2 = findLyric(foodNote.lyrics, "verse", "2");
        EXPECT_EQ(foodVerse2.text, "fo");
        EXPECT_EQ(foodVerse2.syllabic, mx::api::LyricSyllabic::end);
        ASSERT_EQ(foodVerse2.continuations.size(), 1u);
        EXPECT_EQ(foodVerse2.continuations.at(0).text, "od,");
        EXPECT_EQ(foodVerse2.continuations.at(0).syllabic, mx::api::LyricSyllabic::single);
    }

    // "thy": em dash is not an elision trigger either, so verse 1's edited syllable stays intact.
    {
        const auto& note = firstNoteAt(*score, 3, 1);
        const auto& verse1 = findLyric(note.lyrics, "verse", "1");
        EXPECT_EQ(verse1.text, "th\xE2\x80\x94y"); // em dash
        EXPECT_TRUE(verse1.continuations.empty());
    }
}

TEST(MusicXmlLyrics, VerseChorusSectionMatchFinaleNameAndNumber)
{
    setupTestDataPaths();
    const auto score = loadScoreData(exportMusicXmlFixture("verse_chorus_section.musx"));
    ASSERT_TRUE(score.has_value());

    // Each Finale lyric block numbers from 1, so the number alone does not identify a line and
    // Verse 1, Chorus 1, and Section 1 all carry number "1". Finale's own export of this fixture
    // does the same; see design-decisions.md.
    std::map<std::pair<std::string, std::string>, size_t> pairs;
    for (const auto& part : score->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        for (const auto& lyric : note.lyrics) {
                            ++pairs[{ lyric.verseName, lyric.verseNumber }];
                        }
                    }
                }
            }
        }
    }

    const auto expected = std::map<std::pair<std::string, std::string>, size_t>{
        { { "verse", "1" }, 2 },   { { "verse", "2" }, 2 },
        { { "chorus", "1" }, 3 },  { { "chorus", "2" }, 3 },
        { { "section", "1" }, 4 }, { { "section", "2" }, 3 },
    };
    EXPECT_EQ(pairs, expected);

    // The same counts as Finale's reference export, which is the point of the pair encoding.
    const auto reference = loadScoreData(
        std::filesystem::path("inputs") / "musicxml" / "verse_chorus_section-ref.musicxml");
    ASSERT_TRUE(reference.has_value());
    std::map<std::pair<std::string, std::string>, size_t> referencePairs;
    for (const auto& part : reference->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        for (const auto& lyric : note.lyrics) {
                            ++referencePairs[{ lyric.verseName, lyric.verseNumber }];
                        }
                    }
                }
            }
        }
    }
    EXPECT_EQ(pairs, referencePairs);
}

TEST(MusicXmlLyrics, VerseOnlyDocumentStillNamesTheBlock)
{
    setupTestDataPaths();
    const auto score = loadScoreData(exportMusicXmlFixture("for_health_and_strength.musx"));
    ASSERT_TRUE(score.has_value());

    // Finale omits `name` when every lyric is a verse. Denigma always emits it, so a file arriving
    // for diagnosis names its blocks without the reader inferring anything. The numbers are plain
    // integers either way, which is what importers place lines by.
    size_t lyricCount = 0;
    for (const auto& part : score->parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        for (const auto& lyric : note.lyrics) {
                            ++lyricCount;
                            EXPECT_EQ(lyric.verseName, "verse");
                            EXPECT_TRUE(lyric.verseNumber == "1" || lyric.verseNumber == "2")
                                << "verseNumber was " << lyric.verseNumber;
                        }
                    }
                }
            }
        }
    }
    EXPECT_GT(lyricCount, 0u);
}

// Counts lyric extension endpoints, which is what a word extension becomes in MusicXML.
namespace {
struct ExtendCounts
{
    size_t starts{};
    size_t stops{};
    size_t untyped{};

    bool operator==(const ExtendCounts&) const = default;
};

std::ostream& operator<<(std::ostream& os, const ExtendCounts& counts)
{
    return os << "starts=" << counts.starts << " stops=" << counts.stops
              << " untyped=" << counts.untyped;
}

ExtendCounts countExtends(const mx::api::ScoreData& score)
{
    ExtendCounts counts;
    for (const auto& part : score.parts) {
        for (const auto& measure : part.measures) {
            for (const auto& staff : measure.staves) {
                for (const auto& [voiceIndex, voice] : staff.voices) {
                    static_cast<void>(voiceIndex);
                    for (const auto& note : voice.notes) {
                        for (const auto& lyric : note.lyrics) {
                            if (!lyric.hasExtend) {
                                continue;
                            }
                            switch (lyric.extendType) {
                            case mx::api::LyricExtendType::start: ++counts.starts; break;
                            case mx::api::LyricExtendType::stop: ++counts.stops; break;
                            default: ++counts.untyped; break;
                            }
                        }
                    }
                }
            }
        }
    }
    return counts;
}
} // namespace

TEST(MusicXmlLyrics, WordExtensionsRequireASpanAndMatchFinale)
{
    setupTestDataPaths();

    // Finale stores most word extensions with both ends on the syllable's own entry, working out
    // how far to draw them at layout time, so LyricAssign::wext marks a candidate rather than a
    // span. Only a shape reaching a different entry becomes a MusicXML extension, and that is what
    // Finale itself exports.
    //
    // for_health_and_strength.musx is the case that proves it: one assignment carries wext, all 27
    // of its wordExt shapes are degenerate, and Finale writes no extension. The syllable continues
    // past the first ending into the second, so none is needed.
    struct Expected
    {
        const char* fixture;
        const char* reference;
    };
    constexpr Expected fixtures[] = {
        { "for_health_and_strength.musx", "for_health_and_strength-ref.musicxml" },
        { "zwei_gesange.musx", "zwei_gesange-ref.musicxml" },
        { "verse_chorus_section.musx", "verse_chorus_section-ref.musicxml" },
    };

    for (const auto& entry : fixtures) {
        const auto ours = loadScoreData(exportMusicXmlFixture(entry.fixture));
        ASSERT_TRUE(ours.has_value()) << entry.fixture;
        const auto reference = loadScoreData(
            std::filesystem::path("inputs") / "musicxml" / entry.reference);
        ASSERT_TRUE(reference.has_value()) << entry.reference;

        const auto oursCounts = countExtends(*ours);
        EXPECT_EQ(oursCounts, countExtends(*reference)) << entry.fixture;
        // A start without its stop, or vice versa, means a degenerate span leaked through.
        EXPECT_EQ(oursCounts.starts, oursCounts.stops) << entry.fixture;
    }
}
