#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <unordered_set>
#include <stdexcept>
#include "../shared/Type.h"
#include "../shared/Token.h"
#include "../shared/CheckFunctions.h"
#include "../shared/DocumentTree.h"
#include "../shared/TomlStringUtils.h"

class RecursiveDescentParser {
public:
    RecursiveDescentParser(Token::TokenList<>& input) :
        input(input),
        position(input.begin()),
        docTree(new DocTree::Table({}, true, {}, false)),
        lastDefinedTable(docTree) {
    }

    enum ParsedKeyType {
        Table,
        Array,
        Key,
    };

    bool IsIdentifierToken(Token::TokenList<>::iterator& position, bool genErrorIfNot = false) {
        bool isIdentifier = false;
        if (std::get<1>(*position) == "identifier") {
            isIdentifier = true;
        }
        else if (std::get<1>(*position) == "string") {
            auto stringType = ((Type::String*)std::get<2>(*position))->getType();
            if (stringType == Type::String::MultiLineBasic || stringType == Type::String::MultiLineLiteral) {
                errors.push_back({ "Multi-line string cannot be used as a key.", std::get<3>(*position) });
            }
            auto contentInString = std::get<0>(*position).substr(1, std::get<0>(*position).size() - 2);
            if (contentInString.empty()) {
                warnings.push_back({ "Empty string key is not recommended.", std::get<3>(*position) });
            }
            auto [content, type, prop, region] = *position;
            type = "identifier";
            *position = std::make_tuple(content, type, prop, region);
            isIdentifier = true;
        }
        else if (std::get<1>(*position) == "boolean") {
            auto [content, type, prop, region] = *position;
            type = "identifier";
            *position = std::make_tuple(content, type, prop, region);
            isIdentifier = true;
        }
        else if (std::get<1>(*position) == "number" && (dynamic_cast<Type::SpecialNumber*>(std::get<2>(*position)) || isdigit(std::get<0>(*position)[0]))) {
            auto dotPos = std::get<0>(*position).find('.');
            if (dotPos != std::string::npos) {
                auto [idStartIndex1, idContent1] = CheckIdentifier(std::get<0>(*position).substr(0, dotPos));
                auto [idStartIndex2, idContent2] = CheckIdentifier(std::get<0>(*position).substr(dotPos + 1));
                if (idContent1.size() && idContent2.size()) {
                    auto [content, type, prop, region] = *position;
                    type = "identifier";
                    auto tokenBeforeDotRegion = region;
                    tokenBeforeDotRegion.end.column = tokenBeforeDotRegion.start.column + dotPos;
                    auto tokenBeforeDot = std::make_tuple(content.substr(0, dotPos), type, Type::CopyType(prop), tokenBeforeDotRegion);
                    auto tokenAfterDotRegion = region;
                    tokenAfterDotRegion.start.column = tokenBeforeDotRegion.end.column + 1;
                    auto tokenAfterDot = std::make_tuple(content.substr(dotPos + 1), type, Type::CopyType(prop), tokenAfterDotRegion);
                    auto dotOperatorTokenRegion = region;
                    dotOperatorTokenRegion.start.column = tokenBeforeDotRegion.end.column;
                    dotOperatorTokenRegion.end.column = tokenAfterDotRegion.start.column;
                    auto dotOperatorToken = std::make_tuple(".", "operator", nullptr, dotOperatorTokenRegion);
                    position = input.erase(position);
                    position = input.insert(position, tokenAfterDot);
                    position = input.insert(position, dotOperatorToken);
                    position = input.insert(position, tokenBeforeDot);
                    isIdentifier = true;
                }
            }
            else {
                auto [idStartIndex, idContent] = CheckIdentifier(std::get<0>(*position));
                if (idStartIndex == 0 && idContent.size()) {
                    auto [content, type, prop, region] = *position;
                    type = "identifier";
                    *position = std::make_tuple(content, type, prop, region);
                    isIdentifier = true;
                }
            }
        }
        if (genErrorIfNot && !isIdentifier) {
            errors.push_back({ "Expect key. Got " + std::get<0>(*position) + ".", std::get<3>(*position) });
        }
        return isIdentifier;
    }

