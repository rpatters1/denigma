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

#include "denigma/classify/expressions.h"
#include "mx/api/MarkData.h"
#include "mx/api/NoteData.h"
#include "mx/api/StaffData.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

void processExpressions(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex)
{
    (void)measure;
    (void)staffIndex;
    if (!musxMeasure->hasExpression) {
        return;
    }

    auto noteForEntry = [&](const EntryInfoPtr& entryInfo) -> mx::api::NoteData* {
        if (!entryInfo) {
            return nullptr;
        }
        const auto locationIt = context.entryNumberToFirstNote.find(entryInfo->getEntry()->getEntryNumber());
        if (locationIt == context.entryNumberToFirstNote.end()) {
            return nullptr;
        }
        const auto& location = locationIt->second;
        const auto voiceIndex = static_cast<size_t>(location.userVoiceNumber - 1);
        const auto voiceIt = staff.voices.find(voiceIndex);
        ASSERT_IF(voiceIt == staff.voices.end()) {
            return nullptr;
        }

        auto& voice = voiceIt->second;
        ASSERT_IF(location.noteIndex >= voice.notes.size()) {
            return nullptr;
        }
        return &voice.notes[location.noteIndex];
    };

    auto appendMarkToAssociatedNote = [&](const MusxInstance<others::MeasureExprAssign>& assignment, mx::api::MarkData mark) {
        if (auto* note = noteForEntry(assignment->calcAssociatedEntry())) {
            note->noteAttachmentData.marks.emplace_back(std::move(mark));
        } else {
            context.logMessage(LogMsg() << "Expression in measure " << musxMeasure->getCmper()
                << " could not be attached to a MusicXML note.", MessageSeverity::Info);
        }
    };

    const auto exprAssigns = context.document->getOthers()->getArray<others::MeasureExprAssign>(
        musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
    for (const auto& assignment : exprAssigns) {
        if (assignment->hidden || assignment->staffAssign != staffId) {
            continue;
        }
        if (!assignment->calcIsAssignedInRequestedPart()) {
            continue;
        }

        const auto classification = classify::classifyExpression(assignment);
        const auto placement = assignment->calcVerticalPlacement();
        for (const auto& run : classification.runs) {
            if (const auto* dynamic = run.as<classify::DynamicMark>()) {
                appendDynamicExpression(context, staff, staffIndex, assignment, *dynamic, placement);
            }
        }
        switch (classification.type) {
        case classify::ExpressionType::Dynamic:
            break;
        case classify::ExpressionType::Fermata: {
            const auto& fermata = classification.fermata();
            if (!assignment->calcIsPartOfStaffListAssignment() && !fermata.isRightBarline) {
                appendMarkToAssociatedNote(assignment, musicXmlMark(musicXmlFermataType(fermata.fermata), placement));
            }
            break;
        }
        case classify::ExpressionType::BreathMark:
            appendMarkToAssociatedNote(assignment, musicXmlMark(mx::api::MarkType::breathMark, placement));
            break;
        case classify::ExpressionType::NonArpeggio:
            /// @todo Export when mx::api support it.
            break;
        case classify::ExpressionType::Error:
            context.logMessage(LogMsg() << classification.error().message, MessageSeverity::Warning);
            break;
        default:
            break;
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
