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
    const MusxInstance<others::Measure>& musxMeasure,
    const MusxInstance<options::BarlineOptions>& musxBarlineOptions,
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
                    mnxMeasure.ensure_barline(mnx::BarlineType::Regular);
                } else if (!isForFinalMeasure && musxBarlineOptions->drawDoubleBarlineBeforeKeyChanges) {
                    if (const auto& nextMeasure = musxMeasure->getDocument()->getOthers()->get<others::Measure>(SCORE_PARTID, musxMeasure->getCmper() + 1)) {
                        if (!musxMeasure->createKeySignature()->isSame(*nextMeasure->createKeySignature().get())) {
                            mnxMeasure.ensure_barline(mnx::BarlineType::Double);
                        }
                    }
                }
                break;
            default:
                mnxMeasure.ensure_barline(enumConvert<mnx::BarlineType>(musxMeasure->barlineType));
                break;
        }
    } else {
        mnxMeasure.ensure_barline(mnx::BarlineType::NoBarline);
    }
}

static void createEnding(mnx::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    if (musxMeasure->hasEnding) {
        if (auto musxEnding = musxMeasure->getDocument()->getOthers()->get<others::RepeatEndingStart>(SCORE_PARTID, musxMeasure->getCmper())) {
            auto mnxEnding = mnxMeasure.ensure_ending(musxEnding->calcEndingLength());
            mnxEnding.set_open(musxEnding->calcIsOpen());
            if (auto musxNumbers = musxMeasure->getDocument()->getOthers()->get<others::RepeatPassList>(SCORE_PARTID, musxMeasure->getCmper())) {
                for (int value : musxNumbers->values) {
                    mnxEnding.ensure_numbers().push_back(value);
                }
            }
        }
    }
}

static musx::util::Fraction calcJumpLocation(const MusxInstance<others::TextRepeatAssign> repeatAssign, const MusxInstance<others::Measure>& musxMeasure)
{
    musx::util::Fraction result{};
    if (const auto def = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, repeatAssign->textRepeatId)) {
        if (def->justification != AlignJustify::Left) {
            const auto timeSig = musxMeasure->createTimeSignature();
            result = timeSig->calcTotalDuration();
        }
    }
    return result;
}

static std::optional<MusxInstance<others::TextRepeatAssign>> searchForJump(const MnxMusxMappingPtr& context, JumpType jumpType, const MusxInstance<others::Measure>& musxMeasure)
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
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(context, JumpType::Fine, musxMeasure)) {
        auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
        mnxMeasure.ensure_fine(mnxFractionFromFraction(location));
    }
}

static void createJump(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    constexpr auto jumpMapping = std::to_array<std::pair<JumpType, mnx::JumpType>>(
    {
        {JumpType::DalSegno, mnx::JumpType::Segno},
        {JumpType::DsAlFine, mnx::JumpType::DsAlFine},
    });

    for (const auto& mapping : jumpMapping) {
        if (auto repeatAssign = searchForJump(context, mapping.first, musxMeasure)) {
            auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
            mnxMeasure.ensure_jump(mapping.second, mnxFractionFromFraction(location));
        }            
    }
}

static void assignKey(
    mnx::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure,
    std::optional<int>& prevKeyFifths)
{
    auto keyFifths = musxMeasure->createKeySignature()->getAlteration(KeySignature::KeyContext::Concert);
    if (keyFifths != prevKeyFifths) {
        mnxMeasure.ensure_key(keyFifths);
        prevKeyFifths = keyFifths;
    }
}

static void assignDisplayNumber(mnx::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    if (const auto displayNumber = musxMeasure->calcDisplayNumber()) {
        if (displayNumber.value() != musxMeasure->getCmper()) {
            mnxMeasure.set_number(displayNumber.value());
        }
    }
}

static void assignRepeats(mnx::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    if (musxMeasure->forwardRepeatBar) {
        mnxMeasure.ensure_repeatStart();
    }
    if (musxMeasure->backwardsRepeatBar) {
        mnxMeasure.ensure_repeatEnd();
        /// @todo add `times` if appropriate.
    }
}

