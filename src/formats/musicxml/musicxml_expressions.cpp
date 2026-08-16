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
#include "mx/api/TempoData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

mx::api::HarpPedalsData musicXmlHarpPedals(const classify::expression::HarpDiagram& diagram)
{
    using PedalPosition = classify::expression::HarpDiagram::PedalPosition;

    constexpr int FLAT_ALTERATION = -1;
    constexpr int NATURAL_ALTERATION = 0;
    constexpr int SHARP_ALTERATION = 1;
    auto alteration = [](PedalPosition position) {
        switch (position) {
        case PedalPosition::Flat: return FLAT_ALTERATION;
        case PedalPosition::Natural: return NATURAL_ALTERATION;
        case PedalPosition::Sharp: return SHARP_ALTERATION;
        }
        return NATURAL_ALTERATION;
    };

    mx::api::HarpPedalsData result;
    result.pedalTunings = {
        { mx::api::Step::d, alteration(diagram.d) },
        { mx::api::Step::c, alteration(diagram.c) },
        { mx::api::Step::b, alteration(diagram.b) },
        { mx::api::Step::e, alteration(diagram.e) },
        { mx::api::Step::f, alteration(diagram.f) },
        { mx::api::Step::g, alteration(diagram.g) },
        { mx::api::Step::a, alteration(diagram.a) }
    };
    return result;
}

std::optional<mx::api::AccordionRegistrationData> musicXmlAccordionRegistration(
    const classify::articulation::AccordionRegistration& registration,
    VerticalPlacement placement)
{
    using DotPosition = classify::articulation::AccordionRegistration::DotPosition;

    mx::api::AccordionRegistrationData result;
    int middleDots = 0;
    for (const auto& dot : registration.dots) {
        switch (dot.position) {
        case DotPosition::Top:
            result.high = true;
            break;
        case DotPosition::UpperMiddle:
        case DotPosition::Middle:
        case DotPosition::LowerMiddle:
            ++middleDots;
            break;
        case DotPosition::Bottom:
            result.low = true;
            break;
        case DotPosition::Other:
            return std::nullopt;
        }
    }
    if (middleDots > 3) {
        return std::nullopt;
    }
    if (middleDots > 0) {
        result.middle = middleDots;
    }
    result.positionData.placement = enumConvert<mx::api::Placement>(placement);
    return result;
}

bool isTopStaffAssignment(const MusxInstance<others::MeasureExprAssign>& assignment)
{
    return assignment->staffAssign == static_cast<StaffCmper>(others::StaffList::FloatingValues::TopStaff);
}

mx::api::HorizontalAlignment musicXmlHorizontalAlignmentForTextExpression(
    const MusxInstance<others::MeasureExprAssign>& assignment)
{
    const auto textExpression = assignment ? assignment->getTextExpression() : nullptr;
    // horzMeasExprAlign chooses the musical landmark used as the anchor. The separate
    // horzExprJustification says which edge of the expression sits at that anchor: MusicXML halign.
    return textExpression ? enumConvert<mx::api::HorizontalAlignment>(textExpression->horzExprJustification)
                          : mx::api::HorizontalAlignment::unspecified;
}

mx::api::HorizontalAlignment musicXmlJustifyForTextExpression(
    const MusxInstance<others::MeasureExprAssign>& assignment)
{
    const auto textExpression = assignment ? assignment->getTextExpression() : nullptr;
    const auto textBlock = textExpression ? textExpression->getTextBlock() : nullptr;
    // TextBlock::justify controls the alignment of lines inside the text, which is MusicXML justify.
    return textBlock ? enumConvert<mx::api::HorizontalAlignment>(textBlock->justify)
                     : mx::api::HorizontalAlignment::unspecified;
}

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
    // A Finale TOP assignment draws the expression on the top staff of every system rather than on
    // one part, which is what MusicXML system="only-top" means. The staff value is left off to match.
    if (isTopStaffAssignment(assignment)) {
        direction.systemRelation = mx::api::SystemRelation::onlyTop;
    }
    direction.isStaffValueSpecified = isStaffValueSpecified;
    if (assignment->layer > 0 || assignment->voice2) {
        const LayerIndex layer = assignment->layer > 0 ? assignment->layer - 1 : 0;
        direction.voice = musicXmlVoiceNumber(staffIndex, layer, assignment->voice2 ? 2 : 1);
    }
    return direction;
}

