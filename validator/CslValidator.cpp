#include <vector>
#include <tuple>
#include <string>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cmath>
#include "../shared/CSLRepresentation.h"
#include "../shared/DocumentTree.h"
#include "../shared/FilePosition.h"
#include "../shared/TomlStringUtils.h"

namespace CSLValidator {

    using ErrorWarningList = std::vector<std::tuple<std::string, FilePosition::Region>>;

    class Validator {
    protected:
        using Value = std::variant<double, bool, std::string, std::monostate>;
        enum class ValueType { Number, Boolean, String, Null };

        const std::shared_ptr<CSL::ConfigSchema> schema;
        const DocTree::Table* docRoot;
        ErrorWarningList errors;
        ErrorWarningList warnings;
        std::string currentPath;

        void addError(const std::string& message, const FilePosition::Region& region) {
            errors.emplace_back(message, region);
        }

        void addWarning(const std::string& message, const FilePosition::Region& region) {
            warnings.emplace_back(message, region);
        }

        // Resolve a path in the current document context
        const DocTree::DocTreeNode* resolvePath(const std::string& path, const DocTree::Table* context) const {
            const DocTree::Table* currentTable = context;
            std::istringstream iss(path);
            std::string segment;

            while (std::getline(iss, segment, '.')) {
                auto currentTableElems = currentTable->getElems();
                auto it = currentTableElems.find(segment);
                if (it == currentTableElems.end()) return nullptr;

                if (auto keyNode = dynamic_cast<DocTree::Key*>(it->second)) {
                    auto valueNode = std::get<1>(keyNode->get());
                    if (auto table = dynamic_cast<DocTree::Table*>(valueNode)) {
                        currentTable = table;
                    }
                    else {
                        return valueNode;
                    }
                }
            }
            return currentTable;
        }

        FilePosition::Region getDocNodeDefPos(const DocTree::DocTreeNode* docNode) {
            if (auto valueNode = dynamic_cast<const DocTree::Value*>(docNode)) {
                return std::get<2>(valueNode->get());
            }
            else if (auto tableNode = dynamic_cast<const DocTree::Table*>(docNode)) {
                return std::get<2>(tableNode->get());
            }
            else if (auto arrayNode = dynamic_cast<const DocTree::Array*>(docNode)) {
                return std::get<2>(arrayNode->get());
            }
            else {
                return {};
            }
        }

        bool validateType(const CSL::CSLType* schemaType, const DocTree::DocTreeNode* docNode, const std::string& path) {
            switch (schemaType->getKind()) {
            case CSL::CSLType::Kind::Primitive: {
                auto primitiveType = static_cast<const CSL::PrimitiveType*>(schemaType);
                if (auto valueNode = dynamic_cast<const DocTree::Value*>(docNode)) {
                    return validatePrimitive(primitiveType, valueNode);
                }
                addError("Expected " + path + " as a primitive value", getDocNodeDefPos(docNode));
                return false;
            }
            case CSL::CSLType::Kind::Table: {
                auto tableType = static_cast<const CSL::TableType*>(schemaType);
                if (auto tableNode = dynamic_cast<const DocTree::Table*>(docNode)) {
                    return validateTable(tableType, tableNode, path);
                }
                addError("Expected " + path + " as a table", getDocNodeDefPos(docNode));
                return false;
            }
            case CSL::CSLType::Kind::Array: {
                auto arrayType = static_cast<const CSL::ArrayType*>(schemaType);
                if (auto arrayNode = dynamic_cast<const DocTree::Array*>(docNode)) {
                    return validateArray(arrayType, arrayNode, path);
                }
                addError("Expected " + path + " as an array", getDocNodeDefPos(docNode));
                return false;
            }
            case CSL::CSLType::Kind::Union: {
                auto unionType = static_cast<const CSL::UnionType*>(schemaType);
                return validateUnion(unionType, docNode, path);
            }
            case CSL::CSLType::Kind::AnyTable: {
                if (!dynamic_cast<const DocTree::Table*>(docNode)) {
                    addError("Expected " + path + " as an any table", getDocNodeDefPos(docNode));
                    return false;
                }
                return true;
            }
            case CSL::CSLType::Kind::AnyArray: {
                if (!dynamic_cast<const DocTree::Array*>(docNode)) {
                    addError("Expected " + path + " as an any array", getDocNodeDefPos(docNode));
                    return false;
                }
                return true;
            }
            default: {
                addError("Unsupported type kind", schemaType->getRegion());
                return false;
            }
            }
        }

