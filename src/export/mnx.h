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
    std::size_t operator()(const std::tuple<InstCmper, LayerIndex, int>& t) const {
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
        : denigmaContext(&context), document(doc), mnxDocument() {}

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    std::shared_ptr<FontInfo> defaultMusicFont;
    std::unique_ptr<mnx::Document> mnxDocument;

    std::unordered_map<std::string, std::vector<InstCmper>> part2Inst;
    std::unordered_map<InstCmper, std::string> inst2Part;
    std::unordered_set<std::string> lyricLineIds;

    // musx mappings
    std::unordered_map<Cmper, JumpType> textRepeat2Jump;

    MeasCmper currMeas{};
    InstCmper currStaff{};
    musx::util::Fraction currMeasDura{};    ///< duration of current measure
    musx::util::Fraction duraOffset{};      ///< offset to apply to leftOverEntries
    std::string voice;
    std::vector<InstCmper> partStaves;
    std::unordered_map<std::tuple<InstCmper, LayerIndex, int>, std::vector<EntryInfoPtr>, SequenceHash> leftOverEntries; // left over entries per layer/voice combo
    std::unordered_map<std::tuple<InstCmper, LayerIndex, int>, musx::util::Fraction, SequenceHash> duraOffsets; // dura offsets for leftovers per layer/voice combo
    std::unordered_map<Cmper, std::shared_ptr<const others::SmartShape>> ottavasApplicableInMeasure;
    std::unordered_map<Cmper, bool> insertedOttavas; ///< tracks (for each measure) whether we inserted ottavas that started in the measure.
    std::unordered_map<InstCmper, std::vector<Cmper>> leftOverOttavas; ///< tracks ottavas that start after all entries have been processed for that inst/measure.
    std::unordered_map<std::shared_ptr<const others::MeasureExprAssign>, bool> dynamicsInMeasure;
    std::unordered_set<musx::dom::EntryNumber> visifiedEntries;

    void clearCounts()
    {
        currMeas = currStaff = 0;
        currMeasDura = duraOffset = 0;
        voice.clear();
        partStaves.clear();
        leftOverEntries.clear();
        duraOffsets.clear();
        ottavasApplicableInMeasure.clear();
        insertedOttavas.clear();
        leftOverOttavas.clear();
        dynamicsInMeasure.clear();
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

inline std::string trimNewLineFromString(const std::string& src)
{
    size_t pos = src.find('\n');
    if (pos != std::string::npos) {
        return src.substr(0, pos);  // Truncate at the newline, excluding it
    }
    return src;
}

mnx::NoteValue::Initializer mnxNoteValueFromEdu(Edu duration);
std::pair<int, mnx::NoteValue::Initializer> mnxNoteValueQuantityFromFraction(const MnxMusxMappingPtr& context, musx::util::Fraction duration);
mnx::LyricLineType mnxLineTypeFromLyric(const std::shared_ptr<const LyricsSyllableInfo>& syl);

mnx::Fraction::Initializer mnxFractionFromFraction(const musx::util::Fraction& fraction);
mnx::Fraction::Initializer mnxFractionFromEdu(Edu eduValue);
int mnxStaffPosition(const std::shared_ptr<const others::Staff>& staff, int musxStaffPosition);

void createLayouts(const MnxMusxMappingPtr& context);
void createGlobal(const MnxMusxMappingPtr& context);
void createParts(const MnxMusxMappingPtr& context);
void createSequences(const MnxMusxMappingPtr& context,
    mnx::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber,
    const std::shared_ptr<others::Measure>& musxMeasure,
    InstCmper staffCmper);

void exportJson(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext);
void exportMnx(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext);

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum value);

} // namespace mnxexp
} // namespace denigma
