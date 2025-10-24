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
      ("fn_lf", "File name to write load factor results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t rratio = result["rratio"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();

    // Files to write results
    std::string fn_lf = result["fn_lf"].as<std::string>();

    std::ofstream file_lf(fn_lf, std::ios::app);

    if (!file_lf) {
        std::cerr << "Error opening file." << std::endl;
        return 1;
    }

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 80 * nvals / 100;

    bool auto_resize = false;
    
    // Configure filter by given ID
    zeno_bench::IZF* filter = new zeno_bench::IZF(qbits, qbits + fbits, rratio);
    filter->auto_resize(auto_resize);
    util::set_cpu_affinity(12);

    file_lf << filter->name(true) << "_" << fbits << ",";

    uint64_t insert_time = 0;

    // For calculating the number of insertions between intervals
    uint64_t sample_interval = 1ULL << qbits;
    uint64_t prev_interval = 0;

    // For storing query results
    bool ret = true;

    // Sampling Ratio
    double sampling_ratio = std::pow(2.0, 1.0/8.0);

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        uint64_t key = util::generate_random();
        // Calculate insert time
        insert_time += util::timing([&]{
            ret = filter->insert(key);
        });

        if (!ret) {
            filter->resize();
            insert_time += util::timing([&]{
                ret = filter->insert(key);
            });
        }

        if (i == sample_interval) {
            double interval = (double)(sample_interval - prev_interval);
            // Print insert time of the previous interval
            double spaceamp = filter->space_amplification();
            file_lf << sample_interval << "," << spaceamp << ",";
            // Cleanup
            insert_time = 0;
            prev_interval = sample_interval;
            sample_interval *= sampling_ratio;
        }
    }

    file_lf << std::endl;
    file_lf.close();
    delete filter;

    return 0;
}