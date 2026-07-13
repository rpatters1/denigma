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

#include "musicxml.h"

#include <algorithm>
#include <string_view>

#include "denigma/classify/articulations.h"
#include "mx/api/MarkData.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

mx::api::MarkData musicXmlMark(mx::api::MarkType type, VerticalPlacement placement)
{
    return mx::api::MarkData(enumConvert<mx::api::Placement>(placement), type);
}

namespace {

constexpr int MIN_SUPPORTED_TREMOLO_MARKS = 1;
constexpr int MAX_SUPPORTED_TREMOLO_MARKS = 5;

mx::api::MarkType musicXmlArticulationType(const classify::articulation::ArticulationMark& mark)
{
    using Placement = classify::GlyphStyle::Placement;
    if (mark.type == classify::articulation::ArticulationMark::Type::StrongAccent) {
        switch (mark.glyphStyle.placement) {
        case Placement::Above:
            return mx::api::MarkType::strongAccentUp;
        case Placement::Below:
            return mx::api::MarkType::strongAccentDown;
        case Placement::Automatic:
            break;
        }
    }
    return enumConvert<mx::api::MarkType>(mark.type);
}

mx::api::MarkType musicXmlTechniqueType(const classify::articulation::TechniqueMark& mark)
{
    return enumConvert<mx::api::MarkType>(mark.type);
}

mx::api::MarkType musicXmlHarmonMuteType(const classify::articulation::HarmonMute&)
{
    return mx::api::MarkType::harmonMute;
}

std::string fallbackNameForClassification(const classify::ArticulationClassification& classification, std::string_view fallbackName)
{
    if (classification.glyphName && !classification.glyphName->empty()) {
        return classification.glyphName.value();
    }
    return std::string(fallbackName);
}

mx::api::MarkData fallbackMarkData(
    mx::api::MarkType fallbackType, const classify::ArticulationClassification& classification, std::string_view fallbackName)
{
    auto markData = musicXmlMark(fallbackType, classification.placement);
    /// @todo Switch from MarkData.name to a dedicated SMuFL glyph-name field once mx::api exposes it for other-* mark payloads.
    markData.name = fallbackNameForClassification(classification, fallbackName);
    return markData;
}

mx::api::MarkType musicXmlOtherMarkType(const classify::articulation::OtherMark& mark)
{
    switch (mark.category) {
    case classify::articulation::OtherMark::Category::PerformanceTechnique:
        return mx::api::MarkType::otherTechnical;
    case classify::articulation::OtherMark::Category::Articulation:
        return mx::api::MarkType::otherArticulation;
    }
    return mx::api::MarkType::otherArticulation;
}

mx::api::MarkType musicXmlTremoloType(int marks)
{
    marks = std::clamp(marks, MIN_SUPPORTED_TREMOLO_MARKS, MAX_SUPPORTED_TREMOLO_MARKS);
    switch (marks) {
    case 1: return mx::api::MarkType::tremoloSingleOne;
    case 2: return mx::api::MarkType::tremoloSingleTwo;
    case 3: return mx::api::MarkType::tremoloSingleThree;
    case 4: return mx::api::MarkType::tremoloSingleFour;
    case 5: return mx::api::MarkType::tremoloSingleFive;
    }
    return mx::api::MarkType::tremoloSingleThree;
}

} // namespace

mx::api::MarkType musicXmlFermataType(const classify::articulation::Fermata& fermata)
{
    using Placement = classify::GlyphStyle::Placement;
    switch (fermata.glyphStyle.placement) {
    case Placement::Above:
        switch (fermata.shape) {
        case classify::articulation::Fermata::Shape::Normal: return mx::api::MarkType::fermataNormalUpright;
        case classify::articulation::Fermata::Shape::Angled: return mx::api::MarkType::fermataAngledUpright;
        case classify::articulation::Fermata::Shape::Square: return mx::api::MarkType::fermataSquareUpright;
        case classify::articulation::Fermata::Shape::Curlew:
        case classify::articulation::Fermata::Shape::DoubleAngled:
        case classify::articulation::Fermata::Shape::DoubleDot:
        case classify::articulation::Fermata::Shape::DoubleSquare:
        case classify::articulation::Fermata::Shape::HalfCurve:
            break;
        }
        break;
    case Placement::Below:
        switch (fermata.shape) {
        case classify::articulation::Fermata::Shape::Normal: return mx::api::MarkType::fermataNormalInverted;
        case classify::articulation::Fermata::Shape::Angled: return mx::api::MarkType::fermataAngledInverted;
        case classify::articulation::Fermata::Shape::Square: return mx::api::MarkType::fermataSquareInverted;
        case classify::articulation::Fermata::Shape::Curlew:
        case classify::articulation::Fermata::Shape::DoubleAngled:
        case classify::articulation::Fermata::Shape::DoubleDot:
        case classify::articulation::Fermata::Shape::DoubleSquare:
        case classify::articulation::Fermata::Shape::HalfCurve:
            break;
        }
        break;
    case Placement::Automatic:
        break;
    }
    return enumConvert<mx::api::MarkType>(fermata.shape);
}

