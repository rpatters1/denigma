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

#include "musx/musx.h"

namespace denigma {

struct FinaleOptions
{
    musx::dom::Cmper forPartId{};

    musx::dom::MusxInstance<musx::dom::options::FontOptions> fontOptions;
    musx::dom::MusxInstance<musx::dom::FontInfo> defaultMusicFont;

    musx::dom::MusxInstance<musx::dom::options::AccidentalOptions> accidentalOptions;
    musx::dom::MusxInstance<musx::dom::options::AlternateNotationOptions> alternateNotationOptions;
    musx::dom::MusxInstance<musx::dom::options::AugmentationDotOptions> augDotOptions;
    musx::dom::MusxInstance<musx::dom::options::BarlineOptions> barlineOptions;
    musx::dom::MusxInstance<musx::dom::options::BeamOptions> beamOptions;
    musx::dom::MusxInstance<musx::dom::options::ChordOptions> chordOptions;
    musx::dom::MusxInstance<musx::dom::options::ClefOptions> clefOptions;
    musx::dom::MusxInstance<musx::dom::options::FlagOptions> flagOptions;
    musx::dom::MusxInstance<musx::dom::options::GraceNoteOptions> graceOptions;
    musx::dom::MusxInstance<musx::dom::options::KeySignatureOptions> keyOptions;
    musx::dom::MusxInstance<musx::dom::options::LineCurveOptions> lineCurveOptions;
    musx::dom::MusxInstance<musx::dom::options::MiscOptions> miscOptions;
    musx::dom::MusxInstance<musx::dom::options::MusicSymbolOptions> musicSymbolOptions;
    musx::dom::MusxInstance<musx::dom::options::MultimeasureRestOptions> mmRestOptions;
    musx::dom::MusxInstance<musx::dom::options::MusicSpacingOptions> musicSpacing;
    musx::dom::MusxInstance<musx::dom::options::PageFormatOptions> pageFormatOptions;
    musx::dom::MusxInstance<musx::dom::options::PianoBraceBracketOptions> braceOptions;
    musx::dom::MusxInstance<musx::dom::options::RepeatOptions> repeatOptions;
    musx::dom::MusxInstance<musx::dom::options::SmartShapeOptions> smartShapeOptions;
    musx::dom::MusxInstance<musx::dom::options::StaffOptions> staffOptions;
    musx::dom::MusxInstance<musx::dom::options::StemOptions> stemOptions;
    musx::dom::MusxInstance<musx::dom::options::TieOptions> tieOptions;
    musx::dom::MusxInstance<musx::dom::options::TimeSignatureOptions> timeOptions;
    musx::dom::MusxInstance<musx::dom::options::TupletOptions> tupletOptions;

    // Values resolved for forPartId, applying Finale's score/part fallback rules.
    musx::dom::MusxInstance<musx::dom::options::PageFormatOptions::PageFormat> effectivePageFormat;
    musx::dom::MusxInstance<musx::dom::others::MeasureNumberRegion::ScorePartData> effectiveMeasNumScorePart;
    musx::dom::MusxInstance<musx::dom::others::PartGlobals> effectivePartGlobals;
};

FinaleOptions loadFinaleOptions(const musx::dom::DocumentPtr& document, musx::dom::Cmper forPartId = musx::dom::SCORE_PARTID);

} // namespace denigma
