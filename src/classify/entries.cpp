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
#include "denigma/classify/entries.h"

#include <algorithm>
#include <array>
#include <vector>

namespace denigma::classify {

using namespace entry;
using namespace notehead;

namespace {

struct ChordNote
{
    musx::dom::NoteInfoPtr note;
    NoteheadClassification notehead;
};

/// @brief One recognized touch interval, expressed diatonically (so it works in any EDO, not just 12-EDO),
/// plus the diatonic distance from the stopped note up to the theoretical sounding pitch it produces.
struct TouchIntervalDefinition
{
    ArtificialHarmonic::TouchInterval interval;
    int diatonicSteps;          ///< Steps from the stopped note up to the touched node (0 = unison, 3 = fourth, etc.)
    int soundingDiatonicSteps;  ///< Steps from the stopped note up to the theoretical sounding pitch.
};

constexpr std::array<TouchIntervalDefinition, 3> kTouchIntervals = { {
    // Touching a fourth above excites the 4th partial, which sounds 2 octaves above the stopped note.
    { ArtificialHarmonic::TouchInterval::Fourth, 3, 2 * music_theory::STANDARD_DIATONIC_STEPS },
    // Touching a major third above excites the 5th partial, which sounds 2 octaves + a major third above.
    { ArtificialHarmonic::TouchInterval::MajorThird, 2, 2 * music_theory::STANDARD_DIATONIC_STEPS + 2 },
    // Touching a fifth above excites the 3rd partial, which sounds 1 octave + a fifth above.
    { ArtificialHarmonic::TouchInterval::Fifth, 4, music_theory::STANDARD_DIATONIC_STEPS + 4 },
} };

/// @brief Determines whether @p candidate is spelled exactly as @p fromNote transposed up by @p diatonicSteps
/// (a perfect/major diatonic interval, per music_theory::Transposer::chromaticTranspose's convention of
/// 0 = perfect/major), in whatever EDO the note's key signature uses.
/// @note This intentionally checks exact spelling (displacement and alteration) rather than enharmonic
/// pitch equivalence. Harmonics are always notated with the plain interval (e.g. a perfect fourth), never
/// an enharmonic respelling (e.g. an augmented third), so an exact-spelling match is both sufficient and
/// more precise than an enharmonic-equivalence check would be.
bool isSpelledAtInterval(const musx::dom::NoteInfoPtr& fromNote, int diatonicSteps, const musx::dom::NoteInfoPtr& candidate)
{
    const auto transposer = fromNote.createTransposer();
    transposer->chromaticTranspose(diatonicSteps, 0);
    return transposer->displacement() == candidate->harmLev && transposer->alteration() == candidate->harmAlt;
}

} // namespace

EntryNoteheadClassification classifyEntryNoteheads(const musx::dom::EntryInfoPtr& entryInfo)
{
    if (!entryInfo) {
        return {};
    }
    const auto musxEntry = entryInfo->getEntry();
    if (!musxEntry->isNote) {
        return {};
    }
    const size_t noteCount = musxEntry->notes.size();
    if (noteCount < 2) {
        return {};
    }

    std::vector<ChordNote> chordNotes;
    chordNotes.reserve(noteCount);
    for (size_t index = 0; index < noteCount; ++index) {
        musx::dom::NoteInfoPtr note(entryInfo, index);
        chordNotes.push_back({ note, classifyNotehead(note) });
    }

    std::vector<bool> used(noteCount, false);
    std::vector<ArtificialHarmonic> harmonics;

    for (size_t touchedIndex = 0; touchedIndex < noteCount; ++touchedIndex) {
        if (used[touchedIndex] || chordNotes[touchedIndex].notehead.shape != Shape::Diamond) {
            continue;
        }
        for (size_t stoppedIndex = 0; stoppedIndex < noteCount; ++stoppedIndex) {
            if (stoppedIndex == touchedIndex || used[stoppedIndex]
                || chordNotes[stoppedIndex].notehead.shape != Shape::Regular) {
                continue;
            }
            const auto& stoppedNote = chordNotes[stoppedIndex].note;
            const auto& touchedNote = chordNotes[touchedIndex].note;

            const auto matchedInterval = std::find_if(kTouchIntervals.begin(), kTouchIntervals.end(),
                [&](const TouchIntervalDefinition& candidate) {
                    return isSpelledAtInterval(stoppedNote, candidate.diatonicSteps, touchedNote);
                });
            if (matchedInterval == kTouchIntervals.end()) {
                continue;
            }

            ArtificialHarmonic harmonic;
            harmonic.stoppedNote = stoppedNote;
            harmonic.touchedNote = touchedNote;
            harmonic.stoppedNotehead = chordNotes[stoppedIndex].notehead;
            harmonic.touchedNotehead = chordNotes[touchedIndex].notehead;
            harmonic.interval = matchedInterval->interval;
            used[touchedIndex] = true;
            used[stoppedIndex] = true;

            for (size_t soundingIndex = 0; soundingIndex < noteCount; ++soundingIndex) {
                if (used[soundingIndex]) {
                    continue;
                }
                if (isSpelledAtInterval(stoppedNote, matchedInterval->soundingDiatonicSteps, chordNotes[soundingIndex].note)) {
                    harmonic.soundingNote = chordNotes[soundingIndex].note;
                    used[soundingIndex] = true;
                    break;
                }
            }

            harmonics.push_back(std::move(harmonic));
            break;
        }
    }

    EntryNoteheadClassification result;
    if (!harmonics.empty()) {
        result.value = ArtificialHarmonics{ std::move(harmonics) };
    }
    return result;
}

} // namespace denigma::classify
