/*
 * Copyright (C) 2024, Robert Patterson
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
#include <iostream>
#include <filesystem>
#include <fstream>

#include "mnx.h"

#include "nlohmann/json.hpp"
#include "nlohmann/json-schema.hpp"
#include "mnx_schema.xxd"

#include "musx/musx.h"
#include "denigma.h"

namespace denigma {
namespace mnx {

// placeholder
void convert(const std::filesystem::path& file, const Buffer&, const DenigmaOptions& options)
{
    options.logMessage(LogMsg() << "converting to " << file.u8string());
}

// temp func
static bool validateJsonAgainstSchema(const std::filesystem::path& jsonFilePath, const DenigmaOptions& options)
{
    static const std::string_view kMnxSchema(reinterpret_cast<const char *>(mnx_schema_json), mnx_schema_json_len);

    options.logMessage(LogMsg() << "validate JSON " << jsonFilePath.u8string());
    try {
        // Load JSON schema
        nlohmann::json schemaJson = nlohmann::json::parse(kMnxSchema);
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schemaJson);

        // Load JSON file
        std::ifstream jsonFile(jsonFilePath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Unable to open JSON file: " + jsonFilePath.u8string());
        }
        nlohmann::json jsonData;
        jsonFile >> jsonData;

        // Validate JSON
        validator.validate(jsonData);
        options.logMessage(LogMsg() << "JSON is valid against the MNX schema.");
        return true;
    } catch (const nlohmann::json::exception& e) {
        options.logMessage(LogMsg() << "JSON parsing error: " << e.what(), LogSeverity::Error);
    } catch (const std::invalid_argument& e) {
        options.logMessage(LogMsg() << "Invalid argument: " << e.what(), LogSeverity::Error);
    } catch (const std::exception& e) {
        options.logMessage(LogMsg() << e.what(), LogSeverity::Error);
    }
    options.logMessage(LogMsg() << "JSON is not valid against the MNX schema.", LogSeverity::Error);
    return false;
}

} // namespace mnx
} // namespace denigma
