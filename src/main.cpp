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
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <errno.h>

#include "zip_file.hpp"		// miniz submodule (for zip)
#include "ezgz.hpp"			// ezgz submodule (for gzip)
#include "nlohmann/json.hpp"
#include "nlohmann/json-schema.hpp"
#include "mnx_schema.xxd"

#include "musx/musx.h"

#define MUSX_USE_TINYXML2
#include "musx/xml/TinyXmlImpl.h"

constexpr char MUSX_EXTENSION[]		    = ".musx";
constexpr char JSON_EXTENSION[]		    = ".json";
constexpr char ENIGMAXML_EXTENSION[]    = ".enigmaxml";
constexpr char SCORE_DAT_NAME[] 		= "score.dat";

static std::vector<char> extractEnigmaXml(const std::string& zipFile)
{
    try
    {
        miniz_cpp::zip_file zip(zipFile.c_str());
        std::string buffer = zip.read(SCORE_DAT_NAME);
        musx::util::ScoreFileEncoder::recodeBuffer(buffer);
        return EzGz::IGzFile<>({reinterpret_cast<uint8_t *>(buffer.data()), buffer.size()}).readAll();
    }
    catch (const std::exception &ex)
    {
        std::cout << "Error: unable to extract enigmaxml from file " << zipFile << " (exception: " << ex.what() << ")" << std::endl;
    }
    return {};
}

static bool writeEnigmaXml(const std::string& outputPath, const std::vector<char>& xmlBuffer)
{
    std::cout << "write to " << outputPath << std::endl;

    try	{
        std::ifstream inFile;

        size_t uncompressedSize = xmlBuffer.size();
        std::cout << "decompressed Size " << uncompressedSize<< std::endl;

        std::ofstream xmlFile;
        xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
        xmlFile.open(outputPath, std::ios::binary);
        xmlFile.write(xmlBuffer.data(), xmlBuffer.size());
        return true;
    } catch (const std::ios_base::failure& ex) {
        std::cout << "unable to write " << outputPath << std::endl
                  << "message: " << ex.what() << std::endl
                  << "details: " << std::strerror(ex.code().value()) << std::endl;
    } catch (const std::exception &ex) {
        std::cout << "unable to write " << outputPath << " (exception: " << ex.what() << ")" << std::endl;
    }
    return false;
}

static bool processMusxFile(const std::filesystem::path& musxFilePath)
{
    const std::filesystem::path enigmaXmlPath = [musxFilePath]() -> std::filesystem::path
    {
        std::filesystem::path retval = musxFilePath;
        return retval.replace_extension(ENIGMAXML_EXTENSION);
    }();
    const std::vector<char> xmlBuffer = extractEnigmaXml(musxFilePath.string());
    if (xmlBuffer.size() <= 0) {
        std::cerr << "Failed to extract enigmaxml " << musxFilePath << std::endl;
        return false;
    }

    try {
        musx::xml::tinyxml2::Document enigmaXml;
        enigmaXml.loadFromString(xmlBuffer);
    } catch (const musx::xml::load_error& ex) {
        std::cerr << "Load XML failed: " << ex.what() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Unknown error: " << ex.what() << std::endl;
    }

    if (!writeEnigmaXml(enigmaXmlPath.string(), xmlBuffer)) {
        std::cerr << "Failed to extract zip file " << musxFilePath << " to " << enigmaXmlPath << std::endl;
        return false;
    }

    std::cout << "Extraction complete!" << std::endl;
    return true; 
}

static bool validateJsonAgainstSchema(const std::filesystem::path& jsonFilePath)
{
    static const std::string_view kMnxSchema(reinterpret_cast<const char *>(mnx_schema_json), mnx_schema_json_len);

    std::cout << "validate JSON " << jsonFilePath << std::endl;
    try {
        // Load JSON schema
        nlohmann::json schemaJson = nlohmann::json::parse(kMnxSchema);
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(schemaJson);

        // Load JSON file
        std::ifstream jsonFile(jsonFilePath);
        if (!jsonFile.is_open()) {
            throw std::runtime_error("Unable to open JSON file: " + jsonFilePath.string());
        }
        nlohmann::json jsonData;
        jsonFile >> jsonData;

        // Validate JSON
        validator.validate(jsonData);
        std::cout << "JSON is valid against the MNX schema." << std::endl;
        return true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid argument: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    std::cout << "JSON is not valid against the MNX schema.\n";
    return false;
}

static bool isValidExtension(const std::string& extension)
{
    if (extension == MUSX_EXTENSION) {
        return true;
    } else if (extension == JSON_EXTENSION) {
        return true;
    }
    return false;
}

static int showHelp(const std::string& programName)
{
    std::cerr << "Usage: " << programName << " <musx|json file>" << std::endl;
    return 1;
}

int main(int argc, char* argv[]) {
    if (argc <= 0) {
        std::cerr << "Error: argv[0] is unavailable" << std::endl;
        return 1;
    }

    std::string programName = std::filesystem::path(argv[0]).stem().string();
    std::cout << programName << " " << MUSXCONVERT_VERSION << std::endl;

    if (argc < 2) {
        return showHelp(programName);
    }

    const std::filesystem::path inputFilePath = argv[1];
    if (!std::filesystem::is_regular_file(inputFilePath) || ! isValidExtension(inputFilePath.extension().string())) {
        return showHelp(programName);
    }

    const bool success = [inputFilePath]() -> bool
    {
        if (inputFilePath.extension() == MUSX_EXTENSION) {
            return processMusxFile(inputFilePath);
        } else if (inputFilePath.extension() == JSON_EXTENSION) {
            return validateJsonAgainstSchema(inputFilePath);
        }
        std::cout << "Unsupported file extension: " << inputFilePath.extension() << std::endl;
        return false;
    }();

    return !success;
}
