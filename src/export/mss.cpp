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
#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "denigma.h"

#include "mss.h"
#include "musx/musx.h"
#include "pugixml.hpp"
#include "utils/stringutils.h"
#include "utils/smufl_support.h"
#include "utils/textmetrics.h"

namespace denigma {
namespace mss {

using namespace ::musx::dom;
using namespace ::musx::factory;

using XmlDocument = ::pugi::xml_document;
using XmlElement = ::pugi::xml_node;
using XmlAttribute = ::pugi::xml_attribute;

constexpr char MSS_VERSION[] = "4.60"; // Do not change this version without checking notes on changed values.
constexpr double MUSE_FINALE_SCALE_DIFFERENTIAL = 20.0 / 24.0;
constexpr double POINTS_PER_INCH = 72.0;
constexpr double FONT_ASCENT_SCALE = 0.7;
constexpr int MUSE_NUMERIC_PRECISION = 5;
constexpr double SYMBOLS_DEFAULT_SIZE = 10.0;

static constexpr auto solidLinesWithHooks = std::to_array<std::string_view>({
    "textLine",
    "systemTextLine",
    "letRing",
    "palmMute",
    "pedal"
});

static constexpr auto dashedLinesWithHooks = std::to_array<std::string_view>({
    "whammyBar"
});

static constexpr auto solidLinesNoHooks = std::to_array<std::string_view>({
    "noteLine",
    "glissando"
});

static constexpr auto dashedLinesNoHooks = std::to_array<std::string_view>({
    "ottava",
    "tempoChange"
});

// Legacy names are normalised to lower-case with spaces removed.
static constexpr auto finaleToSmuflFontMap = std::to_array<std::pair<std::string_view, std::string_view>>({
    std::pair<std::string_view, std::string_view>{ "ashmusic", "Finale Ash" },
    std::pair<std::string_view, std::string_view>{ "broadwaycopyist", "Finale Broadway" },
    std::pair<std::string_view, std::string_view>{ "engraver", "Finale Engraver" },
    std::pair<std::string_view, std::string_view>{ "engraverfontset", "Finale Engraver" },
    std::pair<std::string_view, std::string_view>{ "jazz", "Finale Jazz" },
    std::pair<std::string_view, std::string_view>{ "maestro", "Finale Maestro" },
    std::pair<std::string_view, std::string_view>{ "petrucci", "Finale Legacy" },
    std::pair<std::string_view, std::string_view>{ "pmusic", "Finale Maestro" },
    std::pair<std::string_view, std::string_view>{ "sonata", "Finale Maestro" },
});

static std::string normalizedFontName(std::string_view fontName)
{
    std::string result;
    result.reserve(fontName.size());
    for (unsigned char c : fontName) {
        if (c <= 0x7f && std::isspace(c)) {
            continue;
        }
        result.push_back(char(std::tolower(c)));
    }
    return result;
}

static std::optional<std::string_view> mappedSmuflFontName(std::string_view fontName)
{
    const std::string normalized = normalizedFontName(fontName);
    for (const auto& [finaleName, smuflName] : finaleToSmuflFontMap) {
        if (finaleName == normalized || normalizedFontName(smuflName) == normalized) {
            return smuflName;
        }
    }
    return std::nullopt;
}

// Finale preferences:
struct FinalePreferences
{
    FinalePreferences(const DenigmaContext& context)
        : denigmaContext(&context) {}

    const DenigmaContext* denigmaContext;
    DocumentPtr document;
    Cmper forPartId{};
    //
    MusxInstance<FontInfo> defaultMusicFont;
    std::string musicFontName;
    double spatiumScaling{};
    //
    MusxInstance<options::AccidentalOptions> accidentalOptions;
    MusxInstance<options::AlternateNotationOptions> alternateNotationOptions;
    MusxInstance<options::AugmentationDotOptions> augDotOptions;
    MusxInstance<options::BarlineOptions> barlineOptions;
    MusxInstance<options::BeamOptions> beamOptions;
    MusxInstance<options::ChordOptions> chordOptions;
    MusxInstance<options::ClefOptions> clefOptions;
    MusxInstance<options::FlagOptions> flagOptions;
    MusxInstance<options::GraceNoteOptions> graceOptions;
    MusxInstance<options::KeySignatureOptions> keyOptions;
    MusxInstance<options::LineCurveOptions> lineCurveOptions;
    MusxInstance<options::MiscOptions> miscOptions;
    MusxInstance<options::MusicSymbolOptions> musicSymbolOptions;
    MusxInstance<options::MultimeasureRestOptions> mmRestOptions;
    MusxInstance<options::MusicSpacingOptions> musicSpacing;
    MusxInstance<options::PageFormatOptions::PageFormat> pageFormat;
    MusxInstance<options::PianoBraceBracketOptions> braceOptions;
    MusxInstance<options::RepeatOptions> repeatOptions;
    MusxInstance<options::SmartShapeOptions> smartShapeOptions;
    MusxInstance<options::StaffOptions> staffOptions;
    MusxInstance<options::StemOptions> stemOptions;
    MusxInstance<options::TieOptions> tieOptions;
    MusxInstance<options::TimeSignatureOptions> timeOptions;
    MusxInstance<options::TupletOptions> tupletOptions;
    //
    MusxInstance<others::LayerAttributes> layerOneAttributes;
    MusxInstance<others::MeasureNumberRegion::ScorePartData> measNumScorePart;
    MusxInstance<others::PartGlobals> partGlobals;
};
using FinalePreferencesPtr = std::shared_ptr<FinalePreferences>;

template <typename T>
static MusxInstance<T> getDocOptions(const FinalePreferencesPtr& prefs, const std::string& prefsName)
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
        }
        if (const auto mapped = mappedSmuflFontName(fontName)) {
            return std::string(*mapped);
        }
        return {};
    }();
    //
    retval->accidentalOptions = getDocOptions<options::AccidentalOptions>(retval, "accidental");
    retval->alternateNotationOptions = getDocOptions<options::AlternateNotationOptions>(retval, "alternate notation");
    retval->augDotOptions = getDocOptions<options::AugmentationDotOptions>(retval, "augmentation dot");
    retval->barlineOptions = getDocOptions<options::BarlineOptions>(retval, "barline");
    retval->beamOptions = getDocOptions<options::BeamOptions>(retval, "beam");
    retval->chordOptions = getDocOptions<options::ChordOptions>(retval, "chord");
    retval->clefOptions = getDocOptions<options::ClefOptions>(retval, "clef");
    retval->flagOptions = getDocOptions<options::FlagOptions>(retval, "flag");
    retval->graceOptions = getDocOptions<options::GraceNoteOptions>(retval, "grace note");
    retval->keyOptions = getDocOptions<options::KeySignatureOptions>(retval, "key signature");
    retval->lineCurveOptions = getDocOptions<options::LineCurveOptions>(retval, "lines & curves");
    retval->miscOptions = getDocOptions<options::MiscOptions>(retval, "miscellaneous");
    retval->musicSymbolOptions = getDocOptions<options::MusicSymbolOptions>(retval, "music symbol");
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
    retval->spatiumScaling = retval->pageFormat->calcCombinedSystemScaling().toDouble();

    return retval;
}

