#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
struct DocumentInfo {
    uint32_t docID;
    uint64_t passage_offset;
    size_t passage_length;
};

int main() {
    std::unordered_map<uint32_t, DocumentInfo> page_table;
    std::ifstream page_table_file("output/page_table.txt");
    uint32_t docID;
    uint64_t offset;
    size_t length;
    while(page_table_file >> docID >> offset >> length) {
        page_table[docID] = DocumentInfo{docID, offset, length};
    }
    page_table_file.close();

    uint32_t query_docID = 1; // Example docID
    if(page_table.find(query_docID) != page_table.end()) {
        DocumentInfo info = page_table[query_docID];
        std::ifstream passages_file("output/passages.bin", std::ios::binary);
        passages_file.seekg(info.passage_offset, std::ios::beg);
        uint32_t passage_length;
        passages_file.read(reinterpret_cast<char*>(&passage_length), sizeof(uint32_t));
        std::vector<char> passage_chars(passage_length);
        passages_file.read(passage_chars.data(), passage_length);
        std::string passage(passage_chars.begin(), passage_chars.end());
        passages_file.close();
        std::cout << "DocID: " << query_docID << " | Passage: " << passage << std::endl;
    } else {
        std::cout << "DocID not found." << std::endl;
    }

    return 0;
}
