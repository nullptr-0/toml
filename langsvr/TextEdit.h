#include <vector>
#include <string>
#include <algorithm>
#include <sstream>

struct Position {
    size_t line;
    size_t character;

    Position(size_t line = 0, size_t character = 0) : line(line), character(character) {}

    bool operator==(const Position& other) const {
        return line == other.line && character == other.character;
    }
};

struct Range {
    Position start;
    Position end;

    Range(Position start = Position(), Position end = Position()) : start(start), end(end) {}

    bool operator==(const Range& other) const {
        return start == other.start && end == other.end;
    }
};

struct TextEdit {
    Range range;
    std::string newText;
};

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    if (text.empty() || text.back() == '\n') {
        lines.push_back("");
    }
    return lines;
}

Position offsetToPosition(size_t offset, const std::vector<std::string>& lines) {
    size_t line = 0;
    while (line < lines.size()) {
        size_t lineLength = lines[line].size() + 1; // +1 for the newline character
        if (offset < lineLength) {
            return Position(line, offset);
        }
        offset -= lineLength;
        ++line;
    }
    return Position(line, 0);
}

std::vector<TextEdit> computeEdits(const std::string& original, const std::string& modified) {
    std::vector<TextEdit> edits;

    std::vector<std::string> origLines = splitLines(original);
    std::vector<std::string> modLines = splitLines(modified);

    size_t o = 0, m = 0;
    while (o < origLines.size() || m < modLines.size()) {
        if (o < origLines.size() && m < modLines.size() && origLines[o] == modLines[m]) {
            ++o;
            ++m;
        }
        else {
            // Find the end of the current diff hunk
            size_t origStart = o;
            size_t modStart = m;

            // Find the end of the diff hunk in original and modified
            while (o < origLines.size() && (m >= modLines.size() || origLines[o] != modLines[m])) ++o;
            while (m < modLines.size() && (o >= origLines.size() || modLines[m] != origLines[o])) ++m;

            size_t origEnd = o;
            size_t modEnd = m;

            // Convert line numbers to offsets in the original text
            size_t originalStartOffset = 0;
            for (size_t i = 0; i < origStart; ++i) {
                originalStartOffset += origLines[i].size() + 1; // +1 for newline
            }
            size_t originalEndOffset = originalStartOffset;
            for (size_t i = origStart; i < origEnd; ++i) {
                originalEndOffset += origLines[i].size() + 1;
            }

            // Convert the modified lines to the new text
            std::string newText;
            for (size_t i = modStart; i < modEnd; ++i) {
                newText += modLines[i];
                if (i != modEnd - 1) {
                    newText += "\n";
                }
            }

            // Create the text edit
            TextEdit edit;
            edit.range.start = offsetToPosition(originalStartOffset, origLines);
            edit.range.end = offsetToPosition(originalEndOffset, origLines);
            edit.newText = newText;
            edits.push_back(edit);
        }
    }

    return edits;
}