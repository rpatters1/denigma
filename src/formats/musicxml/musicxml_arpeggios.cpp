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

#include <utility>
#include <vector>

#include "mx/api/MarkDataChoice.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

/// Upper bound of the MusicXML `number-level` type, which the `number` attribute uses.
constexpr int MAX_ARPEGGIO_NUMBER_LEVEL = 16;

mx::api::NoteData* findArpeggioBoundaryNote(
    MusicXmlMusxMapping& context,
    const EntryInfoPtr& preferredEntry,
    const EntryInfoPtr& fallbackEntry,
    bool top)
{
    const auto findInEntry = [&](const EntryInfoPtr& entryInfo) -> mx::api::NoteData* {
        if (!entryInfo) {
            return nullptr;
        }
        const auto entry = entryInfo->getEntry();
        const auto findAtIndex = [&](size_t noteIndex) -> mx::api::NoteData* {
            const NoteInfoPtr noteInfo(entryInfo, noteIndex);
            const auto locationIt = context.noteLocations.find(musicXmlNoteKey(entry->getEntryNumber(), noteInfo->getNoteId()));
            return locationIt == context.noteLocations.end() ? nullptr : noteDataAt(context, locationIt->second);
        };
        if (top) {
            for (size_t noteIndex = entry->notes.size(); noteIndex-- > 0; ) {
                if (auto* note = findAtIndex(noteIndex)) {
                    return note;
                }
            }
        } else {
            for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
                if (auto* note = findAtIndex(noteIndex)) {
                    return note;
                }
            }
        }
        return nullptr;
    };

    if (auto* note = findInEntry(preferredEntry)) {
        return note;
    }
    return findInEntry(fallbackEntry);
}

void collectEntryNotes(MusicXmlMusxMapping& context, const EntryInfoPtr& entryInfo, std::vector<mx::api::NoteData*>& notes)
{
    if (!entryInfo) {
        return;
    }
    const auto entry = entryInfo->getEntry();
    for (size_t noteIndex = 0; noteIndex < entry->notes.size(); ++noteIndex) {
        const NoteInfoPtr noteInfo(entryInfo, noteIndex);
        const auto locationIt = context.noteLocations.find(musicXmlNoteKey(entry->getEntryNumber(), noteInfo->getNoteId()));
        if (locationIt == context.noteLocations.end()) {
            continue;
        }
        if (auto* note = noteDataAt(context, locationIt->second)) {
            notes.emplace_back(note);
        }
    }
}

EntryNumber sourceEntryNumber(const ArpeggioSpanCandidate& candidate)
{
    return candidate.sourceEntry ? candidate.sourceEntry->getEntry()->getEntryNumber() : 0;
}

void appendNonArpeggiate(MusicXmlMusxMapping& context, const ArpeggioSpanCandidate& candidate)
{
    auto* topNote = findArpeggioBoundaryNote(context, candidate.topEntry, candidate.bottomEntry, true);
    auto* bottomNote = findArpeggioBoundaryNote(context, candidate.bottomEntry, candidate.topEntry, false);
    if (!topNote || !bottomNote) {
        context.logMessage(LogMsg() << "Non-arpeggio at entry " << sourceEntryNumber(candidate)
            << " could not be attached to its MusicXML endpoint notes.", MessageSeverity::Info);
        return;
    }

    auto topMark = mx::api::MarkData(mx::api::MarkType::nonArpeggiate);
    mx::api::NonArpeggiateMarkData topMarkData;
    topMarkData.placement = mx::api::NonArpeggiatePlacement::top;
    topMark.choice = topMarkData;
    topNote->noteAttachmentData.marks.emplace_back(std::move(topMark));

    auto bottomMark = mx::api::MarkData(mx::api::MarkType::nonArpeggiate);
    mx::api::NonArpeggiateMarkData bottomMarkData;
    bottomMarkData.placement = mx::api::NonArpeggiatePlacement::bottom;
    bottomMark.choice = bottomMarkData;
    bottomNote->noteAttachmentData.marks.emplace_back(std::move(bottomMark));
}

void appendArpeggiate(MusicXmlMusxMapping& context, const ArpeggioSpanCandidate& candidate, int& crossEntryCount)
{
    // MusicXML draws the roll from an <arpeggiate> on every note it passes through, unlike the
    // bracket, which marks only its two ends.
    std::vector<mx::api::NoteData*> notes;
    collectEntryNotes(context, candidate.topEntry, notes);
    const size_t topNoteCount = notes.size();
    if (candidate.topEntry && candidate.bottomEntry && !candidate.topEntry.isSameEntry(candidate.bottomEntry)) {
        collectEntryNotes(context, candidate.bottomEntry, notes);
    }
    if (notes.empty()) {
        context.logMessage(LogMsg() << "Arpeggio at entry " << sourceEntryNumber(candidate)
            << " could not be attached to any MusicXML note.", MessageSeverity::Info);
        return;
    }

    mx::api::ArpeggiateMarkData arpeggiateData;
    if (topNoteCount > 0 && notes.size() > topNoteCount) {
        // One roll drawn across two of this part's entries, which MusicXML expresses by giving every
        // note of both the same number and asking for an unbroken line. The number only has to
        // separate rolls that sound together, so cycling it through the schema's range keeps it in
        // bounds. A span whose other end left this part reaches here with one entry's notes and
        // takes neither attribute.
        arpeggiateData.number = (crossEntryCount++ % MAX_ARPEGGIO_NUMBER_LEVEL) + 1;
        arpeggiateData.unbroken = mx::api::Bool::yes;
    }

    // The MusicXML `direction` attribute is the arrowhead, so an arrowless roll takes no direction.
    const auto markType = enumConvert<mx::api::MarkType>(candidate.arrow);
    for (auto* note : notes) {
        auto mark = mx::api::MarkData(markType);
        mark.choice = arpeggiateData;
        note->noteAttachmentData.marks.emplace_back(std::move(mark));
    }
}

} // namespace

void appendArpeggioCandidate(MusicXmlMusxMapping& context, const ArpeggioSpanCandidate& candidate)
{
    if (context.deferredArpeggioCandidateKeys.emplace(candidate.key()).second) {
        context.deferredArpeggioCandidates.emplace_back(candidate);
    }
}

void finalizeArpeggioCandidates(MusicXmlMusxMapping& context)
{
    int crossEntryCount = 0;
    for (const auto& candidate : context.deferredArpeggioCandidates) {
        switch (candidate.type) {
        case ArpeggioSpanType::Bracket:
            appendNonArpeggiate(context, candidate);
            break;
        case ArpeggioSpanType::Normal:
            appendArpeggiate(context, candidate, crossEntryCount);
            break;
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