namespace {

void appendOrnament(mx::api::NoteData& note, const classify::articulation::Ornament& ornament, VerticalPlacement placement)
{
    auto mark = musicXmlMark(enumConvert<mx::api::MarkType>(ornament.type), placement);
    /// @todo Export ornament accidentals once mx::api exposes the placement-bearing accidental-mark children.
    note.noteAttachmentData.marks.emplace_back(mark);
}

} // namespace

void processArticulations(MusicXmlMusxMapping& context, mx::api::NoteData& note, const EntryInfoPtr& entryInfo)
{
    const auto entry = entryInfo->getEntry();
    const auto articAssigns = context.document->getDetails()->getArray<details::ArticulationAssign>(SCORE_PARTID, entry->getEntryNumber());
    for (const auto& asgn : articAssigns) {
        if (asgn->hide) {
            continue;
        }
        const auto classification = classify::classifyArticulation(asgn, entryInfo);
        if (!classification) {
            continue;
        }
        if (const auto* articulation = classification.as<classify::articulation::ArticulationMarks>()) {
            auto hasMark = [&](classify::articulation::ArticulationMark::Type type) {
                return std::ranges::any_of(articulation->marks, [type](const auto& mark) { return mark.type == type; });
            };
            if (articulation->marks.size() == 2 && hasMark(classify::articulation::ArticulationMark::Type::Staccato)
                    && hasMark(classify::articulation::ArticulationMark::Type::Tenuto)) {
                note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::detachedLegato, classification.placement));
                continue;
            }
            for (const auto& mark : articulation->marks) {
                const auto markType = musicXmlArticulationType(mark);
                if (markType != mx::api::MarkType::unspecified) {
                    note.noteAttachmentData.marks.emplace_back(musicXmlMark(markType, classification.placement));
                } else {
                    note.noteAttachmentData.marks.emplace_back(
                        fallbackMarkData(mx::api::MarkType::otherArticulation, classification, "unmapped articulation"));
                }
            }
        } else if (const auto* technique = classification.as<classify::articulation::TechniqueMark>()) {
            const auto markType = musicXmlTechniqueType(*technique);
            if (markType != mx::api::MarkType::unspecified) {
                note.noteAttachmentData.marks.emplace_back(musicXmlMark(markType, classification.placement));
            } else {
                note.noteAttachmentData.marks.emplace_back(
                    fallbackMarkData(mx::api::MarkType::otherTechnical, classification, "unmapped technique"));
            }
        } else if (const auto* harmonMute = classification.as<classify::articulation::HarmonMute>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(musicXmlHarmonMuteType(*harmonMute), classification.placement));
        } else if (const auto* fermata = classification.as<classify::articulation::Fermata>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(musicXmlFermataType(*fermata), classification.placement));
        } else if (classification.is<classify::articulation::BreathMark>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::breathMark, classification.placement));
        } else if (classification.is<classify::articulation::Caesura>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::caesura, classification.placement));
        } else if (classification.is<classify::articulation::VerticalEntryBracket>()) {
            if (const auto candidate = musx::util::calcNonArpeggioSpanForAssignment(entryInfo, asgn)) {
                appendArpeggioCandidate(context, candidate.value());
            }
        } else if (const auto* other = classification.as<classify::articulation::OtherMark>()) {
            note.noteAttachmentData.marks.emplace_back(
                fallbackMarkData(musicXmlOtherMarkType(*other), classification, "unmapped mark"));
        } else if (const auto* ornament = classification.as<classify::articulation::Ornament>()) {
            appendOrnament(note, *ornament, classification.placement);
        } else if (const auto* tremolo = classification.as<classify::articulation::Tremolo>()) {
            constexpr int fallbackUnmeasuredTremoloMarks = 3;
            int marks = tremolo->marks;
            if (tremolo->style == classify::articulation::Tremolo::Style::Unmeasured) {
                context.logMessage(LogMsg() << "Unmeasured tremolo at entry " << entry->getEntryNumber()
                    << " cannot be exported through mx::api yet. Emitting a 3-slash single-note tremolo.", MessageSeverity::Info);
                marks = fallbackUnmeasuredTremoloMarks;
            } else {
                marks = std::clamp(marks, MIN_SUPPORTED_TREMOLO_MARKS, MAX_SUPPORTED_TREMOLO_MARKS);
                if (marks != tremolo->marks) {
                    context.logMessage(LogMsg() << "Measured single-note tremolo at entry " << entry->getEntryNumber()
                    << " has " << tremolo->marks << " marks. Clamping to mx::api's supported 1..5 range.", MessageSeverity::Info);
                }
            }
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(musicXmlTremoloType(marks), classification.placement));
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
