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
    const std::shared_ptr<others::Measure>& musxMeasure,
    mnx::global::Measure& mnxMeasure,
    const std::shared_ptr<options::BarlineOptions>& musxBarlineOptions,
    bool isForFinalMeasure)
{
    if (musxBarlineOptions->drawBarlines) {
        switch (musxMeasure->barlineType) {
        default:
            break;
        case others::Measure::BarlineType::Normal:
            if (isForFinalMeasure && !musxBarlineOptions->drawFinalBarlineOnLastMeas) {
                // force normal on final bar
                mnxMeasure.create_barline(mnx::BarlineType::Regular);
            } else if (!isForFinalMeasure && musxBarlineOptions->drawDoubleBarlineBeforeKeyChanges) {
                if (const auto& nextMeasure = musxMeasure->getDocument()->getOthers()->get<others::Measure>(SCORE_PARTID, musxMeasure->getCmper() + 1)) {
                    if (!musxMeasure->keySignature->isSameKey(*nextMeasure->keySignature.get())) {
                        mnxMeasure.create_barline(mnx::BarlineType::Double);
                    }
                }
            }
            break;
        case others::Measure::BarlineType::Double:
            mnxMeasure.create_barline(mnx::BarlineType::Double);
            break;
        case others::Measure::BarlineType::Final:
            mnxMeasure.create_barline(mnx::BarlineType::Final);
            break;
        case others::Measure::BarlineType::Solid:
            mnxMeasure.create_barline(mnx::BarlineType::Heavy);
            break;
        case others::Measure::BarlineType::Dashed:
            mnxMeasure.create_barline(mnx::BarlineType::Dashed);
            break;
        case others::Measure::BarlineType::Tick:
            mnxMeasure.create_barline(mnx::BarlineType::Tick);
            break;
        }
    } else {
        mnxMeasure.create_barline(mnx::BarlineType::NoBarline);
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
    for (const auto& musxMeasure : musxMeasures) {
        auto mnxMeasure = mnxDocument->global().measures().append();
        // MNX default indices match our cmper values, so there is no reason to include them.
        // mnxMeasure.set_index(musxMeasure->getCmper());
        // Add barline if needed
        assignBarline(musxMeasure, mnxMeasure, musxBarlineOptions, musxMeasure->getCmper() == musxMeasures.size());
        // Add key if needed
        auto keyFifths = musxMeasure->keySignature->getAlteration();
        if (keyFifths && keyFifths != prevKeyFifths) {
            mnxMeasure.create_key(keyFifths.value());
            prevKeyFifths = keyFifths;
        }
        int visibleNumber = musxMeasure->calcDisplayNumber();
        if (visibleNumber != musxMeasure->getCmper()) {
            mnxMeasure.set_number(visibleNumber);
        }
    }
}

void createGlobal(const MnxMusxMappingPtr& context)
{
    createGlobalMeasures(context);
}

} // namespace mnxexp
} // namespace denigma
