// query_processor.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <limits>
#include "varbyte.h"
#include "tokenizer.h"

struct LexiconEntry {
    uint64_t docid_offset;
    size_t docid_length;
    uint64_t freq_offset;
    size_t freq_length;
};

struct DocumentInfo {
    uint32_t docID;
    std::string url;
    // Add other metadata if needed (e.g., passage text)
};

// BM25 Parameters
const double k1 = 1.5;
const double b = 0.75;

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
    while(infile >> term >> docid_offset >> docid_length >> freq_length) {
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

// Function to load page table
bool load_page_table(const std::string& page_table_file, std::unordered_map<uint32_t, DocumentInfo>& page_table) {
    std::ifstream infile(page_table_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open page table file: " << page_table_file << std::endl;
        return false;
    }

    uint32_t docID;
    std::string url;
    while(std::getline(infile, url)) {
        std::istringstream iss(url);
        if(!(iss >> docID >> url)) {
            std::cerr << "Invalid page table entry: " << url << std::endl;
            continue;
        }
        DocumentInfo doc_info;
        doc_info.docID = docID;
        doc_info.url = url;
        page_table[docID] = doc_info;
    }

    infile.close();
    return true;
}

// Function to calculate IDF
double calculate_idf(uint32_t total_docs, uint32_t doc_freq) {
    return log((static_cast<double>(total_docs) - doc_freq + 0.5) / (doc_freq + 0.5) + 1);
}

// Function to tokenize query
std::vector<std::string> tokenize_query(const std::string& query) {
    return tokenize(query);
}

int main(int argc, char* argv[]) {
    // Example usage:
    // ./query_processor final_index.bin lexicon.txt page_table.txt total_docs avgdl
    if(argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <final_index> <lexicon_file> <page_table_file> <total_docs> <avgdl>" << std::endl;
        return 1;
    }

    std::string final_index_file = argv[1];
    std::string lexicon_file = argv[2];
    std::string page_table_file = argv[3];
    uint32_t total_docs = std::stoul(argv[4]);
    double avgdl = std::stod(argv[5]);

    // Load lexicon
    std::unordered_map<std::string, LexiconEntry> lexicon;
    if(!load_lexicon(lexicon_file, lexicon)) {
        return 1;
    }

    // Load page table
    std::unordered_map<uint32_t, DocumentInfo> page_table;
    if(!load_page_table(page_table_file, page_table)) {
        return 1;
    }

    // Open the inverted index file
    std::ifstream index_file(final_index_file, std::ios::binary);
    if(!index_file.is_open()) {
        std::cerr << "Failed to open final inverted index file: " << final_index_file << std::endl;
        return 1;
    }

    // Query processing loop
    std::string query;
    while(true) {
        std::cout << "Enter query (or type 'exit' to quit): ";
        std::getline(std::cin, query);
        if(query == "exit") break;
        if(query.empty()) continue;

        // Tokenize query
        std::vector<std::string> terms = tokenize_query(query);
        if(terms.empty()) {
            std::cout << "No valid terms in query." << std::endl;
            continue;
        }

        // Retrieve postings for each term
        struct Posting {
            uint32_t docID;
            uint32_t freq;
        };

        std::unordered_map<uint32_t, double> doc_scores; // docID -> BM25 score
        std::unordered_map<std::string, uint32_t> term_doc_freqs; // term -> doc frequency

        for(const auto& term : terms) {
            auto it = lexicon.find(term);
            if(it == lexicon.end()) {
                // Term not found in lexicon
                continue;
            }

            LexiconEntry entry = it->second;

            // Read encoded docIDs
            std::vector<uint8_t> encoded_docIDs(entry.docid_length);
            index_file.seekg(entry.docid_offset, std::ios::beg);
            index_file.read(reinterpret_cast<char*>(encoded_docIDs.data()), entry.docid_length);

            // Read encoded frequencies
            std::vector<uint8_t> encoded_freqs(entry.freq_length);
            index_file.seekg(entry.freq_offset, std::ios::beg);
            index_file.read(reinterpret_cast<char*>(encoded_freqs.data()), entry.freq_length);

            // Decode docIDs and frequencies
            size_t index_pos = 0;
            std::vector<uint32_t> doc_ids = decodeVarByteList(encoded_docIDs, index_pos);

            index_pos = 0;
            std::vector<uint32_t> freqs = decodeVarByteList(encoded_freqs, index_pos);

            // Ensure doc_ids and freqs are the same size
            if(doc_ids.size() != freqs.size()) {
                std::cerr << "Mismatch between docIDs and frequencies for term: " << term << std::endl;
                continue;
            }

            // Update BM25 scores
            uint32_t doc_freq = doc_ids.size();
            term_doc_freqs[term] = doc_freq;
            double idf = calculate_idf(total_docs, doc_freq);
            for(size_t i = 0; i < doc_ids.size(); ++i) {
                uint32_t docID = doc_ids[i];
                uint32_t freq = freqs[i];
                // Placeholder: Assume document length is avgdl (replace with actual document length)
                // To accurately calculate BM25, you need document lengths. For this assignment, we'll proceed with avgdl.
                double score = idf * ((freq * (k1 + 1)) / (freq + k1 * (1 - b + b * (avgdl / avgdl))));
                doc_scores[docID] += score;
            }
        }

        // Rank documents by BM25 score
        std::vector<std::pair<uint32_t, double>> ranked_docs(doc_scores.begin(), doc_scores.end());
        std::sort(ranked_docs.begin(), ranked_docs.end(),
                  [](const std::pair<uint32_t, double>& a, const std::pair<uint32_t, double>& b) -> bool {
                      return a.second > b.second;
                  });

        // Display top-k results
        int k = 10; // Top 10 results
        std::cout << "Top " << k << " results:" << std::endl;
        for(int i = 0; i < std::min(k, static_cast<int>(ranked_docs.size())); ++i) {
            uint32_t docID = ranked_docs[i].first;
            double score = ranked_docs[i].second;
            auto it = page_table.find(docID);
            std::string url = (it != page_table.end()) ? it->second.url : "Unknown URL";
            std::cout << i+1 << ". DocID: " << docID << " | Score: " << score << " | URL: " << url << std::endl;
        }

        if(ranked_docs.empty()) {
            std::cout << "No matching documents found." << std::endl;
        }

        // Reset the stream state for the next query
        index_file.clear();
    }

    index_file.close();
    return 0;
}
