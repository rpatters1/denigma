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
#include <optional>

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

std::optional<mx::api::MarkType> musicXmlMarkType(const classify::ArticulationMark& mark)
{
    switch (mark.type) {
    case classify::ArticulationMark::Type::Accent: return mx::api::MarkType::accent;
    case classify::ArticulationMark::Type::BrassBend: return mx::api::MarkType::brassBend;
    case classify::ArticulationMark::Type::BrassDoit: return mx::api::MarkType::doit;
    case classify::ArticulationMark::Type::BrassFalloff: return mx::api::MarkType::falloff;
    case classify::ArticulationMark::Type::BrassFlip: return mx::api::MarkType::flip;
    case classify::ArticulationMark::Type::BrassOpen: return mx::api::MarkType::open;
    case classify::ArticulationMark::Type::BrassPlop: return mx::api::MarkType::plop;
    case classify::ArticulationMark::Type::BrassScoop: return mx::api::MarkType::scoop;
    case classify::ArticulationMark::Type::BrassSmear: return mx::api::MarkType::smear;
    case classify::ArticulationMark::Type::BrassStopped: return mx::api::MarkType::stopped;
    case classify::ArticulationMark::Type::BuzzPizzicato: return mx::api::MarkType::otherTechnical;
    case classify::ArticulationMark::Type::DownBow: return mx::api::MarkType::downBow;
    case classify::ArticulationMark::Type::Fingernails: return mx::api::MarkType::fingernails;
    case classify::ArticulationMark::Type::SnapPizzicato: return mx::api::MarkType::snapPizzicato;
    case classify::ArticulationMark::Type::SoftAccent: return mx::api::MarkType::softAccent;
    case classify::ArticulationMark::Type::Spiccato: return mx::api::MarkType::spiccato;
    case classify::ArticulationMark::Type::Staccatissimo: return mx::api::MarkType::staccatissimo;
    case classify::ArticulationMark::Type::Staccato: return mx::api::MarkType::staccato;
    case classify::ArticulationMark::Type::Stress: return mx::api::MarkType::stress;
    case classify::ArticulationMark::Type::StringHarmonic: return mx::api::MarkType::harmonic;
    case classify::ArticulationMark::Type::StrongAccent: return mx::api::MarkType::strongAccent;
    case classify::ArticulationMark::Type::Tenuto: return mx::api::MarkType::tenuto;
    case classify::ArticulationMark::Type::Unstress: return mx::api::MarkType::unstress;
    case classify::ArticulationMark::Type::UpBow: return mx::api::MarkType::upBow;
    case classify::ArticulationMark::Type::BrassLift:
        break;
    }
    return std::nullopt;
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

mx::api::MarkType musicXmlFermataType(const classify::Fermata& fermata)
{
    switch (fermata.glyphStyle.placement) {
    case VerticalPlacement::Above:
        switch (fermata.shape) {
        case classify::Fermata::Shape::Normal: return mx::api::MarkType::fermataNormalUpright;
        case classify::Fermata::Shape::Angled: return mx::api::MarkType::fermataAngledUpright;
        case classify::Fermata::Shape::Square: return mx::api::MarkType::fermataSquareUpright;
        case classify::Fermata::Shape::Curlew:
        case classify::Fermata::Shape::DoubleAngled:
        case classify::Fermata::Shape::DoubleDot:
        case classify::Fermata::Shape::DoubleSquare:
        case classify::Fermata::Shape::HalfCurve:
            break;
        }
        break;
    case VerticalPlacement::Below:
        switch (fermata.shape) {
        case classify::Fermata::Shape::Normal: return mx::api::MarkType::fermataNormalInverted;
        case classify::Fermata::Shape::Angled: return mx::api::MarkType::fermataAngledInverted;
        case classify::Fermata::Shape::Square: return mx::api::MarkType::fermataSquareInverted;
        case classify::Fermata::Shape::Curlew:
        case classify::Fermata::Shape::DoubleAngled:
        case classify::Fermata::Shape::DoubleDot:
        case classify::Fermata::Shape::DoubleSquare:
        case classify::Fermata::Shape::HalfCurve:
            break;
        }
        break;
    case VerticalPlacement::Float:
    case VerticalPlacement::NotApplicable:
        break;
    }
    return enumConvert<mx::api::MarkType>(fermata.shape);
}

namespace {

void appendOrnament(mx::api::NoteData& note, const classify::Ornament& ornament, VerticalPlacement placement)
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
        if (const auto* articulation = classification.as<classify::ArticulationMarks>()) {
            auto hasMark = [&](classify::ArticulationMark::Type type) {
                return std::ranges::any_of(articulation->marks, [type](const auto& mark) { return mark.type == type; });
            };
            if (articulation->marks.size() == 2 && hasMark(classify::ArticulationMark::Type::Staccato)
                    && hasMark(classify::ArticulationMark::Type::Tenuto)) {
                note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::detachedLegato, classification.placement));
                continue;
            }
            for (const auto& mark : articulation->marks) {
                if (const auto markType = musicXmlMarkType(mark)) {
                    auto markData = musicXmlMark(markType.value(), classification.placement);
                    if (mark.type == classify::ArticulationMark::Type::BuzzPizzicato) {
                        markData.name = "buzz pizzicato";
                    }
                    note.noteAttachmentData.marks.emplace_back(markData);
                }
            }
        } else if (const auto* fermata = classification.as<classify::Fermata>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(musicXmlFermataType(*fermata), classification.placement));
        } else if (classification.is<classify::BreathMark>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::breathMark, classification.placement));
        } else if (classification.is<classify::Caesura>()) {
            note.noteAttachmentData.marks.emplace_back(musicXmlMark(mx::api::MarkType::caesura, classification.placement));
        } else if (const auto* ornament = classification.as<classify::Ornament>()) {
            appendOrnament(note, *ornament, classification.placement);
        } else if (const auto* tremolo = classification.as<classify::Tremolo>()) {
            constexpr int fallbackUnmeasuredTremoloMarks = 3;
            int marks = tremolo->marks;
            if (tremolo->style == classify::Tremolo::Style::Unmeasured) {
                context.logMessage(LogMsg() << "Unmeasured tremolo at entry " << entry->getEntryNumber()
                    << " cannot be exported through mx::api yet. Emitting a 3-slash single-note tremolo.", MessageSeverity::Info);
                marks = fallbackUnmeasuredTremoloMarks;
            } else {
                marks = marks = std::clamp(marks, MIN_SUPPORTED_TREMOLO_MARKS, MAX_SUPPORTED_TREMOLO_MARKS);
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
