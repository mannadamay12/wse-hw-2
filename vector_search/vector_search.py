import faiss
import numpy as np
from typing import Tuple, List, Dict
import time
import logging

class VectorSearchEngine:
    def __init__(self, 
                 dim: int = 384,
                 m: int = 4,                     # Number of connections per layer
                 ef_construction: int = 50,      # Size of dynamic candidate list for construction
                 ef_search: int = 50):           # Size of dynamic candidate list for search
        self.dim = dim
        self.m = m
        self.ef_construction = ef_construction
        self.ef_search = ef_search
        self.index = None
        self.id_map = None
        
        logging.info(f"Initializing HNSW with parameters: m={m}, "
                    f"ef_construction={ef_construction}, ef_search={ef_search}")

    def build_index(self, embeddings: np.ndarray, ids: np.ndarray) -> None:
        """Build HNSW index from embeddings."""
        start_time = time.time()
        self.id_map = {i: str(id_) for i, id_ in enumerate(ids)}
        # Normalize embeddings for dot product similarity
        faiss.normalize_L2(embeddings)
        
        # Initialize HNSW index
        self.index = faiss.IndexHNSWFlat(self.dim, self.m, faiss.METRIC_INNER_PRODUCT)
        self.index.hnsw.efConstruction = self.ef_construction
        self.index.hnsw.efSearch = self.ef_search
        
        # Train and add vectors
        self.index.train(embeddings)
        self.index.add(embeddings)
        
        build_time = time.time() - start_time
        logging.info(f"Index built in {build_time:.2f} seconds. "
                    f"Total vectors: {self.index.ntotal}")
        
    def search(self, 
              query_embedding: np.ndarray, 
              k: int = 10) -> Tuple[List[str], List[float]]:
        """Search for k nearest neighbors."""
        # Normalize query embedding
        faiss.normalize_L2(query_embedding.reshape(1, -1))
        
        # Search
        start_time = time.time()
        scores, indices = self.index.search(query_embedding.reshape(1, -1), k)
        search_time = time.time() - start_time
        
        # Map internal indices to document IDs
        doc_ids = [self.id_map[idx] for idx in indices[0]]
        scores = scores[0]  # Flatten scores array
        
        logging.info(f"Search completed in {search_time*1000:.2f} ms")
        return doc_ids, scores

    # Add to existing VectorSearchEngine class:

    def batch_search(self, query_embeddings: np.ndarray, 
                    query_ids: List[str], k: int = 100) -> List[Tuple[str, List[Tuple[str, float]]]]:
        """Perform batch search for multiple queries."""
        if self.index is None:
            raise ValueError("Index not built or loaded")
        
        # Normalize query embeddings
        query_embeddings = query_embeddings.astype(np.float32)
        faiss.normalize_L2(query_embeddings)
        
        # Perform batch search
        scores, indices = self.index.search(query_embeddings, k)
        
        # Format results
        results = []
        for i, (query_scores, query_indices) in enumerate(zip(scores, indices)):
            try:
                # Filter out invalid indices and create doc_scores list
                doc_scores = []
                for idx, score in zip(query_indices, query_scores):
                    if idx != -1 and idx in self.id_map:  # Check for valid index
                        doc_scores.append((self.id_map[idx], float(score)))
                results.append((str(query_ids[i]), doc_scores))
            except Exception as e:
                logging.error(f"Error processing results for query {query_ids[i]}: {str(e)}")
                continue
        
        return results

    def save_index(self, filepath: str) -> None:
        """Save the FAISS index to disk."""
        faiss.write_index(self.index, filepath)
        
    def load_index(self, filepath: str) -> None:
        """Load the FAISS index from disk."""
        self.index = faiss.read_index(filepath)