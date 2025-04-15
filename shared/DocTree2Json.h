#pragma once

#ifndef DOC_TREE_2_JSON_H
#define DOC_TREE_2_JSON_H

#include <stdexcept>
#include <limits>
#include <json.hpp>
#include "DocumentTree.h"
#include "TomlStringUtils.h"

using json = nlohmann::json;

namespace DocTree {

#ifndef DEF_GLOBAL
    extern json toJson(const DocTreeNode* node, bool isValueTagged);
#else
    std::string convertToDecimalString(std::string input) {
        if (input.empty()) {
            return "Empty string";
        }
        bool isNeg = input[0] == '-';
        if (isNeg) {
            input = input.substr(1);
        }
        int base = 10;
        size_t start = 0;

        if (input.size() > 2 && input[0] == '0') {
            if (input[1] == 'x') {
                base = 16;
                start = 2;
            }
            else if (input[1] == 'o') {
                base = 8;
                start = 2;
            }
            else if (input[1] == 'b') {
                base = 2;
                start = 2;
            }
        }

        std::string numberPart = input.substr(start);

        try {
            unsigned long long value = std::stoull(numberPart, nullptr, base);
            return (isNeg && value ? "-" : "") + std::to_string(value);
        }
        catch (const std::invalid_argument&) {
            return "Invalid input";
        }
        catch (const std::out_of_range&) {
            return "Number out of range";
        }
    }

    json toJson(const DocTreeNode* node, bool isValueTagged) {
        if (!node) {
            return nullptr; // Handle nullptr nodes as JSON null
        }

        // Handle Value nodes
        if (const Value* valueNode = dynamic_cast<const Value*>(node)) {
            auto [type, valueStr] = valueNode->get();

            if (dynamic_cast<Type::String*>(type)) {
                if (isValueTagged) {
                    json valueJson;
                    valueJson["type"] = "string";
                    valueJson["value"] = extractStringLiteralContent(valueStr, ((Type::String*)type)->getType());
                    return valueJson;
                }
                else {
                    return extractStringLiteralContent(valueStr, ((Type::String*)type)->getType());
                }
            }
            else if (dynamic_cast<Type::Integer*>(type)) {
                std::string valueStrWithoutDelimiter = valueStr;
                valueStrWithoutDelimiter.erase(std::remove(valueStrWithoutDelimiter.begin(), valueStrWithoutDelimiter.end(), '_'), valueStrWithoutDelimiter.end());
                if (valueStrWithoutDelimiter[0] == '+') {
                    valueStrWithoutDelimiter.erase(valueStrWithoutDelimiter.begin());
                }
                std::string decimalValueStr = convertToDecimalString(valueStrWithoutDelimiter);
                if (isValueTagged) {
                    json valueJson;
                    valueJson["type"] = "integer";
                    valueJson["value"] = decimalValueStr;
                    return valueJson;
                }
                else {
                    return decimalValueStr;
                }
            }
            else if (dynamic_cast<Type::Float*>(type)) {
                std::string valueStrWithoutDelimiter = valueStr;
                valueStrWithoutDelimiter.erase(std::remove(valueStrWithoutDelimiter.begin(), valueStrWithoutDelimiter.end(), '_'), valueStrWithoutDelimiter.end());
                if (valueStrWithoutDelimiter[0] == '+') {
                    valueStrWithoutDelimiter.erase(valueStrWithoutDelimiter.begin());
                }
                if (isValueTagged) {
                    json valueJson;
                    valueJson["type"] = "float";
                    valueJson["value"] = valueStrWithoutDelimiter;
                    return valueJson;
                }
                else {
                    return std::stod(valueStrWithoutDelimiter);
                }
            }
            else if (dynamic_cast<Type::Boolean*>(type)) {
                if (isValueTagged) {
                    json valueJson;
                    valueJson["type"] = "bool";
                    valueJson["value"] = valueStr;
                    return valueJson;
                }
                else {
                    return valueStr == "true";
                }
            }
            else if (dynamic_cast<Type::DateTime*>(type)) {
                if (isValueTagged) {
                    json valueJson;
                    switch (((Type::DateTime*)type)->getType()) {
                    case Type::DateTime::OffsetDateTime:
                        valueJson["type"] = "datetime";
                        break;
                    case Type::DateTime::LocalDateTime:
                        valueJson["type"] = "datetime-local";
                        break;
                    case Type::DateTime::LocalDate:
                        valueJson["type"] = "date-local";
                        break;
                    case Type::DateTime::LocalTime:
                        valueJson["type"] = "time-local";
                        break;
                    default:
                        break;
                    }
                    valueJson["value"] = valueStr;
                    return valueJson;
                }
                else {
                    return valueStr; // DateTime as string
                }
            }
            else if (dynamic_cast<Type::SpecialNumber*>(type)) {
                if (isValueTagged) {
                    json valueJson;
                    valueJson["type"] = "float";
                    if (((Type::SpecialNumber*)type)->getType() == Type::SpecialNumber::NaN) {
                        valueJson["value"] = "nan";
                    }
                    else {
                        valueJson["value"] = valueStr;
                    }
                    return valueJson;
                }
                else {
                    if (valueStr == "nan" || valueStr == "+nan") {
                        return std::numeric_limits<double>::quiet_NaN();
                    }
                    else if (valueStr == "-nan") {
                        return -std::numeric_limits<double>::quiet_NaN();
                    }
                    else if (valueStr == "inf" || valueStr == "+inf") {
                        return std::numeric_limits<double>::infinity();
                    }
                    else if (valueStr == "-inf") {
                        return -std::numeric_limits<double>::infinity();
                    }
                    else {
                        throw std::runtime_error("Invalid SpecialNumber value: " + valueStr);
                    }
                }
            }
            else {
                throw std::runtime_error("Unsupported Type in Value node");
            }
        }

        // Handle Array nodes
        if (const Array* arrayNode = dynamic_cast<const Array*>(node)) {
            json arr = json::array();
            const auto elemVec = std::get<0>(arrayNode->get());
            for (const auto& elem : elemVec) {
                arr.push_back(elem ? toJson(elem, isValueTagged) : json(nullptr));
            }
            return arr;
        }

        // Handle Table nodes
        if (const Table* tableNode = dynamic_cast<const Table*>(node)) {
            json obj = json::object();
            const auto elemMap = std::get<0>(tableNode->get());
            for (const auto& [key, keyNode] : elemMap) {
                if (!keyNode) continue; // Skip null Key nodes
                auto [id, value] = keyNode->get();
                obj[id] = value ? toJson(value, isValueTagged) : json(nullptr);
            }
            return obj;
        }

        // Handle Key nodes (if encountered directly)
        if (const Key* keyNode = dynamic_cast<const Key*>(node)) {
            auto [id, value] = keyNode->get();
            json obj = json::object();
            obj[id] = value ? toJson(value, isValueTagged) : json(nullptr);
            return obj;
        }

        throw std::runtime_error("Unknown DocTreeNode type");
    }
#endif
} // namespace DocTree

#endif
