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
      ("a,loadfactor", "Load factor threshold for expansion", cxxopts::value<uint64_t>())
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
    uint64_t loadfactor = result["loadfactor"].as<uint64_t>();

    std::string label = result["label"].as<std::string>();
    std::cout << label << ",";

    double alpha = ((double)loadfactor) / 100.0;

    // Configure zeno filter
    Zeno* zeno = new Zeno(qbits, qbits + fbits, vbits, rratio, hashmode::Default, seed, alpha);
    zeno->set_auto_resize(false);

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    // nvals = nvals / 100;

    std::cerr << "nvals : " << nvals << std::endl;
    uint64_t* keys;

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

    uint64_t total_time = util::timing([&]{
        for (uint64_t i = 0; i < nvals; ++i) {
            int ret = zeno->insert(keys[i], 0, key_count, kNoLock);
            if (ret < 0) break;
        }
    });

    std::cerr << "Total insert time : " << total_time / 1000 << " us" << std::endl; 

    // Calculate cluster size here
    std::vector<uint64_t> cluster_lengths;
    std::vector<uint64_t> cluster_lengths_runend;
    zeno->calculate_cluster(cluster_lengths);
    zeno->calculate_cluster_runend(cluster_lengths_runend);

    if (cluster_lengths.size() == cluster_lengths_runend.size()) {
        std::cerr << "two sizes are same" << std::endl;
        auto t = cluster_lengths.size();
        for (uint64_t i = 0; i < (uint64_t)t; ++i) {
            if (cluster_lengths[i] == cluster_lengths_runend[i]) {

            } else {
                std::cerr << "different cluster lengths : " << cluster_lengths[i] << " " << cluster_lengths_runend[i] << std::endl;
                abort();
            }
        }
    }

    uint64_t max_cl = 0;

    for (uint64_t i = 0; i < cluster_lengths.size(); ++i) {
        if (cluster_lengths[i] < 5) continue;
        std::cout << cluster_lengths[i] << ",";
        if (cluster_lengths[i] > max_cl) {
            max_cl = cluster_lengths[i];
        }
    }

    std::cerr << "maxcl : " << max_cl << std::endl;
    // std::cout << "nocc : " << total_noccupieds << std::endl;

    

    std::cout << std::endl;
    
    return 0;
}