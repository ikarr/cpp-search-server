#pragma once
#include <set>
#include <string>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string, std::less<>> SplitIntoStrings(const StringContainer& text) {
    std::set<std::string, std::less<>> strings;
    for (std::string_view string : text) {
        if (!string.empty()) {
            strings.insert(std::string(string));
        }
    }
    return strings;
}