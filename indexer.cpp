#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <cstdint>
#include "varbyte.h" // Ensure VarByte encoding functions are correctly implemented

using namespace std;

// Structure to hold term and its postings
struct TermPostings {
    string term;
    vector<pair<uint32_t, uint32_t>> postings; // (docID, freq)
};

// Comparator for sorting terms
bool compare_terms(const TermPostings& a, const TermPostings& b) {
    return a.term < b.term;
}

int main(int argc, char* argv[]) {
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " <intermediate_file> <final_index> <lexicon_file>" << endl;
        return 1;
    }

    string intermediate_file = argv[1];
    string final_index_file = argv[2];
    string lexicon_file = argv[3];

    // Read all term postings
    vector<TermPostings> all_terms;
    ifstream infile(intermediate_file);
    if(!infile.is_open()) {
        cerr << "Failed to open intermediate file: " << intermediate_file << endl;
        return 1;
    }

    string line;
    while(getline(infile, line)) {
        if(line.empty()) continue;

        size_t pos = line.find('\t');
        if(pos == string::npos) continue;

        TermPostings tp;
        tp.term = line.substr(0, pos);

        size_t current = pos + 1;
        while(current < line.size()) {
            size_t next_tab = line.find('\t', current);
            if(next_tab == string::npos) break;
            string doc_id_str = line.substr(current, next_tab - current);
            current = next_tab + 1;
            next_tab = line.find('\t', current);
            if(next_tab == string::npos) {
                break;
            }
            string freq_str = line.substr(current, next_tab - current);
            current = next_tab + 1;

            uint32_t doc_id = stoi(doc_id_str);
            uint32_t freq = stoi(freq_str);
            tp.postings.emplace_back(doc_id, freq);
        }

        // Handle last freq if any
        size_t last_tab = line.rfind('\t');
        if(last_tab != string::npos && last_tab + 1 < line.size()) {
            string freq_str = line.substr(last_tab + 1);
            uint32_t freq = stoi(freq_str);
            // To correctly associate the last docID, we need to parse it from before the last tab
            size_t second_last_tab = line.rfind('\t', last_tab - 1);
            if(second_last_tab != string::npos) {
                string doc_id_str = line.substr(second_last_tab + 1, last_tab - second_last_tab - 1);
                uint32_t doc_id = stoi(doc_id_str);
                tp.postings.emplace_back(doc_id, freq);
            }
        }

        all_terms.push_back(tp);
    }
    infile.close();

    // Sort all_terms lexicographically
    sort(all_terms.begin(), all_terms.end(), compare_terms);

    // Open final_index and lexicon files
    ofstream final_index(final_index_file, ios::binary);
    if(!final_index.is_open()) {
        cerr << "Failed to create final index file: " << final_index_file << endl;
        return 1;
    }

    ofstream lexicon(lexicon_file);
    if(!lexicon.is_open()) {
        cerr << "Failed to create lexicon file: " << lexicon_file << endl;
        return 1;
    }

    uint64_t current_offset = 0;

    for(auto& term_postings : all_terms) {
        // Separate docIDs and freqs
        vector<uint32_t> doc_ids;
        vector<uint32_t> freqs;
        for(auto& [doc_id, freq] : term_postings.postings) {
            doc_ids.push_back(doc_id);
            freqs.push_back(freq);
        }

        // Apply VarByte encoding
        vector<uint8_t> encoded_docids = encodeVarByte(doc_ids);
        vector<uint8_t> encoded_freqs = encodeVarByte(freqs);

        // Write to final_index.bin
        final_index.write(reinterpret_cast<char*>(encoded_docids.data()), encoded_docids.size());
        final_index.write(reinterpret_cast<char*>(encoded_freqs.data()), encoded_freqs.size());

        uint64_t freq_offset = current_offset + encoded_docids.size();

        // Write to lexicon.txt
        lexicon << term_postings.term << "\t" << current_offset << "\t" << encoded_docids.size() << "\t" << freq_offset << "\t" << encoded_freqs.size() << "\n";

        // Update current_offset
        current_offset += encoded_docids.size() + encoded_freqs.size();
    }

    final_index.close();
    lexicon.close();

    cout << "Indexing completed. Final inverted index and lexicon are created." << endl;

    return 0;
}
