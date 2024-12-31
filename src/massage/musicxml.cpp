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
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>
#include <sstream>
#include <functional>

#include "musx/musx.h"
#include "pugixml.hpp"

#include "denigma.h"
#include "massage/massage.h"
#include "massage/musicxml.h"
#include "export/enigmaxml.h"
#include "util/ziputils.h"

constexpr static double EDU_PER_QUARTER = 1024.0;
constexpr static char INDENT_SPACES[] = "    ";


namespace denigma {
namespace musicxml {

struct MassageMusicXmlContext
{
    DenigmaContext denigmaContext;
    musx::dom::DocumentPtr musxDocument;
    int currentPart{};
    int currentMeasure{};
    int currentStaff{};
    int currentStaffOffset{};
    int errorCount{};

    void initCounts()
    {
        currentPart = currentMeasure = currentStaff = currentStaffOffset = errorCount = 0;
    }

    void logMessage(LogMsg&& msg, LogSeverity severity = LogSeverity::Info);
};

void MassageMusicXmlContext::logMessage(LogMsg&& msg, LogSeverity severity)
{
    if (severity == LogSeverity::Error) {
        ++errorCount;
    }
    std::string logEntry;
    if (currentStaff > 0 && currentMeasure > 0) {
        std::string staffText = "p" + std::to_string(currentPart);
        int staffNumber = currentStaff + currentStaffOffset;

        // Simulate fetching the staff name (replace with actual implementation)
        std::string staffName = "[StaffName]"; // Replace with staff name fetching logic

        staffText += "[" + staffName + "]";
        logEntry += "(" + staffText + " m" + std::to_string(currentMeasure) + ") ";
    }
    denigmaContext.logMessage(LogMsg() << logEntry << msg.str(), severity);
}

Buffer read(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
    std::ifstream xmlFile;
    xmlFile.exceptions(std::ios::failbit | std::ios::badbit);
    xmlFile.open(inputPath, std::ios::binary);
    return Buffer((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());
}

static int staffNumberFromNote(pugi::xml_node xmlNote) {
    if (!xmlNote) return 1;
    auto xmlStaff = xmlNote.child("staff");
    if (xmlStaff && xmlStaff.text().get()) {
        try {
            return std::stoi(xmlStaff.text().get());
        } catch (...) {}
    }
    return 1; // Default to 1 if no <staff> node or invalid content.
}

// This iterator function finds the next matching direction *before* sending the current one to the callback.
// This allows the callback to move the direction without perturbing the iteration sequence.
void feedDirectionsOfType(pugi::xml_node node, const std::string& nodeName, const std::function<void(pugi::xml_node)>& processNode)
{
    // Local function to find the next matching <direction> node
    auto findNextMatchingDirection = [&](pugi::xml_node& currentNode) {
        while (currentNode) {
            if (auto nodeForType = currentNode.child("direction-type").child(nodeName.c_str())) {
                return; // Found a matching node, return to caller
            }
            currentNode = currentNode.next_sibling("direction");
        }
    };

    // Start with the first <direction> node
    auto child = node.child("direction");
    findNextMatchingDirection(child); // Initialize by finding the first match

    // Process each matching <direction> node
    while (child) {
        pugi::xml_node currentNode = child; // Store the current node
        child = child.next_sibling("direction"); // Move to the next sibling
        findNextMatchingDirection(child); // Find the next matching node
        processNode(currentNode); // Process the current node
    }
}

static void fixDirectionBrackets(pugi::xml_node xmlMeasure, const std::string& directionType, const std::shared_ptr<MassageMusicXmlContext>& context)
{
    const DenigmaContext& denigmaContext = context->denigmaContext;
    if (!(denigmaContext.extendOttavasLeft || denigmaContext.extendOttavasRight)) return;

    feedDirectionsOfType(xmlMeasure, directionType, [&](pugi::xml_node currentDirection) {
        auto xmlDirectionType = currentDirection.child("direction-type");
        if (!xmlDirectionType) return;

        auto nodeForType = xmlDirectionType.child(directionType.c_str());
        if (!nodeForType) return;

        auto directionCopy = currentDirection; // Shallow copy
        std::string shiftType = nodeForType.attribute("type").value();

        // keep this for next iteration
        auto nextDirection = currentDirection.next_sibling("direction");

        if (shiftType == "stop") {
            if (denigmaContext.extendOttavasRight) {
                auto nextNote = currentDirection.next_sibling("note");
                // Find the next note, skipping over extra notes in chords
                if (nextNote) {
                    auto chordCheck = nextNote.next_sibling("note");
                    while (chordCheck && chordCheck.child("chord")) {
                        nextNote = chordCheck;
                        chordCheck = chordCheck.next_sibling("note");
                    }
                }
                if (nextNote && !nextNote.child("rest")) {
                    xmlMeasure.remove_child(currentDirection);
                    xmlMeasure.insert_copy_after(directionCopy, nextNote);
                    context->currentStaffOffset = staffNumberFromNote(nextNote) - 1;
                    context->logMessage(LogMsg() << "Extended " << directionType << " element by one note/chord.");
                }
            }
        } else if (directionType == "octave-shift" && (shiftType == "up" || shiftType == "down")) {
            if (denigmaContext.extendOttavasLeft) {
                int sign = (shiftType == "down") ? 1 : -1;
                int octaves = (nodeForType.attribute("size").as_int(8) - 1) / 7;

                auto prevNote = currentDirection.previous_sibling("note");
                pugi::xml_node prevGraceNote;

                while (prevNote) {
                    if (!prevNote.child("rest") && prevNote.child("grace")) {
                        prevGraceNote = prevNote;
                        auto pitch = prevNote.child("pitch");
                        auto octave = pitch.child("octave");
                        if (octave) {
                            octave.text().set(octave.text().as_int() + sign * octaves);
                        }
                    } else {
                        break;
                    }
                    prevNote = prevNote.previous_sibling("note");
                }

                if (prevGraceNote) {
                    xmlMeasure.remove_child(currentDirection);
                    auto prevElement = prevGraceNote.previous_sibling();
                    if (prevElement) {
                        xmlMeasure.insert_copy_after(directionCopy, prevElement);
                    } else {
                        xmlMeasure.prepend_copy(directionCopy);
                    }
                    context->currentStaffOffset = staffNumberFromNote(prevGraceNote) - 1;
                    context->logMessage(LogMsg() << "Adjusted octave-shift element of size "
                                                 << std::to_string(nodeForType.attribute("size").as_int(8))
                                                 << " to include preceding grace notes.");
                }
            }
        }
    });
}

static void fixFermataWholeRests(pugi::xml_node xmlMeasure, const std::shared_ptr<MassageMusicXmlContext>& context)
{
    assert(xmlMeasure);

    auto noteNode = xmlMeasure.child("note");
    if (!noteNode) return;
    auto restNode = noteNode.child("rest");
    if (!restNode) return;
    auto typeNode = noteNode.child("type");
    if (!typeNode || std::strcmp(typeNode.text().get(), "whole") != 0) return;

    // Process <notations> nodes
    for (auto notationsNode = noteNode.child("notations");
         notationsNode;
         notationsNode = notationsNode.next_sibling("notations")) {
        if (notationsNode.child("fermata")) {
            auto measureAttr = restNode.attribute("measure");
            if (measureAttr) {
                measureAttr.set_value("yes");
            } else {
                restNode.append_attribute("measure") = "yes";
            }
            noteNode.remove_child(typeNode);
            context->currentStaffOffset = staffNumberFromNote(noteNode) - 1;
            context->logMessage(LogMsg() << "Removed real whole rest under fermata.");
            break;
        }
    }
}

void massageXml(pugi::xml_node scorePartWiseNode, const std::shared_ptr<MassageMusicXmlContext>& context)
{
    if (!context->musxDocument && context->denigmaContext.refloatRests) {
        context->logMessage(LogMsg() << "Corresponding Finale document not found.", LogSeverity::Warning);
    }

    context->currentPart = 1;
    context->currentStaff = 1;
    context->currentStaffOffset = 0;

    for (auto xmlPart = scorePartWiseNode.child("part"); xmlPart; xmlPart = xmlPart.next_sibling("part")) {
        context->currentMeasure = 0;
        double durationUnit = EDU_PER_QUARTER;
        int stavesUsed = 1;

        for (auto xmlMeasure = xmlPart.child("measure"); xmlMeasure; xmlMeasure = xmlMeasure.next_sibling("measure")) {
            auto attributes = xmlMeasure.child("attributes");
            if (attributes) {
                if (auto divisions = attributes.child("divisions")) {
                    durationUnit = EDU_PER_QUARTER / std::stod(divisions.text().get());
                }

                if (auto staves = attributes.child("staves")) {
                    int numStaves = std::stoi(staves.text().get());
                    if (numStaves > stavesUsed) {
                        stavesUsed = numStaves;
                    }
                }
            }

            context->currentMeasure++;

            /// @todo write the necessary functions and uncomment
            /*
            if (context->musxDocument && context->denigmaContext.refloatRests) {
                for (int staffNum = 1; staffNum <= stavesUsed; ++staffNum) {
                    context->currentStaffOffset = staffNum - 1;
                    massageXmlWithFinaleDocument(
                        xmlMeasure,
                        context->currentStaff + context->currentStaffOffset,
                        context->currentMeasure,
                        durationUnit,
                        staffNum
                    );
                }
            }
            */

            fixDirectionBrackets(xmlMeasure, "octave-shift", context);

            if (context->denigmaContext.fermataWholeRests) {
                fixFermataWholeRests(xmlMeasure, context);
            }
        }

        context->currentPart++;
        context->currentStaff += stavesUsed;
        context->currentStaffOffset = 0;
    }
}

static void processXml(pugi::xml_document& xmlDocument, const std::shared_ptr<MassageMusicXmlContext>& context, const std::shared_ptr<others::PartDefinition>& part = nullptr)
{
    const DenigmaContext& denigmaContext = context->denigmaContext;

    auto scorePartwise = xmlDocument.child("score-partwise");
    if (!scorePartwise) {
        throw std::invalid_argument("file does not appear to be exported from Finale");
    }

    auto identificationElement = scorePartwise.child("identification");
    auto encodingElement = identificationElement.child("encoding");
    auto softwareElement = encodingElement.child("software");
    auto encodingDateElement = encodingElement.child("encoding-date");

    if (!softwareElement || !encodingDateElement) {
        throw std::invalid_argument("missing required element 'software' and/or 'encoding-date'");
    }

    std::string creatorSoftware = softwareElement.text().get();
    if (creatorSoftware.empty()) creatorSoftware = "Unspecified";
    if (creatorSoftware.substr(0, 6) != "Finale") {
        throw std::invalid_argument("skipping file exported by " + creatorSoftware);
    }

    softwareElement.text().set((denigmaContext.programName + ' ' + MassageCommand().commandName().data() + ' ' + DENIGMA_VERSION).c_str());

    std::string originalEncodingDate = encodingDateElement.text().get();
    encodingDateElement.text().set(getTimeStamp("%Y-%m-%d").c_str());

    pugi::xml_node miscellaneousElement = identificationElement.child("miscellaneous");
    if (!miscellaneousElement) {
        miscellaneousElement = identificationElement.append_child("miscellaneous");
    }
    
    auto insertMiscellaneousField = [&](const std::string& name, const auto& value) {
        pugi::xml_node element = miscellaneousElement.append_child("miscellaneous-field");
        element.append_attribute("name").set_value(name.c_str());
        
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
            element.text().set(value.c_str());
        } else {
            element.text().set(value);
        }
    };

    // Insert misc fields
    insertMiscellaneousField("original-software", creatorSoftware);
    insertMiscellaneousField("original-encoding-date", originalEncodingDate);
    insertMiscellaneousField("extend-ottavas-right", denigmaContext.extendOttavasRight);
    insertMiscellaneousField("extend-ottavas-left", denigmaContext.extendOttavasLeft);
    insertMiscellaneousField("fermata-whole-rests", denigmaContext.fermataWholeRests);
    insertMiscellaneousField("refloat-rests", denigmaContext.refloatRests);

    massageXml(scorePartwise, context);
}

static std::filesystem::path calcQualifiedOutputPath(const std::filesystem::path& outputPath)
{
    std::filesystem::path qualifiedOutputPath = outputPath;
    qualifiedOutputPath.replace_extension(".massaged" + outputPath.extension().u8string());
    return qualifiedOutputPath;
}

static void processFile(pugi::xml_document&& xmlDocument, const std::filesystem::path& outputPath, const std::shared_ptr<MassageMusicXmlContext>& context, const std::shared_ptr<others::PartDefinition>& part = nullptr)
{
    std::filesystem::path qualifiedOutputPath = calcQualifiedOutputPath(outputPath);
    if (!denigma::validatePathsAndOptions(qualifiedOutputPath, context->denigmaContext)) {
        return;
    }

    processXml(xmlDocument, context, part);

    std::ofstream outputFile;
    outputFile.exceptions(std::ios::failbit | std::ios::badbit);
    outputFile.open(qualifiedOutputPath);
    pugi::xml_writer_stream writer(outputFile);
    xmlDocument.save(writer, INDENT_SPACES);
    outputFile.close();
    xmlDocument.reset();
}

std::optional<std::string> findPartFileNameByPartName(const pugi::xml_document& scoreXml, const std::string& utf8PartName)
{
    auto nextScorePart = scoreXml
        .child("score-partwise")
        .child("part-list")
        .child("score-part");
    while (nextScorePart) {
        for (auto partLink = nextScorePart.child("part-link"); partLink; partLink = partLink.next_sibling("part-link")) {
            if (partLink.attribute("xlink:title").value() == utf8PartName) {
                return partLink.attribute("xlink:href").value();
            }
        }
        nextScorePart = nextScorePart.next_sibling("score-part");
    }
    return std::nullopt;
}

std::filesystem::path findPartNameByPartFileName(const pugi::xml_document& scoreXml, const std::filesystem::path& partFileName)
{
    auto nextScorePart = scoreXml
        .child("score-partwise")
        .child("part-list")
        .child("score-part");
    while (nextScorePart) {
        for (auto partLink = nextScorePart.child("part-link"); partLink; partLink = partLink.next_sibling("part-link")) {
            if (partLink.attribute("xlink:href").value() == partFileName.u8string()) {
                auto retval = stringutils::utf8ToPath(partLink.attribute("xlink:title").value());
                retval.replace_extension(MUSICXML_EXTENSION);
                return retval;
            }
        }
        nextScorePart = nextScorePart.next_sibling("score-part");
    }
    return partFileName;
}

std::optional<std::filesystem::path> findFinaleFile(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
    // Helper function to check if a file exists
    auto fileExists = [](const std::filesystem::path& path) -> bool {
        return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
    };

    // Helper function to construct a candidate file path by replacing the extension
    auto constructPath = [](const std::filesystem::path& basePath, const char* ext) -> std::filesystem::path {
        std::filesystem::path candidate = basePath;
        candidate.replace_extension(ext);
        return candidate;
    };

    // Helper function to search for a specific extension in a directory
    auto findWithExtension = [&](const std::filesystem::path& dir, const std::filesystem::path& baseName, const char* ext) -> std::filesystem::path {
        auto candidate = constructPath(dir / baseName, ext);
        if (fileExists(candidate)) {
            return candidate;
        }
        return {};
    };

    // Search a list of directories for `.musx` files first, then `.enigmaxml` files
    auto searchDirectories = [&](const std::vector<std::filesystem::path>& directories, const std::filesystem::path& baseName) -> std::filesystem::path {
        // Search for `.musx`
        for (const auto& dir : directories) {
            auto result = findWithExtension(dir, baseName, MUSX_EXTENSION);
            if (!result.empty()) return result;
        }
        // Search for `.enigmaxml`
        for (const auto& dir : directories) {
            auto result = findWithExtension(dir, baseName, ENIGMAXML_EXTENSION);
            if (!result.empty()) return result;
        }
        return {};
    };

    // Check user-supplied path
    if (denigmaContext.finaleFilePath.has_value()) {
        const auto& userPath = denigmaContext.finaleFilePath.value();
        if (std::filesystem::is_regular_file(userPath)) {
            // If it's a file, return it directly
            if (fileExists(userPath)) {
                return userPath;
            }
        } else if (std::filesystem::is_directory(userPath)) {
            // If it's a directory, search for `.musx` and `.enigmaxml` in order
            auto result = searchDirectories({userPath}, inputPath.stem());
            if (!result.empty()) {
                return result;
            }
        }
    }

    // Define default search paths: current directory of inputPath and its parent
    std::vector<std::filesystem::path> defaultPaths = {
        inputPath.parent_path(),
        inputPath.parent_path().parent_path()
    };

    // Search default paths for `.musx` and `.enigmaxml` in order
    auto result = searchDirectories(defaultPaths, inputPath.stem());
    if (!result.empty()) {
        return result;
    }

    // Return nullopt if no matching file is found
    return std::nullopt;
}

static std::shared_ptr<MassageMusicXmlContext> createContext(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
    auto context = std::make_shared<MassageMusicXmlContext>();
    context->denigmaContext = denigmaContext;
    auto finaleFilePath = findFinaleFile(inputPath, denigmaContext);
    if (finaleFilePath.has_value()) {
        auto xmlBuffer = [&]() -> Buffer {
            Buffer retval;
            const auto& path = finaleFilePath.value();
            if (path.extension().u8string() == std::string(".") + MUSX_EXTENSION) {
                return enigmaxml::extract(path, denigmaContext);
            } else if (path.extension().u8string() == std::string(".") + ENIGMAXML_EXTENSION) {
                return enigmaxml::read(path, denigmaContext);
            }
            assert(false); // bug in findFinaleFile if here
            return {};
        }();
        if (!xmlBuffer.empty()) {
            context->musxDocument = musx::factory::DocumentFactory::create<MusxReader>(xmlBuffer);
        }
    }
    return context;
}

template <typename T>
pugi::xml_document openXmlDocument(const T& xmlData)
{
    pugi::xml_document xmlDocument;
    auto parseResult = xmlDocument.load_buffer(xmlData.data(), xmlData.size(), pugi::parse_full | pugi::parse_ws_pcdata_single);
    if (parseResult.status != pugi::xml_parse_status::status_ok) {
        throw std::invalid_argument(std::string("Error parsing xml: ") + parseResult.description());
    }
    return xmlDocument;
};

void massage(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
    auto context = createContext(inputPath, denigmaContext);

    if ((inputPath.extension() != std::string(".") + MXL_EXTENSION) || !xmlBuffer.empty()) {
        processFile(openXmlDocument(xmlBuffer), outputPath, context);
        return;
    }

    auto xmlScore = openXmlDocument(ziputils::getMusicXmlScoreFile(inputPath, denigmaContext));
    auto partFileName = !denigmaContext.allPartsAndScore && denigmaContext.partName.has_value() && !denigmaContext.partName.value().empty()
                      ? findPartFileNameByPartName(xmlScore, denigmaContext.partName.value())
                      : std::nullopt;

    bool processedAFile = false;
    auto processPartOrScore = [&](const std::filesystem::path& fileName, const std::string& xmlData) -> bool {
        auto qualifiedOutputPath = outputPath;
        std::shared_ptr<others::PartDefinition> partDef;
        /// @todo figure out which part definition
        auto partFileNamePath = [&]() {
            if (!partFileName.has_value()) {
                return findPartNameByPartFileName(xmlScore, fileName);
            } else if (denigmaContext.partName.has_value()) {
                auto retval = stringutils::utf8ToPath(denigmaContext.partName.value());
                retval.replace_extension(MUSICXML_EXTENSION);
                return retval;
            } else {
                return stringutils::utf8ToPath(partFileName.value());
            }
        }();
        qualifiedOutputPath.replace_extension(partFileNamePath);
        processFile(openXmlDocument(xmlData), qualifiedOutputPath, context, partDef);
        processedAFile = true;
        return denigmaContext.allPartsAndScore; // exit loop if not allPartsAndScore
    };

    if (denigmaContext.allPartsAndScore || denigmaContext.partName.has_value()) {
        ziputils::iterateMusicXmlPartFiles(inputPath, denigmaContext, partFileName, processPartOrScore);
        if (denigmaContext.allPartsAndScore) {
            processFile(std::move(xmlScore), outputPath, context); // must do score last, because std::move
        }
    } else {
        processFile(std::move(xmlScore), outputPath, context);
        processedAFile = true;
    }

    if (!processedAFile && denigmaContext.partName.has_value() && !denigmaContext.allPartsAndScore) {
        if (denigmaContext.partName->empty()) {
            denigmaContext.logMessage(LogMsg() << "No parts were found in document", LogSeverity::Warning);
        } else {
            denigmaContext.logMessage(LogMsg() << "No part name starting with \"" << denigmaContext.partName.value() << "\" was found", LogSeverity::Warning);
        }
    }
}

void massageMxl(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const Buffer& musicXml, const DenigmaContext& denigmaContext)
{
    if (inputPath.extension().u8string() != std::string(".") + MXL_EXTENSION) {
        denigmaContext.logMessage(LogMsg() << inputPath.u8string() << " is not a .mxl file.", LogSeverity::Error);
        return;
    }
    std::filesystem::path qualifiedOutputPath = calcQualifiedOutputPath(outputPath);
    if (!denigma::validatePathsAndOptions(qualifiedOutputPath, denigmaContext)) {
        return;
    }

    auto context = createContext(inputPath, denigmaContext);
    ziputils::iterateModifyFilesInPlace(inputPath, qualifiedOutputPath, denigmaContext, [&](const std::filesystem::path& fileName, std::string& fileContents, bool isScore) {
        denigmaContext.logMessage(LogMsg() << ">>>>>>>>>> Processing zipped file " << fileName.u8string() << " <<<<<<<<<<");
        if (fileName.extension().u8string() == std::string(".") + MUSICXML_EXTENSION) {
            auto xmlDocument = openXmlDocument(fileContents);
            std::shared_ptr<others::PartDefinition> partDef;
            if (!isScore) {
                /// @todo figure out which part definition
            }
            processXml(xmlDocument, context, partDef);
            std::stringstream ss;
            pugi::xml_writer_stream writer(ss);
            xmlDocument.save(writer, INDENT_SPACES);
            fileContents = ss.str();
        }
        return true; // always save the file back, even if we didn't modify it
    });
}

} // namespace musicxml
} // namespace denigma