/// The enclosure for a text expression. An absent or degenerate Finale enclosure is unspecified;
/// callers exporting an element whose MusicXML default is not `none` must override it explicitly.
mx::api::Enclosure enclosureForTextExpression(
    const MusxInstance<others::MeasureExprAssign>& assignment)
{
    const auto textExpression = assignment ? assignment->getTextExpression() : nullptr;
    if (!textExpression || !textExpression->hasEnclosure) {
        return mx::api::Enclosure::unspecified;
    }

    const auto enclosure = textExpression->getEnclosure();
    if (!enclosure) {
        return mx::api::Enclosure::unspecified;
    }
    if (enclosure->lineWidth <= 0) {
        return mx::api::Enclosure::unspecified;
    }

    if (enclosure->shape == others::Enclosure::Shape::Rectangle) {
        return enclosure->equalAspect ? mx::api::Enclosure::square : mx::api::Enclosure::rectangle;
    }
    if (enclosure->shape == others::Enclosure::Shape::Ellipse) {
        return enclosure->equalAspect ? mx::api::Enclosure::circle : mx::api::Enclosure::oval;
    }
    return enumConvert<mx::api::Enclosure>(enclosure->shape);
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
    const classify::expression::TempoInfo* tempo = nullptr;
    if (const auto* metronomeMark = classification.as<classify::expression::MetronomeMark>()) {
        auto musicXmlMetronome = musicXmlMetronomeMark(context, assignment, classification);
        direction.directionTypes.emplace_back(mx::api::DirectionChoice(std::move(musicXmlMetronome)));
        tempo = &metronomeMark->tempo;
    } else {
        auto words = std::vector<mx::api::WordsChoice>{};
        if (classification.enigmaCtx) {
            words = musicXmlWordsFromEnigmaText(context, *classification.enigmaCtx);
        }
        const auto enclosure = enclosureForTextExpression(assignment);
        const auto horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
        const auto justify = musicXmlJustifyForTextExpression(assignment);
        forEachMusicXmlWordsRunItem(words, [&](mx::api::PositionData& positionData,
                mx::api::Enclosure& itemEnclosure, mx::api::HorizontalAlignment& itemJustify) {
            positionData.horizontalAlignment = horizontalAlignment;
            itemEnclosure = enclosure;
            itemJustify = justify;
        });
        appendMusicXmlWordsRun(direction, std::move(words));
        tempo = &classification.tempoText().tempo;
    }

    const double quarterNotesPerMinute = musicXmlQuarterNotesPerMinute(*tempo);
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
    auto words = std::vector<mx::api::WordsChoice>{};
    if (classification.enigmaCtx) {
        words = musicXmlWordsFromEnigmaText(context, *classification.enigmaCtx);
    }
    const auto enclosure = enclosureForTextExpression(assignment);
    const auto horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    const auto justify = musicXmlJustifyForTextExpression(assignment);
    forEachMusicXmlWordsRunItem(words, [&](mx::api::PositionData& positionData,
            mx::api::Enclosure& itemEnclosure, mx::api::HorizontalAlignment& itemJustify) {
        positionData.horizontalAlignment = horizontalAlignment;
        itemEnclosure = enclosure;
        itemJustify = justify;
    });
    appendMusicXmlWordsRun(direction, std::move(words));
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
    if (rehearsal.enclosure == mx::api::Enclosure::unspecified) {
        // Unlike words and symbols, MusicXML rehearsal marks default to a square enclosure.
        rehearsal.enclosure = mx::api::Enclosure::none;
    }
    rehearsal.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    rehearsal.justify = musicXmlJustifyForTextExpression(assignment);
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

    direction.directionTypes.emplace_back(std::move(rehearsal));
    return direction;
}

std::optional<mx::api::DirectionData> createStringMuteExpressionDirection(
    const MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    auto stringMute = mx::api::StringMuteData{};
    stringMute.type = enumConvert<mx::api::StringMuteType>(classification.stringMute().type);
    stringMute.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    direction.directionTypes.emplace_back(std::move(stringMute));
    return direction;
}

std::optional<mx::api::DirectionData> createHarpDiagramExpressionDirection(
    const MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    auto harpPedals = musicXmlHarpPedals(classification.harpDiagram());
    harpPedals.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    direction.directionTypes.emplace_back(std::move(harpPedals));
    return direction;
}

