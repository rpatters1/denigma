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
#include "musicxml_formatted_text.h"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "denigma/classify/barlines.h"
#include "denigma/classify/clefs.h"

#include "mx/api/BarlineData.h"
#include "mx/api/ClefData.h"
#include "mx/api/DirectionData.h"
#include "mx/api/KeyData.h"
#include "mx/api/MeasureData.h"
#include "mx/api/PartData.h"
#include "mx/api/SoundData.h"
#include "mx/api/StaffData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

using BarlineType = others::Measure::BarlineType;

mx::api::BarlineData& ensureBarlineData(
    mx::api::MeasureData& measure,
    mx::api::HorizontalAlignment location)
{
    for (auto& barline : measure.barlines) {
        if (barline.location == location) {
            return barline;
        }
    }

    auto barline = mx::api::BarlineData{};
    barline.location = location;
    barline.barlineType = mx::api::BarlineType::unspecified;
    if (location == mx::api::HorizontalAlignment::right) {
        barline.tickTimePosition = mx::api::TICK_TIME_INFINITY;
    }
    if (location == mx::api::HorizontalAlignment::left) {
        const auto rightBarlineIt = std::find_if(measure.barlines.begin(), measure.barlines.end(), [](const auto& existingBarline) {
            return existingBarline.location == mx::api::HorizontalAlignment::right;
        });
        return *measure.barlines.insert(rightBarlineIt, barline);
    }
    measure.barlines.emplace_back(barline);
    return measure.barlines.back();
}

mx::api::BarlineData& setBarlineData(
    mx::api::MeasureData& measure,
    mx::api::BarlineType barlineType,
    mx::api::HorizontalAlignment location)
{
    auto& barline = ensureBarlineData(measure, location);
    barline.barlineType = barlineType;
    return barline;
}

void setBarline(
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    classify::barline::Type barlineType,
    mx::api::HorizontalAlignment location)
{
    const auto musicXmlBarlineType = enumConvert<mx::api::BarlineType>(barlineType);
    if (musicXmlBarlineType == mx::api::BarlineType::unspecified || musicXmlBarlineType == mx::api::BarlineType::unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(barlineType) << ".", MessageSeverity::Info);
        return;
    }
    setBarlineData(measure, musicXmlBarlineType, location);
}

void setBarline(
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
    setBarlineData(measure, musicXmlBarlineType, location);
}

void assignBarlines(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    bool isFinalMeasure,
    const MusxInstance<others::Staff>& staff)
{
    const auto barlineOptions = context.finaleOptions.barlineOptions;
    if (musxMeasure->leftBarlineType != BarlineType::OptionsDefault && musxMeasure->leftBarlineType != BarlineType::None) {
        setBarline(context, measure, musxMeasure->leftBarlineType, mx::api::HorizontalAlignment::left);
    }

    const auto classification = classify::classifyBarline(staff, musxMeasure, isFinalMeasure, barlineOptions);
    if (classification.type == classify::barline::Type::Unsupported) {
        context.logMessage(LogMsg() << "Skipping unsupported MusicXML barline type " << int(musxMeasure->barlineType) << ".", MessageSeverity::Info);
        return;
    }

    if (classification.type == classify::barline::Type::Regular) {
        if (classification.isShort) {
            setBarlineData(measure, mx::api::BarlineType::short_, mx::api::HorizontalAlignment::right);
        }
    } else {
        setBarline(context, measure, classification.type, mx::api::HorizontalAlignment::right);
    }

    const auto setRepeat = [&](mx::api::HorizontalAlignment location, mx::api::BarlineType repeatBarlineType) {
        auto& barline = ensureBarlineData(measure, location);
        if (barline.barlineType == mx::api::BarlineType::unspecified || barline.barlineType == mx::api::BarlineType::normal) {
            barline.barlineType = repeatBarlineType;
        }
        barline.repeat = true;
    };

    if (musxMeasure->forwardRepeatBar) {
        setRepeat(mx::api::HorizontalAlignment::left, mx::api::BarlineType::heavyLight);
    }
    if (musxMeasure->backwardsRepeatBar) {
        setRepeat(mx::api::HorizontalAlignment::right, mx::api::BarlineType::lightHeavy);
    }
}

