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

mx::api::MarginsData createPageMargins(
    const MusicXmlMusxMapping& context,
    double leftEvpu,
    double rightEvpu,
    double topEvpu,
    double bottomEvpu,
    double combinedSystemScaling)
{
    return mx::api::MarginsData{
        context.musicXmlTenthsFromEvpu(leftEvpu, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(rightEvpu, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(topEvpu, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(bottomEvpu, combinedSystemScaling)
    };
}

} // namespace

void createGlobalData(const MusicXmlMusxMapping& context)
{
    auto& score = *context.musicXmlScore;
    const auto& pagePrefs = context.finaleOptions.effectivePageFormat;
    score.ticksPerQuarter = context.timing.divisions;

    constexpr auto kUnscaledMmPerStaff = EVPU_PER_STANDARD_STAFF / EVPU_PER_MM;
    const auto combinedSystemScaling = pagePrefs->calcCombinedSystemScaling().toDouble();
    score.defaults.scalingMillimeters = combinedSystemScaling * kUnscaledMmPerStaff;
    score.defaults.scalingTenths = MUSICXML_DEFAULT_TENTHS_PER_STAFF; // standard value for many MusicXML exporters, including Finale

    auto& pageLayout = score.defaults.pageLayout;
    pageLayout.size = mx::api::SizeData{
        context.musicXmlTenthsFromEvpu(pagePrefs->pageHeight, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(pagePrefs->pageWidth, combinedSystemScaling)
    };

    const auto evenMargins = createPageMargins(
        context,
        pagePrefs->leftPageMarginLeft,
        -pagePrefs->leftPageMarginRight,
        -pagePrefs->leftPageMarginTop,
        pagePrefs->leftPageMarginBottom,
        combinedSystemScaling);
    pageLayout.margins.even = evenMargins;
    pageLayout.margins.odd = pagePrefs->facingPages
        ? createPageMargins(
              context,
              pagePrefs->rightPageMarginLeft,
              -pagePrefs->rightPageMarginRight,
              -pagePrefs->rightPageMarginTop,
              pagePrefs->rightPageMarginBottom,
              combinedSystemScaling)
        : evenMargins;

    score.defaults.systemLayout.margins = mx::api::LeftRight{
        context.musicXmlTenthsFromEvpu(pagePrefs->sysMarginLeft, combinedSystemScaling),
        context.musicXmlTenthsFromEvpu(-pagePrefs->sysMarginRight, combinedSystemScaling)
    };
    const auto systemDistance =
        -pagePrefs->sysMarginTop
        - pagePrefs->sysDistanceBetween
        - (pagePrefs->sysMarginBottom + musx::dom::EVPU_PER_STANDARD_STAFF);
    score.defaults.systemLayout.systemDistance =
        context.musicXmlTenthsFromEvpu(systemDistance, combinedSystemScaling);
    if (const auto firstSystem = context.document->getOthers()->get<others::StaffSystem>(context.forPartId, 1)) {
        const auto topSystemDistance = -firstSystem->top - firstSystem->distanceToPrev;
        score.defaults.systemLayout.topSystemDistance =
            context.musicXmlTenthsFromEvpu(topSystemDistance, combinedSystemScaling);
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
