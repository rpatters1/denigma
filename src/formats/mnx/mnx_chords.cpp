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
#include "mnx.h"

#include <algorithm>
#include <string>

#include "core/element_ids.h"
#include "denigma/classify/chords.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {

std::optional<std::string> pitchStep(music_theory::NoteName noteName)
{
    using NoteName = music_theory::NoteName;
    switch (noteName) {
    case NoteName::A: return "A";
    case NoteName::B: return "B";
    case NoteName::C: return "C";
    case NoteName::D: return "D";
    case NoteName::E: return "E";
    case NoteName::F: return "F";
    case NoteName::G: return "G";
    }
    return std::nullopt;
}

std::optional<std::string> bassArrangement(details::ChordAssign::BassPosition position)
{
    using BassPosition = details::ChordAssign::BassPosition;
    switch (position) {
    case BassPosition::AfterRoot: return "horizontal";
    case BassPosition::UnderRoot: return "vertical";
    case BassPosition::Subtext: return "diagonal";
    }
    return std::nullopt;
}

GapTargetAnchor targetAnchor(const MnxMusxMapping& context, const MusxInstance<details::ChordAssign>& assignment)
{
    GapTargetAnchor anchor;
    const auto position = Fraction::fromEdu((std::max)(Edu{}, assignment->horzEdu));
    anchor.positionNumerator = position.numerator();
    anchor.positionDenominator = position.denominator();

    const auto staffId = assignment->getCmper1();
    std::optional<std::string> matchingPartId;
    std::optional<int> matchingStaff;
    for (const auto& [partId, staves] : context.part2Inst) {
        const auto staffIt = std::find(staves.begin(), staves.end(), staffId);
        if (staffIt == staves.end()) {
            continue;
        }
        if (matchingPartId) {
            return anchor;
        }
        matchingPartId = partId;
        if (staves.size() > 1) {
            matchingStaff = static_cast<int>(std::distance(staves.begin(), staffIt) + 1);
        }
    }
    if (matchingPartId) {
        anchor.partId = matchingPartId;
        anchor.measureId = core::calcPartMeasureId(*matchingPartId, assignment->getCmper2());
        anchor.staff = matchingStaff;
    }
    return anchor;
}

std::optional<ChordSymbolGapPayload> chordPayload(
    const MnxMusxMapping& context,
    const MusxInstance<details::ChordAssign>& assignment)
{
    const auto measure = context.document->getOthers()->get<others::Measure>(
        assignment->getSourcePartId(), assignment->getCmper2());
    const auto keySignature = measure ? measure->createKeySignature(assignment->getCmper1()) : nullptr;
    if (!keySignature) {
        return std::nullopt;
    }

    const auto root = keySignature->calcPitch(
        assignment->rootScaleNum, assignment->rootAlter, KeySignature::KeyContext::Written);
    const auto rootStep = pitchStep(root.noteName);
    if (!rootStep) {
        return std::nullopt;
    }
    const auto suffix = assignment->showSuffix
        ? classify::classifyChordSuffix(assignment->getChordSuffix())
        : classify::classifyChordSuffix();

    ChordSymbolGapPayload payload;
    payload.anchor = targetAnchor(context, assignment);
    payload.root = { *rootStep, root.alteration };
    payload.rootLowerCase = assignment->rootLowerCase;
    payload.showRoot = assignment->showRoot;
    if (suffix.quality) {
        payload.quality = classify::chordQualityName(*suffix.quality);
    }
    payload.suffixText = suffix.calcText();
    payload.showSuffix = assignment->showSuffix;
    if (assignment->showAltBass) {
        const auto bass = keySignature->calcPitch(
            assignment->bassScaleNum, assignment->bassAlter, KeySignature::KeyContext::Written);
        const auto bassStep = pitchStep(bass.noteName);
        if (!bassStep) {
            return std::nullopt;
        }
        payload.bass = ChordPitch{ *bassStep, bass.alteration };
        payload.bassLowerCase = assignment->bassLowerCase;
        payload.bassArrangement = bassArrangement(assignment->bassPosition);
    }
    for (const auto& degree : suffix.degrees) {
        payload.degrees.push_back({ degree.value, degree.alteration,
            std::string(classify::chordDegreeTypeName(degree.type)), degree.impliedByText });
    }
    payload.parenthesizeDegrees = suffix.parenthesizeDegrees;
    payload.stackDegrees = suffix.stackDegrees;
    payload.hasOuterParentheses = suffix.hasOuterParentheses;
    payload.hasUnrecognizedGlyphs = suffix.hasUnrecognizedGlyphs;
    return payload;
}

} // namespace

void reportUnsupportedChordSymbols(const MnxMusxMappingPtr& context)
{
    if (!context->denigmaContext->conversionResult) {
        return;
    }
    const auto assignments = context->document->getDetails()->getArray<details::ChordAssign>(SCORE_PARTID);
    for (const auto& assignment : assignments) {
        FinaleSourceLocator source;
        source.pool = "details";
        source.recordType = std::string(details::ChordAssign::XmlNodeName);
        source.partId = assignment->getSourcePartId();
        source.cmper1 = assignment->getCmper1();
        source.cmper2 = assignment->getCmper2();
        source.inci = assignment->getInci().value_or(0);
        auto payload = chordPayload(*context, assignment);
        context->denigmaContext->conversionResult->addGap({
            "finale.chord-symbol",
            CHORD_SYMBOL_GAP_PAYLOAD_VERSION,
            FormatId::MnxJson,
            GapRepresentation::None,
            GapCause::TargetUnsupported,
            std::move(source),
            "Chord symbols are not representable in standard MNX.",
            payload ? ConversionGapPayload(std::move(*payload)) : ConversionGapPayload{}
        });
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma