#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>
// Structure for Document Information
struct DocumentInfo {
    uint32_t docID;
    uint64_t passage_offset;
    size_t passage_length;
};

// Function to load page_table.txt
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

int main(int argc, char* argv[]) {
    if(argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <passages.bin> <page_table.txt> <docID_to_verify>" << std::endl;
        return 1;
    }

    std::string passages_bin = argv[1];
    std::string page_table_file = argv[2];
    uint32_t verify_docID = std::stoul(argv[3]);

    // Load page table
    std::unordered_map<uint32_t, DocumentInfo> page_table;
    if(!load_page_table(page_table_file, page_table)) {
        return 1;
    }

    // Check if docID exists
    if(page_table.find(verify_docID) == page_table.end()) {
        std::cerr << "docID " << verify_docID << " not found in page_table." << std::endl;
        return 1;
    }

    DocumentInfo info = page_table[verify_docID];

    // Open passages.bin
    std::ifstream passages_file(passages_bin, std::ios::binary);
    if(!passages_file.is_open()) {
        std::cerr << "Failed to open passages.bin file: " << passages_bin << std::endl;
        return 1;
    }

    // Seek to the offset
    passages_file.seekg(info.passage_offset, std::ios::beg);
    if(passages_file.fail()) {
        std::cerr << "Failed to seek to offset " << info.passage_offset << " in passages.bin." << std::endl;
        return 1;
    }

    // Read passage length
    uint32_t passage_length;
    passages_file.read(reinterpret_cast<char*>(&passage_length), sizeof(uint32_t));
    if(passages_file.fail()) {
        std::cerr << "Failed to read passage_length for docID " << verify_docID << "." << std::endl;
        return 1;
    }

    // Validate passage_length
    if(passage_length != info.passage_length) {
        std::cerr << "Mismatch in passage_length for docID " << verify_docID << ": "
                  << "page_table.txt reports " << info.passage_length
                  << ", but passages.bin has " << passage_length << "." << std::endl;
        // Optionally, proceed to read anyway
    }

    // Read passage
    std::vector<char> passage_chars(passage_length);
    passages_file.read(passage_chars.data(), passage_length);
    if(passages_file.fail()) {
        std::cerr << "Failed to read passage for docID " << verify_docID << "." << std::endl;
        return 1;
    }

    std::string passage(passage_chars.begin(), passage_chars.end());

    // Output the passage
    std::cout << "DocID: " << verify_docID << std::endl;
    std::cout << "Passage: " << passage << std::endl;

    passages_file.close();
    return 0;
}
