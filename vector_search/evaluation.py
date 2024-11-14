import numpy as np
import pandas as pd
from typing import Dict, List, Tuple
import logging
from collections import defaultdict
import time
import json
import os

class SearchEvaluator:
    def __init__(self):
        self.binary_metrics = ['mrr@10', 'map', 'recall@100']  # for dev
        self.graded_metrics = ['ndcg@10', 'ndcg@100']  # for eval_one and eval_two
        
        # Load relevance judgments with appropriate binary flag
        self.qrels = {
            'dev': self._load_qrels('data/qrels.dev.tsv', binary=True),  # binary relevance
            'eval_one': self._load_qrels('data/qrels.eval.one.tsv', binary=False),  # graded relevance
            'eval_two': self._load_qrels('data/qrels.eval.two.tsv', binary=False)  # graded relevance
        }
        
        # Load query text
        self.queries = self._load_queries()
        logging.info("Evaluator initialized successfully")
        # Log initialization details
        for qrel_type, qrel_data in self.qrels.items():
            logging.info(f"Loaded {len(qrel_data)} queries for {qrel_type}")

    def _load_qrels(self, path: str, binary: bool = False) -> Dict:
        """Load relevance judgments."""
        qrels = defaultdict(dict)
        try:
            with open(path, 'r') as f:
                for line in f:
                    parts = line.strip().split('\t')
                    if len(parts) == 3:  # dev qrels format
                        qid, pid, rel = parts
                    elif len(parts) == 4:  # eval qrels format
                        qid, _, pid, rel = parts  # ignore second column
                    else:
                        logging.warning(f"Skipping malformed line in {path}: {line.strip()}")
                        continue
                    
                    rel = int(rel)
                    if binary:
                        rel = 1 if rel > 0 else 0
                    qrels[qid][pid] = rel
                
            logging.info(f"Loaded {len(qrels)} queries from {path}")
            return dict(qrels)
        except Exception as e:
            logging.error(f"Error loading qrels from {path}: {str(e)}")
            return {}

    def _load_queries(self) -> Dict[str, str]:
        """Load query text from both dev and eval files."""
        queries = {}
        files = ['data/queries.dev.tsv', 'data/queries.eval.tsv']
        for file in files:
            try:
                with open(file, 'r') as f:
                    for line in f:
                        qid, text = line.strip().split('\t')
                        queries[qid] = text
            except Exception as e:
                logging.error(f"Error loading queries from {file}: {str(e)}")
        return queries

    def calculate_metrics(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                        qrels_type: str = 'dev') -> Dict[str, float]:
        """Calculate all relevant metrics for a set of results."""
        if qrels_type not in self.qrels:
            raise ValueError(f"Invalid qrels_type: {qrels_type}")
        
        metrics = {}
        qrels = self.qrels[qrels_type]
        
        # Choose metrics based on qrels type
        metric_list = self.binary_metrics if qrels_type == 'dev' else self.graded_metrics
        
        for metric in metric_list:
            if metric == 'mrr@10':
                metrics[metric] = self._calculate_mrr(results, qrels, cutoff=10)
            elif metric.startswith('ndcg@'):
                cutoff = int(metric.split('@')[1])
                metrics[metric] = self._calculate_ndcg(results, qrels, cutoff)
            elif metric == 'map':
                metrics[metric] = self._calculate_map(results, qrels)
            elif metric.startswith('recall@'):
                cutoff = int(metric.split('@')[1])
                metrics[metric] = self._calculate_recall(results, qrels, cutoff)
        
        return metrics

    def _calculate_mrr(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                      qrels: Dict, cutoff: int = 10) -> float:
        """Calculate Mean Reciprocal Rank at cutoff."""
        mrr = 0.0
        num_queries = 0
        
        for qid, doc_scores in results:
            if qid in qrels:
                num_queries += 1
                for rank, (doc_id, _) in enumerate(doc_scores[:cutoff], 1):
                    if doc_id in qrels[qid] and qrels[qid][doc_id] > 0:
                        mrr += 1.0 / rank
                        break
        
        return mrr / num_queries if num_queries > 0 else 0.0

    def _calculate_ndcg(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                       qrels: Dict, cutoff: int) -> float:
        """Calculate NDCG at cutoff."""
        def dcg(relevances: List[int]) -> float:
            return sum((2**rel - 1) / np.log2(i + 2) 
                      for i, rel in enumerate(relevances[:cutoff]))
        
        ndcg_sum = 0.0
        num_queries = 0
        
        for qid, doc_scores in results:
            if qid in qrels:
                num_queries += 1
                relevances = [qrels[qid].get(doc_id, 0) 
                            for doc_id, _ in doc_scores[:cutoff]]
                dcg_val = dcg(relevances)
                
                # Calculate ideal DCG
                ideal_relevances = sorted(qrels[qid].values(), reverse=True)
                idcg_val = dcg(ideal_relevances)
                
                if idcg_val > 0:
                    ndcg_sum += dcg_val / idcg_val
        
        return ndcg_sum / num_queries if num_queries > 0 else 0.0

    def _calculate_map(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                      qrels: Dict) -> float:
        """Calculate Mean Average Precision."""
        ap_sum = 0.0
        num_queries = 0
        
        for qid, doc_scores in results:
            if qid in qrels:
                num_queries += 1
                relevant_docs = set(doc_id for doc_id, rel 
                                  in qrels[qid].items() if rel > 0)
                if not relevant_docs:
                    continue
                
                num_relevant = 0
                precision_sum = 0.0
                
                for rank, (doc_id, _) in enumerate(doc_scores, 1):
                    if doc_id in relevant_docs:
                        num_relevant += 1
                        precision_sum += num_relevant / rank
                
                ap_sum += precision_sum / len(relevant_docs)
        
        return ap_sum / num_queries if num_queries > 0 else 0.0

    def _calculate_recall(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                         qrels: Dict, cutoff: int) -> float:
        """Calculate Recall at cutoff."""
        recall_sum = 0.0
        num_queries = 0
        
        for qid, doc_scores in results:
            if qid in qrels:
                num_queries += 1
                relevant_docs = set(doc_id for doc_id, rel 
                                  in qrels[qid].items() if rel > 0)
                if not relevant_docs:
                    continue
                
                retrieved_relevant = sum(1 for doc_id, _ in doc_scores[:cutoff] 
                                      if doc_id in relevant_docs)
                recall_sum += retrieved_relevant / len(relevant_docs)
        
        return recall_sum / num_queries if num_queries > 0 else 0.0

    def evaluate_system(self, results: List[Tuple[str, List[Tuple[str, float]]]], 
                       system_name: str) -> Dict:
        """Evaluate results for all three sets of relevance judgments."""
        start_time = time.time()
        
        evaluation = {
            'system_name': system_name,
            'timestamp': time.strftime('%Y-%m-%d %H:%M:%S'),
            'metrics': {
                'dev': self.calculate_metrics(results, 'dev'),
                'eval_one': self.calculate_metrics(results, 'eval_one'),
                'eval_two': self.calculate_metrics(results, 'eval_two')
            },
            'num_queries': len(results),
            'evaluation_time': time.time() - start_time
        }
        
        # Save results
        os.makedirs('output/evaluations', exist_ok=True)
        output_file = f'output/evaluations/{system_name}_{time.strftime("%Y%m%d_%H%M%S")}.json'
        with open(output_file, 'w') as f:
            json.dump(evaluation, f, indent=2)
        
        return evaluation

    def print_evaluation(self, evaluation: Dict):
        """Print evaluation results in a formatted way."""
        print(f"\nEvaluation Results for {evaluation['system_name']}")
        print(f"Timestamp: {evaluation['timestamp']}")
        print(f"Number of queries: {evaluation['num_queries']}")
        print(f"Evaluation time: {evaluation['evaluation_time']:.2f} seconds")
        
        for qrels_type, metrics in evaluation['metrics'].items():
            print(f"\n{qrels_type.upper()} Metrics:")
            for metric, value in metrics.items():
                print(f"{metric}: {value:.4f}")