std::optional<mx::api::DirectionData> createAccordionRegistrationExpressionDirection(
    const MusicXmlMusxMapping& context,
    size_t staffIndex,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    VerticalPlacement placement,
    bool isStaffValueSpecified)
{
    const auto accordion = musicXmlAccordionRegistration(classification.accordionRegistration(), placement);
    if (!accordion) {
        return std::nullopt;
    }
    auto direction = createExpressionDirection(context, staffIndex, assignment, placement, isStaffValueSpecified);
    auto accordionData = std::move(*accordion);
    accordionData.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    if (classification.enigmaCtx) {
        if (const auto font = classify::singleVisibleFont(*classification.enigmaCtx); font && font->calcIsSMuFL()) {
            accordionData.fontData = context.musicXmlFontDataFromFontInfo(*font);
        }
    }
    direction.directionTypes.emplace_back(std::move(accordionData));
    return direction;
}

enum class GroupedDirectionAction
{
    None,
    Emit,
    ReplacePrior
};

/// Returns true if every staff of the measure contains nothing but a full-measure rest. The notes
/// for all staves are built before any expression is processed, so this inspects finished data.
bool measureHoldsOnlyFullMeasureRests(const mx::api::MeasureData& measure)
{
    if (measure.staves.empty()) {
        return false;
    }
    for (const auto& staff : measure.staves) {
        const mx::api::NoteData* onlyNote = nullptr;
        for (const auto& voiceEntry : staff.voices) {
            for (const auto& note : voiceEntry.second.notes) {
                if (onlyNote) {
                    return false;
                }
                onlyNote = &note;
            }
        }
        if (!onlyNote || !onlyNote->isRest || !onlyNote->isMeasureRest) {
            return false;
        }
    }
    return true;
}

/// Decides what to do with an expression that carries a multimeasure rest number. Returns true when
/// the expression has been accounted for and must not also be emitted as text.
bool applyMultimeasureRestNumber(
    mx::api::MeasureData& measure,
    const classify::ExpressionClassification& classification)
{
    const int number = classification.multimeasureRestNumber().number;
    if (measure.multiMeasureRest > 0) {
        // The <multiple-rest> element already renders this number. A number that disagrees with it
        // is not describing this rest, so let it through as text.
        return number == measure.multiMeasureRest;
    }
    // A number over an otherwise empty measure stands in for a one-bar multimeasure rest. Symbol
    // style keeps the measure's whole rest instead of turning it into an H-bar.
    if (number == 1 && measureHoldsOnlyFullMeasureRests(measure)) {
        measure.multiMeasureRest = 1;
        measure.multiMeasureRestUseSymbols = mx::api::Bool::yes;
        return true;
    }
    return false;
}

} // namespace

double musicXmlQuarterNotesPerMinute(const classify::expression::TempoInfo& tempo)
{
    if (tempo.beatsPerMinute <= 0 || tempo.beatUnitEdu <= 0) {
        return mx::api::DOUBLE_UNSPECIFIED;
    }

    constexpr EduFloat eduPerQuarterNote = EduFloat(NoteType::Quarter);
    return static_cast<double>(tempo.beatsPerMinute) * static_cast<double>(tempo.beatUnitEdu) / eduPerQuarterNote;
}

