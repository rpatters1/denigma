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
struct FinalePrefences
{
    DocumentPtr document;
    std::shared_ptr<FontInfo> defaultMusicFont;
    std::shared_ptr<options::BarlineOptions> barlineOptions;
    std::shared_ptr<options::ClefOptions> clefOptions;
    std::shared_ptr<options::LineCurveOptions> lineCurveOptions;
    std::shared_ptr<options::PageFormatOptions::PageFormat> pageFormat;
    std::shared_ptr<options::RepeatOptions> repeatOptions;
};
using FinalePrefencesPtr = std::shared_ptr<FinalePrefences>;

template <typename T>
static std::shared_ptr<T> getDocOptions(const std::shared_ptr<FinalePrefences>& prefs, const std::string& prefsName)
{
    auto retval = prefs->document->getOptions()->get<T>();
    if (!retval) {
        throw std::invalid_argument("document contains no default " + prefsName + " options");
    }
    return retval;
}

static FinalePrefencesPtr getCurrentPrefs(const enigmaxml::Buffer& xmlBuffer)
{
    auto retval = std::make_shared<FinalePrefences>();
    retval->document = musx::factory::DocumentFactory::create<musx::xml::rapidxml::Document>(xmlBuffer);

    auto defaultFonts = getDocOptions<options::DefaultFonts>(retval, "font");
    retval->defaultMusicFont = defaultFonts->getFontInfo(options::DefaultFonts::FontType::Music);
    retval->barlineOptions = getDocOptions<options::BarlineOptions>(retval, "barline");
    retval->clefOptions = getDocOptions<options::ClefOptions>(retval, "clef");
    retval->lineCurveOptions = getDocOptions<options::LineCurveOptions>(retval, "lines & curves");
    auto pageFormatOptions = getDocOptions<options::PageFormatOptions>(retval, "page format");
    retval->pageFormat = pageFormatOptions->pageFormatScore; // ToDo: check for score or part
    retval->repeatOptions = getDocOptions<options::RepeatOptions>(retval, "repeat");

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

static double museMagVal(const FinalePrefencesPtr& prefs, const options::DefaultFonts::FontType type)
{
    auto fontPrefs = options::DefaultFonts::getFontInfo(prefs->document, type);
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

static void writeDefaultFontPref(XmlElement* styleElement, const FinalePrefencesPtr& prefs, const std::string& namePrefix, options::DefaultFonts::FontType type)
{
    auto fontPrefs = options::DefaultFonts::getFontInfo(prefs->document, type);
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

static void writeCategoryTextFontPref(XmlElement* styleElement, const FinalePrefencesPtr& prefs, const std::string& namePrefix, Cmper categoryId)
{
    auto cat = prefs->document->getOthers()->get<others::MarkingCategory>(categoryId);
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

static void writePagePrefs(XmlElement* styleElement, const FinalePrefencesPtr& prefs)
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

static void writeLyricsPrefs(XmlElement* styleElement, const FinalePrefencesPtr& prefs)
{
    auto fontInfo = options::DefaultFonts::getFontInfo(prefs->document, options::DefaultFonts::FontType::LyricVerse);
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

#if 0
void writeLineMeasurePrefs(XmlElement& styleElement, const FinalePrefencesPtr& prefs, bool currentIsPart)
{
    using RepeatWingStyle = options::RepeatOptions::WingStyle;

    setElementText(styleElement, "barWidth", prefs->barlineOptions->barlineWidth / EFIX_PER_SPACE);
    setElementText(styleElement, "doubleBarWidth", prefs->barlineOptions->barlineWidth / EFIX_PER_SPACE);
    setElementText(styleElement, "endBarWidth", prefs->barlineOptions->thickBarlineWidth / EFIX_PER_SPACE);

    // Finale's double bar distance is measured from the beginning of the thin line
    setElementText(styleElement, "doubleBarDistance", 
        (prefs->barlineOptions->doubleBarlineSpace - prefs->barlineOptions->barlineWidth) / EFIX_PER_SPACE);

    // Finale's final bar distance is the separation amount
    setElementText(styleElement, "endBarDistance", prefs->barlineOptions->finalBarlineSpace / EFIX_PER_SPACE);
    setElementText(styleElement, "repeatBarlineDotSeparation", prefs->repeatOptions->forwardDotHPos / EVPU_PER_SPACE);
    setElementText(styleElement, "repeatBarTips", prefs->repeatOptions->wingStyle != RepeatWingStyle::None);

    setElementText(styleElement, "startBarlineSingle", prefs->barlineOptions->drawLeftBarlineSingleStaff);
    setElementText(styleElement, "startBarlineMultiple", prefs->barlineOptions->drawLeftBarlineMultipleStaves);

    setElementText(styleElement, "bracketWidth", 0.5); // Hard-coded in Finale
    setElementText(styleElement, "bracketDistance", 
        -prefs->clefOptions->groupBracketDefaultDistance / EVPU_PER_SPACE);
    setElementText(styleElement, "akkoladeBarDistance", 
        -prefs->clefOptions->groupBracketDefaultDistance / EVPU_PER_SPACE);

    setElementText(styleElement, "clefLeftMargin", prefs->clefOptions->clefFrontSepar / EVPU_PER_SPACE);
    setElementText(styleElement, "keysigLeftMargin", prefs->clefOptions->keySpaceBefore / EVPU_PER_SPACE);

    double timeSigSpaceBefore = currentIsPart ? 
        prefs->clefOptions->timeSigPartsSpaceBefore : prefs->clefOptions->timeSigSpaceBefore;
    setElementText(styleElement, "timesigLeftMargin", timeSigSpaceBefore / EVPU_PER_SPACE);

    setElementText(styleElement, "clefKeyDistance", 
        (prefs->clefOptions->clefBackSepar + prefs->clefOptions->clefKeySepar + prefs->clefOptions->keySpaceBefore) / EVPU_PER_SPACE);
    setElementText(styleElement, "clefTimesigDistance", 
        (prefs->clefOptions->clefBackSepar + prefs->clefOptions->clefTimeSepar + timeSigSpaceBefore) / EVPU_PER_SPACE);
    setElementText(styleElement, "keyTimesigDistance", 
        (prefs->clefOptions->keySpaceAfter + prefs->clefOptions->keyTimeExtraSpace + timeSigSpaceBefore) / EVPU_PER_SPACE);
    setElementText(styleElement, "keyBarlineDistance", prefs->repeatOptions->afterKeySpace / EVPU_PER_SPACE);

    // Differences in how MuseScore and Finale interpret these settings mean the following are better left alone
    // setElementText(styleElement, "systemHeaderDistance", prefs->clefOptions->keySpaceAfter / EVPU_PER_SPACE);
    // setElementText(styleElement, "systemHeaderTimeSigDistance", prefs->clefOptions->timeSigSpaceAfter / EVPU_PER_SPACE);

    setElementText(styleElement, "clefBarlineDistance", prefs->repeatOptions->afterClefSpace / EVPU_PER_SPACE);
    setElementText(styleElement, "timesigBarlineDistance", prefs->repeatOptions->afterClefSpace / EVPU_PER_SPACE);

    setElementText(styleElement, "measureRepeatNumberPos", 
        -(prefs->repeatOptions->bracketTextVPos + 0.5) / EVPU_PER_SPACE);
    setElementText(styleElement, "staffLineWidth", prefs->lineCurveOptions->staffLineWidth / EFIX_PER_SPACE);
    setElementText(styleElement, "ledgerLineWidth", prefs->lineCurveOptions->legerLineWidth / EFIX_PER_SPACE);
    setElementText(styleElement, "ledgerLineLength", 
        (prefs->lineCurveOptions->legerFrontLength + prefs->lineCurveOptions->legerBackLength) / (2 * EVPU_PER_SPACE));
    setElementText(styleElement, "keysigAccidentalDistance", 
        (prefs->clefOptions->keySpaceBetweenAccidentals + 4) / EVPU_PER_SPACE); // Observed fudge factor
    setElementText(styleElement, "keysigNaturalDistance", 
        (prefs->clefOptions->keySpaceBetweenAccidentals + 6) / EVPU_PER_SPACE); // Observed fudge factor

    setElementText(styleElement, "smallClefMag", prefs->clefOptions->clefChangePercent / 100);
    setElementText(styleElement, "genClef", !prefs->clefOptions->showClefFirstSystemOnly);
    setElementText(styleElement, "genKeysig", !prefs->clefOptions->keySigOnlyFirstSystem);
    setElementText(styleElement, "genCourtesyTimesig", prefs->clefOptions->courtesyTimeSigAtSystemEnd);
    setElementText(styleElement, "genCourtesyKeysig", prefs->clefOptions->courtesyKeySigAtSystemEnd);
    setElementText(styleElement, "genCourtesyClef", prefs->clefOptions->cautionaryClefChanges);

    setElementText(styleElement, "keySigCourtesyBarlineMode", prefs->barlineOptions->drawDoubleBarlineBeforeKeyChanges);
    setElementText(styleElement, "timeSigCourtesyBarlineMode", 0); // Hard-coded as 0 in Lua
    setElementText(styleElement, "hideEmptyStaves", !currentIsPart);
}
#endif

void convert(const std::filesystem::path& outputPath, const enigmaxml::Buffer& xmlBuffer)
{
    // ToDo: lots
    try {
        // construct source instance and release input memory
        auto prefs = getCurrentPrefs(xmlBuffer);

        // extract document to mss
        XmlDocument mssDoc; // output
        mssDoc.InsertEndChild(mssDoc.NewDeclaration());
        auto museScoreElement = mssDoc.NewElement("museScore");
        museScoreElement->SetAttribute("version", MSS_VERSION);
        mssDoc.InsertEndChild(museScoreElement);
        auto styleElement = museScoreElement->InsertNewChildElement("Style");
        writePagePrefs(styleElement, prefs);
        writeLyricsPrefs(styleElement, prefs);
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
