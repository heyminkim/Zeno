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
      ("fn_amp", "File name to write max space amp results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_query", "File name to write avg query latency results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_avginsert", "File name to write avg insert latency results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_avgexpand", "File name to write avg expand latency results to", cxxopts::value<std::string>()->default_value("/dev/null"))
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
    std::string fn_query = result["fn_query"].as<std::string>();
    std::string fn_avginsert = result["fn_avginsert"].as<std::string>();
    std::string fn_avgexpand = result["fn_avgexpand"].as<std::string>();

    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_amp(fn_amp, std::ios::app);
    std::ofstream file_query(fn_query, std::ios::app);
    std::ofstream file_avginsert(fn_avginsert, std::ios::app);
    std::ofstream file_avgexpand(fn_avgexpand, std::ios::app);

    if (!file_fpr || !file_amp || !file_query || !file_avginsert || !file_avgexpand) {
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
    file_query << filter->name(true) << "_" << fbits << ",";
    file_avginsert << filter->name(true) << "_" << fbits << ",";
    file_avgexpand << filter->name(true) << "_" << fbits << ",";

    // For total write amplification
    uint64_t total_insert_time = 0;
    uint64_t total_expand_time = 0;
    uint64_t total_query_time = 0;
    double max_space_amplification = 0.0;

    // For calculating the number of insertions between intervals
    uint64_t prev_interval = 0;

    // For storing query results
    bool ret = true;

    uint64_t nexpands = 0;

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
            total_expand_time += util::timing([&]{
                filter->resize();
            });

            double current_spaceamp = filter->space_amplification();
            if (current_spaceamp > max_space_amplification) {
                max_space_amplification = current_spaceamp;
            }

            total_insert_time += util::timing([&]{
                filter->insert(key);
            });

            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            uint64_t num_positives = 0;
            for (uint64_t j = 0; j < num_queries; ++j) {
                total_query_time += util::timing([&]{
                    ret = filter->query(queries[j]);
                });
                if (ret) {
                    ++num_positives;
                }
            }
            ++nexpands;

            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
                     << ((double)num_positives) / ((double)num_queries) << ",";
        }
    }

    // file_amp << ((double)total_insert_time) / (1'000'000'000.0 /*s*/);
    file_amp << max_space_amplification;
    file_avginsert << ((double)total_query_time) / (num_queries * nexpands * 1'000.0) << ",";
    file_avginsert << ((double)total_insert_time) / (nvals * 1'000.0 /*us*/) << ",";
    total_insert_time += total_expand_time;
    file_avginsert << ((double)total_insert_time) / (nvals * 1'000.0 /*us*/) << ",";
    file_fpr << std::endl;
    file_amp << std::endl;
    file_query << std::endl;
    file_avginsert << std::endl;
    file_avgexpand << std::endl;
    file_fpr.close();
    file_amp.close();
    file_query.close();
    file_avginsert.close();
    file_avgexpand.close();
    delete filter;

    return 0;
}