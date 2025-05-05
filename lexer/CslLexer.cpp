#include <iostream>
#include <string>
#include <list>
#include <tuple>
#include <regex>
#include "../shared/CslCheckFunctions.h"
#include "../shared/Token.h"
#include "../shared/FilePosition.h"

using namespace CSL;

namespace CSLLexer {
    class Lexer {
    protected:
        std::istream& inputCode;
        bool multilineToken;
        std::vector<std::tuple<std::string, FilePosition::Region>> errors;
        std::vector<std::tuple<std::string, FilePosition::Region>> warnings;

        FilePosition::Position getEndPosition(const std::string& text, const FilePosition::Position& start) {
            auto line = start.line;
            auto col = start.column;

            for (char ch : text) {
                if (ch == '\n') {
                    ++line;
                    col = 0;
                }
                else {
                    ++col;
                }
            }

            return { line, col };
        }

        bool isNumberReasonablyGrouped(const std::string& str) {
            size_t dotPos = str.find('.');
            std::string beforeDot = str.substr(0, dotPos);
            if (beforeDot.size() && (beforeDot[0] == '+' || beforeDot[0] == '-')) {
                beforeDot.erase(0, 1);
            }
            if (beforeDot.size() > 2 && beforeDot[0] == '0' && (beforeDot[1] == 'b' || beforeDot[1] == 'o' || beforeDot[1] == 'x')) {
                beforeDot.erase(0, 2);
            }
            std::string afterDot = dotPos > str.size() ? "" : str.substr(dotPos + 1);

            std::vector<std::string> parts;
            std::vector<size_t> sizes;

            // Split by underscores
            size_t start = 0, end;
            while ((end = beforeDot.find('_', start)) != std::string::npos) {
                const auto part = beforeDot.substr(start, end - start);
                if (part.empty()) return false; // invalid like "1__000"
                parts.push_back(part);
                sizes.push_back(part.size());
                start = end + 1;
            }
            parts.push_back(beforeDot.substr(start));
            sizes.push_back(parts.back().size());

            if (parts.size() != 1) { // Has underscores
                bool allSame = true;

                for (size_t i = 1; i < sizes.size(); ++i) {
                    if (sizes[i] != sizes[1]) {
                        allSame = false;
                        break;
                    }
                }
                if (allSame) {
                    if (sizes[1] == 1) {
                        return false;
                    }
                }
                else {
                    allSame = true;
                    for (size_t i = 1; i < sizes.size() - 1; ++i) {
                        if (sizes[i] != 2) {
                            allSame = false;
                            break;
                        }
                    }
                    if (!allSame || sizes[sizes.size() - 1] != 3) {
                        return false;
                    }
                }
            }

            parts.clear();
            sizes.clear();
            start = 0;
            while ((end = afterDot.find('_', start)) != std::string::npos) {
                const auto part = afterDot.substr(start, end - start);
                if (part.empty()) return false; // invalid like "1__000"
                parts.push_back(part);
                sizes.push_back(part.size());
                start = end + 1;
            }
            parts.push_back(afterDot.substr(start));
            sizes.push_back(parts.back().size());

            if (parts.size() == 1) return true; // No underscores

            bool allSame = true;

            for (size_t i = 1; i < sizes.size(); ++i) {
                if (sizes[i] != sizes[1]) {
                    allSame = false;
                    break;
                }
            }
            if (!allSame) return false;
            if (sizes[1] == 1) {
                return false;
            }

            return true;
        }