void assignRepeatEndings(
    const MusicXmlMusxMapping& context,
    mx::api::PartData& part)
{
    const auto endingStarts = context.document->getOthers()->getArray<others::RepeatEndingStart>(context.forPartId);
    for (const auto& ending : endingStarts) {
        const auto measureIndex = static_cast<size_t>(ending->getCmper() - 1);
        ASSERT_IF(measureIndex >= part.measures.size()) {
            continue;
        }
        auto& startBarline = ensureBarlineData(part.measures[measureIndex], mx::api::HorizontalAlignment::left);
        startBarline.endingType = mx::api::EndingType::start;
        if (const auto passList = context.document->getOthers()->get<others::RepeatPassList>(context.forPartId, ending->getCmper())) {
            if (!passList->values.empty()) {
                startBarline.endingNumber = passList->values.front();
                if (passList->values.size() > 1) {
                    context.logMessage(LogMsg() << "MusicXML ending at measure " << ending->getCmper()
                        << " has multiple pass numbers, but mx::api can only export the first.", MessageSeverity::Info);
                }
            }
        }
        const auto endingNumber = startBarline.endingNumber;

        const auto endMeasureIndex = static_cast<size_t>(ending->getCmper() + ending->calcEndingLength() - 2);
        ASSERT_IF(endMeasureIndex >= part.measures.size()) {
            continue;
        }
        auto& stopBarline = ensureBarlineData(part.measures[endMeasureIndex], mx::api::HorizontalAlignment::right);
        stopBarline.endingType = ending->calcIsOpen() ? mx::api::EndingType::discontinue : mx::api::EndingType::stop;
        stopBarline.endingNumber = endingNumber;
    }
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

mx::api::ComplexTimeSymbol musicXmlTimeSignatureSymbol(const MusxInstance<TimeSignature>& timeSignature)
{
    if (!timeSignature->getAbbreviatedSymbol()) {
        return mx::api::ComplexTimeSymbol::unspecified;
    }
    if (timeSignature->isCommonTime()) {
        return mx::api::ComplexTimeSymbol::common;
    }
    if (timeSignature->isCutTime()) {
        return mx::api::ComplexTimeSymbol::cut;
    }
    return mx::api::ComplexTimeSymbol::unspecified;
}

std::string musicXmlDecimalText(const Fraction& value)
{
    if (value.denominator() == 1) {
        return std::to_string(value.numerator());
    }

    constexpr int timeSignatureDecimalPlaces = 6;
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(timeSignatureDecimalPlaces)
        << static_cast<double>(value.numerator()) / static_cast<double>(value.denominator());
    auto result = stream.str();
    while (!result.empty() && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

std::optional<mx::api::TimeFraction> musicXmlTimeFraction(const TimeSignature::TimeSigComponent& componentIn)
{
    if (componentIn.counts.empty() || componentIn.units.empty()) {
        return std::nullopt;
    }

    const auto join = [](const auto& values, const auto& formatter) {
        std::string result;
        for (const auto& value : values) {
            if (!result.empty()) {
                result += " + ";
            }
            result += formatter(value);
        }
        return result;
    };

    const auto componentNormalized = componentIn.normalizeCompound();
    const auto counts = componentNormalized.counts;
    const auto units = componentNormalized.units;

    auto beats = join(counts, [](const Fraction& count) { return musicXmlDecimalText(count); });
    auto beatType = join(units, [](Edu unit) {
        if (!unit) {
            return std::string{};
        }
        return musicXmlDecimalText(Fraction(EDU_PER_WHOLE_NOTE, unit));
    });
    if (beatType.empty()) {
        return std::nullopt;
    }
    return mx::api::TimeFraction{std::move(beats), std::move(beatType)};
}

void processTempoChanges(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (!context.denigmaContext->includeTempoTool || measure.staves.empty()) {
        return;
    }

    std::optional<NoteType> tempoUnit;
    const auto tempoChanges = musxMeasure->getDocument()->getOthers()->getArray<others::TempoChange>(SCORE_PARTID, musxMeasure->getCmper());
    auto& directions = measure.staves.front().directions;
    for (const auto& tempoChange : tempoChanges) {
        if (tempoChange->isRelative) {
            continue;
        }
        if (!tempoUnit) {
            auto [count, unit] = musxMeasure->createTimeSignature()->calcSimplified();
            static_cast<void>(count);
            tempoUnit = std::min(unit, NoteType::Quarter);
        }
        const auto noteType = tempoUnit.value_or(NoteType::Quarter);
        const double quarterNotesPerMinute =
            musicXmlQuarterNotesPerMinute(classify::expression::TempoInfo{ {}, tempoChange->getAbsoluteTempo(noteType), Edu(noteType) });
        if (quarterNotesPerMinute < 0.0) {
            continue;
        }

        mx::api::DirectionData direction;
        direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(Fraction::fromEdu(tempoChange->eduPosition));
        direction.isStaffValueSpecified = false;
        direction.soundData.tempo = quarterNotesPerMinute;
        direction.isSoundDataSpecified = direction.soundData.isSpecified();
        directions.emplace_back(std::move(direction));
    }
}

mx::api::TimeFraction musicXmlCumulativeTimeFraction(const MusxInstance<TimeSignature>& timeSignature)
{
    auto [count, noteType] = timeSignature->calcSimplified();
    if (count.remainder()) {
        if ((Edu(noteType) % count.denominator()) == 0) {
            noteType = NoteType(Edu(noteType) / count.denominator());
            count *= count.denominator();
        }
    }
    return {std::to_string(count.quotient()), std::to_string(EDU_PER_WHOLE_NOTE / Edu(noteType))};
}

mx::api::TimeChoice createTimeChoice(const MusxInstance<TimeSignature>& timeSignature)
{
    auto metered = mx::api::MeteredTimeSignature{};
    metered.fractions.clear();
    for (const auto& component : timeSignature->components) {
        auto fraction = musicXmlTimeFraction(component);
        if (!fraction) {
            metered.fractions.clear();
            break;
        }
        metered.fractions.emplace_back(std::move(*fraction));
    }
    if (metered.fractions.empty()) {
        metered.fractions.emplace_back(musicXmlCumulativeTimeFraction(timeSignature));
    }
    metered.symbol = musicXmlTimeSignatureSymbol(timeSignature);
    auto result = mx::api::TimeChoice{mx::api::ComplexTimeSignature{std::move(metered)}};
    result.isImplicit = false;
    return result;
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

std::optional<mx::api::TransposeData> createTransposeData(
    const MusicXmlMusxMapping& context,
    const MusxInstance<others::StaffComposite>& staff)
{
    const auto [transpositionDisp, transpositionAlt] = staff->calcTranspositionInterval();
    if (!transpositionDisp && !transpositionAlt) {
        return std::nullopt;
    }

    const bool shouldEmitTransposition = context.finaleOptions.effectivePartGlobals->showTransposed
        || (context.finaleOptions.miscOptions->keepWrittenOctaveInConcertPitch
            && music_theory::calcTranspositionIsOctave(transpositionDisp, transpositionAlt));
    if (!shouldEmitTransposition) {
        return std::nullopt;
    }

    return mx::api::TransposeData(
        -music_theory::calc12EdoHalfstepsInInterval(transpositionDisp, transpositionAlt),
        -transpositionDisp);
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

bool transposeDataEqualIgnoringStaffAndTick(const mx::api::TransposeData& lhs, const mx::api::TransposeData& rhs)
{
    auto normalizedLhs = lhs;
    auto normalizedRhs = rhs;
    normalizedLhs.staffIndex = mx::api::INDEX_UNSPECIFIED;
    normalizedRhs.staffIndex = mx::api::INDEX_UNSPECIFIED;
    normalizedLhs.tickTimePosition = 0;
    normalizedRhs.tickTimePosition = 0;
    return normalizedLhs == normalizedRhs;
}

bool transposeDataEqualIgnoringStaffAndTick(
    const std::optional<mx::api::TransposeData>& lhs,
    const std::optional<mx::api::TransposeData>& rhs)
{
    if (!lhs || !rhs) {
        return !lhs && !rhs;
    }
    return transposeDataEqualIgnoringStaffAndTick(*lhs, *rhs);
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
    /// @note MusicXML <cancel> is intentionally not exported yet. If we need Finale-style
    /// explicit cancellation naturals later, this previous/current key comparison is the
    /// place to decide whether to populate mx::api::KeyData::cancel.
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
    const MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    const std::vector<StaffCmper>& staves,
    std::vector<std::optional<mx::api::TimeChoice>>& prevTimeSigs)
{
    ASSERT_IF(staves.empty()) {
        return;
    }

    // Staff-level hiding trumps the measure's show mode: a staff that hides time signatures
    // never shows one, even for ShowTimeSigMode::Always.
    const auto staffHidesTimeSignature = [&](StaffCmper staffId) {
        const auto staff = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId,
            musxMeasure->getCmper(), 0);
        if (!staff) {
            return false;
        }
        return context.forPartId == SCORE_PARTID ? staff->hideTimeSigs : staff->hideTimeSigsInParts;
    };
    const bool measureNeverShows = musxMeasure->showTime == others::Measure::ShowTimeSigMode::Never;
    const bool measureAlwaysShows = musxMeasure->showTime == others::Measure::ShowTimeSigMode::Always;

    // The base choices carry only staff-level visibility and drive the measure-to-measure
    // carry-forward comparison. Measure-scoped visibility (Never/Always) is applied only to
    // the emitted copies, so it cannot trigger spurious restatements on following measures.
    std::vector<mx::api::TimeChoice> baseTimeSigs;
    std::vector<mx::api::TimeChoice> emitTimeSigs;
    std::vector<bool> staffForced(staves.size());
    baseTimeSigs.reserve(staves.size());
    emitTimeSigs.reserve(staves.size());
    for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
        auto baseTimeSig = createTimeChoice(musxMeasure->createDisplayTimeSignature(staves[staffIndex]));
        const bool staffHides = staffHidesTimeSignature(staves[staffIndex]);
        if (staffHides) {
            baseTimeSig.display = mx::api::Bool::no;
        }
        auto emitTimeSig = baseTimeSig;
        if (measureNeverShows) {
            emitTimeSig.display = mx::api::Bool::no;
        } else if (measureAlwaysShows && !staffHides) {
            emitTimeSig.display = mx::api::Bool::yes;
            staffForced[staffIndex] = true;
        }
        baseTimeSigs.emplace_back(std::move(baseTimeSig));
        emitTimeSigs.emplace_back(std::move(emitTimeSig));
    }

    const bool allStavesSame = std::all_of(emitTimeSigs.begin(), emitTimeSigs.end(),
        [&](const auto& emitTimeSig) { return emitTimeSig == emitTimeSigs.front(); });
    const auto staffEmits = [&](size_t staffIndex) {
        return staffForced[staffIndex] || prevTimeSigs[staffIndex] != baseTimeSigs[staffIndex];
    };

    if (allStavesSame) {
        // An unnumbered <time> applies to (and resets) every staff, so emit one part-wide
        // time signature as soon as any staff's effective signature changed (or is forced).
        bool anyStaffEmits = false;
        for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
            anyStaffEmits = anyStaffEmits || staffEmits(staffIndex);
        }
        if (anyStaffEmits) {
            measure.timeSignature = emitTimeSigs.front();
        }
    } else {
        for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
            if (staffEmits(staffIndex)) {
                measure.staffTimeSignatures[static_cast<int>(staffIndex)] = emitTimeSigs[staffIndex];
            }
        }
    }

    for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
        prevTimeSigs[staffIndex] = std::move(baseTimeSigs[staffIndex]);
    }
}

