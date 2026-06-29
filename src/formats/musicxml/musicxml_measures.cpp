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

#include "musicxml.h"

#include <cstdlib>
#include <string>

#include "denigma/classify/clefs.h"

#include "mx/api/BarlineData.h"
#include "mx/api/ClefData.h"
#include "mx/api/MeasureData.h"
#include "mx/api/NoteData.h"
#include "mx/api/PartData.h"
#include "mx/api/StaffData.h"
#include "mx/api/VoiceData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

using BarlineType = others::Measure::BarlineType;

void addBarline(
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    BarlineType barlineType,
    mx::api::HorizontalAlignment location)
{
    auto barline = mx::api::BarlineData{};
    barline.barlineType = enumConvert<mx::api::BarlineType>(barlineType);
    if (barline.barlineType == mx::api::BarlineType::unspecified || barline.barlineType == mx::api::BarlineType::unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(barlineType) << ".", MessageSeverity::Info);
        return;
    }
    barline.location = location;
    if (location == mx::api::HorizontalAlignment::right) {
        barline.tickTimePosition = mx::api::TICK_TIME_INFINITY;
    }
    measure.barlines.emplace_back(barline);
}

void assignBarlines(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    bool isFinalMeasure)
{
    const auto barlineOptions = context.finaleOptions.barlineOptions;
    if (!barlineOptions->drawBarlines) {
        addBarline(context, measure, BarlineType::None, mx::api::HorizontalAlignment::right);
        return;
    }

    if (musxMeasure->leftBarlineType != BarlineType::OptionsDefault && musxMeasure->leftBarlineType != BarlineType::None) {
        addBarline(context, measure, musxMeasure->leftBarlineType, mx::api::HorizontalAlignment::left);
    }

    if (musxMeasure->barlineType == BarlineType::Normal) {
        if (isFinalMeasure && barlineOptions->drawFinalBarlineOnLastMeas) {
            addBarline(context, measure, BarlineType::Final, mx::api::HorizontalAlignment::right);
        } else if (!isFinalMeasure && barlineOptions->drawDoubleBarlineBeforeKeyChanges) {
            if (const auto nextMeasure = context.document->getOthers()->get<others::Measure>(
                    context.forPartId, static_cast<Cmper>(musxMeasure->getCmper() + 1))) {
                if (!musxMeasure->createKeySignature()->isSame(*nextMeasure->createKeySignature().get())) {
                    addBarline(context, measure, BarlineType::Double, mx::api::HorizontalAlignment::right);
                }
            }
        }
        return;
    }

    if (musxMeasure->barlineType != BarlineType::OptionsDefault) {
        addBarline(context, measure, musxMeasure->barlineType, mx::api::HorizontalAlignment::right);
    }
}

mx::api::ClefData musicXmlClefFromMusxClef(
    const MusxInstance<options::ClefOptions::ClefDef>& clefDef,
    const MusxInstance<others::StaffComposite>& staff)
{
    auto result = mx::api::ClefData{};
    const auto clef = classify::classifyClef(clefDef, staff);
    if (clef.type == music_theory::ClefType::Unknown) {
        result.setTreble();
        result.printObject = mx::api::Bool::no;
    } else {
        result.symbol = enumConvert<mx::api::ClefSymbol>(clef.type);
        result.line = 5 + (clefDef->staffPosition / 2);
        result.octaveChange = clef.octave;
        if (clef.isBlank) {
            result.printObject = mx::api::Bool::no;
        }
    }
    return result;
}

void assignTimeSignature(
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    MusxInstance<TimeSignature>& prevTimeSig)
{
    auto timeSig = musxMeasure->createTimeSignature();
    if (prevTimeSig && timeSig->isSame(*prevTimeSig.get())) {
        return;
    }

    auto [count, noteType] = timeSig->calcSimplified();
    if (count.remainder()) {
        if ((Edu(noteType) % count.denominator()) == 0) {
            noteType = NoteType(Edu(noteType) / count.denominator());
            count *= count.denominator();
        }
    }

    measure.timeSignature.beats = std::to_string(count.quotient());
    measure.timeSignature.beatType = std::to_string(EDU_PER_WHOLE_NOTE / Edu(noteType));
    measure.timeSignature.isImplicit = false;
    prevTimeSig = timeSig;
}

void addFullMeasureRest(
    const MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId)
{
    const Fraction measureDuration = musxMeasure->calcDuration(staffId);

    auto rest = mx::api::NoteData{};
    rest.isRest = true;
    rest.isMeasureRest = true;
    rest.durationData.durationName = mx::api::DurationName::whole;
    rest.durationData.durationTimeTicks = context.timing.calcMusicXmlDivisions(measureDuration);

    auto& voice = staff.voices[0];
    voice.notes.emplace_back(rest);
}

void addInitialClef(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    const MusxInstance<others::Measure>& musxMeasure)
{
    auto musxStaff = others::StaffComposite::createCurrent(
        context.document,
        context.forPartId,
        staffId,
        musxMeasure->getCmper(),
        0);
    if (!musxStaff) {
        context.logMessage(LogMsg() << "No staff information found for staff " << staffId << ".", MessageSeverity::Warning);
        return;
    }
    const auto clefIndex = musxStaff->calcClefIndex(/*forWrittenPitch*/ true);
    const auto clefDef = context.finaleOptions.clefOptions->getClefDef(clefIndex);
    staff.clefs.emplace_back(musicXmlClefFromMusxClef(clefDef, musxStaff));
}

void createMeasuresForPart(MusicXmlMusxMapping& context, mx::api::PartData& part)
{
    const auto stavesIt = context.partIdToStaves.find(part.uniqueId);
    if (stavesIt == context.partIdToStaves.end() || stavesIt->second.empty()) {
        context.logMessage(LogMsg() << "MusicXML part " << part.uniqueId << " is not mapped to any Finale staves.", MessageSeverity::Warning);
        return;
    }

    const auto musxMeasures = context.document->getOthers()->getArray<others::Measure>(context.forPartId);
    part.measures.reserve(musxMeasures.size());
    MusxInstance<TimeSignature> prevTimeSig;
    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        const auto& musxMeasure = musxMeasures[measureIndex];
        const bool isFinalMeasure = measureIndex + 1 == musxMeasures.size();
        auto& measure = part.measures.emplace_back(mx::api::MeasureData{});
        measure.number = std::to_string(musxMeasure->getCmper());
        assignBarlines(context, measure, musxMeasure, isFinalMeasure);
        assignTimeSignature(measure, musxMeasure, prevTimeSig);
        if (const auto partSymbolIt = context.partIdToPartSymbol.find(part.uniqueId); partSymbolIt != context.partIdToPartSymbol.end()) {
            measure.partSymbol = partSymbolIt->second;
        }

        measure.staves.resize(stavesIt->second.size());
        for (size_t staffIndex = 0; staffIndex < stavesIt->second.size(); ++staffIndex) {
            const StaffCmper staffId = stavesIt->second[staffIndex];
            auto& staff = measure.staves[staffIndex];
            if (measureIndex == 0) {
                addInitialClef(context, staff, staffId, musxMeasure);
            }
            addFullMeasureRest(context, staff, musxMeasure, staffId);
        }
    }
}

} // namespace

void createMeasures(MusicXmlMusxMapping& context)
{
    for (auto& part : context.musicXmlScore->parts) {
        createMeasuresForPart(context, part);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
