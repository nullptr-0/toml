#pragma once

#ifndef FILE_POSITION_H
#define FILE_POSITION_H

#include "IntLike.h"

namespace FilePosition {
    struct PositionHasher;

    struct Position {
        IntLike line;
        IntLike column;

        bool operator<(const Position& other) const {
            return line < other.line || (line == other.line && column < other.column);
        }

        bool operator<=(const Position& other) const {
            return *this < other || *this == other;
        }

        bool operator>(const Position& other) const {
            return line > other.line || (line == other.line && column > other.column);
        }

        bool operator>=(const Position& other) const {
            return *this > other || *this == other;
        }

        bool operator==(const Position& other) const {
            return line == other.line && column == other.column;
        }

        bool operator!=(const Position& other) const {
            return !(*this == other);
        }

        Position operator+(const Position& delta) const {
            return { line + delta.line, column + delta.column };
        }

        bool contains(const Position& other) const {
            return line < other.line || (line == other.line && column <= other.column);
        }

        friend struct PositionHasher;
    };

    struct PositionHasher {
        size_t operator()(const Position& value) const {
            return
                IntLikeHasher{}(value.line) ^
                IntLikeHasher{}(value.column);
        }
    };

    struct RegionHasher;

    struct Region {
        Position start;
        Position end;

        bool operator==(const Region& other) const {
            return start == other.start && end == other.end;
        }

        bool contains(const Position& position) const {
            return start <= position && position <= end;
        }

        bool contains(const Region& other) const {
            return start <= other.start && other.end <= end;
        }

        bool overlaps(const Region& other) const {
            return !(end < other.start || other.end < start);
        }

        IntLike lineSpan() const { return end.line - start.line + 1; }
        IntLike colSpan() const { return end.column - start.column + 1; }

        friend struct RegionHasher;
    };

    struct RegionHasher {
        size_t operator()(const Region& value) const {
            return
                PositionHasher{}(value.start) ^
                PositionHasher{}(value.end);
        }
    };
}

#endif // FILE_POSITION_H
