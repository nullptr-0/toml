#pragma once

#ifndef CSL_OPERATORS_H
#define CSL_OPERATORS_H

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace CSLOperator {
    struct OperatorKey {
        std::string operatorText;
        size_t numOperandBeforeOperator;

        bool operator==(const OperatorKey& other) const {
            return (operatorText == other.operatorText
                && numOperandBeforeOperator == other.numOperandBeforeOperator);
        }
    };

    struct OperatorProp {
        std::string pairedOperator;
        std::string operationName;
        size_t numOperand = 0;
        size_t precedence = 0;
        short associativity = 0;
    };

    struct OperatorKeyHasher
    {
        std::size_t operator()(const OperatorKey& k) const {
            using std::size_t;
            using std::hash;
            using std::string;

            return (hash<string>()(k.operatorText)
                ^ (hash<size_t>()(k.numOperandBeforeOperator) << 1));
        }
    };

    class Operator : public std::unordered_map<OperatorKey, OperatorProp, OperatorKeyHasher> {
    public:
        Operator(std::unordered_map<OperatorKey, OperatorProp, OperatorKeyHasher>&& ops) : lowestPriority(0), std::unordered_map<OperatorKey, OperatorProp, OperatorKeyHasher>(ops) {
            for (auto& op : ops) {
                if (op.second.precedence > lowestPriority) {
                    lowestPriority = op.second.precedence;
                }
            }
        }

        size_t GetLowestPriority() const {
            return lowestPriority;
        }

    protected:
        size_t lowestPriority;
    };

#ifndef DEF_GLOBAL
    extern Operator operators;
#else
    Operator operators({
        {{".", 1}, {"", "Member", 2, 1, 0}},
        {{"@", 1}, {"", "Annotation", 2, 1, 0}},
        {{"[", 1}, {"]", "Subscript", 2, 2, 0}},
        {{"]", 0}, {"", "", 0, 17, 0}},
        {{"(", 1}, {")", "FunctionCall", 2, 2, 0}},
        {{")", 0}, {"", "", 0, 17, 0}},
        {{"~", 0}, {"", "Complement", 1, 3, 1}},
        {{"!", 0}, {"", "LogicalNot", 1, 3, 1}},
        {{"+", 0}, {"", "UnaryPlus", 1, 3, 1}},
        {{"-", 0}, {"", "UnaryNegation", 1, 3, 1}},
        {{"*", 1}, {"", "Multiplication", 2, 5, 0}},
        {{"/", 1}, {"", "Division", 2, 5, 0}},
        {{"%", 1}, {"", "Modulus", 2, 5, 0}},
        {{"+", 1}, {"", "Addition", 2, 6, 0}},
        {{"-", 1}, {"", "Subtraction", 2, 6, 0}},
        {{"<<", 1}, {"", "LeftShift", 2, 7, 0}},
        {{">>", 1}, {"", "RightShift", 2, 7, 0}},
        {{"<", 1}, {"", "LessThan", 2, 8, 0}},
        {{">", 1}, {"", "GreaterThan", 2, 8, 0}},
        {{"<=", 1}, {"", "LessThanOrEqualTo", 2, 8, 0}},
        {{">=", 1}, {"", "GreaterThanOrEqualTo", 2, 8, 0}},
        {{"==", 1}, {"", "Equality", 2, 9, 0}},
        {{"!=", 1}, {"", "Inequality", 2, 9, 0}},
        {{"&", 1}, {"", "BitwiseAnd", 2, 10, 0}},
        {{"^", 1}, {"", "BitwiseExclusiveOr", 2, 11, 0}},
        {{"|", 1}, {"", "BitwiseInclusiveOr", 2, 12, 0}},
        {{"&&", 1}, {"", "LogicalAnd", 2, 13, 0}},
        {{"||", 1}, {"", "LogicalOr", 2, 14, 0}},
        {{"?", 1}, {":", "Conditional", 3, 15, 1}},
        {{":", 0}, {"", "", 0, 17, 0}},
        {{"=", 1}, {"", "Assignment", 2, 15, 1}},
        });
#endif
};

#endif