        bool isStringContentValid(const std::string& stringToCheck, int stringType) {
            size_t i = 0;
            while (i < stringToCheck.size()) {
                uint32_t codepoint = 0;
                unsigned char c = stringToCheck[i];

                size_t bytes = 0;
                if ((c & 0x80) == 0) {
                    // 1-byte ASCII
                    codepoint = c;
                    bytes = 1;
                }
                else if ((c & 0xE0) == 0xC0) {
                    // 2-byte
                    if (i + 1 >= stringToCheck.size()) return false;
                    unsigned char c1 = stringToCheck[i + 1];
                    if ((c1 & 0xC0) != 0x80) return false;

                    codepoint = ((c & 0x1F) << 6) | (c1 & 0x3F);
                    if (codepoint < 0x80) return false;  // Overlong
                    bytes = 2;
                }
                else if ((c & 0xF0) == 0xE0) {
                    // 3-byte
                    if (i + 2 >= stringToCheck.size()) return false;
                    unsigned char c1 = stringToCheck[i + 1];
                    unsigned char c2 = stringToCheck[i + 2];
                    if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) return false;

                    codepoint = ((c & 0x0F) << 12) |
                        ((c1 & 0x3F) << 6) |
                        (c2 & 0x3F);
                    if (codepoint < 0x800) return false;        // Overlong
                    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) return false;  // Surrogates
                    bytes = 3;
                }
                else if ((c & 0xF8) == 0xF0) {
                    // 4-byte
                    if (i + 3 >= stringToCheck.size()) return false;
                    unsigned char c1 = stringToCheck[i + 1];
                    unsigned char c2 = stringToCheck[i + 2];
                    unsigned char c3 = stringToCheck[i + 3];
                    if ((c1 & 0xC0) != 0x80 ||
                        (c2 & 0xC0) != 0x80 ||
                        (c3 & 0xC0) != 0x80) return false;

                    codepoint = ((c & 0x07) << 18) |
                        ((c1 & 0x3F) << 12) |
                        ((c2 & 0x3F) << 6) |
                        (c3 & 0x3F);
                    if (codepoint < 0x10000 || codepoint > 0x10FFFF) return false;  // Overlong or out of range
                    bytes = 4;
                }
                else {
                    // Invalid leading byte
                    return false;
                }

                if ((stringType == 0 || stringType == 2) &&
                    (codepoint >= 0x0000 && codepoint <= 0x0008 ||
                        codepoint >= 0x000A && codepoint <= 0x001F ||
                        codepoint == 0x007F)) {
                    return false;
                }

                if (stringType == 1 || stringType == 3) {
                    if (codepoint >= 0x0000 && codepoint <= 0x0008 ||
                        codepoint == 0x000B || codepoint == 0x000C ||
                        codepoint >= 0x000E && codepoint <= 0x001F ||
                        codepoint == 0x007F) {
                        return false;
                    }
                    else if (codepoint == 0x000D &&
                        (i + 1 >= stringToCheck.size() || stringToCheck[i] != 0x000A)) {
                        return false;
                    }
                }

                i += bytes;
            }

