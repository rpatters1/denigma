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
#include "core/finale_options.h"

#include <stdexcept>
#include <string>

namespace denigma {

namespace {

template <typename T>
musx::dom::MusxInstance<T> getRequiredOptions(const musx::dom::DocumentPtr& document, const std::string& optionsName)
{
    auto retval = document->getOptions()->get<T>();
    if (!retval) {
        throw std::invalid_argument("document contains no default " + optionsName + " options");
    }
    return retval;
}

} // namespace

FinaleOptions loadFinaleOptions(const musx::dom::DocumentPtr& document, musx::dom::Cmper forPartId)
{
    using namespace musx::dom;

    FinaleOptions retval;
    retval.forPartId = forPartId;

    retval.fontOptions = getRequiredOptions<options::FontOptions>(document, "font");
    retval.defaultMusicFont = retval.fontOptions->getFontInfo(options::FontOptions::FontType::Music);
    if (!retval.defaultMusicFont) {
        throw std::invalid_argument("document contains no information for the default music font.");
    }

    retval.accidentalOptions = getRequiredOptions<options::AccidentalOptions>(document, "accidental");
    retval.alternateNotationOptions = getRequiredOptions<options::AlternateNotationOptions>(document, "alternate notation");
    retval.augDotOptions = getRequiredOptions<options::AugmentationDotOptions>(document, "augmentation dot");
    retval.barlineOptions = getRequiredOptions<options::BarlineOptions>(document, "barline");
    retval.beamOptions = getRequiredOptions<options::BeamOptions>(document, "beam");
    retval.chordOptions = getRequiredOptions<options::ChordOptions>(document, "chord");
    retval.clefOptions = getRequiredOptions<options::ClefOptions>(document, "clef");
    retval.flagOptions = getRequiredOptions<options::FlagOptions>(document, "flag");
    retval.graceOptions = getRequiredOptions<options::GraceNoteOptions>(document, "grace note");
    retval.keyOptions = getRequiredOptions<options::KeySignatureOptions>(document, "key signature");
    retval.lineCurveOptions = getRequiredOptions<options::LineCurveOptions>(document, "lines & curves");
    retval.miscOptions = getRequiredOptions<options::MiscOptions>(document, "miscellaneous");
    retval.musicSymbolOptions = getRequiredOptions<options::MusicSymbolOptions>(document, "music symbol");
    retval.mmRestOptions = getRequiredOptions<options::MultimeasureRestOptions>(document, "multimeasure rest");
    retval.musicSpacing = getRequiredOptions<options::MusicSpacingOptions>(document, "music spacing");
    retval.pageFormatOptions = getRequiredOptions<options::PageFormatOptions>(document, "page format");
    retval.braceOptions = getRequiredOptions<options::PianoBraceBracketOptions>(document, "piano braces & brackets");
    retval.repeatOptions = getRequiredOptions<options::RepeatOptions>(document, "repeat");
    retval.smartShapeOptions = getRequiredOptions<options::SmartShapeOptions>(document, "smart shape");
    retval.staffOptions = getRequiredOptions<options::StaffOptions>(document, "staff");
    retval.stemOptions = getRequiredOptions<options::StemOptions>(document, "stem");
    retval.tieOptions = getRequiredOptions<options::TieOptions>(document, "tie");
    retval.timeOptions = getRequiredOptions<options::TimeSignatureOptions>(document, "time signature");
    retval.tupletOptions = getRequiredOptions<options::TupletOptions>(document, "tuplet");

    retval.effectivePageFormat = retval.pageFormatOptions->calcPageFormatForPart(forPartId);

    auto measNumRegions = document->getOthers()->getArray<others::MeasureNumberRegion>(forPartId);
    if (!measNumRegions.empty()) {
        retval.effectiveMeasNumScorePart = (forPartId && measNumRegions[0]->useScoreInfoForPart && measNumRegions[0]->partData)
                                         ? measNumRegions[0]->partData
                                         : measNumRegions[0]->scoreData;
        if (!retval.effectiveMeasNumScorePart) {
            throw std::invalid_argument("document contains no ScorePartData for measure number region "
                + std::to_string(measNumRegions[0]->getCmper()));
        }
    }

    retval.effectivePartGlobals = document->getOthers()->get<others::PartGlobals>(forPartId, MUSX_GLOBALS_CMPER);
    if (!retval.effectivePartGlobals) {
        throw std::invalid_argument("document contains no part globals");
    }

    return retval;
}

} // namespace denigma
