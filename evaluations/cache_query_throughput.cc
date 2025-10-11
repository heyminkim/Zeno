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
      ("flush", "Flush cache", cxxopts::value<uint64_t>()->default_value("0"))
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
    uint64_t flush_cache = result["flush"].as<uint64_t>();

    std::string label = result["label"].as<std::string>();
    std::cout << label << ",";

    util::set_cpu_affinity(11);

    // Configure zeno filter
    Zeno* zeno = new Zeno(qbits, qbits + fbits, vbits, rratio, 
                          hashmode::Default, seed);
    zeno->set_auto_resize(true);

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

    uint64_t* memory;
    constexpr uint64_t memsize = 26e6 / 8;
    if (flush_cache) {
        memory = (uint64_t*)malloc(memsize * sizeof(memory[0]));
        RAND_bytes((unsigned char*)memory, sizeof(*memory) * memsize);
    }

    // Execute evaluations
    uint64_t query_time = 0;
    uint64_t interval = 100'000;
    uint64_t count = 0;
    uint64_t value;

    int idx = 0;

    std::vector<uint64_t> offsets = {1000, 1778, 3162, 5623};

    interval = offsets[idx];
    uint64_t prev_interval = 0;
    ++idx;

    uint64_t random_sum = 0;

    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = 0;
        ret = zeno->insert(keys[i], vals[i], key_count, kNoLock);

        if (i % interval == 0 && i != 0) {
            uint64_t num_queries = i < 1000 ? i : 1000;
            for (int j = 0; j <= num_queries; ++j) {
                query_time += util::timing([&]{
                    count = zeno->query(keys[i], value, kNoLock);
                });
                if (flush_cache) {
                    for (uint64_t z = 0; z < memsize; ++z) {
                        random_sum += memory[z];
                    }
                }
            }
            std::cout << i << "," << ((double)query_time) / ((double)num_queries * 1000) << ",";
            query_time = 0;

            prev_interval = interval;
            interval = offsets[idx];
            ++idx;
            if (idx == 4) {
                for (auto& o : offsets) o *= 10;
                idx = 0;
            }
        }
    }
    std::cout << std::endl;

    std::cerr << "rsum was : " << random_sum << std::endl;
    
    return 0;
}