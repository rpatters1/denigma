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

#include "mnx.h"
#include "mnx_expressions.h"
#include "denigma/classify/expressions.h"
#include "utils/stringutils.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {
struct ExpressionAttachmentContext {
    EntryInfoPtr entryInfo;
    const EntryTarget* entryTarget{ nullptr };
};

ExpressionAttachmentContext calcAttachmentContext(const MnxMusxMappingPtr& context, const MusxInstance<others::MeasureExprAssign>& asgn)
    {
    ExpressionAttachmentContext result;
    result.entryInfo = asgn->calcAssociatedEntry();
    if (!result.entryInfo) {
        return result;
    }
    const auto entryNumber = result.entryInfo->getEntry()->getEntryNumber();
    const auto entryIt = context->entryTargetByNumber.find(entryNumber);
    if (entryIt != context->entryTargetByNumber.end()) {
        result.entryTarget = &entryIt->second;
    }
    return result;
}

struct MnxDynamicProjection
{
    classify::dynamics::Dynamic dynamic{};
    std::string prefixText;
    std::string suffixText;
    std::vector<std::string> glyphs;
    classify::dynamics::Change change{ classify::dynamics::Change::Absolute };

    [[nodiscard]] bool containsText() const noexcept
    { return !prefixText.empty() || !glyphs.empty() || !suffixText.empty(); }
};

std::optional<MnxDynamicProjection> projectPrimaryDynamicForMnx(const classify::ExpressionClassification& classification)
{
    auto appendText = [](std::string& dest, const classify::expression::RunClassification& run) {
        dest += run.chunk.text;
    };
    auto mergeQualifier = [](classify::dynamics::Change& current, classify::dynamics::Change next) {
        if (next == classify::dynamics::Change::Absolute) {
            return true;
        }
        if (current == classify::dynamics::Change::Absolute) {
            current = next;
            return true;
        }
        return current == next;
    };

    MnxDynamicProjection result;
    bool sawDynamic = false;
    bool afterDynamic = false;
    for (const auto& run : classification.runs) {
        if (const auto* dynamicMark = run.as<classify::dynamics::Mark>()) {
            if (sawDynamic) {
                return std::nullopt;
            }
            sawDynamic = true;
            result.dynamic = dynamicMark->dynamic;
            result.glyphs = dynamicMark->glyphs;
            afterDynamic = true;
            continue;
        }

        if (const auto* genericText = run.as<classify::expression::GenericText>()) {
            (void)genericText;
            if (afterDynamic) {
                appendText(result.suffixText, run);
            } else {
                appendText(result.prefixText, run);
            }
            continue;
        }

        if (const auto* qualifier = run.as<classify::expression::DynamicQualifier>()) {
            if (!mergeQualifier(result.change, qualifier->change)) {
                return std::nullopt;
            }
            if (afterDynamic) {
                appendText(result.suffixText, run);
            } else {
                appendText(result.prefixText, run);
            }
            continue;
        }

        return std::nullopt;
    }

    if (!sawDynamic || result.dynamic == classify::dynamics::Dynamic{}) {
        return std::nullopt;
    }
    result.prefixText = utils::trimAscii(result.prefixText);
    result.suffixText = utils::trimAscii(result.suffixText);
    if (result.dynamic == classify::dynamics::Dynamic::Other && result.glyphs.empty() && result.prefixText.empty() && result.suffixText.empty()) {
        const auto dynamicIt = std::find_if(classification.runs.begin(), classification.runs.end(), [](const classify::expression::RunClassification& run) {
            return std::holds_alternative<classify::dynamics::Mark>(run.value);
        });
        if (dynamicIt != classification.runs.end()) {
            result.prefixText = utils::trimAscii(dynamicIt->chunk.text);
        }
    }

    return result;
}

