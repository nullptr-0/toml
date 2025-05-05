#pragma once

#ifndef LANGUAGE_SERVER_H
#define LANGUAGE_SERVER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <iostream>
#include <fstream>
#include <json.hpp>
#include "../shared/Token.h"
#include "../shared/FilePosition.h"
#include "../shared/DocumentTree.h"
#include "../shared/DocTree2Toml.h"
#include "../shared/Components.h"
#include "TextEdit.h"
#include "FindPairs.h"

using json = nlohmann::json;

enum LineEndType {
    LF,
    CRLF,
    UNKNOWN
};

LineEndType lineEndType = LineEndType::UNKNOWN;

std::string readLSPContent(std::istream& stream) {
    std::string line;
    size_t contentLength = 0;

    // Read header part
    while (true) {
        char ch = stream.get();
        if (ch == EOF) {
            throw std::runtime_error("unexpected EOF reached when reading LSP header");
        }

        line += ch;

        // Header fields are terminated by "\r\n"
        if (!line.empty() && line[line.size() - 1] == '\n') {
            if (line == "\n" || line == "\r\n") {
                if (lineEndType == LineEndType::UNKNOWN) {
                    lineEndType = line.size() == 1 ? LF : CRLF;
                }
                if (contentLength) {
                    break;  // End of headers
                }
            }

            if (line.find("Content-Length:") == 0) {
                contentLength = std::stoi(line.substr(15));  // Extract content length
            }

            line.clear();
        }
    }
    // Read content part
    std::string content;
    content.reserve(contentLength);
    for (size_t i = 0; i < contentLength; ++i) {
        char ch = stream.get();
        if (ch == EOF) {
            throw std::runtime_error("unexpected EOF reached when reading LSP content");
        }
        content += ch;
    }

    return content;
}

void writeLSPContent(std::ostream& stream, const std::string& content) {
    // Calculate the content length
    size_t contentLength = content.size();

    // Create the header
    std::string header = "Content-Length: " + std::to_string(contentLength
#ifdef EMSCRIPTEN
        + (lineEndType == LF ? 1 : 2)
#endif
    ) + (lineEndType == LF ? "\n\n" : "\r\n\r\n");

    // Write the header and content to the file
    stream.write(header.c_str(), header.size());
    stream.write((content
#ifdef EMSCRIPTEN
        + (lineEndType == LF ? "\n" : "\r\n")
#endif
        ).c_str(), content.size()
#ifdef EMSCRIPTEN
        + (lineEndType == LF ? 1 : 2)
#endif
    );

    // Flush to ensure data is written
    stream.flush();
}

class LanguageServer {
public:
    LanguageServer(
        std::istream& inChannel, std::ostream& outChannel,
        const TomlLexerFunctionWithStringInput& tomlLexer,
        const TomlParserFunction& tomlParser,
        const CslLexerFunctionWithStringInput& cslLexer,
        const CslParserFunction& cslParser,
        const CslValidatorFunction& cslValidator) :
        tomlLexer(tomlLexer), tomlParser(tomlParser),
        cslLexer(cslLexer), cslParser(cslParser),
        cslValidator(cslValidator), jsonId(0),
        inChannel(inChannel), outChannel(outChannel) {
    }

