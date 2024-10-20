// inspect_inverted_index.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "varbyte.h" // Ensure this header includes VarByte encoding/decoding functions

// Structure for Lexicon Entry
struct LexiconEntry {
    uint64_t docid_offset;
    size_t docid_length;
    uint64_t freq_offset;
    size_t freq_length;
};

// Function to load lexicon
bool load_lexicon(const std::string& lexicon_file, std::unordered_map<std::string, LexiconEntry>& lexicon) {
    std::ifstream infile(lexicon_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open lexicon file: " << lexicon_file << std::endl;
        return false;
    }

    std::string term;
    uint64_t docid_offset, freq_offset;
    size_t docid_length, freq_length;
    while(infile >> term >> docid_offset >> docid_length >> freq_offset >> freq_length) {
        LexiconEntry entry;
        entry.docid_offset = docid_offset;
        entry.docid_length = docid_length;
        entry.freq_offset = freq_offset;
        entry.freq_length = freq_length;
        lexicon[term] = entry;
    }

    infile.close();
    return true;
}

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <final_index.bin> <lexicon.txt> <term_to_inspect>" << std::endl;
        return 1;
    }

    std::string final_index_file = argv[1];
    std::string lexicon_file = argv[2];
    std::string term_to_inspect = argv[3];

    // Load lexicon
    std::unordered_map<std::string, LexiconEntry> lexicon;
    if(!load_lexicon(lexicon_file, lexicon)) {
        return 1;
    }

    // Check if the term exists
    auto it = lexicon.find(term_to_inspect);
    if(it == lexicon.end()) {
        std::cerr << "Term '" << term_to_inspect << "' not found in lexicon." << std::endl;
        return 1;
    }

    LexiconEntry entry = it->second;

    // Open final_index.bin
    std::ifstream index_file(final_index_file, std::ios::binary);
    if(!index_file.is_open()) {
        std::cerr << "Failed to open final index file: " << final_index_file << std::endl;
        return 1;
    }

    // Read encoded docIDs
    std::vector<uint8_t> encoded_docids(entry.docid_length);
    index_file.seekg(entry.docid_offset, std::ios::beg);
    index_file.read(reinterpret_cast<char*>(encoded_docids.data()), entry.docid_length);

    // Read encoded frequencies
    std::vector<uint8_t> encoded_freqs(entry.freq_length);
    index_file.seekg(entry.freq_offset, std::ios::beg);
    index_file.read(reinterpret_cast<char*>(encoded_freqs.data()), entry.freq_length);

    index_file.close();

    // Decode docIDs and frequencies
    size_t index_pos = 0;
    std::vector<uint32_t> doc_ids = decodeVarByteList(encoded_docids, index_pos);

    index_pos = 0;
    std::vector<uint32_t> freqs = decodeVarByteList(encoded_freqs, index_pos);

    // Ensure doc_ids and freqs are the same size
    if(doc_ids.size() != freqs.size()) {
        std::cerr << "Mismatch between docIDs and frequencies for term: " << term_to_inspect << std::endl;
        return 1;
    }

    // Display postings list
    std::cout << "Postings list for term '" << term_to_inspect << "':" << std::endl;
    for(size_t i = 0; i < doc_ids.size(); ++i) {
        std::cout << "DocID: " << doc_ids[i] << " | Freq: " << freqs[i] << std::endl;
    }

    return 0;
}
