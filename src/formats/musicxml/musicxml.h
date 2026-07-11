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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/denigma.h"
#include "denigma/classify/entries.h"
#include "denigma/classify/expressions.h"
#include "musicxml_mapping.h"
#include "mx/api/DirectionData.h"
#include "mx/api/MeasureData.h"
#include "mx/api/MarkData.h"
#include "mx/api/NoteData.h"
#include "mx/api/SoundID.h"
#include "mx/api/StaffData.h"

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum);

inline std::string createPartId(int partNumber)
{
    return "P" + std::to_string(partNumber);
}

inline int musicXmlVoiceNumber(size_t staffIndex, musx::dom::LayerIndex layer, int v1v2)
{
    constexpr int v1v2StreamsPerLayer = 2;
    constexpr int finaleLayersPerStaff = 4;
    constexpr int voicesPerStaff = finaleLayersPerStaff * v1v2StreamsPerLayer;
    return (static_cast<int>(staffIndex) * voicesPerStaff) + (static_cast<int>(layer) * v1v2StreamsPerLayer) + v1v2;
}

MusicXmlPitchContext pitchContextForPart(const MusicXmlMusxMapping& context, const std::string& partId);
std::optional<mx::api::SoundID> musicXmlSoundIdFromInstrumentUuid(std::string_view instUuid);
mx::api::MarkData musicXmlMark(mx::api::MarkType type, musx::dom::VerticalPlacement placement);
mx::api::MarkType musicXmlFermataType(const classify::articulation::Fermata& fermata);
double musicXmlQuarterNotesPerMinute(const classify::expression::TempoInfo& tempo);
mx::api::ScoreData createMusicXmlDocument(
    const CommandInputData& inputData,
    const DenigmaContext& denigmaContext,
    const musx::dom::MusxInstance<musx::dom::others::PartDefinition>& part = nullptr);

void createDefaults(const MusicXmlMusxMapping& context);
void createMeasures(MusicXmlMusxMapping& context);
void createMetaData(const MusicXmlMusxMapping& context);
void createNotesForMeasureStaff(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    mx::api::StaffData& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& musxMeasure,
    musx::dom::StaffCmper staffId,
    size_t measureIndex,
    size_t staffIndex);
void createParts(MusicXmlMusxMapping& context);
std::vector<mx::api::DirectionData> createDynamicExpressionDirections(
    MusicXmlMusxMapping& context,
    size_t staffIndex,
    const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment,
    const classify::ExpressionClassification& classification,
    musx::dom::VerticalPlacement placement,
    bool isStaffValueSpecified = true);
void processExpressions(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    mx::api::StaffData& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& musxMeasure,
    musx::dom::StaffCmper staffId,
    size_t staffIndex);
void processJumps(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& musxMeasure,
    musx::dom::StaffCmper staffId,
    size_t staffIndex);
void processArticulations(MusicXmlMusxMapping& context, mx::api::NoteData& note, const musx::dom::EntryInfoPtr& entryInfo);
void applyNoteheadData(
    mx::api::NoteData& note,
    const musx::dom::NoteInfoPtr& noteInfo,
    const classify::EntryNoteheadClassification& entryNoteheads);
void processSmartShapes(
    MusicXmlMusxMapping& context,
    const musx::dom::MusxInstanceList<musx::dom::others::Measure>& musxMeasures,
    const std::vector<musx::dom::StaffCmper>& staves);

/// Converts to MusicXML and invokes outputCallback once per generated document: the score
/// (when denigmaContext.allPartsAndScore is true or denigmaContext.partName is unset), and/or
/// one document per matching linked part (when denigmaContext.allPartsAndScore or
/// denigmaContext.partName is set).
void convert(
    const CommandInputData& inputData,
    const DenigmaContext& denigmaContext,
    const MultiOutputCallback& outputCallback);

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
