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
#include <map>
#include <unordered_map>

#include "denigma/classify/barlines.h"
#include "denigma/classify/expressions.h"
#include "mnx.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

static void assignBarline(
    const MnxMusxMappingPtr& context,
    mnxdom::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure,
    const MusxInstance<options::BarlineOptions>& musxBarlineOptions,
    bool isForFinalMeasure)
{
    /// @todo MNX global measures have a single barline type, while Finale staff geometry can vary by staff.
    /// MNX will probably need to revisit how short barlines are handled in any case. Revise if/when that happens.
    const auto findRepresentativeStaff = [&]() -> MusxInstance<others::Staff> {
        MusxInstance<others::Staff> firstFoundStaff;
        const auto endOfBar = musxMeasure->calcDuration().calcEduDuration();
        for (const auto& staffSlot : context->document->getScrollViewStaves(SCORE_PARTID)) {
            auto staff = staffSlot->getStaffInstance(musxMeasure->getCmper(), endOfBar);
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
    };

    const auto musxStaff = findRepresentativeStaff();
    const auto classification = classify::classifyBarline(
        musxStaff, musxMeasure, isForFinalMeasure, musxBarlineOptions);
    
    switch (classification.type) {
    case classify::barline::Type::Unsupported:
        break;
    case classify::barline::Type::Regular:
        if (classification.isShort) {
            mnxMeasure.ensure_barline(mnxdom::BarlineType::Short);
        } else if (isForFinalMeasure) {
            mnxMeasure.ensure_barline(mnxdom::BarlineType::Regular);
        }
        break;
    case classify::barline::Type::Final:
        if (!isForFinalMeasure) {
            mnxMeasure.ensure_barline(mnxdom::BarlineType::Final);
        }
        break;
    case classify::barline::Type::Dashed:
    case classify::barline::Type::Double:
    case classify::barline::Type::Heavy:
    case classify::barline::Type::NoBarline:
    case classify::barline::Type::Tick:
        mnxMeasure.ensure_barline(enumConvert<mnxdom::BarlineType>(classification.type));
        break;
    }
}

static void createEnding(mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
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

enum class JumpClassificationSide
{
    Visual,
    Playback
};

static std::optional<MusxInstance<others::TextRepeatAssign>> searchForJump(
    classify::jump::Jump jumpType,
    const MusxInstance<others::Measure>& musxMeasure,
    JumpClassificationSide side)
{
    if (musxMeasure->hasTextRepeat) {
        auto textRepeatAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::TextRepeatAssign>(SCORE_PARTID, musxMeasure->getCmper());
        for (const auto& next : textRepeatAssigns) {
            const auto classification = classify::classifyJump(next);
            const auto candidate = (side == JumpClassificationSide::Visual) ? classification.visual : classification.playback;
            if (candidate == jumpType) {
                return next;
            }
        }
    }
    return std::nullopt;
}

static void createFine(
    mnxdom::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(classify::jump::Jump::Fine, musxMeasure, JumpClassificationSide::Playback)) {
        auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
        mnxMeasure.ensure_fine(mnxFractionFromFraction(location));
    }
}

static void createJump(
    mnxdom::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    constexpr auto jumpMapping = std::to_array<std::pair<classify::jump::Jump, mnxdom::JumpType>>(
    {
        {classify::jump::Jump::DalSegno, mnxdom::JumpType::Segno},
        {classify::jump::Jump::DsAlCoda, mnxdom::JumpType::Segno},
        {classify::jump::Jump::DsAlFine, mnxdom::JumpType::DsAlFine},
    });

    for (const auto& mapping : jumpMapping) {
        if (auto repeatAssign = searchForJump(mapping.first, musxMeasure, JumpClassificationSide::Playback)) {
            auto location = calcJumpLocation(repeatAssign.value(), musxMeasure);
            mnxMeasure.ensure_jump(mapping.second, mnxFractionFromFraction(location));
        }            
    }
}

static void assignKey(
    mnxdom::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure,
    std::optional<int>& prevKeyFifths)
{
    auto keyFifths = musxMeasure->createKeySignature()->getAlteration(KeySignature::KeyContext::Concert);
    if (keyFifths != prevKeyFifths) {
        mnxMeasure.ensure_key(keyFifths);
        prevKeyFifths = keyFifths;
    }
}

static void assignDisplayNumber(mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    if (const auto displayNumber = musxMeasure->calcDisplayNumber()) {
        if (displayNumber.value() != musxMeasure->getCmper()) {
            mnxMeasure.set_number(displayNumber.value());
        }
    }
}

static void assignRepeats(mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
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
    mnxdom::global::Measure& mnxMeasure,
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (auto repeatAssign = searchForJump(classify::jump::Jump::Segno, musxMeasure, JumpClassificationSide::Visual)) {
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

static void createTempos(const MnxMusxMappingPtr& context, mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    auto createTempo = [&mnxMeasure](int bpm, Edu noteValue, Edu eduPosition) {
        auto mnxTempos = mnxMeasure.ensure_tempos();
        auto tempo = mnxTempos.append(bpm, mnxNoteValueFromEdu(noteValue));
        if (eduPosition) {
            auto pos = Fraction::fromEdu(eduPosition);
            tempo.ensure_location(mnxFractionFromFraction(pos));
        }    
    };
    std::map<Edu, classify::expression::TempoInfo> temposAtPositions;
    if (musxMeasure->hasExpression) {
        // Search in order of decreasing precedence. Using emplace keeps the first tempo at a beat location.
        const auto expAssigns = musxMeasure->getDocument()->getOthers()->getArray<others::MeasureExprAssign>(SCORE_PARTID, musxMeasure->getCmper());
        const auto expAssignClassifications = classify::classifyExpressionAssignments(expAssigns);
        const auto addExpressionTempos = [&](bool textExpressions) {
            for (const auto& expAssignClassification : expAssignClassifications) {
                const auto& expAssign = expAssignClassification.assignment;
                if (!expAssign->calcIsAssignedInRequestedPart()) {
                    continue;
                }
                const bool isSelectedExpressionType = textExpressions
                    ? static_cast<bool>(expAssign->textExprId)
                    : static_cast<bool>(expAssign->shapeExprId);
                if (!isSelectedExpressionType) {
                    continue;
                }
                const auto& classification = expAssignClassification.classification;
                if (const auto* tempo = classification.as<classify::expression::TempoText>();
                    tempo && tempo->tempo.beatsPerMinute > 0 && tempo->tempo.beatUnitEdu > 0) {
                    temposAtPositions.emplace(expAssign->eduPosition, tempo->tempo);
                }
            }
        };
        addExpressionTempos(true);
        addExpressionTempos(false);
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
                const auto noteType = tempoUnit.value_or(NoteType::Quarter);
                temposAtPositions.emplace(tempoChange->eduPosition, classify::expression::TempoInfo{ {}, tempoChange->getAbsoluteTempo(noteType), Edu(noteType) });
            }
        }
    }
    for (const auto& [position, tempo] : temposAtPositions) {
        createTempo(tempo.beatsPerMinute, Edu(tempo.beatUnitEdu), position);
    }
    /// @todo hide tempo tool changes if MNX ever adds visibility to the tempo object.
}

static void assignTimeSignature(
    const MnxMusxMappingPtr& context,
    mnxdom::global::Measure& mnxMeasure,
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
                    << " has fractional portion that could not be reduced.", MessageSeverity::Warning);
            }
        }
        mnxMeasure.ensure_time(count.quotient(), enumConvert<mnxdom::TimeSignatureUnit>(noteType));
        prevTimeSig = timeSig;
    }
}

