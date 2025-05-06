#pragma once

#ifndef CSL_STRING_UTILS_H
#define CSL_STRING_UTILS_H

#include <string>
#include <cstdint>
#include <stdexcept>
#include <regex>
#include <limits>

#ifndef DEF_GLOBAL
extern std::string extractStringLiteralContent(const std::string& stringLiteral);
extern std::string extractQuotedIdentifierContent(const std::string& quotedIdentifier);
#else
std::string unicodeCodePointToUTF8(uint32_t codePoint) {
    std::string utf8;
    if (codePoint <= 0x7F) {
        // 1-byte UTF-8
        utf8 += static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7FF) {
        // 2-byte UTF-8
        utf8 += static_cast<char>(0xC0 | (codePoint >> 6));
        utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0xFFFF) {
        // 3-byte UTF-8
        utf8 += static_cast<char>(0xE0 | (codePoint >> 12));
        utf8 += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0x10FFFF) {
        // 4-byte UTF-8
        utf8 += static_cast<char>(0xF0 | (codePoint >> 18));
        utf8 += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        utf8 += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        utf8 += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else {
        // Invalid code point (replace with Unicode replacement character)
        utf8 += "\xEF\xBF\xBD";
    }
    return utf8;
}

std::string processEscapeSequences(const std::string& input) {
    std::string result;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\') {
            if (i + 1 >= input.size()) {
                result += '\\';
                continue;
            }
            char next = input[i + 1];
            switch (next) {
            // Standard escape sequences
            case 'a': result += '\a'; i++; break;
            case 'b': result += '\b'; i++; break;
            case 'f': result += '\f'; i++; break;
            case 'n': result += '\n'; i++; break;
            case 'r': result += '\r'; i++; break;
            case 't': result += '\t'; i++; break;
            case 'v': result += '\v'; i++; break;
            case '\\': result += '\\'; i++; break;
            case '\?': result += '\?'; i++; break;
            case '\'': result += '\''; i++; break;
            case '\"': result += '\"'; i++; break;
            case '`': result += '`'; i++; break;

            // Octal escape (up to 3 digits)
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': {
                int val = 0;
                int digits = 0;
                for (int j = 0; j < 3 && (i + 1 + j) < input.size(); ++j) {
                    char c = input[i + 1 + j];
                    if (c < '0' || c > '7') break;
                    val = val * 8 + (c - '0');
                    digits++;
                }
                result += static_cast<char>(val);
                i += digits;
                break;
            }

            // Hexadecimal escape (variable length)
            case 'x': {
                size_t j = i + 2;
                int val = 0;
                int digits = 0;
                while (j < input.size() && std::isxdigit(input[j])) {
                    char c = input[j];
                    val = val * 16 + (c >= '0' && c <= '9' ? c - '0' :
                        (c >= 'a' && c <= 'f' ? 10 + c - 'a' :
                            10 + c - 'A'));
                    digits++;
                    j++;
                }
                if (digits == 0) {
                    result += 'x';
                    i++;
                }
                else {
                    result += static_cast<char>(val);
                    i = j - 1;
                }
                break;
            }

            // Unicode escape sequences
            case 'u': {
                const int hexDigits = 4;
                if (i + 1 + hexDigits >= input.size()) {
                    result += 'u';
                    i++;
                    break;
                }
                std::string hexStr = input.substr(i + 2, hexDigits);
                bool valid = (hexStr.size() == hexDigits);
                for (char c : hexStr) {
                    if (!std::isxdigit(c)) valid = false;
                }
                if (!valid) {
                    result += 'u';
                    i++;
                }
                else {
                    uint32_t codePoint = std::stoul(hexStr, nullptr, 16);
                    result += unicodeCodePointToUTF8(codePoint);
                    i += 1 + hexDigits; // Skip \u and 4 digits
                }
                break;
            }

            case 'U': {
                const int hexDigits = 8;
                if (i + 1 + hexDigits >= input.size()) {
                    result += 'U';
                    i++;
                    break;
                }
                std::string hexStr = input.substr(i + 2, hexDigits);
                bool valid = (hexStr.size() == hexDigits);
                for (char c : hexStr) {
                    if (!std::isxdigit(c)) valid = false;
                }
                if (!valid) {
                    result += 'U';
                    i++;
                }
                else {
                    uint32_t codePoint = std::stoul(hexStr, nullptr, 16);
                    result += unicodeCodePointToUTF8(codePoint);
                    i += 1 + hexDigits; // Skip \U and 8 digits
                }
                break;
            }

            // Invalid escape: remove backslash
            default:
                result += next;
                i++;
                break;
            }
        }
        else {
            result += input[i];
        }
    }
    return result;
}

std::string extractStringLiteralContent(const std::string& stringLiteral) {
    std::regex stringLiteralRegex(R"(^(\s*)((\"([^\"\\]|\\.)*\")|(R\"([^()\\]{0,16})\(((.|\n)*?)\)\6\")))");
    std::smatch match;

    if (!std::regex_match(stringLiteral, match, stringLiteralRegex)) {
        throw std::invalid_argument("Input is not a valid string literal");
    }

    if (match[3].matched) { // Regular string
        auto matchedStr = match[3].str();
        return processEscapeSequences(matchedStr.substr(1, matchedStr.length() - 2));
    }
    else if (match[5].matched) { // Raw string
        return match[7].str();
    }

    throw std::invalid_argument("Unexpected string literal format");
}

std::string extractQuotedIdentifierContent(const std::string& quotedIdentifier) {
    std::regex quotedIdentifierRegex(R"(^(\s*)((`([^`\\]|\\.)*`)|(R`([^()\\]{0,16})\(((.|\n)*?)\)\6`)))");
    std::smatch match;

    if (!std::regex_match(quotedIdentifier, match, quotedIdentifierRegex)) {
        throw std::invalid_argument("Input is not a valid quoted identifier");
    }

    if (match[3].matched) { // Regular quoted identifier
        auto matchedStr = match[3].str();
        return processEscapeSequences(matchedStr.substr(1, matchedStr.length() - 2));
    }
    else if (match[5].matched) { // Raw quoted identifier
        return match[7].str();
    }

    throw std::invalid_argument("Unexpected quoted identifier format");
}
#endif

#endif
