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

namespace denigma {
namespace mnxexp {

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

void appendDynamic(const MnxMusxMappingPtr& context, mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::MeasureExprAssign>& asgn, classify::DynamicClassification dynamicClass)
{
    if (asgn->layer > 0 && context->current.cueDiscardPlan.discardLayers.contains(asgn->layer - 1)) {
        return;
    }
    std::optional<mnx::DynamicValue> dynValue;
    std::optional<mnx::DynamicValue> attackValue;
    std::vector<std::string> glyphs = std::move(dynamicClass.glyphs);
    bool isAccent = false;
    using DynType = classify::Dynamic;
    switch (dynamicClass.dynamic) {
        case DynType::pppppp:
        case DynType::ppppp:
        case DynType::pppp:
            dynValue = mnx::DynamicValue::ppp;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::ppp:
            dynValue = mnx::DynamicValue::ppp;
            break;
        case DynType::pp:
            dynValue = mnx::DynamicValue::pp;
            break;
        case DynType::p:
            dynValue = mnx::DynamicValue::p;
            break;
        case DynType::mp:
            dynValue = mnx::DynamicValue::mp;
            break;
        case DynType::mf:
            dynValue = mnx::DynamicValue::mf;
            break;
        case DynType::f:
            dynValue = mnx::DynamicValue::f;
            break;
        case DynType::ff:
            dynValue = mnx::DynamicValue::ff;
            break;
        case DynType::fff:
            dynValue = mnx::DynamicValue::fff;
            break;
        case DynType::ffff:
        case DynType::fffff:
        case DynType::ffffff:
            dynValue = mnx::DynamicValue::fff;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::fp:
            attackValue = mnx::DynamicValue::f;
            dynValue = mnx::DynamicValue::p;
            break;
        case DynType::ffp:
            attackValue = mnx::DynamicValue::ff;
            dynValue = mnx::DynamicValue::p;
            break;
        case DynType::fz:
        case DynType::sf:
        case DynType::sfz:
        case DynType::rf:
        case DynType::rfz:
            dynValue = mnx::DynamicValue::f;
            isAccent = true;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::ffz:
        case DynType::sffz:
            dynValue = mnx::DynamicValue::ff;
            isAccent = true;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::pf:
            attackValue = mnx::DynamicValue::p;
            dynValue = mnx::DynamicValue::f;
            break;
        case DynType::sfp:
        case DynType::sfzp:
            attackValue = mnx::DynamicValue::f;
            dynValue = mnx::DynamicValue::p;
            isAccent = true;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::sfpp:
            attackValue = mnx::DynamicValue::f;
            dynValue = mnx::DynamicValue::pp;
            isAccent = true;
            if (!glyphs.empty()) {
                glyphs = classify::dynamicCanonicalLetterGlyphs(dynamicClass.dynamic);
            }
            break;
        case DynType::n:
            dynValue = mnx::DynamicValue::n;
            break;
        case DynType::Other:
        case DynType::None:
            break;
    }
    if (!dynValue && (dynamicClass.change == classify::DynamicChange::Absolute || (dynamicClass.prefixText.empty() && dynamicClass.suffixText.empty()))) {
        return;
    }
    auto mnxDynamic = [&]() -> mnx::part::DynamicGroupBase {
        using DynRelType = classify::DynamicChange;
        if (dynamicClass.change != DynRelType::Absolute) {
            auto relValue = dynamicClass.change == DynRelType::RelativeIncrease
                ? mnx::DynamicRelativeValue::Louder
                : mnx::DynamicRelativeValue::Softer;
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
    if (!dynamicClass.prefixText.empty()) {
        mnxDynamic.set_prefix(dynamicClass.prefixText);
    }
    if (!dynamicClass.suffixText.empty()) {
        mnxDynamic.set_suffix(dynamicClass.suffixText);
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
    if (asgn->layer > 0 || asgn->voice2 || (entryInfo && entryInfo->getEntry()->v2Launch)) {
        mnxDynamic.set_staff(mnxStaffNumber.value_or(1));
        mnxDynamic.set_voice(calcVoice(mnxStaffNumber.value_or(1), voiceLayerIdx, entryVoice));
    } else if (mnxStaffNumber > 1) { // we get better import results not specifying the 1st staff number: this could become an option
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}

void attachFermata(const MnxMusxMappingPtr& context, mnx::part::Measure& mnxMeasure,
    const MusxInstance<others::MeasureExprAssign>& asgn, const denigma::classify::ExpressionFermata& fermataInfo,
    const ExpressionAttachmentContext& attachment, const mnx::Fermata& fermata)
{
    if (asgn->calcIsPartOfStaffListAssignment() || fermataInfo.isRightBarline) {
        return;
    }
    if (attachment.entryTarget) {
        switch (attachment.entryTarget->kind) {
        case EntryTargetKind::Event:
            mnx::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
            break;
        case EntryTargetKind::FullMeasureRest:
            mnx::sequence::FullMeasureRest(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
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

void attachBreathMark(const MnxMusxMappingPtr& context, const ExpressionAttachmentContext& attachment, const mnx::sequence::BreathMark& breathMark) {
    if (attachment.entryTarget && attachment.entryTarget->kind == EntryTargetKind::Event) {
        mnx::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).ensure_markings().set_breath(breathMark);
    }
};
} // namespace

void processExpressions(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure,
    mnx::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber)
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
            /// @todo Add calculation for above or below, rather than just Float, for marking placement
            switch (classification.type) {
            case classify::ExpressionType::Dynamic:
                appendDynamic(context, mnxMeasure, mnxStaffNumber, asgn, classification.dynamic());
                break;
            case classify::ExpressionType::Fermata: {
                const auto& fermata = classification.fermata();
                if (auto mnxFermata = makeFermata(fermata.fermata, fermata.glyphName, VerticalPlacement::Float)) {
                    attachFermata(context, mnxMeasure, asgn, fermata, calcAttachmentContext(context, asgn), mnxFermata.value());
                }
                break;
            }
            case classify::ExpressionType::BreathMark:
                if (auto mnxBreathMark = makeBreathMark(classification.breathMark().breathMark, VerticalPlacement::Float)) {
                    attachBreathMark(context, calcAttachmentContext(context, asgn), mnxBreathMark.value());
                }
                break;
            case classify::ExpressionType::NonArpeggio:
                appendArpeggioCandidate(context, mnxMeasure, classification.nonArpeggio().candidate);
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

} // namespace mnxexp
} // namespace denigma
