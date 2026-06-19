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
#pragma once

#include "core/denigma.h"
#include "musx/musx.h"
#include "mnxdom.h"

#include "mnx_fwd.h"
#include "utils/smufl_support.h"
#include "denigma/classify/articulations.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

mnx::MarkingUpDownAuto calcPointing(const std::string_view glyphName, VerticalPlacement placement);
mnx::MarkingUpDownAuto calcPointing(const MusxInstance<FontInfo>& fontInfo, char32_t sym, VerticalPlacement placement);

std::optional<mnx::Fermata> makeFermata(const classify::Fermata& fermata, const std::optional<std::string>& glyphName, VerticalPlacement placement);

std::optional<mnx::sequence::BreathMark> makeBreathMark(const classify::BreathMark& breathMark, VerticalPlacement placement);

std::optional<musx::util::ArpeggioSpanCandidate> makeArpeggio(
    const EntryInfoPtr& sourceEntry,
    const MusxInstance<details::ArticulationAssign>& assign,
    const classify::Arpeggio& arpeggio);
void appendArpeggioCandidate(const MnxMusxMappingPtr& context, mnx::part::Measure& mnxPartMeasure, const musx::util::ArpeggioSpanCandidate& candidate);
void finalizeArpeggios(const MnxMusxMappingPtr& context);

} // namespace mnxexp
} // namespace denigma
