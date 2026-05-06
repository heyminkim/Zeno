#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <thread>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "zenofiltervm_template.hpp"
#include "quotient_template.hpp"

#include "../util/cxxopts.hpp"

class ConcurrentFilter {
    public:
    virtual void concurrent_insert(uint64_t) = 0;
    virtual bool concurrent_query(uint64_t) = 0;
    virtual bool query(uint64_t) = 0;
    virtual uint64_t report_time() const = 0;
    virtual std::string name(bool) const = 0;
    virtual ~ConcurrentFilter() = default;
};

class ZenoFilterVMAdapter : public ConcurrentFilter {
    public:
    zeno_bench::ZenoFilterVM* f;
    explicit ZenoFilterVMAdapter(zeno_bench::ZenoFilterVM* f) : f(f) {}

    void concurrent_insert(uint64_t k) override {
        f->concurrent_insert(k);
    }

    bool concurrent_query(uint64_t k) override {
        return f->concurrent_query(k);
    }

    bool query(uint64_t k) override {
        return f->query(k);
    }

    uint64_t report_time() const {
        return f->report_time();
    }

    std::string name(bool b) const override {
        return f->name(b);
    }
};

class RSQFAdapter : public ConcurrentFilter {
    public:
    zeno_bench::Quotient* f;
    explicit RSQFAdapter(zeno_bench::Quotient* f) : f(f) {}

    void concurrent_insert(uint64_t k) override {
        f->concurrent_insert(k);
    }

    bool concurrent_query(uint64_t k) override {
        return f->concurrent_query(k);
    }

    bool query(uint64_t k) override {
        return f->query(k);
    }

    uint64_t report_time() const {
        return f->report_time();
    }

    std::string name(bool b) const override {
        return f->name(b);
    }
};

template <typename T>
void insert_keys(T filter, const uint64_t* keys, uint64_t num_keys) {
    for (uint64_t i = 0; i < num_keys; ++i) {
        filter->concurrent_insert(keys[i]);
    }
}

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("r,region", "Size of locking region in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("t,threads", "Number of threads", cxxopts::value<uint64_t>()->default_value("1"))
      ("a,threshold", "Expansion threshold", cxxopts::value<uint64_t>()->default_value("9"))
      ("fn_insert", "File name to write insert thpt results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t region = result["region"].as<uint64_t>();
    uint64_t nthreads = result["threads"].as<uint64_t>();
    uint64_t threshold = result["threshold"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();

    // Files to write results
    std::string fn_insert = result["fn_insert"].as<std::string>();

    std::ofstream file_insert(fn_insert, std::ios::app);

    if (!file_insert) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    // Parameters for benchmark
    bool record_insert = (fn_insert != "/dev/null");

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 70 * nvals / 100;

    uint64_t* keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    size_t total_bytes = nvals * sizeof(keys[0]);
    size_t done = 0;
    unsigned char* p = (unsigned char*)keys;
    while (done < total_bytes) {
        int this_chunk = (total_bytes - done > (size_t)INT_MAX)
                       ? INT_MAX
                       : (int)(total_bytes - done);
        if (RAND_bytes(p + done, this_chunk) != 1) {
            std::cerr << "RAND_bytes failed at offset " << done << std::endl;
            abort();
        }
        done += this_chunk;
    }

    bool auto_resize = true;
    
    // Configure filter
    ConcurrentFilter* filter = nullptr;
    if (id == 1) {  // ZenoFilterVM
        auto* f = new zeno_bench::ZenoFilterVM(qbits, qbits + fbits, 1, 0.8);
        f->auto_resize(auto_resize);
        filter = new ZenoFilterVMAdapter(f);
    } 
    else if (id == 2) {
        auto* f = new zeno_bench::Quotient(qbits, qbits + fbits, 0.8);
        f->auto_resize(auto_resize);
        filter = new RSQFAdapter(f);
    }

#if defined(LOCKSLOTS)
    file_insert << filter->name(false) << "con" << LOCKSLOTS << ",";
#else
    file_insert << filter->name(false) << "con" << expansions << ",";
#endif

    // Aggregated insert time
    uint64_t insert_time = 0;

    // Number of inserts per thread
    uint64_t nvals_per_thread = nvals / nthreads;

    // Thread pool
    std::vector<std::thread> threads;
    std::vector<uint64_t> time_per_thread(nthreads);

    const auto start = std::chrono::high_resolution_clock::now();

    // Execute evaluation
    for (uint64_t i = 0; i < nthreads; ++i) {
        const uint64_t* keys_start = keys + (i * nvals_per_thread);
        threads.emplace_back(
            insert_keys<ConcurrentFilter*>,
            filter,
            keys_start,
            nvals_per_thread
        );
    }

    // Join finished threads
    for (auto& t : threads) if (t.joinable()) t.join();

    const auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    double seconds = ((double)ns) / 1e9;
    double mops = ((double)nvals / seconds) / 1e6;

    file_insert << nthreads << "," << mops << std::endl;
    file_insert.close();
    delete filter;

    return 0;
}