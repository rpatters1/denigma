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

void appendTechniquePlayback(mx::api::DirectionData& direction, const classify::expression::TechniqueText& technique)
{
    // @todo Revisit this when mx::api exposes richer direction playback or technical modeling.
    switch (technique.type) {
    case classify::expression::TechniqueText::Type::Pizzicato:
        direction.soundData.pizzicato = mx::api::Bool::yes;
        direction.isSoundDataSpecified = direction.soundData.isSpecified();
        break;
    case classify::expression::TechniqueText::Type::Arco:
        direction.soundData.pizzicato = mx::api::Bool::no;
        direction.isSoundDataSpecified = direction.soundData.isSpecified();
        break;
    default:
        break;
    }
}

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
    /// @todo When mx::api exposes MusicXML direction system relation, emit standalone
    /// TOP assignments as system="only-top" instead of approximating them by omitting
    /// the explicit staff value.
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

mx::api::RehearsalEnclosure enclosureForTextExpression(
    const MusxInstance<others::MeasureExprAssign>& assignment)
{
    const auto textExpression = assignment ? assignment->getTextExpression() : nullptr;
    if (!textExpression || !textExpression->hasEnclosure) {
        return mx::api::RehearsalEnclosure::none;
    }

    const auto enclosure = textExpression->getEnclosure();
    if (!enclosure) {
        return mx::api::RehearsalEnclosure::none;
    }
    if (enclosure->lineWidth <= 0) {
        return mx::api::RehearsalEnclosure::none;
    }

    if (enclosure->shape == others::Enclosure::Shape::Rectangle) {
        return enclosure->equalAspect ? mx::api::RehearsalEnclosure::square : mx::api::RehearsalEnclosure::rectangle;
    }
    if (enclosure->shape == others::Enclosure::Shape::Ellipse) {
        return enclosure->equalAspect ? mx::api::RehearsalEnclosure::circle : mx::api::RehearsalEnclosure::oval;
    }
    /// @todo Replace the temporary MUSX pentagon-through-octagon fallback to square when mx::api
    /// exposes matching direction text enclosure shapes.
    return enumConvert<mx::api::RehearsalEnclosure>(enclosure->shape);
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
    const auto enclosure = enclosureForTextExpression(assignment);
    for (auto& words : direction.words) {
        words.enclosure = enclosure;
    }

    const double quarterNotesPerMinute = musicXmlQuarterNotesPerMinute(classification.tempoText().tempo);
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
    const auto enclosure = enclosureForTextExpression(assignment);
    for (auto& words : direction.words) {
        words.enclosure = enclosure;
    }
    if (mx::api::isDirectionDataEmpty(direction)) {
        return std::nullopt;
    }
    return direction;
}

std::optional<mx::api::DirectionData> createRehearsalExpressionDirection(
    MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);

    mx::api::RehearsalData rehearsal;
    rehearsal.text = classification.rehearsalMark().text;
    rehearsal.enclosure = enclosureForTextExpression(assignment);
    if (classification.enigmaCtx) {
        const auto chunks = classification.enigmaCtx->collectEnigmaTextChunks(
            EnigmaString::EnigmaParsingOptions(EnigmaString::AccidentalStyle::Unicode));
        const auto chunkIt = std::find_if(chunks.begin(), chunks.end(), [](const auto& chunk) {
            return !chunk.text.empty() && chunk.styles.font && !chunk.styles.font->hidden;
        });
        if (chunkIt != chunks.end()) {
            const auto words = musicXmlWordsFromEnigmaTextChunk(context, *chunkIt);
            if (words) {
                rehearsal.fontData = words->fontData;
            }
        }
    }

    direction.rehearsals.emplace_back(std::move(rehearsal));
    return direction;
}

enum class GroupedDirectionAction
{
    None,
    Emit,
    ReplacePrior
};

} // namespace

double musicXmlQuarterNotesPerMinute(const classify::expression::TempoInfo& tempo)
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
    /// @todo Export harp pedal diagrams here once mx::api exposes a public harp-pedals direction
    /// model. The generated MX core supports `<harp-pedals>`, but the public api layer does not.
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
        const bool isVoiceAttached = assignment->layer > 0 || assignment->voice2;
        const bool singleStaffPart = measure.staves.size() == 1;
        /// @todo Once mx::api can write direction system="only-top|also-top", grouped
        /// TOP/existing-staff combinations should keep both directions and upgrade the
        /// TOP-owned one to also-top instead of replacing or suppressing entries here.
        const bool isStaffValueSpecified = !emittedFromTopStaffAssignment && (!singleStaffPart || isVoiceAttached);
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
        case classify::ExpressionType::KeyboardPedal:
            /// @todo Emit semantic pedal directions when mx::api can represent the complete
            /// keyboard-pedal vocabulary. Its current mark model would lose pedal 2, pedal 3,
            /// half-pedal, special release, hook, and hyphen distinctions.
            [[fallthrough]];
        case classify::ExpressionType::TempoAlteration:
        case classify::ExpressionType::GenericText: {
            emitGroupedDirection(createWordsExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        }
        case classify::ExpressionType::TechniqueText: {
            auto direction = createWordsExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified);
            if (direction) {
                appendTechniquePlayback(*direction, classification.techniqueText());
            }
            emitGroupedDirection(std::move(direction));
            break;
        }
        case classify::ExpressionType::RehearsalMark: {
            emitGroupedDirection(createRehearsalExpressionDirection(
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
            appendArpeggioCandidate(context, classification.nonArpeggio().candidate);
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
