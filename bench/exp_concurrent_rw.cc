#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <thread>
#include <vector>
#include <utility>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "zenofiltervm_template.hpp"
#include "quotient_template.hpp"

#include "../util/cxxopts.hpp"

std::atomic<bool> stop_flag{false};

class ConcurrentFilter {
    public:
    virtual void concurrent_insert(uint64_t) = 0;
    virtual bool concurrent_query(uint64_t) = 0;
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

    std::string name(bool b) const override {
        return f->name(b);
    }
};

template <typename T>
void insert_keys(T filter, const uint64_t* keys_start, uint64_t num_keys, 
                 uint64_t interval, std::vector<uint64_t>& result, 
                 std::chrono::_V2::steady_clock::time_point start_time) {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();
    bool res = false;

    for (uint64_t i = 0; i < num_keys; ++i) {
        filter->concurrent_insert(util::generate_random());

        if (i != 0 && i % interval == 0) {
            now = clock::now();
            auto us_since_last_sample = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
            result.emplace_back(us_since_last_sample);
        }
    }
}

template <typename T>
void query_keys(T filter, const uint64_t* keys_start, uint64_t num_keys, 
                 uint64_t interval, std::vector<uint64_t>& result, 
                 std::chrono::_V2::steady_clock::time_point start_time) {
    using clock = std::chrono::steady_clock;
    auto now = clock::now();

    uint64_t i = 0;
    while (!stop_flag.load()) {
        filter->concurrent_query(util::generate_random());

        if (i != 0 && i % interval == 0) {
            now = clock::now();
            auto us_since_last_sample = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
            result.emplace_back(us_since_last_sample);
        }
        ++i;
        i = i % num_keys;
    }
}

uint64_t calculate_result(std::vector<std::vector<uint64_t>>& datas, std::vector<double>& result, 
                      uint64_t unit_time, uint64_t sample_interval) {
    uint64_t num_threads = datas.size();
    uint64_t min_total_time = (0ULL - 1);
    uint64_t max_total_time = 0ULL;

    // For each thread result
    for (const auto& data : datas) {
        if (data.size() == 0) {
            std::cerr << "size is zero" << std::endl;
            abort();
        }
        if (data[data.size() - 1] < min_total_time) min_total_time = data[data.size() - 1];
    }

    uint64_t num_samples = 1 + min_total_time / unit_time;

    // Resize vector. resize fails for some reason..
    for (uint64_t i = 0; i < num_samples; ++i) {
        result.push_back(0);
    }

    double interval = static_cast<double>(sample_interval);

    for (uint64_t t = 0; t < num_threads; ++t) {
        const auto& data = datas[t];
        uint64_t prev_mod = 0;
        uint64_t prev_index = 0;
        uint64_t current_mod = 0;
        uint64_t current_index = 0;
        for (uint64_t d = 0; d < data.size(); ++d) {
            current_index = data[d] / unit_time;
            current_mod = data[d] % unit_time;

            uint64_t full_intervals = current_index - prev_index >= 2 ? current_index - prev_index - 1 : 0;
            double num_samples = static_cast<double>(full_intervals * unit_time + prev_mod + current_mod);
            
            result[prev_index] += interval * ((double)prev_mod / num_samples);

            if (full_intervals > 0) {
                for (uint64_t i = prev_index + 1; i < prev_index + 1 + full_intervals; ++i) {
                    result[i] += interval * ((double)unit_time / num_samples);
                }
            }

            result[current_index] += interval * ((double)current_mod / num_samples);


            prev_mod = unit_time - current_mod;
            prev_index = current_index;
        }
    }

    return max_total_time;
}

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("t,threads", "Number of threads", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_insert", "File name to write insert thpt results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_query", "File name to write query thpt results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t nthreads = result["threads"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();

    // Files to write results
    std::string fn_insert = result["fn_insert"].as<std::string>();
    std::string fn_query = result["fn_query"].as<std::string>();

    std::ofstream file_insert(fn_insert, std::ios::app);
    std::ofstream file_query(fn_query, std::ios::app);

    if (!file_insert || !file_query) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 80 * nvals / 100;

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

    uint64_t* queries = (uint64_t*)malloc(nvals * sizeof(queries[0]));
    total_bytes = nvals * sizeof(queries[0]);
    done = 0;
    p = (unsigned char*)queries;
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

    // String for buffering results
    std::ostringstream str_insert;
    std::ostringstream str_query;
    str_insert << filter->name(false) << "insert" << ",";
    str_query << filter->name(false) << "query" << ",";

    // Number of inserts per thread
    uint64_t nvals_per_thread = nvals / nthreads;
    uint64_t query_nthreads = 1;
    uint64_t nvals_per_query_thread = nvals / query_nthreads;

    // Thread pool
    std::vector<std::thread> threads;

    std::vector<std::thread> insert_threads;
    std::vector<std::thread> query_threads;

    // Result vectors
    std::vector<std::vector<uint64_t>> insert_time(nthreads);
    std::vector<std::vector<uint64_t>> query_time(query_nthreads);

    // Sample interval
    uint64_t sample_interval = 100;
    uint64_t unit_time = 1000;

    // Measure start time
    using clock = std::chrono::steady_clock;
    auto start_time = clock::now();

    // Execute inserts
    for (uint64_t i = 0; i < nthreads; ++i) {
        const uint64_t* keys_start = keys + (i * nvals_per_thread);
        insert_threads.emplace_back(
            insert_keys<ConcurrentFilter*>,
            filter,
            keys_start,
            nvals_per_thread,
            sample_interval,
            std::ref(insert_time[i]),
            start_time
        );
    }

    start_time = clock::now();
    // Execute queries
    for (uint64_t i = 0; i < query_nthreads; ++i) {
        const uint64_t* keys_start = queries + (i * nvals_per_query_thread);
        query_threads.emplace_back(
            query_keys<ConcurrentFilter*>,
            filter,
            keys_start,
            nvals_per_query_thread,
            sample_interval,
            std::ref(query_time[i]),
            start_time
        );
    }

    // Join finished threads
    for (auto& t : insert_threads) if (t.joinable()) t.join();
    stop_flag.store(true);
    for (auto& t : query_threads) if (t.joinable()) t.join();
    
    std::vector<double> insert_result;
    calculate_result(insert_time, insert_result, unit_time, sample_interval);
    for (uint64_t f = 0; f < insert_result.size(); ++f) {
        str_insert << ((double)f) / 1000.0 << "," << std::fixed << std::setprecision(10)
                   << ((insert_result[f]) / 1000 /*us to ms*/) << ",";
    }

    std::vector<double> query_result;
    calculate_result(query_time, query_result, unit_time, sample_interval);
    for (uint64_t f = 0; f < query_result.size(); ++f) {
        str_query << ((double)f) / 1000.0 << "," << std::fixed << std::setprecision(10)
                  << ((query_result[f]) / 1000 /*us to ms*/) << ",";
    }
    
    file_insert << str_insert.str() << std::endl;
    file_query << str_query.str() << std::endl;
    file_insert.close();
    file_query.close();

    delete filter;

    return 0;
}