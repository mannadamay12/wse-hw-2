// varbyte.h
#ifndef VARBYTE_H
#define VARBYTE_H

#include <vector>
#include <cstdint>

// Function to encode a single integer using VarByte encoding
inline std::vector<uint8_t> encodeVarByte(uint32_t num) {
    std::vector<uint8_t> bytes;
    while (num >= 128) {
        bytes.push_back(static_cast<uint8_t>((num % 128) + 128)); // Set MSB to 1
        num /= 128;
    }
    bytes.push_back(static_cast<uint8_t>(num)); // MSB = 0
    return bytes;
}

// Function to encode a list of integers using VarByte encoding
inline std::vector<uint8_t> encodeVarByte(const std::vector<uint32_t>& numbers) {
    std::vector<uint8_t> encoded;
    for (auto num : numbers) {
        auto bytes = encodeVarByte(num);
        encoded.insert(encoded.end(), bytes.begin(), bytes.end());
    }
    return encoded;
}

// Function to decode a single integer from VarByte encoding
inline uint32_t decodeVarByte(const std::vector<uint8_t>& bytes, size_t& index) {
    uint32_t num = 0;
    while(index < bytes.size()) {
        uint8_t byte = bytes[index++];
        if(byte & 128) { // MSB = 1
            num = (num << 7) | (byte & 127);
        } else { // MSB = 0
            num = (num << 7) | byte;
            break;
        }
    }
    return num;
}

// Function to decode a list of integers from VarByte encoding
inline std::vector<uint32_t> decodeVarByteList(const std::vector<uint8_t>& bytes, size_t& index) {
    std::vector<uint32_t> numbers;
    while(index < bytes.size()) {
        uint32_t num = decodeVarByte(bytes, index);
        numbers.push_back(num);
    }
    return numbers;
}

#endif // VARBYTE_H
