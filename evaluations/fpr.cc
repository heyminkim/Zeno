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
    // keys = util::read_vals("fpr.data", nvals);
    keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    RAND_bytes((unsigned char*)keys, sizeof(*keys) * nvals);
    srand(0);
    util::dump_vals("fpr.data", keys, nvals);

    // Dummy keys for testing false positive rates
    uint64_t* dummy;
    uint64_t num_queries = 500'000;
    // dummy = util::read_vals("fpr.query", num_queries);
    dummy = (uint64_t*)malloc(num_queries * sizeof(dummy[0]));
    RAND_bytes((unsigned char*)dummy, sizeof(*dummy) * num_queries);
    util::dump_vals("fpr.query", dummy, num_queries);

    // For adjusting the evaluation for Zeno
    uint64_t value, count;
    uint64_t num_positives = 0;

    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = 0;
        ret = zeno->insert(keys[i], 0, key_count, kNoLock);

        if (ret < 0) {
            // RSQF has run out of bits. 
            if (ret == kErrNoFpBits) {
                std::cout << std::endl;
                return 0;
            }

            ret = zeno->grow(0, 0, 0);
            ret = zeno->insert(keys[i], 0, key_count, kNoLock);
            if (ret < 0) {
                std::cerr << "failed increasing filter size" << std::endl;
                break;
            }

            for (int j = 0; j < num_queries; ++j) {
                count = zeno->query(dummy[j], value, kNoLock);
                if (count >= key_count) { // there is a false positive
                    ++num_positives;
                }
            }
            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            std::cout << next_po2 << "," << std::fixed << std::setprecision(8) << ((double)num_positives) / ((double)num_queries) << ",";

            num_positives = 0;
        }
    }
    std::cout << std::endl;

    
    return 0;
}