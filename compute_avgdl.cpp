// compute_avgdl.cpp
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// compute average document length

int main(int argc, char* argv[]) {
    if(argc < 4) {
        cerr << "Usage: " << argv[0] << " <doc_lengths_file> <total_docs> <output_avgdl_file>" << endl;
        return 1;
    }

    string doc_lengths_file = argv[1];
    uint32_t total_docs;
    try {
        total_docs = stoul(argv[2]);
    } catch(const exception& e) {
        cerr << "Invalid total_docs: " << argv[2] << " | Error: " << e.what() << endl;
        return 1;
    }
    string output_avgdl_file = argv[3];

    ifstream infile(doc_lengths_file);
    if(!infile.is_open()) {
        cerr << "Failed to open doc_lengths_file: " << doc_lengths_file << endl;
        return 1;
    }

    uint32_t docID;
    uint32_t doc_length;
    uint64_t total_tokens = 0;
    uint32_t count = 0;

    while(infile >> docID >> doc_length) {
        total_tokens += doc_length;
        count++;
    }

    infile.close();

    if(count == 0) {
        cerr << "No documents found in doc_lengths_file." << endl;
        return 1;
    }

    double avgdl = static_cast<double>(total_tokens) / static_cast<double>(total_docs);

    ofstream outfile(output_avgdl_file);
    if(!outfile.is_open()) {
        cerr << "Failed to open output_avgdl_file: " << output_avgdl_file << endl;
        return 1;
    }

    outfile << avgdl << "\n";
    outfile.close();

    cout << "Average Document Length (avgdl): " << avgdl << endl;

    return 0;
}