#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "vzf_template.hpp"
#include "ldcf_template.hpp"
#include "aleph_template.hpp"
#include "quotient_template.hpp"
#include "e2cf_template.hpp"
#include "ebf_template.hpp"

// Bamboo only works on x86
#if defined(__x86_64__)
#include "bamboo_template.hpp"
#endif

#include "../util/cxxopts.hpp"

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_size", "File name to write size results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_pquery", "File name to write positive query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_nquery", "File name to write negative query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_insert", "File name to write space/write amp results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();

    // Files to write results
    std::string fn_size = result["fn_size"].as<std::string>();
    std::string fn_pquery = result["fn_pquery"].as<std::string>();
    std::string fn_nquery = result["fn_nquery"].as<std::string>();
    std::string fn_fpr = result["fn_fpr"].as<std::string>();
    std::string fn_insert = result["fn_insert"].as<std::string>();

    std::ofstream file_size(fn_size, std::ios::app);
    std::ofstream file_pquery(fn_pquery, std::ios::app);
    std::ofstream file_nquery(fn_nquery, std::ios::app);
    std::ofstream file_fpr(fn_fpr, std::ios::app);
    std::ofstream file_insert(fn_insert, std::ios::app);

    if (!file_size || !file_pquery || !file_nquery || !file_fpr || !file_insert) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    // Parameters for benchmark
    bool record_size = (fn_size != "/dev/null");
    bool record_fpr = (fn_fpr != "/dev/null");
    bool record_insert = (fn_insert != "/dev/null");
    bool record_pquery = (fn_pquery != "/dev/null");
    bool record_nquery = (fn_nquery != "/dev/null");

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 90 * nvals / 100;

    // Generate keys for insertion
    uint64_t* keys = nullptr;

    if (record_pquery) {
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
    }

    // Dummy keys for testing false positive rates
    uint64_t* queries = nullptr;
    uint64_t num_queries = 100'000;

    if (record_nquery) {
        queries = (uint64_t*)malloc(num_queries * sizeof(queries[0]));
        RAND_bytes((unsigned char*)queries, sizeof(*queries) * num_queries);
    }

    constexpr uint64_t EXP_COEF = 1;    // Exp coef r = 1

    bool auto_resize = false;
    
    // Configure filter by given ID
    zeno_bench::Filter* filter;
    if (id == 1) {
        filter = new zeno_bench::VZF(qbits, qbits + fbits, EXP_COEF);
        filter->auto_resize(false);
        auto_resize = false;
    }
    else if (id == 2) {
#if defined(__x86_64__)
        filter = new zeno_bench::Bamboo(qbits);
#else
        delete filter;
        return 0;
#endif
        auto_resize = true;
    }
    else if (id == 3) {
        filter = new zeno_bench::LDCF(qbits, fbits);
        filter->auto_resize(false);
        auto_resize = false;
    }
    else if (id == 4) {
        filter = new zeno_bench::Aleph(qbits, qbits + fbits);
        filter->auto_resize(false);
        auto_resize = false;
    }
    else if (id == 5) {
        filter = new zeno_bench::Quotient(qbits, qbits + fbits);
        filter->auto_resize(false);
        auto_resize = false;
    }
    else if (id == 6) {
        filter = new zeno_bench::E2CF(qbits, fbits);
        filter->auto_resize(false);
        auto_resize = false;
    }
    else if (id == 7) {
        filter = new zeno_bench::EBF(qbits);
        filter->auto_resize(true);
        auto_resize = true;
    }

    util::set_cpu_affinity(1);

    file_size << filter->name(true) << ",";
    file_pquery << filter->name(true) << ",";
    file_nquery << filter->name(true) << ",";
    file_fpr << filter->name(true) << ",";
    file_insert << filter->name(true) << ",";

    uint64_t insert_time = 0;
    uint64_t query_time = 0;
    // For total write amplification
    uint64_t total_insert_time = 0;
    uint64_t total_grow_time = 0;

    // For calculating the number of insertions between intervals
    uint64_t sample_interval = 1ULL << qbits;
    uint64_t prev_interval = 0;

    uint64_t query_index = 0;

    // For storing query results
    bool ret = true;

    // For checking if sampling was done via expansion
    bool sampling_done = false;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        uint64_t key = record_pquery ? keys[i] : util::generate_random();
        // Calculate insert time
        insert_time += util::timing([&]{
            ret = filter->insert(key);
        });

        if ((sampling_done == false && i == sample_interval) || !ret) {
            bool insert_failed = !ret;

            // Calculate FPR and negative query time
            uint64_t num_positives = 0;

            // Record negative query
            if (record_nquery) {
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

            if (insert_failed) {
                insert_time += util::timing([&]{
                    filter->resize();
                    filter->insert(key);
                });
                sampling_done = true;
            }

            uint64_t next_po2 = sampling_done ? 1ULL << (64 - __builtin_clzll(i)) : sample_interval;

            double interval = (double)(next_po2 - prev_interval);
            // Print insert time of the previous interval
            if (record_insert)
            file_insert << next_po2 << "," 
                        << ((double)insert_time) / (interval * 1000.0 /*us*/) << ",";
            // Print fpr of the previous interval
            if (record_fpr)
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
                    << ((double)num_positives) / ((double)num_queries) << ",";
            // Print negative query time of the previous interval
            if (record_nquery) 
            file_nquery << next_po2 << ","
                        << ((double)query_time) / ((double)num_queries * 1000.0 /*us*/) << ",";

            // Calculate positive query time
            query_time = 0;
            num_positives = 0;

            if (record_pquery) {
                uint64_t lim = std::min(query_index + num_queries, i);
                uint64_t num_positive_queries = lim - query_index;
                for (; query_index < lim; ++query_index) {
                    query_time += util::timing([&]{
                        ret = filter->query(keys[query_index]);
                    });
                    if (ret) {
                        ++num_positives;
                    } else {
                        if (id == 1) std::cerr << "fail at " << query_index << ",";
                        // abort();
                    }
                }
                if (num_positives != num_positive_queries) {
                    if (id == 1) std::cerr << (num_positive_queries - num_positives) << " positive queries failed at " << i << std::endl;
                }
                // Print positive query time of the previous interval
                file_pquery << next_po2 << ","
                            << ((double)query_time) / ((double)num_queries * 1000.0 /*us*/) << ",";
            }

            if (record_size) {
                file_size << next_po2 << "," << ((double)filter->size() / 1'000'000.0) << ",";
            }

            // Cleanup
            query_time = 0;
            insert_time = 0;
            prev_interval = next_po2;
            sample_interval <<= 1;

            if (!filter->is_valid()) break;
        }
    }

    file_size << std::endl;
    file_pquery << std::endl;
    file_nquery << std::endl;
    file_fpr << std::endl;
    file_insert << std::endl;

    file_size.close();
    file_pquery.close();
    file_nquery.close();
    file_fpr.close();
    file_insert.close();
    // delete filter;

    return 0;
}