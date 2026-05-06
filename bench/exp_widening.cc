#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

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
      ("fn_fpr", "File name to write fpr results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();

    // Files to write results
    std::string fn_fpr = result["fn_fpr"].as<std::string>();

    std::ofstream file_fpr(fn_fpr, std::ios::app);

    if (!file_fpr) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    bool record_fpr = (fn_fpr != "/dev/null");

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    uint64_t* keys;

    // Dummy keys for testing false positive rates
    uint64_t* queries;
    uint64_t num_queries = 1'000'000;
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

    // Filter configuration
    zeno_bench::Filter* filter;
    if (id == 1) {
        filter = new zeno_bench::ZenoFilterVM(qbits, qbits + fbits);
        if (record_fpr) file_fpr << filter->name(false) << "noW,";
    }
    else if (id == 2) {
        filter = new zeno_bench::ZenoFilterVM(qbits, qbits + fbits);
        if (record_fpr) file_fpr << filter->name(false) << "W,";
    }
    else if (id == 3) {
        filter = new zeno_bench::Aleph(qbits, qbits + fbits);
        if (record_fpr) file_fpr << filter->name(false) << "W,";
    }
    filter->auto_resize(false);

    

    // For storing query / resize results
    bool ret = false;

    // For terminating execution when filter expansion fails. 
    bool terminate_loop = false;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        // Calculate insert time
        ret = filter->insert(util::generate_random());

        if (!ret) {
            // Calculate FPR and query time
            uint64_t num_positives = 0;

            if (record_fpr)
            for (uint64_t j = 0; j < num_queries; ++j) {
                ret = filter->query(queries[j]);
                if (ret) {  // there is a false positive
                    ++num_positives;
                }
            }
            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));

            // Print fpr of the previous interval
            if (record_fpr)
            file_fpr << next_po2 << "," << std::fixed << std::setprecision(8)
                     << ((double)num_positives) / ((double)num_queries) << ",";

            ret = filter->resize();
            if (!ret) {
                terminate_loop = true;
            }

            if (terminate_loop) break;
        }
    }

    if (record_fpr) file_fpr << std::endl;

    file_fpr.close();

    delete filter;

    return 0;
}