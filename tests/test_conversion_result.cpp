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
#include <cstddef>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>

#include "gtest/gtest.h"

#include "denigma/conversion.h"

namespace {

class MemoryConverter final : public denigma::IConverter
{
public:
    [[nodiscard]] denigma::FormatId sourceFormat() const override { return denigma::FormatId::EnigmaXml; }
    [[nodiscard]] denigma::FormatId targetFormat() const override { return denigma::FormatId::MnxJson; }

    denigma::ConversionResult convert(std::span<const std::byte>,
                                      std::ostream& output,
                                      const denigma::ConversionRequest&) const override
    {
        output << "mnx";
        denigma::ConversionResult result;
        result.addDiagnostic(denigma::MessageSeverity::Warning, "preserved warning");
        return result;
    }
};

class ReaderMultiOutputConverter final : public denigma::IReaderMultiOutputConverter
{
public:
    [[nodiscard]] denigma::FormatId sourceFormat() const override { return denigma::FormatId::Musx; }
    [[nodiscard]] denigma::FormatId targetFormat() const override { return denigma::FormatId::MusicXml; }

    denigma::ConversionResult convert(const denigma::IRandomAccessReader&,
                                      const denigma::MultiOutputCallback& outputCallback,
                                      const denigma::ConversionRequest&) const override
    {
        const std::string score = "score";
        const std::string part = "part";
        outputCallback("score.musicxml", std::as_bytes(std::span(score)));
        outputCallback("part.musicxml", std::as_bytes(std::span(part)));
        return {};
    }
};

std::string outputText(const denigma::ConversionOutput& output)
{
    return { reinterpret_cast<const char*>(output.data.data()), output.data.size() };
}

} // namespace

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

TEST(ConverterRegistry, CollectsOwnedSingleOutputAndDiagnostics)
{
    denigma::ConverterRegistry registry;
    registry.add(std::make_unique<MemoryConverter>());
    const std::byte input{};

    const auto artifact = registry.convert(denigma::FormatId::EnigmaXml,
                                           denigma::FormatId::MnxJson,
                                           std::span(&input, 1));

    EXPECT_TRUE(artifact);
    ASSERT_EQ(artifact.outputs().size(), 1u);
    EXPECT_TRUE(artifact.outputs().front().suggestedName.empty());
    EXPECT_EQ(outputText(artifact.outputs().front()), "mnx");
    ASSERT_EQ(artifact.result().diagnostics().size(), 1u);
    EXPECT_EQ(artifact.result().diagnostics().front().message, "preserved warning");
}

TEST(ConverterRegistry, CollectsNamedReaderMultiOutputs)
{
    denigma::ConverterRegistry registry;
    registry.add(std::make_unique<ReaderMultiOutputConverter>());
    const std::byte input{};
    denigma::BufferRandomAccessReader reader(std::span(&input, 1));

    const auto artifact = registry.convert(denigma::FormatId::Musx, denigma::FormatId::MusicXml, reader);

    EXPECT_TRUE(artifact);
    ASSERT_EQ(artifact.outputs().size(), 2u);
    EXPECT_EQ(artifact.outputs()[0].suggestedName, "score.musicxml");
    EXPECT_EQ(outputText(artifact.outputs()[0]), "score");
    EXPECT_EQ(artifact.outputs()[1].suggestedName, "part.musicxml");
    EXPECT_EQ(outputText(artifact.outputs()[1]), "part");
}

TEST(ConverterRegistry, ReportsUnsupportedConversion)
{
    const denigma::ConverterRegistry registry;
    const std::byte input{};

    const auto artifact = registry.convert(denigma::FormatId::EnigmaXml,
                                           denigma::FormatId::Svg,
                                           std::span(&input, 1));

    EXPECT_FALSE(artifact);
    EXPECT_TRUE(artifact.hasError());
    EXPECT_TRUE(artifact.outputs().empty());
    ASSERT_EQ(artifact.result().diagnostics().size(), 1u);
    EXPECT_EQ(artifact.result().diagnostics().front().severity, denigma::MessageSeverity::Error);
}
