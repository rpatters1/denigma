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
#include <optional>
#include <string>
#include <vector>

#include "denigma/classify/barlines.h"
#include "denigma/classify/clefs.h"

#include "mx/api/BarlineData.h"
#include "mx/api/ClefData.h"
#include "mx/api/KeyData.h"
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

void addBarlineData(
    mx::api::MeasureData& measure,
    mx::api::BarlineType barlineType,
    mx::api::HorizontalAlignment location)
{
    auto barline = mx::api::BarlineData{};
    barline.barlineType = barlineType;
    barline.location = location;
    if (location == mx::api::HorizontalAlignment::right) {
        barline.tickTimePosition = mx::api::TICK_TIME_INFINITY;
    }
    measure.barlines.emplace_back(barline);
}

void addBarline(
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    classify::BarlineType barlineType,
    mx::api::HorizontalAlignment location)
{
    const auto musicXmlBarlineType = enumConvert<mx::api::BarlineType>(barlineType);
    if (musicXmlBarlineType == mx::api::BarlineType::unspecified || musicXmlBarlineType == mx::api::BarlineType::unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(barlineType) << ".", MessageSeverity::Info);
        return;
    }
    addBarlineData(measure, musicXmlBarlineType, location);
}

void addBarline(
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    BarlineType barlineType,
    mx::api::HorizontalAlignment location)
{
    const auto musicXmlBarlineType = enumConvert<mx::api::BarlineType>(barlineType);
    if (musicXmlBarlineType == mx::api::BarlineType::unspecified || musicXmlBarlineType == mx::api::BarlineType::unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(barlineType) << ".", MessageSeverity::Info);
        return;
    }
    addBarlineData(measure, musicXmlBarlineType, location);
}

MusxInstance<others::Staff> findRepresentativeBarlineStaff(
    MusicXmlMusxMapping& context,
    const MusxInstance<others::Measure>& musxMeasure,
    const std::vector<StaffCmper>& staves)
{
    MusxInstance<others::Staff> firstFoundStaff;
    const auto endOfBar = musxMeasure->calcDuration().calcEduDuration();
    for (StaffCmper staffId : staves) {
        auto staff = others::StaffComposite::createCurrent(
            context.document,
            context.forPartId,
            staffId,
            musxMeasure->getCmper(),
            endOfBar);
        if (!staff) {
            continue;
        }
        if (!firstFoundStaff) {
            firstFoundStaff = staff;
        }
        if (!staff->hideBarlines) {
            return staff;
        }
    }
    return firstFoundStaff;
}

void assignBarlines(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    bool isFinalMeasure,
    const std::vector<StaffCmper>& staves)
{
    const auto barlineOptions = context.finaleOptions.barlineOptions;
    if (musxMeasure->leftBarlineType != BarlineType::OptionsDefault && musxMeasure->leftBarlineType != BarlineType::None) {
        addBarline(context, measure, musxMeasure->leftBarlineType, mx::api::HorizontalAlignment::left);
    }

    const auto staff = findRepresentativeBarlineStaff(context, musxMeasure, staves);
    const auto classification = classify::classifyBarline(staff, musxMeasure, isFinalMeasure, barlineOptions);
    if (classification.type == classify::BarlineType::Unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(musxMeasure->barlineType) << ".", MessageSeverity::Info);
        return;
    }

    if (classification.type == classify::BarlineType::Regular) {
        if (classification.isShort) {
            addBarlineData(measure, mx::api::BarlineType::short_, mx::api::HorizontalAlignment::right);
        }
        return;
    }

    addBarline(context, measure, classification.type, mx::api::HorizontalAlignment::right);
}

mx::api::KeyMode musicXmlKeyModeFromMusxKeySignature(const MusxInstance<KeySignature>& keySignature)
{
    if (keySignature->isMajor()) {
        return mx::api::KeyMode::major;
    }
    if (keySignature->isMinor()) {
        return mx::api::KeyMode::minor;
    }
    return mx::api::KeyMode::unspecified;
}

int musicXmlKeyFifths(
    const MusicXmlMusxMapping& context,
    const MusxInstance<KeySignature>& keySignature,
    MeasCmper measureId,
    MusicXmlPitchContext pitchContext)
{
    if (keySignature->calcEDODivisions() != music_theory::STANDARD_12EDO_STEPS) {
        context.logMessage(LogMsg() << "Skipping unsupported microtonal key signature in measure " << measureId << ".", MessageSeverity::Info);
        return 0;
    }
    if (!keySignature->isLinear()) {
        context.logMessage(LogMsg() << "Skipping unsupported non-linear key signature " << keySignature->getKeyMode()
            << " in measure " << measureId << ".", MessageSeverity::Info);
        return 0;
    }
    if (keySignature->keyless || keySignature->hideKeySigShowAccis) {
        return 0;
    }
    return keySignature->getAlteration(enumConvert<KeySignature::KeyContext>(pitchContext));
}