        bool validatePrimitive(const CSL::PrimitiveType* schemaType, const DocTree::Value* valueNode) {
            Value actualValue = convertDocValue(valueNode);
            const auto& allowedValues = schemaType->getAllowedValues();
            std::vector<Value> allowedValuesActualValue;
            for (auto& allowedValue : allowedValues) {
                auto [valueStr, type] = allowedValue;
                allowedValuesActualValue.push_back(convertDocValue(type, valueStr));
            }

            // Check allowed values
            if (!allowedValues.empty()) {
                if (std::find(allowedValuesActualValue.begin(), allowedValuesActualValue.end(), actualValue) == allowedValuesActualValue.end()) {
                    addError("Value '" + std::get<1>(valueNode->get()) + "' not in allowed values", getDocNodeDefPos(valueNode));
                    return false;
                }
            }

            auto annotaions = schemaType->getAnnotations();
            for (const auto& annotation : annotaions) {
                if (!evaluateAnnotation(annotation, actualValue)) {
                    addError("Failed to validate key against annotation '" + annotation.get()->getName() + "'.", getDocNodeDefPos(valueNode));
                    return false;
                }
            }

            // Type validation
            auto valueNodeType = std::get<0>(valueNode->get());
            switch (schemaType->getPrimitive()) {
            case CSL::PrimitiveType::Primitive::String:
                if (!dynamic_cast<Type::String*>(valueNodeType)) {
                    addError("Expected string value", getDocNodeDefPos(valueNode));
                    return false;
                }
                break;
            case CSL::PrimitiveType::Primitive::Number:
                if (!(dynamic_cast<Type::Integer*>(valueNodeType) || dynamic_cast<Type::Float*>(valueNodeType) || dynamic_cast<Type::SpecialNumber*>(valueNodeType))) {
                    addError("Expected numeric value", getDocNodeDefPos(valueNode));
                    return false;
                }
                break;
            case CSL::PrimitiveType::Primitive::Boolean:
                if (!dynamic_cast<Type::Boolean*>(valueNodeType)) {
                    addError("Expected boolean value", getDocNodeDefPos(valueNode));
                    return false;
                }
                break;
            case CSL::PrimitiveType::Primitive::Datetime:
                if (!dynamic_cast<Type::DateTime*>(valueNodeType)) {
                    addError("Expected datetime value", getDocNodeDefPos(valueNode));
                    return false;
                }
                break;
            }

            return true;
        }

        bool validateTable(const CSL::TableType* schemaType, const DocTree::Table* tableNode, const std::string& path) {
            bool valid = true;
            const auto& explicitKeys = schemaType->getExplicitKeys();
            const auto& wildcardKey = schemaType->getWildcardKey();

            // Validate explicit keys
            for (const auto& keyDef : explicitKeys) {
                auto tableNodeElems = tableNode->getElems();
                auto it = tableNodeElems.find(keyDef.name);
                std::string newPath = path + "." + keyDef.name;
                if (it == tableNodeElems.end()) {
                    if (!keyDef.isOptional) {
                        addError("Missing required key: " + newPath, getDocNodeDefPos(tableNode));
                        valid = false;
                    }
                    continue;
                }

                auto keyValueNode = std::get<1>(it->second->get());
                if (!validateType(keyDef.type.get(), keyValueNode, newPath)) {
                    valid = false;
                }

                if (keyDef.annotations.size()) {
                    for (const auto& annotation : keyDef.annotations) {
                        if (!(dynamic_cast<DocTree::Value*>(keyValueNode) && evaluateAnnotation(annotation, convertDocValue((DocTree::Value*)keyValueNode)))) {
                            valid = false;
                            break;
                        }
                    }
                }
            }

            // Validate wildcard keys
            for (const auto& [keyName, keyNode] : tableNode->getElems()) {
                if (std::none_of(explicitKeys.begin(), explicitKeys.end(),
                    [&](const auto& k) { return k.name == keyName; })) {
                    auto keyValueNode = std::get<1>(keyNode->get());
                    if (wildcardKey) {
                        std::string newPath = path + ".*";
                        if (!validateType(wildcardKey->type.get(), keyValueNode, newPath)) {
                            addError("Key '" + path + "." + keyName + "' failed to match the type of the wildcard key",
                                getDocNodeDefPos(keyValueNode));
                            valid = false;
                        }
                    }
                    else {
                        addWarning("Key " + path + "." + keyName + " is not in the schema", getDocNodeDefPos(keyValueNode));
                    }
                }
            }

            // Validate constraints
            for (const auto& constraint : schemaType->getConstraints()) {
                if (!checkConstraint(constraint.get(), tableNode)) {
                    valid = false;
                }
            }

            return valid;
        }

        bool validateArray(const CSL::ArrayType* schemaType, const DocTree::Array* arrayNode, const std::string& path) {
            bool valid = true;
            auto elementType = schemaType->getElementType();
            int index = 0;

            for (auto elem : arrayNode->getElems()) {
                std::string elemPath = path + "[" + std::to_string(index++) + "]";
                if (!validateType(elementType.get(), elem, elemPath)) {
                    valid = false;
                }
            }
            return valid;
        }

        bool validateUnion(const CSL::UnionType* schemaType, const DocTree::DocTreeNode* docNode, const std::string& path) {
            for (const auto& memberType : schemaType->getMemberTypes()) {
                if (validateType(memberType.get(), docNode, path)) {
                    return true;
                }
            }
            addError("Value of " + path + " doesn't match any union member type", getDocNodeDefPos(docNode));
            return false;
        }

