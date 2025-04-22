#pragma once

#ifndef DOCUMENT_TREE_H
#define DOCUMENT_TREE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <any>
#include "Type.h"
#include "TypeUtils.h"
#include "FilePosition.h"

// get value from variable 'std::any value'
#define get_any_val(varRealType, varName) \
    if (value.type() == typeid(varRealType)) { \
        varName = std::any_cast<varRealType>(value); \
    } \
    else { \
        throw std::invalid_argument("Invalid value type"); \
    }

namespace DocTree
{
    class DocTreeNode {
    public:
        virtual ~DocTreeNode() {}
    };

    class Value : public DocTreeNode {
    public:
        Value(Type::Type* type, std::string value) : type(Type::CopyType(type)), value(value) {}

        ~Value() {
            if (type) {
                Type::DeleteType(type);
            }
        }

        std::tuple<Type::Type*, std::string> get() const {
            return std::make_tuple(type, value);
        }

        template <int N>
        void set(const std::any& value) {
            if constexpr (N == 0) {
                get_any_val(Type::Type*, this->type)
            }
            else if constexpr (N == 1) {
                get_any_val(std::string, this->value)
            }
            else {
                throw std::invalid_argument("Invalid setter index");
            }
        }

    protected:
        Type::Type* type;
        std::string value;
    };

    class Array : public DocTreeNode {
        using ValueArray = std::vector<DocTreeNode*>;

    public:
        Array(ValueArray elems, bool isMutable, FilePosition::Region defPos)
            : elems(elems), isMutable(isMutable), defPos(defPos) {}

        ~Array() {
            for (auto elem : elems) {
                if (elem) {
                    delete elem;
                }
            }
        }

        std::tuple<ValueArray, bool, FilePosition::Region> get() const {
            return std::make_tuple(elems, isMutable, defPos);
        }

        template <int N>
        void set(const std::any& value) {
            if constexpr (N == 0) {
                get_any_val(ValueArray, this->elems)
            }
            else if constexpr (N == 1) {
                get_any_val(bool, this->isMutable)
            }
            else if constexpr (N == 2) {
                get_any_val(FilePosition::Region, this->defPos)
            }
            else {
                throw std::invalid_argument("Invalid setter index");
            }
        }

        ValueArray getElems() const {
            return elems;
        }

        ValueArray& getElems() {
            return elems;
        }

        bool getIsMutable() const {
            return isMutable;
        }

        void seal() {
            isMutable = false;
        }

    protected:
        ValueArray elems;
        bool isMutable;
        FilePosition::Region defPos;
    };

    class Table;

    class Key : public DocTreeNode {
    public:
        Key(std::string id, DocTreeNode* value, Table* parentTable) : id(id), value(value), parentTable(parentTable) {}

        ~Key() {
            if (value) {
                delete value;
            }
        }

        std::tuple<std::string, DocTreeNode*, Table*> get() const {
            return std::make_tuple(id, value, parentTable);
        }

        template <int N>
        void set(const std::any& value) {
            if constexpr (N == 0) {
                get_any_val(std::string, this->id)
            }
            else if constexpr (N == 1) {
                get_any_val(DocTreeNode*, this->value)
            }
            else if constexpr (N == 2) {
                get_any_val(Table*, this->parentTable)
            }
            else {
                throw std::invalid_argument("Invalid setter index");
            }
        }

    protected:
        std::string id;
        DocTreeNode* value;
        Table* parentTable;
    };

    class Table : public DocTreeNode {
        using KeyTable = std::unordered_map<std::string, Key*>;

    public:
        Table(KeyTable elems, bool isMutable, FilePosition::Region defPos, bool isExplicitlyDefined)
            : elems(elems), isMutable(isMutable), defPos(defPos), isExplicitlyDefined(isExplicitlyDefined) {}

        ~Table() {
            for (auto& elem : elems) {
                if (elem.second) {
                    delete elem.second;
                }
            }
        }

        std::tuple<KeyTable, bool, FilePosition::Region, bool> get() const {
            return std::make_tuple(elems, isMutable, defPos, isExplicitlyDefined);
        }

        template <int N>
        void set(const std::any& value) {
            if constexpr (N == 0) {
                get_any_val(KeyTable, this->elems)
            }
            else if constexpr (N == 1) {
                get_any_val(bool, this->isMutable)
            }
            else if constexpr (N == 2) {
                get_any_val(FilePosition::Region, this->defPos)
            }
            else if constexpr (N == 3) {
                get_any_val(bool, this->isExplicitlyDefined)
            }
            else {
                throw std::invalid_argument("Invalid setter index");
            }
        }

        void addElem(Key* key) {
            elems[std::get<0>(key->get())] = key;
        }

        KeyTable getElems() const {
            return elems;
        }

        KeyTable& getElems() {
            return elems;
        }

        bool getIsMutable() const {
            return isMutable;
        }

        void seal() {
            isMutable = false;
        }

        bool getIsExplicitlyDefined() const {
            return isExplicitlyDefined;
        }

    protected:
        KeyTable elems;
        bool isMutable;
        FilePosition::Region defPos;
        bool isExplicitlyDefined;
    };
};

#endif