    int run() {
        size_t jsonId = 0;
        int serverExitCode = -1;
        std::string input = readLSPContent(inChannel);

        while (!input.empty() && serverExitCode == -1) {
            json request;
            try {
                request = json::parse(input);
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing JSON: " << e.what() << "\n";
            }
            try {
                if (isResponse(request)) {
                    try {
                        if (request.contains("id")) {
                            size_t id = request["id"].get<size_t>();
                            auto it = responseCallbacks.find(id);
                            if (it != responseCallbacks.end()) {
                                auto callback = it->second;
                                callback(request);
                                responseCallbacks.erase(it);
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Error processing response: " << e.what() << "\n";
                    }
                }
                else {
                    auto response = handleRequest(request);
                    if (!response.empty() && response != json::object() && !response.is_null()) {
                        sendResponse(response);
                    }
                }
            }
            catch (const std::exception& e) {
                json error;
                error["jsonrpc"] = "2.0";
                error["id"] = jsonId;
                error["error"]["error"] = -32700;
                error["error"]["message"] = e.what();
                ++jsonId;
                sendResponse(error);
            }
            serverExitCode = getServerExitCode();
            input = readLSPContent(inChannel);
        }
        return serverExitCode;
    }

protected:
    std::istream& inChannel;
    std::ostream& outChannel;
    size_t jsonId;
    bool isServerInitialized = false;
    bool isClientInitialized = false;
    bool isServerShutdown = false;
    bool isServerExited = false;
    bool clientSupportsMultilineToken = false;
    std::string traceValue;
    TomlLexerFunctionWithStringInput tomlLexer;
    TomlParserFunction tomlParser;
    CslLexerFunctionWithStringInput cslLexer;
    CslParserFunction cslParser;
    CslValidatorFunction cslValidator;
    std::unordered_map<std::string, std::string> documentCache;
    std::vector<std::shared_ptr<CSL::ConfigSchema>> cslSchemas;
    std::string currentCslSchema;
    std::unordered_map<size_t, std::function<void(const json&)>> responseCallbacks;

    void sendRequest(const json& request, std::function<void(const json&)> callback = [](const json&) {}) {
        if (!request.contains("jsonrpc") ||
            request["jsonrpc"] != "2.0" ||
            !request.contains("id") ||
            !request.contains("method")) {
            return;
        }
        writeLSPContent(outChannel, request.dump());
        responseCallbacks[request["id"].get<size_t>()] = callback;
    }

    bool isResponse(const json& response) {
        return response.contains("jsonrpc")
            && response["jsonrpc"] == "2.0"
            && (response.contains("result")
            || response.contains("error"));
    }

    void sendResponse(const json& response) {
        if (!isResponse(response)) {
            return;
        }
        writeLSPContent(outChannel, response.dump());
    }

    void sendNotification(const json& notification) {
        if (!notification.contains("jsonrpc") ||
            notification["jsonrpc"] != "2.0" ||
            !notification.contains("method")) {
			return;
		}
        writeLSPContent(outChannel, notification.dump());
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
                    else if (request["method"] == "textDocument/references") {
                        return handleReferences(request);
                    }
                    else if (request["method"] == "textDocument/rename") {
                        return handleRename(request);
                    }
                    else if (request["method"] == "textDocument/foldingRange") {
                        return handleFoldingRange(request);
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
                    else if (request["method"] == "configSchemaLanguage/setSchemas") {
                        return handleCslSetSchemas(request);
                    }
                    else if (request["method"] == "configSchemaLanguage/setSchema") {
                        return handleCslSetSchema(request);
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
                       "referencesProvider": true,
                       "renameProvider": true,
                       "foldingRangeProvider": true,
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
        return json();
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
        return json();
    }

    json handleDidOpen(const json& request) {
        const auto& text = request["params"]["textDocument"]["text"].get<std::string>();
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        documentCache[uri] = text;
        return json();
    }

    json handleDidChange(const json& request) {
        const auto& changes = request["params"]["contentChanges"];
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        if (!changes.empty()) {
            documentCache[uri] = changes[changes.size() - 1]["text"].get<std::string>();
        }
        return json();
    }

    json handleDidClose(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        documentCache.erase(uri);
        return json();
    }

    json handleSetTrace(const json& request) {
        traceValue = request["params"]["value"];
        return json();
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

    json genDiagnosticsForTomlFile(const std::string& uri) {
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> errors;
        std::vector<std::tuple<std::string, FilePosition::Region>> warnings;
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        errors.insert(errors.end(), lexErrors.begin(), lexErrors.end());
        errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());
        warnings.insert(warnings.end(), lexWarnings.begin(), lexWarnings.end());
        warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());
        if (cslSchemas.size()) {
            auto [cslErrors, cslWarnings] = cslValidator(currentCslSchema, cslSchemas, docTree);
            errors.insert(errors.end(), cslErrors.begin(), cslErrors.end());
            warnings.insert(warnings.end(), cslWarnings.begin(), cslWarnings.end());
        }
        json diagnostics = genDiagnosticsFromErrorWarningList(errors, warnings);

        delete docTree;
        tokenList.clear();

        return diagnostics;
    }

    json genPublishDiagnosticsNotification(const std::string& uri, json diag = json::array()) {
        json params;
        params["uri"] = uri;
        params["diagnostics"] = diag.size() ? diag : genDiagnosticsForTomlFile(uri);
        return genNotification("textDocument/publishDiagnostics", params);
    }

    json handlePullDiagnostic(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }

        json diagnostics = genDiagnosticsForTomlFile(uri);

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
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
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
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
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
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
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
    
    std::shared_ptr<CSL::TableType> findTableType(
        DocTree::Table* currentDocTable,
        DocTree::Table* targetTable,
        const std::shared_ptr<CSL::TableType>& currentSchemaType
    ) {
        if (currentDocTable == targetTable) {
            return currentSchemaType;
        }

        const auto& docElems = currentDocTable->getElems();
        const auto& schemaExplicitKeys = currentSchemaType->getExplicitKeys();
        const auto& schemaWildcardKey = currentSchemaType->getWildcardKey();

        for (const auto& [keyName, keyNode] : docElems) {
            auto valueNode = std::get<1>(keyNode->get());
            if (auto childDocTable = dynamic_cast<DocTree::Table*>(valueNode)) {
                std::shared_ptr<CSL::CSLType> childSchemaType;

                // Check explicit keys
                auto it = std::find_if(schemaExplicitKeys.begin(), schemaExplicitKeys.end(),
                    [&](const CSL::TableType::KeyDefinition& kd) { return kd.name == keyName; });
                if (it != schemaExplicitKeys.end()) {
                    childSchemaType = it->type;
                }
                // Check wildcard
                else if (schemaWildcardKey) {
                    childSchemaType = schemaWildcardKey->type;
                }
                else {
                    continue;
                }

                // Handle TableType
                if (childSchemaType->getKind() == CSL::CSLType::Kind::Table) {
                    auto childSchemaTable = std::static_pointer_cast<CSL::TableType>(childSchemaType);
                    auto result = findTableType(childDocTable, targetTable, childSchemaTable);
                    if (result) return result;
                }
                // Handle UnionType
                else if (childSchemaType->getKind() == CSL::CSLType::Kind::Union) {
                    auto unionType = std::static_pointer_cast<CSL::UnionType>(childSchemaType);
                    for (const auto& memberType : unionType->getMemberTypes()) {
                        if (memberType->getKind() == CSL::CSLType::Kind::Table) {
                            auto memberTable = std::static_pointer_cast<CSL::TableType>(memberType);
                            auto result = findTableType(childDocTable, targetTable, memberTable);
                            if (result) return result;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    std::shared_ptr<CSL::TableType> getTableTypeForDocTable(
        DocTree::Table* targetTable,
        DocTree::Table* docTree,
        const std::shared_ptr<CSL::ConfigSchema>& schema
    ) {
        if (!docTree || !schema) return nullptr;
        auto rootSchemaType = schema->getRootTable();
        return findTableType(docTree, targetTable, rootSchemaType);
    }

    json handleCompletion(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        auto completions = json::array();
        DocTree::Table* lastDefinedTable = docTree;
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<1>(token) == "identifier" && std::next(tokenListIterator) != tokenList.end() && std::get<0>(*std::next(tokenListIterator)) == "]") {
                auto lastDefinedTableKeyIndex = std::distance(tokenList.begin(), tokenListIterator);
                if (tokenDocTreeMapping.find(lastDefinedTableKeyIndex) != tokenDocTreeMapping.end()) {
                    auto lastDefinedTableKeyValue = std::get<1>(tokenDocTreeMapping[lastDefinedTableKeyIndex]->get());
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
                else {
                    lastDefinedTable = nullptr;
                }
            }
            if (std::get<3>(token).contains(position)) {
                auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
				if (tokenDocTreeMapping.find(tokenIndex) != tokenDocTreeMapping.end() || std::get<0>(token) == ".") {
                    std::vector<std::pair<std::string, DocTree::Key*>> docTreeCompletionKeyPairs;
                    std::vector<std::pair<std::string, CSL::TableType::KeyDefinition>> cslSchemaCompletionKeyPairs;
                    if (std::get<0>(token) == ".") {
                        auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), std::prev(tokenListIterator))];
                        auto keyValue = std::get<1>(targetKey->get());
                        DocTree::Table* keyTableValue = dynamic_cast<DocTree::Table*>(keyValue);
                        if (!keyTableValue && dynamic_cast<DocTree::Array*>(keyValue)) {
                            auto keyArrayValue = (DocTree::Array*)keyValue;
                            auto keyArray = std::get<0>(keyArrayValue->get());
                            if (keyArray.size()) {
                                keyTableValue = dynamic_cast<DocTree::Table*>(keyArray.back());
                            }
                        }
                        if (keyTableValue) {
                            auto keyTable = std::get<0>(keyTableValue->get());
                            for (auto& keyPair : keyTable) {
                                docTreeCompletionKeyPairs.push_back(keyPair);
                            }
                            if (cslSchemas.size()) {
                                std::shared_ptr<CSL::ConfigSchema> schema;
                                if (currentCslSchema.empty() && cslSchemas.size() == 1) {
                                    schema = cslSchemas[0];
                                }
                                for (const auto& sch : cslSchemas) {
                                    if (sch->getName() == currentCslSchema) {
                                        schema = sch;
                                        break;
                                    }
                                }
                                if (schema) {
                                    auto tableType = getTableTypeForDocTable(keyTableValue, docTree, cslSchemas[0]);
                                    if (tableType) {
                                        auto explicitKeys = tableType->getExplicitKeys();
                                        std::unordered_map<std::string, CSL::TableType::KeyDefinition> keyNameKeyDefMapping;
                                        for (const auto& keyDef : explicitKeys) {
                                            cslSchemaCompletionKeyPairs.push_back({ keyDef.name, keyDef });
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else {
                        auto targetKey = tokenDocTreeMapping[std::distance(tokenList.begin(), tokenListIterator)];
                        auto parentTable = std::get<0>(std::get<2>(targetKey->get())->get());
                        docTreeCompletionKeyPairs = findPairs(parentTable, std::get<0>(targetKey->get()));
                        for (auto keyPairIterator = docTreeCompletionKeyPairs.begin(); keyPairIterator != docTreeCompletionKeyPairs.end(); ++keyPairIterator) {
                            if (keyPairIterator->second == targetKey) {
                                docTreeCompletionKeyPairs.erase(keyPairIterator);
                                break;
                            }
                        }
                        if (cslSchemas.size()) {
                            std::shared_ptr<CSL::ConfigSchema> schema;
                            if (currentCslSchema.empty() && cslSchemas.size() == 1) {
                                schema = cslSchemas[0];
                            }
                            for (const auto& sch : cslSchemas) {
                                if (sch->getName() == currentCslSchema) {
                                    schema = sch;
                                    break;
                                }
                            }
                            if (schema) {
                                auto tableType = getTableTypeForDocTable(std::get<2>(targetKey->get()), docTree, schema);
                                if (tableType) {
                                    auto explicitKeys = tableType->getExplicitKeys();
                                    std::unordered_map<std::string, CSL::TableType::KeyDefinition> keyNameKeyDefMapping;
                                    for (const auto& keyDef : explicitKeys) {
                                        keyNameKeyDefMapping[keyDef.name] = keyDef;
                                    }
                                    cslSchemaCompletionKeyPairs = findPairs(keyNameKeyDefMapping, std::get<0>(targetKey->get()));
                                }
                            }
                        }
                    }
                    for (auto& completionKeyPair : docTreeCompletionKeyPairs) {
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
                    for (auto& completionKeyPair : cslSchemaCompletionKeyPairs) {
                        auto completionKeyId = std::get<0>(completionKeyPair);
                        auto completionKeyValue = std::get<1>(completionKeyPair);
                        json completionItem;
                        completionItem["label"] = completionKeyId;
                        completionItem["kind"] = 6;
                        completionItem["detail"] = std::string(completionKeyValue.isOptional ? "Optional" : "Mandatory") + " key in schema";
                        completionItem["insertText"] = completionKeyId;
                        completions.push_back(completionItem);
                    }
                }
            }
            else if (lastDefinedTable && std::get<3>(token).end.line > position.line && (std::next(tokenListIterator) == tokenList.end() || std::get<3>(*std::next(tokenListIterator)).start < position)) {
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
                if (cslSchemas.size()) {
                    std::shared_ptr<CSL::ConfigSchema> schema;
                    if (currentCslSchema.empty() && cslSchemas.size() == 1) {
                        schema = cslSchemas[0];
                    }
                    for (const auto& sch : cslSchemas) {
                        if (sch->getName() == currentCslSchema) {
                            schema = sch;
                            break;
                        }
                    }
                    if (schema) {
                        auto tableType = getTableTypeForDocTable(lastDefinedTable, docTree, cslSchemas[0]);
                        if (tableType) {
                            auto explicitKeys = tableType->getExplicitKeys();
                            std::unordered_map<std::string, CSL::TableType::KeyDefinition> keyNameKeyDefMapping;
                            for (const auto& keyDef : explicitKeys) {
                                auto completionKeyId = keyDef.name;
                                auto completionKeyValue = keyDef;
                                json completionItem;
                                completionItem["label"] = completionKeyId;
                                completionItem["kind"] = 6;
                                completionItem["detail"] = std::string(completionKeyValue.isOptional ? "Optional" : "Mandatory") + " key in schema";
                                completionItem["insertText"] = completionKeyId;
                                completions.push_back(completionItem);
                            }
                        }
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
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        auto hover = json::object();
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            const auto tokenRange = std::get<3>(token);
            if (tokenRange.contains(position)) {
                auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
                if (tokenDocTreeMapping.find(tokenIndex) == tokenDocTreeMapping.end()) {
                    continue;
                }
                auto targetKey = tokenDocTreeMapping[tokenIndex];
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

    json handleReferences(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        bool includeDeclaration = request["params"]["context"]["includeDeclaration"].get<bool>();
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        auto references = json::array();
        std::unordered_map<DocTree::Key*, std::vector<FilePosition::Region>> referencesMap;
        DocTree::Key* targetKey = nullptr;
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
            if (tokenDocTreeMapping.find(tokenIndex) == tokenDocTreeMapping.end()) {
                continue;
            }
            auto curKey = tokenDocTreeMapping[tokenIndex];
            referencesMap[curKey].push_back(std::get<3>(token));
            if (std::get<3>(token).contains(position)) {
                targetKey = curKey;
            }
        }
        FilePosition::Region targetKeyDefRegion;
        if (targetKey) {
            if (auto table = dynamic_cast<DocTree::Table*>(std::get<1>(targetKey->get()))) {
                targetKeyDefRegion = std::get<2>(table->get());
            }
            else if (auto array = dynamic_cast<DocTree::Array*>(std::get<1>(targetKey->get()))) {
                targetKeyDefRegion = std::get<2>(array->get());
            }
            else if (auto value = dynamic_cast<DocTree::Value*>(std::get<1>(targetKey->get()))) {
                targetKeyDefRegion = std::get<2>(value->get());
            }
            for (const auto& keyRef : referencesMap[targetKey]) {
                if (!includeDeclaration && keyRef == targetKeyDefRegion) {
                    continue;
                }
                json reference;
                reference["uri"] = uri;
                reference["range"]["start"]["line"] = keyRef.start.line.getValue();
                reference["range"]["start"]["character"] = keyRef.start.column.getValue();
                reference["range"]["end"]["line"] = keyRef.end.line.getValue();
                reference["range"]["end"]["character"] = keyRef.end.column.getValue();
                references.push_back(reference);
            }
        }
        delete docTree;
        tokenList.clear();
        return genResponse(
            request["id"].get<size_t>(),
            references,
            json()
        );
    }

    json handleRename(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        auto newName = request["params"]["newName"].get<std::string>();
        FilePosition::Position position = { { request["params"]["position"]["line"].get<size_t>(), false }, { request["params"]["position"]["character"].get<size_t>(), false } };
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        std::unordered_map<DocTree::Key*, std::vector<FilePosition::Region>> referencesMap;
        DocTree::Key* targetKey = nullptr;
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            auto tokenIndex = std::distance(tokenList.begin(), tokenListIterator);
            if (tokenDocTreeMapping.find(tokenIndex) == tokenDocTreeMapping.end()) {
                continue;
            }
            auto curKey = tokenDocTreeMapping[tokenIndex];
            referencesMap[curKey].push_back(std::get<3>(token));
            if (std::get<3>(token).contains(position)) {
                targetKey = curKey;
            }
        }
        delete docTree;
        tokenList.clear();
        json result;
        if (targetKey) {
            json changes = json::array();
            for (const auto& ref : referencesMap[targetKey]) {
                json change;
                change["range"]["start"]["line"] = ref.start.line.getValue();
                change["range"]["start"]["character"] = ref.start.column.getValue();
                change["range"]["end"]["line"] = ref.end.line.getValue();
                change["range"]["end"]["character"] = ref.end.column.getValue();
                change["newText"] = newName;
                changes.push_back(change);
            }
            result["changes"][uri] = changes;
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

    json handleFoldingRange(const json& request) {
        const auto& uri = request["params"]["textDocument"]["uri"].get<std::string>();
        auto it = documentCache.find(uri);
        if (it == documentCache.end()) {
            throw std::runtime_error("Document not found");
        }
        auto [tokenList, lexErrors, lexWarnings] = tomlLexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = tomlParser(tokenList);
        auto ranges = json::array();
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end() && std::next(tokenListIterator) != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<0>(token) == "{") {
                auto startPosition = std::get<3>(token).start;
                for (tokenListIterator = std::next(tokenListIterator); tokenListIterator != tokenList.end(); ++tokenListIterator) {
                    if (std::get<0>(*tokenListIterator) == "}") {
                        break;
                    }
                }
                if (tokenListIterator == tokenList.end()) {
                    break;
                }
                auto endPosition = std::get<3>(*tokenListIterator).end;
                if (startPosition.line == endPosition.line) {
                    continue;
                }
                json range;
                range["startLine"] = startPosition.line.getValue();
                range["startCharacter"] = startPosition.column.getValue();
                range["endLine"] = endPosition.line.getValue();
                range["endCharacter"] = endPosition.column.getValue();
                range["kind"] = "range";
                ranges.push_back(range);
            }
        }
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end() && std::next(tokenListIterator) != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<0>(token) == "[") {
                auto IsTableHeaderDef = [&tokenList, &tokenDocTreeMapping](const auto& tokenListIterator) {
                    return std::get<0>(*tokenListIterator) == "[" && std::get<1>(*std::next(tokenListIterator)) == "identifier" && tokenDocTreeMapping.find(std::distance(tokenList.begin(), std::next(tokenListIterator))) != tokenDocTreeMapping.end();
                    };
                auto IsArrayHeaderDef = [&tokenList, &tokenDocTreeMapping](const auto& tokenListIterator) {
                    return std::get<0>(*tokenListIterator) == "[" && std::get<0>(*std::next(tokenListIterator)) == "[" && std::next(tokenListIterator, 2) != tokenList.end() && std::get<1>(*std::next(tokenListIterator, 2)) == "identifier" && tokenDocTreeMapping.find(std::distance(tokenList.begin(), std::next(tokenListIterator, 2))) != tokenDocTreeMapping.end();
                    };
                auto IsHeaderDef = [&IsTableHeaderDef, &IsArrayHeaderDef](const auto& tokenListIterator) {
                    return IsArrayHeaderDef(tokenListIterator) || IsTableHeaderDef(tokenListIterator);
                    };
                auto startPosition = std::get<3>(token).start;
                if (IsHeaderDef(tokenListIterator)) {
                    for (tokenListIterator = std::next(tokenListIterator); tokenListIterator != tokenList.end(); ++tokenListIterator) {
                        if (std::next(tokenListIterator) == tokenList.end() || IsHeaderDef(std::next(tokenListIterator))) {
                            break;
                        }
                    }
                }
                else {
                    for (tokenListIterator = std::next(tokenListIterator); tokenListIterator != tokenList.end(); ++tokenListIterator) {
                        if (std::get<0>(*tokenListIterator) == "]") {
                            break;
                        }
                    }
                }
                if (tokenListIterator == tokenList.end()) {
                    break;
                }
                auto endPosition = std::get<3>(*tokenListIterator).end;
                if (startPosition.line == endPosition.line) {
                    continue;
                }
                json range;
                range["startLine"] = startPosition.line.getValue();
                range["startCharacter"] = startPosition.column.getValue();
                range["endLine"] = endPosition.line.getValue();
                range["endCharacter"] = endPosition.column.getValue();
                range["kind"] = "range";
                ranges.push_back(range);
            }
        }
        for (auto tokenListIterator = tokenList.begin(); tokenListIterator != tokenList.end() && std::next(tokenListIterator) != tokenList.end(); ++tokenListIterator) {
            const auto& token = *tokenListIterator;
            if (std::get<1>(token) == "comment") {
                auto startPosition = std::get<3>(token).start;
                for (; tokenListIterator != tokenList.end(); ++tokenListIterator) {
                    if (std::next(tokenListIterator) == tokenList.end() || std::get<1>(*std::next(tokenListIterator)) != "comment") {
                        break;
                    }
                }
                if (tokenListIterator == tokenList.end()) {
                    break;
                }
                auto endPosition = std::get<3>(*tokenListIterator).end;
                if (startPosition.line == endPosition.line) {
                    continue;
                }
                json range;
                range["startLine"] = startPosition.line.getValue();
                range["startCharacter"] = startPosition.column.getValue();
                range["endLine"] = endPosition.line.getValue();
                range["endCharacter"] = endPosition.column.getValue();
                range["kind"] = "comment";
                ranges.push_back(range);
            }
        }
        delete docTree;
        tokenList.clear();
        return genResponse(
            request["id"].get<size_t>(),
            ranges,
            json()
        );
    }

    json handleCslSetSchemas(const json& request) {
        cslSchemas.clear();
        std::string cslContent = request["params"]["schemas"].get<std::string>();
        if (request["params"].contains("schema")) {
            currentCslSchema = request["params"]["schema"].get<std::string>();
        }
        auto [tokenList, lexErrors, lexWarnings] = cslLexer(cslContent, clientSupportsMultilineToken);
        auto [schemas, parseErrors, parseWarnings] = cslParser(tokenList);
        cslSchemas = schemas;
        sendRequest(genRequest("workspace/diagnostic/refresh", json()));
        return genResponse(
            request["id"].get<size_t>(),
            json(),
            json()
        );
    }

    json handleCslSetSchema(const json& request) {
        currentCslSchema = request["params"]["schema"].get<std::string>();
        sendRequest(genRequest("workspace/diagnostic/refresh", json()));
        return genResponse(
            request["id"].get<size_t>(),
            json(),
            json()
        );
    }
};

#endif // LANGUAGE_SERVER_H