void assignClefs(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    StaffCmper staffId,
    const MusxInstance<others::Measure>& musxMeasure,
    MusicXmlPitchContext pitchContext,
    std::optional<ClefIndex>& prevClefIndex)
{
    const auto& musxDocument = musxMeasure->getDocument();
    const auto measureStartStaff = others::StaffComposite::createCurrent(musxDocument, context.forPartId, staffId,
        musxMeasure->getCmper(), 0);
    if (!measureStartStaff) {
        context.logMessage(LogMsg() << "No staff information found for staff " << staffId << ".",
            MessageSeverity::Warning);
        return;
    }

    auto addClef = [&](const others::Staff::ClefChange& clefChange) {
        const auto clefIndex = clefChange.clefIndex;
        const auto location = clefChange.position;
        if (clefIndex == prevClefIndex) {
            return;
        }
        auto musxStaff = measureStartStaff;
        if (location && clefChange.showClefMode == ShowClefMode::WhenNeeded) {
            musxStaff = others::StaffComposite::createCurrent(musxDocument, context.forPartId, staffId,
                musxMeasure->getCmper(), location.calcEduDuration());
        }
        if (!musxStaff) {
            context.logMessage(LogMsg() << "No staff information found for staff " << staffId << ".",
                MessageSeverity::Warning);
            return;
        }

        auto clef = musicXmlClefFromMusxClef(context.finaleOptions.clefOptions->getClefDef(clefIndex), musxStaff);
        clef.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(location);
        clef.location = location ? mx::api::ClefLocation::midMeasure : mx::api::ClefLocation::unspecified;
        if (clefChange.showClefMode == ShowClefMode::Never
            || (clefChange.showClefMode == ShowClefMode::WhenNeeded && musxStaff->hideClefs)) {
            clef.printObject = mx::api::Bool::no;
        }
        staff.clefs.emplace_back(clef);
        prevClefIndex = clefIndex;
    };

    measureStartStaff->iterateClefChangesAtMeasure(musxMeasure->getCmper(), pitchContext == MusicXmlPitchContext::Written,
        [&](const others::Staff::ClefChange& clefChange) {
            addClef(clefChange);
            return true;
        });
}