        bool checkConstraint(const CSL::Constraint* constraint, const DocTree::Table* context) {
            switch (constraint->getKind()) {
            case CSL::Constraint::Kind::Conflict:
                return checkConflict(static_cast<const CSL::ConflictConstraint*>(constraint), context);
            case CSL::Constraint::Kind::Dependency:
                return checkDependency(static_cast<const CSL::DependencyConstraint*>(constraint), context);
            case CSL::Constraint::Kind::Validate:
                return checkValidation(static_cast<const CSL::ValidateConstraint*>(constraint), context);
            default:
                addError("Unsupported constraint type", constraint->getRegion());
                return false;
            }
        }

        bool checkConflict(const CSL::ConflictConstraint* conflict, const DocTree::Table* context) {
            auto firstExpr = conflict->getFirstExpr();
            auto secondExpr = conflict->getSecondExpr();
            bool hasFirst = evaluateExpr(firstExpr, context);
            bool hasSecond = evaluateExpr(secondExpr, context);

            if (hasFirst && hasSecond) {
                auto firstExprDocNode = isSimpleKeyPath(firstExpr) ? resolvePath(exprToString(firstExpr), context) : nullptr;
                auto secondExprDocNode = isSimpleKeyPath(secondExpr) ? resolvePath(exprToString(secondExpr), context) : nullptr;
                auto errorMsg = "Conflicting keys: " + exprToString(firstExpr)
                    + " and " + exprToString(secondExpr);
                addError(errorMsg, getDocNodeDefPos(firstExprDocNode));
                addError(errorMsg, getDocNodeDefPos(secondExprDocNode));
                return false;
            }
            return true;
        }

        bool checkDependency(const CSL::DependencyConstraint* dep, const DocTree::Table* context) {
            auto dependentExpr = dep->getDependentExpr();
            auto conditionExpr = dep->getCondition();
            bool hasDependent = evaluateExpr(dependentExpr, context);
            bool hasCondition = evaluateExpr(conditionExpr, context);

            if (hasDependent && !hasCondition) {
                auto dependentExprDocNode = isSimpleKeyPath(dependentExpr) ? resolvePath(exprToString(dependentExpr), context) : nullptr;
                auto errorMsg = "Dependency failed: " + exprToString(dependentExpr) +
                    " requires " + exprToString(conditionExpr);
                addError(errorMsg, getDocNodeDefPos(dependentExprDocNode));
                return false;
            }
            return true;
        }

        bool checkValidation(const CSL::ValidateConstraint* validate, const DocTree::Table* context) {
            try {
                bool result = evaluateExpr(validate->getExpr(), context);
                if (!result) {
                    addError("Validation failed: " + exprToString(validate->getExpr()), {});
                }
                return result;
            }
            catch (const std::exception& e) {
                addError("Validation error: " + std::string(e.what()), validate->getRegion());
                return false;
            }
        }

        std::string exprToString(const std::shared_ptr<CSL::Expr>& expr) const {
            if (auto id = dynamic_cast<CSL::IdentifierExpr*>(expr.get())) {
                return id->getName();
            }
            else if (auto lit = dynamic_cast<CSL::LiteralExpr*>(expr.get())) {
                return lit->getValue();
            }
            else if (auto bin = dynamic_cast<CSL::BinaryExpr*>(expr.get())) {
                return exprToString(bin->getLHS()) + bin->getOp() + exprToString(bin->getRHS());
            }
            else if (auto un = dynamic_cast<CSL::UnaryExpr*>(expr.get())) {
                return un->getOp() + exprToString(un->getOperand());
            }
            else if (auto tern = dynamic_cast<CSL::TernaryExpr*>(expr.get())) {
                return exprToString(tern->getCondition()) + " ? " +
                    exprToString(tern->getTrueExpr()) + " : " +
                    exprToString(tern->getFalseExpr());
            }
            else if (auto funcArg = dynamic_cast<CSL::FunctionArgExpr*>(expr.get())) {
                if (auto arg = std::get_if<std::shared_ptr<CSL::Expr>>(&funcArg->getValue())) {
                    return exprToString(*arg);
                }
                else if (auto args = std::get_if<std::vector<std::shared_ptr<CSL::Expr>>>(&funcArg->getValue())) {
                    std::string argsStr;
                    for (const auto& arg : *args) {
                        argsStr += exprToString(arg) + ", ";
                    }
                    if (!argsStr.empty()) {
                        argsStr.pop_back();
                        argsStr.pop_back();
                    }
                    return "[" + argsStr + "]";
                }
            }
            else if (auto funcCall = dynamic_cast<CSL::FunctionCallExpr*>(expr.get())) {
                std::string argsStr;
                for (const auto& arg : funcCall->getArgs()) {
                    argsStr += exprToString(arg) + ", ";
                }
                if (!argsStr.empty()) {
                    argsStr.pop_back();
                    argsStr.pop_back();
                }
                return funcCall->getFuncName() + "(" + argsStr + ")";
            }
            else if (auto annotation = dynamic_cast<CSL::AnnotationExpr*>(expr.get())) {
                std::string argsStr;
                for (const auto& arg : annotation->getAnnotation()->getArgs()) {
                    argsStr += exprToString(arg) + ", ";
                }
                if (!argsStr.empty()) {
                    argsStr.pop_back();
                    argsStr.pop_back();
                }
                return exprToString(annotation->getTarget()) + "@" + annotation->getAnnotation()->getName()
                    + "(" + argsStr + ")";
            }
            return "";
        }

