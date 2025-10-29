#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "zenofilter_template.hpp"
#include "zenofiltervm_template.hpp"
#include "aleph_template.hpp"

#include "../util/cxxopts.hpp"

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("8"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("l,label", "Label of this experiment", cxxopts::value<std::string>())
      ("fn_query", "File name to write query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_insert", "File name to write insert results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t r = result["id"].as<uint64_t>();
    std::string label = result["label"].as<std::string>();

    // Files to write results
    std::string fn_query = result["fn_query"].as<std::string>();
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_insert = result["fn_insert"].as<std::string>();

    std::ofstream file_query(fn_query, std::ios::app);
    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_insert(fn_insert, std::ios::app);

    if (!file_query || !file_fpr || !file_insert) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    bool record_query = (fn_query != "/dev/null");

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }

    // Dummy keys for testing false positive rates
    uint64_t num_queries = 100'000;
    uint64_t* queries;
    queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
    size_t total_bytes = num_queries * sizeof(queries[0]);
    size_t done = 0;
    unsigned char* p = (unsigned char*)queries;
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

    zeno_bench::Filter* filter;
    if (r == 1) filter = new zeno_bench::ZenoFilter(qbits, qbits + fbits, 1);
    else if (r == 2) filter = new zeno_bench::ZenoFilterVM(qbits, qbits + fbits, 1);
    else if (r == 3) filter = new zeno_bench::Aleph(qbits, qbits + fbits);

    file_query << label << ",";
    file_fpr << label << ",";
    file_insert << label << ",";

    uint64_t insert_time = 0;
    uint64_t query_time = 0;
    uint64_t grow_time = 0;
    uint64_t interval = 0;

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
            if (record_query) {
                    for (uint64_t j = 0; j < num_queries; ++j) {
                    // uint64_t random_query = util::generate_random();
                    query_time += util::timing([&]{
                        ret = filter->query(queries[j]);
                    });
                    if (ret) {  // there is a false positive
                        ++num_positives;
                    }
                }
            }

            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
                     << ((double)num_positives) / ((double)num_queries) << ",";
            file_query << next_po2 << ","
                       << ((double)query_time) / ((double)num_queries * 1000.0 /*us*/) << ",";
            query_time = 0;

            // Resize filter if insertion fails
            ret = filter->resize();
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

            file_insert << next_po2 << ","
                        << ((double)insert_time + grow_time) / ((double)(i - interval) * 1000) << ",";

            insert_time = 0;
            grow_time = 0;

            interval = i;

            if (terminate_loop) break;
        }
    }

    file_query << std::endl;
    file_fpr << std::endl;
    file_insert << std::endl;

    file_query.close();
    file_fpr.close();
    file_insert.close();
    delete filter;

    return 0;
}