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
#include <set>

#include "denigma.h"

#include "mss.h"
#include "musx/musx.h"
#include "pugixml.hpp"
#include "utils/stringutils.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace mss {

using namespace ::musx::dom;

using XmlDocument = ::pugi::xml_document;
using XmlElement = ::pugi::xml_node;
using XmlAttribute = ::pugi::xml_attribute;

constexpr static char MSS_VERSION[] = "4.50"; // Do not change this version without checking notes on changed values.
constexpr static double MUSE_FINALE_SCALE_DIFFERENTIAL = 20.0 / 24.0;

// Finale preferences:
struct FinalePreferences
{
    FinalePreferences(const DenigmaContext& context)
        : denigmaContext(&context) {}

    const DenigmaContext* denigmaContext;
    DocumentPtr document;
    Cmper forPartId{};
    //
    std::shared_ptr<FontInfo> defaultMusicFont;
    std::string musicFontName;
    //
    std::shared_ptr<options::AccidentalOptions> accidentalOptions;
    std::shared_ptr<options::AlternateNotationOptions> alternateNotationOptions;
    std::shared_ptr<options::AugmentationDotOptions> augDotOptions;
    std::shared_ptr<options::BarlineOptions> barlineOptions;
    std::shared_ptr<options::BeamOptions> beamOptions;
    std::shared_ptr<options::ClefOptions> clefOptions;
    std::shared_ptr<options::FlagOptions> flagOptions;
    std::shared_ptr<options::GraceNoteOptions> graceOptions;
    std::shared_ptr<options::KeySignatureOptions> keyOptions;
    std::shared_ptr<options::LineCurveOptions> lineCurveOptions;
    std::shared_ptr<options::MiscOptions> miscOptions;
    std::shared_ptr<options::MultimeasureRestOptions> mmRestOptions;
    std::shared_ptr<options::MusicSpacingOptions> musicSpacing;
    std::shared_ptr<options::PageFormatOptions::PageFormat> pageFormat;
    std::shared_ptr<options::PianoBraceBracketOptions> braceOptions;
    std::shared_ptr<options::RepeatOptions> repeatOptions;
    std::shared_ptr<options::SmartShapeOptions> smartShapeOptions;
    std::shared_ptr<options::StaffOptions> staffOptions;
    std::shared_ptr<options::StemOptions> stemOptions;
    std::shared_ptr<options::TieOptions> tieOptions;
    std::shared_ptr<options::TimeSignatureOptions> timeOptions;
    std::shared_ptr<options::TupletOptions> tupletOptions;
    //
    std::shared_ptr<others::LayerAttributes> layerOneAttributes;
    std::shared_ptr<others::MeasureNumberRegion::ScorePartData> measNumScorePart;
    std::shared_ptr<others::PartGlobals> partGlobals;
};
using FinalePreferencesPtr = std::shared_ptr<FinalePreferences>;

template <typename T>
static std::shared_ptr<T> getDocOptions(const FinalePreferencesPtr& prefs, const std::string& prefsName)
{
    auto retval = prefs->document->getOptions()->get<T>();
    if (!retval) {
        throw std::invalid_argument("document contains no default " + prefsName + " denigmaContext");
    }
    return retval;
}

static FinalePreferencesPtr getCurrentPrefs(const DocumentPtr& document, Cmper forPartId, const DenigmaContext& denigmaContext)
{
    auto retval = std::make_shared<FinalePreferences>(denigmaContext);
    retval->document = document;
    retval->forPartId = forPartId;

    auto fontOptions = getDocOptions<options::FontOptions>(retval, "font");
    retval->defaultMusicFont = fontOptions->getFontInfo(options::FontOptions::FontType::Music);
    if (!retval->defaultMusicFont) {
        throw std::invalid_argument("document contains no information for the default music font.");
    }
    retval->musicFontName = [&]() -> std::string {
        std::string fontName = retval->defaultMusicFont->getName();
        if (retval->defaultMusicFont->calcIsSMuFL()) {
            return fontName;
        } else if (fontName == "Broadway Copyist") {
            return "Finale Broadway";
        } else if (fontName == "Engraver") {
            return "Finale Engraver";
        } else if (fontName == "Jazz") {
            return "Finale Jazz";
        } else if (fontName == "Maestro" || fontName == "Pmusic" || fontName == "Sonata") {
            return "Finale Maestro";
        } else if (fontName == "Petrucci") {
            return "Finale Legacy";
        } // other `else if` checks as required go here
        return {};
    }();
    //
    retval->accidentalOptions = getDocOptions<options::AccidentalOptions>(retval, "accidental");
    retval->alternateNotationOptions = getDocOptions<options::AlternateNotationOptions>(retval, "alternate notation");
    retval->augDotOptions = getDocOptions<options::AugmentationDotOptions>(retval, "augmentation dot");
    retval->barlineOptions = getDocOptions<options::BarlineOptions>(retval, "barline");
    retval->beamOptions = getDocOptions<options::BeamOptions>(retval, "beam");
    retval->clefOptions = getDocOptions<options::ClefOptions>(retval, "clef");
    retval->flagOptions = getDocOptions<options::FlagOptions>(retval, "flag");
    retval->graceOptions = getDocOptions<options::GraceNoteOptions>(retval, "grace note");
    retval->keyOptions = getDocOptions<options::KeySignatureOptions>(retval, "key signature");
    retval->lineCurveOptions = getDocOptions<options::LineCurveOptions>(retval, "lines & curves");
    retval->miscOptions = getDocOptions<options::MiscOptions>(retval, "miscellaneous");
    retval->mmRestOptions = getDocOptions<options::MultimeasureRestOptions>(retval, "multimeasure rest");
    retval->musicSpacing = getDocOptions<options::MusicSpacingOptions>(retval, "music spacing");
    auto pageFormatOptions = getDocOptions<options::PageFormatOptions>(retval, "page format");
    retval->pageFormat = pageFormatOptions->calcPageFormatForPart(forPartId);
    retval->braceOptions = getDocOptions<options::PianoBraceBracketOptions>(retval, "piano braces & brackets");
    retval->repeatOptions = getDocOptions<options::RepeatOptions>(retval, "repeat");
    retval->smartShapeOptions = getDocOptions<options::SmartShapeOptions>(retval, "smart shape");
    retval->staffOptions = getDocOptions<options::StaffOptions>(retval, "staff");
    retval->stemOptions = getDocOptions<options::StemOptions>(retval, "stem");
    retval->tieOptions = getDocOptions<options::TieOptions>(retval, "tie");
    retval->timeOptions = getDocOptions<options::TimeSignatureOptions>(retval, "time signature");
    retval->tupletOptions = getDocOptions<options::TupletOptions>(retval, "tuplet");
    //
    retval->layerOneAttributes = retval->document->getOthers()->get<others::LayerAttributes>(forPartId, 0);
    if (!retval->layerOneAttributes) {
        throw std::invalid_argument("document contains no options for Layer 1");
    }
    auto measNumRegions = retval->document->getOthers()->getArray<others::MeasureNumberRegion>(forPartId);
    if (measNumRegions.size() > 0) {
        retval->measNumScorePart = (forPartId && measNumRegions[0]->useScoreInfoForPart && measNumRegions[0]->partData)
                                 ? measNumRegions[0]->partData
                                 : measNumRegions[0]->scoreData;
        if (!retval->measNumScorePart) {
            throw std::invalid_argument("document contains no ScorePartData for measure number region " + std::to_string(measNumRegions[0]->getCmper()));
        }
    }
    retval->partGlobals = retval->document->getOthers()->get<others::PartGlobals>(forPartId, MUSX_GLOBALS_CMPER);
    if (!retval->layerOneAttributes) {
        throw std::invalid_argument("document contains no options for Layer 1");
    }

    return retval;
}