            return true;
        }

        bool customGetline(std::istream& in, std::string& line) {
            line.clear();
            char ch;

            while (in.get(ch)) {
                if (ch == '\n') {
                    // Check if we have a CRLF sequence
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();  // Remove the '\r' before '\n'
                    }
                    return true;  // End of line
                }
                else {
                    line += ch;
                }
            }

            // Return true if we got any characters, even if we hit EOF without newline
            return !line.empty();
        }

    public:
        Lexer(std::istream& inputCode, bool multilineToken = true) :
            inputCode(inputCode), multilineToken(multilineToken) {
        }

        Token::TokenList<> Lex() {
            Token::TokenList<> tokenList;
            std::string codeToProcess;
            FilePosition::Position currentPosition = { 0, 0 };
            std::string curLine;
            bool isContinued = false;
            while (customGetline(inputCode, curLine)) {
                if (std::regex_match(curLine, std::regex(R"(\s*)"))) {
                    if (std::regex_search(curLine, std::regex(R"(\r(?!\n))"))) {
                        FilePosition::Region errorRegion = { currentPosition.line, { 0, false }, currentPosition.line, { curLine.size(), false } };
                        errors.push_back({ "Line ending is not valid.", errorRegion });
                    }
                    ++currentPosition.line;
                    currentPosition.column = 0;
                    if (inputCode.peek() != -1 || std::regex_match(codeToProcess, std::regex(R"(\s+)"))) {
                        continue;
                    }
                }
                if (isContinued) {
                    codeToProcess += curLine;
                }
                else {
                    codeToProcess = curLine;
                }
                if (HasIncompleteString(codeToProcess)) {
                    isContinued = true;
                    codeToProcess += "\n";
                    if (inputCode.peek() != -1) {
                        continue;
                    }
                    else {
                        FilePosition::Region errorRegion = { currentPosition.line, { 0, false }, currentPosition.line, { codeToProcess.find('\n'), false } };
                        errors.push_back({ "String literal is not closed.", errorRegion });
                    }
                }
                isContinued = false;
                while (codeToProcess.size()) {
                    // Comment
                    {
                        auto [tokenStartIndex, tokenContent] = CheckComment(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            //tokenList.AddTokenToList(tokenContent, "comment", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            if (tokenContent.find("//") >= tokenContent.size() ? false : !isStringContentValid(tokenContent.substr(tokenContent.find('#') + 1), 0)) {
                                errors.push_back({ "Comment contains invalid content.", tokenRegion });
                            }
                            continue;
                        }
                    }
                    // String Literal
                    {
                        auto [tokenType, tokenStartIndex, tokenContent] = CheckStringLiteral(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "string", tokenType, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            if (!isStringContentValid(tokenContent, ((Type::String*)tokenType)->getType())) {
                                errors.push_back({ "String literal contains invalid content.", tokenRegion });
                            }
                            continue;
                        }
                    }
                    // Date Time Literal
                    {
                        auto [tokenType, tokenStartIndex, tokenContent] = CheckDateTimeLiteral(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "datetime", tokenType, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Numeric Literal
                    {
                        auto [tokenType, tokenStartIndex, tokenContent] = CheckNumericLiteral(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "number", tokenType, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            if (tokenContent.size() > 3 && (tokenContent[0] == '+' || tokenContent[0] == '-') && tokenContent[1] == '0' && (tokenContent[2] == 'b' || tokenContent[2] == 'o' || tokenContent[2] == 'x')) {
                                errors.push_back({ "Number literal in hexadecimal, octal or binary cannot have a positive or negative sign.", tokenRegion });
                            }
                            if (!isNumberReasonablyGrouped(tokenContent)) {
                                warnings.push_back({ "Number literal is not grouped reasonably.", tokenRegion });
                            }
                            continue;
                        }
                    }
                    // Boolean Literal
                    {
                        auto [tokenType, tokenStartIndex, tokenContent] = CheckBooleanLiteral(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "boolean", new Type::Boolean(), tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Keyword
                    {
                        auto [tokenStartIndex, tokenContent] = CheckKeyword(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "keyword", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Type
                    {
                        auto [tokenStartIndex, tokenContent] = CheckType(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "type", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Operator
                    {
                        auto [tokenStartIndex, tokenContent] = CheckOperator(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "operator", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Identifier
                    {
                        auto [tokenStartIndex, tokenContent] = CheckIdentifier(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "identifier", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }
                    // Punctuator
                    {
                        auto [tokenStartIndex, tokenContent] = CheckPunctuator(codeToProcess);
                        if (!tokenContent.empty()) {
                            auto tokenStart = getEndPosition(codeToProcess.substr(0, tokenStartIndex), currentPosition);
                            auto tokenEnd = getEndPosition(tokenContent, tokenStart);
                            FilePosition::Region tokenRegion = { tokenStart, tokenEnd };
                            tokenList.AddTokenToList(tokenContent, "punctuator", nullptr, tokenRegion);
                            currentPosition = tokenEnd;
                            codeToProcess.erase(0, tokenStartIndex + tokenContent.size());
                            continue;
                        }
                    }

                    if (std::regex_match(codeToProcess, std::regex(R"(\s*)"))) {
                        auto tokenEnd = getEndPosition(codeToProcess, currentPosition);
                        currentPosition = tokenEnd;
                        codeToProcess.clear();
                        continue;
                    }

                    // Unknown Content
                    if (!tokenList.IsTokenBuffered()) {
                        tokenList.SetTokenInfo("unknown");
                    }
                    tokenList.AppendBufferedToken(codeToProcess[0]);
                    if (codeToProcess[0] == '\n') {
                        ++currentPosition.line;
                        currentPosition.column = 0;
                    }
                    else {
                        ++currentPosition.column;
                    }
                    codeToProcess.erase(0, 1);
                }
                tokenList.FlushBuffer();
                ++currentPosition.line;
                currentPosition.column = 0;
            }
            for (const auto& token : tokenList) {
                auto [tokenContent, tokenType, tokenProp, tokenRegion] = token;
                if (tokenType == "unknown") {
                    errors.push_back({ "Unknown token: " + tokenContent + ".", tokenRegion });
                }
            }
            return tokenList;
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> GetErrors() {
            return errors;
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> GetWarnings() {
            return warnings;
        }
    };
}

std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslLexerMain(std::istream& inputCode, bool multilineToken = true) {
    CSLLexer::Lexer lexer(inputCode, multilineToken);
    return { lexer.Lex(), lexer.GetErrors(), lexer.GetWarnings() };
}
