import logging
from vector_search import VectorSearchEngine
from read_h5 import load_embeddings
import numpy as np
import time
import os

# Set up logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('logs/vector_search.log'),
        logging.StreamHandler()
    ]
)

def main():
    os.makedirs('output', exist_ok=True)
    
    # Load passage embeddings
    logging.info("Loading passage embeddings...")
    doc_ids, doc_embeddings = load_embeddings("data/embeddings.h5")
    logging.info(f"Loaded {len(doc_ids)} passage embeddings with shape {doc_embeddings.shape}")
    
    # Initialize and build index
    vse = VectorSearchEngine(
        dim=384,
        m=8,
        ef_construction=200,
        ef_search=200
    )
    
    # Build and save index
    logging.info("Building HNSW index...")
    vse.build_index(doc_embeddings, doc_ids)
    vse.save_index("output/vector_index.faiss")
    
    # Load query embeddings
    logging.info("Loading query embeddings...")
    query_ids, query_embeddings = load_embeddings("data/queries_dev_eval_embeddings.h5")
    logging.info(f"Loaded {len(query_ids)} query embeddings")
    
    # Test search with a few queries
    logging.info("Testing search with sample queries...")
    
    # Load query text for display
    query_text = {}
    for filename in ["data/queries.dev.tsv", "data/queries.eval.tsv"]:
        with open(filename, 'r', encoding='utf-8') as f:
            for line in f:
                qid, text = line.strip().split('\t')
                query_text[qid] = text

    # Test first 5 queries
    for i in range(min(5, len(query_ids))):
        query_id = query_ids[i]
        query_embedding = query_embeddings[i].reshape(1, -1)
        
        print(f"\nQuery ID: {query_id}")
        if query_id in query_text:
            print(f"Query Text: {query_text[query_id]}")
        
        start_time = time.time()
        doc_ids, scores = vse.search(query_embedding, k=10)
        search_time = time.time() - start_time
        
        print(f"Search Time: {search_time*1000:.2f}ms")
        print("Top 10 results:")
        for doc_id, score in zip(doc_ids, scores):
            print(f"DocID: {doc_id}, Score: {score:.4f}")

    logging.info("Vector search test completed!")

if __name__ == "__main__":
    main()