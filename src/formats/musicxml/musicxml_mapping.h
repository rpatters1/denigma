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
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/denigma.h"
#include "core/finale_options.h"
#include "musx/musx.h"
#include "mx/api/FontData.h"
#include "mx/api/PartSymbolData.h"
#include "mx/api/ScoreData.h"

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

inline constexpr double MUSICXML_DEFAULT_TENTHS_PER_STAFF = 40.0;

enum class MusicXmlFontFamilyFallback
{
    None,
    Music,
    Engraved,
    Handwritten,
    Text,
    Serif,
    SansSerif,
    Cursive,
    Fantasy,
    Monospace
};

struct MusicXmlTimingPlan
{
    int divisions{};

    int calcMusicXmlDivisions(const musx::util::Fraction& wholeNoteFraction) const
    {
        const auto result = wholeNoteFraction * 4 * divisions;
        if (result.denominator() != 1) {
            throw std::logic_error("MusicXML duration is not representable with the selected divisions.");
        }
        return result.numerator();
    }
};

struct MusicXmlCurrentLocation
{
    musx::dom::MeasCmper measure{};
    musx::dom::StaffCmper staff{};
    musx::dom::LayerIndex layer{};
    int voice{};
    musx::util::Fraction positionInMeasure;

    void clear()
    {
        measure = 0;
        staff = 0;
        layer = 0;
        voice = 0;
        positionInMeasure = {};
    }
};

struct MusicXmlMusxMapping
{
    MusicXmlMusxMapping(const DenigmaContext& context, const musx::dom::DocumentPtr& doc, musx::dom::Cmper partId)
        : denigmaContext(&context),
          document(doc),
          finaleOptions(loadFinaleOptions(doc)),
          forPartId(partId)
    {
    }

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    FinaleOptions finaleOptions;
    std::unique_ptr<mx::api::ScoreData> musicXmlScore;
    musx::dom::Cmper forPartId;

    MusicXmlTimingPlan timing;
    MusicXmlCurrentLocation current;

    std::unordered_map<musx::dom::StaffCmper, std::string> staffToPartId;
    std::unordered_map<std::string, std::vector<musx::dom::StaffCmper>> partIdToStaves;
    std::unordered_map<std::string, mx::api::PartSymbolData> partIdToPartSymbol;
    std::unordered_map<musx::dom::EntryNumber, std::string> entryNumberToNoteId;
    std::unordered_set<musx::dom::EntryNumber> beamedEntries;

    void clearCurrent()
    {
        current.clear();
    }

    double musicXmlTenthsFromEvpu(double evpu, double backoutScaling = 1.0) const;
    mx::api::FontData musicXmlFontDataFromFontInfo(
        const musx::dom::FontInfo& fontInfo,
        MusicXmlFontFamilyFallback fallback = MusicXmlFontFamilyFallback::None) const;

    void logMessage(LogMsg&& msg, MessageSeverity severity = MessageSeverity::Info) const
    {
        denigmaContext->logMessage(std::move(msg), severity);
    }
};

using MusicXmlMusxMappingPtr = std::shared_ptr<MusicXmlMusxMapping>;

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
