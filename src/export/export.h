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

#include "denigma.h"

namespace denigma {

struct ExportCommand : public ICommand
{
    using ICommand::ICommand;

    int showHelpPage(const std::string_view& programName, const std::string& indentSpaces = {}) const override;
    
    bool canProcess(const std::filesystem::path& inputPath) const override;
    CommandInputData processInput(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext) const override;
    void processOutput(const CommandInputData& inputData, const std::filesystem::path& outputPath, const std::filesystem::path&, const DenigmaContext& denigmaContext) const override;

    std::optional<std::u8string_view> defaultInputFormat() const override { return MUSX_EXTENSION; };
    std::optional<std::u8string> defaultOutputFormat(const std::filesystem::path& inputPath) const override
    {
        if (utils::pathExtensionEquals(inputPath, ENIGMAXML_EXTENSION)) {
            return MUSX_EXTENSION;
        }
        return ENIGMAXML_EXTENSION;
    };
    const std::string_view commandName() const override { return "export"; }
};

} // namespace denigma
