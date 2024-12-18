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

#include "mss.h"
#include "musx/musx.h"
#include "tinyxml2.h"

namespace denigma {
namespace mss {

using namespace ::musx::dom;

using XmlDocument = ::tinyxml2::XMLDocument;
using XmlElement = ::tinyxml2::XMLElement;
using XmlAttribute = ::tinyxml2::XMLAttribute;

constexpr Cmper forPartId = false; // ToDo: eventually this will be a program option

constexpr static char MSS_VERSION[] = "4.50";

constexpr static double EVPU_PER_INCH = 288.0;
constexpr static double EVPU_PER_MM = 288.0 / 25.4;
constexpr static double EVPU_PER_SPACE = 24.0;
constexpr static double EFIX_PER_EVPU = 64.0;
constexpr static double EFIX_PER_SPACE = EVPU_PER_SPACE * EFIX_PER_EVPU;
constexpr static double MUSE_FINALE_SCALE_DIFFERENTIAL = 20.0 / 24.0;

static const std::set<std::string_view> museScoreSMuFLFonts{
    "Bravura",
    "Leland",
    "Emmentaler",
    "Gonville",
    "MuseJazz",
    "Petaluma",
    "Finale Maestro",
    "Finale Broadway"
};

// Finale preferences:
struct FinalePreferences
{
    DocumentPtr document;
    std::shared_ptr<FontInfo> defaultMusicFont;
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
    std::shared_ptr<options::StemOptions> stemOptions;
    std::shared_ptr<options::TieOptions> tieOptions;
    std::shared_ptr<options::TimeSignatureOptions> timeOptions;
    //
    std::shared_ptr<others::LayerAttributes> layerOneAttributes;
    std::shared_ptr<others::PartGlobals> partGlobals;
};
using FinalePreferencesPtr = std::shared_ptr<FinalePreferences>;

template <typename T>
static std::shared_ptr<T> getDocOptions(const FinalePreferencesPtr& prefs, const std::string& prefsName)
{
    auto retval = prefs->document->getOptions()->get<T>();
    if (!retval) {
        throw std::invalid_argument("document contains no default " + prefsName + " options");
    }
    return retval;
}

static FinalePreferencesPtr getCurrentPrefs(const enigmaxml::Buffer& xmlBuffer, Cmper forPartId)
{
    auto retval = std::make_shared<FinalePreferences>();
    retval->document = musx::factory::DocumentFactory::create<musx::xml::rapidxml::Document>(xmlBuffer);

    auto fontOptions = getDocOptions<options::FontOptions>(retval, "font");
    retval->defaultMusicFont = fontOptions->getFontInfo(options::FontOptions::FontType::Music);
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
    retval->miscOptions = getDocOptions<options::MiscOptions>(retval, "multimeasure rest");
    retval->mmRestOptions = getDocOptions<options::MultimeasureRestOptions>(retval, "miscellaneous");
    retval->musicSpacing = getDocOptions<options::MusicSpacingOptions>(retval, "music spacing");
    auto pageFormatOptions = getDocOptions<options::PageFormatOptions>(retval, "page format");
    retval->pageFormat = forPartId ? pageFormatOptions->pageFormatParts : pageFormatOptions->pageFormatScore;
    retval->braceOptions = getDocOptions<options::PianoBraceBracketOptions>(retval, "piano braces & brackets");
    retval->repeatOptions = getDocOptions<options::RepeatOptions>(retval, "repeat");
    retval->smartShapeOptions = getDocOptions<options::SmartShapeOptions>(retval, "smart shape");
    retval->stemOptions = getDocOptions<options::StemOptions>(retval, "stem");
    retval->tieOptions = getDocOptions<options::TieOptions>(retval, "tie");
    retval->timeOptions = getDocOptions<options::TimeSignatureOptions>(retval, "time signature");
    //
    retval->layerOneAttributes = retval->document->getOthers()->get<others::LayerAttributes>(0);
    if (!retval->layerOneAttributes) {
        throw std::invalid_argument("document contains no options for Layer 1");
    }
    retval->partGlobals = retval->document->getOthers()->getEffectiveForPart<others::PartGlobals>(forPartId, MUSX_GLOBALS_CMPER);
    if (!retval->layerOneAttributes) {
        throw std::invalid_argument("document contains no options for Layer 1");
    }

    return retval;
}

template<typename T>
static XmlElement* setElementValue(XmlElement* styleElement, const std::string& nodeName, const T& value)
{
    if (!styleElement) {
        throw std::invalid_argument("styleElement cannot be null");
    }

    XmlElement* element = styleElement->FirstChildElement(nodeName.c_str());
    if (!element) {
        element = styleElement->InsertNewChildElement(nodeName.c_str());
    }

    if constexpr (std::is_same_v<T, std::nullptr_t>) {
        static_assert(std::is_same_v<T, std::nullptr_t>, "Incorrect property.");
    } else if constexpr (std::is_floating_point_v<T>) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "%.5g", value);
        element->SetText(buffer);
    } else if constexpr (std::is_same_v<T, std::string>) {
        element->SetText(value.c_str());
    } else if constexpr (std::is_same_v<T, bool>) {
        element->SetText(int(value));
    } else {
        element->SetText(value);
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
    if (fontPrefs->getFontName() == prefs->defaultMusicFont->getFontName()) {
        return double(fontPrefs->fontSize) / double(prefs->defaultMusicFont->fontSize);
    }
    return 1.0;
}

static void writeFontPref(XmlElement* styleElement, const std::string& namePrefix, const FontInfo* fontInfo)
{
    setElementValue(styleElement, namePrefix + "FontFace", fontInfo->getFontName());
    setElementValue(styleElement, namePrefix + "FontSize", 
                    double(fontInfo->fontSize) * (fontInfo->absolute ? 1.0 : MUSE_FINALE_SCALE_DIFFERENTIAL));
    setElementValue(styleElement, namePrefix + "FontSpatiumDependent", !fontInfo->absolute);
    setElementValue(styleElement, namePrefix + "FontStyle", museFontEfx(fontInfo));
}

static void writeDefaultFontPref(XmlElement* styleElement, const FinalePreferencesPtr& prefs, const std::string& namePrefix, options::FontOptions::FontType type)
{
    auto fontPrefs = options::FontOptions::getFontInfo(prefs->document, type);
    writeFontPref(styleElement, namePrefix, fontPrefs.get());
}

void writeLinePrefs(XmlElement* styleElement,
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

static void writeFramePrefs(XmlElement* styleElement, const std::string& namePrefix, const others::Enclosure* enclosure = nullptr)
{
    if (!enclosure || enclosure->shape == others::Enclosure::Shape::NoEnclosure || enclosure->lineWidth == 0) {
        setElementValue(styleElement, namePrefix + "FrameType", 0);
        return; // Do not override any other defaults if no enclosure shape
    } 
    if (enclosure->shape == others::Enclosure::Shape::Ellipse) {
        setElementValue(styleElement, namePrefix + "FrameType", 2);
    } else {
        setElementValue(styleElement, namePrefix + "FrameType", 1);
    }
    setElementValue(styleElement, namePrefix + "FramePadding", enclosure->xMargin / EVPU_PER_SPACE);
    setElementValue(styleElement, namePrefix + "FrameWidth", enclosure->lineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, namePrefix + "FrameRound", 
                    enclosure->roundCorners ? enclosure->cornerRadius / EFIX_PER_EVPU : 0.0);
}

static void writeCategoryTextFontPref(XmlElement* styleElement, const FinalePreferencesPtr& prefs, const std::string& namePrefix, others::MarkingCategory::CategoryType categoryType)
{
    auto cat = prefs->document->getOthers()->get<others::MarkingCategory>(Cmper(categoryType));
    if (!cat) {
        std::cout << "unable to load category def for " << namePrefix << std::endl;
        return;
    }
    if (!cat->textFont) {
        std::cout << "marking category " << cat->getName() << " has no text font." << std::endl;
        return;
    }
    writeFontPref(styleElement, namePrefix, cat->textFont.get());
    for (auto& it : cat->textExpressions) {
        if (auto exp = it.second.lock()) {
            writeFramePrefs(styleElement, namePrefix, exp->getEnclosure().get());
            break;
        } else {
            std::cout << "marking category " << cat->getName() << " has invalid text expression." << std::endl;
        }
    }
}

static void writePagePrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    auto pagePrefs = prefs->pageFormat;

    // Set XML element values
    setElementValue(styleElement, "pageWidth", double(pagePrefs->pageWidth) / EVPU_PER_INCH);
    setElementValue(styleElement, "pageHeight", double(pagePrefs->pageHeight) / EVPU_PER_INCH);
    setElementValue(styleElement, "pagePrintableWidth",
                    double(pagePrefs->pageWidth + pagePrefs->leftPageMarginRight + pagePrefs->leftPageMarginRight) / EVPU_PER_INCH);
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
    auto pagePercent = double(pagePrefs->pagePercent) / 100.0;
    auto staffPercent = (double(pagePrefs->rawStaffHeight) / (EVPU_PER_SPACE * 4 * 16)) * (double(pagePrefs->sysPercent) / 100.0);
    setElementValue(styleElement, "spatium", (EVPU_PER_SPACE * staffPercent * pagePercent) / EVPU_PER_MM);

    // Default music font
    auto defaultMusicFont = prefs->defaultMusicFont;
    bool isSMuFL = [defaultMusicFont]() -> bool {
        if (defaultMusicFont->calcIsSMuFL()) {
            return true;
        }
        auto it = museScoreSMuFLFonts.find(defaultMusicFont->getFontName());
        return it != museScoreSMuFLFonts.end();
    }();
    if (isSMuFL) {
        setElementValue(styleElement, "musicalSymbolFont", defaultMusicFont->getFontName());
        setElementValue(styleElement, "musicalTextFont", defaultMusicFont->getFontName() + " Text");
    }
}

static void writeLyricsPrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    auto fontInfo = options::FontOptions::getFontInfo(prefs->document, options::FontOptions::FontType::LyricVerse);
    for (auto [verseNumber, evenOdd] : {
            std::make_pair(1, "Odd"),
            std::make_pair(2, "Even")
        }) {
        auto verseText = prefs->document->getTexts()->get<texts::LyricsVerse>(verseNumber);
        if (verseText && !verseText->text.empty()) {
            auto font = verseText->parseFirstFontInfo();
            if (font) {
                fontInfo = font;
            }
        }
        writeFontPref(styleElement, "lyrics" + std::string(evenOdd), fontInfo.get());
    }
}

void writeLineMeasurePrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs, Cmper forPartId)
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
    setElementValue(styleElement, "bracketDistance", -prefs->braceOptions->defBracketPos / EVPU_PER_SPACE);
    setElementValue(styleElement, "akkoladeBarDistance", -prefs->braceOptions->defBracketPos / EVPU_PER_SPACE);

    setElementValue(styleElement, "clefLeftMargin", prefs->clefOptions->clefFrontSepar / EVPU_PER_SPACE);
    setElementValue(styleElement, "keysigLeftMargin", prefs->keyOptions->keyFront / EVPU_PER_SPACE);

    const double timeSigSpaceBefore = forPartId
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

    setElementValue(styleElement, "clefBarlineDistance", prefs->repeatOptions->afterClefSpace / EVPU_PER_SPACE);
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
    setElementValue(styleElement, "hideEmptyStaves", !forPartId);
}

void writeStemPrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "useStraightNoteFlags", prefs->flagOptions->straightFlags);
    setElementValue(styleElement, "stemWidth", prefs->stemOptions->stemWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "shortenStem", true);
    setElementValue(styleElement, "stemLength", prefs->stemOptions->stemLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "shortestStem", prefs->stemOptions->shortStemLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "stemSlashThickness", prefs->graceOptions->graceSlashWidth / EFIX_PER_SPACE);
}

void writeMusicSpacingPrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "minMeasureWidth", prefs->musicSpacing->minWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "minNoteDistance", prefs->musicSpacing->minDistance / EVPU_PER_SPACE);
    setElementValue(styleElement, "measureSpacing", prefs->musicSpacing->scalingFactor);
    setElementValue(styleElement, "minTieLength", prefs->musicSpacing->minDistTiedNotes / EVPU_PER_SPACE);
}

void writeNoteRelatedPrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "accidentalDistance", prefs->accidentalOptions->acciAcciSpace / EVPU_PER_SPACE);
    setElementValue(styleElement, "accidentalNoteDistance", prefs->accidentalOptions->acciNoteSpace / EVPU_PER_SPACE);
    setElementValue(styleElement, "beamWidth", prefs->beamOptions->beamWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "useWideBeams", prefs->beamOptions->beamSepar > (0.75 * EVPU_PER_SPACE));
    // Finale randomly adds twice the stem width to the length of a beam stub. (Observed behavior)
    setElementValue(styleElement, "beamMinLen", (prefs->beamOptions->beamStubLength + (2.0 * prefs->stemOptions->stemWidth / EFIX_PER_EVPU)) / EVPU_PER_SPACE);
    setElementValue(styleElement, "beamNoSlope", prefs->beamOptions->beamingStyle == options::BeamOptions::FlattenStyle::AlwaysFlat);
    setElementValue(styleElement, "dotMag", museMagVal(prefs, options::FontOptions::FontType::AugDots));
    setElementValue(styleElement, "dotNoteDistance", prefs->augDotOptions->dotNoteOffset / EVPU_PER_SPACE);
    setElementValue(styleElement, "dotRestDistance", prefs->augDotOptions->dotNoteOffset / EVPU_PER_SPACE); // Same value as dotNoteDistance
    setElementValue(styleElement, "dotDotDistance", prefs->augDotOptions->dotOffset / EVPU_PER_SPACE);
    setElementValue(styleElement, "articulationMag", museMagVal(prefs, options::FontOptions::FontType::Articulation));
    setElementValue(styleElement, "graceNoteMag", prefs->graceOptions->gracePerc / 100.0);
    setElementValue(styleElement, "concertPitch", !prefs->partGlobals->showTransposed);
    setElementValue(styleElement, "multiVoiceRestTwoSpaceOffset", std::labs(prefs->layerOneAttributes->restOffset) >= 4);
    setElementValue(styleElement, "mergeMatchingRests", prefs->miscOptions->consolidateRestsAcrossLayers);
}

void writeSmartShapePrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs)
{
    // Hairpin-related settings
    setElementValue(styleElement, "hairpinHeight", prefs->smartShapeOptions->shortHairpinOpeningWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "hairpinContHeight", 0.5); // Hardcoded to a half space
    writeCategoryTextFontPref(styleElement, prefs, "hairpin", others::MarkingCategory::CategoryType::Dynamics);
    writeLinePrefs(styleElement, "hairpin", prefs->smartShapeOptions->smartLineWidth, prefs->smartShapeOptions->smartDashOn, prefs->smartShapeOptions->smartDashOff);

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

void writeMeasureNumberPrefs(XmlElement* styleElement, const FinalePreferencesPtr& prefs, Cmper forPartId)
{
    setElementValue(styleElement, "createMultiMeasureRests", forPartId != 0); // Equivalent to Lua's `current_is_part`
    setElementValue(styleElement, "minEmptyMeasures", prefs->mmRestOptions->numStart);
    setElementValue(styleElement, "minMMRestWidth", prefs->mmRestOptions->measWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "mmRestNumberPos", (prefs->mmRestOptions->numAdjY / EVPU_PER_SPACE) + 1);
    // setElementValue(styleElement, "multiMeasureRestMargin", prefs->mmRestOptions->startAdjust / EVPU_PER_SPACE); // Uncomment if margin is required
    setElementValue(styleElement, "oldStyleMultiMeasureRests", prefs->mmRestOptions->useSymbols && prefs->mmRestOptions->useSymsThreshold > 1);
    setElementValue(styleElement, "mmRestOldStyleMaxMeasures", std::max(prefs->mmRestOptions->useSymsThreshold - 1, 0));
    setElementValue(styleElement, "mmRestOldStyleSpacing", prefs->mmRestOptions->symSpacing / EVPU_PER_SPACE);
}

void convert(const std::filesystem::path& outputPath, const enigmaxml::Buffer& xmlBuffer)
{
    // ToDo: lots
    try {
        // construct source instance and release input memory
        auto prefs = getCurrentPrefs(xmlBuffer, forPartId);

        // extract document to mss
        XmlDocument mssDoc; // output
        mssDoc.InsertEndChild(mssDoc.NewDeclaration());
        auto museScoreElement = mssDoc.NewElement("museScore");
        museScoreElement->SetAttribute("version", MSS_VERSION);
        mssDoc.InsertEndChild(museScoreElement);
        auto styleElement = museScoreElement->InsertNewChildElement("Style");
        //
        writePagePrefs(styleElement, prefs);
        writeLyricsPrefs(styleElement, prefs);
        writeLineMeasurePrefs(styleElement, prefs, forPartId);
        writeStemPrefs(styleElement, prefs);
        writeMusicSpacingPrefs(styleElement, prefs);
        writeNoteRelatedPrefs(styleElement, prefs);
        writeSmartShapePrefs(styleElement, prefs);
        writeMeasureNumberPrefs(styleElement, prefs, forPartId);
        //
        if (mssDoc.SaveFile(outputPath.string().c_str()) != ::tinyxml2::XML_SUCCESS) {
            throw std::runtime_error(mssDoc.ErrorStr());
        }
        std::cout << "converted to " << outputPath.string() << std::endl;
    } catch (const musx::xml::load_error& ex) {
        std::cerr << "Load XML failed: " << ex.what() << std::endl;
        throw;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        throw;
    }
}

} // namespace mss
} // namespace denigma
