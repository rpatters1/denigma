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

#include <optional>
#include <utility>

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

// Attaches a <harmonic> technical mark stating noteInfo's role in an artificial harmonic, if noteInfo
// is one of its notes. MusicXML's <harmonic> is a per-note notation whose pitch child names the pitch
// that this note's own notehead states, so every note of the pattern carries its own mark rather than
// depending on its neighbor for meaning.
void applyArtificialHarmonicMark(
    mx::api::NoteData& note,
    const NoteInfoPtr& noteInfo,
    const classify::EntryNoteheadClassification& entryNoteheads)
{
    const auto* harmonics = entryNoteheads.as<classify::entry::ArtificialHarmonics>();
    if (!harmonics) {
        return;
    }
    const auto harmonicPitch = [&]() -> std::optional<mx::api::HarmonicPitch> {
        for (const auto& harmonic : harmonics->harmonics) {
            // The stopped note is played before touching, so its notehead states the base pitch. The
            // touched note is the lightly touched node. A sounding note is present only where the
            // source explicitly writes out the pitch that is heard.
            if (noteInfo.isSameNote(harmonic.stoppedNote)) {
                return mx::api::HarmonicPitch::basePitch;
            }
            if (noteInfo.isSameNote(harmonic.touchedNote)) {
                return mx::api::HarmonicPitch::touchingPitch;
            }
            if (harmonic.soundingNote && noteInfo.isSameNote(harmonic.soundingNote)) {
                return mx::api::HarmonicPitch::soundingPitch;
            }
        }
        return std::nullopt;
    }();
    if (!harmonicPitch) {
        return;
    }
    auto mark = musicXmlMark(mx::api::MarkType::harmonic, VerticalPlacement::NotApplicable);
    // The <harmonic> attributes govern the circular harmonic symbol, which Finale does not draw for
    // these chords. Leave print-object unspecified rather than writing print-object="no": the
    // <artificial/> child already tells a consumer this is not the circle case.
    mark.choice = mx::api::HarmonicMarkData{ mx::api::HarmonicKind::artificial, *harmonicPitch };
    note.noteAttachmentData.marks.emplace_back(std::move(mark));
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
