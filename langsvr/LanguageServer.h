#pragma once

#ifndef LANGUAGE_SERVER_H
#define LANGUAGE_SERVER_H

#include <string>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <json.hpp>
#include "../shared/Token.h"
#include "../shared/DocumentTree.h"
#include "../shared/DocTree2Toml.h"
#include "TextEdit.h"

using json = nlohmann::json;

class LanguageServer {
public:
    LanguageServer(const std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>>(const std::string&, bool)>& lexer, const std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>>(Token::TokenList<>& tokenList)>& parser, size_t& jsonId) : lexer(lexer), parser(parser), jsonId(jsonId) {
    }

    json handleRequest(const json& request) {
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
                    else if (request["method"] == "textDocument/diagnostic") {
                        return handlePullDiagnostic(request);
                    }
                    json error;
                    error["jsonrpc"] = "2.0";
                    error["id"] = jsonId;
                    error["error"]["error"] = -32601;
                    error["error"]["message"] = "Method not found";
                    ++jsonId;
                    return error;
                }
            }
        }
        catch (const std::exception& e) {
            json error;
            error["jsonrpc"] = "2.0";
            error["id"] = jsonId;
            error["error"]["error"] = -32603;
            error["error"]["message"] = e.what();
            ++jsonId;
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
    std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>>(const std::string&, bool)> lexer;
    std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>, std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>>(Token::TokenList<>& tokenList)> parser;
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

    json genResponse(const json& result, const json& error) {
        json response;
        if (error.is_null()) {
            response["jsonrpc"] = "2.0";
            response["id"] = jsonId;
            response["result"] = result;
        }
        else {
            response["jsonrpc"] = "2.0";
            response["id"] = jsonId;
            response["error"] = error;
        }
        ++jsonId;
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

    json genDiagnosticsFromErrorWarningList(const std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>& errors, const std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>>& warnings) {
        json diagnostics = json::array();
        for (const auto& error : errors) {
            auto [message, startLine, startCol, endLine, endCol] = error;
            json diag;
            diag["range"]["start"]["line"] = startLine;
            diag["range"]["start"]["character"] = startCol;
            diag["range"]["end"]["line"] = endLine;
            diag["range"]["end"]["character"] = endCol;
            diag["message"] = message;
            diag["severity"] = 1;
            diagnostics.push_back(diag);
        }
        for (const auto& warning : warnings) {
            auto [message, startLine, startCol, endLine, endCol] = warning;
            json diag;
            diag["range"]["start"]["line"] = startLine;
            diag["range"]["start"]["character"] = startCol;
            diag["range"]["end"]["line"] = endLine;
            diag["range"]["end"]["character"] = endCol;
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

        std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>> errors;
        std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>> warnings;
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings] = parser(tokenList);
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
        std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>> errors;
        std::vector<std::tuple<std::string, size_t, size_t, size_t, size_t>> warnings;
        auto [tokenList, lexErrors, lexWarnings] = lexer(it->second, clientSupportsMultilineToken);
        auto [docTree, parseErrors, parseWarnings] = parser(tokenList);
        errors.insert(errors.end(), lexErrors.begin(), lexErrors.end());
        errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());
        warnings.insert(warnings.end(), lexWarnings.begin(), lexWarnings.end());
        warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());
        auto tokens = tokenList.GetTokenList();

        std::vector<size_t> data;
        size_t prevLine = 0;
        size_t prevChar = 0;

        for (const auto& token : tokens) {
            auto [content, type, prop, startLine, startCol, endLine, endCol] = token;

            // Calculate delta positions
            size_t deltaLine = startLine - prevLine;
            size_t deltaChar = (deltaLine == 0) ? startCol - prevChar : startCol;

            size_t length = endCol - startCol;
            size_t tokenType = getTokenTypeIndex(type, prop);

            data.insert(data.end(), { deltaLine, deltaChar, length, tokenType, 0 });

            prevLine = startLine;
            prevChar = startCol;
        }

        delete docTree;
        tokenList.clear();

        json result;
        result["data"] = data;

        return genResponse(
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
        auto [docTree, parseErrors, parseWarnings] = parser(tokenList);
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
            result,
            json()
        );
    }
};

#endif // LANGUAGE_SERVER_H
