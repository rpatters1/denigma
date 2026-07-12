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

#include <algorithm>
#include <array>
#include <string_view>

#include "utils/font_names.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

struct MusicFontFallbackMapping
{
    std::string_view name;
    MusicXmlFontFamilyFallback fallback;
};

MusicXmlFontFamilyFallback musicFontFallbackFromFontInfo(const FontInfo& fontInfo)
{
    static constexpr auto knownMusicFonts = std::to_array<MusicFontFallbackMapping>({
        { "bravura", MusicXmlFontFamilyFallback::Engraved },
        { "broadwaycopyist", MusicXmlFontFamilyFallback::Handwritten },
        { "broadwaycopyistperc", MusicXmlFontFamilyFallback::Handwritten },
        { "finalebroadway", MusicXmlFontFamilyFallback::Handwritten },
        { "finaleengraver", MusicXmlFontFamilyFallback::Engraved },
        { "finalejazz", MusicXmlFontFamilyFallback::Handwritten },
        { "finalemaestro", MusicXmlFontFamilyFallback::Engraved },
        { "engraver", MusicXmlFontFamilyFallback::Engraved },
        { "engraverfontset", MusicXmlFontFamilyFallback::Engraved },
        { "jazz", MusicXmlFontFamilyFallback::Handwritten },
        { "jazzperc", MusicXmlFontFamilyFallback::Handwritten },
        { "leland", MusicXmlFontFamilyFallback::Engraved },
        { "maestro", MusicXmlFontFamilyFallback::Engraved },
        { "maestropercussion", MusicXmlFontFamilyFallback::Engraved },
        { "musejazz", MusicXmlFontFamilyFallback::Handwritten },
        { "opus", MusicXmlFontFamilyFallback::Engraved },
        { "petaluma", MusicXmlFontFamilyFallback::Handwritten },
        { "petrucci", MusicXmlFontFamilyFallback::Engraved },
        { "pmusic", MusicXmlFontFamilyFallback::Engraved },
        { "sonata", MusicXmlFontFamilyFallback::Engraved },
    });

    const auto fontName = utils::normalizedFontName(fontInfo.getName());
    const auto it = std::find_if(knownMusicFonts.begin(), knownMusicFonts.end(), [&](const auto& item) {
        return item.name == std::string_view(fontName.data(), fontName.size());
    });
    return it != knownMusicFonts.end() ? it->fallback : MusicXmlFontFamilyFallback::Music;
}

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

void createSystemLayoutData(
    const MusicXmlMusxMapping& context,
    const options::PageFormatOptions::PageFormat& pagePrefs)
{
    auto& systemLayout = context.musicXmlScore->defaults.systemLayout;
    const auto systemScaleBackout = pagePrefs.calcSystemScaling().toDouble();
    systemLayout.margins = mx::api::LeftRight{
        context.musicXmlTenthsFromEvpu(pagePrefs.sysMarginLeft, systemScaleBackout),
        context.musicXmlTenthsFromEvpu(-pagePrefs.sysMarginRight, systemScaleBackout)
    };
    const auto systemDistance =
        -pagePrefs.sysMarginTop
        - pagePrefs.sysDistanceBetween
        - (pagePrefs.sysMarginBottom + EVPU_PER_STANDARD_STAFF);
    systemLayout.systemDistance =
        context.musicXmlTenthsFromEvpu(systemDistance, systemScaleBackout);
    if (const auto firstSystem = context.document->getOthers()->get<others::StaffSystem>(context.forPartId, 1)) {
        const auto topSystemDistance = -firstSystem->top - firstSystem->distanceToPrev;
        systemLayout.topSystemDistance =
            context.musicXmlTenthsFromEvpu(topSystemDistance, systemScaleBackout);
    }
}

