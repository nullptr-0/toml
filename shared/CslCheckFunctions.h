#pragma once

#ifndef CSL_CHECK_FUNCTIONS_H
#define CSL_CHECK_FUNCTIONS_H

#include <tuple>
#include <string>
#include <regex>
#include "Type.h"
#include "TypeUtils.h"
#include "CslOperators.h"

namespace CSL {
#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckIdentifier(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckIdentifier(std::string strToCheck) {
        size_t identifierStartIndex;
        std::string identifierContent;
        std::regex identifierRegex(R"(^(\s*)([a-zA-Z_][a-zA-Z0-9_]*))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, identifierRegex) && !match.prefix().length()) {
            identifierStartIndex = match[1].length();
            identifierContent = match[0].str().substr(identifierStartIndex);
            if (identifierContent == "true" || identifierContent == "false") {
                identifierStartIndex = 0;
                identifierContent = "";
            }
        }
        return { identifierStartIndex, identifierContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<Type::Type*, size_t, std::string> CheckNumericLiteral(std::string strToCheck);
#else
    std::tuple<Type::Type*, size_t, std::string> CheckNumericLiteral(std::string strToCheck) {
        Type::Type* literalType = nullptr;
        size_t literalStartIndex;
        std::string literalContent;
        std::regex integerLiteralRegex(R"(^(\s*)(0(?![xob])|[1-9]+(_?\d+)*|0x[\da-fA-F]+(_?[\da-fA-F]+)*|0o[0-7]+(_?[0-7]+)*|0b[01]+(_?[01]+)*))");
        std::regex floatLiteralRegex(R"(^(\s*)((0(?![xob])|[1-9]+(_?\d+)*)(\.((\d+_)*\d+))?(e[-+]?\d+(_?\d+)*)?))");
        std::regex specialNumLiteralRegex(R"(^(\s*)((nan|inf)(?![-\w])))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, specialNumLiteralRegex) && !match.prefix().length()) {
            auto matchedStr = match[3].str();
            literalType = new Type::SpecialNumber(matchedStr == "nan" ? Type::SpecialNumber::NaN : Type::SpecialNumber::Infinity);
        }
        else {
            bool matched = false;
            std::smatch integerMatch;
            std::smatch floatMatch;
            if (std::regex_search(strToCheck, integerMatch, integerLiteralRegex) && !integerMatch.prefix().length()) {
                matched = true;
            }
            if (std::regex_search(strToCheck, floatMatch, floatLiteralRegex) && !floatMatch.prefix().length()) {
                matched = true;
            }
            if (matched) {
                if (integerMatch[0].length() >= floatMatch[0].length()) {
                    match = integerMatch;
                    literalType = new Type::Integer();
                }
                else {
                    match = floatMatch;
                    literalType = new Type::Float();
                }
            }
        }
        if (literalType) {
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
            auto [identifierStartIndex, identifierContent] = CheckIdentifier(strToCheck);
            if (literalContent.length() < identifierContent.length()) {
                literalStartIndex = 0;
                literalContent = "";
                Type::DeleteType(literalType);
                literalType = nullptr;
            }
        }
        return { literalType, literalStartIndex, literalContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<Type::Type*, size_t, std::string> CheckBooleanLiteral(std::string strToCheck);
#else
    std::tuple<Type::Type*, size_t, std::string> CheckBooleanLiteral(std::string strToCheck) {
        Type::Type* literalType = nullptr;
        size_t literalStartIndex;
        std::string literalContent;
        std::regex boolLiteralRegex(R"(^(\s*)((true|false)(?![-\w])))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, boolLiteralRegex) && !match.prefix().length()) {
            literalType = new Type::Boolean();
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
        }
        return { literalType, literalStartIndex, literalContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckKeyword(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckKeyword(std::string strToCheck) {
        size_t keywordStartIndex;
        std::string keywordContent;
        std::regex keywordRegex(R"(^(\s*)((config|constraints|requires|conflicts|with|validate|exists|count_keys|all_keys|wildcard_keys|subset|\*)(?![-\w])))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, keywordRegex) && !match.prefix().length()) {
            keywordStartIndex = match[1].length();
            keywordContent = match[0].str().substr(keywordStartIndex);
        }
        return { keywordStartIndex, keywordContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckType(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckType(std::string strToCheck) {
        size_t keywordStartIndex;
        std::string keywordContent;
        std::regex keywordRegex(R"(^(\s*)((any\{\}|any\[\]|string|number|boolean|datetime|duration)(?![-\w])))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, keywordRegex) && !match.prefix().length()) {
            keywordStartIndex = match[1].length();
            keywordContent = match[0].str().substr(keywordStartIndex);
        }
        return { keywordStartIndex, keywordContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckOperator(std::string strToCheck);
#else
    std::string escapeRegex(const std::string& str) {
        static const std::string specialChars = ".^$|()[]{}*+?\\";
        std::string escaped;
        for (char c : str) {
            if (specialChars.find(c) != std::string::npos) {
                escaped += '\\';
            }
            escaped += c;
        }
        return escaped;
    }

    std::tuple<size_t, std::string> CheckOperator(std::string strToCheck) {
        std::vector<std::string> operatorStrList;
        for (const auto& pair : CSLOperator::operators) {
            operatorStrList.push_back(pair.first.operatorText);
        }
        std::sort(operatorStrList.begin(), operatorStrList.end(), [](const std::string& a, const std::string& b) -> bool {
            return a.size() > b.size();
            });
        std::string regexPattern = "^(\\s*)(";
        for (auto i = operatorStrList.begin(); i != operatorStrList.end(); ++i) {
            regexPattern += escapeRegex(*i);
            if (i != std::prev(operatorStrList.end())) {
                regexPattern += "|";
            }
        }
        regexPattern += ")";
        size_t operatorStartIndex;
        std::string operatorContent;
        std::regex operatorRegex(regexPattern);
        std::smatch match;
        if (std::regex_search(strToCheck, match, operatorRegex) && !match.prefix().length()) {
            operatorStartIndex = match[1].length();
            operatorContent = match[0].str().substr(operatorStartIndex);
        }
        return { operatorStartIndex, operatorContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<Type::Type*, size_t, std::string> CheckDateTimeLiteral(std::string strToCheck);
#else
    bool isLeapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    bool isValidDatePart(const std::string& part, int min, int max) {
        if (part.empty() || part.size() > 2 || !std::all_of(part.begin(), part.end(), ::isdigit)) {
            return false;
        }
        int value = std::stoi(part);
        return value >= min && value <= max;
    }

    bool isValidDate(const std::string& dateStr) {
        if (dateStr.length() != 10 || dateStr[4] != '-' || dateStr[7] != '-') {
            return false;
        }

        std::string yearStr = dateStr.substr(0, 4);
        std::string monthStr = dateStr.substr(5, 2);
        std::string dayStr = dateStr.substr(8, 2);

        // Check if all characters are digits
        if (!std::all_of(yearStr.begin(), yearStr.end(), ::isdigit)) return false;

        int year = std::stoi(yearStr);
        int month = std::stoi(monthStr);
        int day = std::stoi(dayStr);

        if (year < 1 || month < 1 || month > 12) return false;

        // Days in each month
        int daysInMonth[] = { 31, isLeapYear(year) ? 29 : 28, 31, 30, 31, 30,
                              31, 31, 30, 31, 30, 31 };

        if (day < 1 || day > daysInMonth[month - 1]) return false;

        return true;
    }

    std::tuple<Type::Type*, size_t, std::string> CheckDateTimeLiteral(std::string strToCheck) {
        Type::Type* literalType = nullptr;
        size_t literalStartIndex;
        std::string literalContent;
        std::regex offsetDateTimeRegex(R"(^(\s*)((\d{4}-\d{2}-\d{2})[Tt ]([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?([Zz]|[+-]([01]\d|2[0-3]):[0-5]\d)))");
        std::regex localDateTimeRegex(R"(^(\s*)((\d{4}-\d{2}-\d{2})[Tt ]([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?))");
        std::regex localDateRegex(R"(^(\s*)(\d{4}-\d{2}-\d{2}))");
        std::regex localTimeRegex(R"(^(\s*)(([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, offsetDateTimeRegex) && !match.prefix().length() && isValidDate(match[3].str())) {
            literalType = new Type::DateTime(Type::DateTime::OffsetDateTime);
        }
        else if (std::regex_search(strToCheck, match, localDateTimeRegex) && !match.prefix().length() && isValidDate(match[3].str())) {
            literalType = new Type::DateTime(Type::DateTime::LocalDateTime);
        }
        else if (std::regex_search(strToCheck, match, localDateRegex) && !match.prefix().length() && isValidDate(match[2].str())) {
            literalType = new Type::DateTime(Type::DateTime::LocalDate);
        }
        else if (std::regex_search(strToCheck, match, localTimeRegex) && !match.prefix().length()) {
            literalType = new Type::DateTime(Type::DateTime::LocalTime);
        }
        if (literalType) {
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
        }
        return { literalType, literalStartIndex, literalContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckPunctuator(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckPunctuator(std::string strToCheck) {
        size_t punctuatorStartIndex;
        std::string punctuatorContent;
        std::regex punctuatorRegex(R"(^(\s*)(\{|\}|\[|\]|,|:|;|@|=>))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, punctuatorRegex) && !match.prefix().length()) {
            punctuatorStartIndex = match[1].length();
            punctuatorContent = match[0].str().substr(punctuatorStartIndex);
        }
        return { punctuatorStartIndex, punctuatorContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckComment(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckComment(std::string strToCheck) {
        size_t commentStartIndex;
        std::string commentContent;
        std::regex commentRegex(R"(^(\s*)(//[^\n]*))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, commentRegex) && !match.prefix().length()) {
            commentStartIndex = match[1].length();
            commentContent = match[0].str().substr(commentStartIndex);
        }
        return { commentStartIndex, commentContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<Type::Type*, size_t, std::string> CheckStringLiteral(std::string strToCheck);
#else
    std::tuple<Type::Type*, size_t, std::string> CheckStringLiteral(std::string strToCheck) {
        Type::Type* literalType = nullptr;
        size_t literalStartIndex;
        std::string literalContent;
        std::regex stringLiteralRegex(R"(^(\s*)((\"([^\"\\]|\\.)*\")|(R\"([^()\\]{0,16})\(((.|\n)*?)\)\6\")))");
        std::smatch match;
        if (std::regex_search(strToCheck, match, stringLiteralRegex) && !match.prefix().length()) {
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
            literalType = new Type::String(literalContent[0] == 'R' ? (literalContent.find('\n') == std::string::npos ? Type::String::Raw : Type::String::MultiLineRaw) : (literalContent.find('\n') == std::string::npos ? Type::String::Basic : Type::String::MultiLineBasic));
        }
        return { literalType, literalStartIndex, literalContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern bool HasIncompleteString(const std::string& input);
#else
    std::string firstAppearedStringOrCommentStarter(const std::string& input) {
        size_t posBasicString = input.find("\"");
        size_t posRawString = input.find("R\"");
        size_t posComment = input.find("//");

        // Set to string::npos if not found
        if (posBasicString == std::string::npos) posBasicString = input.length() + 1;
        if (posRawString == std::string::npos) posRawString = input.length() + 1;
        if (posComment == std::string::npos) posComment = input.length() + 1;

        if (posBasicString < posRawString && posBasicString < posComment) {
            return "\"";
        }
        else if (posRawString < posBasicString && posRawString < posComment) {
            return "R\"";
        }
        else if (posComment < posBasicString && posComment < posRawString) {
            return "//";
        }
        else {
            return "";
        }
    }

    bool HasIncompleteString(const std::string& input) {
        std::regex commentRegex(R"((\s*)(//[^\n]*))");
        std::regex stringLiteralRegex(R"(^(\s*)((\"([^\"\\]|\\.)*\")|(R\"([^()\\]{0,16})\(((.|\n)*?)\)\6\")))");
        std::regex stringStartRegex(R"(("|R"))");

        bool commentAppearsFirst = firstAppearedStringOrCommentStarter(input) == "//";

        auto cleanedInput = std::regex_replace(std::regex_replace(input, commentAppearsFirst ? commentRegex : stringLiteralRegex, ""), commentAppearsFirst ? stringLiteralRegex : commentRegex, "");

        // Check if the input starts with a string start but isn't a complete string
        std::smatch match;
        if (std::regex_search(cleanedInput, match, stringStartRegex)) {
            return true;
        }

        return false;
    }
#endif
}

#endif