    std::tuple<ParsedKeyType, DocTree::DocTreeNode*> ParseKey() {
        static std::unordered_set<DocTree::Table*> headerDefinedTables;
        DocTree::DocTreeNode* targetKey = nullptr;
        ParsedKeyType type = ParsedKeyType::Key;
        if (position != input.end() && std::get<0>(*position) == "[") {
            if (std::next(position) != input.end() && std::get<0>(*std::next(position)) == "[") {
                auto curTokenRegion = std::get<3>(*position);
                auto nextTokenRegion = std::get<3>(*std::next(position));
                if (curTokenRegion.end.line == nextTokenRegion.start.line && curTokenRegion.end.column == nextTokenRegion.start.column) {
                    type = ParsedKeyType::Array;
                    position = std::next(position, 2);
                }
                else {
                    errors.push_back({ "Operator [[ cannot be seperated by whitespace.", { curTokenRegion.start, nextTokenRegion.end } });
                }
            }
            else {
                type = ParsedKeyType::Table;
                position = std::next(position);
            }
        }
        if (position == input.end()) {
            errors.push_back({ "Expect key " + (position != input.begin() ? "after " + std::get<0>(*std::prev(position)) : "at the end of the file") + ".", position != input.begin() ? std::get<3>(*std::prev(position)) : FilePosition::Region{ 0, 0, 0, 0 } });
        }
        else if (IsIdentifierToken(position)) {
            DocTree::Table* curTable = type == ParsedKeyType::Key ? lastDefinedTable : docTree;
            do {
                if (!curTable->getIsMutable()) {
                    errors.push_back({ "Key " + std::get<0>(*position) + " is not mutable.", std::get<3>(*position) });
                }
                auto curIdentifier = std::get<0>(*position);
                if (curIdentifier.size() && (curIdentifier[0] == '\"' || curIdentifier[0] == '\'')) {
                    curIdentifier = extractStringLiteralContent(curIdentifier, ((Type::String*)std::get<2>(*position))->getType());
                }
                if (std::next(position) != input.end() && std::get<0>(*std::next(position)) == ".") {
                    auto keyIter = curTable->getElems().find(curIdentifier);
                    if (keyIter == curTable->getElems().end()) {
                        if (type == ParsedKeyType::Key && headerDefinedTables.find(curTable) != headerDefinedTables.end() && lastDefinedTable != curTable) {
                            errors.push_back({ "Parent table is already defined.", std::get<3>(*position) });
                        }
                        auto newKey = new DocTree::Key(curIdentifier, new DocTree::Table({}, true, std::get<3>(*position), type == ParsedKeyType::Key));
                        curTable->addElem(newKey);
                        curTable = (DocTree::Table*)std::get<1>(newKey->get());
                    }
                    else {
                        auto curKeyValue = std::get<1>(keyIter->second->get());
                        if (auto tableValue = dynamic_cast<DocTree::Table*>(curKeyValue)) {
                            curTable = tableValue;
                        }
                        else if (auto arrayValue = dynamic_cast<DocTree::Array*>(curKeyValue)) {
                            auto arrElemVec = std::get<0>(arrayValue->get());
                            if (arrElemVec.empty()) {
                                errors.push_back({ "Array " + curIdentifier + " is empty.", std::get<3>(*position) });
                            }
                            else {
                                if (type == ParsedKeyType::Key) {
                                    errors.push_back({ "Cannot append to array with dotted keys.", std::get<3>(*position) });
                                }
                                curTable = (DocTree::Table*)arrElemVec.back();
                            }
                        }
                        else {
                            errors.push_back({ "Key " + curIdentifier + " is defined as a bare key.", std::get<3>(*position) });
                        }
                    }
                }
                else {
                    auto keyIter = curTable->getElems().find(curIdentifier);
                    if (keyIter == curTable->getElems().end()) {
                        if (type == ParsedKeyType::Key && headerDefinedTables.find(curTable) != headerDefinedTables.end() && lastDefinedTable != curTable) {
                            errors.push_back({ "Parent table is already defined.", std::get<3>(*position) });
                        }
                        auto newKey = new DocTree::Key(curIdentifier, nullptr);
                        curTable->addElem(newKey);
                        if (type == ParsedKeyType::Array) {
                            lastDefinedTable = new DocTree::Table({}, true, std::get<3>(*position), true);
                            newKey->set<1>((DocTree::DocTreeNode*)new DocTree::Array({ lastDefinedTable }, true, std::get<3>(*position)));
                        }
                        else if (type == ParsedKeyType::Table) {
                            lastDefinedTable = new DocTree::Table({}, true, std::get<3>(*position), true);
                            newKey->set<1>((DocTree::DocTreeNode*)lastDefinedTable);
                            headerDefinedTables.insert(lastDefinedTable);
                        }
                        targetKey = newKey;
                    }
                    else {
                        if (type == ParsedKeyType::Array) {
                            auto arrValue = std::get<1>(keyIter->second->get());
                            if (auto array = dynamic_cast<DocTree::Array*>(arrValue)) {
                                if (array->getIsMutable()) {
                                    auto& arrElemVec = array->getElems();
                                    lastDefinedTable = new DocTree::Table({}, true, std::get<3>(*position), true);
                                    arrElemVec.push_back(lastDefinedTable);
                                    targetKey = array;
                                }
                                else {
                                    errors.push_back({ "Static array " + curIdentifier + " cannot be modified.", std::get<3>(*position) });
                                }
                            }
                            else {
                                errors.push_back({ "Key " + curIdentifier + " is not an array.", std::get<3>(*position) });
                            }
                        }
                        else if (type == ParsedKeyType::Table) {
                            auto tableValue = std::get<1>(keyIter->second->get());
                            if (auto table = dynamic_cast<DocTree::Table*>(tableValue)) {
								if (table->getIsExplicitlyDefined()) {
                                    errors.push_back({ "Table " + curIdentifier + " is already defined.", std::get<3>(*position) });
                                }
                                else {
                                    table->set<3>(true);
                                    table->set<2>(std::get<3>(*position));
                                    targetKey = lastDefinedTable = table;
                                }
                            }
                            else {
                                errors.push_back({ "Key " + curIdentifier + " is not a table.", std::get<3>(*position) });
                            }
                        }
                        else {
                            errors.push_back({ "Key " + curIdentifier + " is already defined.", std::get<3>(*position) });
                        }
                    }
                }
                ++position;
            } while (position != input.end() && (std::get<0>(*position) == "." ? (++position, position != input.end() && IsIdentifierToken(position, true)) : false));
            if (type != ParsedKeyType::Key) {
                bool defComplete = false;
                if (position != input.end() && std::get<0>(*position) == "]") {
                    ++position;
                    if (type == ParsedKeyType::Table) {
                        defComplete = true;
                    }
                    else if (position != input.end() && std::get<0>(*position) == "]") {
                        auto curTokenRegion = std::get<3>(*position);
                        auto prevTokenRegion = std::get<3>(*std::prev(position));
                        if (prevTokenRegion.end.line == curTokenRegion.start.line && prevTokenRegion.end.column == curTokenRegion.start.column) {
                            defComplete = true;
                        }
                        else {
                            errors.push_back({ "Operator ]] cannot be seperated by whitespace.", { prevTokenRegion.start, curTokenRegion.end } });
                        }
                        ++position;
                    }
                }
                if (!defComplete) {
                    if (type == ParsedKeyType::Table) {
                        if (position == input.end()) {
                            errors.push_back({ "Expect ]" + (position != input.begin() ? " after " + std::get<0>(*std::prev(position)) : "at the end of the file") + ".", position != input.begin() ? std::get<3>(*std::prev(position)) : FilePosition::Region{ 0, 0, 0, 0 } });
                        }
                        else {
                            errors.push_back({ "Expect ].", std::get<3>(*position) });
                        }
                    }
                    else {
                        if (position == input.end()) {
                            errors.push_back({ "Expect ]]" + (position != input.begin() ? " after " + std::get<0>(*std::prev(position)) : "at the end of the file") + ".", position != input.begin() ? std::get<3>(*std::prev(position)) : FilePosition::Region{ 0, 0, 0, 0 } });
                        }
                        else {
                            errors.push_back({ "Expect ]].", std::get<3>(*position) });
                        }
                    }
                }
            }
        }
        else {
            errors.push_back({ "Expect key. Got " + std::get<0>(*position) + ".", std::get<3>(*position) });
        }
        return { type, targetKey };
    }

