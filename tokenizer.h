// tokenizer.h
#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>

// Function to convert string to lowercase
inline std::string to_lowercase(const std::string& str) {
    std::string lower;
    lower.reserve(str.size());
    for(char c : str) {
        lower += std::tolower(c);
    }
    return lower;
}

// Function to remove punctuation from a string
inline std::string remove_punctuation(const std::string& str) {
    std::string clean;
    clean.reserve(str.size());
    for(char c : str) {
        if(!std::ispunct(static_cast<unsigned char>(c))) {
            clean += c;
        }
    }
    return clean;
}

// Function to tokenize a string
inline std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string token;
    std::string cleaned_text = remove_punctuation(to_lowercase(text));

    std::istringstream iss(cleaned_text);
    while(iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

#endif // TOKENIZER_H
