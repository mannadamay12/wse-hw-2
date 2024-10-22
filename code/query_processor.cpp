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
#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "varbyte.h"
#include "tokenizer.h"

// Structure for Lexicon Entry
struct LexiconEntry {
    uint64_t docid_offset;
    size_t docid_length;
    uint64_t freq_offset;
    size_t freq_length;
    size_t doc_freq; // Number of documents containing the term
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

// Function to load page table
bool load_page_table(const std::string& page_table_file, std::unordered_map<uint32_t, DocumentInfo>& page_table) {
    std::ifstream infile(page_table_file);
    if(!infile.is_open()) {
        std::cerr << "Error: Failed to open page table file: " << page_table_file << std::endl;
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

#ifdef __linux__
// Function to get total CPU time (user + system) in clock ticks
long get_total_cpu_time() {
    std::ifstream stat_file("/proc/stat");
    if(!stat_file.is_open()) {
        std::cerr << "Error: Unable to open /proc/stat for CPU time." << std::endl;
        return 0;
    }
    std::string cpu_label;
    long user, nice, system, idle, iowait, irq, softirq, steal;
    stat_file >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
    stat_file.close();
    return user + nice + system + irq + softirq + steal;
}

// Function to get process CPU time (user + system) in clock ticks
long get_process_cpu_time() {
    std::ifstream proc_stat("/proc/self/stat");
    if(!proc_stat.is_open()) {
        std::cerr << "Error: Unable to open /proc/self/stat for process CPU time." << std::endl;
        return 0;
    }
    std::string ignore;
    long utime, stime;
    for(int i = 0; i < 13; ++i) proc_stat >> ignore; // Skip first 13 fields
    proc_stat >> utime >> stime;
    proc_stat.close();
    return utime + stime;
}

// Function to get current memory usage in KB
long get_memory_usage_kb() {
    std::ifstream status_file("/proc/self/status");
    if(!status_file.is_open()) {
        std::cerr << "Error: Unable to open /proc/self/status for memory usage." << std::endl;
        return 0;
    }
    std::string line;
    long vmrss = 0;
    while (std::getline(status_file, line)) {
        if(line.find("VmRSS:") == 0) {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> vmrss; // Label and value
            break;
        }
    }
    status_file.close();
    return vmrss; // in KB
}
#endif

int main(int argc, char* argv[]) {
    // Usage:
    // ./query_processor <final_index.bin> <lexicon.txt> <page_table.txt> <passages.bin> <doc_lengths.txt> <avgdl.txt>
    if(argc < 7) {
        std::cerr << "Usage: " << argv[0] << " <final_index.bin> <lexicon.txt> <page_table.txt> <passages.bin> <doc_lengths.txt> <avgdl.txt>" << std::endl;
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

    // Load avgdl
    std::ifstream avgdl_ifs(avgdl_file);
    if(!avgdl_ifs.is_open()) {
        std::cerr << "Error: Failed to open avgdl file: " << avgdl_file << std::endl;
        return 1;
    }

    double avgdl;
    avgdl_ifs >> avgdl;
    avgdl_ifs.close();
    std::cout << "Average Document Length (avgdl) loaded: " << avgdl << std::endl;

    // Determine total number of documents
    uint32_t total_docs = doc_lengths.size();
    std::cout << "Total Documents: " << total_docs << std::endl;

    // Open the inverted index file
    std::ifstream index_file(final_index_file, std::ios::binary);
    if(!index_file.is_open()) {
        std::cerr << "Error: Failed to open final inverted index file: " << final_index_file << std::endl;
        return 1;
    }

    // Open passages.bin for reading
    std::ifstream passages_file(passages_bin_file, std::ios::binary);
    if(!passages_file.is_open()) {
        std::cerr << "Error: Failed to open passages.bin file: " << passages_bin_file << std::endl;
        return 1;
    }

    // Query processing loop
    std::string query;
    while(true) {
        // Select query mode
        int mode = 0;
        while (mode != 1 && mode != 2) {
            std::cout << "Select query mode (1 for conjunctive, 2 for disjunctive): ";
            std::cin >> mode;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear input buffer
            if (mode != 1 && mode != 2) {
                std::cout << "Invalid mode selected. Please enter 1 or 2." << std::endl;
            }
        }

        std::cout << "Enter query (or type 'exit' to quit): ";
        std::getline(std::cin, query);
        if(query == "exit") break;
        if(query.empty()) continue;

#ifdef __linux__
        long total_cpu_start = get_total_cpu_time();
        long process_cpu_start = get_process_cpu_time();
        long memory_start = get_memory_usage_kb();
#endif

        auto query_start_time = std::chrono::high_resolution_clock::now(); // Start timing

        // Tokenize query
        std::vector<std::string> terms = tokenize(query);

        // Convert all terms to lowercase
        for(auto &term : terms) {
            term = to_lowercase(term);
        }

        if(terms.empty()) {
            std::cout << "No valid terms in query." << std::endl;
#ifdef __linux__
            long total_cpu_end = get_total_cpu_time();
            long process_cpu_end = get_process_cpu_time();
            long memory_end = get_memory_usage_kb();

            long total_cpu_diff = total_cpu_end - total_cpu_start;
            long process_cpu_diff = process_cpu_end - process_cpu_start;
            long memory_diff = memory_end - memory_start;

            // Get clock ticks per second
            long ticks_per_sec = sysconf(_SC_CLK_TCK);

            double cpu_usage = (total_cpu_diff > 0) ? (static_cast<double>(process_cpu_diff) / static_cast<double>(total_cpu_diff)) * 100.0 : 0.0;

            // Calculate elapsed time
            auto query_end_time_now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = query_end_time_now - query_start_time;

            // Display metrics
            std::cout << "Elapsed Time: " << elapsed.count() << " seconds." << std::endl;
            std::cout << "CPU Usage: " << cpu_usage << " %" << std::endl;
            std::cout << "Memory Usage Change: " << memory_diff << " KB." << std::endl;
#endif
            std::cout << std::endl;
            continue;
        }

        // Retrieve postings for each term
        std::unordered_map<std::string, std::vector<uint32_t>> term_doc_ids; // term -> list of docIDs
        std::unordered_map<std::string, std::vector<uint32_t>> term_freqs;   // term -> list of frequencies
        std::unordered_map<std::string, size_t> term_positions;              // term -> current position in postings

        for(const auto& term : terms) {
            auto it = lexicon.find(term);
            if(it == lexicon.end()) {
                // Term not found in lexicon
                std::cout << "Term '" << term << "' not found in lexicon." << std::endl;
                continue;
            }

            LexiconEntry entry = it->second;

            // Read encoded docIDs
            std::vector<uint8_t> encoded_docids(entry.docid_length);
            index_file.seekg(entry.docid_offset, std::ios::beg);
            if(!index_file.read(reinterpret_cast<char*>(encoded_docids.data()), entry.docid_length)) {
                std::cerr << "Error: Failed to read docIDs for term '" << term << "'." << std::endl;
                continue;
            }

            // Read encoded frequencies
            std::vector<uint8_t> encoded_freqs(entry.freq_length);
            index_file.seekg(entry.freq_offset, std::ios::beg);
            if(!index_file.read(reinterpret_cast<char*>(encoded_freqs.data()), entry.freq_length)) {
                std::cerr << "Error: Failed to read frequencies for term '" << term << "'." << std::endl;
                continue;
            }

            // Decode docIDs (gap decoding) and frequencies
            size_t index_pos = 0;
            std::vector<uint32_t> doc_id_gaps;
            try {
                doc_id_gaps = decodeVarByteList(encoded_docids, index_pos, entry.doc_freq);
            } catch(const std::runtime_error& e) {
                std::cerr << "Decoding error for docIDs of term '" << term << "': " << e.what() << std::endl;
                continue;
            }

            // Reconstruct original docIDs from gaps
            std::vector<uint32_t> doc_ids;
            doc_ids.reserve(doc_id_gaps.size());
            uint32_t prev_doc_id = 0;
            for (uint32_t gap : doc_id_gaps) {
                uint32_t doc_id = prev_doc_id + gap;
                doc_ids.push_back(doc_id);
                prev_doc_id = doc_id;
            }

            index_pos = 0;
            std::vector<uint32_t> freqs;
            try {
                freqs = decodeVarByteList(encoded_freqs, index_pos, entry.doc_freq);
            } catch(const std::runtime_error& e) {
                std::cerr << "Decoding error for frequencies of term '" << term << "': " << e.what() << std::endl;
                continue;
            }

            // Ensure doc_ids and freqs are the same size
            if(doc_ids.size() != freqs.size()) {
                std::cerr << "Error: Mismatch between docIDs and frequencies for term: " << term << std::endl;
                continue;
            }

            term_doc_ids[term] = doc_ids;
            term_freqs[term] = freqs;
            term_positions[term] = 0;
        }

        // Capture the end time immediately after retrieving postings
        auto query_end_time_now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = query_end_time_now - query_start_time;

#ifdef __linux__
        // Measure CPU and memory usage after retrieving postings
        long total_cpu_end = get_total_cpu_time();
        long process_cpu_end = get_process_cpu_time();
        long memory_end = get_memory_usage_kb();

        long total_cpu_diff = total_cpu_end - total_cpu_start;
        long process_cpu_diff = process_cpu_end - process_cpu_start;
        long memory_diff = memory_end - memory_start;

        // Calculate CPU usage percentage
        double cpu_usage = (total_cpu_diff > 0) ? (static_cast<double>(process_cpu_diff) / static_cast<double>(total_cpu_diff)) * 100.0 : 0.0;
#endif

        // Check if any terms have postings
        if(term_doc_ids.empty()) {
            std::cout << "No matching documents found." << std::endl;
#ifdef __linux__
            // Display performance metrics even if no results
            std::cout << "Elapsed Time: " << elapsed.count() << " seconds." << std::endl;
            std::cout << "CPU Usage: " << cpu_usage << " %" << std::endl;
            std::cout << "Memory Usage Change: " << memory_diff << " KB." << std::endl;
#endif
            std::cout << std::endl;
            continue;
        }

        // Initialize data structures for DAAT processing
        std::unordered_map<uint32_t, double> doc_scores; // docID -> BM25 score

        // Collect all unique docIDs across all terms
        std::set<uint32_t> all_doc_ids;
        for (const auto& [term, doc_ids] : term_doc_ids) {
            all_doc_ids.insert(doc_ids.begin(), doc_ids.end());
        }

        // Process documents in order
        for(auto doc_id_iter = all_doc_ids.begin(); doc_id_iter != all_doc_ids.end(); ++doc_id_iter) {
            uint32_t current_doc_id = *doc_id_iter;
            double score = 0.0;
            int found_terms = 0;

            for (const auto& term : terms) {
                // Skip terms not found in lexicon
                if (term_doc_ids.find(term) == term_doc_ids.end()) {
                    continue;
                }

                auto& doc_ids = term_doc_ids[term];
                auto& freqs = term_freqs[term];
                auto& pos = term_positions[term];

                // Move the pointer forward if necessary
                while (pos < doc_ids.size() && doc_ids[pos] < current_doc_id) {
                    pos++;
                }

                if (pos < doc_ids.size() && doc_ids[pos] == current_doc_id) {
                    found_terms++;

                    // Retrieve frequency
                    uint32_t freq = freqs[pos];

                    // Retrieve document length
                    auto len_it = doc_lengths.find(current_doc_id);
                    if(len_it == doc_lengths.end()) {
                        std::cerr << "Warning: Document length not found for docID: " << current_doc_id << std::endl;
                        continue;
                    }
                    uint32_t doc_length = len_it->second;

                    // Compute BM25 score for this term
                    LexiconEntry entry = lexicon[term];
                    double idf = calculate_idf(total_docs, entry.doc_freq);
                    double denominator = freq + k1 * (1 - b + b * (static_cast<double>(doc_length) / avgdl));
                    double numerator = freq * (k1 + 1);
                    double bm25_component = (denominator != 0) ? (numerator / denominator) : 0.0;
                    double term_score = idf * bm25_component;

                    score += term_score;
                }
            }

            // Apply query mode filtering
            if ((mode == 1 && found_terms == terms.size()) || (mode == 2 && found_terms > 0)) {
                doc_scores[current_doc_id] = score;
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
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << std::fixed << std::setprecision(4) << score << " | Passage: [Not Found]" << std::endl;
                continue;
            }

            uint64_t offset = it->second.passage_offset;
            size_t length = it->second.passage_length;

            // Seek to the passage in passages.bin
            passages_file.seekg(offset, std::ios::beg);
            if(passages_file.fail()) {
                std::cerr << "Error: Failed to seek to passage for docID: " << docID << std::endl;
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << std::fixed << std::setprecision(4) << score << " | Passage: [Seek Failed]" << std::endl;
                continue;
            }

            // Read passage length (first 4 bytes as uint32_t)
            uint32_t passage_length;
            passages_file.read(reinterpret_cast<char*>(&passage_length), sizeof(uint32_t));
            if(passages_file.fail()) {
                std::cerr << "Error: Failed to read passage length for docID: " << docID << std::endl;
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << std::fixed << std::setprecision(4) << score << " | Passage: [Read Failed]" << std::endl;
                continue;
            }

            // Validate passage_length
            if(passage_length == 0 || passage_length > length) {
                std::cerr << "Warning: Invalid passage length for docID: " << docID << std::endl;
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << std::fixed << std::setprecision(4) << score << " | Passage: [Invalid Length]" << std::endl;
                continue;
            }

            // Read passage characters based on byte length
            std::vector<char> passage_chars(passage_length);
            passages_file.read(reinterpret_cast<char*>(passage_chars.data()), passage_length);
            if(passages_file.fail()) {
                std::cerr << "Error: Failed to read passage content for docID: " << docID << std::endl;
                std::cout << i+1 << ". DocID: " << docID << " | Score: " << std::fixed << std::setprecision(4) << score << " | Passage: [Content Read Failed]" << std::endl;
                continue;
            }
            std::string passage(passage_chars.begin(), passage_chars.end());

            // Output formatting
            std::cout << std::fixed << std::setprecision(4);
            std::cout << i+1 << ". DocID: " << docID << " | Score: " << score << "\nPassage: " << passage << "\n" << std::endl;
        }

        if(ranked_docs.empty()) {
            std::cout << "No matching documents found." << std::endl;
        }

        // Display performance metrics
#ifdef __linux__
        std::cout << "Elapsed Time: " << elapsed.count() << " seconds." << std::endl;
        std::cout << "CPU Usage: " << cpu_usage << " %" << std::endl;
        std::cout << "Memory Usage Change: " << memory_diff << " KB." << std::endl;
#endif

        std::cout << std::endl;

        // Reset the stream state for the next query
        index_file.clear();
    }

    // Close files before exiting
    index_file.close();
    passages_file.close();

    return 0;
}
