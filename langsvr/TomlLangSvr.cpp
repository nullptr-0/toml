#include "TomlLanguageServer.h"

int TomlLangSvrMain(
    std::istream& inChannel,
    std::ostream& outChannel,
    const TomlLexerFunctionWithStringInput& tomlLexer,
    const TomlParserFunction& tomlParser,
    const CslLexerFunctionWithStringInput& cslLexer,
    const CslParserFunction& cslParser,
    const CslValidatorFunction& cslValidator
) {
    LanguageServer server(inChannel, outChannel, tomlLexer, tomlParser, cslLexer, cslParser, cslValidator);
    return server.run();
}