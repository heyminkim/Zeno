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
#include "vzf_template.hpp"
#include "aleph_template.hpp"

#include "../util/cxxopts.hpp"

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("8"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("r,ratio", "Expansion ratio", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_size", "File name to write size results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_query", "File name to write query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_amp", "File name to write space/write amp results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t r = result["ratio"].as<uint64_t>();

    // Files to write results
    std::string fn_size = result["fn_size"].as<std::string>();
    std::string fn_query = result["fn_query"].as<std::string>();
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_amp = result["fn_amp"].as<std::string>();

    std::ofstream file_size(fn_size, std::ios::app);
    std::ofstream file_query(fn_query, std::ios::app);
    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_amp(fn_amp, std::ios::app);

    if (!file_size || !file_query || !file_fpr || !file_amp) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }

    uint64_t num_queries = 500'000;

    zeno_bench::Filter* filter;
    if (r <= 4) filter = new zeno_bench::IZF(qbits, qbits + fbits, r);
    else if (r == 5) filter = new zeno_bench::Aleph(qbits, qbits + fbits);
    else if (r == 6) filter = new zeno_bench::Aleph(qbits, qbits + fbits - 1);
    else if (r == 7) filter = new zeno_bench::VZF(qbits, qbits + fbits, 1);

    util::set_cpu_affinity(1);
    file_size << filter->name(true) << ",";
    file_query << filter->name(true) << ",";
    file_fpr << filter->name(true) << ",";
    file_amp << filter->name(true) << ",";

    uint64_t insert_time = 0;
    uint64_t query_time = 0;
    // For total write amplification
    uint64_t total_insert_time = 0;
    uint64_t total_grow_time = 0;

    // For storing query / resize results
    bool ret = false;

    // For terminating execution when filter expansion fails. 
    bool terminate_loop = false;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        uint64_t random_key = util::generate_random();

        // Calculate insert time
        insert_time += util::timing([&]{
            ret = filter->insert(random_key);
        });

        if (!ret) {
            // Calculate FPR and query time
            uint64_t num_positives = 0;
            for (uint64_t j = 0; j < num_queries; ++j) {
                uint64_t random_query = util::generate_random();
                query_time += util::timing([&]{
                    ret = filter->query(random_query);
                });
                if (ret) {  // there is a false positive
                    ++num_positives;
                }
            }

            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            // Print filter size of the previous interval
            file_size << i << "," << ((double)filter->size() / 1'000'000.0) << ",";
            // Print fpr of the previous interval
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(10)
                     << ((double)num_positives) / ((double)num_queries) << ",";
            // Print worst-case query time of the previous interval
            file_query << next_po2 << ","
                       << ((double)query_time) / ((double)num_queries * 1000.0 /*us*/) << ",";
            query_time = 0;

            // Resize filter if insertion fails
            total_grow_time += util::timing([&]{
                ret = filter->resize();
            });
            if (!ret) {
                terminate_loop = true;
            }

            if (!terminate_loop) {
                // Insert the failed key
                insert_time += util::timing([&]{
                    ret = filter->insert(random_key);
                });
                if (!ret) {
                    std::cerr << "failed inserting key for " << filter->name() << std::endl;
                    return false;
                }
            }

            total_insert_time += insert_time;
            insert_time = 0;

            if (terminate_loop) break;
        }
    }

    uint64_t total_time = total_insert_time + total_grow_time;
    file_amp << ((double)total_time) / (1'000'000'000.0 /*s*/);

    file_size << std::endl;
    file_query << std::endl;
    file_fpr << std::endl;
    file_amp << std::endl;

    file_size.close();
    file_query.close();
    file_fpr.close();
    file_amp.close();
    delete filter;

    return 0;
}