#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>
#include "../shared/Components.h"
#include "../shared/Dump.h"
#include "../shared/DocTree2Json.h"

#ifndef STDIO_ONLY
#include "../shared/UniSock.hpp"
#include "../shared/UniPipe.hpp"
#endif // !STDIO_ONLY

#ifdef EMSCRIPTEN
#include "../shared/BlockingInput.h"
BlockingStdinStream cbin;
#endif

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
            << argVector[0] << " --parse <path>[ --validate=<path>][ --schema=<name>][ --output=<path>]\n"
            << argVector[0] << " --parse <path>[ --validate <path>][ --schema <name>][ --output <path>]\n"
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
        auto tomlStringLexer = [](const std::string& input, bool multilineToken) -> std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> {
            std::istringstream stream(input);
            return TomlLexerMain(stream, multilineToken);
        };
        auto cslStringLexer = [](const std::string& input, bool multilineToken) -> std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> {
            std::istringstream stream(input);
            return CslLexerMain(stream, multilineToken);
            };
        int retVal = 1;
        if (argc == 3 && argVector[2] == "--stdio") {
            retVal = TomlLangSvrMain(
#ifdef EMSCRIPTEN
                cbin
#else
                std::cin
#endif
                , std::cout, tomlStringLexer, TomlRdparserMain, cslStringLexer, CslRdParserMain, CslValidatorMain);
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
                retVal = TomlLangSvrMain(socket, socket, tomlStringLexer, TomlRdparserMain, cslStringLexer, CslRdParserMain, CslValidatorMain);
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
                retVal = TomlLangSvrMain(pipe, pipe, tomlStringLexer, TomlRdparserMain, cslStringLexer, CslRdParserMain, CslValidatorMain);
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
        std::string cslPath;
        std::string cslSchemaName;
        std::string outputPath;
        for (size_t i = 3; i < argVector.size(); ++i) {
            const std::string& arg = argv[i];

            // Check for --validate
            if (arg.rfind("--validate=", 0) == 0) {
                cslPath = arg.substr(11); // skip "--validate="
            }
            else if (arg == "--validate") {
                if (i + 1 < argVector.size()) {
                    cslPath = argVector[i + 1];
                    ++i; // skip next since it's used
                }
                else {
                    printInfo(std::cerr);
                    std::cerr << "invalid arguments:" << arg;
                    for (const auto& arg : argVector) {
                        std::cerr << " " << arg;
                    }
                    std::cerr << "\n";
                    return 2;
                }
            }

            else if (arg.rfind("--schema=", 0) == 0) {
                cslSchemaName = arg.substr(9); // skip "--output="
            }
            else if (arg == "--schema") {
                if (i + 1 < argVector.size()) {
                    cslSchemaName = argVector[i + 1];
                    ++i; // skip next since it's used
                }
                else {
                    printInfo(std::cerr);
                    std::cerr << "invalid arguments:" << arg;
                    for (const auto& arg : argVector) {
                        std::cerr << " " << arg;
                    }
                    std::cerr << "\n";
                    return 2;
                }
            }

            // Check for --output
            else if (arg.rfind("--output=", 0) == 0) {
                outputPath = arg.substr(9); // skip "--output="
            }
            else if (arg == "--output") {
                if (i + 1 < argVector.size()) {
                    outputPath = argVector[i + 1];
                    ++i; // skip next since it's used
                }
                else {
                    printInfo(std::cerr);
                    std::cerr << "invalid arguments:" << arg;
                    for (const auto& arg : argVector) {
                        std::cerr << " " << arg;
                    }
                    std::cerr << "\n";
                    return 2;
                }
            }

            else {
                printInfo(std::cerr);
                std::cerr << "invalid arguments:" << arg;
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
                auto [tomlTokenList, tomlLexErrors, tomlLexWarnings] = TomlLexerMain(*inputStream, true);
                auto [docTree, tomlParseErrors, tomlParseWarnings, tokenDocTreeMapping] = TomlRdparserMain(tomlTokenList);
                errors.insert(errors.end(), tomlLexErrors.begin(), tomlLexErrors.end());
                errors.insert(errors.end(), tomlParseErrors.begin(), tomlParseErrors.end());
                warnings.insert(warnings.end(), tomlLexWarnings.begin(), tomlLexWarnings.end());
                warnings.insert(warnings.end(), tomlParseWarnings.begin(), tomlParseWarnings.end());
                if (cslPath.size() && std::filesystem::is_regular_file(cslPath)) {
                    auto cslInputStream = getStreamForDiskFile(cslPath, std::ios::in);
                    auto [cslTokenList, cslLexErrors, cslLexWarnings] = CslLexerMain(*cslInputStream, false);
                    auto [schemas, cslParseErrors, cslParseWarnings] = CslRdParserMain(cslTokenList);
                    auto [cslValidationErrors, cslValidationWarnings] = CslValidatorMain("BuildConfig", schemas, docTree);
                    errors.insert(errors.end(), cslLexErrors.begin(), cslLexErrors.end());
                    errors.insert(errors.end(), cslParseErrors.begin(), cslParseErrors.end());
                    errors.insert(errors.end(), cslValidationErrors.begin(), cslValidationErrors.end());
                    warnings.insert(warnings.end(), cslLexWarnings.begin(), cslLexWarnings.end());
                    warnings.insert(warnings.end(), cslParseWarnings.begin(), cslParseWarnings.end());
                    warnings.insert(warnings.end(), cslValidationWarnings.begin(), cslValidationWarnings.end());
                }
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
        else {
            printInfo(std::cerr);
            std::cerr << "file " << inputPath << "is not valid\n";
            return 1;
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