template<typename T>
static XmlElement setElementValue(XmlElement& styleElement, const std::string& nodeName, const T& value)
{
    if (!styleElement) {
        throw std::invalid_argument("styleElement cannot be null");
    }

    XmlElement element = styleElement.child(nodeName.c_str());
    if (!element) {
        element = styleElement.append_child(nodeName.c_str());
    }

    if constexpr (std::is_same_v<T, std::nullptr_t>) {
        static_assert(std::is_same_v<T, std::nullptr_t>, "Incorrect property.");
    } else if constexpr (std::is_floating_point_v<T>) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%.5g", value);
        element.text().set(buffer);
    } else if constexpr (std::is_same_v<T, std::string>) {
        element.text().set(value.c_str());
    } else if constexpr (std::is_same_v<T, bool>) {
        element.text().set(static_cast<int>(value)); // MuseScore .mss files use 0 and 1 for booleans
    } else {
        element.text().set(value);
    }

    return element;
}

static uint16_t museFontEfx(const FontInfo* fontInfo)
{
    uint16_t retval = 0;

    if (fontInfo->bold) { retval |= 0x01; }
    if (fontInfo->italic) { retval |= 0x02; }
    if (fontInfo->underline) { retval |= 0x04; }
    if (fontInfo->strikeout) { retval |= 0x08; }

    return retval;
}

static double museMagVal(const FinalePreferencesPtr& prefs, const options::FontOptions::FontType type)
{
    auto fontPrefs = options::FontOptions::getFontInfo(prefs->document, type);
    if (fontPrefs->getName() == prefs->defaultMusicFont->getName()) {
        return double(fontPrefs->fontSize) / double(prefs->defaultMusicFont->fontSize);
    }
    return 1.0;
}

static void writeFontPref(XmlElement& styleElement, const std::string& namePrefix, const FontInfo* fontInfo)
{
    setElementValue(styleElement, namePrefix + "FontFace", fontInfo->getName());
    setElementValue(styleElement, namePrefix + "FontSize", 
                    double(fontInfo->fontSize) * (fontInfo->absolute ? 1.0 : MUSE_FINALE_SCALE_DIFFERENTIAL));
    setElementValue(styleElement, namePrefix + "FontSpatiumDependent", !fontInfo->absolute);
    setElementValue(styleElement, namePrefix + "FontStyle", museFontEfx(fontInfo));
}

static void writeDefaultFontPref(XmlElement& styleElement, const FinalePreferencesPtr& prefs, const std::string& namePrefix, options::FontOptions::FontType type)
{
    if (auto fontPrefs = options::FontOptions::getFontInfo(prefs->document, type)) {
        writeFontPref(styleElement, namePrefix, fontPrefs.get());
    } else {
        prefs->denigmaContext->logMessage(LogMsg() << "unable to load default font pref for " << int(type), LogSeverity::Warning);
    }
}

void writeLinePrefs(XmlElement& styleElement,
                    const std::string& namePrefix, 
                    double widthEfix, 
                    double dashLength, 
                    double dashGap, 
                    const std::string& styleString = {})
{
    const double lineWidthEvpu = widthEfix / EFIX_PER_EVPU;
    setElementValue(styleElement, namePrefix + "LineWidth", widthEfix / EFIX_PER_SPACE);
    if (!styleString.empty()) {
        setElementValue(styleElement, namePrefix + "LineStyle", styleString);
    }
    setElementValue(styleElement, namePrefix + "DashLineLen", dashLength / lineWidthEvpu);
    setElementValue(styleElement, namePrefix + "DashGapLen", dashGap / lineWidthEvpu);
}