    void SkipToNextDefine() {
        while (position != input.end() && std::get<0>(*position) != "[" && (position == input.begin() || std::get<3>(*std::prev(position)).end.line < std::get<3>(*position).start.line)) {
            ++position;
        }
    }

    void SkipAssignment() {
        if (position == input.end()) {
            errors.push_back({ "Expect an assignment.", position == input.begin() ? FilePosition::Region{ 0, 0, 0, 0 } : std::get<3>(*std::prev(position)) });
            return;
        }
        if (std::get<0>(*position) != "=") {
            errors.push_back({ "Expect =. Got " + std::get<0>(*position) + ".", std::get<3>(*position) });
        }
        else {
            ++position;
        }
        if (position == input.end()) {
            errors.push_back({ "Expect a value for the assignment.", std::get<3>(*std::prev(position)) });
            return;
        }
        if (std::get<0>(*position) != "[" && std::get<0>(*position) != "{") {
            errors.push_back({ "Expect [ or {. Got " + std::get<0>(*position) + ".", std::get<3>(*position) });
        }
        std::stack<FilePosition::Region> squareParenStack;
        std::stack<FilePosition::Region> curlParenStack;
        do {
            if (std::get<0>(*position) == "[") {
                squareParenStack.push(std::get<3>(*position));
            }
            else if (std::get<0>(*position) == "{") {
                curlParenStack.push(std::get<3>(*position));
            }
            else if (std::get<0>(*position) == "]") {
                if (squareParenStack.empty()) {
                    errors.push_back({ "Unbalanced [.", std::get<3>(*position) });
                }
                else {
                    squareParenStack.pop();
                }
            }
            else if (std::get<0>(*position) == "}") {
                if (curlParenStack.empty()) {
                    errors.push_back({ "Unbalanced {.", std::get<3>(*position) });
                }
                else {
                    curlParenStack.pop();
                }
            }
            ++position;
        } while (position != input.end() && squareParenStack.empty() && curlParenStack.empty());
        while (!squareParenStack.empty()) {
            errors.push_back({ "Unbalanced [.", squareParenStack.top() });
            squareParenStack.pop();
        }
        while (!curlParenStack.empty()) {
            errors.push_back({ "Unbalanced {.", curlParenStack.top() });
            curlParenStack.pop();
        }
    }

