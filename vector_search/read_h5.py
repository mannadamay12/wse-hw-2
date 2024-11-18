import h5py
import numpy as np
import pandas as pd

def explore_h5_structure(file_path):
    with h5py.File(file_path, 'r') as f:
        print(f"\nExploring H5 file: {file_path}")
        print("\nDatasets:")
        for key in f.keys():
            dataset = f[key]
            print(f"\nDataset: {key}")
            print(f"Shape: {dataset.shape}")
            print(f"Dtype: {dataset.dtype}")
            
            # Print first few values as sample
            if len(dataset.shape) == 2:
                print(f"First vector sample:\n{dataset[0][:10]}...")
            else:
                print(f"First few values:\n{dataset[:10]}...")

def load_embeddings(file_path, id_key='id', embedding_key='embedding'):
    with h5py.File(file_path, 'r') as f:
        ids = np.array(f[id_key]).astype(str)
        embeddings = np.array(f[embedding_key]).astype(np.float32)  
        # return np.array(f[dataset_name])
    print(f"Loaded {len(ids)} embeddings.")
    return ids, embeddings

def verify_data_subset(passage_ids_file, original_collection_file):
    # Read passage IDs
    passage_ids = set(pd.read_csv(passage_ids_file, header=None)[0])
    
    # Count matches in original collection
    matches = 0
    total = 0
    with open(original_collection_file, 'r') as f:
        for line in f:
            total += 1
            doc_id = int(line.split('\t')[0])
            if doc_id in passage_ids:
                matches += 1
    
    print(f"\nData Verification:")
    print(f"Total passages in subset: {len(passage_ids)}")
    print(f"Matched passages: {matches}")
    print(f"Total passages in original collection: {total}")

if __name__ == "__main__":
    EMBEDDINGS_FILE = "data/embeddings.h5"
    PASSAGE_IDS_FILE = "data/subset.tsv"
    ORIGINAL_COLLECTION = "data/data.tsv"
    
    # Explore H5 structure
    explore_h5_structure(EMBEDDINGS_FILE)
    
    # Verify data subset
    verify_data_subset(PASSAGE_IDS_FILE, ORIGINAL_COLLECTION)
    
    # Load sample embeddings
    ids, embeddings = load_embeddings(EMBEDDINGS_FILE)
    for i in range(min(5, len(ids))):
        print(f"ID: {ids[i]}, Embedding: {embeddings[i]}")
    print(f"\nLoaded embeddings shape: {embeddings.shape}")