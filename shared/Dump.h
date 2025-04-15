#pragma once

#ifndef DUMP_H
#define DUMP_H

#include <string>
#include <tuple>
#include "Type.h"
#include "DocumentTree.h"
#include "Log.h"

namespace Dump
{
#ifndef DEF_GLOBAL
    extern Log::StreamLogger logger;
#else
    Log::RawStreamLogger logger;
#endif

    // Helper function to print indentation
#ifndef DEF_GLOBAL
    extern void DumpIndent(size_t level);
#else
    void DumpIndent(size_t level) {
        for (size_t i = 0; i < level; ++i) {
            logger << "  ";
        }
    }
#endif

#ifndef DEF_GLOBAL
    extern void DumpType(Type::Type* type);
#else
    void DumpType(Type::Type* type) {
        if (!type) return;

        if (auto typeType = dynamic_cast<Type::Type*>(type)) {
            if (auto typeInvalid = dynamic_cast<Type::Invalid*>(type)) {
                logger << "invalid\n";
            }
            else if (dynamic_cast<Type::Valid*>(type)) {
                if (dynamic_cast<Type::Table*>(type)) {
                    logger << "table\n";
                }
                else if (dynamic_cast<Type::Array*>(type)) {
                    logger << "array\n";
                }
                else if (dynamic_cast<Type::BuiltIn*>(type)) {
                    if (dynamic_cast<Type::Boolean*>(type)) {
                        logger << "boolean\n";
                    }
                    else if (dynamic_cast<Type::Numeric*>(type)) {
                        if (dynamic_cast<Type::Integer*>(type)) {
                            logger << "integer\n";
                        }
                        else if (dynamic_cast<Type::Float*>(type)) {
                            logger << "float\n";
                        }
                        else if (auto typeSpecialNumber = dynamic_cast<Type::SpecialNumber*>(type)) {
                            auto specialNumberType = typeSpecialNumber->getType();
                            if (specialNumberType == Type::SpecialNumber::NaN) {
                                logger << "NaN\n";
                            }
                            else if (specialNumberType == Type::SpecialNumber::Infinity) {
                                logger << "infinity\n";
                            }
                        }
                    }
                    else if (auto typeString = dynamic_cast<Type::String*>(type)) {
                        auto stringType = typeString->getType();
                        if (stringType == Type::String::Basic) {
                            logger << "string\n";
                        }
                        else if (stringType == Type::String::MultiLineBasic) {
                            logger << "multi-line string\n";
                        }
                        else if (stringType == Type::String::Literal) {
                            logger << "literal string\n";
                        }
                        else if (stringType == Type::String::MultiLineLiteral) {
                            logger << "multi-line literal string\n";
                        }
                    }
                    else if (auto typeDateTime = dynamic_cast<Type::DateTime*>(type)) {
                        auto dateTimeType = typeDateTime->getType();
                        if (dateTimeType == Type::DateTime::OffsetDateTime) {
                            logger << "offset date-time\n";
                        }
                        else if (dateTimeType == Type::DateTime::LocalDateTime) {
                            logger << "local date-time\n";
                        }
                        else if (dateTimeType == Type::DateTime::LocalDate) {
                            logger << "local date\n";
                        }
                        else if (dateTimeType == Type::DateTime::LocalTime) {
                            logger << "local time\n";
                        }
                    }
                }
            }
        }
        else {
            logger << "unknown type\n";
        }
    }
#endif

#ifndef DEF_GLOBAL
    extern void DumpDocumentTree(DocTree::DocTreeNode* node, size_t indent = 0);
#else
    void DumpDocumentTree(DocTree::DocTreeNode* node, size_t indent = 0) {
        if (!node) return;

        // Print the current node
        DumpIndent(indent);

        if (auto value = dynamic_cast<DocTree::Value*>(node)) {
            logger << "Value:\n";
            DumpIndent(indent + 1);
            logger << "type: ";
            DumpType(std::get<0>(value->get()));
            DumpIndent(indent + 1);
            logger << "value: " << std::get<1>(value->get()) << "\n";
        }
        else if (auto array = dynamic_cast<DocTree::Array*>(node)) {
            logger << "Array:\n";
            DumpIndent(indent + 1);
            logger << "elems:\n";
            auto elems = std::get<0>(array->get());
            for (auto elem : elems) {
                DumpDocumentTree(elem, indent + 2);
            }
            DumpIndent(indent + 1);
            logger << "isDynamic: " << (std::get<1>(array->get()) ? "true" : "false") << "\n";
        }
        else if (auto key = dynamic_cast<DocTree::Key*>(node)) {
            logger << "Key:\n";
            DumpIndent(indent + 1);
            logger << "id: " << std::get<0>(key->get()) << "\n";
            DumpIndent(indent + 1);
            logger << "value:\n";
            DumpDocumentTree(std::get<1>(key->get()), indent + 2);
        }
        else if (auto table = dynamic_cast<DocTree::Table*>(node)) {
            logger << "Table\n";
            DumpIndent(indent + 1);
            logger << "elems:\n";
            auto elems = std::get<0>(table->get());
            for (auto elem : elems) {
                DumpDocumentTree(elem.second, indent + 2);
            }
        }
        else {
            logger << "Unknown node type\n";
        }
    }
#endif
};

#endif
