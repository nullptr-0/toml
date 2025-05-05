#pragma once

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <iostream>
#include <tuple>
#include <vector>
#include <functional>
#include "../shared/Token.h"
#include "../shared/DocumentTree.h"
#include "../shared/FilePosition.h"
#include "../shared/CslRepresentation.h"

extern std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> TomlLexerMain(std::istream& inputCode, bool multilineToken = true);
using TomlLexerFunction = std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(std::istream&, bool)>;
using TomlLexerFunctionWithStringInput = std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>;
extern std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>> TomlRdparserMain(Token::TokenList<>& tokenList);
using TomlParserFunction = std::function<std::tuple<DocTree::Table*, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::unordered_map<size_t, DocTree::Key*>>(Token::TokenList<>&)>;

extern std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslLexerMain(std::istream& inputCode, bool multilineToken = true);
using CslLexerFunction = std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(std::istream&, bool)>;
using CslLexerFunctionWithStringInput = std::function<std::tuple<Token::TokenList<>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(const std::string&, bool)>;
extern std::tuple<std::vector<std::shared_ptr<CSL::ConfigSchema>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslRdParserMain(Token::TokenList<>& tokenList);
using CslParserFunction = std::function<std::tuple<std::vector<std::shared_ptr<CSL::ConfigSchema>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(Token::TokenList<>&)>;
extern std::tuple<std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslValidatorMain(std::string schemaName, std::vector<std::shared_ptr<CSL::ConfigSchema>> schemas, DocTree::Table* docTree);
using CslValidatorFunction = std::function<std::tuple<std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>>(std::string, std::vector<std::shared_ptr<CSL::ConfigSchema>>, DocTree::Table*)>;

extern int TomlLangSvrMain(std::istream& inChannel, std::ostream& outChannel, const TomlLexerFunctionWithStringInput& tomlLexer, const TomlParserFunction& tomlParser, const CslLexerFunctionWithStringInput& cslLexer, const CslParserFunction& cslParser, const CslValidatorFunction& cslValidator);

#endif
