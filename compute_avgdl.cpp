// compute_avgdl.cpp
#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <total_tokens_file> <output_file>" << std::endl;
        return 1;
    }

    std::string total_tokens_file = argv[1];
    std::string output_file = argv[2];

    std::ifstream infile(total_tokens_file);
    if(!infile.is_open()) {
        std::cerr << "Failed to open file: " << total_tokens_file << std::endl;
        return 1;
    }

    uint64_t total_tokens;
    infile >> total_tokens;
    infile.close();

    uint32_t total_docs = 8841823; // Replace with actual total number of documents

    double avgdl = static_cast<double>(total_tokens) / static_cast<double>(total_docs);

    std::ofstream outfile(output_file);
    if(!outfile.is_open()) {
        std::cerr << "Failed to open output file: " << output_file << std::endl;
        return 1;
    }

    outfile << avgdl << "\n";
    outfile.close();

    std::cout << "Average Document Length (avgdl): " << avgdl << std::endl;

    return 0;
}
