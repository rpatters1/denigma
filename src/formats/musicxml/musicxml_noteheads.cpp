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
    using Fill = classify::notehead::Fill;
    using Shape = classify::notehead::Shape;
    if (const auto classification = classify::classifyNotehead(noteInfo)) {
        note.notehead = enumConvert<mx::api::Notehead>(classification.shape);

        // Absent the filled attribute, MusicXML draws the notehead from the note's duration: hollow
        // for a half note and longer, solid for a quarter note and shorter. Write the attribute only
        // where Finale's glyph disagrees with that default, as Finale's own export does.
        const bool defaultsToFilled = note.durationData.durationName >= mx::api::DurationName::quarter;
        if (classification.fill == Fill::Filled && !defaultsToFilled) {
            note.noteheadFilled = mx::api::Bool::yes;
        } else if (classification.fill == Fill::Unfilled && defaultsToFilled) {
            note.noteheadFilled = mx::api::Bool::no;
        }

        // The MusicXML value identifies the glyph for every shape Denigma models except these two:
        // `other` carries no visual information at all, and both slash sizes share the `slash` value.
        // Naming the glyph lets a consumer draw what Finale drew.
        switch (classification.shape) {
        case Shape::Other:
        case Shape::SmallSlash:
        case Shape::LargeSlash:
            note.noteheadSmufl = classification.glyphName;
            break;
        default:
            break;
        }
    }

    applyArtificialHarmonicMark(note, noteInfo, entryNoteheads);
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
