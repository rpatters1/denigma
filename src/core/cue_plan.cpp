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

#include "core/cue_plan.h"

#include "musx/util/Cue.h"

namespace denigma {

CueStaffMeasurePlan createCueStaffMeasurePlan(
    const musx::dom::details::GFrameHoldContext& staffMeasureContext,
    std::optional<int> forcedCueLayer)
{
    CueStaffMeasurePlan result;
    const auto analysis = musx::util::Cue::calcStaffMeasureAnalysis(staffMeasureContext);
    result.isDetectedCueOnly = analysis.isCueOnly;
    result.cueLayers.insert(analysis.cueLayers.begin(), analysis.cueLayers.end());
    result.visibleCueLayers.insert(analysis.visibleCueLayers.begin(), analysis.visibleCueLayers.end());
    result.detectedCueLayers.insert(analysis.cueLayers.begin(), analysis.cueLayers.end());

    if (forcedCueLayer) {
        const auto layer = static_cast<musx::dom::LayerIndex>(*forcedCueLayer - 1);
        if (layer < staffMeasureContext->frames.size() && staffMeasureContext->frames[layer] != 0) {
            result.forcedCueLayer = layer;
            result.cueLayers.emplace(layer);
            if (const auto frame = staffMeasureContext.createEntryFrame(layer)) {
                frame->iterateEntries([&](const musx::dom::EntryInfoPtr& entry) {
                    if (musx::util::Cue::calcIsVisibleInRequestedContext(entry)) {
                        result.visibleCueLayers.emplace(layer);
                        return false;
                    }
                    return true;
                });
            }
        }
    }

    return result;
}

} // namespace denigma
