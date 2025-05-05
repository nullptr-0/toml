#pragma once

#ifndef TYPE_UTILS_H
#define TYPE_UTILS_H

#include "Type.h"

namespace Type {
#ifndef DEF_GLOBAL
    extern Type* CopyType(Type* type);
#else
    Type* CopyType(Type* type) {
        Type* copy = nullptr;
        if (auto* typeInvalid = dynamic_cast<Invalid*>(type)) {
            copy = new Invalid(*typeInvalid);
        }
        else if (auto* typeBoolean = dynamic_cast<Boolean*>(type)) {
            copy = new Boolean(*typeBoolean);
        }
        else if (auto* typeInteger = dynamic_cast<Integer*>(type)) {
            copy = new Integer(*typeInteger);
        }
        else if (auto* typeFloat = dynamic_cast<Float*>(type)) {
            copy = new Float(*typeFloat);
        }
        else if (auto* typeSpecialNumber = dynamic_cast<SpecialNumber*>(type)) {
            copy = new SpecialNumber(*typeSpecialNumber);
        }
        else if (auto* typeString = dynamic_cast<String*>(type)) {
            copy = new String(*typeString);
        }
        else if (auto* typeDateTime = dynamic_cast<DateTime*>(type)) {
            copy = new DateTime(*typeDateTime);
        }
        return copy;
    }
#endif

#ifndef DEF_GLOBAL
    extern void DeleteType(Type* type);
#else
    void DeleteType(Type* type) {
        if (!type) {
            return;
        }
        if (auto* typeInvalid = dynamic_cast<Invalid*>(type)) {
            delete typeInvalid;
        }
        else if (auto* typeBoolean = dynamic_cast<Boolean*>(type)) {
            delete typeBoolean;
        }
        else if (auto* typeInteger = dynamic_cast<Integer*>(type)) {
            delete typeInteger;
        }
        else if (auto* typeFloat = dynamic_cast<Float*>(type)) {
            delete typeFloat;
        }
        else if (auto* typeSpecialNumber = dynamic_cast<SpecialNumber*>(type)) {
            delete typeSpecialNumber;
        }
        else if (auto* typeString = dynamic_cast<String*>(type)) {
            delete typeString;
        }
        else if (auto* typeDateTime = dynamic_cast<DateTime*>(type)) {
            delete typeDateTime;
        }
        return;
    }
#endif
};

#endif
