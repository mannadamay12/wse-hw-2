Submission hw-2 || am14579 || dp4076
================================================================================
Directory structure:
--------------------------------------------------------------------------------
```
wse-hw-2/
├── include/
│   ├── tokenizer.h
│   └── varbyte.h
├── src/
│   ├── parser.cpp
│   ├── compute_avgdl.cpp
│   ├── indexer.cpp
│   └── query_processor.cpp
```

Files:
--------------------------------------------------------------------------------
To compiler cpp files `g++ -std=c++17 -O2 -o <executable name> src/<cpp file>.cpp`

1. tokenizer.h
    - Convert all text to lowercase to ensure case-insensitive indexing.
    - Remove punctuation and special characters to clean the tokens.
    - Split text into tokens based on whitespace.

2. varbyte.h
    - Apply <b>VarByte encoding</b> to compress docIDs and frequencies separately.

3. parser.cpp
    - Parses the raw MS MARCO dataset and creates sorted intermediate index posting.
    - To compiler parser.cpp `g++ -std=c++17 -O2 -o parser parser.cpp`
    - `./parser collection.tsv output/`
    - The parser will create 1 GB capped files named as intermediate_1.txt etc.
    - Ensure that the output directory exists before running the parser.

4. indexer.cpp
    - Merges sorted intermediate postings into a final compressed inverted index.
    
    ```
    ./indexer output/intermediate_1.txt output/intermediate_2.txt output/intermediate_3.txt output/final_index.bin output/lexicon.txt
    ```

5. compute_avgdl.cpp
    - Calculates the average document lenght for query processor.
    
    ```
    ./compute_avgdl output/doc_lengths.txt 8841823 output/avgdl.txt
    ```

6. query_processor.cpp
    - Processes user queries, retrieves and ranks relevant documents using the BM25 algorithm, and displays the top-10 results with corresponding passages.

    ```
    ./query_processor output/final_index.bin output/lexicon.txt output/page_table.txt output/passages.bin output/doc_lengths.txt output/avgdl.txt
    ```

7. logs/*
    - Covers the logging time for parsing and indexing.

8. output/*
    - Covers all the intermdeiate files.
    - Has inverted index, page table and passages in .bin and .text format

</p>Expect the result of query processor as top 10 passages with doc ID, BM25 score and passage text in addition to time required to search that particular query and the memory used. Also in terminal select 1 for conjunctive search or 2 for disjunctive search. After this enter your query or simply type "exit" to move out of the processor.</p> 