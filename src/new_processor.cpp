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
#include <iomanip>
#include <set>
#include <chrono>
#include "/Users/ad12/Documents/Develop/wse-hw-2/include/varbyte.h"
#include "/Users/ad12/Documents/Develop/wse-hw-2/include/tokenizer.h"

// Structure for Lexicon Entry
struct LexiconEntry {
    uint64_t docid_offset;
    size_t docid_length;
    uint64_t freq_offset;
    size_t freq_length;
    size_t doc_freq;
};

// Structure for Document Information
struct DocumentInfo {
    uint32_t docID;
    uint64_t passage_offset;
    size_t passage_length;
};

// Structure for Query
struct Query {
    std::string qid;
    std::string text;
};

// BM25 Parameters
const double k1 = 1.5;
const double b = 0.75;

// Function to calculate IDF
double calculate_idf(uint32_t total_docs, uint32_t doc_freq) {
    return log((static_cast<double>(total_docs) - doc_freq + 0.5) / (doc_freq + 0.5) + 1);
}

// Function to load lexicon
bool load_lexicon(const std::string& lexicon_file, std::unordered_map<std::string, LexiconEntry>& lexicon) {
    std::ifstream infile(lexicon_file);
    if(!infile.is_open()) {
        std::cerr << "Error: Failed to open lexicon file: " << lexicon_file << std::endl;
        return false;
    }

    std::string term;
    uint64_t docid_offset, freq_offset;
    size_t docid_length, freq_length, doc_freq;
    while(infile >> term >> docid_offset >> docid_length >> freq_offset >> freq_length >> doc_freq) {
        LexiconEntry entry;
        entry.docid_offset = docid_offset;
        entry.docid_length = docid_length;
        entry.freq_offset = freq_offset;
        entry.freq_length = freq_length;
        entry.doc_freq = doc_freq;
        lexicon[term] = entry;
    }

    infile.close();
    return true;
}

// Function to load document lengths
bool load_doc_lengths(const std::string& doc_lengths_file, std::unordered_map<uint32_t, uint32_t>& doc_lengths) {
    std::ifstream infile(doc_lengths_file);
    if(!infile.is_open()) {
        std::cerr << "Error: Failed to open document lengths file: " << doc_lengths_file << std::endl;
        return false;
    }

    uint32_t docID, length;
    while(infile >> docID >> length) {
        doc_lengths[docID] = length;
    }

    infile.close();
    return true;
}

// Function to read queries from TSV
std::vector<Query> read_queries(const std::string& query_file) {
    std::vector<Query> queries;
    std::ifstream infile(query_file);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open query file: " + query_file);
    }

    std::string line;
    // Skip header if present
    std::getline(infile, line);
    
    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        std::string qid, text;
        
        if (std::getline(iss, qid, '\t') && std::getline(iss, text)) {
            queries.push_back({qid, text});
        }
    }
    return queries;
}

// Function to write results in TREC format
void write_trec_results(std::ofstream& out, 
                       const std::string& qid,
                       const std::vector<std::pair<uint32_t, double>>& ranked_docs,
                       size_t max_results = 1000) {
    for (size_t i = 0; i < std::min(ranked_docs.size(), max_results); i++) {
        out << qid << " Q0 " 
            << ranked_docs[i].first << " "
            << (i + 1) << " "
            << std::fixed << std::setprecision(6) 
            << ranked_docs[i].second << " "
            << "BM25" << std::endl;
    }
}

