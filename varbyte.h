#ifndef VARBYTE_H
#define VARBYTE_H

#include <vector>
#include <cstdint>
#include <stdexcept>

// Encode a single integer using VarByte encoding
inline void encodeVarByteSingle(uint32_t num, std::vector<uint8_t>& bytes) {
    while (num >= 0x80) {
        bytes.push_back(static_cast<uint8_t>((num & 0x7F) | 0x80)); // Continuation bit in MSB
        num >>= 7;
    }
    bytes.push_back(static_cast<uint8_t>(num));
}

// Encode a list of integers using VarByte encoding
inline void encodeVarByteList(const std::vector<uint32_t>& numbers, std::vector<uint8_t>& encoded) {
    for (auto num : numbers) {
        encodeVarByteSingle(num, encoded);
    }
}

// Decode a single integer from VarByte encoding with error checking
inline uint32_t decodeVarByteSingle(const std::vector<uint8_t>& bytes, size_t& index) {
    uint32_t num = 0;
    uint32_t shift = 0;
    bool has_more = true;
    
    while (index < bytes.size() && has_more) {
        uint8_t byte = bytes[index++];
        num |= (static_cast<uint32_t>(byte & 0x7F)) << shift;
        if (byte & 0x80) {
            shift += 7;
            if (shift > 28) { // Prevent overflow for 32-bit integers
                throw std::runtime_error("VarByte decoding error: shift exceeds 28 bits.");
            }
        } else {
            has_more = false;
        }
    }
    
    if (has_more) {
        throw std::runtime_error("VarByte decoding error: incomplete byte sequence.");
    }
    
    return num;
}

// Decode a list of integers from VarByte encoding with known count
inline std::vector<uint32_t> decodeVarByteList(const std::vector<uint8_t>& bytes, size_t& index, size_t count) {
    std::vector<uint32_t> numbers;
    numbers.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        if (index >= bytes.size()) {
            throw std::runtime_error("VarByte decoding error: not enough bytes to decode the expected count.");
        }
        uint32_t num = decodeVarByteSingle(bytes, index);
        numbers.push_back(num);
    }
    return numbers;
}

// Decode all remaining integers from VarByte encoding
inline std::vector<uint32_t> decodeVarByteAll(const std::vector<uint8_t>& bytes, size_t& index) {
    std::vector<uint32_t> numbers;
    while (index < bytes.size()) {
        uint32_t num = decodeVarByteSingle(bytes, index);
        numbers.push_back(num);
    }
    return numbers;
}

#endif // VARBYTE_H
