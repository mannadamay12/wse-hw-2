import logging
from vector_search import VectorSearchEngine
from read_h5 import load_embeddings
from evaluation import SearchEvaluator
import time

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('logs/vector_search_eval.log'),
        logging.StreamHandler()
    ]
)

def main():
    # Initialize evaluator
    evaluator = SearchEvaluator()
    logging.info("Loading passage embeddings...")
    passage_ids, passage_embeddings = load_embeddings("data/embeddings.h5")
    
    # Load vector search engine
    vse = VectorSearchEngine()
    vse.build_index(passage_embeddings, passage_ids)
    # vse.load_index("output/vector_index.faiss")
    
    # Load query embeddings
    logging.info("Loading query embeddings...")
    query_ids, query_embeddings = load_embeddings("data/queries_dev_eval_embeddings.h5")
    
    relevant_qids = set()
    for qrels in evaluator.qrels.values():
        relevant_qids.update(qrels.keys())

    relevant_indices = [i for i, qid in enumerate(query_ids) if qid in relevant_qids]
    filtered_query_embeddings = query_embeddings[relevant_indices]
    filtered_query_ids = [query_ids[i] for i in relevant_indices]
    # Perform batch search
    logging.info(f"Processing {len(filtered_query_ids)} queries with relevance judgments")
    start_time = time.time()
    results = vse.batch_search(filtered_query_embeddings, filtered_query_ids, k=100)
    search_time = time.time() - start_time
    
    logging.info(f"Completed {len(results)} searches in {search_time:.2f} seconds")
    logging.info(f"Average search time: {(search_time/len(results))*1000:.2f}ms per query")
    
    # Evaluate results
    evaluation = evaluator.evaluate_system(results, "vector_search")
    evaluator.print_evaluation(evaluation)

if __name__ == "__main__":
    main()