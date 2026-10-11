# Zeno Filter

Zeno filter is an expandable and space-efficient filter designed to minimize memory overhead both during and after expansion. 
It achieves this by expanding in-place through a lightweight and fast layer of indirection, and can further optimize in-place expansion using virtual memory. 
Unlike traditional filters that double in size, Zeno Filter expands by less than a factor of two, conserving space while maintaining performance. 
Overall, it supports indefinite expansion, constant-time access across expansions, and a stable false positive rate.

## Variants

Zeno filter introduces two implementations, each taking a different approach to in-place expansion. 
**ZenoFilter** refers to the default implementation that utilizes the singly resizable array as the layer of indirection. 
**ZenoFilterVM** refers to the implementation that leverages virtual memory. 

The source code of each variant can be found under `include/`. 


## APIs

### Initial Configurations

Zeno filter takes the following parameters for initialization:

```c++
ZenoFilter(uint64_t exp_size, uint64_t fp_bits, uint64_t value_bits, 
           uint64_t expansion_coefficient, hashmode hash_mode, uint32_t seed, double threshold);
```

- `exp_size`: The hash length in bits. Includes both the length of the quotient and length of the fingerprint. Corresponds to $q + f$ from the paper. 
- `fp_bits`: The fingerprint length in bits. Corresponds to $f$ from the paper. 
- `value_bits`: Zeno filter supports payloads. `value_bits` specifies the payload length in bits. 
- `expansion_coefficient`: The expansion coefficient for Stretching. Corresponds to $r$ from the paper. 
- `hash_mode`: Zeno filter supports two hash modes. 
    - `zeno::hashmode::Default`: the key is not hashed - Zeno filter uses a hash function to hash the key. 
    - `zeno::hashmode::None`: the key is hashed - Zeno filter will not hash the key.
    - More details can be found in `include/decls.hpp`. 
- `seed`: The seed for the hash function. 
- `threshold`: The expansion threshold for exansion. Corresponds to $\alpha$ from the paper. 

The following example code initializes Zeno filter with $2^{12}$ slots, use 8-bit fingerprints, $r=1$, and expands when 90% full. 

```c++
using namespace zeno;
ZenoFilter* filter = ZenoFilter(20 /*12+8*/, 8, 0 /*no payload*/, 1, hashmode::Default, 0, 0.9);
```


### Filter Operations

The core API provides standard filter operations: 

- `insert(uint64_t key, uint64_t value, uint64_t count, uint8_t flags)`: Increments the counter for this key/value pair by `count`. 
- `int32_t remove(uint64_t key, uint64_t value, uint64_t count, uint8_t flags)`: Removes up to `count` instances of this key/value combination. 
- `uint64_t query(uint64_t key, uint64_t& value, uint8_t flags)`: Looks up the value associated with the key. Returns the count of that key/value pair in the filter.

For a more detailed description of the APIs, please refer to the source code in `include/zenofilter.hpp` and `include/zenofiltervm.hpp`. 

## Instructions

### Dependencies

The filter itself is header-only and requires a C++20 compiler. Building the benchmarks requires:

- cmake 3.19 (or later)
- gcc-11 (or later)
- OpenSSL 1.1.1f (or later, development headers)
- OpenMP (ships with gcc as `libgomp`)
- An x86-64 CPU. The benchmarks compile with `-march=native`; the Bamboo filter baseline
  additionally requires AVX2/FMA support.

Running the experiment scripts and generating the figures additionally requires:

- Python 3 with `matplotlib` and `numpy`
- `git`, `wget`, and `zstd` (used by experiment 8 to fetch WiredTiger and the SOSD datasets)
- (Optional) The Times New Roman font for paper-identical figures; `plot.py` falls back to the default serif font when it is unavailable.

On Ubuntu:

```bash
sudo apt install build-essential cmake git libssl-dev python3 python3-matplotlib python3-numpy wget zstd
```

### Compilation

The following commands will build Zeno filter along with the benchmarks and examples. 

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j 8 exp_fractional_expansion exp_spaceamp exp_inplace_expansion exp_widening \
          exp_directory exp_contract exp_concurrency exp_concurrent_rw exp_overall
