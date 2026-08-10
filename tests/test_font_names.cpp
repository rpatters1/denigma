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

    // Fonts the smufl_mapping registry covers that Denigma's own list never did.
    mapped = utils::mappedSmuflFontForFinaleLegacyFont("Maestro Wide");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Maestro");

    mapped = utils::mappedSmuflFontForFinaleLegacyFont("JazzText");
    ASSERT_TRUE(mapped.has_value());
    EXPECT_EQ(*mapped, "Finale Jazz Text");
}

TEST(FontNames, DoesNotMapUnknownFonts)
{
    EXPECT_FALSE(utils::mappedSmuflFontForFinaleLegacyFont("Times New Roman").has_value());
    EXPECT_FALSE(utils::isFinaleLegacyMusicFontMappedToSmufl("Times New Roman"));

    // A legacy font the registry knows but for which no successor has been established. Nothing is
    // invented for it, which is the same answer an unknown font gets.
    EXPECT_FALSE(utils::mappedSmuflFontForFinaleLegacyFont("Crescendo").has_value());

    // A SMuFL font is not a legacy font, so it has no successor to report. Both callers test
    // FontInfo::calcIsSMuFL before asking, so neither can reach this.
    EXPECT_FALSE(utils::mappedSmuflFontForFinaleLegacyFont("Finale Legacy").has_value());
}

TEST(FontNames, ReportsMusicFontStyleForLegacyAndSmuflFonts)
{
    // Style is independent of whether a font is legacy or SMuFL, and of whether it is set in the
    // score or inline with text: the Jazz text faces are handwritten too.
    EXPECT_EQ(utils::musicFontStyleForFont("Maestro"), smufl_mapping::MusicFontStyle::Engraved);
    EXPECT_EQ(utils::musicFontStyleForFont("Petrucci"), smufl_mapping::MusicFontStyle::Engraved);
    EXPECT_EQ(utils::musicFontStyleForFont("Jazz"), smufl_mapping::MusicFontStyle::Handwritten);
    EXPECT_EQ(utils::musicFontStyleForFont("JazzText"), smufl_mapping::MusicFontStyle::Handwritten);
    EXPECT_EQ(utils::musicFontStyleForFont("Bravura"), smufl_mapping::MusicFontStyle::Engraved);
    EXPECT_EQ(utils::musicFontStyleForFont("Finale Maestro"), smufl_mapping::MusicFontStyle::Engraved);
    EXPECT_EQ(utils::musicFontStyleForFont("Petaluma"), smufl_mapping::MusicFontStyle::Handwritten);
    EXPECT_FALSE(utils::musicFontStyleForFont("Times New Roman").has_value());
}

TEST(FontNames, ReportsMusicFontTypeForLegacyAndSmuflFonts)
{
    EXPECT_EQ(utils::musicFontTypeForFont("Maestro"), smufl_mapping::MusicFontType::Engraving);
    EXPECT_EQ(utils::musicFontTypeForFont("Engraver Font Set"), smufl_mapping::MusicFontType::Engraving);
    EXPECT_EQ(utils::musicFontTypeForFont("EngraverTextT"), smufl_mapping::MusicFontType::Text);
    EXPECT_EQ(utils::musicFontTypeForFont("Finale Maestro"), smufl_mapping::MusicFontType::Engraving);
    EXPECT_EQ(utils::musicFontTypeForFont("Finale Maestro Text"), smufl_mapping::MusicFontType::Text);
    EXPECT_FALSE(utils::musicFontTypeForFont("Times New Roman").has_value());
}

TEST(FontNames, ReportsLegacySizeRatioOnlyForStaffRelativeFonts)
{
    // Every Finale music font measured so far spans four staff spaces to the em, the same as SMuFL,
    // so the conversion is an identity. The ratio is still read rather than assumed.
    const auto maestro = utils::legacySmuflSizeRatioForFont("Maestro");
    ASSERT_TRUE(maestro.has_value());
    EXPECT_DOUBLE_EQ(*maestro, 1.0);

    // Patmm's measurements scatter across the mapped glyphs with no coherent scale, so its point
    // size says nothing about staff size and no size may be derived from it.
    EXPECT_FALSE(utils::legacySmuflSizeRatioForFont("Patmm").has_value());

    EXPECT_FALSE(utils::legacySmuflSizeRatioForFont("Times New Roman").has_value());
}