void assignStaffAttributes(
    MusicXmlMusxMapping& context,
    mx::api::PartData& part,
    const MusxInstanceList<others::Measure>& musxMeasures,
    const std::vector<StaffCmper>& staves)
{
    ASSERT_IF(part.measures.size() != musxMeasures.size() || musxMeasures.empty()) {
        context.logMessage(LogMsg() << "Cannot assign MusicXML staff attributes for part " << part.uniqueId
            << ": measure count mismatch or empty MUSX measure list (MusicXML measures="
            << part.measures.size() << ", MUSX measures=" << musxMeasures.size() << ").",
            MessageSeverity::Warning);
        return;
    }

    const MeasCmper finaleMeasureId = musxMeasures.back()->getCmper();
    const auto systems = context.document->getOthers()->getArray<others::StaffSystem>(context.forPartId);
    const auto systemForMeasure = [&systems](MeasCmper measureId) -> MusxInstance<others::StaffSystem> {
        MusxInstance<others::StaffSystem> result;
        for (const auto& system : systems) {
            if (system->startMeas > measureId) {
                break;
            }
            result = system;
        }
        return result;
    };

    for (size_t staffIndex = 0; staffIndex < staves.size(); ++staffIndex) {
        const auto staffId = staves[staffIndex];
        std::set<MusicPoint> attributeChanges{ MusicPoint{} };
        if (const auto rawStaff = context.document->getOthers()->get<others::Staff>(context.forPartId, staffId); rawStaff && rawStaff->hasStyles) {
            const auto styleAssigns = context.document->getOthers()->getArray<others::StaffStyleAssign>(context.forPartId, staffId);
            for (const auto& styleAssign : styleAssigns) {
                attributeChanges.emplace(styleAssign->startMeas, Fraction::fromEdu(styleAssign->startEdu));
                if (const auto nextLocation = styleAssign->nextLocation(staffId)) {
                    attributeChanges.emplace(*nextLocation);
                }
            }
        }
        for (const auto& system : systems) {
            const auto systemStaves = context.document->getOthers()->getArray<others::StaffUsed>(context.forPartId, system->getCmper());
            if (systemStaves.getIndexForStaff(staffId).has_value()) {
                attributeChanges.emplace(system->startMeas, Fraction{});
            }
        }

        int prevStaffLines = music_theory::STANDARD_NUMBER_OF_STAFFLINES;
        std::optional<mx::api::TransposeData> prevTransposition;
        for (const auto& point : attributeChanges) {
            if (point.measureId <= 0 || size_t(point.measureId) > part.measures.size()) {
                continue;
            }

            const auto pointStaff = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId,
                point.measureId, point.position.calcEduDuration());
            ASSERT_IF(!pointStaff) {
                context.logMessage(LogMsg() << "No staff composite found for staff " << staffId
                    << " at measure " << point.measureId << ", edu " << point.position.calcEduDuration()
                    << " while assigning MusicXML staff attributes.", MessageSeverity::Warning);
                continue;
            }

            const auto measureStartStaff = [&]() -> MusxInstance<others::StaffComposite> {
                if (point.position == 0) {
                    return pointStaff;
                }
                if (point.measureId < finaleMeasureId) {
                    return others::StaffComposite::createCurrent(context.document, context.forPartId, staffId, point.measureId + 1, 0);
                }
                return nullptr;
            }();

            if (measureStartStaff) {
                // items here can only be changed at start of measure in musicxml/mx::api
                const auto measureId = measureStartStaff->getMeasureId();
                auto& measure = part.measures[size_t(measureId - 1)];
                ASSERT_IF(measure.staves.size() != staves.size()) {
                    context.logMessage(LogMsg() << "Measure " << measureId << " in part " << part.uniqueId
                        << " has " << measure.staves.size() << " staves, expected " << staves.size()
                        << " while assigning MusicXML measure-start staff attributes.", MessageSeverity::Warning);
                    return;
                }
                const auto system = systemForMeasure(measureId);
                ASSERT_IF(!system) {
                    context.logMessage(LogMsg() << "No staff system found for measure " << measureId
                        << " while assigning MusicXML staff attributes for staff " << staffId << ".",
                        MessageSeverity::Warning);
                    continue;
                }
                const int staffLines = measureStartStaff->calcNumberOfStafflines();
                if (staffLines != prevStaffLines) {
                    measure.staves[staffIndex].staffLines = staffLines;
                }
                prevStaffLines = staffLines;

                const Fraction lineSpaceFactor{measureStartStaff->lineSpace, Evpu(EVPU_PER_SPACE)};
                const Fraction systemScale = system->calcStaffScaling(staffId);
                context.layout.setStaffSize(measure.staves[staffIndex], staffId, lineSpaceFactor * systemScale, systemScale);
            }

            const auto currentTransposition = createTransposeData(context, pointStaff);
            if (!transposeDataEqualIgnoringStaffAndTick(prevTransposition, currentTransposition)) {
                const bool initialPointCoveredByPart =
                    point == MusicPoint{}
                    && transposeDataEqualIgnoringStaffAndTick(currentTransposition, part.transposition);
                if (!initialPointCoveredByPart) {
                    auto& measure = part.measures[size_t(point.measureId - 1)];
                    ASSERT_IF(measure.staves.size() != staves.size()) {
                        context.logMessage(LogMsg() << "Measure " << point.measureId << " in part " << part.uniqueId
                            << " has " << measure.staves.size() << " staves, expected " << staves.size()
                            << " while assigning MusicXML transposition attributes.", MessageSeverity::Warning);
                        return;
                    }
                    auto transpose = currentTransposition.value_or(mx::api::TransposeData{});
                    if (staves.size() > 1) {
                        transpose.staffIndex = static_cast<int>(staffIndex);
                    }
                    transpose.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(point.position);
                    measure.transpositions.emplace_back(transpose);
                }
                prevTransposition = currentTransposition;
            }
        }
    }
}

