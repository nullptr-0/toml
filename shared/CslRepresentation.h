#pragma once

#ifndef CSL_IMR_H
#define CSL_IMR_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include "Type.h"
#include "FilePosition.h"

namespace CSL {

    // Forward declarations
    class Expr;
    class Constraint;
    class Annotation;

    // -------------------- Type System --------------------
    class CSLType {
    public:
        enum class Kind {
            Primitive,
            Table,
            Array,
            Union,
            AnyTable,
            AnyArray,
            Invalid
        };

        CSLType(Kind kind, FilePosition::Region region)
            : kind(kind), region(std::move(region)) {
        }
        virtual ~CSLType() = default;

        const Kind getKind() const { return kind; }
        const FilePosition::Region& getRegion() const { return region; }

    private:
        Kind kind;
        FilePosition::Region region;
    };

    class PrimitiveType : public CSLType {
    public:
        enum class Primitive { String, Number, Boolean, Datetime, Duration };
        PrimitiveType(Primitive type, std::vector<std::pair<std::string, Type::Type*>> allowedValues, std::vector<std::shared_ptr<Annotation>> annotations, FilePosition::Region region)
            : CSLType(Kind::Primitive, region), primitiveType(type), allowedValues(std::move(allowedValues)), annotations(std::move(annotations)) {
        }
        const Primitive getPrimitive() const { return primitiveType; }
        const std::vector<std::pair<std::string, Type::Type*>>& getAllowedValues() const { return allowedValues; }
        const std::vector<std::shared_ptr<Annotation>>& getAnnotations() const { return annotations; }

    private:
        Primitive primitiveType;
        std::vector<std::pair<std::string, Type::Type*>> allowedValues;
        std::vector<std::shared_ptr<Annotation>> annotations;
    };

    class TableType : public CSLType {
    public:
        struct KeyDefinition {
            std::string name;
            bool isWildcard;
            bool isOptional;
            std::shared_ptr<CSLType> type;
            std::vector<std::shared_ptr<Annotation>> annotations;
            std::optional<std::pair<std::string, Type::Type*>> defaultValue;
        };

        TableType(
            std::vector<KeyDefinition> explicitKeys,
            std::shared_ptr<KeyDefinition> wildcardKey, // null if no wildcard
            std::vector<std::shared_ptr<Constraint>> constraints,
            FilePosition::Region region
        ) : CSLType(Kind::Table, region),
            explicitKeys(std::move(explicitKeys)),
            wildcardKey(std::move(wildcardKey)),
            constraints(std::move(constraints)) {
        }

        const std::vector<KeyDefinition>& getExplicitKeys() const { return explicitKeys; }
        const std::shared_ptr<KeyDefinition>& getWildcardKey() const { return wildcardKey; }
        const std::vector<std::shared_ptr<Constraint>>& getConstraints() const { return constraints; }

    private:
        std::vector<KeyDefinition> explicitKeys;
        std::shared_ptr<KeyDefinition> wildcardKey;
        std::vector<std::shared_ptr<Constraint>> constraints;
    };

    class ArrayType : public CSLType {
    public:
        ArrayType(std::shared_ptr<CSLType> elementType, FilePosition::Region region)
            : CSLType(Kind::Array, region), elementType(std::move(elementType)) {
        }
        const std::shared_ptr<CSLType>& getElementType() const { return elementType; }

    private:
        std::shared_ptr<CSLType> elementType;
    };

    class UnionType : public CSLType {
    public:
        UnionType(std::vector<std::shared_ptr<CSLType>> memberTypes, FilePosition::Region region)
            : CSLType(Kind::Union, region), memberTypes(std::move(memberTypes)) {
        }
        const std::vector<std::shared_ptr<CSLType>>& getMemberTypes() const { return memberTypes; }

    private:
        std::vector<std::shared_ptr<CSLType>> memberTypes; // types and their respective allowed values
    };

    class AnyTableType : public CSLType {
    public:
        AnyTableType(FilePosition::Region region)
            : CSLType(Kind::AnyTable, region) {
        }
    };

