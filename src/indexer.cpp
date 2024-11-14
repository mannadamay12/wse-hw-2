#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <queue>
#include <functional>
#include <memory>
#include <exception>
#include "/Users/ad12/Documents/Develop/wse-hw-2/include/varbyte.h"


using namespace std;

// Structure to hold a term and its postings
struct TermPostings {
    string term;
    vector<pair<uint32_t, uint32_t>> postings; // (docID, freq)
};

// Comparator for priority queue to merge terms lexicographically
struct TermComparator {
    bool operator()(const pair<string, size_t>& a, const pair<string, size_t>& b) const {
        return a.first > b.first; // Min-heap based on term
    }
};

// Function to read the next term and its postings from a file
bool readNextTerm(ifstream& infile, string& term, vector<pair<uint32_t, uint32_t>>& postings) {
    postings.clear();
    string line;
    if (!getline(infile, line)) {
        return false; // End of file
    }

    istringstream iss(line);
    if (!(iss >> term)) {
        cerr << "Invalid line format in intermediate file: " << line << endl;
        return false; // Invalid line
    }

    uint32_t doc_id, freq;
    while (iss >> doc_id >> freq) {
        postings.emplace_back(doc_id, freq);
    }

    if (postings.empty()) {
        cerr << "No postings found for term '" << term << "' in line: " << line << endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <intermediate_file1> [<intermediate_file2> ...] <final_index> <lexicon_file>" << endl;
        return 1;
    }

    // Last two arguments are the final index and lexicon files
    string final_index_file = argv[argc - 2];
    string lexicon_file = argv[argc - 1];

    // Open intermediate files
    size_t num_files = argc - 3;
    vector<ifstream> intermediate_files(num_files);
    for (size_t i = 0; i < num_files; ++i) {
        intermediate_files[i].open(argv[i + 1]);
        if (!intermediate_files[i].is_open()) {
            cerr << "Failed to open intermediate file: " << argv[i + 1] << endl;
            return 1;
        }
    }

    // Initialize the priority queue with the first term from each file
    priority_queue<pair<string, size_t>, vector<pair<string, size_t>>, TermComparator> min_heap;
    vector<string> current_terms(num_files);
    vector<vector<pair<uint32_t, uint32_t>>> current_postings(num_files);

    for (size_t i = 0; i < num_files; ++i) {
        if (readNextTerm(intermediate_files[i], current_terms[i], current_postings[i])) {
            min_heap.emplace(current_terms[i], i);
        }
    }

    // Open final index and lexicon files
    ofstream final_index(final_index_file, ios::binary);
    if (!final_index.is_open()) {
        cerr << "Failed to create final index file: " << final_index_file << endl;
        return 1;
    }

    ofstream lexicon(lexicon_file);
    if (!lexicon.is_open()) {
        cerr << "Failed to create lexicon file: " << lexicon_file << endl;
        return 1;
    }

    uint64_t current_offset = 0;

    while (!min_heap.empty()) {
        // Get the smallest term
        auto [term, file_idx] = min_heap.top();
        min_heap.pop();

        // Collect postings for this term from all files
        vector<pair<uint32_t, uint32_t>> merged_postings = std::move(current_postings[file_idx]);

        // Read the next term from the same file
        if (readNextTerm(intermediate_files[file_idx], current_terms[file_idx], current_postings[file_idx])) {
            min_heap.emplace(current_terms[file_idx], file_idx);
        }

        // Collect postings from other files that have the same term
        while (!min_heap.empty() && min_heap.top().first == term) {
            auto [_, idx] = min_heap.top();
            min_heap.pop();

            // Merge postings
            merged_postings.insert(merged_postings.end(), current_postings[idx].begin(), current_postings[idx].end());

            // Read the next term from this file
            if (readNextTerm(intermediate_files[idx], current_terms[idx], current_postings[idx])) {
                min_heap.emplace(current_terms[idx], idx);
            }
        }

        // Sort merged postings by docID
        sort(merged_postings.begin(), merged_postings.end());

        // Gap encode docIDs
        vector<uint32_t> doc_gaps;
        vector<uint32_t> freqs;
        uint32_t prev_doc_id = 0;
        for (const auto& [doc_id, freq] : merged_postings) {
            uint32_t gap = doc_id - prev_doc_id;
            doc_gaps.push_back(gap);
            freqs.push_back(freq);
            prev_doc_id = doc_id;
        }

        try {
            // Encode docIDs and frequencies using VarByte encoding
            vector<uint8_t> encoded_docids;
            encodeVarByteList(doc_gaps, encoded_docids);

            vector<uint8_t> encoded_freqs;
            encodeVarByteList(freqs, encoded_freqs);

            // Write encoded postings to the final index file
            final_index.write(reinterpret_cast<char*>(encoded_docids.data()), encoded_docids.size());
            final_index.write(reinterpret_cast<char*>(encoded_freqs.data()), encoded_freqs.size());

            // Write term information to the lexicon
            uint64_t freq_offset = current_offset + encoded_docids.size();
            lexicon << term << "\t" << current_offset << "\t" << encoded_docids.size() << "\t"
                    << freq_offset << "\t" << encoded_freqs.size() << "\t" << merged_postings.size() << "\n";

            // Update the current offset
            current_offset += encoded_docids.size() + encoded_freqs.size();
        } catch (const std::runtime_error& e) {
            cerr << "Encoding error for term '" << term << "': " << e.what() << endl;
            // Optionally, skip this term or handle the error as needed
            continue;
        }
    }

    // Close all files
    final_index.close();
    lexicon.close();
    for (auto& infile : intermediate_files) {
        infile.close();
    }

    cout << "Indexing completed. Final inverted index and lexicon are created." << endl;

    return 0;
}
