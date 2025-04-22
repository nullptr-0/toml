#pragma once

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <iostream>
#include <tuple>
#include <vector>
#include "../shared/Token.h"
#include "../shared/DocumentTree.h"
#include "../shared/FilePosition.h"

extern std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> TomlLexerMain(std::istream& inputCode, bool multilineToken = true);
extern std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>> TomlRdparserMain(Token::TokenList<>& tokenList);

extern int TomlLangSvrMain(std::istream& inChannel, std::ostream& outChannel, const std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>& lexer, const std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>>(Token::TokenList<>& tokenList)>& parser);

#endif
