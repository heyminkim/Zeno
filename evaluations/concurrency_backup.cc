#include <cmath>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <thread>
#include <utility>
#include <openssl/rand.h>

#include "zeno.hpp"

#include "../util/cxxopts.hpp"
#include "../util/util.hpp"

using namespace zeno;

void insert_keys(Zeno* zeno, const uint64_t* keys_start, size_t num_keys, uint8_t flags,
                 std::chrono::_V2::steady_clock::time_point start, 
                 uint64_t interval, std::vector<uint64_t>& result) {
    int64_t res = 0; 

    using clock = std::chrono::steady_clock;
    auto now = clock::now();

    // std::cout << "tid : " << std::this_thread::get_id() << std::endl;

    for (size_t i = 0; i < num_keys; ++i) {
        res = zeno->insert(keys_start[i], 0, 1, flags);
        if (res < 0) {
            std::cerr << "res : " << res << std::endl;
            res = i;
            break;
        }

        if (i != 0 && i % interval == 0) {
            now = clock::now();
            auto us_since_last_sample = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
            result.emplace_back(us_since_last_sample);
        }
    }
}

uint64_t calculate_result(std::vector<std::vector<uint64_t>>& datas, std::vector<double>& result, 
                      uint64_t unit_time, uint64_t sample_interval) {
    uint64_t num_threads = datas.size();
    uint64_t min_total_time = (0ULL - 1);
    uint64_t max_total_time = 0ULL;

    // For each thread result
    for (const auto& data : datas) {
        if (data[data.size() - 1] < min_total_time) min_total_time = data[data.size() - 1];
        // if (data[data.size() - 1] > max_total_time) max_total_time = data[data.size() - 1];
    }

    uint64_t num_samples = 1 + min_total_time / unit_time;
    // uint64_t num_samples = 1 + max_total_time / unit_time;

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

uint64_t calculate_latency(std::vector<std::vector<uint64_t>>& datas, std::vector<double>& result, 
                           uint64_t sample_interval) {
    uint64_t num_threads = datas.size();

    // Resize vector. resize fails for some reason..
    for (uint64_t i = 1; i < datas[0].size(); ++i) {
        result.push_back(0);
    }

    for (uint64_t t = 0; t < num_threads; ++t) {
        const auto& data = datas[t];
        
        for (uint64_t d = 1; d < data.size(); ++d) {
            double elapsed_time = static_cast<double>(data[d] - data[d - 1]);
            result[d - 1] += sample_interval / elapsed_time;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("8"))
      ("v,value", "Length of value in bits", cxxopts::value<uint64_t>()->default_value("0"))
      ("r,rratio", "Reciprocal ratio", cxxopts::value<uint64_t>()->default_value("2"))
      ("s,seed", "Seed for hash", cxxopts::value<uint32_t>()->default_value("0"))
      ("c,count", "Number of keys per insertion", cxxopts::value<uint64_t>()->default_value("1"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("l,label", "Label of this experiment", cxxopts::value<std::string>())
      ("t,threads", "Number of threads", cxxopts::value<uint64_t>()->default_value("1"))
      ("a,threshold", "Expansion threshold", cxxopts::value<double>()->default_value("0.8"))
      ("fn_tp", "File name to write fpr results to", cxxopts::value<std::string>())
      ("fn_oa", "File name to write space/write amp results to", cxxopts::value<std::string>())
      ("fn_dm", "Backup file name, used for debugging", cxxopts::value<std::string>())
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t vbits = result["value"].as<uint64_t>();
    uint64_t rratio = result["rratio"].as<uint64_t>();
    uint64_t seed = result["seed"].as<uint32_t>();
    uint64_t key_count = result["count"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    double threshold = result["threshold"].as<double>();
    uint64_t num_threads = result["threads"].as<uint64_t>();
    if (num_threads == 0) threshold = 0.95;

    std::string label = result["label"].as<std::string>();
    
    // Files to write results
    std::string fn_tp = result["fn_tp"].as<std::string>();
    std::string fn_oa = result["fn_oa"].as<std::string>();
    std::string fn_dm = result["fn_dm"].as<std::string>();

    std::ofstream file_tp(fn_tp, std::ios::app);
    std::ofstream file_oa(fn_oa, std::ios::app);
    std::ofstream file_dm(fn_dm, std::ios::app);

    if (!file_tp || !file_oa || !file_dm) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    file_tp << label << ",";
    file_oa << label << ",";
    file_dm << label << ",";

    // Configure zeno filter
    Zeno* zeno = new Zeno(qbits, qbits + fbits, vbits, rratio, 
                          hashmode::Default, seed, threshold);
    zeno->set_auto_resize(true);

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 85 * nvals / 100;

    uint64_t* keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    // RAND_bytes((unsigned char*)keys, sizeof(*keys) * nvals);
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

    // In the scripts, num_threads = 0 is reserved for using kNoLock
    uint8_t flags = num_threads > 0 ? kWaitForLock : kNoLock;
    if (num_threads == 0) num_threads = 1;

    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> aggregate_time(num_threads);
    uint64_t sample_interval = 10000;
    uint64_t unit_time = 1000;

    uint64_t nvals_per_thread = nvals / num_threads;

    const auto start = std::chrono::high_resolution_clock::now();

    using clock = std::chrono::steady_clock;
    auto start_time = clock::now();

    for (uint64_t i = 0; i < num_threads; ++i) {
        const uint64_t* keys_start = keys + (i * nvals_per_thread);
        threads.emplace_back(
            insert_keys,
            zeno,
            keys_start,
            static_cast<size_t>(nvals_per_thread),
            flags,
            start_time,
            sample_interval,
            std::ref(aggregate_time[i])
        );
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    // Print results to file_oa
    double seconds = ((double)ns) / 1e9;
    double mops = (((double)nvals) / seconds) / 1e6;
    file_oa << ((double)ns) / 1'000'000'000.0 << "," 
            << ((double)zeno->get_grow_time()) / 1'000'000.0 << std::endl;

    std::vector<double> final_result;
    std::vector<double> final_result_dm;

    calculate_result(aggregate_time, final_result, unit_time, sample_interval);
    calculate_latency(aggregate_time, final_result_dm, sample_interval);
    // std::cout << "total time ? : " << ((double)total_time) / 1000.0 << std::endl;

    for (uint64_t f = 0; f < final_result.size(); ++f) {
        file_tp << ((double)f) / 1000.0 << "," << std::fixed << std::setprecision(10)
                << ((final_result[f]) / 1000 /*us to ms*/) << ",";
    }
    for (uint64_t f = 0; f < final_result_dm.size(); ++f) {
        file_dm << ((double)f) * sample_interval << "," << std::fixed << std::setprecision(10)
                << ((final_result_dm[f])) << ",";
    }
    file_tp << std::endl;
    file_dm << std::endl;

    file_tp.close();
    file_oa.close();
    file_dm.close();

    return 0;
}