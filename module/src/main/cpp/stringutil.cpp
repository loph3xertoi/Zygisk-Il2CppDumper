//
// Created by daw on 4/23/25.
//

#include "stringutil.h"


/** splits str into vector of substrings, str is not changed */
std::vector<std::string> StringSplit(std::string str, const std::string &delim) {
    std::vector<std::string> res;
    size_t pos;
    while ((pos = str.find(delim)) != std::string::npos) {
        res.emplace_back(str.substr(0, pos));
        str.erase(0, pos + delim.length());
    }
    res.emplace_back(str);
    return res;
}


/** joins a vector of strings into a single string */
std::string StringJoin(const std::vector<std::string> &strs, const std::string &delim) {
    if (strs.empty()) return "";
    std::vector<char> res;
    for (int i = 0; i < strs.size() - 1; ++i) {
        for (auto c: strs[i]) res.emplace_back(c);
        for (auto c: delim) res.emplace_back(c);
    }
    for (auto c: strs[strs.size() - 1]) res.emplace_back(c);
    return std::string{res.begin(), res.end()};
}

// Encode a input string with any bytes to a valid c++ identifier name.
std::string encodeToVariableName(const std::string &input) {
    static const char hex_digits[] = "0123456789abcdef";
    std::string output = "var_";  // Ensure it starts with a letter

    for (unsigned char c: input) {
        output += hex_digits[c >> 4];
        output += hex_digits[c & 0xF];
    }

    return output;
}