    DocTree::DocTreeNode* ParseValue() {
        if (position == input.end()) {
            errors.push_back({ "Expect a value for the assignment.", std::get<3>(*std::prev(position)) });
            return nullptr;
        }
        DocTree::DocTreeNode* parsedValue = nullptr;
        std::stack<FilePosition::Region> squareParenStack;
        std::stack<FilePosition::Region> curlParenStack;
        do {
            if (std::get<0>(*position) == "[") {
                squareParenStack.push(std::get<3>(*position));
                auto arrayDefStart = std::get<3>(*position).start;
                ++position;
                parsedValue = new DocTree::Array({}, false, {});
                while (position != input.end() && std::get<0>(*position) != "]") {
                    auto arrElem = ParseValue();
                    if (arrElem) {
                        ((DocTree::Array*)parsedValue)->getElems().push_back(arrElem);
                        if (position == input.end()) {
                            errors.push_back({ "Expect either a , or a ].", std::get<3>(*std::prev(position)) });
                        }
                        else if (std::get<0>(*position) == ",") {
                            ++position;
                        }
                        else if (std::get<0>(*position) != "]") {
                            errors.push_back({ "Expect either a , or a ].", std::get<3>(*std::prev(position)) });
                        }
                    }
                    else if (std::get<1>(*std::prev(position)) != "comment") {
                        errors.push_back({ "Expect an array element.", std::get<3>(*std::prev(position)) });
                    }
                }
                if (position != input.end() && std::get<0>(*position) == "]") {
                    FilePosition::Region tableDefRegion = { arrayDefStart, std::get<3>(*position).end };
                    ((DocTree::Array*)parsedValue)->set<2>(tableDefRegion);
                }
            }
            else if (std::get<0>(*position) == "{") {
                curlParenStack.push(std::get<3>(*position));
                auto tableDefStart = std::get<3>(*position).start;
                bool allowMultiLine = false;
                ++position;
                parsedValue = new DocTree::Table({}, true, {}, false);
                while (position != input.end() && std::get<0>(*position) != "}") {
                    auto curLastDefinedTable = lastDefinedTable;
                    lastDefinedTable = (DocTree::Table*)parsedValue;
                    auto parsedKey = ParseStatement(false, true);
                    lastDefinedTable = curLastDefinedTable;
                    if (!parsedKey) {
                        errors.push_back({ "Expect a key-value pair.", std::get<3>(*std::prev(position)) });
                    }
                    else {
                        auto [keyId, keyValue] = parsedKey->get();
                        if (dynamic_cast<DocTree::Array*>(keyValue) || dynamic_cast<DocTree::Table*>(keyValue)) {
                            allowMultiLine = true;
                        }
                        else if (auto value = dynamic_cast<DocTree::Value*>(keyValue)) {
                            auto stringValue = dynamic_cast<Type::String*>(std::get<0>(value->get()));
                            if (stringValue && (stringValue->getType() == Type::String::MultiLineBasic || stringValue->getType() == Type::String::MultiLineLiteral)) {
                                allowMultiLine = true;
                            }
                        }
                    }
                    if (position == input.end()) {
                        errors.push_back({ "Expect either a , or a }.", std::get<3>(*std::prev(position)) });
                    }
                    else if (std::get<0>(*position) == ",") {
                        ++position;
                    }
                    else if (std::get<0>(*position) != "}") {
                        errors.push_back({ "Expect either a , or a }.", std::get<3>(*std::prev(position)) });
                    }
                }
                ((DocTree::Table*)parsedValue)->seal();
                ((DocTree::Table*)parsedValue)->set<3>(true);
                if (position != input.end() && std::get<0>(*position) == "}") {
                    auto tableDefEnd = std::get<3>(*position).end;
                    if (std::get<0>(*std::prev(position)) == ",") {
                        errors.push_back({ "A terminating comma is not permitted after the last key-value pair in an inline table.", std::get<3>(*std::prev(position)) });
                    }
                    FilePosition::Region tableDefRegion = { tableDefStart, tableDefEnd };
                    ((DocTree::Table*)parsedValue)->set<2>(tableDefRegion);
                    if (!allowMultiLine && tableDefEnd.line != tableDefStart.line) {
                        errors.push_back({ "All parts of the inline table definition should be in the same line.", tableDefRegion });
                    }
                }
            }
            else if (std::get<0>(*position) == "]") {
                if (!squareParenStack.empty()) {
                    squareParenStack.pop();
                    ++position;
                }
            }
            else if (std::get<0>(*position) == "}") {
                if (!curlParenStack.empty()) {
                    curlParenStack.pop();
                    ++position;
                }
            }
            else if (std::get<1>(*position) == "comment") {
                ++position;
            }
            else {
                auto valueType = std::get<2>(*position);
                if (!dynamic_cast<Type::String*>(valueType) &&
                    !dynamic_cast<Type::Integer*>(valueType) &&
                    !dynamic_cast<Type::Float*>(valueType) &&
                    !dynamic_cast<Type::SpecialNumber*>(valueType) &&
                    !dynamic_cast<Type::Boolean*>(valueType) &&
                    !dynamic_cast<Type::DateTime*>(valueType)) {
                    errors.push_back({ "Type of " + std::get<0>(*position) + " is not string, integer, floating-point, NaN, infinity, boolean or date-time.", std::get<3>(*position) });
                }
                else {
                    parsedValue = new DocTree::Value(valueType, std::get<0>(*position));
                }
                ++position;
            }
        } while (position != input.end() && (std::get<1>(*position) == "comment" || !(squareParenStack.empty() && curlParenStack.empty())));
        while (!squareParenStack.empty()) {
            errors.push_back({ "Unbalanced [.", squareParenStack.top() });
            squareParenStack.pop();
        }
        while (!curlParenStack.empty()) {
            errors.push_back({ "Unbalanced {.", curlParenStack.top() });
            curlParenStack.pop();
        }
        return parsedValue;
    }

