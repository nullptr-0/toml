#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
#include "shared/Token.h"
#include "shared/DocumentTree.h"
#include "shared/Dump.h"
#include "shared/DocTree2Json.h"

#ifndef STDIO_ONLY
#include "shared/unisock.hpp"
#include "shared/unipipe.hpp"
#endif // !STDIO_ONLY

#ifdef EMSCRIPTEN
#include "shared/BlockingInput.h"
BlockingStdinStream cbin;
#endif

extern std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> lexerMain(std::istream& inputCode, bool multilineToken = true);
extern std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> rdparserMain(Token::TokenList<>& tokenList);

extern int langSvrMain(std::istream& inChannel, std::ostream& outChannel, const std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>& lexer, const std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(Token::TokenList<>& tokenList)>& parser);

//#define DEBUG

int main(int argc, char* argv[]) {
    std::vector<std::string> argVector(argv, argv + argc);
    auto printInfo = [](std::ostream& stream) {
        stream << "TOML: A TOML Implementation [alpha]\n";
        stream << "Built at: " __TIME__ " " __DATE__ << "\n";
        stream << "Copyright (C) 2023-2025 nullptr-0.\n";
        stream << "Open-source Projects:\n";
        stream << "json: MIT License https://github.com/nlohmann/json/blob/master/LICENSE.MIT\n";
        stream << "regex: Boost Software License http://www.boost.org/LICENSE_1_0.txt\n";
        stream.flush();
    };
    auto printHelp = [&argVector](std::ostream& stream) {
        stream << "Usage:\n"
            << argVector[0] << " --parse <path>[ --output=<path>]\n"
            << argVector[0] << " --parse <path>[ --output <path>]\n"
            << argVector[0] << " --langsvr --stdio\n"
#ifndef STDIO_ONLY
            << argVector[0] << " --langsvr --socket=<port>\n"
            << argVector[0] << " --langsvr --socket <port>\n"
            << argVector[0] << " --langsvr --port=<port>\n"
            << argVector[0] << " --langsvr --port <port>\n"
            << argVector[0] << " --langsvr --pipe=<pipe>\n"
            << argVector[0] << " --langsvr --pipe <pipe>\n"
#endif // !STDIO_ONLY
            << argVector[0] << " --help\n"
            << argVector[0] << " -h\n";
    };
    if (argc >= 3 && argVector[1] == "--langsvr") {
        auto lexer = [](const std::string& input, bool multilineToken) -> std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> {
            std::istringstream stream(input);
            return lexerMain(stream, multilineToken);
        };
        int retVal = 1;
        if (argc == 3 && argVector[2] == "--stdio") {
            retVal = langSvrMain(
#ifdef EMSCRIPTEN
                cbin
#else
                std::cin
#endif
                , std::cout, lexer, rdparserMain);
        }
#ifndef STDIO_ONLY
        else if (argc >= 3 && (argVector[2].substr(0, 6) == "--port" || argVector[2].substr(0, 8) == "--socket")) {
            size_t optionPrefixLength = argVector[2].substr(0, 6) == "--port" ? 6 : 8;
#ifndef DEBUG
            try {
#endif // DEBUG
                std::string port;
                if (argc == 4 && argVector[2].size() == optionPrefixLength) {
                    port = argVector[3];
                }
                else if (argc == 3) {
                    port = argVector[2].substr(optionPrefixLength + 1);
                }
                else {
                    throw std::invalid_argument("invalid arguments");
                }
                socketstream socket("127.0.0.1", atoi(port.c_str()), socketstream::client);
                if (!socket.is_open()) {
                    throw std::runtime_error("unable to open socket on port " + port);
                    return 2;
                }
                retVal = langSvrMain(socket, socket, lexer, rdparserMain);
#ifndef DEBUG
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                return 1;
            }
#endif // DEBUG
        }
        else if (argc >= 3 && (argVector[2].substr(0, 6) == "--pipe")) {
#ifndef DEBUG
            try {
#endif // DEBUG
                std::string pipeName;
                if (argc == 4 && argVector[2].size() == 6) {
                    pipeName = argVector[3];
                }
                else if (argc == 3) {
                    pipeName = argVector[2].substr(7);
                }
                else {
                    throw std::invalid_argument("invalid arguments");
                }
                pipestream pipe(pipeName, NamedPipeDescriptor::client);
                if (!pipe.is_open()) {
                    throw std::runtime_error("unable to open pipe " + pipeName);
                }
                retVal = langSvrMain(pipe, pipe, lexer, rdparserMain);
#ifndef DEBUG
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                return 1;
            }
#endif // DEBUG
        }
#endif // !STDIO_ONLY
        else {
            printInfo(std::cerr);
            std::cerr << "invalid arguments:";
            for (const auto& arg : argVector) {
                std::cerr << " " << arg;
            }
            std::cerr << "\n";
            return 2;
        }
        return retVal;
    }
    else if (argc >= 3 && argVector[1] == "--parse") {
        int retVal = 0;
        std::string outputPath;
        if (argc >= 4) {
            if (argc == 5 && argVector[3] == "--output") {
                outputPath = argVector[4];
            }
            else if (argc == 4 && argVector[3].substr(0, 8) == "--output=") {
                outputPath = argVector[3].substr(9);
            }
            else {
                printInfo(std::cerr);
                std::cerr << "invalid arguments:";
                for (const auto& arg : argVector) {
                    std::cerr << " " << arg;
                }
                std::cerr << "\n";
                return 2;
            }
        }
        const auto inputPath = argVector[2];
        printInfo(std::cout);
        std::list<std::fstream*> openedFileStreams;
        auto getStreamForDiskFile = [&openedFileStreams](const std::string& path, std::ios_base::openmode mode) -> std::iostream* {
            if ((mode & std::ios::out) && !std::filesystem::exists(path)) {
                std::ofstream(path, std::ios::out).close();
            }
            auto file = new std::fstream(path, mode);
            if (!file->is_open()) {
                throw std::runtime_error("unable to open " + path);
            }
            openedFileStreams.push_back(file);
            return file;
        };
        if (std::filesystem::is_regular_file(inputPath)) {
#ifndef DEBUG
            try {
#endif // DEBUG
                auto inputStream = getStreamForDiskFile(inputPath, std::ios::in);
                std::vector<std::tuple<std::string, FilePosition::Region>> errors;
                std::vector<std::tuple<std::string, FilePosition::Region>> warnings;
                auto [tokenList, lexErrors, lexWarnings] = lexerMain(*inputStream, true);
                auto [docTree, parseErrors, parseWarnings] = rdparserMain(tokenList);
                errors.insert(errors.end(), lexErrors.begin(), lexErrors.end());
                errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());
                warnings.insert(warnings.end(), lexWarnings.begin(), lexWarnings.end());
                warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());
#ifdef DEBUG
                Dump::DumpDocumentTree(docTree);
#endif // DEBUG
                if (errors.size()) {
                    std::cerr << "\nErrors in " << inputPath << ":\n";
                    for (const auto& error : errors) {
                        auto errorStart = std::get<1>(error).start;
                        std::cerr << "Error (line " << errorStart.line << ", col " << errorStart.column << "): " << std::get<0>(error) << "\n";
                    }
                }
                if (warnings.size()) {
                    std::cerr << "\nWarnings in " << inputPath << ":\n";
                    for (const auto& warning : warnings) {
                        auto warningStart = std::get<1>(warning).start;
                        std::cerr << "Warning (line " << warningStart.line << ", col " << warningStart.column << "): " << std::get<0>(warning) << "\n";
                    }
                }

                std::ostream& outputStream = outputPath.empty() ? std::cout << "\nJSON:\n" : *getStreamForDiskFile(outputPath, std::ios::out);
                outputStream << DocTree::toJson(docTree, false) << std::endl;

                delete docTree;
                retVal = errors.size() + warnings.size() ? 1 : 0;
#ifndef DEBUG
            }
            catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
                return 1;
            }
#endif // DEBUG
        }
        for (auto& openedFileStream : openedFileStreams) {
            openedFileStream->flush();
            openedFileStream->close();
            delete openedFileStream;
        }
        return retVal;
    }
    else if (argc == 2 && (argVector[1] == "--help" || argVector[1] == "-h")) {
        printInfo(std::cout);
        printHelp(std::cout);
    }
    else {
        printInfo(std::cerr);
        std::cerr << "invalid arguments:";
        for (const auto& arg : argVector) {
            std::cerr << " " << arg;
        }
        std::cerr << "\n";
        printHelp(std::cerr);
        return 2;
    }
    return 0;
}
