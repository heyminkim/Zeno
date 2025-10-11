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

    util::set_cpu_affinity(24);

    // Configure zeno filter
    Zeno* zeno = new Zeno(qbits, qbits + fbits, vbits, rratio, 
                          hashmode::Default, seed);
    zeno->set_auto_resize(false);

    // Generate data
    std::vector<uint64_t> keys;

    // Put dummy keys that are mapped to slot 0. 
    keys.push_back(1023658);
    keys.push_back(703310);
    keys.push_back(638408);
    keys.push_back(40212647544);
    keys.push_back(123011);
    keys.push_back(18978);
    keys.push_back(503401);

    for (uint64_t i = 0; i < 7; ++i) {
        zeno->insert(keys[i], 0, key_count, kNoLock);
    }

    uint64_t res_count = 0;
    uint64_t res_value = 0;

    std::cout << "Before grow" << std::endl;
    for (uint64_t i = 0; i < 7; ++i) {
        res_count = zeno->query(keys[i], res_value, kNoLock);
        if (res_count < 1) abort();
    }

    zeno->grow(0, 0, kNoLock);

    std::cout << "After first grow" << std::endl;
    for (uint64_t i = 0; i < 7; ++i) {
        res_count = zeno->query(keys[i], res_value, kNoLock);
        if (res_count < 1) abort();
    }
    
    zeno->grow(0, 0, kNoLock);

    std::cout << "After second grow" << std::endl;
    for (uint64_t i = 0; i < 7; ++i) {
        res_count = zeno->query(keys[i], res_value, kNoLock);
        if (res_count < 1) abort();
    }

    zeno->print_by_index(7787);
    
    return 0;
}