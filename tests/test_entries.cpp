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
#include "gtest/gtest.h"

#include "denigma/classify/entries.h"
#include "musx/musx.h"

using namespace denigma::classify;
using namespace musx::dom;

/// @todo Add a positive-path fixture-based test once a .musx/.enigmaxml fixture with an
/// artificial-harmonic chord (a Regular-notehead note and a Diamond-notehead note a fourth,
/// major third, or fifth apart) is available. No such fixture exists in either this repo's or
/// musxdom's test data at the time this classifier was added, and hand-authoring a full
/// entry/frame/staff document has no precedent elsewhere in either test suite.
TEST(EntryNoteheadClassification, ReturnsUnrecognizedForInvalidEntry)
{
    EXPECT_FALSE(classifyEntryNoteheads(EntryInfoPtr()));
}
