#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include "Components.h"
#include "../shared/Dump.h"
#include "../shared/DocTree2Json.h"

//#define DEBUG

int main(int argc, char* argv[])
{
    std::vector<std::string> argVector(argv, argv + argc);
    auto printInfo = [](std::ostream& stream) {
        stream << "TOML Test" << std::endl;
        stream << "Built at: " __TIME__ " " __DATE__ << std::endl;
        stream << "Copyright (C) 2023-2025 nullptr-0." << std::endl;
        stream << "Open-source Projects:" << std::endl;
        stream << "json: MIT License https://github.com/nlohmann/json/blob/master/LICENSE.MIT" << std::endl;
        stream << "regex: Boost Software License http://www.boost.org/LICENSE_1_0.txt" << std::endl;
    };
    auto printHelp = [&argVector](std::ostream& stream) {
        stream << "Usage:\n"
            << argVector[0] << " --parse[ path]\n"
            << argVector[0] << " --help\n"
            << argVector[0] << " -h\n";
    };
    if ((argc == 2 || argc == 3) && argVector[1] == "--parse") {
        int retVal = 0;
        const auto inputPath = argc == 2 ? "" : argVector[2];
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
        std::istream* inputStream;
		if (inputPath.empty() || inputPath == "-") {
			inputStream = &std::cin;
		}
		else if (std::filesystem::is_regular_file(inputPath)) {
            inputStream = getStreamForDiskFile(inputPath, std::ios::in);
        }
        else {
            std::cerr << "input path is not a file" << std::endl;
            return 1;
        }
#ifndef DEBUG
        try {
#endif // DEBUG
            std::vector<std::tuple<std::string, FilePosition::Region>> errors;
            std::vector<std::tuple<std::string, FilePosition::Region>> warnings;
            auto [tokenList, lexErrors, lexWarnings] = TomlLexerMain(*inputStream, true);
            auto [docTree, parseErrors, parseWarnings, tokenDocTreeMapping] = TomlRdparserMain(tokenList);
            errors.insert(errors.end(), lexErrors.begin(), lexErrors.end());
            errors.insert(errors.end(), parseErrors.begin(), parseErrors.end());
            warnings.insert(warnings.end(), lexWarnings.begin(), lexWarnings.end());
            warnings.insert(warnings.end(), parseWarnings.begin(), parseWarnings.end());

#ifdef DEBUG
            Dump::DumpDocumentTree(docTree);
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
#endif // DEBUG

            std::cout << DocTree::toJson(docTree, true) << std::endl;

            delete docTree;
            retVal = errors.size() ? 1 : 0;
#ifndef DEBUG
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            return 1;
        }
#endif // DEBUG
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
        std::cerr << "invalid arguments" << std::endl;
        printHelp(std::cerr);
        return 2;
    }
    return 0;
}