void createAppearance(const MusicXmlMusxMapping& context)
{
    auto& appearance = context.musicXmlScore->defaults.appearance;
    appearance.clear();
    const auto addAppearance = [&](mx::api::AppearanceType type, const std::string& subType, double value) {
        mx::api::AppearanceData data;
        data.appearanceType = type;
        data.appearanceSubType = subType;
        data.value = value;
        appearance.emplace_back(std::move(data));
    };
    const auto addLineWidth = [&](const std::string& subType, double efix) {
        addAppearance(mx::api::AppearanceType::LineWidth, subType, context.musicXmlTenthsFromEvpu(efix / EFIX_PER_EVPU));
    };
    const auto addDistance = [&](const std::string& subType, double evpu) {
        addAppearance(mx::api::AppearanceType::Distance, subType, context.musicXmlTenthsFromEvpu(evpu));
    };
    const auto addNoteSize = [&](const std::string& subType, double percent) {
        addAppearance(mx::api::AppearanceType::NoteSize, subType, percent);
    };

    const auto& options = context.finaleOptions;
    addLineWidth("stem", options.stemOptions->stemWidth);
    addLineWidth("beam", options.beamOptions->beamWidth);
    addLineWidth("staff", options.lineCurveOptions->staffLineWidth);
    addLineWidth("light barline", options.barlineOptions->barlineWidth);
    addLineWidth("heavy barline", options.barlineOptions->thickBarlineWidth);
    addLineWidth("leger", options.lineCurveOptions->legerLineWidth);
    addLineWidth("bracket", options.repeatOptions->bracketLineWidth);
    addLineWidth("ending", options.repeatOptions->bracketLineWidth);
    addLineWidth("dashes", options.smartShapeOptions->smartLineWidth);
    addLineWidth("extend", options.smartShapeOptions->smartLineWidth);
    addLineWidth("octave shift", options.smartShapeOptions->smartLineWidth);
    addLineWidth("pedal", options.smartShapeOptions->smartLineWidth);
    addLineWidth("wedge", options.smartShapeOptions->smartLineWidth);
    addLineWidth("enclosure", options.lineCurveOptions->enclosureWidth);
    addLineWidth("tuplet bracket", options.tupletOptions->tupLineWidth);
    addNoteSize("grace", options.graceOptions->gracePerc);
    addNoteSize("cue", options.graceOptions->gracePerc);
    addDistance("hyphen", options.lyricOptions->maxHyphenSeparation);
    addDistance("beam", options.beamOptions->beamSepar);
}

void createFontData(const MusicXmlMusxMapping& context)
{
    auto& defaults = context.musicXmlScore->defaults;
    defaults.musicFont = context.musicXmlFontDataFromFontInfo(
        *context.finaleOptions.defaultMusicFont,
        musicFontFallbackFromFontInfo(*context.finaleOptions.defaultMusicFont));

    if (const auto wordFont = context.finaleOptions.fontOptions->getFontInfo(options::FontOptions::FontType::Expression)) {
        defaults.wordFont = context.musicXmlFontDataFromFontInfo(*wordFont, MusicXmlFontFamilyFallback::Text);
    }

    const auto addFirstLyricFont = [&](const auto& lyricTexts) {
        for (const auto& lyricText : lyricTexts) {
            ASSERT_IF(!lyricText) {
                continue;
            }
            if (const auto fontInfo = lyricText->getRawTextCtx(lyricText, context.forPartId).parseFirstFontInfo()) {
                mx::api::LyricFontData lyricFont;
                lyricFont.font = context.musicXmlFontDataFromFontInfo(*fontInfo, MusicXmlFontFamilyFallback::Text);
                defaults.lyricFonts.emplace_back(std::move(lyricFont));
                return true;
            }
        }
        return false;
    };
    bool foundLyricFont = addFirstLyricFont(context.document->getTexts()->getArray<texts::LyricsVerse>());
    if (!foundLyricFont) {
        foundLyricFont = addFirstLyricFont(context.document->getTexts()->getArray<texts::LyricsChorus>());
    }
    if (!foundLyricFont) {
        addFirstLyricFont(context.document->getTexts()->getArray<texts::LyricsSection>());
    }
}

void createSystemBreaks(const MusicXmlMusxMapping& context)
{
    const auto measures = context.document->getOthers()->getArray<others::Measure>(context.forPartId);
    const auto setSystemBreak = [&](MeasCmper measureId) {
        const auto it = std::find_if(measures.begin(), measures.end(), [&](const auto& measure) {
            return measure->getCmper() == measureId;
        });
        if (it == measures.end()) {
            return;
        }
        const auto measureIndex = static_cast<mx::api::MeasureIndex>(std::distance(measures.begin(), it));
        if (measureIndex > 0) {
            context.musicXmlScore->layout[measureIndex].system.newSystem = mx::api::Bool::yes;
        }
    };

    for (const auto& measure : measures) {
        if (measure->beginNewSystem) {
            setSystemBreak(measure->getCmper());
        }
    }
    for (const auto& lock : context.document->getOthers()->getArray<others::SystemLock>(context.forPartId)) {
        // Both boundaries are needed when neighboring systems are unlocked.
        // MusicXML breaks are measure-scoped, so a Finale lock ending at a split measure cannot be preserved exactly.
        setSystemBreak(lock->getCmper());
        setSystemBreak(lock->endMeas);
    }
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
    createSystemLayoutData(context, *pagePrefs);
    createSystemBreaks(context);
    createAppearance(context);
    createFontData(context);
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
