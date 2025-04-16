#include <functional>
#include <iostream>
#include <json.hpp>
#include "LanguageServer.h"

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

int langSvrMain(std::istream& inChannel, std::ostream& outChannel, const std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>& lexer, const std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(Token::TokenList<>& tokenList)>& parser) {
    size_t jsonId = 0;
    LanguageServer server(lexer, parser, jsonId);
    int serverExitCode = -1;
    std::string input = readLSPContent(inChannel);

    while (!input.empty() && serverExitCode == -1) {
        json request;
        try {
            request = json::parse(input);
        }
        catch (const std::exception& e) {
            std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        }
        try {
            if (request.contains("method")) {
                auto response = server.handleRequest(request);
                auto writeSingleResponse = [&outChannel](const json& response) {
                    if (!response.empty() && response != json::object() && !response.is_null()) {
                        writeLSPContent(outChannel, response.dump());
                    }
                    };
                if (response.is_array()) {
                    for (const auto& resp : response) {
                        writeSingleResponse(resp);
                    }
                }
                else {
                    writeSingleResponse(response);
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
            writeLSPContent(outChannel, error.dump());
        }
        serverExitCode = server.getServerExitCode();
        input = readLSPContent(inChannel);
    }
    return serverExitCode;
}