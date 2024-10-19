#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include "varbyte.h"

namespace fs = std::filesystem;

// structure to hold term and its postings from a file
struct TermPostings {
    std::string term;
    std::vector<std::pair<uint32_t, uint32_t>> postings; // (docID, freq)
    int file_id; // identifier for the originating file
};

// comparator for the priority queue (min-heap based on term)
struct Compare {
    bool operator()(const TermPostings& a, const TermPostings& b) {
        return a.term > b.term; // min-heap
    }
};

// read the next term-postings from a file
bool read_next_term(std::ifstream& infile, TermPostings& tp) {
    std::string term;
    if(!(infile >> term)) return false;

    tp.term = term;
    tp.postings.clear();

    std::string token;
    uint32_t docID, freq;
    while(infile >> docID >> freq) {
        tp.postings.emplace_back(docID, freq);
        // peek to check if next token is a new term or end of line
        char c = infile.peek();
        if(c == '\n' || c == EOF) break;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <intermediate_files...> <final_index> <lexicon_file>" << std::endl;
        return 1;
    }

    std::ofstream final_index(argv[2], std::ios::binary);
    if (!final_index) {
        std::cerr << "Error: Unable to open file for writing: " << argv[2] << std::endl;
        return 1;
    }

    std::ofstream lexicon_file(argv[3]);
    if (!lexicon_file) {
        std::cerr << "Error: Unable to open file for writing: " << argv[3] << std::endl;
        return 1;
    }

    int num_files = argc - 2;
    std::vector<std::ifstream> in_files(num_files);
    for(int i = 0; i < num_files; ++i) {
        in_files[i].open(argv[i+1]);
        if(!in_files[i].is_open()) {
            std::cerr << "Failed to open intermediate file: " << argv[i+1] << std::endl;
            return 1;
        }
    }

    // initialize priority queue
    std::priority_queue<TermPostings, std::vector<TermPostings>, Compare> pq;
    for(int i = 0; i < num_files; ++i) {
        TermPostings tp;
        if(read_next_term(in_files[i], tp)) {
            tp.file_id = i;
            pq.push(tp);
        }
    }

    // std::ofstream final_index(argv[num_files +1], std::ios::binary);
    // std::ofstream lexicon_file(argv[num_files +2]);
    // if(!final_index.is_open() || !lexicon_file.is_open()) {
    //     std::cerr << "Failed to open output files." << std::endl;
    //     return 1;
    // }

    std::string current_term = "";
    std::vector<std::pair<uint32_t, uint32_t>> current_postings;
    uint64_t current_offset = 0;

    while(!pq.empty()) {
        TermPostings tp = pq.top();
        pq.pop();

        if(current_term.empty()) {
            current_term = tp.term;
            current_postings = tp.postings;
        }
        else if(tp.term == current_term) {
            // merge postings------------------
            current_postings.insert(current_postings.end(), tp.postings.begin(), tp.postings.end());
        }
        else {
            // sort postings by docID
            std::sort(current_postings.begin(), current_postings.end());

            // separate docIDs and frequencies
            std::vector<uint32_t> doc_ids;
            std::vector<uint32_t> freqs;
            doc_ids.reserve(current_postings.size());
            freqs.reserve(current_postings.size());
            for(const auto& [docID, freq] : current_postings) {
                doc_ids.push_back(docID);
                freqs.push_back(freq);
            }

            // apply VarByte encoding
            std::vector<uint8_t> encoded_docIDs = encodeVarByte(doc_ids);
            std::vector<uint8_t> encoded_freqs = encodeVarByte(freqs);

            final_index.write(reinterpret_cast<char*>(encoded_docIDs.data()), encoded_docIDs.size());
            final_index.write(reinterpret_cast<char*>(encoded_freqs.data()), encoded_freqs.size());

            // update lexicon with term and its offset
            lexicon_file << current_term << "\t" << current_offset << "\t" << encoded_docIDs.size() << "\t" << encoded_freqs.size() << "\n";

            // update current_offset
            current_offset += encoded_docIDs.size() + encoded_freqs.size();

            // start new term
            current_term = tp.term;
            current_postings = tp.postings;
        }

        // sead next term from the corresponding file
        TermPostings next_tp;
        if(read_next_term(in_files[tp.file_id], next_tp)) {
            next_tp.file_id = tp.file_id;
            pq.push(next_tp);
        }
    }

    // write the last term
    if(!current_term.empty()) {
        std::sort(current_postings.begin(), current_postings.end());

        std::vector<uint32_t> doc_ids;
        std::vector<uint32_t> freqs;
        doc_ids.reserve(current_postings.size());
        freqs.reserve(current_postings.size());
        for(const auto& [docID, freq] : current_postings) {
            doc_ids.push_back(docID);
            freqs.push_back(freq);
        }

        std::vector<uint8_t> encoded_docIDs = encodeVarByte(doc_ids);
        std::vector<uint8_t> encoded_freqs = encodeVarByte(freqs);

        final_index.write(reinterpret_cast<char*>(encoded_docIDs.data()), encoded_docIDs.size());
        final_index.write(reinterpret_cast<char*>(encoded_freqs.data()), encoded_freqs.size());

        lexicon_file << current_term << "\t" << current_offset << "\t" << encoded_docIDs.size() << "\t" << encoded_freqs.size() << "\n";

        current_offset += encoded_docIDs.size() + encoded_freqs.size();
    }

    for(auto &f : in_files) f.close();
    final_index.close();
    lexicon_file.close();

    std::cout << "Indexing completed. Final inverted index and lexicon are created." << std::endl;

    return 0;
}