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

#include "mss.h"
#include "musx/musx.h"
#include "tinyxml2.h"

namespace musxconvert {
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

// Finale preferences:
struct FinalePrefences
{
    DocumentPtr document;
    std::shared_ptr<FontInfo> defaultMusicFont;
};
using FinalePrefencesPtr = std::shared_ptr<FinalePrefences>;

static FinalePrefencesPtr getCurrentPrefs(const enigmaxml::Buffer& xmlBuffer)
{
    auto retval = std::make_shared<FinalePrefences>();
    retval->document = musx::factory::DocumentFactory::create<musx::xml::rapidxml::Document>(xmlBuffer);

    auto defaultFonts = retval->document->getOptions()->get<options::DefaultFonts>();
    retval->defaultMusicFont = defaultFonts->getFontInfo(options::DefaultFonts::FontType::Music);

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
} // namespace musxconvert
