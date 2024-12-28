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

// NOTE: This namespace is necessary because zip_file.hpp is poorly implemented and
//          can only be included once in the entire project.

namespace ziputils {

/**
 * @brief Reads a specific filename from the input zip archive.
 * @param zipFilePath [in] the zip archive to search.
 * @param fileName [in] the file name to search for within the archive.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 */
std::string readFile(const std::filesystem::path& zipFilePath, const std::string& fileName, const denigma::DenigmaContext& denigmaContext);

/**
 * @brief Iterates through each filename in the input zip archive.
 * @param zipFilePath [in] the zip archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 * @param iterator an iterator function that feeds the next file name. Return `false` from this function to stop iterating.
 */
void iterateFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, std::function<bool(const std::string&)> iterator);

/**
 * @brief Iterates through each filename in the input zip archive.
 * @param zipFilePath [in] the zip archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 * @param searchFileName [in] only feed this file name.
 * @param iterator an iterator function that feeds the contents of the next file. Return `false` from this function to stop iterating.
 */
void iterateFiles(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext, const std::string& searchFileName, std::function<bool(const std::string&)> iterator);

/**
 * @brief Finds and returns the score file from a compressed MusicXml file.
 * @param zipFilePath [in] the compressed MusicXml archive to search.
 * @param denigmaContext [in] the DenigmaContext (for logging).
 */
std::string getMusicXmlRootFile(const std::filesystem::path& zipFilePath, const denigma::DenigmaContext& denigmaContext);

} // namespace ziputils