```

Building the experiment targets by name (as above, or as done by the experiment scripts) is recommended over a plain `make -j 8`: the vendored LDCF baseline ships auxiliary test and benchmark targets that are not used by any experiment and do not compile.

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

will initialize Zeno filter with $2^{12}$ slots with 8-bit fingerprints, and a growth coefficient of 1. 


### Running the Experiments

We include scripts under the `scripts/` directory that reproduces all results in the paper. 

- `./scripts/runall.sh`: Runs all the experiments. 
- `./scripts/exp1.sh`: Runs experiment 1 on Stretching with different values of $`r`$. This runs the following scripts:
    - `./scripts/exp_spaceamp.sh`: Experiment on the space amplification. 
    - `./scripts/exp_stretching.sh`: Experiment on average insert / query latency and false positive rate. 
- `./scripts/exp2.sh`: Runs experiment 2 on in-place expansion. 
- `./scripts/exp3.sh`: Runs experiment 3 on widening. 
- `./scripts/exp4.sh`: Runs experiment 4 on Zeno Filter with fixed data block size. 
- `./scripts/exp5.sh`: Runs experiment 5 on contraction and secondary hash table. 
- `./scripts/exp6.sh`: Runs experiment 6 on concurrency. 
- `./scripts/exp7.sh`: Runs experiment 7, comparing Zeno filter against other baselines. 
- `./scripts/exp8.sh`: Runs experiment 8 on the impact of filter size with WiredTiger. 

Experiment 8 has additional requirements:

- WiredTiger (release 11.3.1) is fetched into `bench/wiredtiger` and compiled automatically the first time `exp8.sh` runs. WiredTiger's configure step additionally requires the Python 3 development headers and SWIG (`python3-dev` and `swig` on Ubuntu).
- The script downloads the SOSD `books` and `osm_cellids` datasets (~3.2 GB) into `data/` on the first run, and the ingestion phase creates a WiredTiger database of roughly 50 GB under `build/wt_database_home`. Make sure enough disk space is available.
- The experiment runs on the `books` dataset by default, as presented in the paper. To run it on `osm_cellids` instead, set `DATASET=osm` (e.g. `DATASET=osm ./scripts/exp8.sh`, or `docker run -e DATASET=osm ... zeno exp8`).

### Results

After running the experiments:

- All results are stored in the `results/` directory.
- Each evaluation produces one or more `.csv` files with raw measurements. 
- The plotting script `plot.py` generates figures in PDF.

The generated PDFs will appear in the `results/` directory, with filenames matching the figures in the paper. 

### Reproducing with Docker

The provided `Dockerfile` and `compose.yaml` package the full build and experiment environment,
including WiredTiger and the plotting toolchain. From a fresh clone, build the image and run every
experiment with a single command. Build on the machine you want to measure, since the benchmarks
compile with `-march=native`:

```bash
docker compose run --build --rm zeno
```

Figures (`*.pdf`) and raw data (`*.csv`) appear in `./out`, and the experiment 8 datasets are cached
in the `zeno-data` volume across runs. To run a subset of experiments, list them:

```bash
docker compose run --build --rm zeno exp2 exp6
```

Without Compose, the equivalent is:

```bash
docker build -t zeno .
docker run --rm -v "$PWD/out:/zeno/results" -v zeno-data:/zeno/data zeno            # all
docker run --rm -v "$PWD/out:/zeno/results" -v zeno-data:/zeno/data zeno exp2 exp6  # subset
```

Docker Desktop runs containers in a VM with its own memory limit (Settings -> Resources). 
The experiments need roughly 4 GB of VM memory. 
Experiment 8 also writes a ~50 GB WiredTiger database per filter configuration into the VM disk, which is a sparse image on the host (`Docker.raw`) that grows but does not shrink when files are deleted inside the VM. 
If the host filesystem backing that image fills up, the VM silently stalls, so keep well over 100 GB free on that filesystem or run experiment 8 natively.

We recommend running experiment 8 on the local machine rather than in Docker Desktop. The experiment
measures the I/O stalls caused by a small WiredTiger cache, but inside the VM the database lives in
`Docker.raw`, which the host's page cache can absorb; `direct_io` inside the VM then no longer reaches
the device and the stalls largely disappear, muting the difference between the filters. Docker Engine
on Linux (containers share the host kernel and disk) does not have this problem.


### Baselines

InfiniFilter and Aleph filter are implemented under `include/`. 
The [Logarithmic Dynamic Cuckoo Filter](https://github.com/DavIvek/BioInf1) and [Bamboo filter](https://github.com/wanghanchengchn/bamboofilters) are cloned from the respective repositories, and include changes for bug fixes. 
The code for the two baselines are under `bench/include/`. 

The file `bench/base.hpp` provides a unified interface for all filter implementation used in benchmarking.
Each filter implements this interface to ensure a consistent API across different designs. 
You can find them under the bench/ directory, with filenames ending in `_template.hpp`. 