std::pair<std::optional<mnxdom::DynamicValue>, std::optional<mnxdom::DynamicValue>> calcDynamicType(
    classify::dynamics::Dynamic dynamic, bool& copyGlyphs, bool& isAccent)
{
    std::optional<mnxdom::DynamicValue> dynValue;
    std::optional<mnxdom::DynamicValue> attackValue;
    copyGlyphs = false;
    isAccent = false;

    using DynType = classify::dynamics::Dynamic;
    switch (dynamic) {
        case DynType::pppppp:
        case DynType::ppppp:
        case DynType::pppp:
            dynValue = mnxdom::DynamicValue::ppp;
            copyGlyphs = true;
            break;
        case DynType::ppp:
            dynValue = mnxdom::DynamicValue::ppp;
            break;
        case DynType::pp:
            dynValue = mnxdom::DynamicValue::pp;
            break;
        case DynType::p:
            dynValue = mnxdom::DynamicValue::p;
            break;
        case DynType::mp:
            dynValue = mnxdom::DynamicValue::mp;
            break;
        case DynType::mf:
            dynValue = mnxdom::DynamicValue::mf;
            break;
        case DynType::f:
            dynValue = mnxdom::DynamicValue::f;
            break;
        case DynType::ff:
            dynValue = mnxdom::DynamicValue::ff;
            break;
        case DynType::fff:
            dynValue = mnxdom::DynamicValue::fff;
            break;
        case DynType::ffff:
        case DynType::fffff:
        case DynType::ffffff:
            dynValue = mnxdom::DynamicValue::fff;
            copyGlyphs = true;
            break;
        case DynType::fp:
            attackValue = mnxdom::DynamicValue::f;
            dynValue = mnxdom::DynamicValue::p;
            break;
        case DynType::ffp:
            attackValue = mnxdom::DynamicValue::ff;
            dynValue = mnxdom::DynamicValue::p;
            break;
        case DynType::fz:
        case DynType::sf:
        case DynType::sfz:
        case DynType::rf:
        case DynType::rfz:
            dynValue = mnxdom::DynamicValue::f;
            isAccent = true;
            copyGlyphs = true;
            break;
        case DynType::ffz:
        case DynType::sffz:
            dynValue = mnxdom::DynamicValue::ff;
            isAccent = true;
            copyGlyphs = true;
            break;
        case DynType::pf:
            attackValue = mnxdom::DynamicValue::p;
            dynValue = mnxdom::DynamicValue::f;
            break;
        case DynType::sfp:
        case DynType::sfzp:
            attackValue = mnxdom::DynamicValue::f;
            dynValue = mnxdom::DynamicValue::p;
            isAccent = true;
            copyGlyphs = true;
            break;
        case DynType::sfpp:
            attackValue = mnxdom::DynamicValue::f;
            dynValue = mnxdom::DynamicValue::pp;
            isAccent = true;
            copyGlyphs = true;
            break;
        case DynType::n:
            dynValue = mnxdom::DynamicValue::n;
            break;
        case DynType::Other:
        case DynType::None:
            break;
    }

    return std::make_pair(dynValue, attackValue);
}

void appendDynamic(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::MeasureExprAssign>& asgn, const classify::ExpressionClassification& classification, VerticalPlacement placement)
{
    if (asgn->layer > 0 && context->current.cueDiscardPlan.discardLayers.contains(asgn->layer - 1)) {
        return;
    }

    auto dynamicClass = projectPrimaryDynamicForMnx(classification);
    if (!dynamicClass) {
        return;
    }

    bool isAccent{};
    bool copyGlyphs{};
    const auto [dynValue, attackValue] = calcDynamicType(dynamicClass->dynamic, copyGlyphs, isAccent);
    if (!dynValue && (dynamicClass->change == classify::dynamics::Change::Absolute || !dynamicClass->containsText())) {
        return;
    }

    std::vector<std::string> glyphs = std::move(dynamicClass->glyphs);
    if (copyGlyphs && !glyphs.empty()) {
        glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass->dynamic);
    }

    auto mnxDynamic = [&]() -> mnxdom::part::DynamicGroupBase {
        using DynRelType = classify::dynamics::Change;
        if (dynamicClass->change != DynRelType::Absolute) {
            auto relValue = dynamicClass->change == DynRelType::RelativeIncrease
                ? mnxdom::DynamicRelativeValue::Louder
                : mnxdom::DynamicRelativeValue::Softer;
            auto dyn = mnxMeasure.ensure_dynamics().appendRelative(relValue, mnxFractionFromEdu(asgn->eduPosition));
            if (dynValue) {
                dyn.set_value(dynValue.value());
            }
            return dyn;
        } else if (isAccent) {
            return mnxMeasure.ensure_dynamics().appendAccent(dynValue.value(), mnxFractionFromEdu(asgn->eduPosition));
        } else {
            return mnxMeasure.ensure_dynamics().appendImmediate(dynValue.value(), mnxFractionFromEdu(asgn->eduPosition));
        }
    }();
    if (attackValue) {
        mnxDynamic.set_attackValue(attackValue.value());
    }
    if (!dynamicClass->prefixText.empty()) {
        mnxDynamic.set_prefix(dynamicClass->prefixText);
    }
    if (!dynamicClass->suffixText.empty()) {
        mnxDynamic.set_suffix(dynamicClass->suffixText);
    }
    if (!glyphs.empty()) {
        mnxDynamic.ensure_glyphs().assign(glyphs);
    }
    const auto entryInfo = asgn->calcAssociatedEntry();
    int entryVoice = 1;
    LayerIndex voiceLayerIdx = asgn->layer > 0 ? asgn->layer - 1 : 0;
    if (entryInfo) {
        entryVoice = static_cast<int>(entryInfo->getEntry()->voice2) + 1;
        if (asgn->layer == 0) {
            voiceLayerIdx = entryInfo.getLayerIndex();
        }
        if (entryInfo->graceIndex > 0) {
            mnxDynamic.position().set_graceIndex(entryInfo.calcReverseGraceIndex());
        } else if (entryInfo.calcHasGraceNote()) {
            mnxDynamic.position().set_graceIndex(0);
        }
    } else if (context->current.gfhold) {
        if (const auto measure = context->document->getOthers()->get<others::Measure>(asgn->getRequestedPartId(), context->current.meas)) {
            if (musx::util::Fraction::fromEdu(asgn->eduPosition) >= measure->calcDuration(context->current.staff)) {
                // if we are at the end of a measure, explicitly require main position to ignore any grace
                // notes that might be trailing at the end of the frame
                mnxDynamic.position().set_graceIndex(0);
            }
        }
    }
    mnxDynamic.set_or_clear_orient(mnxMultiStaffOrientFromVerticalPlacement(mnxStaffNumber, placement));
    if (asgn->layer > 0 || asgn->voice2 || (entryInfo && entryInfo->getEntry()->v2Launch)) {
        mnxDynamic.set_staff(mnxStaffNumber.value_or(1));
        mnxDynamic.set_voice(calcVoice(mnxStaffNumber.value_or(1), voiceLayerIdx, entryVoice));
    } else if (mnxStaffNumber > 1) { // we get better import results not specifying the 1st staff number: this could become an option
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}

