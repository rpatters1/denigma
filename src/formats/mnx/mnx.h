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
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/denigma.h"
#include "core/cue_layers.h"
#include "core/finale_options.h"
#include "core/ottavas.h"
#include "musx/musx.h"
#include "mnxdom.h"

#include "mnx_fwd.h"
#include "mnx_mapping.h"
#include "mnx_articulations.h"
#include "denigma/classify/jumps.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

enum class EntryTargetKind
{
    Event,
    FullMeasureRest
};

struct EntryTarget
{
    EntryTargetKind kind;
    mnxdom::json_pointer pointer;
};

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
        : denigmaContext(&context), document(doc), finaleOptions(loadFinaleOptions(doc)), mnxDocument(), musxParts(doc, SCORE_PARTID) {}

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    FinaleOptions finaleOptions;
    std::unique_ptr<mnxdom::Document> mnxDocument;
    MusxInstanceList<others::PartDefinition> musxParts;

    std::unordered_map<std::string, std::vector<StaffCmper>> part2Inst;
    std::unordered_map<StaffCmper, std::string> inst2Part;
    std::unordered_map<std::string, std::string> part2SplitInstrumentUuid;
    std::unordered_set<std::string> lyricLineIds;

    // musx mappings
    std::unordered_map<std::string, mnxdom::json_pointer> noteJsonById;
    std::unordered_map<EntryNumber, EntryTarget> entryTargetByNumber;

    struct DeferredJumpTie {
        std::string startNoteId;
        std::string endNoteId;
        std::optional<mnxdom::SlurTieSide> side;
    };

    std::vector<DeferredJumpTie> deferredJumpTies;
    std::unordered_set<std::string> deferredJumpTieKeys;
    std::vector<musx::util::ArpeggioSpanCandidate> deferredArpeggios;
    std::unordered_set<std::string> deferredArpeggioKeys;

    std::optional<std::string> currSplitInstrumentUuid;
    std::vector<StaffCmper> currPartStaves;
    std::unordered_set<EntryNumber> beamedEntries;
    size_t discardedCueFrames{};

    struct CurrentMeasureStaff {
        MeasCmper meas{};
        StaffCmper staff{};
        std::string voice;
        std::optional<details::GFrameHoldContext> gfhold;
        std::map<LayerIndex, int> layerVoices;
        CueLayerPlan cueDiscardPlan;
        OttavaShapeMap ottavasApplicableInMeasure;

        void clear()
        {
            meas = staff = 0;
            voice.clear();
            gfhold.reset();
            layerVoices.clear();
            cueDiscardPlan = {};
            ottavasApplicableInMeasure.clear();
        }
    };

    CurrentMeasureStaff current;

    std::optional<int> mnxPartStaffFromStaff(StaffCmper staff) const;

    void clearCounts()
    {
        currSplitInstrumentUuid.reset();
        currPartStaves.clear();
        beamedEntries.clear();
        current.clear();
    }

    void setCurrentMeasureStaff(const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffCmper);
    void logMessage(LogMsg&& msg, MessageSeverity severity = MessageSeverity::Info);
    void logDiscardedHeuristicCueHold();
    void logDiscardedCueLayerFrame(LayerIndex layer);
};

std::string mnxPartDisplayName(const MnxMusxMappingPtr& context, const std::string& partId);
std::string mnxPartDisplayName(const MnxMusxMappingPtr& context, const mnxdom::Part& part);
std::string mnxPartDisplayList(const MnxMusxMappingPtr& context, const std::vector<std::string>& partIds);

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

inline std::string calcVoice(int partStaffNum, LayerIndex idx, int voice)
{
    std::string result = "s" + std::to_string(partStaffNum) + "layer" + std::to_string(idx + 1);
    if (voice > 1) {
        result += "v" + std::to_string(voice);
    }
    return result;
}

inline std::string calcGlobalMeasureId(Cmper cmperValue)
{
    return "m" + std::to_string(cmperValue);
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

void createLayouts(const MnxMusxMappingPtr& context);
void createGlobal(const MnxMusxMappingPtr& context);
void createParts(const MnxMusxMappingPtr& context);
void createSequences(const MnxMusxMappingPtr& context,
    mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure);
void finalizeJumpTies(const MnxMusxMappingPtr& context);

void exportJson(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext);
void exportJson(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext);
void exportMnx(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext);

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum value);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
