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
#pragma once

#include "denigma/classify/dynamics.h"
#include "musx/musx.h"

#include <span>
#include <vector>

namespace denigma::classify {

using ExpressionCategoryType = musx::dom::others::MarkingCategory::CategoryType;

inline ExpressionCategoryType categoryTypeFromId(musx::dom::Cmper categoryId)
{
    switch (static_cast<ExpressionCategoryType>(categoryId)) {
    case ExpressionCategoryType::Invalid:
    case ExpressionCategoryType::Dynamics:
    case ExpressionCategoryType::TempoMarks:
    case ExpressionCategoryType::TempoAlterations:
    case ExpressionCategoryType::ExpressiveText:
    case ExpressionCategoryType::TechniqueText:
    case ExpressionCategoryType::RehearsalMarks:
    case ExpressionCategoryType::Misc:
        return static_cast<ExpressionCategoryType>(categoryId);
    }
    return ExpressionCategoryType::Misc;
}

namespace detail {

struct DynamicSpan
{
    std::span<const char> sourceText;
    DynamicMark mark;
};

std::vector<DynamicSpan> findDynamicSpans(const musx::util::EnigmaTextChunk& chunk);

} // namespace detail

} // namespace denigma::classify
