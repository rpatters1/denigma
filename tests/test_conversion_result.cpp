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
#include <string>

#include "gtest/gtest.h"

#include "denigma/conversion.h"

TEST(ConversionResult, TracksDiagnosticsAndErrorState)
{
    denigma::ConversionResult result;

    EXPECT_TRUE(result);
    EXPECT_FALSE(result.hasError());
    EXPECT_TRUE(result.diagnostics().empty());

    result.addDiagnostic(denigma::MessageSeverity::Warning, std::string("warning"));
    EXPECT_TRUE(result);
    EXPECT_FALSE(result.hasError());
    ASSERT_EQ(result.diagnostics().size(), 1u);
    EXPECT_EQ(result.diagnostics().front().severity, denigma::MessageSeverity::Warning);
    EXPECT_EQ(result.diagnostics().front().message, "warning");

    result.addDiagnostic(denigma::Diagnostic{ denigma::MessageSeverity::Error, "error" });
    EXPECT_FALSE(result);
    EXPECT_TRUE(result.hasError());
    ASSERT_EQ(result.diagnostics().size(), 2u);
    EXPECT_EQ(result.diagnostics().back().severity, denigma::MessageSeverity::Error);
    EXPECT_EQ(result.diagnostics().back().message, "error");
}
