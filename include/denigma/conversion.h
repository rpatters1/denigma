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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "denigma/io/random_access_reader.h"

/// @namespace denigma
/// @brief Core public API for the Denigma conversion libraries.
namespace denigma {

/// @enum FormatId
/// @brief Stable identifiers for converter input and output formats.
enum class FormatId
{    
    Musx,       ///< Finale MUSX archive.  
    EnigmaXml,  ///< Finale Enigma XML.
    MnxJson,    ///< MNX JSON as produced by mnxdom.
    MusicXml,   ///< MusicXML score-partwise XML.
    MssXml,     ///< MuseScore style sheet XML.
    Svg         ///< Scalable Vector Graphics XML.
};

/// @enum MessageSeverity
/// @brief Severity for log messages and diagnostics emitted by converters.
enum class MessageSeverity
{
    Info,
    Warning,
    Error,
    Verbose
};

/// @enum GapRepresentation
/// @brief How much of a source feature was represented in the target document.
enum class GapRepresentation
{
    None,
    Partial
};

/// @enum GapCause
/// @brief Why a source feature was not represented faithfully.
enum class GapCause
{
    TargetUnsupported,
    ExporterUnimplemented,
    Policy
};

/// @struct FinaleSourceLocator
/// @brief Stable identity of one source record in Finale EnigmaXML.
struct FinaleSourceLocator
{
    std::string pool;
    std::string recordType;
    std::optional<int> partId;
    std::optional<int> cmper;
    std::optional<int> cmper1;
    std::optional<int> cmper2;
    std::optional<int> inci;
    std::optional<int> entryNumber;
};

/// @struct GapTargetAnchor
/// @brief Location in the converted document where a recovered feature belongs.
struct GapTargetAnchor
{
    /// MNX part ID, omitted when the source staff maps to more than one generated part.
    std::optional<std::string> partId;
    /// MNX measure ID, omitted together with #partId when the target part is ambiguous.
    std::optional<std::string> measureId;
    /// One-based staff number within the MNX part; omitted for single-staff parts.
    std::optional<int> staff;
    /// Position within the measure as a whole-note fraction.
    int positionNumerator{};
    int positionDenominator{ 1 };
};

/// @struct ChordPitch
/// @brief Written pitch spelling for a chord root or alternate bass.
struct ChordPitch
{
    /// Uppercase diatonic step from A through G.
    std::string step;
    int alteration{};
};

/// @struct ChordDegree
/// @brief One normalized degree modification in a chord suffix.
struct ChordDegree
{
    int value{};
    int alteration{};
    /// One of `add`, `remove`, or `alter`.
    std::string type;
    bool impliedByText{};
};

/// @struct ChordSymbolGapPayload
/// @brief Target-independent musical meaning needed to recover a chord symbol.
struct ChordSymbolGapPayload
{
    GapTargetAnchor anchor;
    ChordPitch root;
    bool rootLowerCase{};
    bool showRoot{};
    std::optional<std::string> quality;
    std::string suffixText;
    bool showSuffix{};
    std::optional<ChordPitch> bass;
    bool bassLowerCase{};
    std::optional<std::string> bassArrangement;
    std::vector<ChordDegree> degrees;
    bool parenthesizeDegrees{};
    bool stackDegrees{};
    bool hasOuterParentheses{};
    bool hasUnrecognizedGlyphs{};
};

using ConversionGapPayload = std::variant<std::monostate, ChordSymbolGapPayload>;

inline constexpr std::uint32_t CHORD_SYMBOL_GAP_PAYLOAD_VERSION = 1;

/// @struct ConversionGap
/// @brief A source feature that was not represented faithfully in a conversion target.
struct ConversionGap
{
    std::string code;
    std::optional<std::uint32_t> payloadVersion;
    FormatId targetFormat{ FormatId::MnxJson };
    GapRepresentation representation{ GapRepresentation::None };
    GapCause cause{ GapCause::TargetUnsupported };
    FinaleSourceLocator source;
    std::string message;
    ConversionGapPayload payload;
};

/// @enum GapEvidenceLevel
/// @brief Source evidence retained alongside structured conversion gaps.
enum class GapEvidenceLevel
{
    None,
    SourceDocument
};

/// @struct ConversionSourceEvidence
/// @brief A source document retained once for all gaps in a conversion result.
///
/// The document may contain the complete source score. Callers must treat it as
/// private input data and avoid persisting or transmitting it unintentionally.
struct ConversionSourceEvidence
{
    FormatId format{ FormatId::EnigmaXml };
    std::string document;
};

/// @struct Diagnostic
/// @brief A non-fatal message emitted by a converter.
struct Diagnostic
{
    MessageSeverity severity{ MessageSeverity::Info }; ///< Message severity.
    std::string message;                 ///< Human-readable diagnostic text.
};

/// @struct CommonOptions
/// @brief Options common to all public converter adapters.
struct CommonOptions
{
    /// Caller-supplied source name used for diagnostics and metadata.
    std::string sourceName;
    /// Enables converter-specific output validation when supported.
    bool validate{ true };
    /// Enables verbose logging when supported by the caller.
    bool verbose{ false };
    /// Suppresses info/verbose logging when true.
    bool quiet{ false };
    /// Every source font will be available in the environment that reads the converted output.
    bool allFontsAvailable{ false };
    /// Controls whether gap reports retain a copy of the source document.
    GapEvidenceLevel gapEvidenceLevel{ GapEvidenceLevel::None };
    /// Optional callback that receives converter log messages. Defaults to no-op.
    std::function<void(MessageSeverity severity, std::string_view message)> logCallback = [](MessageSeverity, std::string_view) {};
};

/// @class IOptions
/// @brief Base class for adapter-specific option structs.
class IOptions
{
public:
    virtual ~IOptions() = default;      ///< Virtual destructor
};

/// @struct ConversionRequest
/// @brief Type-erased request used by registry-based converter calls.
struct ConversionRequest
{
    /// Adapter-specific options. Must remain valid for the duration of the conversion call.
    const IOptions* options{};
};

/// @struct ConversionResult
/// @brief Result metadata returned after a conversion completes.
class ConversionResult
{
public:
    /// Returns the diagnostics collected during conversion.
    [[nodiscard]] std::span<const Diagnostic> diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    /// Returns true when at least one diagnostic has severity Error.
    [[nodiscard]] bool hasError() const noexcept
    {
        return m_hasError;
    }

