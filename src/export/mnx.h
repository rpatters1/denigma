/*
 * Copyright (C) 2024, Robert Patterson
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
#pragma once

#include <filesystem>
#include <optional>
#include <unordered_map>

#include "denigma.h"
#include "musx/musx.h"
#include "mnxdom.h"

#include "mnx_mapping.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

// stoopid c++17 standard does not include a hash for tuple
struct SequenceHash {
    std::size_t operator()(const std::tuple<StaffCmper, LayerIndex, int>& t) const {
        auto [x, y, z] = t;
        std::size_t h1 = std::hash<int>{}(x);
        std::size_t h2 = std::hash<int>{}(y);
        std::size_t h3 = std::hash<int>{}(z);
        return h1 ^ (h2 << 1) ^ (h3 << 2); // XOR-shift for better hash distribution
    }
};

using json = nlohmann::ordered_json;
//using json = nlohmann::json;

struct MnxMusxMapping
{
    MnxMusxMapping(const DenigmaContext& context, const DocumentPtr& doc)
        : denigmaContext(&context), document(doc), mnxDocument(), musxParts(doc, SCORE_PARTID) {}

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    std::unique_ptr<mnx::Document> mnxDocument;
    MusxInstanceList<others::PartDefinition> musxParts;

    std::unordered_map<std::string, std::vector<StaffCmper>> part2Inst;
    std::unordered_map<StaffCmper, std::string> inst2Part;
    std::unordered_set<std::string> lyricLineIds;

    // musx mappings
    std::unordered_map<Cmper, JumpType> textRepeat2Jump;

    MeasCmper currMeas{};
    StaffCmper currStaff{};
    std::string voice;
    std::vector<StaffCmper> partStaves;
    std::unordered_set<EntryNumber> visifiedEntries;
    std::unordered_set<EntryNumber> beamedEntries;
    std::unordered_map<Cmper, MusxInstance<others::SmartShape>> ottavasApplicableInMeasure;

    void clearCounts()
    {
        currMeas = currStaff = 0;
        voice.clear();
        partStaves.clear();
        beamedEntries.clear();
        ottavasApplicableInMeasure.clear();
    }

    void logMessage(LogMsg&& msg, LogSeverity severity = LogSeverity::Info);
};

using MnxMusxMappingPtr = std::shared_ptr<MnxMusxMapping>;

inline std::string calcSystemLayoutId(Cmper partId, Cmper systemId)
{
    if (systemId == BASE_SYSTEM_ID) {
        return "S" + std::to_string(partId) + "-ScrVw";
    }
    return "S" + std::to_string(partId) + "-Sys" + std::to_string(systemId);
}

inline std::string calcEventId(EntryNumber entryNum)
{
    return "ev" + std::to_string(entryNum);
}

inline std::string calcNoteId(const NoteInfoPtr& noteInfo)
{
    return calcEventId(noteInfo.getEntryInfo()->getEntry()->getEntryNumber()) + "n" + std::to_string(noteInfo->getNoteId());
}

inline std::string calcVoice(LayerIndex idx, int voice)
{
    std::string result = "layer" + std::to_string(idx + 1);
    if (voice > 1) {
        result += "v" + std::to_string(voice);
    }
    return result;
}

inline std::string calcLyricLineId(const std::string& type, Cmper textNumber)
{
    return type.substr(0, 1) + std::to_string(textNumber);
}

inline std::string calcPercussionKitId(const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    return "ke" + std::to_string(percNoteInfo->percNoteType);
}

inline std::string calcPercussionSoundId(const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    std::string result = "pn" + std::to_string(percNoteInfo->getBaseNoteTypeId());
    if (auto orderId = percNoteInfo->getNoteTypeOrderId()) {
        result += "o" + std::to_string(orderId + 1);
    }
    return result;
}

inline std::string trimNewLineFromString(const std::string& src)
{
    size_t pos = src.find('\n');
    if (pos != std::string::npos) {
        return src.substr(0, pos);  // Truncate at the newline, excluding it
    }
    return src;
}

void createLayouts(const MnxMusxMappingPtr& context);
void createGlobal(const MnxMusxMappingPtr& context);
void createParts(const MnxMusxMappingPtr& context);
void createSequences(const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffCmper);

void exportJson(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext);
void exportMnx(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext);

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum value);

} // namespace mnxexp
} // namespace denigma
