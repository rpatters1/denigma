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

#include "mx/api/MarkDataChoice.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

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

} // namespace

void appendArpeggioCandidate(MusicXmlMusxMapping& context, const ArpeggioSpanCandidate& candidate)
{
    if (context.deferredArpeggioCandidateKeys.emplace(candidate.key()).second) {
        context.deferredArpeggioCandidates.emplace_back(candidate);
    }
}

void finalizeArpeggioCandidates(MusicXmlMusxMapping& context)
{
    for (const auto& candidate : context.deferredArpeggioCandidates) {
        if (candidate.type != ArpeggioSpanType::Bracket) {
            continue;
        }
        auto* topNote = findArpeggioBoundaryNote(context, candidate.topEntry, candidate.bottomEntry, true);
        auto* bottomNote = findArpeggioBoundaryNote(context, candidate.bottomEntry, candidate.topEntry, false);
        if (!topNote || !bottomNote) {
            const auto sourceEntryNumber = candidate.sourceEntry ? candidate.sourceEntry->getEntry()->getEntryNumber() : 0;
            context.logMessage(LogMsg() << "Non-arpeggio at entry " << sourceEntryNumber
                << " could not be attached to its MusicXML endpoint notes.", MessageSeverity::Info);
            continue;
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
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
