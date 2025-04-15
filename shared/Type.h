#pragma once

#ifndef TYPE_H
#define TYPE_H

#include <string>
#include <sstream>
#include <vector>
#include <tuple>

namespace Type
{
    class Type {
    public:
        Type() {}
        virtual ~Type() {}
    };

    class Invalid : public Type {
    public:
        Invalid() {}
        virtual ~Invalid() {}
    };

    class Valid : public Type {
    public:
        Valid() {}
        virtual ~Valid() {}
    };

    class Table : public Valid {
    public:
        Table() {}
        virtual ~Table() {}
    };

    class Array : public Valid {
    public:
        Array() {}
        virtual ~Array() {}
    };

    class BuiltIn : public Valid {
    public:
        BuiltIn() {}
        virtual ~BuiltIn() {}
    };

    class Boolean : public BuiltIn {
    public:
        Boolean() {}
        virtual ~Boolean() {}
    };

    class Numeric : public BuiltIn {
    public:
        Numeric() {}
        virtual ~Numeric() {}
    };

    class Integer : public Numeric {
    public:
        Integer() {}
        virtual ~Integer() {}
    };

    class Float : public Numeric {
    public:
        Float() {}
        virtual ~Float() {}
    };

    class SpecialNumber : public Numeric {
    public:
        enum SpecialNumberType {
            NaN,
            Infinity
        };

        SpecialNumber(SpecialNumberType type) : type(type) {}
        virtual ~SpecialNumber() {}

        SpecialNumberType getType() const {
            return type;
        }

        void setType(SpecialNumberType type) {
            this->type = type;
        }

    protected:
        SpecialNumberType type;
    };

    class String : public BuiltIn {
    public:
        enum StringType {
            Basic,
            MultiLineBasic,
            Literal,
            MultiLineLiteral
        };

        String(StringType type) : type(type) {}
        virtual ~String() {}

        StringType getType() const {
            return type;
        }

        void setType(StringType type) {
            this->type = type;
        }

    protected:
        StringType type;
    };

    class DateTime : public BuiltIn {
    public:
        enum DateTimeType {
            OffsetDateTime,
            LocalDateTime,
            LocalDate,
            LocalTime
        };

        DateTime(DateTimeType type) : type(type) {}
        virtual ~DateTime() {}

        DateTimeType getType() const {
            return type;
        }

        void setType(DateTimeType type) {
            this->type = type;
        }

    protected:
        DateTimeType type;
    };
};

#endif
