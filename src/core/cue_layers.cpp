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

#include "core/cue_layers.h"

namespace denigma {

CueLayerPlan createCueLayerPlan(const musx::dom::details::GFrameHoldContext& gfHold, std::optional<int> explicitCueLayer)
{
    CueLayerPlan result;
    constexpr bool includeVisibleInScore = false;
    const auto cueSummary = gfHold.calcCueSummary(includeVisibleInScore);
    result.discardWholeHold = cueSummary.isCueHold;
    result.heuristicCueLayers.insert(cueSummary.cueLayers.begin(), cueSummary.cueLayers.end());
    result.discardLayers.insert(cueSummary.cueLayers.begin(), cueSummary.cueLayers.end());

    if (explicitCueLayer) {
        const auto layer = static_cast<musx::dom::LayerIndex>(*explicitCueLayer - 1);
        if (layer < gfHold->frames.size() && gfHold->frames[layer] != 0) {
            result.explicitCueLayer = layer;
            result.discardLayers.emplace(layer);
        }
    }

    return result;
}

} // namespace denigma
