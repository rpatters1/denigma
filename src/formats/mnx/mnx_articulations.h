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

using namespace musx::dom;
using namespace musx::util;

namespace denigma {

namespace classify {
namespace articulation {
struct Arpeggio;
struct BreathMark;
struct Fermata;
} // namespace articulation
namespace glyph {
struct GlyphStyle;
} // namespace glyph
}

namespace formats {
namespace mnx {
namespace detail {

mnxdom::MarkingUpDownAuto calcPointing(const classify::glyph::GlyphStyle& glyphStyle);

std::optional<mnxdom::Fermata> makeFermata(
    const classify::articulation::Fermata& fermata,
    const classify::glyph::GlyphStyle& glyphStyle,
    VerticalPlacement placement);

std::optional<mnxdom::sequence::BreathMark> makeBreathMark(const classify::articulation::BreathMark& breathMark, VerticalPlacement placement);

std::optional<musx::util::ArpeggioSpanCandidate> makeArpeggio(
    const EntryInfoPtr& sourceEntry,
    const MusxInstance<details::ArticulationAssign>& assign,
    const classify::articulation::Arpeggio& arpeggio);
void appendArpeggioCandidate(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxPartMeasure, const musx::util::ArpeggioSpanCandidate& candidate);
void finalizeArpeggios(const MnxMusxMappingPtr& context);
void processArticulations(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo);
void processArticulations(const MnxMusxMappingPtr& context, mnxdom::sequence::FullMeasureRest& mnxFullMeasureRest, const EntryInfoPtr& musxEntryInfo);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