static std::string formatMuseFloat(double value)
{
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%.*g", MUSE_NUMERIC_PRECISION, value);
    return std::string(buffer);
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
        const std::string formatted = formatMuseFloat(value);
        element.text().set(formatted.c_str());
    } else if constexpr (std::is_same_v<T, std::string>) {
        element.text().set(value.c_str());
    } else if constexpr (std::is_same_v<T, bool>) {
        element.text().set(static_cast<int>(value)); // MuseScore .mss files use 0 and 1 for booleans
    } else {
        element.text().set(value);
    }

    return element;
}

static void setPointElement(XmlElement& styleElement, const std::string& nodeName, double x, double y)
{
    if (!styleElement) {
        throw std::invalid_argument("styleElement cannot be null");
    }

    XmlElement element = styleElement.child(nodeName.c_str());
    if (!element) {
        element = styleElement.append_child(nodeName.c_str());
    }

    auto setAttribute = [&](const char* name, double value) {
        XmlAttribute attribute = element.attribute(name);
        if (!attribute) {
            attribute = element.append_attribute(name);
        }
        const std::string formatted = formatMuseFloat(value);
        attribute.set_value(formatted.c_str());
    };

    element.text().set("");
    setAttribute("x", x);
    setAttribute("y", y);
}

static std::string alignJustifyToHorizontalString(AlignJustify justify)
{
    switch (justify) {
        case AlignJustify::Right:
            return "right";
        case AlignJustify::Center:
            return "center";
        default:
            return "left";
    }
}

static std::string alignJustifyToAlignString(AlignJustify justify, const char* vertical)
{
    return alignJustifyToHorizontalString(justify) + "," + vertical;
}

