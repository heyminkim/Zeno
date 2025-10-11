#include <cmath>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <openssl/rand.h>

#include "zeno.hpp"

#include "../util/cxxopts.hpp"
#include "../util/util.hpp"

using namespace zeno;

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
    std::cout << label << ",";

    util::set_cpu_affinity(16);

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
    keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    RAND_bytes((unsigned char*)keys, sizeof(*keys) * nvals);
    srand(0);
    for (uint64_t i = 0; i < nvals; ++i) {
        keys[i] = (1 * keys[i]) % zeno->get_hash_range();
    }

    // For adjusting the evaluation for Zeno
    uint64_t num_expansions = 0;
    uint64_t value, count;
    uint64_t num_positives = 0;
    uint64_t total_time = 0;

    total_time = util::timing([&]{
        
    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = 0;

        ret = zeno->insert(keys[i], 0, key_count, kNoLock);
        
        if (ret < 0) {
            ret = zeno->grow(0, 0, 0);
            if (ret < 0) {
                std::cerr << "failed increasing filter size" << std::endl;
                abort();
            }
            ret = zeno->insert(keys[i], 0, key_count, kNoLock);
        }
    }
    });
    
    std::cout << ((double)total_time) / ((double)1'000'000'000)<< std::endl;
    
    return 0;
}