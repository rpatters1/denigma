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

#include <stdexcept>
#include <string>

// These macros must be expanded in the namespace where enumConvert is declared
// and where its specializations should live.
#define DEFINE_ENUM_CONVERT_TEMPLATE \
template <typename ToEnum, typename FromEnum> \
ToEnum enumConvert(FromEnum) \
{ \
    static_assert(sizeof(FromEnum) == 0, "No specialization exists for this conversion"); \
    return {}; \
}

#define BEGIN_ENUM_CONVERSION(FromEnum, ToEnum) \
template<> \
ToEnum enumConvert(FromEnum value) \
{ \
    switch (value) {

#define END_ENUM_CONVERSION \
    } \
    assert(false && "unmapped enum encountered"); \
    throw std::invalid_argument("Unable to convert enum value: " + std::to_string(int(value))); \
}
