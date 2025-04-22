#pragma once

#ifndef LANGUAGE_SERVER_H
#define LANGUAGE_SERVER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <json.hpp>
#include "../shared/Token.h"
#include "../shared/FilePosition.h"
#include "../shared/DocumentTree.h"
#include "../shared/DocTree2Toml.h"
#include "TextEdit.h"
#include "FindPairs.h"

using json = nlohmann::json;

class LanguageServer {
public:
    LanguageServer(const std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>& lexer, const std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>>(Token::TokenList<>& tokenList)>& parser, size_t& jsonId) : lexer(lexer), parser(parser), jsonId(jsonId) {
    }

    json handleRequest(const json& request) {
        size_t requestId = 0;
        if (request.contains("id")) {
            requestId = request["id"];
        }
        try {
            if (request["method"] == "initialize") {
                return handleInitialize(request);
            }
            else {
                if (!isServerInitialized) {
                    throw std::runtime_error("Server not initialized");
                }
                if (request["method"] == "initialized") {
                    return handleInitialized(request);
                }
                else {
                    if (!isClientInitialized) {
                        throw std::runtime_error("Client not initialized");
                    }
                    if (isServerShutdown && request["method"] != "exit") {
                        throw std::runtime_error("Server already shutdown");
                    }
                    else if (request["method"] == "exit") {
                        return handleExit(request);
                    }
                    else if (request["method"] == "shutdown") {
                        return handleShutdown(request);
                    }
                    else if (request["method"] == "textDocument/didOpen") {
                        return handleDidOpen(request);
                    }
                    else if (request["method"] == "textDocument/didChange") {
                        return handleDidChange(request);
                    }
                    else if (request["method"] == "textDocument/didClose") {
                        return handleDidClose(request);
                    }
                    else if (request["method"] == "$/setTrace") {
                        return handleSetTrace(request);
                    }
                    else if (request["method"] == "textDocument/semanticTokens/full") {
                        return handleSemanticTokens(request);
                    }
                    else if (request["method"] == "textDocument/formatting") {
                        return handleFormatting(request);
                    }
                    else if (request["method"] == "textDocument/definition") {
                        return handleDefinition(request);
                    }
                    else if (request["method"] == "textDocument/completion") {
                        return handleCompletion(request);
                    }
                    else if (request["method"] == "textDocument/hover") {
                        return handleHover(request);
                    }
                    else if (request["method"] == "textDocument/diagnostic") {
                        return handlePullDiagnostic(request);
                    }
                    json error;
                    error["jsonrpc"] = "2.0";
                    error["id"] = requestId;
                    error["error"]["error"] = -32601;
                    error["error"]["message"] = "Method not found";
                    return error;
                }
            }
        }
        catch (const std::exception& e) {
            json error;
            error["jsonrpc"] = "2.0";
            error["id"] = requestId;
            error["error"]["error"] = -32603;
            error["error"]["message"] = e.what();
            return error;
        }
    }