    /// Returns true when the conversion completed without any error diagnostics.
    explicit operator bool() const noexcept
    {
        return !hasError();
    }

    /// Adds a diagnostic and updates the error state if needed.
    void addDiagnostic(MessageSeverity severity, std::string message)
    {
        addDiagnostic(Diagnostic{ severity, std::move(message) });
    }

    /// Adds a diagnostic and updates the error state if needed.
    void addDiagnostic(Diagnostic diagnostic)
    {
        m_hasError = m_hasError || diagnostic.severity == MessageSeverity::Error;
        m_diagnostics.push_back(std::move(diagnostic));
    }

    /// Returns source features that were not represented faithfully.
    [[nodiscard]] std::span<const ConversionGap> gaps() const noexcept
    {
        return m_gaps;
    }

    /// Adds a structured conversion gap.
    void addGap(ConversionGap gap)
    {
        m_gaps.push_back(std::move(gap));
    }

    /// Retains a source document shared by every gap in this result.
    void setSourceEvidence(ConversionSourceEvidence evidence)
    {
        m_sourceEvidence = std::move(evidence);
    }

    /// Returns the shared source document when the converter retained one.
    [[nodiscard]] const std::optional<ConversionSourceEvidence>& sourceEvidence() const noexcept
    {
        return m_sourceEvidence;
    }

private:
    std::vector<Diagnostic> m_diagnostics;
    std::vector<ConversionGap> m_gaps;
    std::optional<ConversionSourceEvidence> m_sourceEvidence;
    bool m_hasError{};
};

/// @struct ConversionOutput
/// @brief One document produced by a conversion.
struct ConversionOutput
{
    /// Suggested output filename. Empty when the converter does not supply one.
    std::string suggestedName;
    /// Owned output bytes.
    std::vector<std::byte> data;
};

/// @class ConversionArtifact
/// @brief Owned documents and result metadata from one conversion.
class ConversionArtifact
{
public:
    /// Creates an artifact from converter result metadata and generated documents.
    ConversionArtifact(ConversionResult result, std::vector<ConversionOutput> outputs)
        : m_result(std::move(result)), m_outputs(std::move(outputs))
    {
    }

    /// Returns converter result metadata.
    [[nodiscard]] const ConversionResult& result() const noexcept
    {
        return m_result;
    }

    /// Returns the generated documents in converter emission order.
    [[nodiscard]] std::span<const ConversionOutput> outputs() const noexcept
    {
        return m_outputs;
    }

    /// Returns true when the conversion result contains an error diagnostic.
    [[nodiscard]] bool hasError() const noexcept
    {
        return m_result.hasError();
    }

    /// Returns true when the conversion completed without an error diagnostic.
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_result);
    }

private:
    ConversionResult m_result;
    std::vector<ConversionOutput> m_outputs;
};

/// @brief Returns typed options from an erased request, or default options when none were supplied.
/// @return converion options typed as `OptionsT`.
/// @throw std::invalid_argument if the request contains incompatible options.
template <typename OptionsT>
OptionsT optionsFromRequest(const ConversionRequest& request, std::string_view converterName)
{
    if (!request.options) {
        return {};
    }
    if (const auto* options = dynamic_cast<const OptionsT*>(request.options)) {
        return *options;
    }
    assert(false && "incompatible conversion options");
    throw std::invalid_argument(std::string(converterName) + " received incompatible conversion options.");
}

/// @class IConverter
/// @brief Public interface implemented by each conversion adapter.
class IConverter
{
public:
    virtual ~IConverter() = default;    ///< virtual destructor

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input memory buffer and writes the converted output to the provided stream.
    virtual ConversionResult convert(std::span<const std::byte> input,
                                     std::ostream& output,
                                     const ConversionRequest& request = {}) const = 0;
};

/// @class IReaderConverter
/// @brief Public interface implemented by adapters whose input is a random-access container.
class IReaderConverter
{
public:
    virtual ~IReaderConverter() = default;    ///< virtual destructor

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input reader and writes the converted output to the provided stream.
    virtual ConversionResult convert(const IRandomAccessReader& input,
                                     std::ostream& output,
                                     const ConversionRequest& request = {}) const = 0;
};

/// Callback used by converters that may emit zero, one, or many output buffers.
using MultiOutputCallback = std::function<void(std::string_view suggestedName, std::span<const std::byte> data)>;

/// @class IMultiOutputConverter
/// @brief Public interface implemented by adapters that may produce multiple output documents.
class IMultiOutputConverter
{
public:
    virtual ~IMultiOutputConverter() = default;    ///< virtual destructor

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input memory buffer and invokes outputCallback once for each generated output.
    virtual ConversionResult convert(std::span<const std::byte> input,
                                     const MultiOutputCallback& outputCallback,
                                     const ConversionRequest& request = {}) const = 0;
};

/// @class IReaderMultiOutputConverter
/// @brief Public interface implemented by reader-backed adapters that may produce multiple output documents.
class IReaderMultiOutputConverter
{
public:
    virtual ~IReaderMultiOutputConverter() = default;    ///< virtual destructor

    /// Returns the source format accepted by this converter.
    [[nodiscard]] virtual FormatId sourceFormat() const = 0;
    /// Returns the target format produced by this converter.
    [[nodiscard]] virtual FormatId targetFormat() const = 0;

    /// Converts the input reader and invokes outputCallback once for each generated output.
    virtual ConversionResult convert(const IRandomAccessReader& input,
                                     const MultiOutputCallback& outputCallback,
                                     const ConversionRequest& request = {}) const = 0;
};

/// @class ConverterRegistry
/// @brief Lightweight registry for locating converters by source and target format.
class ConverterRegistry
{
public:
    /// Adds a converter to the registry.
    void add(std::unique_ptr<IConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_converters.emplace_back(std::move(converter));
    }

    /// Adds a multi-output converter to the registry.
    void add(std::unique_ptr<IMultiOutputConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_multiOutputConverters.emplace_back(std::move(converter));
    }

    /// Adds a reader-backed converter to the registry.
    void add(std::unique_ptr<IReaderConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_readerConverters.emplace_back(std::move(converter));
    }

