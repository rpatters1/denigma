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

#include "musx/musx.h"
#include "pugixml.hpp"

#include "denigma.h"
#include "massage/musicxml.h"
#include "util/ziputils.h"

constexpr static double EDU_PER_QUARTER = 1024.0;

namespace denigma {
namespace musicxml {

struct MassageMusicXmlContext
{
    DenigmaContext denigmaContext;
    pugi::xml_document xmlDocument;
    musx::dom::DocumentPtr musxDocument;
    int currentPart{};
    int currentMeasure{};
    int currentStaff{};
    int currentStaffOffset{};
    int errorCount{};

    // Option booleans
    bool refloatRests{false};
    bool extendOttavasLeft{true};
    bool extendOttavasRight{true};
    bool fermataWholeRests{false};

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

static Buffer extract(const std::filesystem::path& inputPath, const DenigmaContext& denigmaContext)
{
    /// @todo Find and extract the part if it is specified in denigmaContext
    std::string buffer = ziputils::getMusicXmlRootFile(inputPath, denigmaContext);
    return Buffer(buffer.begin(), buffer.end());
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
#include <pugixml.hpp>
#include <functional>
#include <iostream>
#include <string>

// Static function that returns an iterator for filtered direction nodes
static pugi::xml_node getNextDirectionOfType(pugi::xml_node parent, const std::string& nodeName) {
    while (parent) {
        if (auto nodeForType = parent.child("direction-type").child(nodeName.c_str())) {
            return parent; // Found the matching node
        }
        parent = parent.next_sibling("direction");
    }
    return pugi::xml_node(); // Null node to indicate end
}

static void fixDirectionBrackets(pugi::xml_node xmlMeasure, const std::string& directionType, const std::shared_ptr<MassageMusicXmlContext>& context)
{
    if (!(context->extendOttavasLeft || context->extendOttavasRight)) return;

    auto currentDirection = xmlMeasure.child("direction");

    while (currentDirection) {
        currentDirection = getNextDirectionOfType(currentDirection, directionType);
        if (!currentDirection) break;

        auto xmlDirectionType = currentDirection.child("direction-type");
        if (!xmlDirectionType) continue;

        auto nodeForType = xmlDirectionType.child(directionType.c_str());
        if (!nodeForType) continue;

        auto directionCopy = currentDirection; // Shallow copy
        std::string shiftType = nodeForType.attribute("type").value();

        // keep this for next iteration
        auto nextDirection = currentDirection.next_sibling("direction");

        if (shiftType == "stop") {
            if (context->extendOttavasRight) {
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
            if (context->extendOttavasLeft) {
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
        currentDirection = nextDirection;
    }
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

void processXml(pugi::xml_node scorePartWiseNode, const std::shared_ptr<MassageMusicXmlContext>& context)
{
    if (!context->musxDocument && context->refloatRests) {
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
            if (context->musxDocument && context->refloatRests) {
                for (int staffNum = 1; staffNum <= stavesUsed; ++staffNum) {
                    context->currentStaffOffset = staffNum - 1;
                    processXmlWithFinaleDocument(
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

            if (context->fermataWholeRests) {
                fixFermataWholeRests(xmlMeasure, context);
            }
        }

        context->currentPart++;
        context->currentStaff += stavesUsed;
        context->currentStaffOffset = 0;
    }
}

static void processFile(const std::filesystem::path& outputPath, const Buffer &xmlBuffer, const DenigmaContext& denigmaContext, const std::shared_ptr<others::PartDefinition>& part = nullptr)
{
    std::filesystem::path qualifiedOutputPath = outputPath;
    qualifiedOutputPath.replace_extension(".massaged" + outputPath.extension().u8string());
    if (!denigma::validatePathsAndOptions(qualifiedOutputPath, denigmaContext)) {
        return;
    }

    auto context = std::make_shared<MassageMusicXmlContext>();
    context->denigmaContext = denigmaContext;
    pugi::xml_document& xmlDocument = context->xmlDocument;
    auto parseResult = xmlDocument.load_buffer(xmlBuffer.data(), xmlBuffer.size(), pugi::parse_full | pugi::parse_ws_pcdata_single);
    if (parseResult.status != pugi::xml_parse_status::status_ok) {
        throw std::invalid_argument(std::string("Error parsing xml: ") + parseResult.description());
    }
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

    softwareElement.text().set((denigmaContext.programName + ' ' + DENIGMA_VERSION).c_str());

    std::string originalEncodingDate = encodingDateElement.text().get();
    encodingDateElement.text().set(getTimeStamp("%Y-%m-%d").c_str());

    pugi::xml_node miscellaneousElement = encodingElement.child("miscellaneous");
    if (!miscellaneousElement) {
        miscellaneousElement = encodingElement.append_child("miscellaneous");
    }
    
    auto insertMiscellaneousField = [&](const std::string& name, const std::string& value) {
        pugi::xml_node element = miscellaneousElement.append_child("miscellaneous-field");
        element.append_attribute("name").set_value(name.c_str());
        element.text().set(value.c_str());
    };

    // Insert fields
    insertMiscellaneousField("original-software", creatorSoftware);
    insertMiscellaneousField("original-encoding-date", originalEncodingDate);

    /// @todo set the options
    // for (const auto& option : options) {
    //    insertMiscellaneousField(option.field, option.value);
    // }

    processXml(scorePartwise, context);

    std::ofstream outputFile;
    outputFile.exceptions(std::ios::failbit | std::ios::badbit);
    outputFile.open(qualifiedOutputPath);
    pugi::xml_writer_stream writer(outputFile);
    xmlDocument.save(writer, "    "); // 2 spaces for indentation
    outputFile.close();
}

void massage(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, const Buffer& xmlBuffer, const DenigmaContext& denigmaContext)
{
    if ((inputPath.extension() != std::string(".") + MXL_EXTENSION) || !xmlBuffer.empty()) {
        processFile(outputPath, xmlBuffer, denigmaContext);
        return;
    }

    /// @todo add code to search for parts.
    Buffer extractedXml = musicxml::extract(inputPath, denigmaContext);
    processFile(outputPath, extractedXml, denigmaContext);
}

} // namespace musicxml
} // namespace denigma