    DocTree::Key* ParseStatement(bool requireStartFromNewLine, bool assignmentOnly) {
        while (position != input.end() && std::get<1>(*position) == "comment") {
            ++position;
        }
        if (requireStartFromNewLine && position != input.end() && position != input.begin() && std::get<3>(*position).start.line == std::get<3>(*std::prev(position)).end.line) {
            errors.push_back({ "Each statement should start from a new line.", { std::get<3>(*std::prev(position)).start, std::get<3>(*position).end } });
        }
        if (position == input.end()) {
            return nullptr;
        }
        auto [keyTpe, targetKey] = ParseKey();
        if (targetKey) {
            if (keyTpe == ParsedKeyType::Key) {
                if (position == input.end()) {
                    errors.push_back({ "Expect an assignment.", position == input.begin() ? FilePosition::Region{ 0, 0, 0, 0 } : std::get<3>(*std::prev(position)) });
                }
                else {
                    if (std::get<0>(*position) != "=") {
                        errors.push_back({ "Expect =. Got " + std::get<0>(*position) + ".", std::get<3>(*position) });
                    }
                    else if (std::next(position) == input.end()) {
                        errors.push_back({ "Expect an assignment.", position == input.begin() ? FilePosition::Region{ 0, 0, 0, 0 } : std::get<3>(*std::prev(position)) });
                    }
                    else if (std::get<3>(*position).start.line != std::get<3>(*std::prev(position)).end.line || std::get<3>(*position).end.line != std::get<3>(*std::next(position)).start.line) {
                        errors.push_back({ "All parts of the assignment must be in the same line.", std::get<3>(*position) });
                    }
                    else {
                        ++position;
                    }
                    auto value = ParseValue();
                    if (value) {
                        ((DocTree::Key*)targetKey)->set<1>(value);
                    }
                    else {
                        errors.push_back({ "Expect a value for the assignment.", std::get<3>(*std::prev(position)) });
                    }
                }
            }
            else if (assignmentOnly) {
                errors.push_back({ "Only assignment is allowed here.", std::get<3>(*std::prev(position)) });
            }
        }
        else {
            if (keyTpe == ParsedKeyType::Key) {
                SkipAssignment();
            }
            else {
                SkipToNextDefine();
            }
        }
        return dynamic_cast<DocTree::Key*>(targetKey);
    }

    DocTree::Table* ParseDocument() {
        while (position != input.end()) {
            ParseStatement(true, false);
        }
        return docTree;
    }

    std::vector<std::tuple<std::string, FilePosition::Region>> GetErrors() {
        return errors;
    }

    std::vector<std::tuple<std::string, FilePosition::Region>> GetWarnings() {
        return warnings;
    }

protected:
    Token::TokenList<>& input;
    Token::TokenList<>::iterator position;
    DocTree::Table* docTree;
    DocTree::Table* lastDefinedTable;
    std::vector<std::tuple<std::string, FilePosition::Region>> errors;
    std::vector<std::tuple<std::string, FilePosition::Region>> warnings;
};

std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> rdparserMain(Token::TokenList<>& tokenList) {
    RecursiveDescentParser rdparser(tokenList);
    return { rdparser.ParseDocument(), rdparser.GetErrors(), rdparser.GetWarnings() };
}