static void createSegno(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(context, JumpType::Segno, musxMeasure)) {
        auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
        auto segno = mnxMeasure.ensure_segno(mnxFractionFromFraction(location));
        if (auto repeatText = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatText>(SCORE_PARTID, repeatAssign.value()->textRepeatId)) {
            if (auto repeatDef = musxMeasure->getDocument()->getOthers()->get<others::TextRepeatDef>(SCORE_PARTID, repeatAssign.value()->textRepeatId)) {
                if (auto glyphName = utils::smuflGlyphNameForFont(repeatDef->font, repeatText->text)) {
                    segno.set_glyph(glyphName.value());
                }
            }
        }
    }
}

static void createTempos(const MnxMusxMappingPtr& context, mnx::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    auto createTempo = [&mnxMeasure](int bpm, Edu noteValue, Edu eduPosition) {
        auto mnxTempos = mnxMeasure.ensure_tempos();
        auto tempo = mnxTempos.append(bpm, mnxNoteValueFromEdu(noteValue));
        if (eduPosition) {
            auto pos = Fraction::fromEdu(eduPosition);
            tempo.ensure_location(mnxFractionFromFraction(pos));
        }    
    };
    std::map<Edu, MusxInstance<OthersBase>> temposAtPositions; // use OthersBase because it has to accept multiple types
    if (musxMeasure->hasExpression) {
        // Search in order of decreasing precedence: text exprs, then shape exprs, then tempo changes. Using emplace keeps the first one.
        // We want text exprs to be chosen over the others, if they coincide at a beat location.
        const auto expAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::MeasureExprAssign>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& expAssign : expAssigns) {
            if (!expAssign->calcIsAssignedInRequestedPart()) {
                continue;
            }
            if (const auto textExpr = expAssign->getTextExpression()) {
                if (textExpr->playbackType == others::PlaybackType::Tempo && textExpr->auxData1 > 0) {
                    temposAtPositions.emplace(expAssign->eduPosition, textExpr);
                }
            }
            else if (const auto shapeExpr = expAssign->getShapeExpression()) {
                if (shapeExpr->playbackType == others::PlaybackType::Tempo && shapeExpr->auxData1 > 0) {
                    temposAtPositions.emplace(expAssign->eduPosition, shapeExpr);
                }
            }
        }
    }
    std::optional<NoteType> tempoUnit;
    if (context->denigmaContext->includeTempoTool) {
        const auto tempoChanges = musxMeasure->getDocument()->getOthers()->getArray<others::TempoChange>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& tempoChange : tempoChanges) {
            if (!tempoChange->isRelative) {
                if (!tempoUnit) {
                    auto [count, unit] = musxMeasure->createTimeSignature()->calcSimplified();
                    tempoUnit = std::min(unit, NoteType::Quarter);
                }
                temposAtPositions.emplace(tempoChange->eduPosition, tempoChange);
            }
        }
    }
    for (const auto& it : temposAtPositions) {
        if (auto textExpr = std::dynamic_pointer_cast<const others::TextExpressionDef>(it.second)) {
            createTempo(textExpr->value, textExpr->auxData1, it.first);
        } else if (auto shapeExpr = std::dynamic_pointer_cast<const others::ShapeExpressionDef>(it.second)) {
            createTempo(shapeExpr->value, shapeExpr->auxData1, it.first);
        } else if (auto tempoChange = std::dynamic_pointer_cast<const others::TempoChange>(it.second)) {
            const auto noteType = tempoUnit.value_or(NoteType::Quarter);
            createTempo(tempoChange->getAbsoluteTempo(noteType), Edu(noteType), it.first);
            /// @todo hide this tempo change if MNX ever adds visibility to the tempo object.
        }
    }
}

static void assignTimeSignature(
    const MnxMusxMappingPtr& context,
    mnx::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure,
    MusxInstance<TimeSignature>& prevTimeSig)
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
        mnxMeasure.ensure_time(count.quotient(), enumConvert<mnx::TimeSignatureUnit>(noteType));
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
    MusxInstance<TimeSignature> prevTimeSig;
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxDocument->global().measures().append();
        mnxMeasure.set_id(calcGlobalMeasureId(musxMeasure->getCmper()));
        assignBarline(mnxMeasure, musxMeasure, musxBarlineOptions, musxMeasure->getCmper() == musxMeasures.size());
        createEnding(mnxMeasure, musxMeasure);
        createFine(context, mnxMeasure, musxMeasure);
        createJump(context, mnxMeasure, musxMeasure);
        assignKey(mnxMeasure, musxMeasure, prevKeyFifths);
        assignDisplayNumber(mnxMeasure, musxMeasure);
        assignRepeats(mnxMeasure, musxMeasure);
        createSegno(context, mnxMeasure, musxMeasure);
        createTempos(context, mnxMeasure, musxMeasure);
        assignTimeSignature(context, mnxMeasure, musxMeasure, prevTimeSig);
    }
}

