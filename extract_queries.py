#!/usr/bin/python3
import pandas as pd
import argparse
import os

def extract_queries(qrels_file, queries_file, output_file):
    """
    Extract queries from queries file based on qrels and save them to a new file
    """
    # Read qrels
    qrels = pd.read_csv(qrels_file, sep='\t', header=None)
    unique_qids = qrels[0].unique()  # First column contains query IDs
    
    # Read queries
    queries = pd.read_csv(queries_file, sep='\t', header=None, names=['qid', 'query'])
    
    # Filter queries that are in qrels
    filtered_queries = queries[queries['qid'].isin(unique_qids)]
    
    # Save to file
    filtered_queries.to_csv(output_file, sep='\t', index=False, header=False)
    
    print(f"Extracted {len(filtered_queries)} queries for {os.path.basename(qrels_file)}")
    return filtered_queries

def main():
    parser = argparse.ArgumentParser(description='Extract and prepare queries for evaluation')
    parser.add_argument('--data-dir', required=True, help='Directory containing data files')
    parser.add_argument('--output-dir', required=True, help='Directory for output files')
    args = parser.parse_args()
    
    # Create output directory if it doesn't exist
    os.makedirs(args.output_dir, exist_ok=True)
    
    # Process dev queries
    dev_queries = extract_queries(
        os.path.join(args.data_dir, 'qrels.dev.tsv'),
        os.path.join(args.data_dir, 'queries.dev.tsv'),
        os.path.join(args.output_dir, 'filtered_queries.dev.tsv')
    )
    
    # Process eval queries for TREC-DL 2019
    eval_2019_queries = extract_queries(
        os.path.join(args.data_dir, 'qrels.eval.one.tsv'),
        os.path.join(args.data_dir, 'queries.eval.tsv'),
        os.path.join(args.output_dir, 'filtered_queries.eval.2019.tsv')
    )
    
    # Process eval queries for TREC-DL 2020
    eval_2020_queries = extract_queries(
        os.path.join(args.data_dir, 'qrels.eval.two.tsv'),
        os.path.join(args.data_dir, 'queries.eval.tsv'),
        os.path.join(args.output_dir, 'filtered_queries.eval.2020.tsv')
    )

if __name__ == '__main__':
    main()