        Value convertDocValue(const Type::Type* type, const std::string& valueStr) {
            if (dynamic_cast<const Type::String*>(type)) {
                return valueStr;
            }
            else if (dynamic_cast<const Type::Integer*>(type)) {
                std::string cleaned = valueStr;
                cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '_'), cleaned.end());
                if (cleaned.empty()) return 0.0;
                if (cleaned[0] == '+') {
                    cleaned.erase(cleaned.begin());
                }
                std::string decimalValueStr = convertToDecimalString(cleaned);

                try {
                    return static_cast<double>(std::stoll(decimalValueStr));
                }
                catch (...) { return std::monostate{}; }
            }
            else if (dynamic_cast<const Type::Float*>(type)) {
                std::string cleaned = valueStr;
                cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '_'), cleaned.end());
                if (cleaned.empty()) return 0.0;
                if (cleaned[0] == '+') {
                    cleaned.erase(cleaned.begin());
                }
                try { return std::stod(cleaned); }
                catch (...) { return std::monostate{}; }
            }
            else if (dynamic_cast<const Type::Boolean*>(type)) {
                return valueStr == "true";
            }
            else if (dynamic_cast<const Type::DateTime*>(type)) {
                return valueStr;
            }
            else if (dynamic_cast<const Type::SpecialNumber*>(type)) {
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
                    return std::monostate{};
                }
            }
            return std::monostate{};
        }

        Value convertDocValue(const DocTree::Value* valueNode) {
            auto [typeInfo, valueStr, defPos] = valueNode->get();
            return convertDocValue(typeInfo, valueStr);
        }

        Value resolveKeyValue(const std::string& path, const DocTree::Table* context) {
            const DocTree::DocTreeNode* node = resolvePath(path, context);
            if (!node) return std::monostate{};

            if (auto valueNode = dynamic_cast<const DocTree::Value*>(node)) {
                return convertDocValue(valueNode);
            }
            return true; // Exists but not a value node
        }

        bool deepCompare(DocTree::DocTreeNode* a, DocTree::DocTreeNode* b) {
            // Handle null cases
            if (!a || !b) return a == b;

            // Value comparison
            if (auto aVal = dynamic_cast<DocTree::Value*>(a)) {
                if (auto bVal = dynamic_cast<DocTree::Value*>(b)) {
                    return compareValues(convertDocValue(aVal), convertDocValue(bVal), true);
                }
                return false;
            }

            // Table comparison
            if (auto aTable = dynamic_cast<DocTree::Table*>(a)) {
                if (auto bTable = dynamic_cast<DocTree::Table*>(b)) {
                    return compareTables(aTable, bTable);
                }
                return false;
            }

            // Array comparison
            if (auto aArray = dynamic_cast<DocTree::Array*>(a)) {
                if (auto bArray = dynamic_cast<DocTree::Array*>(b)) {
                    return compareArrays(aArray, bArray);
                }
                return false;
            }

            return false;
        }

        bool compareTables(const DocTree::Table* a, const DocTree::Table* b) {
            const auto& aElems = a->getElems();
            const auto& bElems = b->getElems();

            // Check all keys in A exist in B with matching values
            for (const auto& [key, aNode] : aElems) {
                auto bIt = bElems.find(key);
                if (bIt == bElems.end() || !deepCompare(std::get<1>(aNode->get()), std::get<1>(bIt->second->get()))) {
                    return false;
                }
            }
            return true;
        }

        bool compareArrays(const DocTree::Array* a, const DocTree::Array* b) {
            const auto& aItems = a->getElems();
            const auto& bItems = b->getElems();

            // Check array lengths if order matters
            // For subset semantics, check all elements exist regardless of order
            for (auto aItem : aItems) {
                bool found = false;
                for (auto bItem : bItems) {
                    if (deepCompare(aItem, bItem)) {
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        }

        bool evaluateSubsetFunction(const std::vector<std::variant<DocTree::DocTreeNode*, std::vector<std::string>>>& args) {
            try {
                // Extract array elements
                std::vector<DocTree::DocTreeNode*> sourceElements;
                std::vector<DocTree::DocTreeNode*> targetElements;

                if (auto firstArg = *std::get_if<DocTree::DocTreeNode*>(&args[0])) {
                    if (auto arrayNode = dynamic_cast<DocTree::Array*>(firstArg)) {
                        sourceElements = arrayNode->getElems();
                    }
                    else {
                        errors.push_back({ "First argument of subset must be an array", getDocNodeDefPos(firstArg) });
                        return false;
                    }
                }
                else {
                    errors.push_back({ "First argument of subset must be an array", {} });
                    return false;
                }

                if (auto secondArg = *std::get_if<DocTree::DocTreeNode*>(&args[1])) {
                    if (auto arrayNode = dynamic_cast<DocTree::Array*>(secondArg)) {
                        targetElements = arrayNode->getElems();
                    }
                    else {
                        errors.push_back({ "Second argument of subset must be an array", getDocNodeDefPos(secondArg) });
                        return false;
                    }
                }
                else {
                    errors.push_back({ "Second argument of subset must be an array", {} });
                    return false;
                }

                // Handle empty source array
                if (sourceElements.empty()) return true;

                // Get comparison properties if specified
                std::vector<std::string> properties;
                if (args.size() > 2) {
                    if (auto vec = std::get_if<std::vector<std::string>>(&args[2])) {
                        properties = *vec;
                    }
                    else {
                        errors.push_back({ "Third argument of subset must be a key list", {} });
                        return false;
                    }
                }

                // Validate each source element
                for (DocTree::DocTreeNode* sourceElem : sourceElements) {
                    bool found = false;

                    if (properties.empty()) {
                        for (DocTree::DocTreeNode* targetElem : targetElements) {
                            if (deepCompare(sourceElem, targetElem)) {
                                found = true;
                                break;
                            }
                        }
                    }
                    else {
                        // Object property comparison
                        const DocTree::Table* sourceObj = dynamic_cast<DocTree::Table*>(sourceElem);
                        if (!sourceObj) {
                            errors.push_back({ "Source element is not an object", getDocNodeDefPos(sourceElem) });
                            return false;
                        }

                        // Extract source properties
                        std::unordered_map<std::string, DocTree::DocTreeNode*> sourceProps;
                        for (const auto& prop : properties) {
                            auto propValueNode = resolvePath(prop, sourceObj);
                            if (propValueNode) {
                                errors.push_back({ "Missing property '" + prop + "' in source object", getDocNodeDefPos(sourceObj) });
                                return false;
                            }
                            sourceProps[prop] = (DocTree::DocTreeNode*)propValueNode;
                        }

                        // Check against target objects
                        for (DocTree::DocTreeNode* targetElem : targetElements) {
                            const DocTree::Table* targetObj = dynamic_cast<DocTree::Table*>(targetElem);
                            if (!sourceObj) {
                                errors.push_back({ "Target element is not an object", getDocNodeDefPos(targetElem) });
                                return false;
                            }

                            bool match = true;
                            for (const auto& prop : properties) {
                                auto propValueNode = resolvePath(prop, targetObj);
                                if (propValueNode) {
                                    match = false;
                                    break;
                                }

                                if (!deepCompare(sourceProps[prop], (DocTree::DocTreeNode*)propValueNode)) {
                                    match = false;
                                    break;
                                }
                            }

                            if (match) {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (!found) {
                        return false;
                    }
                }

                return true;
            }
            catch (const std::exception& e) {
                addError("Subset validation failed: " + std::string(e.what()), {});
                return false;
            }
        }

        std::variant<Value, std::vector<DocTree::DocTreeNode*>> evaluateFunctionCall(const CSL::FunctionCallExpr* funcCall, const DocTree::Table* context) {
            const std::string& funcName = funcCall->getFuncName();
            const auto& argExprs = funcCall->getArgs();
            std::vector<std::variant<DocTree::DocTreeNode*, std::vector<std::string>>> argValues;
            for (const auto& argExpr : argExprs) {
                if (auto funcArg = dynamic_cast<CSL::FunctionArgExpr*>(argExpr.get())) {
                    if (auto arg = *std::get_if<std::shared_ptr<CSL::Expr>>(&funcArg->getValue())) {
                        argValues.push_back((DocTree::DocTreeNode*)resolvePath(exprToString(arg), context));
                    }
                    else if (auto args = std::get_if<std::vector<std::shared_ptr<CSL::Expr>>>(&funcArg->getValue())) {
                        std::vector<std::string> argsValue;
                        for (const auto& arg : *args) {
                            argsValue.push_back(exprToString(arg));
                        }
                        argValues.push_back(argsValue);
                    }
                }
            }

            try {
                if (funcName == "count_keys") {
                    auto arg = *std::get_if<DocTree::DocTreeNode*>(&argValues[0]);
                    if (auto table = dynamic_cast<DocTree::Table*>(arg)) {
                        return static_cast<double>(table->getElems().size());
                    }
                }
                else if (funcName == "all_keys") {
                    auto arg = *std::get_if<DocTree::DocTreeNode*>(&argValues[0]);
                    std::vector<DocTree::DocTreeNode*> keys;
                    if (auto table = dynamic_cast<DocTree::Table*>(arg)) {
                        for (const auto& [_, key] : table->getElems()) {
                            keys.push_back(key);
                        }
                    }
                    return keys;
                }
                else if (funcName == "subset") {
                    return evaluateSubsetFunction(argValues);
                }
                else if (funcName == "exists") {
                    return *std::get_if<DocTree::DocTreeNode*>(&argValues[0]) != nullptr;
                }
            }
            catch (const std::exception& e) {
                addError("Function call error: " + std::string(e.what()), funcCall->getRegion());
            }
            return false;
        }

        bool evaluateAnnotation(const std::shared_ptr<CSL::Annotation>& annotation, const Value& targetValue) {
            auto annotationName = annotation->getName();
            if (annotationName == "regex") {
                auto pattern = std::get<std::string>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    try {
                        return std::regex_match(*strVal, std::regex(pattern));
                    }
                    catch (...) {
                        return false;
                    }
                }
            }
            else if (annotationName == "start_with") {
                auto prefix = std::get<std::string>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    return strVal->find(prefix) == 0;
                }
            }
            else if (annotationName == "end_with") {
                auto suffix = std::get<std::string>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    return strVal->rfind(suffix) == strVal->length() - suffix.length();
                }
            }
            else if (annotationName == "contain") {
                auto substring = std::get<std::string>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    return strVal->find(substring) != std::string::npos;
                }
            }
            else if (annotationName == "min_length") {
                auto minLength = std::get<double>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    return strVal->length() >= std::string::npos;
                }
            }
            else if (annotationName == "max_length") {
                auto maxLength = std::get<double>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto strVal = std::get_if<std::string>(&targetValue)) {
                    return strVal->length() <= maxLength;
                }
            }
            else if (annotationName == "min") {
                auto minValue = std::get<double>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto numVal = std::get_if<double>(&targetValue)) {
                    return *numVal >= minValue;
                }
            }
            else if (annotationName == "max") {
                auto maxValue = std::get<double>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                if (auto numVal = std::get_if<double>(&targetValue)) {
                    return *numVal <= maxValue;
                }
            }
            else if (annotationName == "range") {
                auto minValue = std::get<double>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                auto maxValue = std::get<double>(evaluateExprValue(annotation->getArgs()[1], nullptr));
                if (auto numVal = std::get_if<double>(&targetValue)) {
                    return *numVal >= minValue && *numVal <= maxValue;
                }
            }
            else if (annotationName == "int") {
                if (auto numVal = std::get_if<double>(&targetValue)) {
                    return std::floor(*numVal) == *numVal;
                }
            }
            else if (annotationName == "float") {
                if (auto numVal = std::get_if<double>(&targetValue)) {
                    return std::floor(*numVal) != *numVal;
                }
            }
            else if (annotationName == "format") {
                if (auto idExpr = dynamic_cast<CSL::IdentifierExpr*>(annotation->getArgs()[0].get())) {
                    auto formatId = idExpr->getName();
                    if (auto strVal = std::get_if<std::string>(&targetValue)) {
                        if (formatId == "email") {
                            return std::regex_match(*strVal, std::regex(R"((?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\]))"));
                        }
                        else if (formatId == "uuid") {
                            return std::regex_match(*strVal, std::regex(R"(([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}))"));
                        }
                        else if (formatId == "ipv4") {
                            return std::regex_match(*strVal, std::regex(R"((25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9]))"));
                        }
                        else if (formatId == "ipv6") {
                            return std::regex_match(*strVal, std::regex(R"((?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}|(?:[0-9a-fA-F]{1,4}:){1,7}:|(?:[0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}|(?:[0-9a-fA-F]{1,4}:){1,5}(?::[0-9a-fA-F]{1,4}){1,2}|(?:[0-9a-fA-F]{1,4}:){1,4}(?::[0-9a-fA-F]{1,4}){1,3}|(?:[0-9a-fA-F]{1,4}:){1,3}(?::[0-9a-fA-F]{1,4}){1,4}|(?:[0-9a-fA-F]{1,4}:){1,2}(?::[0-9a-fA-F]{1,4}){1,5}|[0-9a-fA-F]{1,4}:(?::[0-9a-fA-F]{1,4}){1,6}|:((?::[0-9a-fA-F]{1,4}){1,7}|:)|fe80:(?::[0-9a-fA-F]{0,4}){0,4}%[0-9a-zA-Z]+|::(ffff(:0{1,4}){0,1}:){0,1}(25[0-5]|(2[0-4][0-9]|(1[01][0-9]|[1-9]?[0-9]))\.){3}(25[0-5]|(2[0-4][0-9]|(1[01][0-9]|[1-9]?[0-9]))))"));
                        }
                        else if (formatId == "url") {
                            return std::regex_match(*strVal, std::regex(R"((?:(?:https?|ftp):\/\/)?(?:\S+(?::\S*)?@)?((?:(?!-)[A-Za-z0-9-]{0,62}[A-Za-z0-9]\.)+[A-Za-z]{2,6}|(?:\d{1,3}\.){3}\d{1,3})(?::\d{2,5})?(?:\/[^\s?#]*)?(?:\?[^\s#]*)?(?:#[^\s]*)?)"));
                        }
                        else if (formatId == "phone") {
                            return std::regex_match(*strVal, std::regex(R"(\+?[0-9]{1,4}?[-. ]?\(?[0-9]{1,4}?\)?[-. ]?[0-9]{1,4}[-. ]?[0-9]{1,9})"));
                        }
                        else {
                            addError("Unknown format type: " + formatId, annotation->getRegion());
                            return false;
                        }
                    }
                }
            }
            else if (annotationName == "deprecated") {
                auto deprecatedMsg = std::get<std::string>(evaluateExprValue(annotation->getArgs()[0], nullptr));
                addWarning(deprecatedMsg, annotation->getRegion());
                return true;
            }
            return false;
        }

        bool evaluateAnnotationExpr(const CSL::AnnotationExpr* annoExpr, const DocTree::Table* context) {
            auto targetExpr = annoExpr->getTarget();
            std::vector<Value> targetValues;
            if (auto funcCall = dynamic_cast<CSL::FunctionCallExpr*>(targetExpr.get())) {
                auto funcCallValue = evaluateFunctionCall(funcCall, context);
                if (funcCall->getFuncName() == "all_keys" && std::holds_alternative<std::vector<DocTree::DocTreeNode*>>(funcCallValue)) {
                    auto keys = std::get<std::vector<DocTree::DocTreeNode*>>(funcCallValue);
                    for (auto& key : keys) {
                        if (auto keyNode = dynamic_cast<DocTree::Key*>(key)) {
                            targetValues.push_back(std::get<0>(keyNode->get()));
                        }
                        else {
                            errors.push_back({ "Invalid key from all_keys function", getDocNodeDefPos(key) });
                        }
                    }
                }
                else if (std::holds_alternative<Value>(funcCallValue)) {
                    targetValues.push_back(std::get<Value>(funcCallValue));
                }
                else {
                    errors.push_back({ "Invalid function call as annotation target", funcCall->getRegion() });
                }
            }
            else {
                targetValues.push_back(evaluateExprValue(annoExpr->getTarget(), context));
            }
            auto annotation = annoExpr->getAnnotation();
            auto annotationName = annotation->getName();

            for (auto& targetValue : targetValues) {
                evaluateAnnotation(annotation, targetValue);
            }
            return false;
        }

        Value evaluateExprValue(const std::shared_ptr<CSL::Expr>& expr, const DocTree::Table* context) {
            if (auto idExpr = dynamic_cast<CSL::IdentifierExpr*>(expr.get())) {
                return resolveKeyValue(idExpr->getName(), context);
            }

            if (auto binExpr = dynamic_cast<CSL::BinaryExpr*>(expr.get())) {
                if (binExpr->getOp() == ".") {
                    // Handle dotted key path
                    std::string left = exprToString(binExpr->getLHS());
                    std::string right = exprToString(binExpr->getRHS());
                    return resolveKeyValue(left + "." + right, context);
                }

                // Normal binary operation
                Value lhs = evaluateExprValue(binExpr->getLHS(), context);
                Value rhs = evaluateExprValue(binExpr->getRHS(), context);
                return applyBinaryOp(lhs, rhs, binExpr->getOp());
            }

            if (auto litExpr = dynamic_cast<CSL::LiteralExpr*>(expr.get())) {
                const Type::Type* type = litExpr->getType();
                auto valueStr = litExpr->getValue();
                convertDocValue(type, valueStr);
            }

            if (auto unExpr = dynamic_cast<CSL::UnaryExpr*>(expr.get())) {
                auto operand = evaluateExprValue(unExpr->getOperand(), context);
                if (unExpr->getOp() == "!") return !convertToBool(operand);
                if (unExpr->getOp() == "~") {
                    if (std::holds_alternative<double>(operand)) {
                        return (double)~(long long)std::get<double>(operand);
                    }
                    else {
                        if (std::holds_alternative<bool>(operand)) {
                            return (double)~(long long)std::get<bool>(operand);
                        }
                        else {
                            return (double)~(long long)convertToBool(operand);
                        }
                    }
                }
                if (unExpr->getOp() == "+") return convertToBool(operand);
                if (unExpr->getOp() == "-") {
                    if (std::holds_alternative<double>(operand)) {
                        return -std::get<double>(operand);
                    }
                    else {
                        if (std::holds_alternative<bool>(operand)) {
                            return -(double)std::get<bool>(operand);
                        }
                        else {
                            return -(double)convertToBool(operand);
                        }
                    }
                }
                return std::monostate{};
            }

            if (auto ternExpr = dynamic_cast<CSL::TernaryExpr*>(expr.get())) {
                bool cond = convertToBool(evaluateExprValue(ternExpr->getCondition(), context));
                return cond ? evaluateExprValue(ternExpr->getTrueExpr(), context)
                    : evaluateExprValue(ternExpr->getFalseExpr(), context);
            }

            if (auto funcCall = dynamic_cast<CSL::FunctionCallExpr*>(expr.get())) {
                return std::get<Value>(evaluateFunctionCall(funcCall, context));
            }

            if (auto annoExpr = dynamic_cast<CSL::AnnotationExpr*>(expr.get())) {
                return evaluateAnnotationExpr(annoExpr, context);
            }

            throw std::runtime_error("Unsupported expression type");
        }

        bool isSimpleKeyPath(const std::shared_ptr<CSL::Expr>& expr) {
            if (dynamic_cast<CSL::IdentifierExpr*>(expr.get())) return true;

            if (auto binExpr = dynamic_cast<CSL::BinaryExpr*>(expr.get())) {
                return binExpr->getOp() == "." &&
                    isSimpleKeyPath(binExpr->getLHS()) &&
                    isSimpleKeyPath(binExpr->getRHS());
            }

            return false;
        }

        bool applyBinaryOp(const Value& lhs, const Value& rhs, const std::string& op) {
            auto typeMatch = [](const Value& a, const Value& b) {
                return a.index() == b.index();
            };

            if (op == "==") return compareValues(lhs, rhs, true);
            if (op == "!=") return !compareValues(lhs, rhs, true);

            if (!typeMatch(lhs, rhs)) return false;

            if (std::holds_alternative<double>(lhs)) {
                double l = std::get<double>(lhs);
                double r = std::get<double>(rhs);
                if (op == "+") return l + r;
                if (op == "-") return l - r;
                if (op == "*") return l * r;
                if (op == "/") return l / r;
                if (op == "<") return l < r;
                if (op == ">") return l > r;
                if (op == "<=") return l <= r;
                if (op == ">=") return l >= r;
                if (std::floor(l) == l && std::floor(r) == r) {
                    auto lInt = static_cast<long long>(l);
                    auto rInt = static_cast<long long>(r);
                    if (op == "%") return lInt % rInt;
                    if (op == "<<") return lInt << rInt;
                    if (op == ">>") return lInt >> rInt;
                    if (op == "&") return lInt & rInt;
                    if (op == "|") return lInt | rInt;
                    if (op == "^") return lInt ^ rInt;
                }
            }
            else if (std::holds_alternative<std::string>(lhs)) {
                std::string l = std::get<std::string>(lhs);
                std::string r = std::get<std::string>(rhs);
                if (op == "+") return (l + r).length();
                if (op == "<") return l < r;
                if (op == ">") return l > r;
                if (op == "<=") return l <= r;
                if (op == ">=") return l >= r;
            }

            if (op == "&&") return convertToBool(lhs) && convertToBool(rhs);
            if (op == "||") return convertToBool(lhs) || convertToBool(rhs);

            throw std::runtime_error("Unsupported operator: " + op);
        }

        bool compareValues(const Value& a, const Value& b, bool checkEquality) {
            if (checkEquality) {
                if (std::holds_alternative<std::monostate>(a) || std::holds_alternative<std::monostate>(b))
                    return std::holds_alternative<std::monostate>(a) && std::holds_alternative<std::monostate>(b);
            }

            if (a.index() != b.index()) return false;

            if (std::holds_alternative<double>(a)) {
                double numA = std::get<double>(a);
                double numB = std::get<double>(b);
                return checkEquality ? (numA == numB) : (numA != numB);
            }
            if (std::holds_alternative<bool>(a)) {
                bool boolA = std::get<bool>(a);
                bool boolB = std::get<bool>(b);
                return checkEquality ? (boolA == boolB) : (boolA != boolB);
            }
            if (std::holds_alternative<std::string>(a)) {
                std::string strA = std::get<std::string>(a);
                std::string strB = std::get<std::string>(b);
                return checkEquality ? (strA == strB) : (strA != strB);
            }
            return false;
        }

        bool convertToBool(const Value& value) {
            if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
            if (std::holds_alternative<double>(value)) return std::get<double>(value) != 0;
            if (std::holds_alternative<std::string>(value)) return !std::get<std::string>(value).empty();
            return false;
        }

        bool evaluateExpr(const std::shared_ptr<CSL::Expr>& expr, const DocTree::Table* context) {
            // Handle existence check for bare/dotted keys
            if (isSimpleKeyPath(expr)) {
                std::string path = exprToString(expr);
                return resolvePath(path, context);
            }

            // Handle complex expressions
            Value result = evaluateExprValue(expr, context);
            return convertToBool(result);
        }

    public:
        Validator(const std::shared_ptr<CSL::ConfigSchema> schema, const DocTree::Table* docRoot)
            : schema(schema), docRoot(docRoot) {
        }

        std::tuple<ErrorWarningList, ErrorWarningList> validate() {
            currentPath = schema->getName();
            validateType(schema->getRootTable().get(), docRoot, currentPath);
            return { errors, warnings };
        }
    };

} // namespace CSLValidator

std::tuple<std::vector<std::tuple<std::string, FilePosition::Region>>, std::vector<std::tuple<std::string, FilePosition::Region>>> CslValidatorMain(std::string schemaName, std::vector<std::shared_ptr<CSL::ConfigSchema>> schemas, DocTree::Table* docTree) {
    if (schemaName.empty() && schemas.size() == 1) {
        CSLValidator::Validator validator(schemas[0], docTree);
        return validator.validate();
    }

    std::shared_ptr<CSL::ConfigSchema> schema;
    for (const auto& sch : schemas) {
        if (sch->getName() == schemaName) {
            schema = sch;
            break;
        }
    }

    if (schema) {
        CSLValidator::Validator validator(schema, docTree);
        return validator.validate();
    }
    return { { { "Cannot find config schema " + schemaName, {} } }, {} };
}