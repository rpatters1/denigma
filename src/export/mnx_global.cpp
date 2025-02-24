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
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mnxexp {

static void assignBarline(
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure,
    const std::shared_ptr<options::BarlineOptions>& musxBarlineOptions,
    bool isForFinalMeasure)
{
    if (musxBarlineOptions->drawBarlines) {
        switch (musxMeasure->barlineType) {
            case others::Measure::BarlineType::OptionsDefault:
            case others::Measure::BarlineType::Custom:
                // unable to convert: use default MNX value
                break;
            case others::Measure::BarlineType::Normal:
                if (isForFinalMeasure && !musxBarlineOptions->drawFinalBarlineOnLastMeas) {
                    // force normal on final bar
                    mnxMeasure.create_barline(mnx::BarlineType::Regular);
                } else if (!isForFinalMeasure && musxBarlineOptions->drawDoubleBarlineBeforeKeyChanges) {
                    if (const auto& nextMeasure = musxMeasure->getDocument()->getOthers()->get<others::Measure>(SCORE_PARTID, musxMeasure->getCmper() + 1)) {
                        if (!musxMeasure->keySignature->isSame(*nextMeasure->keySignature.get())) {
                            mnxMeasure.create_barline(mnx::BarlineType::Double);
                        }
                    }
                }
                break;
            default:
                mnxMeasure.create_barline(enumConvert<mnx::BarlineType>(musxMeasure->barlineType));
                break;
        }
    } else {
        mnxMeasure.create_barline(mnx::BarlineType::NoBarline);
    }
}

static void createEnding(mnx::global::Measure& mnxMeasure, const std::shared_ptr<others::Measure>& musxMeasure)
{
    if (musxMeasure->hasEnding) {
        if (auto musxEnding = musxMeasure->getDocument()->getOthers()->get<others::RepeatEndingStart>(SCORE_PARTID, musxMeasure->getCmper())) {
            auto mnxEnding = mnxMeasure.create_ending(musxEnding->calcEndingLength());
            mnxEnding.set_open(musxEnding->calcIsOpen());
            if (auto musxNumbers = musxMeasure->getDocument()->getOthers()->get<others::RepeatPassList>(SCORE_PARTID, musxMeasure->getCmper())) {
                for (int value : musxNumbers->values) {
                    if (!mnxEnding.numbers().has_value()) {
                        mnxEnding.create_numbers();
                    }
                    mnxEnding.numbers().value().push_back(value);
                }
            }
        }
    }
}

static musx::util::Fraction calcJumpLocation(const std::shared_ptr<others::TextRepeatAssign> repeatAssign, const std::shared_ptr<others::Measure>& musxMeasure)
{
    musx::util::Fraction result{};
    if (const auto def = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, repeatAssign->textRepeatId)) {
        if (def->justification != others::HorizontalTextJustification::Left) {
            const auto timeSig = musxMeasure->createTimeSignature();
            result = timeSig->calcTotalDuration();
        }
    }
    return result;
}

static std::optional<std::shared_ptr<others::TextRepeatAssign>> searchForJump(const MnxMusxMappingPtr& context, JumpType jumpType, const std::shared_ptr<others::Measure>& musxMeasure)
{
    if (musxMeasure->hasTextRepeat) {
        auto textRepeatAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::TextRepeatAssign>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& next : textRepeatAssigns) {
            const auto it = context->textRepeat2Jump.find(next->textRepeatId);
            if (it != context->textRepeat2Jump.end() && it->second == jumpType) {
                return next;
            }
        }
    }
    return std::nullopt;
}

static void createFine(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(context, JumpType::Fine, musxMeasure)) {
        auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
        mnxMeasure.create_fine(mnxFractionFromFraction(location));        
    }
}

static void createJump(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure)
{
    static const std::unordered_map<JumpType, mnx::JumpType> jumpMapping = {
        {JumpType::DalSegno, mnx::JumpType::Segno},
        {JumpType::DsAlFine, mnx::JumpType::DsAlFine},
    };

    for (const auto mapping : jumpMapping) {
        if (auto repeatAssign = searchForJump(context, mapping.first, musxMeasure)) {
            auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
            mnxMeasure.create_jump(mapping.second, mnxFractionFromFraction(location));        
        }            
    }
}

static void assignKey(
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure,
    std::optional<int>& prevKeyFifths)
{
    auto keyFifths = musxMeasure->keySignature->getAlteration();
    if (keyFifths && keyFifths != prevKeyFifths) {
        mnxMeasure.create_key(keyFifths.value());
        prevKeyFifths = keyFifths;
    }
}

static void assignDisplayNumber(mnx::global::Measure& mnxMeasure, const std::shared_ptr<others::Measure>& musxMeasure)
{
    int displayNumber = musxMeasure->calcDisplayNumber();
    if (displayNumber != musxMeasure->getCmper()) {
        mnxMeasure.set_number(displayNumber);
    }
}

static void assignRepeats(mnx::global::Measure& mnxMeasure, const std::shared_ptr<others::Measure>& musxMeasure)
{
    if (musxMeasure->forwardRepeatBar) {
        mnxMeasure.create_repeatStart();
    }
    if (musxMeasure->backwardsRepeatBar) {
        mnxMeasure.create_repeatEnd();
        /// @todo add `times` if appropriate.
    }
}

static void createSegno(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(context, JumpType::Segno, musxMeasure)) {
        auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
        auto segno = mnxMeasure.create_segno(mnxFractionFromFraction(location));
        if (auto repeatText = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatText>(SCORE_PARTID, repeatAssign.value()->textRepeatId)) {
            if (auto repeatDef = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, repeatAssign.value()->textRepeatId)) {
                if (auto glyphName = utils::smuflGlyphNameForFont(repeatDef->font, repeatText->text, *context->denigmaContext)) {
                    segno.set_glyph(glyphName.value());
                }
            }
        }
    }
}

