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

#include <filesystem>
#include <optional>

#include "denigma.h"
#include "musx/musx.h"
#include "mnxdom.h"

 //placeholder function

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace mnxexp {

using json = nlohmann::ordered_json;
//using json = nlohmann::json;

struct MnxMusxMapping
{
    MnxMusxMapping(const DenigmaContext& context, const DocumentPtr& doc)
        : denigmaContext(&context), document(doc), mnxDocument() {}

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    std::unique_ptr<mnx::Document> mnxDocument;

    std::unordered_map<std::string, std::vector<InstCmper>> part2Inst;
    std::unordered_map<InstCmper, std::string> inst2Part;
};
using MnxMusxMappingPtr = std::shared_ptr<MnxMusxMapping>;

inline std::string calcSystemLayoutId(Cmper partId, Cmper systemId)
{
    if (systemId == BASE_SYSTEM_ID) {
        return "S" + std::to_string(partId) + "-ScrVw";
    }
    return "S" + std::to_string(partId) + "-Sys" + std::to_string(systemId);
}

void createLayouts(const MnxMusxMappingPtr& context);
void createGlobal(const MnxMusxMappingPtr& context);
void convert(const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext);

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum value);

} // namespace mnxexp
} // namespace denigma