void processMeasureText(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId)
{
    if (!musxMeasure->hasTextBlock) {
        return;
    }
    const auto textAssignments = context.document->getDetails()->getArray<details::MeasureTextAssign>(
        musxMeasure->getRequestedPartId(), staffId, musxMeasure->getCmper());
    for (const auto& assignment : textAssignments) {
        if (assignment->hidden) {
            continue;
        }
        const auto currentStaff = others::StaffComposite::createCurrent(
            context.document, context.forPartId, staffId, musxMeasure->getCmper(), assignment->xDispEdu);
        if (!currentStaff) {
            continue;
        }

        auto rawText = assignment->getRawTextCtx(context.forPartId);
        if (!rawText) {
            continue;
        }

        auto direction = mx::api::DirectionData{};
        direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(Fraction::fromEdu((std::max)(Edu{}, assignment->xDispEdu)));
        direction.words = musicXmlWordsFromEnigmaText(context, rawText);
        if (direction.words.empty()) {
            continue;
        }

        const auto textBlock = assignment->getTextBlock();
        const auto horizontalAlignment = textBlock ? enumConvert<mx::api::HorizontalAlignment>(textBlock->justify)
                                                   : mx::api::HorizontalAlignment::unspecified;
        const bool useStandardFrameEnclosure = textBlock && textBlock->shapeId == 0 && textBlock->stdLineThickness > 0;
        const Evpu resolvedYEvpu = assignment->yDisp + (textBlock ? textBlock->yAdd : Evpu{});
        const Evpu defaultXEvpu = (textBlock ? textBlock->xAdd : Evpu{}) + (assignment->xDispEdu == 0 ? assignment->xDispEvpu : Evpu{});
        const Evpu defaultYEvpu = resolvedYEvpu - currentStaff->calcTopLineEvpu();
        const bool hasDefaultX = defaultXEvpu != 0;
        const bool hasDefaultY = defaultYEvpu != 0;
        if (resolvedYEvpu > currentStaff->calcTopLineEvpu()) {
            direction.placement = mx::api::Placement::above;
        } else if (resolvedYEvpu < currentStaff->calcBottomLineEvpu()) {
            direction.placement = mx::api::Placement::below;
        }
        for (auto& words : direction.words) {
            if (hasDefaultX) {
                words.positionData.defaultX = context.musicXmlTenthsFromEvpu(defaultXEvpu);
                words.positionData.isDefaultXSpecified = true;
            }
            if (hasDefaultY) {
                words.positionData.defaultY = context.musicXmlTenthsFromEvpu(defaultYEvpu);
                words.positionData.isDefaultYSpecified = true;
            }
            if (horizontalAlignment != mx::api::HorizontalAlignment::unspecified) {
                words.positionData.horizontalAlignmnet = horizontalAlignment;
            }
            if (useStandardFrameEnclosure) {
                words.enclosure = mx::api::RehearsalEnclosure::rectangle;
            }
        }

        staff.directions.emplace_back(std::move(direction));
    }
}

