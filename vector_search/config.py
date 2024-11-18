class Config:
    # Data paths
    DATA_TSV = "data/data.tsv"
    PASSAGE_EMBEDDINGS = "data/embeddings.h5"
    QUERY_EMBEDDINGS = "data/queries_dev_eval_embeddings.h5"
    
    # Query files
    QUERIES_DEV = "data/queries.dev.tsv"
    QUERIES_EVAL = "data/queries.eval.tsv"
    
    # Relevance judgment files
    QRELS_DEV = "data/qrels.dev.tsv"
    QRELS_EVAL_ONE = "data/qrels.eval.one.tsv"
    QRELS_EVAL_TWO = "data/qrels.eval.two.tsv"
    
    # HNSW parameters
    VECTOR_DIM = 384
    M = 6  # Number of connections per layer
    EF_CONSTRUCTION = 150
    EF_SEARCH = 150
    
    # Hybrid system parameters
    BM25_CANDIDATES = 10  # Number of candidates to get from BM25
    FINAL_TOP_K = 10  # Final number of results to return
    
    # Output paths
    FAISS_INDEX = "output/vector_index.faiss"
    RESULTS_DIR = "output/results/"
    
    # Evaluation parameters
    METRICS = {
        'binary': ['map', 'recall@100', 'mrr@10'],
        'graded': ['ndcg@10', 'ndcg@100', 'mrr@10']
    }