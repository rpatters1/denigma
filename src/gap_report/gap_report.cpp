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

} // namespace

std::string serializeGapReport(const ConversionResult& result,
                               FormatId sourceFormat,
                               FormatId targetFormat,
                               const GapReportProducer& producer)
{
    auto gaps = nlohmann::ordered_json::array();
    for (const auto& gap : result.gaps()) {
        gaps.push_back({
            { "code", gap.code },
            { "payloadVersion", gap.payloadVersion },
            { "target", {
                { "format", formatName(gap.targetFormat) },
                { "representation", representationName(gap.representation) },
                { "cause", causeName(gap.cause) }
            } },
            { "source", sourceJson(gap.source) },
            { "message", gap.message }
        });
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