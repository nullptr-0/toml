#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <unordered_set>
#include <stdexcept>
#include "../shared/Type.h"
#include "../shared/TypeUtils.h"
#include "../shared/Token.h"
#include "../shared/CSLRepresentation.h"
#include "../shared/CslOperators.h"

namespace CSLParser {

    using namespace CSL;

    class RecursiveDescentParser {
    protected:
        Token::TokenList<>& input;
        Token::TokenList<>::iterator position;
        std::vector<std::tuple<std::string, FilePosition::Region>> errors;
        std::vector<std::tuple<std::string, FilePosition::Region>> warnings;

        void advance() {
            ++position;
        }

        void expect(std::string token, const std::string& msg) {
            auto [content, tokenType, prop, region] = *position;
            if (content != token) {
                errors.push_back({ msg + ". Found: " + content, region });
            }
        }

        void expect(const std::vector<std::pair<std::string , std::string>>& tokenMsgPairs) {
            for (const auto& tokenMsgPair : tokenMsgPairs) {
                auto [token, msg] = tokenMsgPair;
                expect(token, msg);
            }
        }

        void expectType(std::string type, const std::string& msg) {
            auto [content, tokenType, prop, region] = *position;
            if (tokenType != type) {
                errors.push_back({ msg + ". Found: " + content, region });
            }
        }

    public:
        RecursiveDescentParser(Token::TokenList<>& input) : input(input), position(input.begin()) {}

