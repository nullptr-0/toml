#pragma once

#ifndef TOML_CHECK_FUNCTIONS_H
#define TOML_CHECK_FUNCTIONS_H

#include <tuple>
#include <string>
#include <boost/regex.hpp>
#include "Type.h"
#include "TypeUtils.h"

namespace TOML {
#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckIdentifier(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckIdentifier(std::string strToCheck) {
        size_t identifierStartIndex;
        std::string identifierContent;
        boost::regex identifierRegex(R"(^(\s*)([-\w]+))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, identifierRegex) && !match.prefix().length()) {
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
        boost::regex integerLiteralRegex(R"(^(\s*)([+-]?(0(?![xob])|[1-9]+(_?\d+)*|0x[\da-fA-F]+(_?[\da-fA-F]+)*|0o[0-7]+(_?[0-7]+)*|0b[01]+(_?[01]+)*)))");
        boost::regex floatLiteralRegex(R"(^(\s*)([+-]?((0(?![xob])|[1-9]+(_?\d+)*)(\.((\d+_)*\d+))?([eE][-+]?\d+(_?\d+)*)?)))");
        boost::regex specialNumLiteralRegex(R"(^(\s*)([+-]?(nan|inf)(?![-\w])))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, specialNumLiteralRegex) && !match.prefix().length()) {
            auto matchedStr = match[3].str();
            literalType = new Type::SpecialNumber(matchedStr == "nan" ? Type::SpecialNumber::NaN : Type::SpecialNumber::Infinity);
        }
        else {
            bool matched = false;
            boost::smatch integerMatch;
            boost::smatch floatMatch;
            if (boost::regex_search(strToCheck, integerMatch, integerLiteralRegex) && !integerMatch.prefix().length()) {
                matched = true;
            }
            if (boost::regex_search(strToCheck, floatMatch, floatLiteralRegex) && !floatMatch.prefix().length()) {
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
        boost::regex boolLiteralRegex(R"(^(\s*)((true|false)(?![-\w])))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, boolLiteralRegex) && !match.prefix().length()) {
            literalType = new Type::Boolean();
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
        }
        return { literalType, literalStartIndex, literalContent };
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
        boost::regex offsetDateTimeRegex(R"(^(\s*)((\d{4}-\d{2}-\d{2})[Tt ]([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?([Zz]|[+-]([01]\d|2[0-3]):[0-5]\d)))");
        boost::regex localDateTimeRegex(R"(^(\s*)((\d{4}-\d{2}-\d{2})[Tt ]([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?))");
        boost::regex localDateRegex(R"(^(\s*)(\d{4}-\d{2}-\d{2}))");
        boost::regex localTimeRegex(R"(^(\s*)(([01]\d|2[0-3]):[0-5]\d:[0-5]\d(\.\d+)?))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, offsetDateTimeRegex) && !match.prefix().length() && isValidDate(match[3].str())) {
            literalType = new Type::DateTime(Type::DateTime::OffsetDateTime);
        }
        else if (boost::regex_search(strToCheck, match, localDateTimeRegex) && !match.prefix().length() && isValidDate(match[3].str())) {
            literalType = new Type::DateTime(Type::DateTime::LocalDateTime);
        }
        else if (boost::regex_search(strToCheck, match, localDateRegex) && !match.prefix().length() && isValidDate(match[2].str())) {
            literalType = new Type::DateTime(Type::DateTime::LocalDate);
        }
        else if (boost::regex_search(strToCheck, match, localTimeRegex) && !match.prefix().length()) {
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
        boost::regex punctuatorRegex(R"(^(\s*)(\{|\}|\[|\]|,))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, punctuatorRegex) && !match.prefix().length()) {
            punctuatorStartIndex = match[1].length();
            punctuatorContent = match[0].str().substr(punctuatorStartIndex);
        }
        return { punctuatorStartIndex, punctuatorContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckOperator(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckOperator(std::string strToCheck) {
        size_t operatorStartIndex;
        std::string operatorContent;
        boost::regex operatorRegex(R"(^(\s*)(\.|=))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, operatorRegex) && !match.prefix().length()) {
            operatorStartIndex = match[1].length();
            operatorContent = match[0].str().substr(operatorStartIndex);
        }
        return { operatorStartIndex, operatorContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern std::tuple<size_t, std::string> CheckComment(std::string strToCheck);
#else
    std::tuple<size_t, std::string> CheckComment(std::string strToCheck) {
        size_t commentStartIndex;
        std::string commentContent;
        boost::regex commentRegex(R"(^(\s*)(#[^\n]*))");
        boost::smatch match;
        if (boost::regex_search(strToCheck, match, commentRegex) && !match.prefix().length()) {
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
        boost::regex basicStringLiteralRegex(R"(^(\s*)("([^"\\\n]|\\[btnfr"\\]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8})*"))");
        boost::regex multiLineBasicStringLiteralRegex(R"mlbslr(^(\s*)("""((("{0,2})(?!")([^"\\]|\\[btnfr"\\]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8}|\\[ \f\r\t\v]*\n\s*|((?<![^\\]")"(?!"))|((?<![^\\]")("")(?!")))*(?<![^\\]")("{0,2}))|(((?<="{3})("{1,2})(?="{3}))?))"""))mlbslr");
        boost::regex literalStringLiteralRegex(R"(^(\s*)('([^'\n])*'))");
        boost::regex multiLineLiteralStringLiteralRegex(R"(^(\s*)('''((('{0,2})(?!')([^']|((?<!')'(?!'))|((?<!')('')(?!'))|('(?='{3}\s?))|)*(?<!')('{0,2}))|(((?<='{3})('{1,2})(?='{3}))?))'''))");
        int matched = 0;
        boost::smatch basicStringMatch;
        boost::smatch multiLineBasicStringMatch;
        boost::smatch literalStringMatch;
        boost::smatch multiLineLiteralStringMatch;
        boost::smatch& match = basicStringMatch;
        if (boost::regex_search(strToCheck, basicStringMatch, basicStringLiteralRegex) && !basicStringMatch.prefix().length()) {
            matched = 1;
        }
        if (boost::regex_search(strToCheck, literalStringMatch, literalStringLiteralRegex) && !literalStringMatch.prefix().length()) {
            if (literalStringMatch[0].length() > match[0].length()) {
                match = literalStringMatch;
                matched = 3;
            }
        }
        if (boost::regex_search(strToCheck, multiLineBasicStringMatch, multiLineBasicStringLiteralRegex) && !multiLineBasicStringMatch.prefix().length()) {
            if (multiLineBasicStringMatch[0].length() > match[0].length()) {
                match = multiLineBasicStringMatch;
                matched = 2;
            }
        }
        else if (boost::regex_search(strToCheck, multiLineLiteralStringMatch, multiLineLiteralStringLiteralRegex) && !multiLineLiteralStringMatch.prefix().length()) {
            if (multiLineLiteralStringMatch[0].length() > match[0].length()) {
                match = multiLineLiteralStringMatch;
                matched = 4;
            }
        }
        if (matched) {
            switch (matched) {
            case 1:
                literalType = new Type::String(Type::String::Basic);
                break;
            case 2:
                literalType = new Type::String(Type::String::MultiLineBasic);
                break;
            case 3:
                literalType = new Type::String(Type::String::Raw);
                break;
            case 4:
                literalType = new Type::String(Type::String::MultiLineRaw);
                break;
            }
            literalStartIndex = match[1].length();
            literalContent = match[0].str().substr(literalStartIndex);
        }
        return { literalType, literalStartIndex, literalContent };
    }
#endif

#ifndef DEF_GLOBAL
    extern bool HasIncompleteString(const std::string& input);
#else
    std::string firstAppearedStringOrCommentStarter(const std::string& input) {
        size_t posSingle = input.find("'''");
        size_t posDouble = input.find("\"\"\"");
        size_t posHash = input.find("#");

        // Set to string::npos if not found
        if (posSingle == std::string::npos) posSingle = input.length() + 1;
        if (posDouble == std::string::npos) posDouble = input.length() + 1;
        if (posHash == std::string::npos) posHash = input.length() + 1;

        if (posSingle < posDouble && posSingle < posHash) {
            return "'''";
        }
        else if (posDouble < posSingle && posDouble < posHash) {
            return "\"\"\"";
        }
        else if (posHash < posSingle && posHash < posDouble) {
            return "#";
        }
        else {
            return "";
        }
    }

    bool HasIncompleteString(const std::string& input) {
        boost::regex commentRegex(R"((\s*)(#[^\n]*))");
        boost::regex multiLineBasicStringLiteralRegex(R"mlbslr(("""((("{0,2})(?!")([^"\\]|\\[btnfr"\\]|\\u[\da-fA-F]{4}|\\U[\da-fA-F]{8}|\\[ \f\r\t\v]*\n\s*|((?<![^\\]")"(?!"))|((?<![^\\]")("")(?!")))*(?<![^\\]")("{0,2}))|(((?<="{3})("{1,2})(?="{3}))?))"""))mlbslr");
        boost::regex multiLineLiteralStringLiteralRegex(R"(('''((('{0,2})(?!')([^']|((?<!')'(?!'))|((?<!')('')(?!'))|('(?='{3}\s?))|)*(?<!')('{0,2}))|(((?<='{3})('{1,2})(?='{3}))?))'''))");
        boost::regex tripleQuoteStart(R"(("""|'''))");

        bool commentAppearsFirst = firstAppearedStringOrCommentStarter(input) == "#";

        auto cleanedInput = boost::regex_replace(boost::regex_replace(boost::regex_replace(input, commentAppearsFirst ? commentRegex : multiLineLiteralStringLiteralRegex, ""), multiLineBasicStringLiteralRegex, ""), commentAppearsFirst ? multiLineLiteralStringLiteralRegex : commentRegex, "");

        // Check if the input starts with a triple quote but isn't a complete string
        boost::smatch match;
        if (boost::regex_search(cleanedInput, match, tripleQuoteStart)) {
            return true;
        }

        return false;
    }
#endif
}

#endif
