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
#include <unordered_map>
#include <utility>

#include "denigma/classify/expressions.h"
#include "musicxml_formatted_text.h"
#include "mx/api/DirectionData.h"
#include "mx/api/MarkData.h"
#include "mx/api/NoteData.h"
#include "mx/api/StaffData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

mx::api::DirectionData createExpressionDirection(
    const MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    VerticalPlacement placement,
    bool isStaffValueSpecified = true)
{
    auto direction = mx::api::DirectionData{};
    direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(Fraction::fromEdu(assignment->eduPosition));
    direction.placement = enumConvert<mx::api::Placement>(placement);
    direction.isStaffValueSpecified = isStaffValueSpecified;
    if (assignment->layer > 0 || assignment->voice2) {
        const LayerIndex layer = assignment->layer > 0 ? assignment->layer - 1 : 0;
        direction.voice = musicXmlVoiceNumber(staffIndex, layer, assignment->voice2 ? 2 : 1);
    }
    return direction;
}

bool isTopStaffAssignment(const MusxInstance<others::MeasureExprAssign>& assignment)
{
    return assignment->staffAssign == static_cast<StaffCmper>(others::StaffList::FloatingValues::TopStaff);
}

std::optional<mx::api::DirectionData> createTempoExpressionDirection(
    MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    if (classification.enigmaCtx) {
        direction.words = musicXmlWordsFromEnigmaText(context, *classification.enigmaCtx);
    }

    const double quarterNotesPerMinute = musicXmlQuarterNotesPerMinute(classification.tempoMark().tempo);
    if (quarterNotesPerMinute >= 0.0) {
        direction.soundData.tempo = quarterNotesPerMinute;
        direction.isSoundDataSpecified = direction.soundData.isSpecified();
    }

    if (mx::api::isDirectionDataEmpty(direction)) {
        return std::nullopt;
    }
    return direction;
}

std::optional<mx::api::DirectionData> createWordsExpressionDirection(
    MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    if (classification.enigmaCtx) {
        direction.words = musicXmlWordsFromEnigmaText(context, *classification.enigmaCtx);
    }
    if (mx::api::isDirectionDataEmpty(direction)) {
        return std::nullopt;
    }
    return direction;
}

enum class GroupedDirectionAction
{
    None,
    Emit,
    ReplacePrior
};

} // namespace

double musicXmlQuarterNotesPerMinute(const classify::TempoInfo& tempo)
{
    if (tempo.beatsPerMinute <= 0 || tempo.beatUnitEdu <= 0) {
        return mx::api::DOUBLE_UNSPECIFIED;
    }

    constexpr EduFloat eduPerQuarterNote = EduFloat(NoteType::Quarter);
    return static_cast<double>(tempo.beatsPerMinute) * static_cast<double>(tempo.beatUnitEdu) / eduPerQuarterNote;
}

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

    struct DirectionGroupTracking
    {
        size_t directionIndex{};
        bool emittedFromTopStaffAssignment{};
    };

    const auto exprAssigns = context.document->getOthers()->getArray<others::MeasureExprAssign>(
        musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
    const auto cuePlanIt = context.cueDiscardPlansByMeasureStaff.find(musicXmlMeasureStaffKey(musxMeasure->getCmper(), staffId));
    std::unordered_map<int, DirectionGroupTracking> directionGroups;
    for (const auto& assignment : exprAssigns) {
        if (assignment->hidden) {
            continue;
        }
        if (!assignment->calcIsAssignedInRequestedPart()) {
            continue;
        }

        const StaffCmper assignedStaffId = assignment->calcAssignedStaffId(false);
        if (assignedStaffId != staffId) {
            continue;
        }
        if (cuePlanIt != context.cueDiscardPlansByMeasureStaff.end()
            && assignment->layer > 0
            && cuePlanIt->second.skipsLayer(assignment->layer - 1)) {
            continue;
        }

        const auto classification = classify::classifyExpression(assignment);
        const auto placement = assignment->calcVerticalPlacement();
        const bool emittedFromTopStaffAssignment = isTopStaffAssignment(assignment);
        const bool isStaffValueSpecified = !emittedFromTopStaffAssignment;
        GroupedDirectionAction groupedDirectionAction = GroupedDirectionAction::Emit;
        DirectionGroupTracking* groupTracking = nullptr;
        if (assignment->staffGroup > 0) {
            const auto groupIt = directionGroups.find(assignment->staffGroup);
            if (groupIt == directionGroups.end()) {
                groupedDirectionAction = GroupedDirectionAction::Emit;
            } else if (groupIt->second.emittedFromTopStaffAssignment && assignment->staffAssign >= 0) {
                groupedDirectionAction = GroupedDirectionAction::ReplacePrior;
                groupTracking = &groupIt->second;
            } else {
                groupedDirectionAction = GroupedDirectionAction::None;
            }
        }

        auto emitGroupedDirection = [&](std::optional<mx::api::DirectionData> direction) {
            if (!direction || groupedDirectionAction == GroupedDirectionAction::None) {
                return;
            }
            if (groupedDirectionAction == GroupedDirectionAction::ReplacePrior) {
                staff.directions[groupTracking->directionIndex] = std::move(*direction);
                groupTracking->emittedFromTopStaffAssignment = false;
                return;
            }
            staff.directions.emplace_back(std::move(*direction));
            if (assignment->staffGroup > 0) {
                directionGroups.emplace(assignment->staffGroup, DirectionGroupTracking{ staff.directions.size() - 1, emittedFromTopStaffAssignment });
            }
        };
        
        switch (classification.type) {
        case classify::ExpressionType::Dynamic: {
            const auto directions = createDynamicExpressionDirections(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified);
            if (directions.empty() || groupedDirectionAction == GroupedDirectionAction::None) {
                break;
            }
            bool firstDirectionHandled = false;
            for (auto direction : directions) {
                if (groupedDirectionAction == GroupedDirectionAction::ReplacePrior && !firstDirectionHandled) {
                    staff.directions[groupTracking->directionIndex] = std::move(direction);
                    groupTracking->emittedFromTopStaffAssignment = false;
                } else {
                    staff.directions.emplace_back(std::move(direction));
                    if (!firstDirectionHandled && assignment->staffGroup > 0 && groupedDirectionAction == GroupedDirectionAction::Emit) {
                        directionGroups.emplace(assignment->staffGroup,
                            DirectionGroupTracking{ staff.directions.size() - 1, emittedFromTopStaffAssignment });
                    }
                }
                firstDirectionHandled = true;
            }
            break;
        }
        case classify::ExpressionType::TempoMark: {
            emitGroupedDirection(createTempoExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        }
        case classify::ExpressionType::TempoAlteration:
        case classify::ExpressionType::GenericText: {
            emitGroupedDirection(createWordsExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        }
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
