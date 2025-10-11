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

void insert_keys(Zeno* zeno, const uint64_t* keys_start, const uint64_t* queries, uint64_t num_keys, 
                 uint64_t num_queries, uint8_t flags, std::vector<uint64_t>& insert_res, 
                 std::vector<uint64_t>& query_res, 
                 std::chrono::_V2::steady_clock::time_point start_time) {
    int64_t res = 0;
    volatile uint64_t count = 0;;
    uint64_t value = 0; 

    using clock = std::chrono::steady_clock;

    uint64_t base_idx = 0;

    auto start = clock::now();
    auto end = clock::now();

    for (uint64_t i = 0; i < num_keys; ++i) {
        // if (i <= 150000) std::cout << "[" << tid;
        res = zeno->insert(keys_start[i], 0, 1, flags);
        // if (i <= 150000) std::cout << "]";
        
        if (res < 0) {
            std::cerr << "insert failed.\n";
            std::cerr << "res : " << res << std::endl;
            res = i;
            break;
        }

        if (zeno->is_filter_growing()) {
            end = clock::now();
            // number of inserted entries
            insert_res.push_back(i - base_idx);
            // start time
            insert_res.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(start - start_time).count());
            // end time
            insert_res.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start_time).count());
            base_idx = i;

            uint64_t j = 0;
            start = clock::now();
            for (; j < num_queries; ++j) {
                count = zeno->concurrent_query(queries[j], value, kWaitForLock);
                // count = zeno->query(queries[j], value, kWaitForLock);
                if (!zeno->is_filter_growing()) {
                    break;
                }
            }
            end = clock::now();
            // number of queried entries
            query_res.push_back(j);
            // start time
            query_res.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(start - start_time).count());
            // end time
            query_res.emplace_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start_time).count());

            // Wait until growing finishes
            while (zeno->is_filter_growing()) {
                std::this_thread::yield();
            }
            start = clock::now();
        }
    }
}

void calculate_result(std::vector<std::vector<uint64_t>>& datas, std::vector<double>& result,
                      uint64_t quotient) {
    uint64_t min_size = 0ULL - 1;
    uint64_t max_size = 0ULL;
    uint64_t num_threads = datas.size();
    for (auto data : datas) {
        if (data.size() < min_size) min_size = data.size();
        if (data.size() > max_size) max_size = data.size();
    }
    for (uint64_t i = 0; i < max_size; i += 3) {
        uint64_t num_inserted = 0;
        uint64_t earliest_start_us = 0ULL - 1;
        uint64_t latest_end_us = 0;

        for (uint64_t j = 0; j < num_threads; ++j) {
            if (i >= datas[j].size()) continue;
            num_inserted += datas[j][i];
            if (earliest_start_us > datas[j][i + 1]) earliest_start_us = datas[j][i + 1];
            if (latest_end_us < datas[j][i + 2]) latest_end_us = datas[j][i + 2];
        }
        
        uint64_t elapsed_time = latest_end_us - earliest_start_us;
        double average_time = (double)elapsed_time / (double)num_inserted;

        uint64_t num_entries = 1ULL << quotient;
        result.push_back(num_entries);
        ++quotient;
        result.push_back(average_time);
    }
    // for (uint64_t i = 0; i < min_size; i += 3) {
    //     uint64_t num_inserted = 0;
    //     uint64_t earliest_start_us = 0ULL - 1;
    //     uint64_t latest_end_us = 0;

    //     for (uint64_t j = 0; j < num_threads; ++j) {
    //         num_inserted += datas[j][i];
    //         if (earliest_start_us > datas[j][i + 1]) earliest_start_us = datas[j][i + 1];
    //         if (latest_end_us < datas[j][i + 2]) latest_end_us = datas[j][i + 2];
    //     }
        
    //     uint64_t elapsed_time = latest_end_us - earliest_start_us;
    //     double average_time = (double)elapsed_time / (double)num_inserted;

    //     uint64_t num_entries = 1ULL << quotient;
    //     result.push_back(num_entries);
    //     ++quotient;
    //     result.push_back(average_time);
    // }
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
      ("fn_in", "File name to write insert results", cxxopts::value<std::string>())
      ("fn_rd", "File name to write read results", cxxopts::value<std::string>())
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
    std::string fn_in = result["fn_in"].as<std::string>();
    std::string fn_rd = result["fn_rd"].as<std::string>();

    std::ofstream file_in(fn_in, std::ios::app);
    std::ofstream file_rd(fn_rd, std::ios::app);

    if (!file_in || !file_rd ) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    file_in << label << ",";
    file_in << std::fixed;
    file_rd << label << ",";
    file_rd << std::fixed;

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

    // Generate insert keys
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

    // Generate query keys
    uint64_t nqueries_per_thread = 500'000;
    uint64_t num_queries = nqueries_per_thread * num_threads;
    uint64_t* queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
    total_bytes = num_queries * sizeof(queries[0]);
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

    // In the scripts, num_threads = 0 is reserved for using kNoLock
    uint8_t flags = num_threads > 0 ? kWaitForLock : kNoLock;
    if (num_threads == 0) num_threads = 1;

    std::vector<std::thread> threads;
    std::vector<std::vector<uint64_t>> insert_time(num_threads);
    std::vector<std::vector<uint64_t>> query_time(num_threads);

    uint64_t nvals_per_thread = nvals / num_threads;

    const auto start = std::chrono::high_resolution_clock::now();

    using clock = std::chrono::steady_clock;
    auto start_time = clock::now();

    for (uint64_t i = 0; i < num_threads; ++i) {
        const uint64_t* keys_start = keys + (i * nvals_per_thread);
        const uint64_t* queries_start = queries + (i * nqueries_per_thread);
        threads.emplace_back(
            insert_keys,
            zeno,
            keys_start,
            queries_start,
            nvals_per_thread,
            num_queries, 
            flags,
            std::ref(insert_time[i]),
            std::ref(query_time[i]),
            start_time
        );
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    const auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::vector<double> insert_result;
    calculate_result(insert_time, insert_result, qbits);

    std::vector<double> query_result;
    calculate_result(query_time, query_result, qbits);

    for (auto ir : insert_result) file_in << ir << ",";
    file_in << std::endl;
    for (auto ir : query_result) file_rd << ir << ",";
    file_rd << std::endl;

    // // Print results to file_oa
    // double seconds = ((double)ns) / 1e9;
    // double mops = (((double)nvals) / seconds) / 1e6;
    // file_oa << ((double)ns) / 1'000'000'000.0 << "," 
    //         << ((double)zeno->get_grow_time()) / 1'000'000.0 << std::endl;

    // std::vector<double> final_result;
    // std::vector<double> final_result_dm;

    // calculate_result(aggregate_time, final_result, unit_time, sample_interval);
    // calculate_latency(aggregate_time, final_result_dm, sample_interval);
    // // std::cout << "total time ? : " << ((double)total_time) / 1000.0 << std::endl;

    // for (uint64_t f = 0; f < final_result.size(); ++f) {
    //     file_tp << ((double)f) / 1000.0 << "," << std::fixed << std::setprecision(10)
    //             << ((final_result[f]) / 1000 /*us to ms*/) << ",";
    // }
    // for (uint64_t f = 0; f < final_result_dm.size(); ++f) {
    //     file_dm << ((double)f) * sample_interval << "," << std::fixed << std::setprecision(10)
    //             << ((final_result_dm[f])) << ",";
    // }
    // file_tp << std::endl;
    // file_dm << std::endl;

    file_in.close();
    file_rd.close();
    // file_dm.close();

    return 0;
}