    int getServerExitCode() {
        return isServerExited ? isServerShutdown ? 0 : 1 : -1;
    }

protected:
    size_t& jsonId;
    bool isServerInitialized = false;
    bool isClientInitialized = false;
    bool isServerShutdown = false;
    bool isServerExited = false;
    bool clientSupportsMultilineToken = false;
    std::string traceValue;
    std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)> lexer;
    std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>>(Token::TokenList<>& tokenList)> parser;
    std::unordered_map<std::string, std::string> documentCache;

    json genRequest(const std::string& method, const json& params) {
        json request;
        request["jsonrpc"] = "2.0";
        request["id"] = jsonId;
        request["method"] = method;
        request["params"] = params;
        ++jsonId;
        return request;
    }

    json genResponse(const size_t id, const json& result, const json& error) {
        json response;
        if (error.is_null()) {
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["result"] = result;
        }
        else {
            response["jsonrpc"] = "2.0";
            response["id"] = id;
            response["error"] = error;
        }
        return response;
    }

    json genNotification(const std::string& method, const json& params) {
        json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = method;
        notification["params"] = params;
        return notification;
    }

    json handleInitialize(const json& request) {
        if (isServerInitialized) {
            throw std::runtime_error("Initialize request may only be sent once");
        }
        isServerInitialized = true;
        traceValue = request["params"]["trace"];
        clientSupportsMultilineToken = request["params"]["capabilities"]["textDocument"]["semanticTokens"]["multilineTokenSupport"];
        return genResponse(
            request["id"].get<size_t>(),
            json::parse(R"({
                   "capabilities": {
                       "textDocumentSync": 1,
                       "semanticTokensProvider": {
                           "legend": {
                               "tokenTypes": [
                                   "datetime", "number", "boolean", "identifier",
                                   "punctuator", "operator", "comment", "string", "unknown"
                               ],
                               "tokenModifiers": []
                           },
                           "full": true
                       },
                       "documentFormattingProvider": true,
                       "definitionProvider": true,
                       "completionProvider": {
                           "triggerCharacters": [".", "-"],
                           "allCommitCharacters": [".", "=", " ", "\"", "'", "]", "}"]
                        },
                       "hoverProvider": true,
                       "diagnosticProvider": {
                           "interFileDependencies": true,
                           "workspaceDiagnostics": false
                       }
                   }
               })"),
            json()
        );
    }

    json handleInitialized(const json& request) {
        if (isClientInitialized) {
            throw std::runtime_error("Initialized request may only be sent once");
        }
        isClientInitialized = true;
        return json::object();
    }

    json handleShutdown(const json& request) {
        isServerShutdown = true;
        return genResponse(
            request["id"].get<size_t>(),
            json(),
            json()
        );
    }

    json handleExit(const json& request) {
        isServerExited = true;
        isServerInitialized = false;
        return json::object();
    }

    json handleDidOpen(const json& request) {
        const auto& text = request["params"]["textDocument"]["text"].get<std::string>();
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        documentCache[uri] = text;
        return json::object();
    }

    json handleDidChange(const json& request) {
        const auto& changes = request["params"]["contentChanges"];
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        if (!changes.empty()) {
            documentCache[uri] = changes[changes.size() - 1]["text"].get<std::string>();
        }
        return json::object();
    }

    json handleDidClose(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        documentCache.erase(uri);
        return json::object();
    }

    json handleSetTrace(const json& request) {
        traceValue = request["params"]["value"];
        return json::object();
    }

    json genDiagnosticsFromErrorWarningList(const std::vector<std::tuple<std::string, FilePosition::Region>>& errors, const std::vector<std::tuple<std::string, FilePosition::Region>>& warnings) {
        json diagnostics = json::array();
        for (const auto& error : errors) {
            auto [message, region] = error;
            json diag;
            diag["range"]["start"]["line"] = region.start.line.getValue();
            diag["range"]["start"]["character"] = region.start.column.getValue();
            diag["range"]["end"]["line"] = region.end.line.getValue();
            diag["range"]["end"]["character"] = region.end.column.getValue();
            diag["message"] = message;
            diag["severity"] = 1;
            diagnostics.push_back(diag);
        }
        for (const auto& warning : warnings) {
            auto [message, region] = warning;
            json diag;
            diag["range"]["start"]["line"] = region.start.line.getValue();
            diag["range"]["start"]["character"] = region.start.column.getValue();
            diag["range"]["end"]["line"] = region.end.line.getValue();
            diag["range"]["end"]["character"] = region.end.column.getValue();
            diag["message"] = message;
            diag["severity"] = 2;
            diagnostics.push_back(diag);
        }
        return diagnostics;
    }

    json genDiagnosticsForFile(const std::string& uri) {
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> errors;
        std::vector<std::tuple<std::string, FilePosition::Region>> warnings;
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
        errors.insert(errors.end(), lexErrors.begin(), lexErrors.end());
        errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());
        warnings.insert(warnings.end(), lexWarnings.begin(), lexWarnings.end());
        warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());
        json diagnostics = genDiagnosticsFromErrorWarningList(errors, warnings);

        delete docTree;
        tokenList.clear();

        return diagnostics;
    }

    json genPublishDiagnosticsNotification(const std::string& uri) {
        json params;
        params["uri"] = uri;
        params["diagnostics"] = genDiagnosticsForFile(uri);
        return genNotification("textDocument/publishDiagnostics", params);
    }

    json handlePullDiagnostic(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }

        json diagnostics = genDiagnosticsForFile(uri);

        json result;
        result["kind"] = "full";
        result["items"] = diagnostics;

        return genResponse(
            request["id"].get<size_t>(),
            result,
            json()
        );
    }

    json handleSemanticTokens(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }

        // Get tokens with positions
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
        auto tokens = tokenList.GetTokenList();

        std::vector<size_t> data;
        size_t prevLine = 0;
        size_t prevChar = 0;

        for (const auto& token : tokens) {
            auto [content, type, prop, region] = token;

            // Calculate delta positions
            size_t deltaLine = region.start.line.getValue() - prevLine;
            size_t deltaChar = (deltaLine == 0) ? region.start.column.getValue() - prevChar : region.start.column.getValue();

            size_t length = content.length();
            size_t tokenType = getTokenTypeIndex(type, prop);

            data.insert(data.end(), { deltaLine, deltaChar, length, tokenType, 0 });

            prevLine = region.start.line.getValue();
            prevChar = region.start.column.getValue();
        }

        delete docTree;
        tokenList.clear();

        json result;
        result["data"] = data;

        return genResponse(
            request["id"].get<size_t>(),
            result,
            json()
        );
    }

    size_t getTokenTypeIndex(const std::string& type, Type::Type* prop) {
        static const std::vector<std::string> types = {
            "datetime", "number", "boolean", "identifier",
            "punctuator", "operator", "comment", "string", "unknown"
        };

        auto it = std::find(types.begin(), types.end(), type);
        return it != types.end() ? std::distance(types.begin(), it) : 8;
    }

    json handleFormatting(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        // Perform formatting here
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
        auto newToml = DocTree::toToml(docTree);
        auto edits = computeEdits(it->second, newToml);

        delete docTree;
        tokenList.clear();

        json result;
        if (edits.empty()) {
            result = json::object();
        }
        else {
            result = json::array();
            for (auto& edit : edits) {
                json change;
                change["range"]["start"]["line"] = edit.range.start.line;
                change["range"]["start"]["character"] = edit.range.start.character;
                change["range"]["end"]["line"] = edit.range.end.line;
                change["range"]["end"]["character"] = edit.range.end.character;
                change["newText"] = edit.newText;
                result.push_back(change);
            }
        }

        return genResponse(
            request["id"].get<size_t>(),
            result,
            json()
        );
    }

    json handleDefinition(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
		json definition = json::object();
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<3>(token).contains(position)) {
                auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
                if (tokenDocTreeMapping.find(tokenIndex) == tokenDocTreeMapping.end()) {
                    continue;
                }
                auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), tokenListIterator)];
                if (auto table = dynamic_cast<DocTree::Table*>(std::get<1>(targetKey->get()))) {
                    auto tableRegion = std::get<2>(table->get());
                    definition["uri"] = uri;
                    definition["range"]["start"]["line"] = tableRegion.start.line.getValue();
                    definition["range"]["start"]["character"] = tableRegion.start.column.getValue();
                    definition["range"]["end"]["line"] = tableRegion.end.line.getValue();
                    definition["range"]["end"]["character"] = tableRegion.end.column.getValue();
                }
                else if (auto array = dynamic_cast<DocTree::Array*>(std::get<1>(targetKey->get()))) {
                    auto arrayRegion = std::get<2>(array->get());
                    definition["uri"] = uri;
                    definition["range"]["start"]["line"] = arrayRegion.start.line.getValue();
                    definition["range"]["start"]["character"] = arrayRegion.start.column.getValue();
                    definition["range"]["end"]["line"] = arrayRegion.end.line.getValue();
                    definition["range"]["end"]["character"] = arrayRegion.end.column.getValue();
                }
            }
        }
        delete docTree;
        tokenList.clear();
        return genResponse(
            request["id"].get<size_t>(),
            definition,
            json()
        );
    }

    json handleCompletion(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
        auto completions = json::array();
        DocTree::Table* lastDefinedTable = docTree;
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<1>(token) == "identifier" && std::next(tokenListIterator) != tokenList.end() && std::get<0>(*std::next(tokenListIterator)) == "]") {
                auto lastDefinedTableKeyValue = std::get<1>(tokenDocTreeMapping[std::distance(tokenList.begin(), tokenListIterator)]->get());
                if (dynamic_cast<DocTree::Table*>(lastDefinedTableKeyValue)) {
                    lastDefinedTable = dynamic_cast<DocTree::Table*>(lastDefinedTableKeyValue);
                }
                else if (auto keyArrayValue = dynamic_cast<DocTree::Array*>(lastDefinedTableKeyValue)) {
                    auto keyArray = std::get<0>(keyArrayValue->get());
                    if (keyArray.size() && dynamic_cast<DocTree::Table*>(keyArray.back())) {
                        lastDefinedTable = dynamic_cast<DocTree::Table*>(keyArray.back());
                    }
                }
            }
            if (std::get<3>(token).contains(position)) {
                auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
				if (tokenDocTreeMapping.find(tokenIndex) != tokenDocTreeMapping.end() || std::get<0>(token) == ".") {
                    std::vector<std::pair<std::string, DocTree::Key*>> completionKeyPairs;
                    if (std::get<0>(token) == ".") {
                        auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), std::prev(tokenListIterator))];
                        auto keyValue = std::get<1>(targetKey->get());
                        if (auto keyTableValue = dynamic_cast<DocTree::Table*>(keyValue)) {
                            auto keyTable= std::get<0>(keyTableValue->get());
                            for (auto& keyPair : keyTable) {
                                completionKeyPairs.push_back(keyPair);
                            }
                        }
                        else if (auto keyArrayValue = dynamic_cast<DocTree::Array*>(keyValue)) {
                            auto keyArray = std::get<0>(keyArrayValue->get());
                            if (keyArray.size() && dynamic_cast<DocTree::Table*>(keyArray.back())) {
                                auto keyTableValue = dynamic_cast<DocTree::Table*>(keyArray.back());
                                auto keyTable = std::get<0>(keyTableValue->get());
                                for (auto& keyPair : keyTable) {
                                    completionKeyPairs.push_back(keyPair);
                                }
                            }
                        }
                    }
                    else {
                        auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), tokenListIterator)];
                        auto parentTable = std::get<0>(std::get<2>(targetKey->get())->get());
                        completionKeyPairs = findPairs(parentTable, std::get<0>(targetKey->get()));
                        for (auto keyPairIterator = completionKeyPairs.begin(); keyPairIterator != completionKeyPairs.end(); ++keyPairIterator) {
                            if (keyPairIterator->second == targetKey) {
                                completionKeyPairs.erase(keyPairIterator);
                                break;
                            }
                        }
                    }
                    for (auto& completionKeyPair : completionKeyPairs) {
                        auto completionKeyId = std::get<0>(completionKeyPair);
                        auto completionKeyValue = std::get<1>(std::get<1>(completionKeyPair)->get());
                        if (auto table = dynamic_cast<DocTree::Table*>(completionKeyValue)) {
                            auto tableRegion = std::get<2>(table->get());
                            auto tableDefStartString = " defined at ln " + std::to_string(tableRegion.start.line.getValue() + 1) + ", col " + std::to_string(tableRegion.start.column.getValue() + 1);
                            json completionItem;
                            completionItem["label"] = completionKeyId;
                            completionItem["kind"] = 6;
                            completionItem["detail"] = "Table" + tableDefStartString;
                            completionItem["insertText"] = completionKeyId;
                            completions.push_back(completionItem);
                        }
                        else if (auto array = dynamic_cast<DocTree::Array*>(completionKeyValue)) {
                            auto arrayRegion = std::get<2>(array->get());
                            auto arrayDefStartString = " defined at ln " + std::to_string(arrayRegion.start.line.getValue() + 1) + ", col " + std::to_string(arrayRegion.start.column.getValue() + 1);
                            json completionItem;
                            completionItem["label"] = completionKeyId;
                            completionItem["kind"] = 6;
                            completionItem["detail"] = "Array" + arrayDefStartString;
                            completions.push_back(completionItem);
                        }
                    }
                }
            }
            else if (std::get<3>(token).end.line > position.line && (std::next(tokenListIterator) == tokenList.end() || std::get<3>(*std::next(tokenListIterator)).start < position)) {
                auto keyTable = std::get<0>(lastDefinedTable->get());
                for (auto& completionKeyPair : keyTable) {
                    auto completionKeyId = std::get<0>(completionKeyPair);
                    auto completionKeyValue = std::get<1>(std::get<1>(completionKeyPair)->get());
                    if (auto table = dynamic_cast<DocTree::Table*>(completionKeyValue)) {
                        auto tableRegion = std::get<2>(table->get());
                        auto tableDefStartString = " defined at ln " + std::to_string(tableRegion.start.line.getValue() + 1) + ", col " + std::to_string(tableRegion.start.column.getValue() + 1);
                        json completionItem;
                        completionItem["label"] = completionKeyId;
                        completionItem["kind"] = 6;
                        completionItem["detail"] = "Table" + tableDefStartString;
                        completionItem["insertText"] = completionKeyId;
                        completions.push_back(completionItem);
                    }
                    else if (auto array = dynamic_cast<DocTree::Array*>(completionKeyValue)) {
                        auto arrayRegion = std::get<2>(array->get());
                        auto arrayDefStartString = " defined at ln " + std::to_string(arrayRegion.start.line.getValue() + 1) + ", col " + std::to_string(arrayRegion.start.column.getValue() + 1);
                        json completionItem;
                        completionItem["label"] = completionKeyId;
                        completionItem["kind"] = 6;
                        completionItem["detail"] = "Array" + arrayDefStartString;
                        completions.push_back(completionItem);
                    }
                }
            }
        }
        delete docTree;
        tokenList.clear();
        json result;
        if (completions.size()) {
            result["isIncomplete"] = false;
            result["items"] = completions;
        }
        else {
            result = json::object();
        }
        return genResponse(
            request["id"].get<size_t>(),
            result,
            json()
        );
    }

    json handleHover(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = parser(tokenList);
        auto hover = json::object();
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            const auto tokenRange = std::get<3>(token);
            if (tokenRange.contains(position)) {
                auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
                if (tokenDocTreeMapping.find(tokenIndex) == tokenDocTreeMapping.end()) {
                    continue;
                }
                auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), tokenListIterator)];
                if (auto table = dynamic_cast<DocTree::Table*>(std::get<1>(targetKey->get()))) {
                    auto [elems, isMutable, defPos, isExplicitlyDefined] = table->get();
                    std::string markdown = "## **Table** " + std::get<0>(targetKey->get()) + "\n";
                    markdown += "- **Mutability**: " + std::string(isMutable ? "mutable" : "immutable") + "\n";
                    markdown += "- **Explicitly Defined**: " + std::string(isExplicitlyDefined ? "Yes" : "No") + "\n";
                    markdown += "- **Entries**: " + std::to_string(elems.size()) + "\n";
                    markdown += "- **Defined At**: ln " + std::to_string(defPos.start.line.getValue() + 1) + ", col " + std::to_string(defPos.start.column.getValue() + 1);
                    hover["contents"]["kind"] = "markdown";
                    hover["contents"]["value"] = markdown;
                    hover["range"]["start"]["line"] = tokenRange.start.line.getValue();
                    hover["range"]["start"]["character"] = tokenRange.start.column.getValue();
                    hover["range"]["end"]["line"] = tokenRange.end.line.getValue();
                    hover["range"]["end"]["character"] = tokenRange.end.column.getValue();
                }
                else if (auto array = dynamic_cast<DocTree::Array*>(std::get<1>(targetKey->get()))) {
                    auto [elems, isMutable, defPos] = array->get();
                    std::string markdown = "## **Array** " + std::get<0>(targetKey->get()) + "\n";
                    markdown += "- **Mutability**: " + std::string(isMutable ? "mutable" : "immutable") + "\n";
                    markdown += "- **Entries**: " + std::to_string(elems.size()) + "\n";
                    markdown += "- **Defined At**: ln " + std::to_string(defPos.start.line.getValue() + 1) + ", col " + std::to_string(defPos.start.column.getValue() + 1);
                    hover["contents"]["kind"] = "markdown";
                    hover["contents"]["value"] = markdown;
                    hover["range"]["start"]["line"] = tokenRange.start.line.getValue();
                    hover["range"]["start"]["character"] = tokenRange.start.column.getValue();
                    hover["range"]["end"]["line"] = tokenRange.end.line.getValue();
                    hover["range"]["end"]["character"] = tokenRange.end.column.getValue();
                }
            }
        }
        delete docTree;
        tokenList.clear();
        return genResponse(
            request["id"].get<size_t>(),
            hover,
            json()
        );
    }
};

#endif // LANGUAGE_SERVER_H
