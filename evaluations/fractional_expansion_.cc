#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <openssl/rand.h>

#include "zeno.hpp"

#include "../util/cxxopts.hpp"
#include "../util/util.hpp"

using namespace zeno_headeronly;

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
      ("fn_insert", "File name to write insertion results to", cxxopts::value<std::string>())
      ("fn_query", "File name to write query results to", cxxopts::value<std::string>())
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>())
      ("fn_amp", "File name to write space/write amp results to", cxxopts::value<std::string>())
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

    std::string label = result["label"].as<std::string>();

    // Files to write results
    std::string fn_insert = result["fn_insert"].as<std::string>();
    std::string fn_query = result["fn_query"].as<std::string>();
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_amp = result["fn_amp"].as<std::string>();

    std::ofstream file_insert(fn_insert, std::ios::app);
    std::ofstream file_query(fn_query, std::ios::app);
    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_amp(fn_amp, std::ios::app);

    if (!file_insert || !file_query || !file_fpr || !file_amp) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    file_insert << label << ",";
    file_query << label << ",";
    file_fpr << label << ",";

    // util::set_cpu_affinity(12);

    // Configure zeno filter
    Zeno* zeno = new Zeno(qbits, qbits + fbits, vbits, rratio, 
                          hashmode::Default, seed);
    zeno->set_auto_resize(false);

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 95 * nvals / 100;
    uint64_t* keys;
    uint64_t* vals;

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
    uint64_t num_queries = 500'000;
    queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
    RAND_bytes((unsigned char*)queries, sizeof(*queries) * num_queries);

    uint64_t query_time = 0;
    uint64_t count = 0;
    uint64_t value;
    uint64_t num_positives = 0;
    uint64_t insert_time = 0;
    int idx = 0;

    uint64_t interval = 0;
    uint64_t prev_interval = 0;
    ++idx;

    // For total write amplification
    uint64_t total_insert_time = 0;
    uint64_t total_grow_time = 0;
    // For total space amplification
    double num_expansions = 0;
    double total_space_amp = 0;

    // Execute evaluations
    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = 0;
        // 1. Calculate insert time
        insert_time += util::timing([&]{
            ret = zeno->insert(keys[i], /* set val = 0 for now */ 0, key_count, kNoLock);
        });
        
        if (ret < 0) {
            total_grow_time += util::timing([&]{
                ret = zeno->grow(0, 0, kNoLock);
            });
            if (ret < 0) {
                // std::cerr << "failed increasing filter size" << std::endl;
                break;
            }
            insert_time += util::timing([&]{
                ret = zeno->insert(keys[i], 0, key_count, kNoLock);
            });

            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            // Print insert time during the interval
            file_insert << next_po2 << "," 
                        << ((double)insert_time) / ((double)(i - interval) * 1000) << ",";
            interval = i;

            total_insert_time += insert_time;
            insert_time = 0;

            // 2. Calculate FPR and query time
            for (int j = 0; j < num_queries; ++j) {
                query_time += util::timing([&]{
                    count = zeno->query(queries[j], value, kNoLock);
                });
                if (count >= key_count) {   // there is a false positive
                    ++num_positives;
                }
            }
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8) 
                     << ((double)num_positives) / ((double)num_queries) << ",";
            file_query << next_po2 << "," 
                       << ((double)query_time) / ((double)num_queries * 1000) << ",";

            query_time = 0;
            num_positives = 0;

            // 3. Update space/write amplification
            total_space_amp += zeno->get_space_amplification();
            num_expansions += 1;
        }
    }

    // Report total write time and space amplification only in Zeno
    if (rratio > 0) {
        uint64_t total_time = total_insert_time + total_grow_time;
        file_amp << label << "," << (((double)total_time) / (1'000'000'000.0));
                //  << "," << (total_space_amp / num_expansions);
        file_amp << std::endl;
    }

    file_insert << std::endl;
    file_query << std::endl;
    file_fpr << std::endl;

    file_insert.close();
    file_query.close();
    file_fpr.close();
    file_amp.close();
    
    return 0;
}