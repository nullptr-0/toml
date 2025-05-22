#pragma once

#ifndef FIND_PAIRS_H
#define FIND_PAIRS_H

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <string>

template<typename V>
std::vector<std::pair<std::string, V>> findPairs(const std::unordered_map<std::string, V>& map, const std::string& input) {
    std::vector<std::pair<std::string, V>> result;
    std::unordered_set<char> inputChars;

    for (const auto& [key, val] : map) {
        inputChars.insert(input.begin(), input.end());
        std::vector<char> common;
        for (char c : key) {
            if (inputChars.count(c)) {
                common.push_back(c);
                inputChars.erase(c);
            }
        }
        if (common.empty()) {
            continue;
        }
        // Check if common is a subsequence of input
        size_t ptr = 0;
        bool isSubseq = true;
        for (char c : common) {
            size_t pos = input.find(c, ptr);
            if (pos == std::string::npos) {
                isSubseq = false;
                break;
            }
            ptr = pos + 1;
        }
        if (isSubseq) {
            result.emplace_back(key, val);
        }
    }

    return result;
}

#endif // FIND_PAIRS_H
