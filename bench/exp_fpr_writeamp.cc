#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "izf_template.hpp"

#include "../util/cxxopts.hpp"

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("r,rratio", "Expansion ratio", cxxopts::value<uint64_t>()->default_value("1"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_fpr", "File name to write insert thpt results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_amp", "File name to write load factor results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t rratio = result["rratio"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();

    // Files to write results
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_amp = result["fn_amp"].as<std::string>();

    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_amp(fn_amp, std::ios::app);

    if (!file_fpr || !file_amp) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 90 * nvals / 100;

    // Dummy keys for testing false positive rates
    uint64_t* queries;
    uint64_t num_queries = 100'000;
    queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
    RAND_bytes((unsigned char*)queries, sizeof(*queries) * num_queries);

    bool auto_resize = false;
    
    // Configure filter by given ID
    zeno_bench::IZF* filter = new zeno_bench::IZF(qbits, qbits + fbits, rratio);
    filter->auto_resize(auto_resize);
    util::set_cpu_affinity(16);

    file_fpr << filter->name(true) << "_" << fbits << ",";
    file_amp << filter->name(true) << "_" << fbits << ",";

    // For total write amplification
    uint64_t total_insert_time = 0;

    // For calculating the number of insertions between intervals
    uint64_t prev_interval = 0;

    // For storing query results
    bool ret = true;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        uint64_t key = util::generate_random();
        // Calculate insert time
        total_insert_time += util::timing([&]{
            ret = filter->insert(key);
        });

        // Check FPR if insertion fails
        if (!ret) {
            // Account for resizing time
            total_insert_time += util::timing([&]{
                filter->resize();
                filter->insert(key);
            });

            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            uint64_t num_positives = 0;
            for (uint64_t j = 0; j < num_queries; ++j) {
                ret = filter->query(queries[j]);
                if (ret) {
                    ++num_positives;
                }
            }

            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
                     << ((double)num_positives) / ((double)num_queries) << ",";
        }
    }

    file_amp << ((double)total_insert_time) / (1'000'000'000.0 /*s*/);
    file_fpr << std::endl;
    file_amp << std::endl;
    file_fpr.close();
    file_amp.close();
    delete filter;

    return 0;
}