static void writeFramePrefs(XmlElement& styleElement, const std::string& namePrefix, const others::Enclosure* enclosure = nullptr)
{
    if (!enclosure || enclosure->shape == others::Enclosure::Shape::NoEnclosure || enclosure->lineWidth == 0) {
        setElementValue(styleElement, namePrefix + "FrameType", 0);
        if (!enclosure) return; // Do not override any other defaults if no enclosure shape
    } else if (enclosure->shape == others::Enclosure::Shape::Ellipse) {
        setElementValue(styleElement, namePrefix + "FrameType", 2);
    } else {
        setElementValue(styleElement, namePrefix + "FrameType", 1);
    }
    setElementValue(styleElement, namePrefix + "FramePadding", enclosure->xMargin / EVPU_PER_SPACE);
    setElementValue(styleElement, namePrefix + "FrameWidth", enclosure->lineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, namePrefix + "FrameRound", 
                    enclosure->roundCorners ? int(lround(enclosure->cornerRadius / EFIX_PER_EVPU)) : 0);
}

static void writeCategoryTextFontPref(XmlElement& styleElement, const FinalePreferencesPtr& prefs, const std::string& namePrefix, others::MarkingCategory::CategoryType categoryType)
{
    auto cat = prefs->document->getOthers()->get<others::MarkingCategory>(prefs->forPartId, Cmper(categoryType));
    if (!cat) {
        prefs->denigmaContext->logMessage(LogMsg() << "unable to load category def for " << namePrefix, LogSeverity::Warning);
        return;
    }
    if (!cat->textFont) {
        prefs->denigmaContext->logMessage(LogMsg() << "marking category " << cat->getName() << " has no text font.", LogSeverity::Warning);
        return;
    }
    writeFontPref(styleElement, namePrefix, cat->textFont.get());
    for (auto& it : cat->textExpressions) {
        if (auto exp = it.second.lock()) {
            writeFramePrefs(styleElement, namePrefix, exp->getEnclosure().get());
            break;
        } else {
            prefs->denigmaContext->logMessage(LogMsg() << "marking category " << cat->getName() << " has invalid text expression.", LogSeverity::Warning);
        }
    }
}

