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
#pragma once

#include <string>
#include <filesystem>
#include <functional>

#include "denigma.h"

namespace utils {

struct MusxArchiveFiles
{
    std::string scoreDat;
    std::optional<std::string> notationMetadata;
};

/**
 * @brief Reads a specific filename from the input zip archive.
 * @param zipFilePath [in] the zip archive to search.
 * @param fileName [in] the utf8-encoded file name to search for within the archive.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 */
std::string readFile(const std::filesystem::path& zipFilePath, const std::string& fileName, const denigma::DenigmaContext& denigmaContext);
MusxArchiveFiles readMusxArchiveFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext);

/// @brief iterator func that feeds the next filename and xmldata
using IteratorFunc = std::function<bool(const std::filesystem::path& fileName, const std::string& xmlData)>;

/**
 * @brief Finds and returns the score file from a compressed MusicXml file.
 * @param zipFilePath [in] the compressed MusicXml archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 */
std::string getMusicXmlScoreFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext);

/**
 * @brief Iterates through each music xml part file in a compressed MusicXml file. (The score is skipped.)
 * @param zipFilePath [in] the compressed MusicXml archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 * @param onlyFileName [in] if this has a value, only this name feeds to the iterator. It should be encoded utf-8.
 * @param iterator an iterator function that feeds the next filename and xmldata. Return `false` from this function to stop iterating.
 */
bool iterateMusicXmlPartFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, const std::optional<std::string>& fileName, IteratorFunc iterator);

using ModifyIteratorFunc = std::function<bool(const std::filesystem::path& fileName, std::string& fileContents, bool isScore)>;

/**
 * @brief Iterates through every file in a zip archive to create a modified zip archive.
 * @param zipFilePath [in] the compressed MusicXml archive to search.
 * @param outputPath [in] the compressed MusicXml archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 * @param iterator an iterator function that feeds the next filename and xmldata. You can modify the xmldata. You can skip a file by returning false.
 */
bool iterateModifyFilesInPlace(const std::filesystem::path& zipFilePath, const std::filesystem::path& outputPath, const denigma::DenigmaContext& denigmaContext, ModifyIteratorFunc iterator);

} // namespace utils