    class AnyArrayType : public CSLType {
    public:
        AnyArrayType(FilePosition::Region region)
            : CSLType(Kind::AnyArray, region) {
        }
    };

    // -------------------- Annotations --------------------
    class Annotation {
    public:
        Annotation(std::string name, std::vector<std::shared_ptr<Expr>> args, FilePosition::Region region)
            : name(std::move(name)), args(std::move(args)), region(std::move(region)) {
        }

        const std::string& getName() const { return name; }
        const std::vector<std::shared_ptr<Expr>>& getArgs() const { return args; }
        const FilePosition::Region& getRegion() const { return region; }

    private:
        std::string name;
        std::vector<std::shared_ptr<Expr>> args;
        FilePosition::Region region;
    };

    // -------------------- Constraints --------------------
    class Constraint {
    public:
        enum class Kind {
            Conflict,
            Dependency,
            Validate
        };

        Constraint(Kind kind, FilePosition::Region region) : kind(kind), region(std::move(region)) {}
        virtual ~Constraint() = default;

        const Kind getKind() const { return kind; }
        const FilePosition::Region& getRegion() const { return region; }

    private:
        Kind kind;
        FilePosition::Region region;
    };

    class ConflictConstraint : public Constraint {
    public:
        ConflictConstraint(
            std::shared_ptr<Expr> firstExpr,
            std::shared_ptr<Expr> secondExpr,
            FilePosition::Region region
        ) : Constraint(Kind::Conflict, region),
            firstExpr(std::move(firstExpr)),
            secondExpr(std::move(secondExpr)) {
        }

        const std::shared_ptr<Expr>& getFirstExpr() const { return firstExpr; }
        const std::shared_ptr<Expr>& getSecondExpr() const { return secondExpr; }

    private:
        std::shared_ptr<Expr> firstExpr;
        std::shared_ptr<Expr> secondExpr;
    };

    class DependencyConstraint : public Constraint {
    public:
        DependencyConstraint(
            std::shared_ptr<Expr> dependentExpr,
            std::shared_ptr<Expr> condition,
            FilePosition::Region region
        ) : Constraint(Kind::Dependency, region),
            dependentExpr(std::move(dependentExpr)),
            condition(std::move(condition)) {
        }

        const std::shared_ptr<Expr>& getDependentExpr() const { return dependentExpr; }
        const std::shared_ptr<Expr>& getCondition() const { return condition; }

    private:
        std::shared_ptr<Expr> dependentExpr;
        std::shared_ptr<Expr> condition;
    };

    class ValidateConstraint : public Constraint {
    public:
        ValidateConstraint(std::shared_ptr<Expr> expr, FilePosition::Region region)
            : Constraint(Kind::Validate, region), expr(std::move(expr)) {
        }

        const std::shared_ptr<Expr>& getExpr() const { return expr; }

    private:
        std::shared_ptr<Expr> expr;
    };

    // -------------------- Expressions --------------------
    class Expr {
    public:
        enum class Kind {
            BinaryOp,
            UnaryOp,
            TernaryOp,
            Literal,
            Identifier,
            FunctionArg,
            FunctionCall,
            Annotation
        };

        Expr(Kind kind, FilePosition::Region region) : kind(kind), region(std::move(region)) {}
        virtual ~Expr() = default;

        const Kind getKind() const { return kind; }
        const FilePosition::Region& getRegion() const { return region; }

    private:
        Kind kind;
        FilePosition::Region region;
    };

    class BinaryExpr : public Expr {
    public:
        BinaryExpr(
            std::string op,
            std::shared_ptr<Expr> lhs,
            std::shared_ptr<Expr> rhs,
            FilePosition::Region region
        ) : Expr(Kind::BinaryOp, region),
            op(std::move(op)), lhs(std::move(lhs)), rhs(std::move(rhs)) {
        }

        const std::string& getOp() const { return op; }
        const std::shared_ptr<Expr>& getLHS() const { return lhs; }
        const std::shared_ptr<Expr>& getRHS() const { return rhs; }

    private:
        std::string op;
        std::shared_ptr<Expr> lhs;
        std::shared_ptr<Expr> rhs;
    };

