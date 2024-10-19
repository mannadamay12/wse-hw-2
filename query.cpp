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

// Structure for Lexicon Entry
struct LexiconEntry {
    uint64_t docid_offset;
    size_t docid_length;
    uint64_t freq_offset;
    size_t freq_length;
};

// Structure for Document Information
struct DocumentInfo {
    uint32_t docID;
    uint64_t passage_offset;
    size_t passage_length;
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

// Structure for Document Length
struct DocumentLength {
    uint32_t docID;
    uint32_t length;
};

// Function to load document lengths
bool load_doc_lengths(const std::string& doc_lengths_file, std::unordered_map<uint32_t, uint32_t>& doc_lengths) {
    std::ifstream infile(doc_lengths_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open doc_lengths file: " << doc_lengths_file << std::endl;
        return false;
    }

    uint32_t docID;
    uint32_t length;
    while(infile >> docID >> length) {
        doc_lengths[docID] = length;
    }

    infile.close();
    return true;
}

// Function to load page table
bool load_page_table(const std::string& page_table_file, std::unordered_map<uint32_t, DocumentInfo>& page_table) {
    std::ifstream infile(page_table_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open page_table file: " << page_table_file << std::endl;
        return false;
    }

    uint32_t docID;
    uint64_t offset;
    size_t length;
    while(infile >> docID >> offset >> length) {
        DocumentInfo doc_info;
        doc_info.docID = docID;
        doc_info.passage_offset = offset;
        doc_info.passage_length = length;
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
    // Usage:
    // ./query_processor <final_index> <lexicon_file> <page_table_file> <passages_bin> <doc_lengths_file> <avgdl_file>
    if(argc < 7) {
        std::cerr << "Usage: " << argv[0] << " <final_index> <lexicon_file> <page_table_file> <passages_bin> <doc_lengths_file> <avgdl_file>" << std::endl;
        return 1;
    }

    std::string final_index_file = argv[1];
    std::string lexicon_file = argv[2];
    std::string page_table_file = argv[3];
    std::string passages_bin_file = argv[4];
    std::string doc_lengths_file = argv[5];
    std::string avgdl_file = argv[6];

    // Load lexicon
    std::unordered_map<std::string, LexiconEntry> lexicon;
    if(!load_lexicon(lexicon_file, lexicon)) {
        return 1;
    }
    std::cout << "Lexicon loaded with " << lexicon.size() << " terms." << std::endl;

    // Load page table
    std::unordered_map<uint32_t, DocumentInfo> page_table;
    if(!load_page_table(page_table_file, page_table)) {
        return 1;
    }
    std::cout << "Page table loaded with " << page_table.size() << " documents." << std::endl;

    // Load document lengths
    std::unordered_map<uint32_t, uint32_t> doc_lengths;
    if(!load_doc_lengths(doc_lengths_file, doc_lengths)) {
        return 1;
    }
    std::cout << "Document lengths loaded with " << doc_lengths.size() << " entries." << std::endl;

    // Open the inverted index file
    std::ifstream index_file(final_index_file, std::ios::binary);
    if(!index_file.is_open()) {
        std::cerr << "Failed to open final inverted index file: " << final_index_file << std::endl;
        return 1;
    }

    // Open passages.bin for reading
    std::ifstream passages_file(passages_bin_file, std::ios::binary);
    if(!passages_file.is_open()) {
        std::cerr << "Failed to open passages.bin file: " << passages_bin_file << std::endl;
        return 1;
    }

    // Load avgdl
    std::ifstream avgdl_ifs(avgdl_file);
    if(!avgdl_ifs.is_open()) {
        std::cerr << "Failed to open avgdl file: " << avgdl_file << std::endl;
        return 1;
    }

    double avgdl;
    avgdl_ifs >> avgdl;
    avgdl_ifs.close();
    std::cout << "Average Document Length (avgdl) loaded: " << avgdl << std::endl;

    // Placeholder: Total number of documents
    // Ideally, total_docs should be read from the Parser or stored in a separate file
    // For this example, we set it to 8,841,823 as per your dataset
    uint32_t total_docs = 8841823;

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
        std::unordered_map<uint32_t, double> doc_scores; // docID -> BM25 score

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

            // Calculate IDF
            uint32_t doc_freq = doc_ids.size();
            double idf = calculate_idf(total_docs, doc_freq);

            for(size_t i = 0; i < doc_ids.size(); ++i) {
                uint32_t docID = doc_ids[i];
                uint32_t freq = freqs[i];

                // Retrieve document length
                auto len_it = doc_lengths.find(docID);
                if(len_it == doc_lengths.end()) {
                    std::cerr << "Document length not found for docID: " << docID << std::endl;
                    continue;
                }
                uint32_t doc_length = len_it->second;

                // Prevent division by zero
                if(avgdl == 0) {
                    std::cerr << "Average Document Length (avgdl) is zero. Cannot compute BM25." << std::endl;
                    return 1;
                }

                // Compute BM25 score
                double denominator = static_cast<double>(freq) + k1 * (1 - b + b * (static_cast<double>(doc_length) / avgdl));
                double numerator = static_cast<double>(freq) * (k1 + 1);
                double bm25_component = (denominator != 0) ? (numerator / denominator) : 0.0;
                double score = idf * bm25_component;

                // Debugging: Uncomment to see detailed calculations
                /*
                if(docID < 5) { // Adjust condition as needed
                    std::cout << "DocID: " << docID 
                              << " | IDF: " << idf 
                              << " | Freq: " << freq 
                              << " | Doc Length: " << doc_length 
                              << " | Numerator: " << numerator 
                              << " | Denominator: " << denominator 
                              << " | BM25 Component: " << bm25_component 
                              << " | Score: " << score << std::endl;
                }
                */

                // Accumulate score
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

            // Retrieve passage from passages.bin using page_table
            auto it = page_table.find(docID);
            if(it == page_table.end()) {
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << score << " | Passage: [Not Found]" << std::endl;
                continue;
            }

            uint64_t offset = it->second.passage_offset;
            size_t length = it->second.passage_length;

            // Seek to the passage in passages.bin
            passages_file.seekg(offset, std::ios::beg);

            // Read passage length (first 4 bytes as uint32_t)
            uint32_t passage_length;
            passages_file.read(reinterpret_cast<char*>(&passage_length), sizeof(uint32_t));

            // Validate passage_length
            if(passage_length == 0 || passage_length > length) {
                std::cerr << "Invalid passage length for docID: " << docID << std::endl;
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << score << " | Passage: [Invalid Length]" << std::endl;
                continue;
            }

            // Read passage characters
            std::vector<char> passage_chars(passage_length);
            passages_file.read(passage_chars.data(), passage_length);
            std::string passage(passage_chars.begin(), passage_chars.end());

            std::cout << i+1 << ". DocID: " << docID << " | Score: " << score << " | Passage: " << passage << std::endl;
        }

        if(ranked_docs.empty()) {
            std::cout << "No matching documents found." << std::endl;
        }

        // Reset the stream state for the next query
        index_file.clear();
    }

    index_file.close();
    passages_file.close();
    return 0;
}
