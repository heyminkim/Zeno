import numpy as np
import struct
import os

DATA_DIR = "data"
QUERY_SIZE = 10_000_000
NEG_FRACTIONS = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]

def read_dataset(fn):
    path = os.path.join(DATA_DIR, fn)
    d = np.fromfile(path, dtype=np.uint64)
    n = d[0]
    data = d[1:]
    assert len(data) == n
    return data


def write_dataset(fn, data):
    path = os.path.join(DATA_DIR, fn)
    with open(path, "wb") as f:
        f.write(struct.pack("Q", len(data)))
        data.tofile(f)


def prepare_datasets(name):
    fn_insert = f"{name}_insert_100M_uint64"
    fn_query = f"{name}_query_500K_uint64"

    path_insert = os.path.join(DATA_DIR, fn_insert)
    path_query = os.path.join(DATA_DIR, fn_query)

    if os.path.exists(path_insert) and os.path.exists(path_query):
        print(f"Skipping {name}: Output files already exist.")
        return

    print(f"Processing {name}")

    src = f"{name}_200M_uint64"
    data = read_dataset(src)
    assert len(data) == 200_000_000

    # interleaved split
    insert = data[::2]       # even indices
    query_pool = data[1::2]  # odd indices

    assert len(insert) == 100_000_000
    assert len(query_pool) == 100_000_000

    # filter query pool to values not in insert set
    print(f"  Filtering negatives...")
    negatives = query_pool[~np.isin(query_pool, insert)]
    assert len(negatives) >= QUERY_SIZE

    # shuffle arrays for random queries
    print(f"  Shuffling pools...")
    np.random.shuffle(insert)
    np.random.shuffle(negatives)

    # write shuffled dataset
    write_dataset(f"{name}_insert_100M_uint64", insert[:100_000_000])
    write_dataset(f"{name}_query_500K_uint64", negatives[:500_000])
    
    
if __name__ == "__main__":
    if not os.path.exists(DATA_DIR):
        os.makedirs(DATA_DIR)

    prepare_datasets("books")
    prepare_datasets("osm_cellids")