        // Entry point: Parse multiple schemas
        std::vector<std::shared_ptr<ConfigSchema>> parseSchemas() {
            std::vector<std::shared_ptr<ConfigSchema>> schemas;
            while (position != input.end()) {
                if (std::get<0>(*position) == "config") {
                    schemas.push_back(parseConfigSchema());
                }
                else {
                    advance(); // Skip unknown tokens
                }
            }
            return schemas;
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> getErrors() const {
            return errors;
        }

        std::vector<std::tuple<std::string, FilePosition::Region>> getWarnings() const {
            return warnings;
        }

    protected:
        // Parse a single config schema
        std::shared_ptr<ConfigSchema> parseConfigSchema() {
            advance(); // Consume 'config'
            expectType("identifier", "Expected schema name after 'config'");
            std::string name = std::get<0>(*position);
            advance();
            return std::make_shared<ConfigSchema>(name, parseTableType(), std::get<3>(*position));
        }

        // Parse a table type { ... }
        std::shared_ptr<TableType> parseTableType() {
            expect("{", "Expected '{' after schema name");
            advance();

            auto tableStart = std::get<3>(*position).start;
            std::vector<TableType::KeyDefinition> explicitKeys;
            std::shared_ptr<TableType::KeyDefinition> wildcardKey = nullptr;
            std::vector<std::shared_ptr<Constraint>> constraints;

            while (std::get<0>(*position) != "}") {
                if (std::get<0>(*position) == "constraints") {
                    constraints = parseConstraints();
                }
                else {
                    if (std::get<0>(*position) == "*") {
                        wildcardKey = parseWildcardKey();
                    }
                    else {
                        explicitKeys.push_back(parseKeyDefinition());
                    }
                }
            }

            expect("}", "Expected '{' after schema name");
            auto tableEnd = std::get<3>(*position).end;
            advance(); // Consume '}'

            return std::make_shared<TableType>(explicitKeys, wildcardKey, constraints, FilePosition::Region{ tableStart, tableEnd });
        }

        // Parse a key definition (name: type;)
        TableType::KeyDefinition parseKeyDefinition() {
            auto name = std::get<0>(*position);
            bool isOptional = false;
            advance();

            if (std::get<0>(*position) == "?") {
                isOptional = true;
                advance();
            }

            std::shared_ptr<CSLType> type;
            std::optional<std::pair<std::string, Type::Type*>> defaultValue = std::nullopt;
            std::vector<std::shared_ptr<Annotation>> annotations;
            if (std::get<0>(*position) == ":" || std::get<0>(*position) == "=") {
                advance();
                if (std::get<0>(*position) == "=") {
                    defaultValue = std::make_pair(std::get<0>(*position), std::get<2>(*position));
                }
                type = parseType();
                annotations = parseAnnotations(true);
            }
            else {
                expect({ { ":", "Expected ':' after key name" }, { "=", "Expected '=' after key name" } });
                advance();
            }

            expect(";", "Expected ';' after key definition");
            advance();

            return TableType::KeyDefinition{ name, false, isOptional, type, annotations, defaultValue };
        }

        // Parse wildcard key (*: type;)
        std::shared_ptr<TableType::KeyDefinition> parseWildcardKey() {
            advance(); // Consume '*'
            expect(":", "Expected ':' after wildcard");
            advance();

            auto type = parseType();
            std::vector<std::shared_ptr<Annotation>> annotations = parseAnnotations(true);

            expect(";", "Expected ';' after wildcard key");
            advance();

            return std::make_shared<TableType::KeyDefinition>(
                TableType::KeyDefinition{ "*", true, false, type, annotations, std::nullopt }
            );
        }

        // Parse type (primitive, enum, table, array, etc.)
        std::shared_ptr<CSLType> parseType() {
            auto typeStart = std::get<3>(*position).start;
            auto type = parsePostfixType();
            while (std::get<0>(*position) == "|") {
                advance(); // Consume '|'
                auto rightType = parsePostfixType();

                std::vector<std::shared_ptr<CSLType>> members;
                if (type->getKind() == CSLType::Kind::Union) {
                    auto unionType = std::static_pointer_cast<UnionType>(type);
                    members = unionType->getMemberTypes();
                }
                else {
                    members.push_back(type);
                }

                if (rightType->getKind() == CSLType::Kind::Union) {
                    auto rightUnion = std::static_pointer_cast<UnionType>(rightType);
                    members.insert(members.end(), rightUnion->getMemberTypes().begin(), rightUnion->getMemberTypes().end());
                }
                else {
                    members.push_back(rightType);
                }

                type = std::make_shared<UnionType>(members, FilePosition::Region{ typeStart, std::get<3>(*std::prev(position)).end });
            }
            return type;
        }

        std::shared_ptr<CSLType> parsePostfixType() {
            auto type = parsePrimaryType();
            while (std::get<0>(*position) == "[") {
                auto typeStart = std::get<3>(*position).start;
                advance(); // Consume '['
                expect("]", "Expected ']' after array type");
                auto typeEnd = std::get<3>(*position).end;
                advance(); // Consume ']'
                type = std::make_shared<ArrayType>(type, FilePosition::Region{ typeStart, typeEnd });
            }
            return type;
        }

        std::shared_ptr<CSLType> parsePrimaryType() {
            std::vector<std::shared_ptr<CSLType>> members;
            auto typeStart = std::get<3>(*position).start;

            do {
                if (std::get<1>(*position) == "number" ||
                    std::get<1>(*position) == "boolean" ||
                    std::get<1>(*position) == "string" ||
                    std::get<1>(*position) == "datetime") {
                    members.push_back(parseLiteralType());
                }
                else if (std::get<0>(*position) == "string") {
                    auto defRegion = std::get<3>(*position);
                    advance();
                    std::vector<std::shared_ptr<Annotation>> annotations;
                    if (position != input.end()) {
                        annotations = parseAnnotations(false);
                    }
                    members.push_back(std::make_shared<PrimitiveType>(
                        PrimitiveType::Primitive::String, std::vector<std::pair<std::string, Type::Type*>>{}, annotations, defRegion
                    ));
                }
                else if (std::get<0>(*position) == "number") {
                    auto defRegion = std::get<3>(*position);
                    advance();
                    std::vector<std::shared_ptr<Annotation>> annotations;
                    if (position != input.end()) {
                        annotations = parseAnnotations(false);
                    }
                    members.push_back(std::make_shared<PrimitiveType>(
                        PrimitiveType::Primitive::Number, std::vector<std::pair<std::string, Type::Type*>>{}, annotations, defRegion
                    ));
                }
                else if (std::get<0>(*position) == "boolean") {
                    auto defRegion = std::get<3>(*position);
                    advance();
                    std::vector<std::shared_ptr<Annotation>> annotations;
                    if (position != input.end()) {
                        annotations = parseAnnotations(false);
                    }
                    members.push_back(std::make_shared<PrimitiveType>(
                        PrimitiveType::Primitive::Boolean, std::vector<std::pair<std::string, Type::Type*>>{}, annotations, defRegion
                    ));
                }
                else if (std::get<0>(*position) == "datetime") {
                    auto defRegion = std::get<3>(*position);
                    advance();
                    std::vector<std::shared_ptr<Annotation>> annotations;
                    if (position != input.end()) {
                        annotations = parseAnnotations(false);
                    }
                    members.push_back(std::make_shared<PrimitiveType>(
                        PrimitiveType::Primitive::Datetime, std::vector<std::pair<std::string, Type::Type*>>{}, annotations, defRegion
                    ));
                }
                else if (std::get<0>(*position) == "any{}") {
                    members.push_back(std::make_shared<AnyTableType>(std::get<3>(*position)));
                    advance();
                }
                else if (std::get<0>(*position) == "any[]") {
                    members.push_back(std::make_shared<AnyArrayType>(std::get<3>(*position)));
                    advance();
                }
                else if (std::get<0>(*position) == "{") {
                    members.push_back(parseTableType());
                }
                else if (std::get<0>(*position) == "(") {
                    advance(); // Consume '('
                    members.push_back(parseType());
                    expect(")", "Expected ')' after parenthesized type");
                    advance();
                }
                else {
                    errors.push_back({ "Unexpected token in type: " + std::get<0>(*position), std::get<3>(*position) });
                }

                // Check for union operator
                if (position != input.end())  {
                    if (std::get<0>(*position) != "|") break;
                    advance(); // Consume '|'
                }

            } while (position != input.end());

            // Create appropriate type structure
            if (members.size() == 1) {
                return members[0];
            }
            return std::make_shared<UnionType>(members, FilePosition::Region{ typeStart, std::get<3>(*std::prev(position)).end });
        }

        std::shared_ptr<CSLType> parseLiteralType() {
            std::shared_ptr<CSLType> type;
            if (std::get<1>(*position) == "number") {
                type = std::make_shared<PrimitiveType>(
                    PrimitiveType::Primitive::Number, std::vector<std::pair<std::string, Type::Type*>>{ { std::get<0>(*position), std::get<2>(*position) } }, std::vector<std::shared_ptr<Annotation>>{}, std::get<3>(*position)
                );
                advance();
            }
            else if (std::get<1>(*position) == "boolean") {
                type = std::make_shared<PrimitiveType>(
                    PrimitiveType::Primitive::Boolean, std::vector<std::pair<std::string, Type::Type*>>{ { std::get<0>(*position), std::get<2>(*position) } }, std::vector<std::shared_ptr<Annotation>>{}, std::get<3>(*position)
                );
                advance();
            }
            else if (std::get<1>(*position) == "string") {
                type = std::make_shared<PrimitiveType>(
                    PrimitiveType::Primitive::String, std::vector<std::pair<std::string, Type::Type*>>{ { std::get<0>(*position), std::get<2>(*position) } }, std::vector<std::shared_ptr<Annotation>>{}, std::get<3>(*position)
                );
                advance();
            }
            else if (std::get<1>(*position) == "datetime") {
                type = std::make_shared<PrimitiveType>(
                    PrimitiveType::Primitive::Datetime, std::vector<std::pair<std::string, Type::Type*>>{ { std::get<0>(*position), std::get<2>(*position) } }, std::vector<std::shared_ptr<Annotation>>{}, std::get<3>(*position)
                );
                advance();
            }
            else {
                errors.push_back({ "Unexpected literal type: " + std::get<0>(*position), std::get<3>(*position) });
            }
            return type;
        }

        bool isGlobalAnnotation(const std::string& token) {
            return
                token == "deprecated";
        }

        // Parse annotations (@min, @regex, etc.)
        std::vector<std::shared_ptr<Annotation>> parseAnnotations(bool isParsingGlobalAnnotations) {
            std::vector<std::shared_ptr<Annotation>> annotations;
            while (std::get<0>(*position) == "@" && std::next(position) != input.end() && isGlobalAnnotation(std::get<0>(*std::next(position))) == isParsingGlobalAnnotations) {
                annotations.push_back(parseAnnotation(isParsingGlobalAnnotations));
            }
            return annotations;
        }

        // Parse constraints block
        std::vector<std::shared_ptr<Constraint>> parseConstraints() {
            std::vector<std::shared_ptr<Constraint>> constraints;
            advance(); // Consume 'constraints'
            expect("{", "Expected '{' after constraints");
            advance();

            while (std::get<0>(*position) != "}") {
                if (std::get<0>(*position) == "conflicts") {
                    constraints.push_back(parseConflictConstraint());
                }
                else if (std::get<0>(*position) == "requires") {
                    constraints.push_back(parseDependencyConstraint());
                }
                else if (std::get<0>(*position) == "validate") {
                    constraints.push_back(parseValidateConstraint());
                }
                else {
                    advance();
                }
            }

            advance(); // Consume '}'
            if (std::get<0>(*position) == ";") advance();
            return constraints;
        }

        // Parse conflict constraint (conflicts a with b;)
        std::shared_ptr<ConflictConstraint> parseConflictConstraint() {
            auto constraintStart = std::get<3>(*position).start;
            advance(); // Consume 'conflicts'
            auto firstExpr = parseExpression();
            expect("with", "Expected 'with' in conflict constraint");
            advance(); // Consume 'with'
            auto secondExpr = parseExpression();
            expect(";", "Expected ';' after conflict");
            auto constraintEnd = std::get<3>(*position).end;
            advance(); // Consume ';'
            return std::make_shared<ConflictConstraint>(firstExpr, secondExpr, FilePosition::Region{ constraintStart, constraintEnd });
        }

        // Parse dependency constraint (requires a => b;)
        std::shared_ptr<DependencyConstraint> parseDependencyConstraint() {
            auto constraintStart = std::get<3>(*position).start;
            advance(); // Consume 'requires'
            auto dependent = parseExpression();
            expect("=>", "Expected '=>' in dependency");
            advance();
            auto condition = parseExpression();
            expect(";", "Expected ';' after dependency");
            auto constraintEnd = std::get<3>(*position).end;
            advance();
            return std::make_shared<DependencyConstraint>(dependent, condition, FilePosition::Region{ constraintStart, constraintEnd });
        }

        // Parse validate constraint (validate expr;)
        std::shared_ptr<ValidateConstraint> parseValidateConstraint() {
            auto constraintStart = std::get<3>(*position).start;
            advance(); // Consume 'validate'
            auto expr = parseExpression();
            expect(";", "Expected ';' after validate");
            auto constraintEnd = std::get<3>(*position).end;
            advance();
            return std::make_shared<ValidateConstraint>(expr, FilePosition::Region{ constraintStart, constraintEnd });
        }

        // Parse expressions (recursive descent)
        std::shared_ptr<Expr> parseExpression(size_t minPrecedence = 17) {
            auto expressionStart = std::get<3>(*position).start;
            auto lhs = parseUnary();

            while (true) {
                auto opToken = std::get<0>(*position);
                CSLOperator::OperatorKey binKey{ opToken, 1 };
                auto opIt = CSLOperator::operators.find(binKey);
                if (opIt == CSLOperator::operators.end()) break;

                auto& op = opIt->second;
                if (op.precedence >= minPrecedence + op.associativity) break;

                if (opToken == "@") {
                    auto annotation = parseAnnotation(false);
                    lhs = std::make_shared<AnnotationExpr>(lhs, annotation, annotation.get()->getRegion() );
                }
                else {
                    advance();
                    auto rhs = parseExpression(op.precedence);
                    lhs = std::make_shared<BinaryExpr>(opToken, lhs, rhs, FilePosition::Region{ expressionStart, std::get<3>(*std::prev(position)).end });
                }
            }

            return lhs;
        }

        // Helper function to determine operator precedence
        size_t getPrecedence(const std::string& token, bool isBinary) {
            CSLOperator::OperatorKey key{ token, isBinary ? (size_t)1 : (size_t)0 };
            auto it = CSLOperator::operators.find(key);
            return (it != CSLOperator::operators.end()) ? it->second.precedence : 17;
        }

        std::shared_ptr<Expr> parseUnary() {
            auto expressionStart = std::get<3>(*position).start;
            CSLOperator::OperatorKey key{ std::get<0>(*position), 0 };
            auto it = CSLOperator::operators.find(key);

            if (it != CSLOperator::operators.end() && it->second.numOperand == 1) {
                auto op = it->second;
                advance();
                auto expr = parseExpression(op.precedence);
                return std::make_shared<UnaryExpr>(op.operationName, expr, FilePosition::Region{ expressionStart, std::get<3>(*std::prev(position)).end });
            }
            return parsePrimary();
        }

        std::shared_ptr<Expr> parsePrimary() {
            std::shared_ptr<Expr> expr;
            if (std::get<1>(*position) == "string") {
                expr = std::make_shared<LiteralExpr>(std::get<2>(*position), std::get<0>(*position), std::get<3>(*position));
                advance();
            }
            else if (std::get<1>(*position) == "number") {
                expr = std::make_shared<LiteralExpr>(std::get<2>(*position), std::get<0>(*position), std::get<3>(*position));
                advance();
            }
            else if (std::get<1>(*position) == "boolean") {
                expr = std::make_shared<LiteralExpr>(std::get<2>(*position), std::get<0>(*position), std::get<3>(*position));
                advance();
            }
            else if (std::get<1>(*position) == "datetime") {
                expr = std::make_shared<LiteralExpr>(std::get<2>(*position), std::get<0>(*position), std::get<3>(*position));
                advance();
            }
            else if (std::get<1>(*position) == "identifier") {
                expr = std::make_shared<IdentifierExpr>(std::get<0>(*position), std::get<3>(*position));
                advance();
            }
            else if (std::get<1>(*position) == "keyword") {
                auto functionCallStart = std::get<3>(*position).start;
                std::string name = std::get<0>(*position);
                advance();
                advance(); // Consume '('
                std::vector<std::shared_ptr<Expr>> args;
                while (std::get<0>(*position) != ")") {
                    std::shared_ptr<Expr> arg;
                    if (std::get<0>(*position) == "[") {
                        auto functionArgStart = std::get<3>(*position).start;
                        advance(); // Consume '['
                        std::vector<std::shared_ptr<Expr>> elems;
                        while (std::get<0>(*position) != "]") {
                            elems.push_back(parseExpression());
                            if (std::get<0>(*position) == ",") advance();
                        }
                        advance(); // Consume ']'
                        arg = std::make_shared<FunctionArgExpr>(elems, FilePosition::Region{ functionArgStart, std::get<3>(*std::prev(position)).end });
                    }
                    else {
                        auto functionArgStart = std::get<3>(*position).start;
                        arg = std::make_shared<FunctionArgExpr>(parseExpression(), FilePosition::Region{ functionArgStart, std::get<3>(*std::prev(position)).end });
                    }
                    args.push_back(arg);
                    if (std::get<0>(*position) == ",") advance();
                }
                advance(); // Consume ')'
                expr = std::make_shared<FunctionCallExpr>(name, args, FilePosition::Region{ functionCallStart, std::get<3>(*std::prev(position)).end });
            }
            else if (std::get<0>(*position) == "(") {
                advance(); // Consume '('
                expr = parseExpression();
                expect(")", "Expected ')' after expression");
                advance(); // Consume ')'
            }
            else {
                errors.push_back({ "Unexpected primary token: " + std::get<0>(*position), std::get<3>(*position) });
            }
            return expr;
        }

        std::shared_ptr<Annotation> parseAnnotation(bool isParsingGlobalAnnotation) {
            auto annotationStart = std::get<3>(*position).start;
            advance(); // Consume '@'
            std::string name = std::get<0>(*position);
            if (isParsingGlobalAnnotation) {
                if (!isGlobalAnnotation(name)) {
                    errors.push_back({ "Found local annotation " + name + " when parsing global annotations", std::get<3>(*position) });
                }
            }
            else {
                if (isGlobalAnnotation(name)) {
                    errors.push_back({ "Found global annotation " + name + " when parsing local annotations", std::get<3>(*position) });
                }
            }
            advance();
            std::vector<std::shared_ptr<Expr>> args;
            if (std::get<0>(*position) == "(") {
                advance(); // Consume '('
                while (std::get<0>(*position) != ")") {
                    args.push_back(parseExpression());
                    if (std::get<0>(*position) == ",") advance();
                }
                advance(); // Consume ')'
            }
            return std::make_shared<Annotation>(name, args, FilePosition::Region{ annotationStart, std::get<3>(*std::prev(position)).end });
        }
    };
}

std::tuple<std::vector<std::shared_ptr<CSL::ConfigSchema>>, std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslRdParserMain(Token::TokenList<>& tokenList) {
    CSLParser::RecursiveDescentParser rdparser(tokenList);
    return { rdparser.parseSchemas(), rdparser.getErrors(), rdparser.getWarnings() };
}