mx::api::KeyData createKeyData(
    const MusicXmlMusxMapping& context,
    const MusxInstance<KeySignature>& keySignature,
    MeasCmper measureId,
    MusicXmlPitchContext pitchContext)
{
    auto key = mx::api::KeyData{};
    key.fifths = musicXmlKeyFifths(context, keySignature, measureId, pitchContext);
    key.mode = musicXmlKeyModeFromMusxKeySignature(keySignature);
    return key;
}

bool keyDataEqualIgnoringStaffIndex(const mx::api::KeyData& lhs, const mx::api::KeyData& rhs)
{
    auto normalizedLhs = lhs;
    auto normalizedRhs = rhs;
    normalizedLhs.staffIndex = -1;
    normalizedRhs.staffIndex = -1;
    return normalizedLhs == normalizedRhs;
}

bool keyDataChanged(
    const std::optional<mx::api::KeyData>& previous,
    const mx::api::KeyData& current)
{
    return !previous || !keyDataEqualIgnoringStaffIndex(*previous, current);
}

void assignKeySignatures(
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    const std::vector<StaffCmper>& staves,
    MusicXmlPitchContext pitchContext,
    std::vector<std::optional<mx::api::KeyData>>& prevKeyData)
{
    std::vector<mx::api::KeyData> currentKeyData;
    currentKeyData.reserve(staves.size());
    for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
        auto key = createKeyData(
            context,
            musxMeasure->createKeySignature(staves[staffIndex]),
            musxMeasure->getCmper(),
            pitchContext);
        key.staffIndex = static_cast<int>(staffIndex);
        currentKeyData.emplace_back(key);
    }

    const bool forceEmit = musxMeasure->showKey == others::Measure::ShowKeySigMode::Always;
    bool shouldEmit = forceEmit;
    for (size_t index = 0; index < currentKeyData.size(); ++index) {
        shouldEmit = shouldEmit || keyDataChanged(prevKeyData[index], currentKeyData[index]);
    }
    if (!shouldEmit) {
        return;
    }

    bool allStavesShareKey = true;
    for (size_t index = 1; index < currentKeyData.size(); ++index) {
        if (!keyDataEqualIgnoringStaffIndex(currentKeyData.front(), currentKeyData[index])) {
            allStavesShareKey = false;
            break;
        }
    }

    if (allStavesShareKey && !currentKeyData.empty()) {
        auto key = currentKeyData.front();
        key.staffIndex = -1;
        measure.keys.emplace_back(key);
    } else {
        for (const auto& key : currentKeyData) {
            measure.keys.emplace_back(key);
        }
    }

    for (size_t index = 0; index < currentKeyData.size(); ++index) {
        prevKeyData[index] = currentKeyData[index];
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
    const std::vector<StaffCmper>& staves,
    MusxInstance<TimeSignature>& prevTimeSig)
{
    ASSERT_IF(staves.empty()) {
        return;
    }
    auto timeSig = musxMeasure->createTimeSignature(staves.front());
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
    const MusxInstance<others::Measure>& musxMeasure)
{
    const Fraction measureDuration = musxMeasure->calcDuration();

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
    std::vector<std::optional<mx::api::KeyData>> prevKeyData(stavesIt->second.size());
    MusxInstance<TimeSignature> prevTimeSig;
    const auto pitchContext = pitchContextForPart(context, part.uniqueId);
    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        const auto& musxMeasure = musxMeasures[measureIndex];
        const bool isFinalMeasure = measureIndex + 1 == musxMeasures.size();
        auto& measure = part.measures.emplace_back(mx::api::MeasureData{});
        measure.number = std::to_string(musxMeasure->getCmper());
        assignBarlines(context, measure, musxMeasure, isFinalMeasure, stavesIt->second);
        assignKeySignatures(context, measure, musxMeasure, stavesIt->second, pitchContext, prevKeyData);
        assignTimeSignature(measure, musxMeasure, stavesIt->second, prevTimeSig);
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
            addFullMeasureRest(context, staff, musxMeasure);
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