static void createTempos(mnx::global::Measure& mnxMeasure, const std::shared_ptr<others::Measure>& musxMeasure)
{
    auto createTempo = [&mnxMeasure](int bpm, Edu noteValue, Edu eduPosition) {
        if (!mnxMeasure.tempos().has_value()) {
            mnxMeasure.create_tempos();
        }
        auto tempo = mnxMeasure.tempos().value().append(bpm, mnxNoteValueFromEdu(noteValue));
        if (eduPosition) {
            auto pos = Fraction::fromEdu(eduPosition);
            tempo.create_location(mnxFractionFromFraction(pos));
        }    
    };
    if (musxMeasure->hasExpression) {
        std::map<Edu, std::shared_ptr<OthersBase>> temposAtPositions; // use OthersBase because it has to accept multiple types
        // Search in order of decreasing precedence: text exprs, then shape exprs, then tempo changes. Using emplace keeps the first one.
        // We want text exprs to be chosen over the others, if they coincide at a beat location.
        const auto expAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::MeasureExprAssign>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& expAssign : expAssigns) {
            if (const auto textExpr = expAssign->getTextExpression()) {
                if (textExpr->playbackType == others::PlaybackType::Tempo) {
                    temposAtPositions.emplace(expAssign->eduPosition, textExpr);
                }
            } else if (const auto shapeExpr = expAssign->getShapeExpression()) {
                if (shapeExpr->playbackType == others::PlaybackType::Tempo) {
                    temposAtPositions.emplace(expAssign->eduPosition, shapeExpr);
                }
            }
        }
        const auto tempoChanges = musxMeasure->getDocument()->getOthers()->getArray<others::TempoChange>(SCORE_PARTID, musxMeasure->getCmper());
        std::optional<NoteType> tempoUnit;
        for (const auto& tempoChange : tempoChanges) {
            if (!tempoChange->isRelative) {
                if (!tempoUnit) {
                    auto [count, unit] = musxMeasure->createTimeSignature()->calcSimplified();
                    tempoUnit = std::min(unit, NoteType::Quarter);
                }
                temposAtPositions.emplace(tempoChange->eduPosition, tempoChange);
            }
        }
        for (const auto& it : temposAtPositions) {
            if (auto textExpr = std::dynamic_pointer_cast<others::TextExpressionDef>(it.second)) {
                createTempo(textExpr->value, textExpr->auxData1, it.first);
            } else if (auto shapeExpr = std::dynamic_pointer_cast<others::ShapeExpressionDef>(it.second)) {
                createTempo(shapeExpr->value, shapeExpr->auxData1, it.first);
            } else if (auto tempoChange = std::dynamic_pointer_cast<others::TempoChange>(it.second)) {
                const auto noteType = tempoUnit.value_or(NoteType::Quarter);
                createTempo(tempoChange->getAbsoluteTempo(noteType), Edu(noteType), it.first);
                /// @todo hide this tempo change if MNX ever adds visibility to the tempo object.
            }
        }
    }
}

static void assignTimeSignature(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<others::Measure>& musxMeasure,
    std::shared_ptr<TimeSignature>& prevTimeSig)
{
    auto timeSig = musxMeasure->createTimeSignature();
    if (!prevTimeSig || !timeSig->isSame(*prevTimeSig.get())) {
        auto [count, noteType] = timeSig->calcSimplified();
        /// @todo if MNX adds fractional meter, support that here instead of error message or further reduction
        if (count.remainder()) {
            if ((Edu(noteType) % count.denominator()) == 0) {
                noteType = musx::dom::NoteType(Edu(noteType) / count.denominator());
                count *= count.denominator();
            } else {
                context->logMessage(LogMsg() << "Time signature in measure " << musxMeasure->getCmper()
                    << " has fractional portion that could not be reduced.", LogSeverity::Warning);
            }
        }
        mnxMeasure.create_time(count.quotient(), enumConvert<mnx::TimeSignatureUnit>(noteType));
        prevTimeSig = timeSig;
    }
}

static void createGlobalMeasures(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto& musxDocument = context->document;

    // Retrieve the linked parts in order.
    auto musxMeasures = musxDocument->getOthers()->getArray<others::Measure>(SCORE_PARTID);
    auto musxBarlineOptions = musxDocument->getOptions()->get<options::BarlineOptions>();
    std::optional<int> prevKeyFifths;
    std::shared_ptr<TimeSignature> prevTimeSig;
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxDocument->global().measures().append();
        // MNX default indices match our cmper values, so there is no reason to include them.
        // mnxMeasure.set_index(musxMeasure->getCmper());
        assignBarline(mnxMeasure, musxMeasure, musxBarlineOptions, musxMeasure->getCmper() == musxMeasures.size());
        createEnding(mnxMeasure, musxMeasure);
        createFine(context, mnxMeasure, musxMeasure);
        createJump(context, mnxMeasure, musxMeasure);
        assignKey(mnxMeasure, musxMeasure, prevKeyFifths);
        assignDisplayNumber(mnxMeasure, musxMeasure);
        assignRepeats(mnxMeasure, musxMeasure);
        createSegno(context, mnxMeasure, musxMeasure);
        createTempos(mnxMeasure, musxMeasure);
        assignTimeSignature(context, mnxMeasure, musxMeasure, prevTimeSig);
    }
}

void createGlobal(const MnxMusxMappingPtr& context)
{
    createGlobalMeasures(context);
}

} // namespace mnxexp
} // namespace denigma
