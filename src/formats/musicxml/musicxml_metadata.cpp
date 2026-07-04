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

#include <string>
#include <utility>

#include "musicxml.h"
#include "utils/stringutils.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

using FileInfoText = texts::FileInfoText;

std::string fileInfoTextValue(const MusxInstance<FileInfoText>& fileInfoText)
{
    return musx::util::EnigmaString::trimTags(fileInfoText->text);
}

bool hasValidDate(const header::FileInfo& fileInfo)
{
    return fileInfo.year > 0 && fileInfo.month >= 1 && fileInfo.month <= 12 && fileInfo.day >= 1 && fileInfo.day <= 31;
}

std::string formatDate(const header::FileInfo& fileInfo)
{
    return std::to_string(fileInfo.year) + "-"
        + (fileInfo.month < 10 ? "0" : "") + std::to_string(fileInfo.month) + "-"
        + (fileInfo.day < 10 ? "0" : "") + std::to_string(fileInfo.day);
}

void addMiscellaneousField(mx::api::EncodingData& encoding, std::string key, std::string value)
{
    if (!value.empty()) {
        encoding.miscelaneousFields.emplace_back(std::move(key), std::move(value));
    }
}

void setFileInfoText(mx::api::ScoreData& score, FileInfoText::TextType textType, std::string value)
{
    if (value.empty()) {
        return;
    }

    switch (textType) {
    case FileInfoText::TextType::Title:
        score.workTitle = std::move(value);
        break;
    case FileInfoText::TextType::Composer:
        score.composer = std::move(value);
        break;
    case FileInfoText::TextType::Copyright:
        score.copyright = std::move(value);
        break;
    case FileInfoText::TextType::Description:
        addMiscellaneousField(score.encoding, "description", std::move(value));
        break;
    case FileInfoText::TextType::Lyricist:
        score.lyricist = std::move(value);
        break;
    case FileInfoText::TextType::Arranger:
        score.arranger = value;
        addMiscellaneousField(score.encoding, "arranger", std::move(value));
        break;
    case FileInfoText::TextType::Subtitle:
        break;
    }
}

} // namespace

void createMetaData(const MusicXmlMusxMapping& context)
{
    auto& score = *context.musicXmlScore;
    score.workTitle = "Hello World";
    score.encoding.software.push_back(std::string(DENIGMA_NAME) + " " + DENIGMA_VERSION);
    score.encoding.encodingDate = mx::api::EncodingDate::today();
    const auto addSupportedElement = [&score](std::string elementName, bool supported = true) {
        auto& item = score.encoding.supportedItems.emplace_back();
        item.elementName = std::move(elementName);
        item.isSupported = supported;
    };
    /// @todo (possibly) Add an export option to use accidental support="yes" to allow for differences in importer
    /// behavior around this issue.
    addSupportedElement("accidental", false);
    addSupportedElement("beam");
    addSupportedElement("stem", false);

    if (const auto header = context.document->getHeader()) {
        if (hasValidDate(header->created)) {
            addMiscellaneousField(score.encoding, "original-creation-date", formatDate(header->created));
        }
    }

    const MusxInstanceList<FileInfoText> fileInfoTexts = context.document->getTexts()->getArray<FileInfoText>();
    for (const MusxInstance<FileInfoText>& fileInfoText : fileInfoTexts) {
        setFileInfoText(score, fileInfoText->getTextType(), fileInfoTextValue(fileInfoText));
    }

    addMiscellaneousField(score.encoding, "denigma-name", DENIGMA_NAME);
    addMiscellaneousField(score.encoding, "denigma-version", DENIGMA_VERSION);
    addMiscellaneousField(score.encoding, "denigma-commit", gitCommit());

    const auto& inputFilePath = context.denigmaContext->inputFilePath;
    const std::u8string sourceFormat = utils::normalizedPathExtension(inputFilePath);
    addMiscellaneousField(score.encoding, "source-format",
        sourceFormat.empty() ? std::string("unknown") : utils::utf8ToString(sourceFormat));
    if (!inputFilePath.empty()) {
        addMiscellaneousField(score.encoding, "source-filename", utils::pathToString(inputFilePath.filename()));
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
