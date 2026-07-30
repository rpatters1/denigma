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
#pragma once

#include <optional>
#include <unordered_set>

#include "musx/musx.h"

namespace denigma {

struct CueStaffMeasurePlan
{
    bool isDetectedCueOnly{}; ///< Every significant source layer was detected as cue material.
    std::unordered_set<musx::dom::LayerIndex> cueLayers; ///< Union of detected and forced cue layers.
    std::unordered_set<musx::dom::LayerIndex> visibleCueLayers; ///< Cue layers visible in the requested context.
    std::unordered_set<musx::dom::LayerIndex> detectedCueLayers; ///< Cue layers reported by musxdom analysis.
    std::optional<musx::dom::LayerIndex> forcedCueLayer; ///< 0-based layer selected by the caller, when present.

    bool isCueLayer(musx::dom::LayerIndex layer) const
    {
        return cueLayers.contains(layer);
    }

    bool isVisibleCueLayer(musx::dom::LayerIndex layer) const
    {
        return isCueLayer(layer) && visibleCueLayers.contains(layer);
    }
};

/// Creates exporter-neutral cue classification and requested-context visibility for one staff and measure.
CueStaffMeasurePlan createCueStaffMeasurePlan(
    const musx::dom::details::GFrameHoldContext& staffMeasureContext,
    std::optional<int> forcedCueLayer);

} // namespace denigma
