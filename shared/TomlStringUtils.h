#pragma once

#ifndef TOML_STRING_UTILS_H
#define TOML_STRING_UTILS_H

#include <string>
#include <cstdint>
#include <stdexcept>
#include <regex>
#include <limits>

#ifndef DEF_GLOBAL
extern std::string extractStringLiteralContent(const std::string& stringLiteral, int type);
#else
// Helper function to append Unicode code point as UTF-8
void appendUtf8(std::string& str, uint32_t codePoint) {
    if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        throw std::invalid_argument("Invalid Unicode code point");
    }

    if (codePoint <= 0x7F) {
        str += static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7FF) {
        str += static_cast<char>(0xC0 | (codePoint >> 6));
        str += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0xFFFF) {
        str += static_cast<char>(0xE0 | (codePoint >> 12));
        str += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        str += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else {
        str += static_cast<char>(0xF0 | (codePoint >> 18));
        str += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        str += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        str += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

std::string removeLineContinuations(const std::string& input) {
    std::istringstream stream(input);
    std::string result;
    std::string currentLine;
    bool continuing = false;
    bool trailingNewLine = input.size() && input.back() == '\n';

    std::string line;
    while (std::getline(stream, line)) {
        // Remove any carriage return if present (for Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t backslashCount = 0;

        // Count trailing backslashes
        for (size_t i = line.length(); i > 0 && (line[i - 1] == '\\' || std::string(" \t\f\r\n\v").find(line[i - 1]) != std::string::npos); --i) {
            if (line[i - 1] == '\\') {
                ++backslashCount;
            }
        }

        bool isContinuation = (backslashCount % 2 == 1);

        if (continuing) {
            // Strip leading whitespace from this line
            size_t firstNonWs = line.find_first_not_of(" \t\f\r\v");
            if (firstNonWs != std::string::npos) {
                line = line.substr(firstNonWs);
            }
            else { // line was all whitespace
                line.clear();
                continue;
            }
        }

        if (isContinuation) {
            // Remove one backslash from the end
            line.erase(line.find_last_of('\\'), std::string::npos);
            currentLine += line;
            continuing = true;
        }
        else {
            currentLine += line;
            result += currentLine + (stream.peek() == -1 && !trailingNewLine ? "" : "\n");
            currentLine.clear();
            continuing = false;
        }
    }

    // Add any leftover line (if input didn't end with newline)
    if (!currentLine.empty()) {
        result += currentLine;
    }

    return result;
}

// Process basic string escapes
std::string unescapeBasicString(const std::string& content, bool isMultiLine) {
    std::string processed = content;

    // Handle line continuations for multi-line
    if (isMultiLine) {
        processed = removeLineContinuations(processed);
    }

    std::string result;
    for (size_t i = 0; i < processed.size(); ++i) {
        if (processed[i] == '\\') {
            if (i + 1 >= processed.size()) {
                throw std::invalid_argument("Dangling backslash in string");
            }

            char c = processed[++i];
            switch (c) {
            case 'b':  result += '\b'; break;
            case 't':  result += '\t'; break;
            case 'n':  result += '\n'; break;
            case 'f':  result += '\f'; break;
            case 'r':  result += '\r'; break;
            case '"':   result += '"';  break;
            case '\\':  result += '\\'; break;
            case 'u': {
                if (i + 4 >= processed.size()) {
                    throw std::invalid_argument("Invalid \\u escape");
                }
                std::string hex = processed.substr(i + 1, 4);
                i += 4;
                uint32_t code = std::stoul(hex, nullptr, 16);
                if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
                    throw std::invalid_argument("Invalid Unicode code point");
                }
                appendUtf8(result, code);
                break;
            }
            case 'U': {
                if (i + 8 >= processed.size()) {
                    throw std::invalid_argument("Invalid \\U escape");
                }
                std::string hex = processed.substr(i + 1, 8);
                i += 8;
                uint32_t code = std::stoul(hex, nullptr, 16);
                if (code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
                    throw std::invalid_argument("Invalid Unicode code point");
                }
                appendUtf8(result, code);
                break;
            }
            default:
                throw std::invalid_argument(std::string("Invalid escape sequence: \\") + c);
            }
        }
        else {
            result += processed[i];
        }
    }
    return result;
}

// Process literal strings
std::string processLiteralString(const std::string& content, bool isMultiLine) {
    std::string result = content;
    // Trim leading newline for multi-line
    if (isMultiLine && !result.empty() && result[0] == '\n') {
        result.erase(0, 1);
    }
    return result;
}

std::string extractStringLiteralContent(const std::string& stringLiteral, int type) {
    switch (type) {
    case 0: { // Basic string
        std::string content = stringLiteral.substr(1, stringLiteral.length() - 2);
        return unescapeBasicString(content, false);
    }
    case 1: { // Multi-line basic string
        std::string content = stringLiteral.substr(3, stringLiteral.length() - 6);
        if (!content.empty() && content[0] == '\n') content.erase(0, 1);
        return unescapeBasicString(content, true);
    }
    case 2: { // Literal string
        std::string content = stringLiteral.substr(1, stringLiteral.length() - 2);
        return processLiteralString(content, false);
    }
    case 3: { // Multi-line literal string
        std::string content = stringLiteral.substr(3, stringLiteral.length() - 6);
        if (!content.empty() && content[0] == '\n') content.erase(0, 1);
        return processLiteralString(content, true);
    }
    default:
        throw std::invalid_argument("Not a valid string type");
    }
}

#endif

#ifndef DEF_GLOBAL
extern std::string convertToDecimalString(std::string input);
#else
std::string convertToDecimalString(std::string input) {
    if (input.empty()) {
        return "Empty string";
    }
    bool isNeg = input[0] == '-';
    if (isNeg) {
        input = input.substr(1);
    }
    int base = 10;
    size_t start = 0;

    if (input.size() > 2 && input[0] == '0') {
        if (input[1] == 'x') {
            base = 16;
            start = 2;
        }
        else if (input[1] == 'o') {
            base = 8;
            start = 2;
        }
        else if (input[1] == 'b') {
            base = 2;
            start = 2;
        }
    }

    std::string numberPart = input.substr(start);

    try {
        unsigned long long value = std::stoull(numberPart, nullptr, base);
        return (isNeg && value ? "-" : "") + std::to_string(value);
    }
    catch (const std::invalid_argument&) {
        return "Invalid input";
    }
    catch (const std::out_of_range&) {
        return "Number out of range";
    }
}
#endif

#endif
