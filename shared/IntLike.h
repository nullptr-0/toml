#pragma once

#ifndef INT_LIKE_H
#define INT_LIKE_H

#include <iostream>
#include <limits>

struct IntLikeHasher;

class IntLike {
protected:
    size_t value; // Stores the absolute value
    bool isNegative; // Stores the sign

public:
    // Constructors
    IntLike(size_t num, bool isNeg) {
        isNegative = isNeg;
        value = static_cast<size_t>(num);
    }

    IntLike(long long num = 0) {
        if (num < 0) {
            isNegative = true;
            value = static_cast<size_t>(-num);
        }
        else {
            isNegative = false;
            value = static_cast<size_t>(num);
        }
    }

    // Conversion operator
    //operator int() const {
    //    return isNegative ? -static_cast<int>(value) : static_cast<int>(value);
    //}

    // Arithmetic operators
    IntLike operator+(const IntLike& other) const {
        if (this->isNegative + other.isNegative == 1) {
            if (this->value > other.value) {
                return IntLike(this->value - other.value, this->isNegative);
            }
            else if (this->value < other.value) {
                return IntLike(other.value - this->value, other.isNegative);
            }
            else {
                return IntLike(0);
            }
        }
        else {
            return IntLike(this->value + other.value, this->isNegative);
        }
    }

    IntLike operator+() const {
        return *this;
    }

    IntLike operator-(const IntLike& other) const {
        return *this + IntLike(other.value, !other.isNegative);
    }

    IntLike operator-() const {
        return IntLike(this->value, !this->isNegative);
    }

    IntLike operator*(const IntLike& other) const {
        return IntLike(this->value * other.value, this->isNegative + other.isNegative == 1);
    }

    IntLike operator/(const IntLike& other) const {
        if (other.value == 0) throw std::runtime_error("Division by zero");
        return IntLike(this->value / other.value, this->isNegative + other.isNegative == 1);
    }

    // Comparison operators
    bool operator==(const IntLike& other) const {
        return this->value == other.value && this->isNegative == other.isNegative;
    }

    bool operator!=(const IntLike& other) const {
        return !(*this == other);
    }

    bool operator<(const IntLike& other) const {
        if (this->isNegative && !other.isNegative) {
            return true;
        }
        else if (!this->isNegative && other.isNegative) {
            return false;
        }
        else if (this->isNegative && other.isNegative) {
            return this->value > other.value;
        }
        else {
            return this->value < other.value;
        }
    }

    bool operator>(const IntLike& other) const {
        return !(*this < other || *this == other);
    }

    bool operator<=(const IntLike& other) const {
        return !(*this > other);
    }

    bool operator>=(const IntLike& other) const {
        return !(*this < other);
    }

    // Assignment operators
    IntLike& operator+=(const IntLike& other) {
        *this = *this + other;
        return *this;
    }

    IntLike& operator-=(const IntLike& other) {
        *this = *this - other;
        return *this;
    }

    IntLike& operator*=(const IntLike& other) {
        *this = *this * other;
        return *this;
    }

    IntLike& operator/=(const IntLike& other) {
        *this = *this / other;
        return *this;
    }

    IntLike& operator++() {
        *this += IntLike(1);
        return *this;
    }
    
    IntLike& operator--() {
        *this -= IntLike(1);
        return *this;
    }

	IntLike operator++(int) {
		IntLike temp = *this;
		++(*this);
		return temp;
	}

    IntLike operator--(int) {
        IntLike temp = *this;
        --(*this);
        return temp;
    }

    size_t getValue() const {
        return value;
    }

    bool isNegativeValue() const {
        return isNegative;
    }

    // Output operator
    friend std::ostream& operator<<(std::ostream& os, const IntLike& obj) {
        if (obj.isNegative) os << "-";
        os << obj.value;
        return os;
    }

    friend struct IntLikeHasher;
};

struct IntLikeHasher {
    size_t operator()(const IntLike& value) const {
        return
            std::hash<bool>{}(value.isNegative) ^
            std::hash<size_t>{}(value.value);
    }
};

#endif // INT_LIKE_H