    class UnaryExpr : public Expr {
    public:
        UnaryExpr(
            std::string op,
            std::shared_ptr<Expr> operand,
            FilePosition::Region region
        ) : Expr(Kind::UnaryOp, region),
            op(std::move(op)), operand(std::move(operand)) {
        }

        const std::string& getOp() const { return op; }
        const std::shared_ptr<Expr>& getOperand() const { return operand; }
    private:
        std::string op;
        std::shared_ptr<Expr> operand;
    };

    class TernaryExpr : public Expr {
        public:
        TernaryExpr(
            std::shared_ptr<Expr> condition,
            std::shared_ptr<Expr> trueExpr,
            std::shared_ptr<Expr> falseExpr,
            FilePosition::Region region
        ) : Expr(Kind::TernaryOp, region),
            condition(std::move(condition)),
            trueExpr(std::move(trueExpr)),
            falseExpr(std::move(falseExpr)) {
        }
        const std::shared_ptr<Expr>& getCondition() const { return condition; }
        const std::shared_ptr<Expr>& getTrueExpr() const { return trueExpr; }
        const std::shared_ptr<Expr>& getFalseExpr() const { return falseExpr; }
    private:
        std::shared_ptr<Expr> condition;
        std::shared_ptr<Expr> trueExpr;
        std::shared_ptr<Expr> falseExpr;
    };

    class LiteralExpr : public Expr {
    public:
        LiteralExpr(Type::Type* type, std::string value, FilePosition::Region region)
            : Expr(Kind::Literal, region), type(type), value(std::move(value)) {
        }

        const Type::Type* getType() const { return type; }
        const std::string& getValue() const { return value; }

    private:
        Type::Type* type;
        std::string value;
    };

    class IdentifierExpr : public Expr {
    public:
        IdentifierExpr(std::string name, FilePosition::Region region)
            : Expr(Kind::Identifier, region), name(std::move(name)) {
        }

        const std::string& getName() const { return name; }

    private:
        std::string name;
    };

    class FunctionArgExpr : public Expr {
    public:
        using Value = std::variant<std::shared_ptr<Expr>, std::vector<std::shared_ptr<Expr>>>;

        FunctionArgExpr(Value value, FilePosition::Region region)
            : Expr(Kind::Identifier, region), value(std::move(value)) {
        }

        const Value& getValue() const { return value; }

    private:
        Value value;
    };

    class FunctionCallExpr : public Expr {
    public:
        FunctionCallExpr(
            std::string funcName,
            std::vector<std::shared_ptr<Expr>> args,
            FilePosition::Region region
        ) : Expr(Kind::FunctionCall, region),
            funcName(std::move(funcName)),
            args(std::move(args)) {
        }

        const std::string& getFuncName() const { return funcName; }
        const std::vector<std::shared_ptr<Expr>>& getArgs() const { return args; }

    private:
        std::string funcName;
        std::vector<std::shared_ptr<Expr>> args;
    };

    class AnnotationExpr : public Expr {
    public:
        AnnotationExpr(
            std::shared_ptr<Expr> target,
            std::shared_ptr<Annotation> annotation,
            FilePosition::Region region
        ) : Expr(Kind::Annotation, region),
            target(std::move(target)),
            annotation(std::move(annotation)) {
        }

        const std::shared_ptr<Expr>& getTarget() const { return target; }
        const std::shared_ptr<Annotation>& getAnnotation() const { return annotation; }

    private:
        std::shared_ptr<Expr> target;
        std::shared_ptr<Annotation> annotation;
    };

    // -------------------- Schema Root --------------------
    class ConfigSchema {
    public:
        ConfigSchema(
            std::string name,
            std::shared_ptr<TableType> rootTable,
            FilePosition::Region region
        ) : name(std::move(name)),
            rootTable(std::move(rootTable)),
            region(std::move(region)) {
        }

        const std::string& getName() const { return name; }
        const std::shared_ptr<TableType>& getRootTable() const { return rootTable; }
        const FilePosition::Region& getRegion() const { return region; }

    private:
        std::string name;
        std::shared_ptr<TableType> rootTable;
        FilePosition::Region region;
    };

} // namespace CSL

#endif