void addMeasureNumber(
    MusicXmlMusxMapping& context,
    mx::api::MeasureData& measure,
    const MusxInstance<others::Measure>& musxMeasure,
    const std::vector<StaffCmper>& staves,
    const std::vector<StaffCmper>& scoreStaves)
{
    const MeasCmper measureId = musxMeasure->getCmper();
    measure.number = std::to_string(measureId);
    const auto musxMeasureNumberRegion = musxMeasure->findMeasureNumberRegion();
    if (!musxMeasureNumberRegion || musxMeasure->noMeasNum) {
        measure.implicit = mx::api::Bool::yes;
    }
    if (musxMeasureNumberRegion) {
        const auto& scorePartData = context.forPartId == SCORE_PARTID
            ? musxMeasureNumberRegion->scoreData
            : musxMeasureNumberRegion->partData;
        if (measureId == musxMeasureNumberRegion->startMeas) {
            if (scorePartData->hideFirstMeasure) {
                measure.implicit = mx::api::Bool::yes;
            }
        }
        if (measureId == musxMeasureNumberRegion->calcFirstDisplayedMeasureId()) {
            const bool partDisplaysMeasureNumbers = std::ranges::any_of(staves, [&](StaffCmper staffId) {
                const auto staff = others::StaffComposite::createCurrent(
                    context.document, context.forPartId, staffId, measureId, 0);
                if (!staff) {
                    context.logMessage(LogMsg() << "Cannot determine measure-number visibility for staff " << staffId
                        << " in measure " << measureId << ".", MessageSeverity::Warning);
                    return false;
                }
                return !staff->hideMeasNums;
            });

            const bool partIsTopSystemStaff = !scoreStaves.empty()
                && std::ranges::find(staves, scoreStaves.front()) != staves.end();
            const bool partIsBottomSystemStaff = !scoreStaves.empty()
                && std::ranges::find(staves, scoreStaves.back()) != staves.end();

            const auto systemRelation = [&]() {
                const bool onlyTop = scorePartData->showOnTop && !scorePartData->showOnBottom;
                const bool onlyBottom = !scorePartData->showOnTop && scorePartData->showOnBottom;
                if (partDisplaysMeasureNumbers) {
                    if (onlyTop) {
                        return mx::api::SystemRelation::alsoTop;
                    }
                    if (onlyBottom) {
                        return mx::api::SystemRelation::alsoBottom;
                    }
                    return mx::api::SystemRelation::unspecified;
                }
                if (onlyTop && partIsTopSystemStaff) {
                    return mx::api::SystemRelation::onlyTop;
                }
                if (onlyBottom && partIsBottomSystemStaff) {
                    return mx::api::SystemRelation::onlyBottom;
                }
                return mx::api::SystemRelation::unspecified;
            }();

            if (partDisplaysMeasureNumbers || systemRelation != mx::api::SystemRelation::unspecified) {
                if (scorePartData->showOnEvery && scorePartData->incidence == 1) {
                    measure.measureNumbering = mx::api::MeasureNumbering::measure;
                } else if (scorePartData->showOnStart) {
                    measure.measureNumbering = mx::api::MeasureNumbering::system;
                }
            } else {
                measure.measureNumbering = mx::api::MeasureNumbering::none;
            }
            if (measure.measureNumbering != mx::api::MeasureNumbering::unspecified
                && measure.measureNumbering != mx::api::MeasureNumbering::none) {
                measure.measureNumberingMultipleRestAlways = scorePartData->showOnMmRest
                    ? mx::api::Bool::yes
                    : mx::api::Bool::no;
                measure.measureNumberingMultipleRestRange = scorePartData->showMmRange
                    ? mx::api::Bool::yes
                    : mx::api::Bool::no;
                measure.measureNumberingSystemRelation = systemRelation;
            }
        }
        const auto displayNum = musxMeasureNumberRegion->calcDisplayNumberTextFor(measureId);
        MUSX_ASSERT_IF(!displayNum) {
            return;
        }
        /// @todo Export displayNum once mx::api exposes a MeasureData field for measure-numbering element text.
    }
}