    /// Adds a reader-backed multi-output converter to the registry.
    void add(std::unique_ptr<IReaderMultiOutputConverter> converter)
    {
        if (!converter) {
            throw std::invalid_argument("converter cannot be null");
        }
        m_readerMultiOutputConverters.emplace_back(std::move(converter));
    }

    /// Returns the first registered converter matching the requested formats, or nullptr.
    [[nodiscard]] const IConverter* find(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_converters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered multi-output converter matching the requested formats, or nullptr.
    [[nodiscard]] const IMultiOutputConverter* findMultiOutput(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_multiOutputConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered reader-backed converter matching the requested formats, or nullptr.
    [[nodiscard]] const IReaderConverter* findReader(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_readerConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Returns the first registered reader-backed multi-output converter matching the requested formats, or nullptr.
    [[nodiscard]] const IReaderMultiOutputConverter* findReaderMultiOutput(FormatId sourceFormat, FormatId targetFormat) const
    {
        for (const auto& converter : m_readerMultiOutputConverters) {
            if (converter->sourceFormat() == sourceFormat && converter->targetFormat() == targetFormat) {
                return converter.get();
            }
        }
        return nullptr;
    }

    /// Converts an in-memory input with the registered adapter for the requested formats.
    /// Returns an error diagnostic when no matching adapter is registered.
    [[nodiscard]] ConversionArtifact convert(FormatId sourceFormat,
                                             FormatId targetFormat,
                                             std::span<const std::byte> input,
                                             const ConversionRequest& request = {}) const
    {
        if (const auto* converter = find(sourceFormat, targetFormat)) {
            return collectSingleOutput(*converter, input, request);
        }
        if (const auto* converter = findMultiOutput(sourceFormat, targetFormat)) {
            return collectMultipleOutputs(*converter, input, request);
        }
        return unsupportedConversion();
    }

    /// Converts a random-access input with the registered adapter for the requested formats.
    /// Returns an error diagnostic when no matching adapter is registered.
    [[nodiscard]] ConversionArtifact convert(FormatId sourceFormat,
                                             FormatId targetFormat,
                                             const IRandomAccessReader& input,
                                             const ConversionRequest& request = {}) const
    {
        if (const auto* converter = findReader(sourceFormat, targetFormat)) {
            return collectSingleOutput(*converter, input, request);
        }
        if (const auto* converter = findReaderMultiOutput(sourceFormat, targetFormat)) {
            return collectMultipleOutputs(*converter, input, request);
        }
        return unsupportedConversion();
    }

private:
    template <typename Converter, typename Input>
    static ConversionArtifact collectSingleOutput(const Converter& converter,
                                                   const Input& input,
                                                   const ConversionRequest& request)
    {
        std::ostringstream output;
        auto result = converter.convert(input, output, request);
        auto text = output.str();
        std::vector<std::byte> data(text.size());
        if (!text.empty()) {
            std::memcpy(data.data(), text.data(), text.size());
        }
        std::vector<ConversionOutput> outputs;
        outputs.push_back({ {}, std::move(data) });
        return { std::move(result), std::move(outputs) };
    }

    template <typename Converter, typename Input>
    static ConversionArtifact collectMultipleOutputs(const Converter& converter,
                                                      const Input& input,
                                                      const ConversionRequest& request)
    {
        std::vector<ConversionOutput> outputs;
        auto result = converter.convert(input, [&outputs](std::string_view suggestedName, std::span<const std::byte> data) {
            outputs.push_back({ std::string(suggestedName), { data.begin(), data.end() } });
        }, request);
        return { std::move(result), std::move(outputs) };
    }

    static ConversionArtifact unsupportedConversion()
    {
        ConversionResult result;
        result.addDiagnostic(MessageSeverity::Error, "No converter is registered for the requested formats.");
        return { std::move(result), {} };
    }

    std::vector<std::unique_ptr<IConverter>> m_converters;
    std::vector<std::unique_ptr<IMultiOutputConverter>> m_multiOutputConverters;
    std::vector<std::unique_ptr<IReaderConverter>> m_readerConverters;
    std::vector<std::unique_ptr<IReaderMultiOutputConverter>> m_readerMultiOutputConverters;
};

} // namespace denigma
