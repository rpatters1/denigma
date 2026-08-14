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
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "mx/api/ScoreData.h"

namespace denigma::test::musicxml {

std::filesystem::path exportMusicXmlFixture(const std::string& musxFile, bool useFinaleRestPosition = false);
std::optional<mx::api::ScoreData> createScoreDataFromMusicXmlFixture(const std::string& musxFile);
std::optional<mx::api::ScoreData> createScoreDataFromMusxPath(const std::filesystem::path& musxPath);
std::optional<mx::api::ScoreData> loadScoreData(const std::filesystem::path& path);

/// @brief Returns where a direction is drawn after applying its optional MusicXML offset.
inline int directionDrawnTick(const mx::api::DirectionData& direction)
{
    return direction.tickTimePosition + direction.offset.value_or(0);
}

/// @brief Renders a direction's words runs as one string, showing each symbol as {glyphName}.
///
/// Music-font characters export as `<symbol>` rather than text, so a test that only collects words
/// would silently see a marking with its glyph missing. This keeps the glyph visible and asserted.
inline std::string directionRunText(const mx::api::DirectionData& direction)
{
    std::string result;
    for (const auto& choice : direction.directionTypes) {
        if (!choice.isWordsRun()) {
            continue;
        }
        for (const auto& item : choice.wordsRun()) {
            if (item.isWords()) {
                result += item.words().text;
            } else if (item.isSymbol()) {
                result += "{" + item.symbol().smufl + "}";
            }
        }
    }
    return result;
}

/// @brief Returns the position of a direction's first words-run item, whether words or symbol.
///
/// Every item of a run carries the same position, since the exporters apply it through
/// forEachMusicXmlWordsRunItem. Reading it from the first item rather than the first `<words>` keeps
/// a run that opens with a glyph from looking positionless.
inline std::optional<mx::api::PositionData> directionRunPosition(const mx::api::DirectionData& direction)
{
    for (const auto& choice : direction.directionTypes) {
        if (!choice.isWordsRun()) {
            continue;
        }
        for (const auto& item : choice.wordsRun()) {
            if (item.isWords()) {
                return item.words().positionData;
            }
            if (item.isSymbol()) {
                return item.symbol().positionData;
            }
        }
    }
    return std::nullopt;
}

inline std::vector<mx::api::WordsData> directionWords(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::WordsData> result;
    for (const auto& choice : direction.directionTypes) {
        if (!choice.isWordsRun()) {
            continue;
        }
        for (const auto& item : choice.wordsRun()) {
            if (item.isWords()) {
                result.emplace_back(item.words());
            }
        }
    }
    return result;
}

inline std::vector<mx::api::RehearsalData> directionRehearsals(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::RehearsalData> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isRehearsal()) {
            result.emplace_back(choice.rehearsal());
        }
    }
    return result;
}

inline std::vector<mx::api::MarkData> directionMarks(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::MarkData> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isMark()) {
            result.emplace_back(choice.mark());
        }
    }
    return result;
}

inline std::vector<mx::api::OttavaStart> directionOttavaStarts(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::OttavaStart> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isOttavaStart()) {
            result.emplace_back(choice.ottavaStart());
        }
    }
    return result;
}

inline std::vector<mx::api::OttavaStop> directionOttavaStops(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::OttavaStop> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isOttavaStop()) {
            result.emplace_back(choice.ottavaStop());
        }
    }
    return result;
}

inline std::vector<mx::api::SpannerStart> directionBracketStarts(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::SpannerStart> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isBracketStart()) {
            result.emplace_back(choice.bracketStart());
        }
    }
    return result;
}

inline std::vector<mx::api::SpannerStop> directionBracketStops(const mx::api::DirectionData& direction)
{
    std::vector<mx::api::SpannerStop> result;
    for (const auto& choice : direction.directionTypes) {
        if (choice.isBracketStop()) {
            result.emplace_back(choice.bracketStop());
        }
    }
    return result;
}

} // namespace denigma::test::musicxml
