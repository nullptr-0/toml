#pragma once

#ifndef DOC_TREE_2_TOML_H
#define DOC_TREE_2_TOML_H

#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include "DocumentTree.h"

namespace DocTree {

#ifndef DEF_GLOBAL
    extern void toToml(DocTree::Table* docTree, std::ostream& output);
    extern std::string toToml(DocTree::Table* docTree);
#else
    void processInlineTable(Table* table, std::ostream& os, int indent = 0);
    void processInlineArray(Array* array, std::ostream& os, int indent = 0);
    void processTable(Table* table, std::ostream& os, const std::string& currentScope, int indent = 0);

    std::string getIndent(int level) {
        return std::string(level * 2, ' '); // 2 spaces per indent level
    }

    void processInlineTable(Table* table, std::ostream& os, int indent) {
        const auto& elems = table->getElems();
        std::vector<std::string> keys;
        for (const auto& [key, _] : elems) keys.push_back(key);
        std::sort(keys.begin(), keys.end()); // Sort keys lexicographically

        os << "{ ";
        bool first = true;
        for (const auto& key : keys) {
            auto* keyNode = elems.at(key);
            if (!keyNode) continue;
            const auto& [id, valueNode, parentTable] = keyNode->get();
            if (!valueNode) continue;

            if (!first) os << ", ";
            first = false;

            os << id << " = ";
            if (auto* value = dynamic_cast<Value*>(valueNode)) {
                auto [type, valStr] = value->get();
                os << valStr;
            }
            else if (auto* array = dynamic_cast<Array*>(valueNode)) {
                processInlineArray(array, os, indent);
            }
            else if (auto* nestedTable = dynamic_cast<Table*>(valueNode)) {
                processInlineTable(nestedTable, os, indent);
            }
        }
        os << " }";
    }

    void processInlineArray(Array* array, std::ostream& os, int indent) {
        const auto& elems = array->getElems();
        os << "[ ";
        bool first = true;
        for (const auto& elem : elems) {
            if (!elem) continue;

            if (!first) os << ", ";
            first = false;

            if (auto* value = dynamic_cast<Value*>(elem)) {
                auto [type, valStr] = value->get();
                os << valStr;
            }
            else if (auto* nestedArray = dynamic_cast<Array*>(elem)) {
                processInlineArray(nestedArray, os, indent);
            }
            else if (auto* table = dynamic_cast<Table*>(elem)) {
                processInlineTable(table, os, indent);
            }
        }
        os << " ]";
    }

    void processArray(const std::string& key, Array* array, std::ostream& os, const std::string& currentScope, int indent) {
        const auto& elems = array->getElems();
        bool isArrayOfTables = std::all_of(elems.begin(), elems.end(), [](const auto& elem) {
            if (!elem) return false;
            auto* table = dynamic_cast<Table*>(elem);
            return table && table->getIsExplicitlyDefined();
            });

        if (isArrayOfTables) {
            for (const auto& elem : elems) {
                if (!elem) continue;
                Table* tableElem = static_cast<Table*>(elem);
                std::string arrayScope = currentScope.empty() ? key : currentScope + "." + key;
                if (os.tellp() != 0) os << "\n";
                os << getIndent(indent) << "[[" << arrayScope << "]]\n";
                processTable(tableElem, os, arrayScope, indent + 1);
            }
        }
        else {
            os << getIndent(indent) << key << " = ";
            processInlineArray(array, os, indent);
            os << "\n";
        }
    }

    void processTable(Table* table, std::ostream& os, const std::string& currentScope, int indent) {
        const auto& elems = table->getElems();
        std::vector<std::string> keys;
        for (const auto& [key, _] : elems) keys.push_back(key);
        std::sort(keys.begin(), keys.end()); // Sort keys lexicographically

        // Process non-table values first for better readability
        for (const auto& key : keys) {
            auto* keyNode = elems.at(key);
            if (!keyNode) continue;
            const auto& [id, valueNode, parentTable] = keyNode->get();
            if (!valueNode) continue;

            if (dynamic_cast<Value*>(valueNode) || dynamic_cast<Array*>(valueNode)) {
                os << getIndent(indent);
                if (auto* value = dynamic_cast<Value*>(valueNode)) {
                    auto [type, valStr] = value->get();
                    os << id << " = " << valStr << "\n";
                }
                else if (auto* array = dynamic_cast<Array*>(valueNode)) {
                    processArray(id, array, os, currentScope, indent);
                }
            }
        }

        // Process nested tables last
        for (const auto& key : keys) {
            auto* keyNode = elems.at(key);
            if (!keyNode) continue;
            const auto& [id, valueNode, parentTable] = keyNode->get();
            if (!valueNode) continue;

            if (auto* tableValue = dynamic_cast<Table*>(valueNode)) {
                if (tableValue->getIsExplicitlyDefined()) {
                    std::string newScope = currentScope.empty() ? id : currentScope + "." + id;
                    if (os.tellp() != 0) os << "\n";
                    os << getIndent(indent) << "[" << newScope << "]\n";
                    processTable(tableValue, os, newScope, indent);
                }
                else {
                    os << getIndent(indent) << id << " = ";
                    processInlineTable(tableValue, os, indent);
                    os << "\n";
                }
            }
        }
    }

    void toToml(DocTree::Table* docTree, std::ostream& output) {
        processTable(docTree, output, "", 0);
    }

    std::string toToml(DocTree::Table* docTree) {
        std::ostringstream output;
        toToml(docTree, output);
        return output.str();
    }
#endif
} // anonymous namespace
#endif