mx::api::TempoData musicXmlMetronomeMark(
    const MusicXmlMusxMapping& context,
    const MusxInstance<others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification)
{
    const auto& metronomeMark = classification.metronomeMark();
    mx::api::BeatsPerMinute beatsPerMinute;
    beatsPerMinute.durationName = enumConvert<mx::api::DurationName>(metronomeMark.noteType);
    beatsPerMinute.dots = static_cast<int>(metronomeMark.augmentationDots);
    beatsPerMinute.beatsPerMinute = std::to_string(metronomeMark.displayedBeatsPerMinute);
    /// @todo Preserve split fonts by assigning the left-hand font to TempoData::fontData and the
    /// right-hand font to the per-minute element once mx::api::BeatsPerMinute exposes its FontData.
    /// See mx-api-gaps.md.

    mx::api::TempoData result;
    result.choice = mx::api::TempoChoice(std::move(beatsPerMinute));
    if (classification.enigmaCtx) {
        if (const auto font = classify::singleVisibleFont(*classification.enigmaCtx)) {
            result.fontData = context.musicXmlFontDataFromFontInfo(*font);
        }
    }
    result.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
    result.justify = musicXmlJustifyForTextExpression(assignment);
    return result;
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
        const auto voiceIt = staff.voices.find(int(voiceIndex));
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
        mark.positionData.horizontalAlignment = musicXmlHorizontalAlignmentForTextExpression(assignment);
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
    const auto cuePlanIt = context.cuePlansByMeasureStaff.find(musicXmlMeasureStaffKey(musxMeasure->getCmper(), staffId));
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
        if (cuePlanIt != context.cuePlansByMeasureStaff.end()) {
            const auto associatedEntry = assignment->calcAssociatedEntry();
            const auto assignedLayer = assignment->layer > 0
                ? std::make_optional<LayerIndex>(assignment->layer - 1)
                : std::nullopt;
            const auto cueLayer = assignedLayer && cuePlanIt->second.isCueLayer(*assignedLayer)
                ? assignedLayer
                : associatedEntry && cuePlanIt->second.isCueLayer(associatedEntry.getLayerIndex())
                    ? std::make_optional(associatedEntry.getLayerIndex())
                    : std::nullopt;
            if (cueLayer
                && (!cuePlanIt->second.isVisibleCueLayer(*cueLayer)
                    || (associatedEntry
                        && !context.entryNumberToFirstNote.contains(associatedEntry->getEntry()->getEntryNumber())))) {
                continue;
            }
        }

        const auto classification = classify::classifyExpression(assignment);
        const auto placement = assignment->calcVerticalPlacement();
        const bool emittedFromTopStaffAssignment = isTopStaffAssignment(assignment);
        const bool isVoiceAttached = assignment->layer > 0 || assignment->voice2;
        const bool singleStaffPart = measure.staves.size() == 1;
        const bool isStaffValueSpecified = !emittedFromTopStaffAssignment && (!singleStaffPart || isVoiceAttached);
        GroupedDirectionAction groupedDirectionAction = GroupedDirectionAction::Emit;
        DirectionGroupTracking* groupTracking = nullptr;
        if (assignment->staffGroup > 0) {
            const auto groupIt = directionGroups.find(assignment->staffGroup);
            if (groupIt == directionGroups.end()) {
                groupedDirectionAction = GroupedDirectionAction::Emit;
            } else if (groupIt->second.emittedFromTopStaffAssignment && assignment->staffAssign >= 0) {
                // A concrete assignment joins the group's TOP assignment on this same staff. One
                // direction carries both roles: it owns the staff and is also drawn at the top of
                // the system, which is MusicXML system="also-top".
                groupedDirectionAction = GroupedDirectionAction::ReplacePrior;
                groupTracking = &groupIt->second;
            } else if (emittedFromTopStaffAssignment && !groupIt->second.emittedFromTopStaffAssignment) {
                // The same pairing encountered the other way round: the concrete assignment is
                // already emitted, so the TOP companion only upgrades it to also-top.
                staff.directions[groupIt->second.directionIndex].systemRelation = mx::api::SystemRelation::alsoTop;
                groupedDirectionAction = GroupedDirectionAction::None;
            } else {
                groupedDirectionAction = GroupedDirectionAction::None;
            }
        }

        auto emitGroupedDirection = [&](std::optional<mx::api::DirectionData> direction) {
            if (!direction || groupedDirectionAction == GroupedDirectionAction::None) {
                return;
            }
            if (groupedDirectionAction == GroupedDirectionAction::ReplacePrior) {
                direction->systemRelation = mx::api::SystemRelation::alsoTop;
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
                    direction.systemRelation = mx::api::SystemRelation::alsoTop;
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
        case classify::ExpressionType::TempoMark:
        case classify::ExpressionType::MetronomeMark: {
            emitGroupedDirection(createTempoExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        }
        case classify::ExpressionType::MultimeasureRestNumber:
            // A number the <multiple-rest> element already accounts for is dropped; anything else
            // falls through and is emitted as text.
            if (applyMultimeasureRestNumber(measure, classification)) {
                break;
            }
            [[fallthrough]];
        case classify::ExpressionType::MeasureRepeatCount:
            // MusicXML's <measure-repeat> carries no counter, so the count stays text.
            [[fallthrough]];
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
        case classify::ExpressionType::StringMute:
            emitGroupedDirection(createStringMuteExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        case classify::ExpressionType::HarpDiagram:
            emitGroupedDirection(createHarpDiagramExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified));
            break;
        case classify::ExpressionType::AccordionRegistration: {
            auto direction = createAccordionRegistrationExpressionDirection(
                context, staffIndex, assignment, classification, placement, isStaffValueSpecified);
            if (!direction) {
                direction = createWordsExpressionDirection(
                    context, staffIndex, assignment, classification, placement, isStaffValueSpecified);
            }
            emitGroupedDirection(std::move(direction));
            break;
        }
        case classify::ExpressionType::NonArpeggio:
            appendArpeggioCandidate(context, classification.nonArpeggio().candidate);
            break;
        case classify::ExpressionType::PseudoTie:
            if (classification.pseudoTie().type == classify::PseudoTie::Type::LaissezVibrer) {
                applyPseudoLvTies(context, assignment->calcAssociatedEntry());
            }
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
