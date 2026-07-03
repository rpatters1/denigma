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
#include <ostream>
#include <optional>
#include <string>
#include <string_view>

#include "core/denigma.h"
#include "musicxml_mapping.h"
#include "mx/api/MeasureData.h"
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

void createDefaults(const MusicXmlMusxMapping& context);
void createMeasures(MusicXmlMusxMapping& context);
void createMetaData(const MusicXmlMusxMapping& context);
void createNotesForMeasureStaff(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    mx::api::StaffData& staff,
    const musx::dom::MusxInstance<musx::dom::others::Measure>& musxMeasure,
    musx::dom::StaffCmper staffId,
    size_t staffIndex);
void createParts(MusicXmlMusxMapping& context);
void processArticulations(MusicXmlMusxMapping& context, mx::api::NoteData& note, const musx::dom::EntryInfoPtr& entryInfo);

void exportMusicXml(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext);

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
