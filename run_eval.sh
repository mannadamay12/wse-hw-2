#!/bin/bash

# Directory setup
OUTPUT_DIR="output"
DATA_DIR="data"
EVAL_DIR="output/evaluations"
TEMP_DIR="$EVAL_DIR/temp"

mkdir -p $EVAL_DIR $TEMP_DIR

echo "Extracting relevant queries..."
python3 extract_queries.py --data-dir $DATA_DIR --output-dir $TEMP_DIR

echo "Running BM25 on dev queries..."
./query_processor \
    $OUTPUT_DIR/final_index.bin \
    $OUTPUT_DIR/lexicon_file.txt \
    $OUTPUT_DIR/page_table.txt \
    $OUTPUT_DIR/passages.bin \
    $OUTPUT_DIR/doc_lengths.txt \
    $OUTPUT_DIR/avgdl.txt \
    $TEMP_DIR/filtered_queries.dev.tsv \
    $EVAL_DIR/bm25.dev.trec

echo "Running BM25 on TREC-DL 2019 queries..."
./query_processor \
    $OUTPUT_DIR/final_index.bin \
    $OUTPUT_DIR/lexicon_file.txt \
    $OUTPUT_DIR/page_table.txt \
    $OUTPUT_DIR/passages.bin \
    $OUTPUT_DIR/doc_lengths.txt \
    $OUTPUT_DIR/avgdl.txt \
    $TEMP_DIR/filtered_queries.eval.2019.tsv \
    $EVAL_DIR/bm25.eval1.trec

echo "Running BM25 on TREC-DL 2020 queries..."
./query_processor \
    $OUTPUT_DIR/final_index.bin \
    $OUTPUT_DIR/lexicon_file.txt \
    $OUTPUT_DIR/page_table.txt \
    $OUTPUT_DIR/passages.bin \
    $OUTPUT_DIR/doc_lengths.txt \
    $OUTPUT_DIR/avgdl.txt \
    $TEMP_DIR/filtered_queries.eval.2020.tsv \
    $EVAL_DIR/bm25.eval2.trec

echo "Running evaluations..."
python3 evaluate.py \
    --run-file "$EVAL_DIR/bm25" \
    --output-prefix "$EVAL_DIR/bm25"

# Optional: Clean up temporary files
# rm -r $TEMP_DIR