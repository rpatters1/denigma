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

#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "denigma/classify/jumps.h"
#include "musicxml_formatted_text.h"
#include "musx/util/EnigmaString.h"
#include "mx/api/CodaData.h"
#include "mx/api/DirectionData.h"
#include "mx/api/SegnoData.h"
#include "mx/api/SoundData.h"
#include "utils/smufl_support.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

std::string jumpTargetId(MeasCmper measureId)
{
    return std::to_string(measureId);
}

MusxInstance<others::StaffSystem> systemForMeasure(const MusicXmlMusxMapping& context, MeasCmper measureId)
{
    MusxInstance<others::StaffSystem> result;
    for (const auto& system : context.document->getOthers()->getArray<others::StaffSystem>(context.forPartId)) {
        if (system->startMeas > measureId) {
            break;
        }
        result = system;
    }
    return result;
}

bool shouldEmitJumpForStaff(
    const MusicXmlMusxMapping& context,
    const MusxInstance<others::TextRepeatAssign>& assignment,
    StaffCmper staffId,
    size_t staffIndex)
{
    if (assignment->hidden) {
        return false;
    }
    if (assignment->topStaffOnly && staffIndex != 0) {
        return false;
    }
    if (assignment->staffList == 0) {
        return true;
    }

    const auto system = systemForMeasure(context, assignment->getCmper());
    if (!system) {
        return true;
    }
    const auto systemStaves = context.document->getOthers()->getArray<others::StaffUsed>(context.forPartId, system->getCmper());
    const auto staff = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId, assignment->getCmper(), 0);
    return assignment->createStaffListSet().contains(staffId, systemStaves, staff && staff->hideRepeats);
}

std::vector<mx::api::WordsData> createJumpWords(
    const MusicXmlMusxMapping& context,
    const MusxInstance<others::TextRepeatDef>& repeatDef,
    const MusxInstance<others::TextRepeatText>& repeatText)
{
    if (!repeatDef || !repeatText || repeatText->text.empty()) {
        return {};
    }

    EnigmaTextChunk chunk{ repeatText->text, EnigmaStyles(context.document) };
    chunk.styles.font = repeatDef->font;
    auto words = musicXmlWordsFromEnigmaTextChunk(context, chunk);
    if (!words) {
        return {};
    }
    words->positionData.horizontalAlignment = enumConvert<mx::api::HorizontalAlignment>(repeatDef->justification);
    /// @todo Also set the MusicXML <words> justify attribute from TextRepeatDef::justification when mx::api::WordsData exposes it.
    return { std::move(*words) };
}

void appendVisibleJump(
    const MusicXmlMusxMapping& context,
    mx::api::DirectionData& direction,
    classify::jump::Jump jump,
    const MusxInstance<others::TextRepeatDef>& repeatDef,
    const MusxInstance<others::TextRepeatText>& repeatText)
{
    const auto glyphName = (repeatDef && repeatText) ? utils::smuflGlyphNameForFont(repeatDef->font, repeatText->text) : std::nullopt;
    if (glyphName && jump == classify::jump::Jump::Segno) {
        mx::api::SegnoData segno;
        segno.isSmuflSpecified = true;
        segno.smufl = *glyphName;
        direction.segnos.emplace_back(std::move(segno));
        return;
    }
    if (glyphName && jump == classify::jump::Jump::Coda) {
        mx::api::CodaData coda;
        coda.isSmuflSpecified = true;
        coda.smufl = *glyphName;
        direction.codas.emplace_back(std::move(coda));
        return;
    }

    auto words = createJumpWords(context, repeatDef, repeatText);
    direction.words.insert(direction.words.end(), std::make_move_iterator(words.begin()), std::make_move_iterator(words.end()));
}

void appendSoundJump(mx::api::DirectionData& direction, classify::jump::Jump playback, const MusxInstance<others::TextRepeatAssign>& assignment)
{
    auto& sound = direction.soundData;
    switch (playback) {
    case classify::jump::Jump::Segno:
        sound.segno = jumpTargetId(assignment->getCmper());
        break;
    case classify::jump::Jump::Coda:
        sound.coda = jumpTargetId(assignment->getCmper());
        break;
    default:
        break;
    }

    switch (playback) {
    case classify::jump::Jump::DaCapo:
    case classify::jump::Jump::DCAlFine:
    case classify::jump::Jump::DCAlCoda:
        sound.dacapo = mx::api::Bool::yes;
        break;
    case classify::jump::Jump::Fine:
        sound.fine = jumpTargetId(assignment->getCmper());
        break;
    default:
        break;
    }

    if (const auto targetMeasure = assignment->calcTargetMeasure()) {
        const auto targetId = jumpTargetId(*targetMeasure);
        switch (playback) {
        case classify::jump::Jump::DalSegno:
        case classify::jump::Jump::DsAlFine:
        case classify::jump::Jump::DsAlCoda:
            sound.dalsegno = targetId;
            break;
        case classify::jump::Jump::ToCoda:
            sound.tocoda = targetId;
            break;
        default:
            break;
        }
    }

    direction.isSoundDataSpecified = sound.isSpecified();
}

} // namespace

void processJumps(
    MusicXmlMusxMapping& context,
    mx::api::StaffData& staff,
    const MusxInstance<others::Measure>& musxMeasure,
    StaffCmper staffId,
    size_t staffIndex)
{
    if (!musxMeasure->hasTextRepeat) {
        return;
    }

    const auto assignments = context.document->getOthers()->getArray<others::TextRepeatAssign>(context.forPartId, musxMeasure->getCmper());
    for (const auto& assignment : assignments) {
        if (!shouldEmitJumpForStaff(context, assignment, staffId, staffIndex)) {
            continue;
        }
        const auto jump = classify::classifyJump(assignment);
        if (jump.visual == classify::jump::Jump::None && jump.playback == classify::jump::Jump::None) {
            continue;
        }

        const auto repeatDef = context.document->getOthers()->get<others::TextRepeatDef>(context.forPartId, assignment->textRepeatId);
        const auto repeatText = context.document->getOthers()->get<others::TextRepeatText>(context.forPartId, assignment->textRepeatId);
        auto direction = mx::api::DirectionData{};
        direction.tickTimePosition = 0;
        direction.placement = mx::api::Placement::above;
        direction.isStaffValueSpecified = true;

        if (jump.visual != classify::jump::Jump::None) {
            appendVisibleJump(context, direction, jump.visual, repeatDef, repeatText);
        }
        appendSoundJump(direction, jump.playback, assignment);

        if (!mx::api::isDirectionDataEmpty(direction)) {
            staff.directions.emplace_back(std::move(direction));
        }
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
