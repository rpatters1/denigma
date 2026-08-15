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

#include <cstdint>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/cue_plan.h"
#include "core/denigma.h"
#include "core/finale_options.h"
#include "core/ottavas.h"
#include "musx/musx.h"
#include "musx/util/Arpeggio.h"
#include "mx/api/FontData.h"
#include "mx/api/CurveData.h"
#include "mx/api/LyricData.h"
#include "mx/api/PartData.h"
#include "mx/api/PartSymbolData.h"
#include "mx/api/ScoreData.h"
#include "mx/api/StaffData.h"

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

/// @brief Which of Finale's scaling factors reduces a font's nominal point size.
///
/// MusicXML font sizes are physical points, so a font must be scaled by whatever Finale scales the
/// thing it sits on. Text living in staff space shrinks with both the page and the systems, while
/// page-attached text sits on the page and is unaffected by system or staff scaling.
enum class MusicXmlFontScaling
{
    StaffSpace,
    Page
};

enum class MusicXmlPitchContext
{
    Concert,
    Written
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

    int calcNearestMusicXmlDivisions(const musx::util::Fraction& wholeNoteFraction) const;
};

struct MusicXmlCurrentLocation
{
    musx::dom::MeasCmper measure{};
    musx::dom::StaffCmper staff{};
    musx::dom::LayerIndex layer{};
    int voice{};
    musx::util::Fraction positionInMeasure;
    OttavaShapeMap ottavasApplicableInMeasure;

    void clear()
    {
        measure = 0;
        staff = 0;
        layer = 0;
        voice = 0;
        positionInMeasure = {};
        ottavasApplicableInMeasure.clear();
    }
};

struct MusicXmlStaffLayoutState
{
    musx::util::Fraction staffSize{1};
    musx::util::Fraction staffScaling{1};
};

struct MusicXmlLayoutState
{
    std::unordered_map<musx::dom::StaffCmper, MusicXmlStaffLayoutState> staffLayout;

    void clear()
    {
        staffLayout.clear();
    }

    void setStaffSize(
        mx::api::StaffData& staffData,
        musx::dom::StaffCmper staffId,
        const musx::util::Fraction& staffSize,
        const musx::util::Fraction& staffScaling);
};

struct MusicXmlNoteLocation
{
    size_t measureIndex{};
    size_t staffIndex{};
    int userVoiceNumber{};
    size_t noteIndex{};
};

inline std::uint64_t musicXmlNoteKey(musx::dom::EntryNumber entryNumber, musx::dom::NoteNumber noteId)
{
    return (std::uint64_t(entryNumber) << 32) | std::uint64_t(noteId);
}

inline std::uint64_t musicXmlMeasureStaffKey(musx::dom::MeasCmper measure, musx::dom::StaffCmper staff)
{
    return (std::uint64_t(measure) << 32) | std::uint64_t(static_cast<std::uint32_t>(staff));
}

struct MusicXmlMusxMapping
{
    MusicXmlMusxMapping(const DenigmaContext& context, const musx::dom::DocumentPtr& doc, musx::dom::Cmper partId)
        : denigmaContext(&context),
          document(doc),
          finaleOptions(loadFinaleOptions(doc, partId)),
          forPartId(partId)
    {
        // Mirror musxdom's own guards (Document::calcPageFromMeasure and calcSystemFromMeasure) and
        // treat a missing PartDefinition as uncalculated.
        const auto part = doc->getOthers()->get<musx::dom::others::PartDefinition>(musx::dom::SCORE_PARTID, partId);
        partLayoutIsCalculated = part && part->isLayoutCalculated();
    }

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    FinaleOptions finaleOptions;
    std::unique_ptr<mx::api::ScoreData> musicXmlScore;
    musx::dom::Cmper forPartId;
    mx::api::PartData* currentPart{};

    /// True when Finale calculated this part's page layout. When false, nothing that resolves a
    /// measure to a system or a system to a page can be trusted: Finale retains zero-valued
    /// placeholders in others::StaffSystem and others::Page until a linked part's layout is updated.
    /// Saved geometry and saved per-measure flags remain valid either way.
    bool partLayoutIsCalculated{};

    /// Lazily fetched backing store for #systemForMeasure. Callers previously hoisted this array out
    /// of their loops; caching it here keeps the shared lookup equally cheap per call.
    mutable std::optional<musx::dom::MusxInstanceList<musx::dom::others::StaffSystem>> cachedStaffSystems;

    MusicXmlTimingPlan timing;
    MusicXmlCurrentLocation current;
    MusicXmlLayoutState layout;

    std::unordered_map<musx::dom::StaffCmper, std::string> staffToPartId;
    std::unordered_map<std::string, std::vector<musx::dom::StaffCmper>> partIdToStaves;
    std::unordered_map<std::string, mx::api::PartSymbolData> partIdToPartSymbol;
    std::unordered_map<std::string, MusicXmlPitchContext> partIdToPitchContext;
    std::unordered_map<musx::dom::EntryNumber, MusicXmlNoteLocation> entryNumberToFirstNote;
    std::unordered_map<std::uint64_t, MusicXmlNoteLocation> noteLocations;
    std::unordered_map<musx::dom::EntryNumber, std::vector<mx::api::LyricData>> pendingLyricStops;
    std::unordered_map<std::uint64_t, CueStaffMeasurePlan> cuePlansByMeasureStaff;
    std::unordered_set<musx::dom::EntryNumber> beamedEntries;
    std::unordered_set<std::uint64_t> pendingTieStopKeys;
    std::unordered_set<musx::dom::EntryNumber> processedPseudoLvTieEntries;
    std::vector<musx::dom::EntryInfoPtr> deferredPseudoLvTieEntries;
    std::unordered_set<musx::dom::EntryNumber> deferredPseudoLvTieEntryNumbers;
    std::vector<musx::util::ArpeggioSpanCandidate> deferredArpeggioCandidates;
    std::unordered_set<std::string> deferredArpeggioCandidateKeys;

    void clearCurrent()
    {
        currentPart = nullptr;
        current.clear();
        layout.clear();
        entryNumberToFirstNote.clear();
        noteLocations.clear();
        pendingLyricStops.clear();
        cuePlansByMeasureStaff.clear();
        pendingTieStopKeys.clear();
        processedPseudoLvTieEntries.clear();
        deferredPseudoLvTieEntries.clear();
        deferredPseudoLvTieEntryNumbers.clear();
        deferredArpeggioCandidates.clear();
        deferredArpeggioCandidateKeys.clear();
    }

    /// Returns the staff system containing @p measureId, or a null instance when this part's layout
    /// is uncalculated (see #partLayoutIsCalculated) or no system covers the measure.
    musx::dom::MusxInstance<musx::dom::others::StaffSystem> systemForMeasure(musx::dom::MeasCmper measureId) const;

    double musicXmlTenthsFromEvpu(double evpu, double backoutScaling = 1.0) const;
    mx::api::FontData musicXmlFontDataFromFontInfo(
        const musx::dom::FontInfo& fontInfo,
        MusicXmlFontFamilyFallback fallback = MusicXmlFontFamilyFallback::None,
        MusicXmlFontScaling fontScaling = MusicXmlFontScaling::StaffSpace) const;

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