void attachFermata(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure,
    const MusxInstance<others::MeasureExprAssign>& asgn, const denigma::classify::expression::Fermata& fermataInfo,
    const ExpressionAttachmentContext& attachment, const mnxdom::Fermata& fermata)
{
    if (asgn->calcIsPartOfStaffListAssignment() || fermataInfo.isRightBarline) {
        return;
    }
    if (attachment.entryTarget) {
        switch (attachment.entryTarget->kind) {
        case EntryTargetKind::Event:
            mnxdom::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
            break;
        case EntryTargetKind::FullMeasureRest:
            mnxdom::sequence::FullMeasureRest(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
            break;
        }
    } else if (attachment.entryInfo) {
        context->logMessage(LogMsg() << "Entry " << attachment.entryInfo->getEntry()->getEntryNumber()
            << " was not mapped to an event or full measure rest", MessageSeverity::Warning);
    } else {
        if (mnxMeasure.sequences().empty()) {
            mnxMeasure.sequences().append();
        }
        for (auto seq : mnxMeasure.sequences()) {
            if (seq.content().empty()) {
                auto fullMeasureRest = seq.ensure_fullMeasure();
                fullMeasureRest.set_fermata(fermata);
            }
        }
    }
}

void attachBreathMark(const MnxMusxMappingPtr& context, const ExpressionAttachmentContext& attachment, const mnxdom::sequence::BreathMark& breathMark) {
    if (attachment.entryTarget && attachment.entryTarget->kind == EntryTargetKind::Event) {
        mnxdom::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).ensure_markings().set_breath(breathMark);
    }
};
} // namespace

void processExpressions(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasExpression) {
        auto exprAssigns = context->document->getOthers()->getArray<others::MeasureExprAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : exprAssigns) {
            if (asgn->hidden || asgn->staffAssign != context->current.staff) {
                continue;
            }
            if (!asgn->calcIsAssignedInRequestedPart()) {
                continue;
            }
            const auto classification = classify::classifyExpression(asgn);
            auto placement = asgn->calcVerticalPlacement();
            switch (classification.type) {
            case classify::ExpressionType::Dynamic:
                appendDynamic(context, mnxMeasure, mnxStaffNumber, asgn, classification, placement);
                break;
            case classify::ExpressionType::Fermata: {
                const auto& fermata = classification.fermata();
                if (auto mnxFermata = makeFermata(fermata.fermata, fermata.glyphStyle, placement)) {
                    attachFermata(context, mnxMeasure, asgn, fermata, calcAttachmentContext(context, asgn), mnxFermata.value());
                }
                break;
            }
            case classify::ExpressionType::BreathMark:
                if (auto mnxBreathMark = makeBreathMark(classification.breathMark().breathMark, placement)) {
                    attachBreathMark(context, calcAttachmentContext(context, asgn), mnxBreathMark.value());
                }
                break;
            case classify::ExpressionType::NonArpeggio:
                appendArpeggioCandidate(context, mnxMeasure, classification.nonArpeggio().candidate);
                break;
            case classify::ExpressionType::PseudoTie:
                break;
            case classify::ExpressionType::Error:
                context->logMessage(LogMsg() << classification.error().message, MessageSeverity::Warning);
                break;
            default:
                break;
            }
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
