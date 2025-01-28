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
#include <unordered_map>

#include "mnx.h"

#include "nlohmann/json.hpp"

namespace denigma {
namespace mnx {

void createLayouts(const MnxMusxMappingPtr& context)
{
    auto& mnxDocument = context->mnxDocument;
    auto musxLinkedParts = context->document->getOthers()->getArray<others::PartDefinition>(SCORE_PARTID);
    std::sort(musxLinkedParts.begin(), musxLinkedParts.end(), [](const auto& lhs, const auto& rhs) {
        return lhs->partOrder < rhs->partOrder;
    });
    for (const auto& linkedPart : musxLinkedParts) {
        auto layout = json::object();
        layout["id"] = scrollViewLayoutId(linkedPart->getCmper());
        auto groups = context->document->getDetails()->getArray<details::StaffGroup>(linkedPart->getCmper(), linkedPart->calcScrollViewIuList());
        auto scrollViewStaves = context->document->getOthers()->getArray<others::InstrumentUsed>(linkedPart->getCmper(), linkedPart->calcScrollViewIuList());
        layout["content"] = json::array();
        if (mnxDocument["layouts"].empty()) {
            mnxDocument["layouts"] = json::array();
        }
        mnxDocument["layouts"].emplace_back(layout);
    }
}

} // namespace mnx
} // namespace denigma
