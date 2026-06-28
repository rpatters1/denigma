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

#include "musicxml.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

void createPageLayoutData(
    const MusicXmlMusxMapping& context,
    const options::PageFormatOptions::PageFormat& pagePrefs,
    double combinedSystemScaling)
{
    auto& pageLayout = context.musicXmlScore->defaults.pageLayout;
    pageLayout.size = mx::api::SizeData{
        context.musicXmlTenthsFromEvpu(pagePrefs.pageHeight, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(pagePrefs.pageWidth, combinedSystemScaling)
    };

    bool holdMargins = true;
    if (const auto firstPage = context.document->getOthers()->get<others::Page>(context.forPartId, 1)) {
        holdMargins = firstPage->holdMargins;
    }
    const auto pageScaleBackout = holdMargins ? combinedSystemScaling : pagePrefs.calcSystemScaling().toDouble();
    const auto createPageMargins = [&](double leftEvpu, double rightEvpu, double topEvpu, double bottomEvpu) {
        return mx::api::MarginsData{
            context.musicXmlTenthsFromEvpu(leftEvpu, pageScaleBackout),
            context.musicXmlTenthsFromEvpu(rightEvpu, pageScaleBackout),
            context.musicXmlTenthsFromEvpu(topEvpu, pageScaleBackout),
            context.musicXmlTenthsFromEvpu(bottomEvpu, pageScaleBackout)
        };
    };
    const auto evenMargins = createPageMargins(
        pagePrefs.leftPageMarginLeft,
        -pagePrefs.leftPageMarginRight,
        -pagePrefs.leftPageMarginTop,
        pagePrefs.leftPageMarginBottom);
    pageLayout.margins.even = evenMargins;
    pageLayout.margins.odd = pagePrefs.facingPages
        ? createPageMargins(
              pagePrefs.rightPageMarginLeft,
              -pagePrefs.rightPageMarginRight,
              -pagePrefs.rightPageMarginTop,
              pagePrefs.rightPageMarginBottom)
        : evenMargins;
}

} // namespace

void createDefaults(const MusicXmlMusxMapping& context)
{
    auto& score = *context.musicXmlScore;
    const auto& pagePrefs = context.finaleOptions.effectivePageFormat;
    score.ticksPerQuarter = context.timing.divisions;

    constexpr auto kUnscaledMmPerStaff = EVPU_PER_STANDARD_STAFF / EVPU_PER_MM;
    const auto combinedSystemScaling = pagePrefs->calcCombinedSystemScaling().toDouble();
    score.defaults.scalingMillimeters = combinedSystemScaling * kUnscaledMmPerStaff;
    score.defaults.scalingTenths = MUSICXML_DEFAULT_TENTHS_PER_STAFF; // standard value for many MusicXML exporters, including Finale

    createPageLayoutData(context, *pagePrefs, combinedSystemScaling);

    const auto systemScaleBackout = pagePrefs->calcSystemScaling().toDouble();
    score.defaults.systemLayout.margins = mx::api::LeftRight{
        context.musicXmlTenthsFromEvpu(pagePrefs->sysMarginLeft, systemScaleBackout),
        context.musicXmlTenthsFromEvpu(-pagePrefs->sysMarginRight, systemScaleBackout)
    };
    const auto systemDistance =
        -pagePrefs->sysMarginTop
        - pagePrefs->sysDistanceBetween
        - (pagePrefs->sysMarginBottom + musx::dom::EVPU_PER_STANDARD_STAFF);
    score.defaults.systemLayout.systemDistance =
        context.musicXmlTenthsFromEvpu(systemDistance, systemScaleBackout);
    if (const auto firstSystem = context.document->getOthers()->get<others::StaffSystem>(context.forPartId, 1)) {
        const auto topSystemDistance = -firstSystem->top - firstSystem->distanceToPrev;
        score.defaults.systemLayout.topSystemDistance =
            context.musicXmlTenthsFromEvpu(topSystemDistance, systemScaleBackout);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
