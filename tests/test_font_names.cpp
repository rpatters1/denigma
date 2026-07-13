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

#include "utils/font_names.h"

TEST(FontNames, NormalizedFontNameKeepsOnlyAsciiAlphanumerics)
{
    EXPECT_EQ(utils::normalizedFontName("Finale Maestro"), "finalemaestro");
    EXPECT_EQ(utils::normalizedFontName("Finale-Maestro_Text!"), "finalemaestrotext");
    EXPECT_EQ(utils::normalizedFontName(" MaEsTrO "), "maestro");
}

TEST(FontNames, MapsKnownFinaleLegacyFontsToSmuflFonts)
{
    auto mapped = utils::mappedSmuflFontForFinaleLegacyFont("Maestro");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Maestro");

    mapped = utils::mappedSmuflFontForFinaleLegacyFont("Engraver Font Set");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Engraver");

    mapped = utils::mappedSmuflFontForFinaleLegacyFont("Broadway-Copyist");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Broadway");

    mapped = utils::mappedSmuflFontForFinaleLegacyFont("Finale_Legacy");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Legacy");
}

TEST(FontNames, DoesNotMapUnknownFonts)
{
    EXPECT_FALSE(utils::mappedSmuflFontForFinaleLegacyFont("Times New Roman").has_value());
    EXPECT_FALSE(utils::isFinaleLegacyMusicFontMappedToSmufl("Times New Roman"));
}
