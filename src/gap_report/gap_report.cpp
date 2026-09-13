#include "denigma/gap_report.h"

#include <string_view>

#include "nlohmann/json.hpp"

namespace denigma {

namespace {

constexpr int kGapReportSchemaVersion = 1;

std::string_view formatName(FormatId format)
{
    switch (format) {
    case FormatId::Musx: return "musx";
    case FormatId::EnigmaXml: return "enigmaxml";
    case FormatId::MnxJson: return "mnx";
    case FormatId::MusicXml: return "musicxml";
    case FormatId::MssXml: return "mss";
    case FormatId::Svg: return "svg";
    }
    return "unknown";
}

std::string_view representationName(GapRepresentation representation)
{
    switch (representation) {
    case GapRepresentation::None: return "none";
    case GapRepresentation::Partial: return "partial";
    }
    return "none";
}

std::string_view causeName(GapCause cause)
{
    switch (cause) {
    case GapCause::TargetUnsupported: return "target-unsupported";
    case GapCause::ExporterUnimplemented: return "exporter-unimplemented";
    case GapCause::Policy: return "policy";
    }
    return "target-unsupported";
}

void addOptional(nlohmann::ordered_json& object, std::string_view name, const std::optional<int>& value)
{
    if (value) {
        object[std::string(name)] = *value;
    }
}

nlohmann::ordered_json sourceJson(const FinaleSourceLocator& source)
{
    nlohmann::ordered_json result{
        { "pool", source.pool },
        { "recordType", source.recordType }
    };
    addOptional(result, "partId", source.partId);
    addOptional(result, "cmper", source.cmper);
    addOptional(result, "cmper1", source.cmper1);
    addOptional(result, "cmper2", source.cmper2);
    addOptional(result, "inci", source.inci);
    addOptional(result, "entryNumber", source.entryNumber);
    return result;
}

nlohmann::ordered_json pitchJson(const ChordPitch& pitch)
{
    return {
        { "step", pitch.step },
        { "alteration", pitch.alteration }
    };
}

nlohmann::ordered_json chordPayloadJson(const ChordSymbolGapPayload& payload)
{
    nlohmann::ordered_json anchor{
        { "position", {
            { "numerator", payload.anchor.positionNumerator },
            { "denominator", payload.anchor.positionDenominator }
        } }
    };
    if (payload.anchor.partId) {
        anchor["partId"] = *payload.anchor.partId;
    }
    if (payload.anchor.measureId) {
        anchor["measureId"] = *payload.anchor.measureId;
    }
    if (payload.anchor.staff) {
        anchor["staff"] = *payload.anchor.staff;
    }

    auto degrees = nlohmann::ordered_json::array();
    for (const auto& degree : payload.degrees) {
        degrees.push_back({
            { "value", degree.value },
            { "alteration", degree.alteration },
            { "type", degree.type },
            { "impliedByText", degree.impliedByText }
        });
    }

    nlohmann::ordered_json result{
        { "type", "chord-symbol" },
        { "anchor", std::move(anchor) },
        { "root", pitchJson(payload.root) },
        { "rootLowerCase", payload.rootLowerCase },
        { "showRoot", payload.showRoot },
        { "suffixText", payload.suffixText },
        { "showSuffix", payload.showSuffix },
        { "degrees", std::move(degrees) },
        { "parenthesizeDegrees", payload.parenthesizeDegrees },
        { "stackDegrees", payload.stackDegrees },
        { "hasOuterParentheses", payload.hasOuterParentheses },
        { "hasUnrecognizedGlyphs", payload.hasUnrecognizedGlyphs }
    };
    if (payload.quality) {
        result["quality"] = *payload.quality;
    }
    if (payload.bass) {
        result["bass"] = pitchJson(*payload.bass);
        result["bassLowerCase"] = payload.bassLowerCase;
    }
    if (payload.bassArrangement) {
        result["bassArrangement"] = *payload.bassArrangement;
    }
    return result;
}

std::optional<nlohmann::ordered_json> payloadJson(const ConversionGapPayload& payload)
{
    if (const auto* chord = std::get_if<ChordSymbolGapPayload>(&payload)) {
        return chordPayloadJson(*chord);
    }
    return std::nullopt;
}

} // namespace

std::string serializeGapReport(const ConversionResult& result,
                               FormatId sourceFormat,
                               FormatId targetFormat,
                               const GapReportProducer& producer)
{
    auto gaps = nlohmann::ordered_json::array();
    for (const auto& gap : result.gaps()) {
        nlohmann::ordered_json gapJson{
            { "code", gap.code },
            { "target", {
                { "format", formatName(gap.targetFormat) },
                { "representation", representationName(gap.representation) },
                { "cause", causeName(gap.cause) }
            } },
            { "source", sourceJson(gap.source) },
            { "message", gap.message }
        };
        if (const auto payload = payloadJson(gap.payload)) {
            gapJson["payloadVersion"] = gap.payloadVersion.value_or(1);
            gapJson["payload"] = *payload;
        }
        gaps.push_back(std::move(gapJson));
    }

    nlohmann::ordered_json source{
        { "format", formatName(sourceFormat) }
    };
    if (result.sourceEvidence()) {
        source["evidenceFormat"] = formatName(result.sourceEvidence()->format);
        source["document"] = result.sourceEvidence()->document;
    }

    const nlohmann::ordered_json report{
        { "schemaVersion", kGapReportSchemaVersion },
        { "producer", {
            { "name", producer.name },
            { "version", producer.version },
            { "commit", producer.commit }
        } },
        { "source", std::move(source) },
        { "targetFormat", formatName(targetFormat) },
        { "gaps", std::move(gaps) }
    };
    return report.dump(2);
}

} // namespace denigma