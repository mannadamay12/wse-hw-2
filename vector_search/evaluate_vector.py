import argparse
import logging
from typing import Dict, List, Tuple
import numpy as np
import h5py
from tqdm import tqdm
import json
import pytrec_eval

from vector_search import VectorSearchEngine
from read_h5 import load_embeddings

def load_qrels(qrels_file: str) -> str:
    """Convert TSV qrels to space-separated format and return temp file path."""
    temp_path = qrels_file + '.converted'
    with open(qrels_file) as f_in, open(temp_path, 'w') as f_out:
        for line in f_in:
            parts = line.strip().split('\t')
            if len(parts) == 3:  # dev format
                qid, docid, rel = parts
                f_out.write(f"{qid} 0 {docid} {rel}\n")
            else:  # eval format
                qid, _, docid, rel = parts
                f_out.write(f"{qid} 0 {docid} {rel}\n")
    return temp_path

def format_trec_run(results: List[Tuple[str, List[Tuple[str, float]]]], run_tag: str) -> List[str]:
    run_strings = []
    for qid, doc_scores in results:
        for rank, (docid, score) in enumerate(doc_scores, 1):
            run_strings.append(f"{qid} Q0 {docid} {rank} {score:.6f} {run_tag}")
    return run_strings

def evaluate_runs(run_file: str, qrels_file: str, metrics: set) -> Dict:
    # Convert qrels to space-separated format
    converted_qrels = load_qrels(qrels_file)
    
    with open(converted_qrels) as f:
        qrels = pytrec_eval.parse_qrel(f)
    
    with open(run_file) as f:
        run = pytrec_eval.parse_run(f)
    
    evaluator = pytrec_eval.RelevanceEvaluator(qrels, metrics)
    results = evaluator.evaluate(run)
    
    # Calculate mean scores
    mean_scores = {}
    for metric in metrics:
        scores = [query_scores[metric] for query_scores in results.values()]
        mean_scores[metric] = sum(scores) / len(scores)
    
    return mean_scores, results

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--embeddings", required=True)
    parser.add_argument("--queries-embeddings", required=True)
    parser.add_argument("--qrels", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    # Load document embeddings and build index
    doc_ids, doc_embeddings = load_embeddings(args.embeddings)
    engine = VectorSearchEngine()
    engine.build_index(doc_embeddings, doc_ids)

    # Load query embeddings
    query_ids, query_embeddings = load_embeddings(args.queries_embeddings)
    
    # Perform batch search
    results = engine.batch_search(query_embeddings, query_ids, k=100)
    
    # Write TREC run file
    run_strings = format_trec_run(results, "VECTOR")
    with open(f"{args.output}.trec", "w") as f:
        f.write("\n".join(run_strings))
    
    # Evaluate using appropriate metrics
    if "dev" in args.qrels:
        metrics = {"map", "recall_100", "recip_rank"}
    else:
        metrics = {"ndcg_cut_10", "ndcg_cut_100", "recip_rank"}
    
    mean_scores, per_query = evaluate_runs(f"{args.output}.trec", args.qrels, metrics)
    
    # Save evaluation results
    with open(f"{args.output}.json", "w") as f:
        json.dump({
            "mean_scores": mean_scores,
            "per_query": per_query
        }, f, indent=2)
    
    # Print summary
    for metric, score in mean_scores.items():
        print(f"{metric}: {score:.4f}")

if __name__ == "__main__":
    main()