// Function to process a single query
std::vector<std::pair<uint32_t, double>> process_query(
    const std::string& query_text,
    std::ifstream& index_file,
    const std::unordered_map<std::string, LexiconEntry>& lexicon,
    const std::unordered_map<uint32_t, uint32_t>& doc_lengths,
    double avgdl,
    uint32_t total_docs) {

    std::vector<std::string> terms = tokenize(query_text);
    std::unordered_map<uint32_t, double> doc_scores;

    // Process each query term
    for(const auto& term : terms) {
        auto it = lexicon.find(term);
        if(it == lexicon.end()) continue;

        const LexiconEntry& entry = it->second;
        double idf = calculate_idf(total_docs, entry.doc_freq);

        // Read encoded docIDs
        std::vector<uint8_t> encoded_docids(entry.docid_length);
        index_file.seekg(entry.docid_offset);
        if(!index_file.read(reinterpret_cast<char*>(encoded_docids.data()), entry.docid_length)) {
            continue;
        }

        // Read encoded frequencies
        std::vector<uint8_t> encoded_freqs(entry.freq_length);
        index_file.seekg(entry.freq_offset);
        if(!index_file.read(reinterpret_cast<char*>(encoded_freqs.data()), entry.freq_length)) {
            continue;
        }

        // Decode docIDs and frequencies
        size_t pos = 0;
        std::vector<uint32_t> doc_id_gaps = decodeVarByteList(encoded_docids, pos, entry.doc_freq);
        pos = 0;
        std::vector<uint32_t> freqs = decodeVarByteList(encoded_freqs, pos, entry.doc_freq);

        // Reconstruct docIDs from gaps and calculate scores
        uint32_t current_doc_id = 0;
        for(size_t i = 0; i < doc_id_gaps.size(); i++) {
            current_doc_id += doc_id_gaps[i];
            uint32_t freq = freqs[i];

            auto doc_length_it = doc_lengths.find(current_doc_id);
            if(doc_length_it == doc_lengths.end()) continue;

            uint32_t doc_length = doc_length_it->second;
            double numerator = freq * (k1 + 1);
            double denominator = freq + k1 * (1 - b + b * doc_length / avgdl);
            double score = idf * numerator / denominator;

            doc_scores[current_doc_id] += score;
        }
    }

    // Convert scores map to vector for sorting
    std::vector<std::pair<uint32_t, double>> ranked_docs(doc_scores.begin(), doc_scores.end());
    std::sort(ranked_docs.begin(), ranked_docs.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    return ranked_docs;
}

int main(int argc, char* argv[]) {
    if(argc < 9) {
        std::cerr << "Usage: " << argv[0] 
                  << " <final_index.bin> <lexicon.txt> <page_table.txt> "
                  << "<passages.bin> <doc_lengths.txt> <avgdl.txt> "
                  << "<queries.tsv> <output.trec>" << std::endl;
        return 1;
    }

    std::string final_index_file = argv[1];
    std::string lexicon_file = argv[2];
    std::string page_table_file = argv[3];
    std::string passages_bin_file = argv[4];
    std::string doc_lengths_file = argv[5];
    std::string avgdl_file = argv[6];
    std::string queries_file = argv[7];
    std::string output_file = argv[8];

    // Load lexicon
    std::unordered_map<std::string, LexiconEntry> lexicon;
    if(!load_lexicon(lexicon_file, lexicon)) {
        return 1;
    }
    std::cout << "Lexicon loaded with " << lexicon.size() << " terms." << std::endl;

    // Load document lengths
    std::unordered_map<uint32_t, uint32_t> doc_lengths;
    if(!load_doc_lengths(doc_lengths_file, doc_lengths)) {
        return 1;
    }
    std::cout << "Document lengths loaded with " << doc_lengths.size() << " entries." << std::endl;

    // Load avgdl
    std::ifstream avgdl_ifs(avgdl_file);
    if(!avgdl_ifs.is_open()) {
        std::cerr << "Error: Failed to open avgdl file: " << avgdl_file << std::endl;
        return 1;
    }
    double avgdl;
    avgdl_ifs >> avgdl;
    avgdl_ifs.close();

    // Open the inverted index file
    std::ifstream index_file(final_index_file, std::ios::binary);
    if(!index_file.is_open()) {
        std::cerr << "Error: Failed to open final inverted index file: " << final_index_file << std::endl;
        return 1;
    }

    try {
        // Load queries
        auto queries = read_queries(queries_file);
        std::cout << "Loaded " << queries.size() << " queries." << std::endl;

        // Open output file
        std::ofstream trec_out(output_file);
        if (!trec_out.is_open()) {
            throw std::runtime_error("Failed to create output file: " + output_file);
        }

        // Process each query
        size_t query_count = 0;
        uint32_t total_docs = doc_lengths.size();

        for (const auto& query : queries) {
            // Process query
            auto ranked_docs = process_query(query.text, index_file, lexicon, doc_lengths, avgdl, total_docs);
            
            // Write results
            write_trec_results(trec_out, query.qid, ranked_docs);

            // Show progress
            query_count++;
            if (query_count % 100 == 0) {
                std::cout << "Processed " << query_count << "/" 
                         << queries.size() << " queries\r" << std::flush;
            }
        }

        std::cout << "\nDone! Processed " << query_count << " queries." << std::endl;
        trec_out.close();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    index_file.close();
    return 0;
}