# Zeno Filter

Zeno filter is an expandable and space-efficient filter designed to minimize memory overhead both during and after expansion. 
It achieves this by expanding in-place through a lightweight and fast layer of indirection, and can further optimize in-place expansion using virtual memory. 
Unlike traditional filters that double in size, Zeno Filter expands by less than a factor of two, conserving space while maintaining performance. 
Overall, it supports indefinite expansion, constant-time access across expansions, and a stable false positive rate.

## Variants

Zeno filter introduces two implementations, each taking a different approach to in-place expansion. 
**IZF** refers to the implementation that utilizes a layer of indirection. 
**VZF** refers to the implementation that leverages virtual memory. 
In the paper, this variant is referred to as Zeno filter-VM. 


## APIs

The core API provides standard filter operations: 

- `insert(uint64_t key, uint64_t value, uint64_t count, uint8_t flags)`: Increments the counter for this key/value pair by `count`. 
- `int32_t remove(uint64_t key, uint64_t value, uint64_t count, uint8_t flags)`: Removes up to `count` instances of this key/value combination. 
- `uint64_t query(uint64_t key, uint64_t& value, uint8_t flags)`: Looks up the value associated with key. Returns the count of that key/value pair in the filter.

For a more detailed description of the APIs, please refer to the source code in `include/izf.hpp` and `include/vzf.hpp`. 

## Instructions

### Dependencies

Zeno filter requires the following dependencies:

- cmake 3.14 (or later)
- gcc-11 (or later)
- OpenSSL 1.1.1f (or later)

### Compilation

The following commands will build Zeno filter along with the benchmarks and examples. 

```bash
mkdir -p build & cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j 8
```

Alternately for the widening regime, you must set the flag like so:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DWIDENING=ON
```

### Executing the Example Code

Once compiled, the following commands will execute the example code. 
The example code initializes a filter, inserts random entries so that it expands twice, and queries the same inserted keys. 

```bash
# Assuming already in build/
cd examples
./example Q F R
```

Replace Q, F, and R with the desired quotient bits, fingerprint bits, and growth coefficient, as described in the paper. 
For example, this command:

```bash
./example 12 8 1
```

will initialize Zeno filter with $`2^{12}`$ slots with 8-bit fingerprints, and a growth coefficient of 1. 


### Running the Experiments

We include scripts under the `scripts/` directory that reproduces all results in the paper. 

- `./scripts/runall.sh`: Runs all the experiments. 
- `./scripts/exp1.sh`: Runs experiment 1 on Stretching with different values of $`r`$. This runs the following scripts:
    - `./scripts/exp_spaceamp.sh`: Experiment on the space amplification. 
    - `./scripts/exp_stretching.sh`: Experiment on average insert / query latency and false positive rate. 
- `./scripts/exp2.sh`: Runs experiment 2 on in-place expansion. 
- `./scripts/exp3.sh`: Runs experiment 3 on contraction and secondary hash table. 
- `./scripts/exp4.sh`: Runs experiment 4 on concurrency. This runs the following scripts:
    - `./scripts/exp_concurrency.sh`: Experiment on varying expansion thresholds and varying number of threads. 
    - `./scripts/exp_concurrent_rw.sh`: Experiment on concurrent filter access (read / write) during expansion.
- `./scripts/exp5.sh`: Runs experiment 5, comparing Zeno filter against other baselines. 

### Results

After running the experiments:

- All results are stored in the `results/` directory.
- Each evaluation produces one or more `.csv` files with raw measurements. 
- The plotting script `plot.py` generates figures in PDF.

The generated PDFs will appear in the `results/` directory, with filenames matching the figures in the paper. 


### Baselines

InfiniFilter and Aleph filter are implemented under `include/`. 
The [Logarithmic Dynamic Cuckoo Filter](https://github.com/DavIvek/BioInf1) and [Bamboo filter](https://github.com/wanghanchengchn/bamboofilters) are cloned from the respective repositories, and were modified slightly for evaluations. 
No modifications were made that harm the performance of the filters. 
The code for the two baselines are under `bench/include/`. 

The file `bench/base.hpp` provides a unified interface for all filter implementation used in benchmarking.
Each filter implements this interface to ensure a consistent API across different designs. 
You can find them under the bench/ directory, with filenames ending in `_template.hpp`. 