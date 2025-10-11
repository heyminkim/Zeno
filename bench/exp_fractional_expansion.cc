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
    nvals = 95 * nvals / 100;
    uint64_t* keys;

    // Generate keys for insertion
    keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
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

    // Dummy keys for testing false positive rates
    uint64_t* queries;
    uint64_t num_queries = 100'000;
    queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
    RAND_bytes((unsigned char*)queries, sizeof(*queries) * num_queries);

    zeno_bench::Filter* filter = new zeno_bench::IZF(qbits, qbits + fbits, r);

    file_size << filter->name(true) << "_" << fbits << ",";
    file_query << filter->name(true) << "_" << fbits << ",";
    file_fpr << filter->name(true) << "_" << fbits << ",";
    file_amp << filter->name(true) << "_" << fbits << ",";

    uint64_t insert_time = 0;
    uint64_t query_time = 0;
    // For total write amplification
    uint64_t total_insert_time = 0;
    uint64_t total_grow_time = 0;

    // For storing query / resize results
    bool ret = false;

    // For terminating execution when filter expansion fails. 
    bool terminate_loop = false;

    uint64_t last_insert = 0;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        // Calculate insert time
        insert_time += util::timing([&]{
            ret = filter->insert(keys[i]);
        });

        if (!ret) {
            // Calculate FPR and query time
            uint64_t num_positives = 0;
            for (uint64_t j = 0; j < num_queries; ++j) {
                query_time += util::timing([&]{
                    ret = filter->query(queries[j]);
                });
                if (ret) {  // there is a false positive
                    ++num_positives;
                }
            }
            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));

            // Print filter size of the previous interval
            // file_size << i << "," << ((double)filter->size() / 1'000'000.0) << ",";
            // Print fpr of the previous interval
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
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
                    ret = filter->insert(keys[i]);
                });
                if (!ret) {
                    std::cerr << "failed inserting key for " << filter->name() << std::endl;
                    return false;
                }
            }

            uint64_t n_inserts = i - last_insert;
            last_insert = i;
            file_size << next_po2 << "," << ((double)insert_time) / ((double)n_inserts * 1000.0/*us*/) << ",";

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