static void writePagePrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    auto pagePrefs = prefs->pageFormat;

    // Set XML element values
    setElementValue(styleElement, "pageWidth", double(pagePrefs->pageWidth) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageHeight", double(pagePrefs->pageHeight) / EVPU_PER_INCH);
    setElementValue(styleElement, "pagePrintableWidth",
                    double(pagePrefs->pageWidth - pagePrefs->leftPageMarginLeft + pagePrefs->leftPageMarginRight) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageEvenLeftMargin", pagePrefs->leftPageMarginLeft / EVPU_PER_INCH);
    setElementValue(styleElement, "pageOddLeftMargin",
                    double(pagePrefs->facingPages ? pagePrefs->rightPageMarginLeft : pagePrefs->leftPageMarginLeft) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageEvenTopMargin", double(-pagePrefs->leftPageMarginTop) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageEvenBottomMargin", double(pagePrefs->leftPageMarginBottom) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageOddTopMargin",
                    double(pagePrefs->facingPages ? -pagePrefs->rightPageMarginTop : -pagePrefs->leftPageMarginTop) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageOddBottomMargin",
                    double(pagePrefs->facingPages ? pagePrefs->rightPageMarginBottom : pagePrefs->leftPageMarginBottom) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageTwosided", pagePrefs->facingPages);
    setElementValue(styleElement, "enableIndentationOnFirstSystem", pagePrefs->differentFirstSysMargin);
    setElementValue(styleElement, "firstSystemIndentationValue", double(pagePrefs->firstSysMarginLeft) / EVPU_PER_SPACE);

    // Calculate Spatium
    setElementValue(styleElement, "spatium", (EVPU_PER_SPACE * prefs->pageFormat->calcCombinedSystemScaling().toDouble()) / EVPU_PER_MM);

    // Calculate small staff size and small note size from first system, if any is there
    if (const auto& firstSystem = prefs->document->getOthers()->get<others::StaffSystem>(prefs->forPartId, 1)) {
        auto [minSize, maxSize] = firstSystem->calcMinMaxStaffSizes();
        if (minSize < 1) {
            setElementValue(styleElement, "smallStaffMag", minSize.toDouble());
            setElementValue(styleElement, "smallNoteMag", minSize.toDouble());
        }
    }
    if (!prefs->musicFontName.empty()) {
        setElementValue(styleElement, "musicalSymbolFont", prefs->musicFontName);
        setElementValue(styleElement, "musicalTextFont", prefs->musicFontName + " Text");
    }
}

static void writeLyricsPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    auto fontInfo = options::FontOptions::getFontInfo(prefs->document, options::FontOptions::FontType::LyricVerse);
    for (auto [verseNumber, evenOdd] : {
            std::make_pair(1, "Odd"),
            std::make_pair(2, "Even")
        }) {
        auto verseText = prefs->document->getTexts()->get<texts::LyricsVerse>(Cmper(verseNumber));
        if (verseText && !verseText->text.empty()) {
            auto font = verseText->getRawTextCtx(prefs->forPartId).parseFirstFontInfo();
            if (font) {
                fontInfo = font;
            }
        }
        writeFontPref(styleElement, "lyrics" + std::string(evenOdd), fontInfo.get());
    }
}

void writeLineMeasurePrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using RepeatWingStyle = options::RepeatOptions::WingStyle;

    setElementValue(styleElement, "barWidth", prefs->barlineOptions->barlineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "doubleBarWidth", prefs->barlineOptions->barlineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "endBarWidth", prefs->barlineOptions->thickBarlineWidth / EFIX_PER_SPACE);

    // Finale's double bar distance is measured from the beginning of the thin line
    setElementValue(styleElement, "doubleBarDistance", 
        (prefs->barlineOptions->doubleBarlineSpace - prefs->barlineOptions->barlineWidth) / EFIX_PER_SPACE);

    // Finale's final bar distance is the separation amount
    setElementValue(styleElement, "endBarDistance", prefs->barlineOptions->finalBarlineSpace / EFIX_PER_SPACE);
    setElementValue(styleElement, "repeatBarlineDotSeparation", prefs->repeatOptions->forwardDotHPos / EVPU_PER_SPACE);
    setElementValue(styleElement, "repeatBarTips", prefs->repeatOptions->wingStyle != RepeatWingStyle::None);

    setElementValue(styleElement, "startBarlineSingle", prefs->barlineOptions->drawLeftBarlineSingleStaff);
    setElementValue(styleElement, "startBarlineMultiple", prefs->barlineOptions->drawLeftBarlineMultipleStaves);

    setElementValue(styleElement, "bracketWidth", 0.5); // Hard-coded in Finale
    setElementValue(styleElement, "bracketDistance", ((-prefs->braceOptions->defBracketPos) - 0.25 * EVPU_PER_SPACE) / EVPU_PER_SPACE); // Finale subtracts half the bracket width on layout (observed).
    setElementValue(styleElement, "akkoladeBarDistance", -prefs->braceOptions->defBracketPos / EVPU_PER_SPACE);

    setElementValue(styleElement, "clefLeftMargin", prefs->clefOptions->clefFrontSepar / EVPU_PER_SPACE);
    setElementValue(styleElement, "keysigLeftMargin", prefs->keyOptions->keyFront / EVPU_PER_SPACE);

    const double timeSigSpaceBefore = prefs->forPartId
                                      ? prefs->timeOptions->timeFrontParts
                                      : prefs->timeOptions->timeFront;
    setElementValue(styleElement, "timesigLeftMargin", timeSigSpaceBefore / EVPU_PER_SPACE);

    setElementValue(styleElement, "clefKeyDistance", 
        (prefs->clefOptions->clefBackSepar + prefs->clefOptions->clefKeySepar + prefs->keyOptions->keyFront) / EVPU_PER_SPACE);
    setElementValue(styleElement, "clefTimesigDistance", 
        (prefs->clefOptions->clefBackSepar + prefs->clefOptions->clefTimeSepar + timeSigSpaceBefore) / EVPU_PER_SPACE);
    setElementValue(styleElement, "keyTimesigDistance", 
        (prefs->keyOptions->keyBack + prefs->keyOptions->keyTimeSepar + timeSigSpaceBefore) / EVPU_PER_SPACE);
    setElementValue(styleElement, "keyBarlineDistance", prefs->repeatOptions->afterKeySpace / EVPU_PER_SPACE);

    // Differences in how MuseScore and Finale interpret these settings mean the following are better left alone
    // setElementValue(styleElement, "systemHeaderDistance", prefs->keyOptions->keyBack / EVPU_PER_SPACE);
    // setElementValue(styleElement, "systemHeaderTimeSigDistance", prefs->timeOptions->timeBack / EVPU_PER_SPACE);

    setElementValue(styleElement, "clefBarlineDistance", -(prefs->clefOptions->clefChangeOffset) / EVPU_PER_SPACE);
    setElementValue(styleElement, "timesigBarlineDistance", prefs->repeatOptions->afterClefSpace / EVPU_PER_SPACE);

    setElementValue(styleElement, "measureRepeatNumberPos", -(prefs->alternateNotationOptions->twoMeasNumLift + 0.5) / EVPU_PER_SPACE);
    setElementValue(styleElement, "staffLineWidth", prefs->lineCurveOptions->staffLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "ledgerLineWidth", prefs->lineCurveOptions->legerLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "ledgerLineLength", 
        (prefs->lineCurveOptions->legerFrontLength + prefs->lineCurveOptions->legerBackLength) / (2 * EVPU_PER_SPACE));
    setElementValue(styleElement, "keysigAccidentalDistance", (prefs->keyOptions->acciAdd + 4) / EVPU_PER_SPACE); // Observed fudge factor
    setElementValue(styleElement, "keysigNaturalDistance", (prefs->keyOptions->acciAdd + 6) / EVPU_PER_SPACE); // Observed fudge factor

    setElementValue(styleElement, "smallClefMag", prefs->clefOptions->clefChangePercent / 100.0);
    setElementValue(styleElement, "genClef", !prefs->clefOptions->showClefFirstSystemOnly);
    setElementValue(styleElement, "genKeysig", !prefs->keyOptions->showKeyFirstSystemOnly);
    setElementValue(styleElement, "genCourtesyTimesig", prefs->timeOptions->cautionaryTimeChanges);
    setElementValue(styleElement, "genCourtesyKeysig", prefs->keyOptions->cautionaryKeyChanges);
    setElementValue(styleElement, "genCourtesyClef", prefs->clefOptions->cautionaryClefChanges);

    setElementValue(styleElement, "keySigCourtesyBarlineMode", prefs->barlineOptions->drawDoubleBarlineBeforeKeyChanges);
    setElementValue(styleElement, "timeSigCourtesyBarlineMode", 0); // Hard-coded as 0 in Lua
    setElementValue(styleElement, "hideEmptyStaves", prefs->document->calcHasVaryingSystemStaves(prefs->forPartId));
}

void writeStemPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "useStraightNoteFlags", prefs->flagOptions->straightFlags);
    setElementValue(styleElement, "stemWidth", prefs->stemOptions->stemWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "shortenStem", true);
    setElementValue(styleElement, "stemLength", prefs->stemOptions->stemLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "shortestStem", prefs->stemOptions->shortStemLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "stemSlashThickness", prefs->graceOptions->graceSlashWidth / EFIX_PER_SPACE);
}

void writeMusicSpacingPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "minMeasureWidth", prefs->musicSpacing->minWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "minNoteDistance", prefs->musicSpacing->minDistance / EVPU_PER_SPACE);
    setElementValue(styleElement, "measureSpacing", prefs->musicSpacing->scalingFactor);
    setElementValue(styleElement, "minTieLength", prefs->musicSpacing->minDistTiedNotes / EVPU_PER_SPACE);
}

void writeNoteRelatedPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "accidentalDistance", prefs->accidentalOptions->acciAcciSpace / EVPU_PER_SPACE);
    setElementValue(styleElement, "accidentalNoteDistance", prefs->accidentalOptions->acciNoteSpace / EVPU_PER_SPACE);
    setElementValue(styleElement, "beamWidth", prefs->beamOptions->beamWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "useWideBeams", prefs->beamOptions->beamSepar > (0.75 * EVPU_PER_SPACE));
    // Finale randomly adds twice the stem width to the length of a beam stub. (Observed behavior)
    setElementValue(styleElement, "beamMinLen", (prefs->beamOptions->beamStubLength + (2.0 * prefs->stemOptions->stemWidth / EFIX_PER_EVPU)) / EVPU_PER_SPACE);
    setElementValue(styleElement, "beamNoSlope", prefs->beamOptions->beamingStyle == options::BeamOptions::FlattenStyle::AlwaysFlat);
    double dotMag = museMagVal(prefs, options::FontOptions::FontType::AugDots);
    setElementValue(styleElement, "dotMag", dotMag);
    setElementValue(styleElement, "dotNoteDistance", prefs->augDotOptions->dotNoteOffset / EVPU_PER_SPACE);
    setElementValue(styleElement, "dotRestDistance", prefs->augDotOptions->dotNoteOffset / EVPU_PER_SPACE); // Same value as dotNoteDistance
    // Finale's dotOffset value is calculated relative to the rightmost point of the previous dot, MuseScore's dotDotDistance the leftmost. (Observed behavior)
    // Therefore, we need to add the dot width to MuseScore's dotDotDistance.
    auto dotWidth = [&]() -> std::optional<EvpuFloat> {
        if (!prefs->musicFontName.empty()) {
            if (auto musicFontWidth = utils::smuflGlyphWidthForFont(prefs->musicFontName, "augmentationDot")) {
                return musicFontWidth;
            } else if (auto maestroFontWidth = utils::smuflGlyphWidthForFont("Finale Maestro", "augmentationDot")) {
                prefs->denigmaContext->logMessage(LogMsg() << "unable to find augmentation dot width for " << prefs->musicFontName
                    << ". Using Finale Maestro instead.", LogSeverity::Warning);
                return maestroFontWidth;
            }
        }
        return std::nullopt;
    }();
    if (dotWidth) {
        setElementValue(styleElement, "dotDotDistance", (prefs->augDotOptions->dotOffset + (dotMag * dotWidth.value())) / EVPU_PER_SPACE);
    } else {
        prefs->denigmaContext->logMessage(LogMsg() << "Unable to find augmentation dot width for music font [" << prefs->musicFontName
            << "]. Dot-to-dot distance setting was skipped.", LogSeverity::Warning);
    }
    setElementValue(styleElement, "articulationMag", museMagVal(prefs, options::FontOptions::FontType::Articulation));
    setElementValue(styleElement, "graceNoteMag", prefs->graceOptions->gracePerc / 100.0);
    setElementValue(styleElement, "concertPitch", !prefs->partGlobals->showTransposed);
    setElementValue(styleElement, "multiVoiceRestTwoSpaceOffset", std::labs(prefs->layerOneAttributes->restOffset) >= 4);
    setElementValue(styleElement, "mergeMatchingRests", prefs->miscOptions->consolidateRestsAcrossLayers);
}

void writeSmartShapePrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    // Hairpin-related settings
    setElementValue(styleElement, "hairpinHeight", prefs->smartShapeOptions->shortHairpinOpeningWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "hairpinContHeight", 0.5); // Hardcoded to a half space
    writeCategoryTextFontPref(styleElement, prefs, "hairpin", others::MarkingCategory::CategoryType::Dynamics);
    writeLinePrefs(styleElement, "hairpin", prefs->smartShapeOptions->crescLineWidth, prefs->smartShapeOptions->smartDashOn, prefs->smartShapeOptions->smartDashOff);

    // Slur-related settings
    setElementValue(styleElement, "slurEndWidth", prefs->smartShapeOptions->smartSlurTipWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "slurDottedWidth", prefs->smartShapeOptions->smartLineWidth / EFIX_PER_SPACE);

    // Tie-related settings
    setElementValue(styleElement, "tieEndWidth", prefs->tieOptions->tieTipWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "tieDottedWidth", prefs->smartShapeOptions->smartLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "tiePlacementSingleNote", prefs->tieOptions->useOuterPlacement ? "outside" : "inside");
    setElementValue(styleElement, "tiePlacementChord", prefs->tieOptions->useOuterPlacement ? "outside" : "inside");

    // Ottava settings
    setElementValue(styleElement, "ottavaHookAbove", prefs->smartShapeOptions->hookLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "ottavaHookBelow", -prefs->smartShapeOptions->hookLength / EVPU_PER_SPACE);
    writeLinePrefs(styleElement, "ottava", prefs->smartShapeOptions->smartLineWidth, prefs->smartShapeOptions->smartDashOn, prefs->smartShapeOptions->smartDashOff, "dashed");
    setElementValue(styleElement, "ottavaNumbersOnly", prefs->smartShapeOptions->showOctavaAsText);
}

void writeMeasureNumberPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using MeasureNumberRegion = others::MeasureNumberRegion;
    setElementValue(styleElement, "showMeasureNumber", prefs->measNumScorePart != nullptr);
    if (prefs->measNumScorePart) {
        const auto& scorePart = prefs->measNumScorePart;
        setElementValue(styleElement, "showMeasureNumberOne", !scorePart->hideFirstMeasure);
        setElementValue(styleElement, "measureNumberInterval", scorePart->incidence);
        setElementValue(styleElement, "measureNumberSystem", scorePart->showOnStart && !scorePart->showOnEvery);

        // Helper lambdas for processing justification and alignment
        auto justificationString = [](MeasureNumberRegion::AlignJustify justi) -> std::string {
            switch (justi) {
                default:
                case MeasureNumberRegion::AlignJustify::Left: return "left,baseline";
                case MeasureNumberRegion::AlignJustify::Center: return "center,baseline";
                case MeasureNumberRegion::AlignJustify::Right: return "right,baseline";
            }
        };

        // WARNING: These values changed in MuseScore 4.6 to be AlignH values ("left", "center", "right")
        auto horizontalAlignment = [](MeasureNumberRegion::AlignJustify align) -> int {
            switch (align) {
                default:
                case MeasureNumberRegion::AlignJustify::Left: return 0;
                case MeasureNumberRegion::AlignJustify::Center: return 1;
                case MeasureNumberRegion::AlignJustify::Right: return 2;
            }
        };

        auto verticalAlignment = [](Evpu vertical) -> int {
            return (vertical >= 0) ? 0 : 1;
        };

        // Helper function to process segments
        auto processSegment = [&](const std::shared_ptr<FontInfo>& fontInfo,
                                  const std::shared_ptr<others::Enclosure>& enclosure,
                                  bool useEnclosure,
                                  MeasureNumberRegion::AlignJustify justification,
                                  MeasureNumberRegion::AlignJustify alignment,
                                  Evpu vertical,
                                  const std::string& prefix) {
            writeFontPref(styleElement, prefix, fontInfo.get());
            setElementValue(styleElement, prefix + "VPlacement", verticalAlignment(vertical));
            setElementValue(styleElement, prefix + "HPlacement", horizontalAlignment(alignment));
            setElementValue(styleElement, prefix + "Align", justificationString(justification));
            writeFramePrefs(styleElement, prefix, useEnclosure ? enclosure.get() : nullptr);
        };

        // Determine the source for primary measure number segment
        auto fontInfo = scorePart->showOnStart ? scorePart->startFont : scorePart->multipleFont;
        auto enclosure = scorePart->showOnStart ? scorePart->startEnclosure : scorePart->multipleEnclosure;
        auto useEnclosure = scorePart->showOnStart ? scorePart->useStartEncl : scorePart->useMultipleEncl;
        auto justification = scorePart->showOnEvery ? scorePart->multipleJustify : scorePart->startJustify;
        auto alignment = scorePart->showOnEvery ? scorePart->multipleAlign : scorePart->startAlign;
        auto vertical = scorePart->showOnStart ? scorePart->startYdisp : scorePart->multipleYdisp;

        // Process primary measure number segment
        setElementValue(styleElement, "measureNumberOffsetType", 1); // Hardcoded offset type
        processSegment(fontInfo, enclosure, useEnclosure, justification, alignment, vertical, "measureNumber");

        // Process multi-measure rest settings
        setElementValue(styleElement, "mmRestShowMeasureNumberRange", scorePart->showMmRange);

        if (scorePart->leftMmBracketChar == 0) {
            setElementValue(styleElement, "mmRestRangeBracketType", 2); // None
        } else if (scorePart->leftMmBracketChar == '(') {
            setElementValue(styleElement, "mmRestRangeBracketType", 1); // Parenthesis
        } else {
            setElementValue(styleElement, "mmRestRangeBracketType", 0); // Default
        }

        // Process multi-measure rest segment
        processSegment(scorePart->mmRestFont, scorePart->multipleEnclosure, scorePart->useMultipleEncl,
                       scorePart->mmRestJustify, scorePart->mmRestAlign, scorePart->mmRestYdisp, "mmRestRange");
    }
    setElementValue(styleElement, "createMultiMeasureRests", prefs->forPartId != 0);
    setElementValue(styleElement, "minEmptyMeasures", prefs->mmRestOptions->numStart);
    setElementValue(styleElement, "minMMRestWidth", prefs->mmRestOptions->measWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "mmRestNumberPos", (prefs->mmRestOptions->numAdjY / EVPU_PER_SPACE) + 1);
    // setElementValue(styleElement, "multiMeasureRestMargin", prefs->mmRestOptions->startAdjust / EVPU_PER_SPACE); // Uncomment if margin is required
    setElementValue(styleElement, "oldStyleMultiMeasureRests", prefs->mmRestOptions->useSymbols && prefs->mmRestOptions->useSymsThreshold > 1);
    setElementValue(styleElement, "mmRestOldStyleMaxMeasures", (std::max)(prefs->mmRestOptions->useSymsThreshold - 1, 0));
    setElementValue(styleElement, "mmRestOldStyleSpacing", prefs->mmRestOptions->symSpacing / EVPU_PER_SPACE);
}

void writeRepeatEndingPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    const auto& repeatOptions = prefs->repeatOptions;
    setElementValue(styleElement, "voltaLineWidth", repeatOptions->bracketLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "voltaLineStyle", "solid");
    writeDefaultFontPref(styleElement, prefs, "volta", options::FontOptions::FontType::Ending);
    setElementValue(styleElement, "voltaAlign", "left,baseline");

    // Optionally include bracket height and hook lengths if uncommented
    // XmlElement& element = setElementText(styleElement, "voltaPosAbove", "");
    // element->setDoubleAttribute("x", 0);
    // element->setDoubleAttribute("y", repeatOptions->bracketHeight / EVPU_PER_SPACE);

    // setElementValue(styleElement, "voltaHook", repeatOptions->bracketHookLen / EVPU_PER_SPACE);

    // Optionally include text offsets
    // element = setElementText(styleElement, "voltaOffset", "");
    // element->setDoubleAttribute("x", repeatOptions->bracketTextHPos / EVPU_PER_SPACE);
    // element->setDoubleAttribute("y", repeatOptions->bracketTextVPos / EVPU_PER_SPACE);
}

void writeTupletPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using TupletOptions = options::TupletOptions;
    const auto& tupletOptions = prefs->tupletOptions;

    setElementValue(styleElement, "tupletOutOfStaff", tupletOptions->avoidStaff);
    // tupletNumberRythmicCenter and tupletExtendToEndOfDuration are 4.6 settings, but MuseScore 4.5 should ignore them
    // while MuseScore 4.6 picks them up even out of a 4.6 file.
    setElementValue(styleElement, "tupletNumberRythmicCenter", tupletOptions->metricCenter); // 4.6 setting
    setElementValue(styleElement, "tupletExtendToEndOfDuration", tupletOptions->fullDura); // 4.6 setting
    setElementValue(styleElement, "tupletStemLeftDistance", tupletOptions->leftHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletStemRightDistance", tupletOptions->rightHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletNoteLeftDistance", tupletOptions->leftHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletNoteRightDistance", tupletOptions->rightHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletBracketWidth", tupletOptions->tupLineWidth / EFIX_PER_SPACE);

    switch (tupletOptions->posStyle) {
        case TupletOptions::PositioningStyle::Above:
            setElementValue(styleElement, "tupletDirection", 1);
            break;
        case TupletOptions::PositioningStyle::Below:
            setElementValue(styleElement, "tupletDirection", 2);
            break;
        default:
            setElementValue(styleElement, "tupletDirection", 0);
            break;
    }

    switch (tupletOptions->numStyle) {
        case TupletOptions::NumberStyle::Nothing:
            setElementValue(styleElement, "tupletNumberType", 2);
            break;
        case TupletOptions::NumberStyle::Number:
            setElementValue(styleElement, "tupletNumberType", 0);
            break;
        default:
            setElementValue(styleElement, "tupletNumberType", 1);
            break;
    }

    if (tupletOptions->brackStyle == TupletOptions::BracketStyle::Nothing) {
        setElementValue(styleElement, "tupletBracketType", 2);
    } else if (tupletOptions->autoBracketStyle == TupletOptions::AutoBracketStyle::Always) {
        setElementValue(styleElement, "tupletBracketType", 1);        
    } else {
        setElementValue(styleElement, "tupletBracketType", 0);        
    }

    const auto& fontInfo = options::FontOptions::getFontInfo(prefs->document, options::FontOptions::FontType::Tuplet);
    if (!fontInfo) {
        throw std::invalid_argument("Unable to load font pref for tuplets");
    }

    if (fontInfo->calcIsSMuFL()) {
        setElementValue(styleElement, "tupletMusicalSymbolsScale", museMagVal(prefs, options::FontOptions::FontType::Tuplet));
        setElementValue(styleElement, "tupletUseSymbols", true);
    } else {
        writeFontPref(styleElement, "tuplet", fontInfo.get());
        setElementValue(styleElement, "tupletMusicalSymbolsScale", 1.0);
        setElementValue(styleElement, "tupletUseSymbols", false);
    }

    setElementValue(
        styleElement,
        "tupletBracketHookHeight",
        (std::max)(-tupletOptions->leftHookLen, -tupletOptions->rightHookLen) / EVPU_PER_SPACE
    );
}

void writeMarkingPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using FontType = options::FontOptions::FontType;
    using CategoryType = others::MarkingCategory::CategoryType;

    auto cat = prefs->document->getOthers()->get<others::MarkingCategory>(prefs->forPartId, Cmper(CategoryType::Dynamics));
    if (!cat) {
        throw std::invalid_argument("unable to find MarkingCategory for dynamics");
    }
    auto catFontInfo = cat->musicFont;
    const bool catFontIsSMuFL = catFontInfo->calcIsSMuFL();
    const bool override = catFontInfo && catFontIsSMuFL && catFontInfo->fontId != 0;
    setElementValue(styleElement, "dynamicsOverrideFont", override);
    if (override) {
        setElementValue(styleElement, "dynamicsFont", catFontInfo->getName());
        setElementValue(styleElement, "dynamicsSize", double(catFontInfo->fontSize) / double(prefs->defaultMusicFont->fontSize));
    } else {
        setElementValue(styleElement, "dynamicsFont", prefs->defaultMusicFont->getName());
        setElementValue(styleElement, "dynamicsSize",
                        catFontIsSMuFL ? (double(catFontInfo->fontSize) / double(prefs->defaultMusicFont->fontSize)) : 1.0);
    }
    // Load font preferences for Text Blocks
    auto textBlockFont = options::FontOptions::getFontInfo(prefs->document, FontType::TextBlock);
    if (!textBlockFont) {
        throw std::invalid_argument("unable to find font prefs for Text Blocks");
    }
    writeFontPref(styleElement, "default", textBlockFont.get());
    // since the following depend on Page Titles, just update the font face with the TEXTBLOCK font name
    setElementValue(styleElement, "titleFontFace", textBlockFont->getName());
    setElementValue(styleElement, "subTitleFontFace", textBlockFont->getName());
    setElementValue(styleElement, "composerFontFace", textBlockFont->getName());
    setElementValue(styleElement, "lyricistFontFace", textBlockFont->getName());
    writeDefaultFontPref(styleElement, prefs, "longInstrument", FontType::StaffNames);
    const auto fullPosition = prefs->staffOptions->namePos;
    if (!fullPosition) {
        throw std::invalid_argument("unable to find default full name positioning for staves");
    }
    auto justifyToAlignment = [](const std::shared_ptr<others::NamePositioning>& position) {
        switch (position->justify) {
            case others::NamePositioning::AlignJustify::Left:
                return std::string("left,center");
            case others::NamePositioning::AlignJustify::Right:
                return std::string("right,center");
            default:
                return std::string("center,center");
        }
    };
    setElementValue(styleElement, "longInstrumentAlign", justifyToAlignment(fullPosition));
    writeDefaultFontPref(styleElement, prefs, "shortInstrument", FontType::AbbrvStaffNames);
    const auto abbreviatedPosition = prefs->staffOptions->namePosAbbrv;
    if (!abbreviatedPosition) {
        throw std::invalid_argument("unable to find default abbreviated name positioning for staves");
    }
    setElementValue(styleElement, "shortInstrumentAlign", justifyToAlignment(abbreviatedPosition));
    writeDefaultFontPref(styleElement, prefs, "partInstrument", FontType::StaffNames);
    writeCategoryTextFontPref(styleElement, prefs, "dynamics", CategoryType::Dynamics);
    writeCategoryTextFontPref(styleElement, prefs, "expression", CategoryType::ExpressiveText);
    writeCategoryTextFontPref(styleElement, prefs, "tempo", CategoryType::TempoMarks);
    writeCategoryTextFontPref(styleElement, prefs, "tempoChange", CategoryType::ExpressiveText);
    writeLinePrefs(styleElement, "tempoChange", prefs->smartShapeOptions->smartLineWidth, prefs->smartShapeOptions->smartDashOn, prefs->smartShapeOptions->smartDashOff, "dashed");
    writeCategoryTextFontPref(styleElement, prefs, "metronome", CategoryType::TempoMarks);
    setElementValue(styleElement, "translatorFontFace", textBlockFont->getName());
    writeCategoryTextFontPref(styleElement, prefs, "systemText", CategoryType::ExpressiveText);
    writeCategoryTextFontPref(styleElement, prefs, "staffText", CategoryType::TechniqueText);
    writeCategoryTextFontPref(styleElement, prefs, "rehearsalMark", CategoryType::RehearsalMarks);
    writeDefaultFontPref(styleElement, prefs, "repeatLeft", FontType::Repeat);
    writeDefaultFontPref(styleElement, prefs, "repeatRight", FontType::Repeat);
    writeFontPref(styleElement, "frame", textBlockFont.get());
    writeCategoryTextFontPref(styleElement, prefs, "textLine", CategoryType::TechniqueText);
    writeCategoryTextFontPref(styleElement, prefs, "systemTextLine", CategoryType::ExpressiveText);
    writeCategoryTextFontPref(styleElement, prefs, "glissando", CategoryType::TechniqueText);
    writeCategoryTextFontPref(styleElement, prefs, "bend", CategoryType::TechniqueText);
    writeFontPref(styleElement, "header", textBlockFont.get());
    writeFontPref(styleElement, "footer", textBlockFont.get());
    writeFontPref(styleElement, "copyright", textBlockFont.get());
    writeFontPref(styleElement, "pageNumber", textBlockFont.get());
    writeFontPref(styleElement, "instrumentChange", textBlockFont.get());
    writeFontPref(styleElement, "sticking", textBlockFont.get());
    for (int i = 1; i <= 12; ++i) {
        writeFontPref(styleElement, "user" + std::to_string(i), textBlockFont.get());
    }
}

