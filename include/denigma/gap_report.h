#pragma once

#include <string>

#include "denigma/conversion.h"

namespace denigma {

/// @struct GapReportProducer
/// @brief Identifies the software that produced a serialized gap report.
struct GapReportProducer
{
    std::string name;
    std::string version;
    std::string commit;
};

/// Serializes gaps, source evidence, and producer provenance as JSON.
std::string serializeGapReport(const ConversionResult& result,
                               FormatId sourceFormat,
                               FormatId targetFormat,
                               const GapReportProducer& producer);

} // namespace denigma