static void createBarlineFermata(const MnxMusxMappingPtr& context, mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    const auto& musxDocument = context->document;
    const auto forPartId = musxMeasure->getRequestedPartId();
    const auto exprAssigns = musxDocument->getOthers()->getArray<others::MeasureExprAssign>(forPartId, musxMeasure->getCmper());
    for (const auto& exprAssign : exprAssigns) {
        if (!exprAssign->hidden && exprAssign->calcIsPartOfStaffListAssignment()) {
            if (const auto textExp = exprAssign->getTextExpression(); textExp && textExp->horzMeasExprAlign == others::HorizontalMeasExprAlign::RightBarline) {
                const auto classification = classify::classifyExpression(textExp);
                if (const auto* fermata = classification.as<classify::expression::Fermata>()) {
                    auto placement = exprAssign->calcVerticalPlacement();
                    if (const auto mnxFermata = makeFermata(fermata->fermata, fermata->glyphStyle, placement)) {
                        mnxMeasure.set_fermata(mnxFermata.value());
                        break;
                    }
                }
            }
        }
    }
}

static void createGlobalMeasures(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    const auto& musxDocument = context->document;

    // Retrieve the linked parts in order.
    const auto musxMeasures = musxDocument->getOthers()->getArray<others::Measure>(SCORE_PARTID);
    const auto musxBarlineOptions = context->finaleOptions.barlineOptions;
    std::optional<int> prevKeyFifths;
    MusxInstance<TimeSignature> prevTimeSig;
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxDocument->global().measures().append();
        mnxMeasure.set_id(calcGlobalMeasureId(musxMeasure->getCmper()));
        assignBarline(context, mnxMeasure, musxMeasure, musxBarlineOptions, musxMeasure->getCmper() == musxMeasures.size());
        createEnding(mnxMeasure, musxMeasure);
        createBarlineFermata(context, mnxMeasure, musxMeasure);
        createFine(mnxMeasure, musxMeasure);
        createJump(mnxMeasure, musxMeasure);
        assignKey(mnxMeasure, musxMeasure, prevKeyFifths);
        assignDisplayNumber(mnxMeasure, musxMeasure);
        assignRepeats(mnxMeasure, musxMeasure);
        createSegno(mnxMeasure, musxMeasure);
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
            mnxdom::global::LyricLineMetadata metaData = mnxLineMetadata.append(lineId);
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
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsVerse>(SCORE_PARTID, 0, Cmper{0}));
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsChorus>(SCORE_PARTID, 0, Cmper{0}));
    addBaselines(musxDocument->getDetails()->getArray<details::BaselineLyricsSection>(SCORE_PARTID, 0, Cmper{0}));
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

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