static void processPart(const std::filesystem::path& outputPath, const DocumentPtr& document, const DenigmaContext& denigmaContext, const std::shared_ptr<others::PartDefinition>& part = nullptr)
{
    // calculate actual output path
    std::filesystem::path qualifiedOutputPath = outputPath;
    if (part) {
        auto partName = part->getName(); // Unicode-encoded partname can contain non-ASCII characters 
        if (partName.empty()) {
            partName = "Part" + std::to_string(part->getCmper());
            denigmaContext.logMessage(LogMsg() << "No part name found. Using " << partName << " for part name extension");
        }
        auto currExtension = qualifiedOutputPath.extension();
        qualifiedOutputPath.replace_extension(utils::utf8ToPath(partName + currExtension.u8string()));
    }
    if (!denigmaContext.validatePathsAndOptions(qualifiedOutputPath)) return;

    const Cmper forPartId = part ? part->getCmper() : 0;
    auto prefs = getCurrentPrefs(document, forPartId, denigmaContext);

    // extract document to mss
    XmlDocument mssDoc; // output
    auto declaration = mssDoc.append_child(pugi::node_declaration);
    declaration.append_attribute("version") = "1.0";
    declaration.append_attribute("encoding") = "UTF-8";
    auto museScoreElement = mssDoc.append_child("museScore");
    museScoreElement.append_attribute("version") = MSS_VERSION;
    auto styleElement = museScoreElement.append_child("Style");
    // write prefs from document
    writePagePrefs(styleElement, prefs);
    writeLyricsPrefs(styleElement, prefs);
    writeLineMeasurePrefs(styleElement, prefs);
    writeStemPrefs(styleElement, prefs);
    writeMusicSpacingPrefs(styleElement, prefs);
    writeNoteRelatedPrefs(styleElement, prefs);
    writeSmartShapePrefs(styleElement, prefs);
    writeMeasureNumberPrefs(styleElement, prefs);
    writeRepeatEndingPrefs(styleElement, prefs);
    writeTupletPrefs(styleElement, prefs);
    writeMarkingPrefs(styleElement, prefs);
    // output
    // open the file ourselves to avoid Windows ACP encoding issues for path strings
    std::ofstream file;
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.open(qualifiedOutputPath, std::ios::out | std::ios::binary);
    mssDoc.save(file, "    ");
    file.close();
}

void convert(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.testOutput) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << outputPath.u8string());
        return;
    }
#endif

    auto document = musx::factory::DocumentFactory::create<MusxReader>(xmlBuffer);
    if (denigmaContext.allPartsAndScore || !denigmaContext.partName.has_value()) {
        processPart(outputPath, document, denigmaContext); // process the score
    }
    bool foundPart = false;
    if (denigmaContext.allPartsAndScore || denigmaContext.partName.has_value()) {
        auto parts = document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
        for (const auto& part : parts) {
            if (part->getCmper()) {
                if (denigmaContext.allPartsAndScore) {
                    processPart(outputPath, document, denigmaContext, part);
                } else if (denigmaContext.partName->empty() || part->getName().rfind(denigmaContext.partName.value(), 0) == 0) {
                    processPart(outputPath, document, denigmaContext, part);
                    foundPart = true;
                    break;
                }
            }
        }
    }
    if (!foundPart && denigmaContext.partName.has_value() && !denigmaContext.allPartsAndScore) {
        if (denigmaContext.partName->empty()) {
            denigmaContext.logMessage(LogMsg() << "No parts were found in document", LogSeverity::Warning);
        } else {
            denigmaContext.logMessage(LogMsg() << "No part name starting with \"" << denigmaContext.partName.value() << "\" was found", LogSeverity::Warning);
        }
    }
}

} // namespace mss
} // namespace denigma
