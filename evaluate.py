#!/usr/bin/env python3
import argparse
import pytrec_eval
import json
from collections import defaultdict

def load_run(run_file):
    run = defaultdict(dict)
    try:
        with open(run_file) as f:
            for line in f:
                # Format: qid Q0 docid rank score tag
                qid, _, docid, _, score, _ = line.strip().split()
                run[qid][docid] = float(score)
    except Exception as e:
        print(f"Error loading run file {run_file}: {e}")
        return None
    return run

def load_qrels(qrels_file):
    qrels = defaultdict(dict)
    try:
        with open(qrels_file) as f:
            for line in f:
                # Split on whitespace to handle both space and tab separators
                parts = line.strip().split()
                if len(parts) == 3:  # dev format: qid docid rel
                    qid, docid, rel = parts
                    qrels[qid][docid] = int(rel)
                elif len(parts) == 4:  # eval format: qid 0 docid rel
                    qid, _, docid, rel = parts
                    qrels[qid][docid] = int(rel)
    except Exception as e:
        print(f"Error loading qrels file {qrels_file}: {e}")
        return None
    return qrels

def evaluate_run(run_file, qrels_file, metrics, run_name=""):
    # Load data
    run = load_run(run_file)
    qrels = load_qrels(qrels_file)
    
    if not run or not qrels:
        print(f"Failed to evaluate {run_name} - missing run or qrels data")
        return None, None

    try:
        # Create evaluator and evaluate
        evaluator = pytrec_eval.RelevanceEvaluator(qrels, metrics)
        results = evaluator.evaluate(run)
        
        # Compute means
        mean_scores = {}
        for metric in metrics:
            metric_scores = []
            for query_scores in results.values():
                if metric in query_scores:  # Check if metric exists for query
                    metric_scores.append(query_scores[metric])
            if metric_scores:
                mean_scores[metric] = sum(metric_scores) / len(metric_scores)
            else:
                mean_scores[metric] = 0.0
        
        return mean_scores, results
    except Exception as e:
        print(f"Error during evaluation of {run_name}: {e}")
        return None, None

def main():
    parser = argparse.ArgumentParser(description='Evaluate BM25 runs')
    parser.add_argument('--run-file', required=True, help='Base name for run files')
    parser.add_argument('--output-prefix', required=True, help='Prefix for output files')
    args = parser.parse_args()

    data_dir = "data"
    
    binary_metrics = {
        'map',              # Mean Average Precision
        # 'P_10',            # Precision at 10
        'recall_100',      # Recall at 100
        'recip_rank'       # MRR (Mean Reciprocal Rank)
    }
    
    graded_metrics = {
        'ndcg_cut_10',     # NDCG at 10
        'ndcg_cut_100',    # NDCG at 100
        # 'recall_100',      # Recall at 100
        'recip_rank'       # MRR
    }

    # Evaluate dev set (binary relevance)
    print("\nEvaluating on Dev set (Binary relevance)...")
    dev_scores, dev_results = evaluate_run(
        f"{args.run_file}.dev.trec",
        f"{data_dir}/qrels.dev.tsv",
        binary_metrics,
        "Dev Set"
    )
    if dev_scores:
        print(f"MAP:        {dev_scores['map']:.4f}")
        # print(f"P@10:       {dev_scores['P_10']:.4f}")
        print(f"Recall@100: {dev_scores['recall_100']:.4f}")
        print(f"MRR:        {dev_scores['recip_rank']:.4f}")
        
        with open(f"{args.output_prefix}.dev.json", 'w') as f:
            json.dump({
                'mean_scores': dev_scores,
                'per_query': dev_results
            }, f, indent=2)

    # Evaluate TREC-DL 2019
    print("\nEvaluating on TREC-DL 2019...")
    dl19_scores, dl19_results = evaluate_run(
        f"{args.run_file}.eval1.trec",
        f"{data_dir}/qrels.eval.one.tsv",
        graded_metrics,
        "TREC-DL 2019"
    )
    if dl19_scores:
        print(f"NDCG@10:    {dl19_scores['ndcg_cut_10']:.4f}")
        print(f"NDCG@100:   {dl19_scores['ndcg_cut_100']:.4f}")
        # print(f"Recall@100: {dl19_scores['recall_100']:.4f}")
        print(f"MRR:        {dl19_scores['recip_rank']:.4f}")
        
        with open(f"{args.output_prefix}.dl19.json", 'w') as f:
            json.dump({
                'mean_scores': dl19_scores,
                'per_query': dl19_results
            }, f, indent=2)

    # Evaluate TREC-DL 2020
    print("\nEvaluating on TREC-DL 2020...")
    dl20_scores, dl20_results = evaluate_run(
        f"{args.run_file}.eval2.trec",
        f"{data_dir}/qrels.eval.two.tsv",
        graded_metrics,
        "TREC-DL 2020"
    )
    if dl20_scores:
        print(f"NDCG@10:    {dl20_scores['ndcg_cut_10']:.4f}")
        print(f"NDCG@100:   {dl20_scores['ndcg_cut_100']:.4f}")
        # print(f"Recall@100: {dl20_scores['recall_100']:.4f}")
        print(f"MRR:        {dl20_scores['recip_rank']:.4f}")
        
        with open(f"{args.output_prefix}.dl20.json", 'w') as f:
            json.dump({
                'mean_scores': dl20_scores,
                'per_query': dl20_results
            }, f, indent=2)

if __name__ == '__main__':
    main()