void createMeasuresForPart(MusicXmlMusxMapping& context, mx::api::PartData& part)
{
    context.clearCurrent();
    context.currentPart = &part;

    const auto stavesIt = context.partIdToStaves.find(part.uniqueId);
    if (stavesIt == context.partIdToStaves.end() || stavesIt->second.empty()) {
        context.logMessage(LogMsg() << "MusicXML part " << part.uniqueId << " is not mapped to any Finale staves.", MessageSeverity::Warning);
        return;
    }

    const auto musxMeasures = context.document->getOthers()->getArray<others::Measure>(context.forPartId);
    auto scoreStaves = std::vector<StaffCmper>{};
    for (const auto& item : context.document->getScrollViewStaves(context.forPartId)) {
        scoreStaves.emplace_back(item->staffId);
    }
    part.measures.reserve(musxMeasures.size());
    std::vector<std::optional<mx::api::KeyData>> prevKeyData(stavesIt->second.size());
    std::vector<std::optional<ClefIndex>> prevClefIndices(stavesIt->second.size());
    std::vector<std::optional<mx::api::TimeChoice>> prevTimeSigs(stavesIt->second.size());
    const auto pitchContext = pitchContextForPart(context, part.uniqueId);
    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        const auto& musxMeasure = musxMeasures[measureIndex];
        const bool isFinalMeasure = measureIndex + 1 == musxMeasures.size();
        auto& measure = part.measures.emplace_back(mx::api::MeasureData{});
        addMeasureNumber(context, measure, musxMeasure, stavesIt->second, scoreStaves);
        assignKeySignatures(context, measure, musxMeasure, stavesIt->second, pitchContext, prevKeyData);
        assignTimeSignature(context, measure, musxMeasure, stavesIt->second, prevTimeSigs);
        if (const auto partSymbolIt = context.partIdToPartSymbol.find(part.uniqueId); partSymbolIt != context.partIdToPartSymbol.end()) {
            measure.partSymbol = partSymbolIt->second;
        }

        measure.staves.resize(stavesIt->second.size());
        for (size_t staffIndex = 0; staffIndex < stavesIt->second.size(); ++staffIndex) {
            const StaffCmper staffId = stavesIt->second[staffIndex];
            auto& staff = measure.staves[staffIndex];
            assignClefs(context, staff, staffId, musxMeasure, pitchContext, prevClefIndices[staffIndex]);
            const auto musxStaffAtEnd = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId,
                musxMeasure->getCmper(), musxMeasure->calcDuration(staffId).calcEduDuration());
            assignBarlines(context, measure, musxMeasure, isFinalMeasure, musxStaffAtEnd);
            createNotesForMeasureStaff(context, measure, staff, musxMeasure, staffId, measureIndex, staffIndex);
        }
    }

    assignRepeatEndings(context, part);
    processSmartShapes(context, musxMeasures, stavesIt->second);

    for (size_t measureIndex = 0; measureIndex < musxMeasures.size(); ++measureIndex) {
        const auto& musxMeasure = musxMeasures[measureIndex];
        auto& measure = part.measures[measureIndex];
        processTempoChanges(context, measure, musxMeasure);
        for (size_t staffIndex = 0; staffIndex < stavesIt->second.size(); ++staffIndex) {
            const StaffCmper staffId = stavesIt->second[staffIndex];
            auto& staff = measure.staves[staffIndex];
            processMeasureText(context, staff, musxMeasure, staffId);
            processJumps(context, staff, musxMeasure, staffId, staffIndex);
            processExpressions(context, measure, staff, musxMeasure, staffId, staffIndex);
        }
    }

    assignStaffAttributes(context, part, musxMeasures, stavesIt->second);
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