static void createLyricsGlobal(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto& musxDocument = context->document;
    
    auto addLyrics = [&](const auto& lyricsTexts) {
        using PtrType = typename std::decay_t<decltype(lyricsTexts)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<texts::LyricsTextBase, T>, "lyricsTexts must be a subtype of LyricsTextBase");
        for (const auto& musxLyric : lyricsTexts) {
            if (musxLyric->syllables.empty()) {
                continue;
            }
            auto mnxLyrics = mnxDocument->global().ensure_lyrics();
            if (!mnxLyrics.lineMetadata().has_value()) {
                mnxLyrics.ensure_lineMetadata();
            }
            auto mnxLineMetadata = mnxLyrics.lineMetadata().value();
            std::string lineId = calcLyricLineId(std::string(T::XmlNodeName), musxLyric->getTextNumber());
            context->lyricLineIds.emplace(lineId);
            mnx::global::LyricLineMetadata metaData = mnxLineMetadata.append(lineId);
            std::string mnxLabel = std::string(T::XmlNodeName) + " " + std::to_string(musxLyric->getTextNumber());
            mnxLabel[0] = char(std::toupper(mnxLabel[0]));
            metaData.set_label(mnxLabel);
        }
    };
    addLyrics(musxDocument->getTexts()->getArray<texts::LyricsVerse>());
    addLyrics(musxDocument->getTexts()->getArray<texts::LyricsChorus>());
    addLyrics(musxDocument->getTexts()->getArray<texts::LyricsSection>());

    if (!mnxDocument->global().lyrics().has_value()) {
        return;
    }

    std::vector<std::pair<std::string, MusxInstance<details::Baseline>>> baselines;
    auto addBaselines = [&](const auto& lyricsBaselines) {
        using PtrType = typename std::decay_t<decltype(lyricsBaselines)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<details::Baseline, T>, "lyricsBaselines must be a subtype of Baseline");
        for (const auto& baseline : lyricsBaselines) {
            std::string type = std::string(T::TextType::XmlNodeName);
            const auto it = context->lyricLineIds.find(calcLyricLineId(type, baseline->lyricNumber.value_or(0)));
            if (it != context->lyricLineIds.end()) { // only process baselines for lyrics that actually exist
                baselines.emplace_back(std::make_pair(type, baseline));
            }
        }
    };
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsVerse>(SCORE_PARTID, 0, 0));
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsChorus>(SCORE_PARTID, 0, 0));
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsSection>(SCORE_PARTID, 0, 0));
    std::sort(baselines.begin(), baselines.end(), [](const auto& a, const auto& b) {
        if (a.second->baselineDisplacement != b.second->baselineDisplacement) {
            return a.second->baselineDisplacement > b.second->baselineDisplacement;
        }

        auto rank = [](const std::string& s) -> int {
            if (s == texts::LyricsVerse::XmlNodeName) return 0;
            if (s == texts::LyricsChorus::XmlNodeName) return 1;
            if (s == texts::LyricsSection::XmlNodeName) return 2;
            return 3; // All others come after.
        };

        int rankA = rank(a.first);
        int rankB = rank(b.first);
        if (rankA != rankB)
            return rankA < rankB;

        return a.second->lyricNumber < b.second->lyricNumber;
    });

    for (const auto& baseline : baselines) {
        assert(mnxDocument->global().lyrics().has_value()); // bug if false. see early return statement above.
        auto mnxLyrics = mnxDocument->global().lyrics().value();
        auto mnxLineOrder = mnxLyrics.ensure_lineOrder();
        mnxLineOrder.push_back(calcLyricLineId(baseline.first, baseline.second->lyricNumber.value_or(0)));
    }
}

void createGlobal(const MnxMusxMappingPtr& context)
{
    createLyricsGlobal(context);
    createGlobalMeasures(context);
}

} // namespace mnxexp
} // namespace denigma
