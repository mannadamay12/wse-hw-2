#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <algorithm>
#include <filesystem>
#include "/root/wse/wse-hw-2/include/tokenizer.h"


using namespace std; // ---change for defined std-----

namespace fs = std::filesystem;

const size_t MAX_INTERMEDIATE_FILE_SIZE = 1 * 1024 * 1024 * 1024; // 1GB ---- max size for intermeduate files

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_tsv_file> <output_directory>" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_dir = argv[2];

    if(!fs::exists(output_dir)) {  // check if output directory exists; if not, create it
        if(!fs::create_directories(output_dir)) {
            std::cerr << "Failed to create output directory: " << output_dir << std::endl;
            return 1;
        }
    }

    std::ifstream infile(input_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open input file: " << input_file << std::endl;
        return 1;
    }

    size_t current_size = 0;
    int file_count = 1;
    // using unordered_map to store postings; for large-scale, ----------------consider more memory-efficient structures-------------
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> postings_map;
    std::string line;

    uint64_t total_tokens = 0;

    // passages.bin
    std::ofstream passages_file(output_dir + "/passages.bin", std::ios::binary);
    if(!passages_file.is_open()) {
        std::cerr << "Failed to create passages.bin in " << output_dir << std::endl;
        return 1;
    }

    // page_table.txt
    std::ofstream page_table_file(output_dir + "/page_table.txt");
    if(!page_table_file.is_open()) {
        std::cerr << "Failed to create page_table.txt in " << output_dir << std::endl;
        return 1;
    }

    // doc_lengths.txt
    std::ofstream doc_length_file(output_dir + "/doc_lengths.txt");
    if(!doc_length_file.is_open()) {
        std::cerr << "Failed to create doc_lengths.txt in " << output_dir << std::endl;
        return 1;
    }

    while(std::getline(infile, line)) {
        if(line.empty()) continue;

        // extract docID and passage
        size_t tab_pos = line.find('\t');
        if(tab_pos == std::string::npos) {
            std::cerr << "Invalid line format (no tab found): " << line << std::endl;
            continue;
        }

        std::string doc_id_str = line.substr(0, tab_pos);
        std::string passage = line.substr(tab_pos + 1);

        uint32_t doc_id;
        try {
            doc_id = std::stoul(doc_id_str);
        } catch(const std::exception& e) {
            std::cerr << "Invalid docID: " << doc_id_str << " | Error: " << e.what() << std::endl;
            continue;
        }

        // tokenize passage
        std::vector<std::string> tokens = tokenize(passage);

        total_tokens += tokens.size();

        doc_length_file << doc_id << "\t" << tokens.size() << "\n";

        uint64_t offset = passages_file.tellp();
        // write passage length as a 4-byte unsigned integer
//        uint32_t passage_length = tokens.size(); // Or bytes -----------if needed----------
// Correctly set passage_length to the number of bytes in the passage
        uint32_t passage_length = static_cast<uint32_t>(passage.size());

        passages_file.write(reinterpret_cast<char*>(&passage_length), sizeof(uint32_t));
        passages_file.write(passage.c_str(), passage.size());

        // page_table.txt: docID, offset, length
        page_table_file << doc_id << "\t" << offset << "\t" << passage.size() << "\n";

        // count term frequencies
        std::unordered_map<std::string, uint32_t> term_freq;
        for(const auto& token : tokens) {
            term_freq[token]++;
        }

        for(const auto& [term, freq] : term_freq) {
            postings_map[term].emplace_back(doc_id, freq);
        }

        // estimate current_size ----------(simplistic approach)---------
        current_size += line.size();
        if(current_size >= MAX_INTERMEDIATE_FILE_SIZE) {
            std::string intermediate_file = output_dir + "/intermediate_" + std::to_string(file_count) + ".txt";
            std::ofstream outfile(intermediate_file);
            if(!outfile.is_open()) {
                std::cerr << "Failed to open intermediate file: " << intermediate_file << std::endl;
                return 1;
            }

            // extract, sort terms lexicographically
            std::vector<std::string> terms;
            terms.reserve(postings_map.size());
            for(const auto& [term, _] : postings_map) {
                terms.push_back(term);
            }
            std::sort(terms.begin(), terms.end());

            for(const auto& term : terms) {
                outfile << term;
                for(const auto& [doc_id, freq] : postings_map[term]) {
                    outfile << "\t" << doc_id << "\t" << freq;
                }
                outfile << "\n";
            }

            outfile.close();
            std::cout << "Written intermediate file: " << intermediate_file << std::endl;

            postings_map.clear();
            current_size = 0;
            file_count++;
        }
    }

    if(!postings_map.empty()) {   // remaining postings_map to an intermediate file
        std::string intermediate_file = output_dir + "/intermediate_" + std::to_string(file_count) + ".txt";
        std::ofstream outfile(intermediate_file);
        if(!outfile.is_open()) {
            std::cerr << "Failed to open intermediate file: " << intermediate_file << std::endl;
            return 1;
        }

        std::vector<std::string> terms;
        terms.reserve(postings_map.size());
        for(const auto& [term, _] : postings_map) {
            terms.push_back(term);
        }
        std::sort(terms.begin(), terms.end());

        for(const auto& term : terms) {
            outfile << term;
            for(const auto& [doc_id, freq] : postings_map[term]) {
                outfile << "\t" << doc_id << "\t" << freq;
            }
            outfile << "\n";
        }

        outfile.close();
        std::cout << "Written intermediate file: " << intermediate_file << std::endl;
    }

    infile.close();
    passages_file.close();
    page_table_file.close();
    doc_length_file.close();
    std::cout << "Parsing and posting generation completed." << std::endl;
    std::cout << "Total Tokens: " << total_tokens << std::endl;

    // write total_tokens to a separate file for future reference
    std::ofstream total_tokens_file(output_dir + "/total_tokens.txt");
    if(total_tokens_file.is_open()) {
        total_tokens_file << total_tokens << "\n";
        total_tokens_file.close();
        std::cout << "Written total_tokens.txt" << std::endl;
    } else {
        std::cerr << "Failed to write total_tokens.txt" << std::endl;
    }

    return 0;
}



// parser -- is it correctly parsing the words
// are the characters not in english being removed?
// 