/*
 * Copyright (C) 2024, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to do so, subject to the
 * following conditions:
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

#include "xml_compare.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <string_view>

#include "pugixml.hpp"

namespace {

constexpr double XML_NUMERIC_TOLERANCE = 1e-6;
constexpr double MSS_MEASURE_NUMBER_POS_BELOW_Y_TOLERANCE = 0.025;

bool parseDoubleValue(const std::string& text, double& value)
{
    if (text.empty()) {
        return false;
    }
    char* endPtr = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &endPtr);
    if (endPtr == text.c_str() || *endPtr != '\0' || errno == ERANGE) {
        return false;
    }
    value = parsed;
    return true;
}

double numericToleranceForComparison(bool isMssComparison,
                                     std::string_view currentPath,
                                     std::string_view attributeName)
{
    if (isMssComparison
        && currentPath == "/museScore/Style/measureNumberPosBelow"
        && attributeName == "y") {
        return MSS_MEASURE_NUMBER_POS_BELOW_Y_TOLERANCE;
    }
    return XML_NUMERIC_TOLERANCE;
}

bool valuesNearlyEqual(const std::string& lhs, const std::string& rhs, double tolerance)
{
    if (lhs == rhs) {
        return true;
    }
    double leftValue = 0.0;
    double rightValue = 0.0;
    if (parseDoubleValue(lhs, leftValue) && parseDoubleValue(rhs, rightValue)) {
        return std::fabs(leftValue - rightValue) <= tolerance;
    }
    return false;
}

bool isWhitespaceOnly(const char* text)
{
    if (!text) {
        return true;
    }
    while (*text) {
        if (!std::isspace(static_cast<unsigned char>(*text++))) {
            return false;
        }
    }
    return true;
}

bool isIgnorableNode(const pugi::xml_node& node)
{
    switch (node.type()) {
        case pugi::node_comment:
        case pugi::node_declaration:
            return true;
        case pugi::node_pcdata:
        case pugi::node_cdata:
            return isWhitespaceOnly(node.value());
        default:
            break;
    }
    return false;
}

std::string describeNode(const pugi::xml_node& node)
{
    switch (node.type()) {
        case pugi::node_element:
            return node.name();
        case pugi::node_pcdata:
        case pugi::node_cdata:
            return std::string("text()");
        default:
            break;
    }
    return std::string("node");
}

bool compareXmlNodes(const pugi::xml_node& lhs,
                     const pugi::xml_node& rhs,
                     const std::string& currentPath,
                     std::string& message,
                     bool isMssComparison)
{
    if (lhs.type() != rhs.type()) {
        message = currentPath + ": node type mismatch";
        return false;
    }

    if (lhs.type() == pugi::node_element) {
        if (std::strcmp(lhs.name(), rhs.name()) != 0) {
            message = currentPath + ": element name mismatch";
            return false;
        }

        auto buildAttributeMap = [](const pugi::xml_node& node) {
            std::map<std::string, std::string> attributes;
            for (auto attribute = node.first_attribute(); attribute; attribute = attribute.next_attribute()) {
                attributes.emplace(attribute.name(), attribute.value());
            }
            return attributes;
        };

        const auto lhsAttributes = buildAttributeMap(lhs);
        const auto rhsAttributes = buildAttributeMap(rhs);
        if (lhsAttributes.size() != rhsAttributes.size()) {
            message = currentPath + ": attribute count mismatch";
            return false;
        }
        for (const auto& [name, value] : lhsAttributes) {
            const auto it = rhsAttributes.find(name);
            if (it == rhsAttributes.end()) {
                message = currentPath + ": missing attribute '" + name + "'";
                return false;
            }
            const double tolerance = numericToleranceForComparison(isMssComparison, currentPath, name);
            if (!valuesNearlyEqual(value, it->second, tolerance)) {
                message = currentPath + ": attribute '" + name + "' differs: " + value + " vs." + it->second
                        + " (tolerance " + std::to_string(tolerance) + ")";
                return false;
            }
        }

        auto nextChild = [](pugi::xml_node node) {
            while (node && isIgnorableNode(node)) {
                node = node.next_sibling();
            }
            return node;
        };

        auto lhsChild = nextChild(lhs.first_child());
        auto rhsChild = nextChild(rhs.first_child());
        while (lhsChild && rhsChild) {
            const std::string childPath = currentPath + "/" + describeNode(lhsChild);
            if (!compareXmlNodes(lhsChild, rhsChild, childPath, message, isMssComparison)) {
                return false;
            }
            lhsChild = nextChild(lhsChild.next_sibling());
            rhsChild = nextChild(rhsChild.next_sibling());
        }
        if (lhsChild || rhsChild) {
            message = currentPath + ": child count mismatch";
            return false;
        }
        return true;
    }

    if (lhs.type() == pugi::node_pcdata || lhs.type() == pugi::node_cdata) {
        const double tolerance = numericToleranceForComparison(isMssComparison, currentPath, "");
        if (!valuesNearlyEqual(lhs.value(), rhs.value(), tolerance)) {
            message = currentPath + ": text differs: " + lhs.value() + " vs." + rhs.value()
                    + " (tolerance " + std::to_string(tolerance) + ")";
            return false;
        }
        return true;
    }

    if (std::strcmp(lhs.value(), rhs.value()) != 0) {
        message = currentPath + ": node value differs: " + lhs.value() + " vs." + rhs.value();
        return false;
    }
    return true;
}

pugi::xml_node documentElement(const pugi::xml_document& document)
{
    for (auto node = document.first_child(); node; node = node.next_sibling()) {
        if (node.type() == pugi::node_element) {
            return node;
        }
    }
    return {};
}

} // namespace

bool compareXmlFiles(const std::filesystem::path& path1,
                     const std::filesystem::path& path2,
                     std::string& message)
{
    pugi::xml_document document1;
    pugi::xml_document document2;
    const auto result1 = document1.load_file(path1.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
    const auto result2 = document2.load_file(path2.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
    if (!result1) {
        message = std::string("unable to parse ") + path1.u8string() + ": " + result1.description();
        return false;
    }
    if (!result2) {
        message = std::string("unable to parse ") + path2.u8string() + ": " + result2.description();
        return false;
    }
    const auto root1 = documentElement(document1);
    const auto root2 = documentElement(document2);
    if (!root1 || !root2) {
        message = "documents missing root element for comparison";
        return false;
    }
    const std::string rootPath = std::string("/") + root1.name();
    return compareXmlNodes(root1, root2, rootPath, message, isMssComparison);
}

bool shouldUseXmlComparison(const std::filesystem::path& path)
{
    std::string extension = path.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension == ".mss" || extension == ".xml";
}
    auto lowerExtension = [](const std::filesystem::path& path) {
        std::string extension = path.extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return extension;
    };
    const bool isMssComparison = (lowerExtension(path1) == ".mss" && lowerExtension(path2) == ".mss");

