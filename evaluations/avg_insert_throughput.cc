#include <cmath>
#include <cstdlib>
#include <string>
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
    uint64_t* vals;

    keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    RAND_bytes((unsigned char*)keys, sizeof(*keys) * nvals);
    RAND_bytes((unsigned char*)vals, sizeof(*vals) * nvals);
    srand(0);
    for (uint64_t i = 0; i < nvals; ++i) {
        keys[i] = (1 * keys[i]) % zeno->get_hash_range();
    }

    // Execute evaluations
    uint64_t insert_time = 0;

    // For adjusting the evaluation for Zeno
    uint64_t num_expansions = 0;
    uint64_t last_expansion_index = 0;

    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = 0;
        insert_time += util::timing([&]{
            ret = zeno->insert(keys[i], vals[i], key_count, kNoLock);
        });
        if (ret < 0) {
            ret = zeno->grow(0, 0, 0);
            if (ret < 0) {
                std::cerr << "failed increasing filter size" << std::endl;
                abort();
            }
            insert_time += util::timing([&]{
                ret = zeno->insert(keys[i], vals[i], key_count, kNoLock);
            });

            // If CQF, check avg insertion on every expansion. If Zeno, check
            // avg insertion on every period.
            if (rratio == 0 || num_expansions % rratio == 0) {
                // number of insertions done in this period
                uint64_t num_insertions = (i + 1) - last_expansion_index;
                std::cout << ((double)insert_time) / ((double) num_insertions * 1000) << ",";
                last_expansion_index = i + 1;
                insert_time = 0;
            }
            ++num_expansions;
        }
    }
    std::cout << std::endl;
    
    return 0;
}