static double approximateFontAscentInSpaces(const FontInfo* fontInfo, const DenigmaContext& denigmaContext)
{
    if (!fontInfo) {
        return 0.0;
    }

    const double scaledPointSize = double(fontInfo->fontSize)
                                 * (fontInfo->absolute ? 1.0 : MUSE_FINALE_SCALE_DIFFERENTIAL);
    if (auto measuredMetricsEvpu = textmetrics::measureTextEvpu(*fontInfo, U"0123456789", scaledPointSize, denigmaContext)) {
        return measuredMetricsEvpu->ascent / EVPU_PER_SPACE;
    }
    const double ascentEvpu = (scaledPointSize / POINTS_PER_INCH) * EVPU_PER_INCH * FONT_ASCENT_SCALE;
    return ascentEvpu / EVPU_PER_SPACE;
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

static std::optional<EvpuFloat> calcAugmentationDotWidth(const FinalePreferencesPtr& prefs, double dotMag)
{
    if (!prefs->musicFontName.empty()) {
        if (auto musicFontWidth = utils::smuflGlyphWidthForFont(prefs->musicFontName, "augmentationDot")) {
            // SMuFL metadata is unscaled; apply dotMag here so this function always returns
            // the final width contribution used in dotDotDistance.
            return dotMag * musicFontWidth.value();
        }
    }
    if (!prefs->musicSymbolOptions || prefs->musicSymbolOptions->augDot == 0) {
        return std::nullopt;
    }

    const auto augDotFontInfo = options::FontOptions::getFontInfo(prefs->document, options::FontOptions::FontType::AugDots);
    if (!augDotFontInfo) {
        return std::nullopt;
    }
    const double scaledPointSize = double(augDotFontInfo->fontSize)
                                 * (augDotFontInfo->absolute ? 1.0 : MUSE_FINALE_SCALE_DIFFERENTIAL);
    if (auto measured = textmetrics::measureGlyphWidthEvpu(*augDotFontInfo,
                                                           prefs->musicSymbolOptions->augDot,
                                                           scaledPointSize,
                                                           *prefs->denigmaContext)) {
        return measured.value();
    }
    return std::nullopt;
}

static std::optional<EvpuFloat> calcRepeatDotWidth(const FinalePreferencesPtr& prefs)
{
    if (!prefs->musicFontName.empty()) {
        return utils::smuflGlyphWidthForFont(prefs->musicFontName, "repeatDot");
    }
    return std::nullopt;
}

static void writeFontPref(XmlElement& styleElement, const std::string& namePrefix, const FontInfo* fontInfo)
{
    setElementValue(styleElement, namePrefix + "FontFace", fontInfo->getName());
    setElementValue(styleElement, namePrefix + "FontSize", 
                    double(fontInfo->fontSize) * (fontInfo->absolute ? 1.0 : MUSE_FINALE_SCALE_DIFFERENTIAL));
    setElementValue(styleElement, namePrefix + "FontSpatiumDependent", !fontInfo->absolute);
    setElementValue(styleElement, namePrefix + "FontStyle", museFontEfx(fontInfo));
}

static double calcMusicalSymbolScale(const FinalePreferencesPtr& prefs, const FontInfo* fontInfo)
{
    double symbolScale = double(fontInfo->fontSize);
    if (fontInfo->calcIsSymbolFont()) {
        symbolScale /= double(prefs->defaultMusicFont->fontSize); /// @todo account for fixed/non-fixed
    } else if (const auto textBlockFont = options::FontOptions::getFontInfo(prefs->document, options::FontOptions::FontType::TextBlock)) {
        /// Fonts not recognised as symbols likely have their symbols scaled to text size
        symbolScale /= double(textBlockFont->fontSize);
    } else {
        /// Should never happen: Assume text to symbols factor of 2
        symbolScale *= 2 / double(prefs->defaultMusicFont->fontSize);
        prefs->denigmaContext->logMessage(LogMsg() << "Unable to load text block font while calculating symbol scale. "
            << "Assuming text-to-symbol factor of 2 for [" << fontInfo->getName() << "].", LogSeverity::Warning);
    }
    return symbolScale;
}

static void writeDefaultFontPref(XmlElement& styleElement, const FinalePreferencesPtr& prefs, const std::string& namePrefix, options::FontOptions::FontType type)
{
    if (auto fontPrefs = options::FontOptions::getFontInfo(prefs->document, type)) {
        // If font is a symbols font, write text settings from TextBlock and set symbol scaling.
        if (type != options::FontOptions::FontType::TextBlock && (fontPrefs->calcIsSMuFL() || fontPrefs->calcIsSymbolFont())) {
            writeDefaultFontPref(styleElement, prefs, namePrefix, options::FontOptions::FontType::TextBlock);
            const double symbolScale = calcMusicalSymbolScale(prefs, fontPrefs.get());
            setElementValue(styleElement, namePrefix + "MusicalSymbolsScale", symbolScale);
            setElementValue(styleElement, namePrefix + "MusicalSymbolSize", symbolScale * SYMBOLS_DEFAULT_SIZE);
            setElementValue(styleElement, namePrefix + "FontSpatiumDependent", !fontPrefs->absolute);
        } else {
            writeFontPref(styleElement, namePrefix, fontPrefs.get());
        }
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
    const FontInfo* categoryFont = nullptr;
    if (namePrefix == "metronome" && cat->numberFont) {
        categoryFont = cat->numberFont.get();
    } else if (cat->textFont) {
        categoryFont = cat->textFont.get();
    }
    if (!categoryFont) {
        prefs->denigmaContext->logMessage(LogMsg() << "marking category " << cat->getName() << " has no usable text font.", LogSeverity::Warning);
        return;
    }
    writeFontPref(styleElement, namePrefix, categoryFont);
    if (cat->musicFont) {
        const double symbolScale = calcMusicalSymbolScale(prefs, cat->musicFont.get());
        setElementValue(styleElement, namePrefix + "MusicalSymbolsScale", symbolScale);
        setElementValue(styleElement, namePrefix + "MusicalSymbolSize", symbolScale * SYMBOLS_DEFAULT_SIZE);
    }
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
    setElementValue(styleElement, "spatium", (EVPU_PER_SPACE * prefs->spatiumScaling) / EVPU_PER_MM);

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
            auto font = verseText->getRawTextCtx(verseText, prefs->forPartId).parseFirstFontInfo();
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
    double repeatDotDistance = (prefs->repeatOptions->forwardDotHPos + prefs->repeatOptions->backwardDotHPos) / EVPU_PER_SPACE;
    if (const auto repeatDotWidth = calcRepeatDotWidth(prefs)) {
        repeatDotDistance -= repeatDotWidth.value() / EVPU_PER_SPACE;
    }
    setElementValue(styleElement, "repeatBarlineDotSeparation", repeatDotDistance * 0.5);
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
    setElementValue(styleElement, "keyBarlineDistance", (prefs->repeatOptions->afterKeySpace - 1.5 * EVPU_PER_SPACE) / EVPU_PER_SPACE); // observed fudge factor

    // Differences in how MuseScore and Finale interpret these settings mean the following are better left alone
    // setElementValue(styleElement, "systemHeaderDistance", prefs->keyOptions->keyBack / EVPU_PER_SPACE);
    // setElementValue(styleElement, "systemHeaderTimeSigDistance", prefs->timeOptions->timeBack / EVPU_PER_SPACE);

    setElementValue(styleElement, "clefBarlineDistance", -(prefs->clefOptions->clefChangeOffset) / EVPU_PER_SPACE);
    setElementValue(styleElement, "timesigBarlineDistance", (prefs->repeatOptions->afterTimeSpace - 1.5 * EVPU_PER_SPACE) / EVPU_PER_SPACE);

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
    setElementValue(styleElement, "barlineBeforeSigChange", true);
    setElementValue(styleElement, "doubleBarlineBeforeKeySig", prefs->barlineOptions->drawDoubleBarlineBeforeKeyChanges);
    setElementValue(styleElement, "doubleBarlineBeforeTimeSig", false);
    setElementValue(styleElement, "keySigNaturals", prefs->keyOptions->doKeyCancel);
    setElementValue(styleElement, "keySigShowNaturalsChangingSharpsFlats", prefs->keyOptions->doKeyCancelBetweenSharpsFlats);
    setElementValue(styleElement, "hideEmptyStaves", prefs->document->calcHasVaryingSystemStaves(prefs->forPartId));
    setElementValue(styleElement, "placeClefsBeforeRepeats", true);
    setElementValue(styleElement, "showCourtesiesRepeats", false);
    setElementValue(styleElement, "showCourtesiesOtherJumps", false);
    setElementValue(styleElement, "showCourtesiesAfterCancellingRepeats", false);
    setElementValue(styleElement, "showCourtesiesAfterCancellingOtherJumps", false);
    setElementValue(styleElement, "repeatPlayCountShow", false);
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
    setElementValue(styleElement, "barNoteDistance", prefs->musicSpacing->musFront / EVPU_PER_SPACE);
    setElementValue(styleElement, "barAccidentalDistance", prefs->musicSpacing->musFront / EVPU_PER_SPACE);
    setElementValue(styleElement, "noteBarDistance", (prefs->musicSpacing->minDistance + prefs->musicSpacing->musBack) / EVPU_PER_SPACE);
    setElementValue(styleElement, "measureSpacing", prefs->musicSpacing->scalingFactor);

    // In Finale this distance is added to the regular note spacing,
    // whereas MuseScore's value determines effective tie length.
    // Thus we set the value based on the (usually) smallest tie length: ones using inside placement.
    auto horizontalTieEndPointValue = [&](TieConnectStyleType type) -> Evpu {
        auto it = prefs->tieOptions->tieConnectStyles.find(type);
        if (it != prefs->tieOptions->tieConnectStyles.end() && it->second) {
            return it->second->offsetX;
        }
        prefs->denigmaContext->logMessage(LogMsg() << "Missing tie connect style " << int(type) << " while setting minTieLength.", LogSeverity::Warning);
        return 0;
    };
    setElementValue(styleElement, "minTieLength",
                    (prefs->musicSpacing->minDistTiedNotes + prefs->musicSpacing->minDistance
                     + (horizontalTieEndPointValue(TieConnectStyleType::OverEndPosInner)
                        - horizontalTieEndPointValue(TieConnectStyleType::OverStartPosInner)
                        + horizontalTieEndPointValue(TieConnectStyleType::UnderEndPosInner)
                        - horizontalTieEndPointValue(TieConnectStyleType::UnderStartPosInner)) / 2) / EVPU_PER_SPACE);

    // This value isn't always in used in Finale, but we can't use manual positioning.
    setElementValue(styleElement, "graceToMainNoteDist", prefs->musicSpacing->minDistGrace / EVPU_PER_SPACE);
    setElementValue(styleElement, "graceToGraceNoteDist", prefs->musicSpacing->minDistGrace / EVPU_PER_SPACE);

    setElementValue(styleElement, "articulationKeepTogether", false);
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
    const auto dotWidth = calcAugmentationDotWidth(prefs, dotMag);
    if (dotWidth) {
        setElementValue(styleElement, "dotDotDistance", (prefs->augDotOptions->dotOffset + dotWidth.value()) / EVPU_PER_SPACE);
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
    const auto& smartShapePrefs = prefs->smartShapeOptions;
    const auto& tiePrefs = prefs->tieOptions;

    // Hairpins
    setElementValue(styleElement, "hairpinHeight",
                    ((smartShapePrefs->shortHairpinOpeningWidth + smartShapePrefs->crescHeight) * 0.5) / EVPU_PER_SPACE);
    setElementValue(styleElement, "hairpinContHeight", 0.5); // Hardcoded to a half space
    writeCategoryTextFontPref(styleElement, prefs, "hairpin", others::MarkingCategory::CategoryType::Dynamics);
    writeLinePrefs(styleElement, "hairpin", smartShapePrefs->crescLineWidth, smartShapePrefs->smartDashOn, smartShapePrefs->smartDashOff);
    // Cresc. / Decresc. lines
    const double hairpinLineLineWidthEvpu = smartShapePrefs->smartLineWidth / EFIX_PER_EVPU;
    setElementValue(styleElement, "hairpinLineDashLineLen", smartShapePrefs->smartDashOn / hairpinLineLineWidthEvpu);
    setElementValue(styleElement, "hairpinLineDashGapLen", smartShapePrefs->smartDashOff / hairpinLineLineWidthEvpu);

    // Slurs
    constexpr double contourScaling = 0.5; // observed scaling factor
    constexpr double minMuseScoreEndWidth = 0.01; // MuseScore slur- and tie thickness go crazy if the endpoint thickness is zero.
    const double slurEndpointWidth = (std::max)(minMuseScoreEndWidth, smartShapePrefs->smartSlurTipWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "slurEndWidth", slurEndpointWidth);
    // Ignore horizontal thickness values as they hardly affect mid width.
    const double slurMidPointWidth = ((smartShapePrefs->slurThicknessCp1Y + smartShapePrefs->slurThicknessCp2Y) * 0.5) / EVPU_PER_SPACE;
    setElementValue(styleElement, "slurMidWidth", slurMidPointWidth * contourScaling);
    setElementValue(styleElement, "slurDottedWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE);

    // Ties
    const double tieEndpointWidth = (std::max)(minMuseScoreEndWidth, tiePrefs->tieTipWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "tieEndWidth", tieEndpointWidth);
    setElementValue(styleElement, "tieMidWidth", ((tiePrefs->thicknessRight + tiePrefs->thicknessLeft) * 0.5 * contourScaling) / EVPU_PER_SPACE);
    setElementValue(styleElement, "tieDottedWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "tiePlacementSingleNote", tiePrefs->useOuterPlacement ? "outside" : "inside");
    /// @note Finale's 'outer placement' for notes within chords is much closer to inside placement. But outside placement is closer overall.
    setElementValue(styleElement, "tiePlacementChord", tiePrefs->useOuterPlacement ? "outside" : "inside");

    // Ottavas
    setElementValue(styleElement, "ottavaHookAbove", smartShapePrefs->hookLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "ottavaHookBelow", smartShapePrefs->hookLength / EVPU_PER_SPACE);
    setElementValue(styleElement, "ottavaNumbersOnly", smartShapePrefs->showOctavaAsText);

    // Guitar bends
    setElementValue(styleElement, "guitarBendLineWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "guitarDiveLineWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE);
    setElementValue(styleElement, "bendLineWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE); // shape-dependent
    setElementValue(styleElement, "guitarBendLineWidthTab", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE); // shape-dependent
    setElementValue(styleElement, "guitarDiveLineWidthTab", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE); // shape-dependent
    setElementValue(styleElement, "guitarBendUseFull", smartShapePrefs->guitarBendUseFull);
    setElementValue(styleElement, "showFretOnFullBendRelease", !smartShapePrefs->guitarBendHideBendTo);

    // General line settings
    for (std::string_view prefix : solidLinesWithHooks) {
        const std::string prefixString(prefix);
        writeLinePrefs(styleElement, prefixString, smartShapePrefs->smartLineWidth, smartShapePrefs->smartDashOn, smartShapePrefs->smartDashOff);
        setElementValue(styleElement, prefixString + "HookHeight", smartShapePrefs->hookLength / EVPU_PER_SPACE);
    }
    for (std::string_view prefix : dashedLinesWithHooks) {
        const std::string prefixString(prefix);
        writeLinePrefs(styleElement, prefixString, smartShapePrefs->smartLineWidth, smartShapePrefs->smartDashOn, smartShapePrefs->smartDashOff, "dashed");
        setElementValue(styleElement, prefixString + "HookHeight", smartShapePrefs->hookLength / EVPU_PER_SPACE);
    }
    for (std::string_view prefix : solidLinesNoHooks) {
        writeLinePrefs(styleElement, std::string(prefix), smartShapePrefs->smartLineWidth, smartShapePrefs->smartDashOn, smartShapePrefs->smartDashOff);
    }
    setElementValue(styleElement, "noteLineWidth", smartShapePrefs->smartLineWidth / EFIX_PER_SPACE); // noteLineWidth not noteLineLineWidth
    for (std::string_view prefix : dashedLinesNoHooks) {
        writeLinePrefs(styleElement, std::string(prefix), smartShapePrefs->smartLineWidth, smartShapePrefs->smartDashOn, smartShapePrefs->smartDashOff, "dashed");
    }
}

void writeMeasureNumberPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    setElementValue(styleElement, "showMeasureNumber", bool(prefs->measNumScorePart));
    if (prefs->measNumScorePart) {
        const auto& scorePart = prefs->measNumScorePart;
        setElementValue(styleElement, "showMeasureNumberOne", !scorePart->hideFirstMeasure);
        setElementValue(styleElement, "measureNumberInterval", scorePart->incidence);
        const bool useShowOnStart = scorePart->showOnStart && !scorePart->showOnEvery;
        setElementValue(styleElement, "measureNumberSystem", useShowOnStart);

        const auto scrollView = prefs->document->getScrollViewStaves(prefs->forPartId);
        bool topOn = false;
        bool bottomOn = false;
        bool anyInteriorOn = false;
        bool allStavesOn = !scrollView.empty();
        for (std::size_t i = 0; i < scrollView.size(); ++i) {
            if (const auto staff = scrollView[i]->getStaffInstance()) {
                const bool isOn = !staff->hideMeasNums;
                allStavesOn = allStavesOn && isOn;
                if (i == 0) {
                    topOn = isOn;
                } else if (i == scrollView.size() - 1) {
                    bottomOn = isOn;
                } else if (isOn) {
                    anyInteriorOn = true;
                }
            }
        }
        const bool useAbove = scorePart->excludeOthers || (!anyInteriorOn && !bottomOn);
        const bool useBelow = scorePart->excludeOthers || (!anyInteriorOn && !topOn);
        auto placementMode = [&]() -> std::string {
            if (useAbove && scorePart->showOnTop) {
                return "above-system";
            }
            if (useBelow && scorePart->showOnBottom) {
                return "below-system";
            }
            if (allStavesOn) {
                return "on-all-staves";
            }
            if (scorePart->showOnBottom) {
                prefs->denigmaContext->logMessage(LogMsg() << "Show on Bottom not supported when other staves also show measure numbers.",
                                                  LogSeverity::Warning);
            }
            return "on-so-staves";
        }();
        setElementValue(styleElement, "measureNumberPlacementMode", placementMode);

        auto processSegment = [&](const MusxInstance<FontInfo>& fontInfo,
                                  const others::Enclosure* enclosure,
                                  AlignJustify justification,
                                  AlignJustify alignment,
                                  Evpu horizontal,
                                  Evpu vertical,
                                  const std::string& prefix) {
            writeFontPref(styleElement, prefix, fontInfo.get());
            const double verticalSp = double(vertical) / EVPU_PER_SPACE;
            const double horizontalSp = double(horizontal) / EVPU_PER_SPACE;
            setElementValue(styleElement, prefix + "VPlacement", (vertical >= 0) ? 0 : 1);
            setElementValue(styleElement, prefix + "HPlacement", alignJustifyToHorizontalString(alignment));
            setElementValue(styleElement, prefix + "Align", alignJustifyToAlignString(justification, "baseline"));
            setElementValue(styleElement, prefix + "Position", alignJustifyToHorizontalString(justification));
            const double textHeightSp = approximateFontAscentInSpaces(fontInfo.get(), *prefs->denigmaContext) * prefs->spatiumScaling;
            const double normalStaffHeightSp = 4.0;
            setPointElement(styleElement, prefix + "PosAbove", horizontalSp, std::min(-verticalSp, 0.0));
            setPointElement(styleElement, prefix + "PosBelow", horizontalSp,
                            std::max(-(verticalSp + normalStaffHeightSp) - textHeightSp, 0.0));
            writeFramePrefs(styleElement, prefix, enclosure);
        };

        auto fontInfo = useShowOnStart ? scorePart->startFont : scorePart->multipleFont;
        auto enclosure = useShowOnStart ? scorePart->startEnclosure : scorePart->multipleEnclosure;
        auto useEnclosure = useShowOnStart ? scorePart->useStartEncl : scorePart->useMultipleEncl;
        auto justification = useShowOnStart ? scorePart->startJustify : scorePart->multipleJustify;
        auto alignment = useShowOnStart ? scorePart->startAlign : scorePart->multipleAlign;
        auto horizontal = useShowOnStart ? scorePart->startXdisp : scorePart->multipleXdisp;
        auto vertical = useShowOnStart ? scorePart->startYdisp : scorePart->multipleYdisp;

        setElementValue(styleElement, "measureNumberAlignToBarline", alignment == AlignJustify::Left);
        setElementValue(styleElement, "measureNumberOffsetType", 1);
        processSegment(fontInfo, useEnclosure ? enclosure.get() : nullptr, justification, alignment, horizontal, vertical, "measureNumber");
        processSegment(fontInfo, useEnclosure ? enclosure.get() : nullptr, justification, alignment, horizontal, vertical, "measureNumberAlternate");

        setElementValue(styleElement, "mmRestShowMeasureNumberRange", scorePart->showMmRange);
        if (scorePart->leftMmBracketChar == 0) {
            setElementValue(styleElement, "mmRestRangeBracketType", 2); // None
        } else if (scorePart->leftMmBracketChar == '(') {
            setElementValue(styleElement, "mmRestRangeBracketType", 1); // Parentheses
        } else {
            setElementValue(styleElement, "mmRestRangeBracketType", 0); // Default
        }
        processSegment(scorePart->mmRestFont,
                       nullptr,
                       scorePart->mmRestJustify,
                       scorePart->mmRestAlign,
                       scorePart->mmRestXdisp,
                       scorePart->mmRestYdisp,
                       "mmRestRange");
    }
    setElementValue(styleElement, "createMultiMeasureRests", prefs->forPartId != 0);
    setElementValue(styleElement, "minEmptyMeasures", prefs->mmRestOptions->numStart);
    setElementValue(styleElement, "minMMRestWidth", prefs->mmRestOptions->measWidth / EVPU_PER_SPACE);
    setElementValue(styleElement, "mmRestNumberPos", (prefs->mmRestOptions->numAdjY / EVPU_PER_SPACE) + 1);
    setElementValue(styleElement, "oldStyleMultiMeasureRests", prefs->mmRestOptions->useSymbols && prefs->mmRestOptions->useSymsThreshold > 1);
    setElementValue(styleElement, "mmRestOldStyleMaxMeasures", (std::max)(prefs->mmRestOptions->useSymsThreshold - 1, 0));
    setElementValue(styleElement, "mmRestOldStyleSpacing", prefs->mmRestOptions->symSpacing / EVPU_PER_SPACE);
}

void writeRepeatEndingPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    const auto& repeatOptions = prefs->repeatOptions;
    setElementValue(styleElement, "voltaLineWidth", repeatOptions->bracketLineWidth / EFIX_PER_SPACE);
    setPointElement(styleElement, "voltaPosAbove", 0.0, -repeatOptions->bracketHeight / EVPU_PER_SPACE);
    setElementValue(styleElement, "voltaHook", repeatOptions->bracketHookLen / EVPU_PER_SPACE);
    setElementValue(styleElement, "voltaLineStyle", "solid");
    writeDefaultFontPref(styleElement, prefs, "volta", options::FontOptions::FontType::Ending);
    setElementValue(styleElement, "voltaAlign", "left,baseline");
    setPointElement(styleElement, "voltaOffset",
                    repeatOptions->bracketTextHPos / EVPU_PER_SPACE,
                    (repeatOptions->bracketHookLen - repeatOptions->bracketTextHPos) / EVPU_PER_SPACE);

    // This option actually moves the front of the volta after the repeat forwards.
    // Finale only has the option to move the end of the volta before the repeat backwards, so we leave this unset.
    // setElementValue(styleElement, "voltaAlignEndLeftOfBarline", false);
}

void writeTupletPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using TupletOptions = options::TupletOptions;
    const auto& tupletOptions = prefs->tupletOptions;

    setElementValue(styleElement, "tupletOutOfStaff", tupletOptions->avoidStaff);
    // tupletNumberRythmicCenter and tupletExtendToEndOfDuration were added in MuseScore 4.6.
    setElementValue(styleElement, "tupletNumberRythmicCenter", tupletOptions->metricCenter); // 4.6 setting
    setElementValue(styleElement, "tupletExtendToEndOfDuration", tupletOptions->fullDura); // 4.6 setting
    setElementValue(styleElement, "tupletStemLeftDistance", tupletOptions->leftHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletStemRightDistance", tupletOptions->rightHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletNoteLeftDistance", tupletOptions->leftHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletNoteRightDistance", tupletOptions->rightHookExt / EVPU_PER_SPACE);
    setElementValue(styleElement, "tupletBracketWidth", tupletOptions->tupLineWidth / EFIX_PER_SPACE);
    // manualSlopeAdj does not translate well, so else leave value as default
    if (tupletOptions->alwaysFlat) {
        setElementValue(styleElement, "tupletMaxSlope", 0.0);
    }

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
        prefs->denigmaContext->logMessage(LogMsg() << "Unable to load font pref for tuplets", LogSeverity::Warning);
        return;
    }

    if (fontInfo->calcIsSMuFL()) {
        setElementValue(styleElement, "tupletMusicalSymbolsScale", museMagVal(prefs, options::FontOptions::FontType::Tuplet));
        setElementValue(styleElement, "tupletUseSymbols", true);
    } else {
        writeFontPref(styleElement, "tuplet", fontInfo.get());
        setElementValue(styleElement, "tupletMusicalSymbolsScale", 1.0);
        setElementValue(styleElement, "tupletUseSymbols", false);
    }

    setElementValue(styleElement, "tupletBracketHookHeight",
                    -(std::max)(tupletOptions->leftHookLen, tupletOptions->rightHookLen) / EVPU_PER_SPACE); /// or use average
}

void writeMarkingPrefs(XmlElement& styleElement, const FinalePreferencesPtr& prefs)
{
    using FontType = options::FontOptions::FontType;
    using CategoryType = others::MarkingCategory::CategoryType;

    auto cat = prefs->document->getOthers()->get<others::MarkingCategory>(prefs->forPartId, Cmper(CategoryType::Dynamics));
    if (!cat) {
        throw std::invalid_argument("unable to find MarkingCategory for dynamics");
    }
    if (auto catFontInfo = cat->musicFont) {
        const bool catFontIsSMuFL = catFontInfo->calcIsSMuFL();
        const bool override = catFontIsSMuFL && !catFontInfo->calcIsDefaultMusic();
        setElementValue(styleElement, "dynamicsOverrideFont", override);
        if (override) {
            setElementValue(styleElement, "dynamicsFont", catFontInfo->getName());
            setElementValue(styleElement, "dynamicsSize", double(catFontInfo->fontSize) / double(prefs->defaultMusicFont->fontSize));
        } else if (!prefs->musicFontName.empty()) {
            setElementValue(styleElement, "dynamicsFont", prefs->musicFontName);
            setElementValue(styleElement, "dynamicsSize", double(catFontInfo->fontSize) / double(prefs->defaultMusicFont->fontSize));
        }
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
    setElementValue(styleElement, "longInstrumentAlign", alignJustifyToAlignString(fullPosition->justify, "center"));
    setElementValue(styleElement, "longInstrumentPosition", alignJustifyToHorizontalString(fullPosition->hAlign));
    writeDefaultFontPref(styleElement, prefs, "shortInstrument", FontType::AbbrvStaffNames);
    const auto abbreviatedPosition = prefs->staffOptions->namePosAbbrv;
    if (!abbreviatedPosition) {
        throw std::invalid_argument("unable to find default abbreviated name positioning for staves");
    }
    setElementValue(styleElement, "shortInstrumentAlign", alignJustifyToAlignString(abbreviatedPosition->justify, "center"));
    setElementValue(styleElement, "shortInstrumentPosition", alignJustifyToHorizontalString(abbreviatedPosition->hAlign));
    writeDefaultFontPref(styleElement, prefs, "partInstrument", FontType::StaffNames);
    writeDefaultFontPref(styleElement, prefs, "tabFretNumber", FontType::Tablature);
    writeCategoryTextFontPref(styleElement, prefs, "dynamics", CategoryType::Dynamics);
    writeCategoryTextFontPref(styleElement, prefs, "expression", CategoryType::ExpressiveText);
    writeCategoryTextFontPref(styleElement, prefs, "tempo", CategoryType::TempoMarks);
    writeCategoryTextFontPref(styleElement, prefs, "tempoChange", CategoryType::TempoAlterations);
    writeLinePrefs(styleElement, "tempoChange", prefs->smartShapeOptions->smartLineWidth, prefs->smartShapeOptions->smartDashOn, prefs->smartShapeOptions->smartDashOff, "dashed");
    writeCategoryTextFontPref(styleElement, prefs, "metronome", CategoryType::TempoMarks);
    setElementValue(styleElement, "translatorFontFace", textBlockFont->getName());
    writeCategoryTextFontPref(styleElement, prefs, "systemText", CategoryType::ExpressiveText);
    writeCategoryTextFontPref(styleElement, prefs, "staffText", CategoryType::TechniqueText);
    writeCategoryTextFontPref(styleElement, prefs, "rehearsalMark", CategoryType::RehearsalMarks);
    writeDefaultFontPref(styleElement, prefs, "repeatLeft", FontType::Repeat);
    writeDefaultFontPref(styleElement, prefs, "repeatRight", FontType::Repeat);
    writeDefaultFontPref(styleElement, prefs, "repeatPlayCount", FontType::Repeat);
    writeDefaultFontPref(styleElement, prefs, "chordSymbolA", FontType::Chord);
    writeDefaultFontPref(styleElement, prefs, "chordSymbolB", FontType::Chord);
    writeDefaultFontPref(styleElement, prefs, "nashvilleNumber", FontType::Chord);
    writeDefaultFontPref(styleElement, prefs, "romanNumeral", FontType::Chord);
    writeDefaultFontPref(styleElement, prefs, "ottava", FontType::SmartShape8va);
    setElementValue(styleElement, "fretMag", prefs->chordOptions->fretPercent / 100.0);
    setElementValue(styleElement, "chordSymPosition",
                    prefs->chordOptions->chordAlignment == options::ChordOptions::ChordAlignment::Left ? "left" : "center");
    setElementValue(styleElement, "barreAppearanceSlur", true); // Not detectable (uses shapes), but default in most templates
    // setElementValue(styleElement, "verticallyAlignChordSymbols", false); // Otherwise offsets are not accounted for
    auto chordSpellingFromStyle = [](options::ChordOptions::ChordStyle style) {
        switch (style) {
            case options::ChordOptions::ChordStyle::German:
                return 2; // NoteSpellingType::GERMAN_PURE
            case options::ChordOptions::ChordStyle::Scandinavian:
                return 1; // NoteSpellingType::GERMAN
            default:
                return 0; // NoteSpellingType::STANDARD
        }
    };
    setElementValue(styleElement, "chordSymbolSpelling", chordSpellingFromStyle(prefs->chordOptions->chordStyle));
    writeFontPref(styleElement, "frame", textBlockFont.get());
    for (std::string_view prefix : solidLinesWithHooks) {
        writeFontPref(styleElement, std::string(prefix), textBlockFont.get());
    }
    for (std::string_view prefix : dashedLinesWithHooks) {
        writeFontPref(styleElement, std::string(prefix), textBlockFont.get());
    }
    for (std::string_view prefix : solidLinesNoHooks) {
        writeFontPref(styleElement, std::string(prefix), textBlockFont.get());
    }
    writeFontPref(styleElement, "bend", textBlockFont.get());
    writeFontPref(styleElement, "header", textBlockFont.get());
    writeFontPref(styleElement, "footer", textBlockFont.get());
    writeFontPref(styleElement, "copyright", textBlockFont.get());
    writeFontPref(styleElement, "pageNumber", textBlockFont.get());
    writeFontPref(styleElement, "instrumentChange", textBlockFont.get());
    writeFontPref(styleElement, "sticking", textBlockFont.get());
    writeFontPref(styleElement, "fingering", textBlockFont.get());
    for (int i = 1; i <= 12; ++i) {
        writeFontPref(styleElement, "user" + std::to_string(i), textBlockFont.get());
    }
}

static void processPart(const std::filesystem::path& outputPath, const DocumentPtr& document, const DenigmaContext& denigmaContext, const MusxInstance<others::PartDefinition>& part = nullptr)
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
        qualifiedOutputPath.replace_extension(utils::stringToUtf8(partName) + currExtension.u8string());
    }
    if (!denigmaContext.validatePathsAndOptions(qualifiedOutputPath)) return;

    const Cmper forPartId = part ? part->getCmper() : 0;
    auto prefs = getCurrentPrefs(document, forPartId, denigmaContext);

    // extract document to mss
    XmlDocument mssDoc; // output
    auto declaration = mssDoc.append_child(::pugi::node_declaration);
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

void convert(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext)
{
#ifdef DENIGMA_TEST
    if (denigmaContext.forTestOutput()) {
        denigmaContext.logMessage(LogMsg() << "Converting to " << utils::asUtf8Bytes(outputPath));
        return;
    }
#endif

    auto document = DocumentFactory::create<MusxReader>(inputData.primaryBuffer);
    if (denigmaContext.allPartsAndScore || !denigmaContext.partName.has_value()) {
        processPart(outputPath, document, denigmaContext); // process the score
    }
    bool foundPart = false;
    if (denigmaContext.allPartsAndScore || denigmaContext.partName.has_value()) {
        auto parts = document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
        for (const auto& part : parts) {
            if (part->getCmper() != SCORE_PARTID) {
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
