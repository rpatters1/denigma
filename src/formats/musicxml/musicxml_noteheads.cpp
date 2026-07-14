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

#include "musicxml.h"

#include "denigma/classify/entries.h"
#include "denigma/classify/noteheads.h"
#include "mx/api/MarkData.h"
#include "mx/api/NoteData.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

// Attaches a <harmonic> technical mark to the touched (node) note of an artificial harmonic, if
// noteInfo is that note. This is the note conventionally marked in engraved MusicXML, since it is
// already visually distinguished by its diamond notehead.
void applyArtificialHarmonicMark(
    mx::api::NoteData& note,
    const NoteInfoPtr& noteInfo,
    const classify::EntryNoteheadClassification& entryNoteheads)
{
    const auto* harmonics = entryNoteheads.as<classify::entry::ArtificialHarmonics>();
    if (!harmonics) {
        return;
    }
    for (const auto& harmonic : harmonics->harmonics) {
        if (noteInfo.isSameNote(harmonic.touchedNote)) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::harmonic, VerticalPlacement::NotApplicable));
            /// @todo Distinguish natural vs. artificial harmonic type and emit base-pitch/touching-pitch/
            /// sounding-pitch once mx::api exposes Harmonic's choice/choice2 sub-elements. See
            /// "Artificial-harmonic technical detail" in mx-api-gaps.md.
            return;
        }
    }
}

} // namespace

void applyNoteheadData(
    mx::api::NoteData& note,
    const NoteInfoPtr& noteInfo,
    const classify::EntryNoteheadClassification& entryNoteheads)
{
    if (const auto classification = classify::classifyNotehead(noteInfo)) {
        note.notehead = enumConvert<mx::api::Notehead>(classification.shape);
        /// @todo Export notehead fill (filled/unfilled) and, for Shape::Other, the specific SMuFL glyph
        /// name, once mx::api exposes Notehead's `filled`/`smufl` attributes. Also note that
        /// Shape::SmallSlash and Shape::LargeSlash both map to mx::api::Notehead::slash, since mx::api
        /// has no way to select the SMuFL glyph that would distinguish them. See "Notehead fill and
        /// SMuFL glyph refinement" in mx-api-gaps.md.
    }

    applyArtificialHarmonicMark(note, noteInfo, entryNoteheads);
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
