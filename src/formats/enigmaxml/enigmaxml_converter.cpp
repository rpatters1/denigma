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
#include "denigma/formats/enigmaxml.h"

#include <memory>

#include "core/denigma.h"
#include "enigmaxml.h"
#include "utils/stringutils.h"

namespace denigma {
namespace formats {
namespace enigmaxml {

ConversionResult MusxToEnigmaXmlConverter::convert(const IRandomAccessReader& input,
                                                   std::ostream& output,
                                                   const Options& options) const
{
    ConversionResult result;
    DenigmaContext context("denigma");
    context.inputFilePath = options.common.sourceName.empty()
        ? std::filesystem::path("input.musx")
        : utils::utf8ToPath(options.common.sourceName);
    context.noValidate = !options.common.validate;
    context.logCallback = options.common.logCallback;
    context.conversionResult = &result;
    MusxLoggerScope musxLogger(makeMusxLogCallback(context));

    const Buffer buffer = detail::extractMusxInputData(input, context).primaryBuffer;
    output.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return result;
}

ConversionResult MusxToEnigmaXmlConverter::convert(const IRandomAccessReader& input,
                                                   std::ostream& output,
                                                   const ConversionRequest& request) const
{
    return convert(input, output, optionsFromRequest<Options>(request, "MusxToEnigmaXmlConverter"));
}

void registerConverters(ConverterRegistry& registry)
{
    registry.add(std::make_unique<MusxToEnigmaXmlConverter>());
}

} // namespace enigmaxml
} // namespace formats
} // namespace denigma
