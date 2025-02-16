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

static void createTempos(mnx::global::Measure& mnxMeasure, const std::shared_ptr<others::Measure>& musxMeasure)
{
    auto createTempo = [&mnxMeasure](int bpm, Edu noteValue, Edu eduPosition) {
        if (!mnxMeasure.tempos().has_value()) {
            mnxMeasure.create_tempos();
        }
        auto base = enumConvert<mnx::NoteValueBase>(calcNoteTypeFromEdu(noteValue));
        auto dots = [&]() -> std::optional<int> {
            if (int val = calcAugmentationDotsFromEdu(noteValue)) {
                return val;
            }
            return std::nullopt;
        }();
        auto tempo = mnxMeasure.tempos().value().append(bpm, base, dots);
        if (eduPosition) {
            auto pos = Fraction::fromEdu(eduPosition);
            tempo.create_location(pos.numerator(), pos.denominator());
        }    
    };
    if (musxMeasure->hasExpression) {
        std::map<Edu, std::shared_ptr<OthersBase>> temposAtPositions; // use OthersBase because it has to accept multiple types
        // Search in order of decreasing precedence: text exprs, then shape exprs, then tempo changes. Using emplace keeps the first one.
        // We want text exprs to be chosen over the others, if they coincide at a beat location.
        const auto expAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::MeasureExprAssign>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& expAssign : expAssigns) {
            if (const auto expr = expAssign->getTextExpression()) {
                if (expr->playbackType == others::PlaybackType::Tempo) {
                    temposAtPositions.emplace(expAssign->eduPosition, expr);
                }
            } else if (const auto expr = expAssign->getShapeExpression()) {
                if (expr->playbackType == others::PlaybackType::Tempo) {
                    temposAtPositions.emplace(expAssign->eduPosition, expr);
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
            if (auto expr = std::dynamic_pointer_cast<others::TextExpressionDef>(it.second)) {
                createTempo(expr->value, expr->auxData1, it.first);
            } else if (auto expr = std::dynamic_pointer_cast<others::ShapeExpressionDef>(it.second)) {
                createTempo(expr->value, expr->auxData1, it.first);
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
        if (count.remainder()) {
            if ((uint32_t(noteType) % count.denominator()) == 0) {
                count *= count.denominator();
                noteType = musx::dom::NoteType(uint32_t(noteType) / count.denominator());
            } else {
                context->denigmaContext->logMessage(LogMsg() << "Time signature in measure " << musxMeasure->getCmper()
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
        assignKey(mnxMeasure, musxMeasure, prevKeyFifths);
        assignDisplayNumber(mnxMeasure, musxMeasure);
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
