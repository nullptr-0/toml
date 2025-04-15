#pragma once

#ifndef TOKEN_H
#define TOKEN_H

#include <string>
#include <list>
#include <stdexcept>
#include <tuple>
#include "Type.h"
#include "TypeUtils.h"

namespace Token
{
    struct TokenPropertyDeallocator {
        void operator()(Type::Type* type) {
            Type::DeleteType(type);
        }
    };

    template <typename TokenPropertyType = Type::Type, typename TokenPropertyDeallocator = TokenPropertyDeallocator>
    class TokenList {
    public:
        // content, tokenType, contentType, startLine, startCol, endLine, endCol
        using Token = std::tuple<std::string, std::string, TokenPropertyType*, size_t, size_t, size_t, size_t>;

        TokenList() : curTokenProp(nullptr), tokenBuffered(false), curTokenStartLine(0), curTokenStartCol(0), curTokenEndLine(0), curTokenEndCol(0) {}
        virtual ~TokenList() {}

        // Add the specified token to list.
        // Note that this operation will cause the current
        // buffered token (if available) to be flushed to
        // the list first and then the buffered content and
        // information to be cleared.
        void AddTokenToList(std::string tokenContent, std::string tokenType,
            TokenPropertyType* contentType = nullptr,
            size_t startLine = 0, size_t startCol = 0,
            size_t endLine = 0, size_t endCol = 0) {
            FlushBuffer();
            tokenList.push_back(std::make_tuple(tokenContent, tokenType,
                contentType, startLine, startCol, endLine, endCol));
        }

        // Set information for the current buffered token.
        // **REMINDER**:
        // The old curTokenProp instance MUST be properly
        // handled before being overwritten by a new one
        // (curTokenProp is assigned to a new value before
        // the old one is used in the token list) to avoid
        // memory leakage.
        void SetTokenInfo(std::string tokenType,
            TokenPropertyType* contentType = nullptr,
            size_t startLine = 0, size_t startCol = 0,
            size_t endLine = 0, size_t endCol = 0) {
            curTokenType = tokenType;
            curTokenProp = contentType;
            curTokenStartLine = startLine;
            curTokenStartCol = startCol;
            curTokenEndLine = endLine;
            curTokenEndCol = endCol;
        }

        void AppendBufferedToken(const char newContent) {
            curTokenContent.append(1, newContent);
            if (!tokenBuffered) {
                if (!tokenList.empty()) {
                    auto lastToken = *std::prev(tokenList.end());
                    curTokenStartLine = std::get<5>(lastToken);
                    curTokenStartCol = std::get<6>(lastToken);
                    curTokenEndLine = curTokenStartLine;
                    curTokenEndCol = curTokenStartCol;
                }
                else {
                    curTokenStartLine = 0;
                    curTokenStartCol = 0;
                    curTokenEndLine = 0;
                    curTokenEndCol = 0;
                }
            }
            if (newContent != '\n') {
                ++curTokenEndCol;
            }
            else {
                ++curTokenEndLine;
                curTokenEndCol = 0;
            }
            tokenBuffered = true;
        }

        bool IsTokenBuffered() const {
            return tokenBuffered;
        }

        void FlushBuffer() {
            if (!curTokenContent.empty()) {
                tokenList.push_back(std::make_tuple(
                    curTokenContent, curTokenType, curTokenProp,
                    curTokenStartLine, curTokenStartCol,
                    curTokenEndLine, curTokenEndCol));
                curTokenContent = "";
                curTokenType = "";
                curTokenProp = nullptr;
                curTokenStartLine = 0;
                curTokenStartCol = 0;
                curTokenEndLine = 0;
                curTokenEndCol = 0;
                tokenBuffered = false;
            }
        }

        std::list<Token>& GetTokenList() {
            return tokenList;
        }

        // Iterator for the elements
        using iterator = typename std::list<Token>::iterator;

        iterator begin() {
            return tokenList.begin();
        }

        iterator end() {
            return tokenList.end();
        }

        // Const iterator for the elements
        using const_iterator = typename std::list<Token>::const_iterator;

        const_iterator begin() const {
            return tokenList.begin();
        }

        const_iterator end() const {
            return tokenList.end();
        }

        void clear() {
            FlushBuffer();
            for (auto& token : tokenList) {
                TokenPropertyDeallocator()(std::get<2>(token));
            }
            tokenList.clear();
        }

        iterator insert(const_iterator pos, const Token& token) {
            return tokenList.insert(pos, token);
        }

        iterator erase(const_iterator pos) {
            auto token = *pos;
            TokenPropertyDeallocator()(std::get<2>(token));
            return tokenList.erase(pos);
        }

    protected:
        std::string curTokenContent;
        std::string curTokenType;
        TokenPropertyType* curTokenProp;
        size_t curTokenStartLine;
        size_t curTokenStartCol;
        size_t curTokenEndLine;
        size_t curTokenEndCol;
        bool tokenBuffered;
        std::list<Token> tokenList;
    };
};

#endif
