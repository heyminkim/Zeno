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
#include "aleph_template.hpp"

#include "../util/cxxopts.hpp"

int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("a,age", "Target age to delete", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_delete", "File name to write positive query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_size", "File name to write negative query results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();
    uint64_t target_age = result["age"].as<uint64_t>();

    // Files to write results
    std::string fn_delete = result["fn_delete"].as<std::string>();
    std::string fn_size = result["fn_size"].as<std::string>();

    std::ofstream file_delete(fn_delete, std::ios::app);
    std::ofstream file_size(fn_size, std::ios::app);

    if (!file_delete || !file_size) {
        std::cerr << "Error opening files." << std::endl;
        return 1;
    }

    // Parameters for benchmark
    bool record_delete = (fn_delete != "/dev/null");
    bool record_size = (fn_size != "/dev/null");

    // Generate data
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 80 * nvals / 100;

    // Generate keys for insertion
    uint64_t* keys = nullptr;
    if (record_delete) {
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
        // util::dump_vals("deletes.data", keys, nvals);
    }
    // keys = util::read_vals("deletes.data", nvals);

    bool auto_resize = false;
    
    // Configure filter by given ID
    zeno_bench::Filter* filter;
    if (id == 1) {
        filter = new zeno_bench::VZF(qbits, qbits + fbits, 1);
        filter->auto_resize(auto_resize);
    }
    else if (id == 2) {
        filter = new zeno_bench::Aleph(qbits, qbits + fbits);
        filter->auto_resize(auto_resize);
    }
    else {
        abort();
    }

    file_delete << filter->name(false) << ",";
    file_size << filter->name(false) << ",";

    // For storing query results
    bool ret = true;

    // For storing the indexes to delete
    std::vector<uint64_t> delete_targets;

    uint64_t num_zero_keys = 0;
    // Insert entries
    for (uint64_t i = 0; i < nvals; ++i) {
        if (keys[i] == 0) {
            ++num_zero_keys;
            continue;
        }
        ret = filter->insert(keys[i]);
        if (!ret) {
            delete_targets.push_back(i - 1);
            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            uint64_t filter_size = filter->size();
            file_size << next_po2 << "," << ((double)filter_size / (1024 * 1024)) << ",";
            filter->resize();
            filter->insert(keys[i]);
            filter->auto_resize(auto_resize);
        }
    }

    // Small working example
    // for (int i = 0; i < 4; ++i) {
    //     filter->resize();
    // }

    // for (int i = 0; i < 4; ++i) {
    //     filter->contract();
    //     uint64_t filter_size = filter->size();
    //     file_size << i << "," << ((double)filter_size / (1024 * 1024)) << ",";
    // }
    // end example

    // uint64_t num_valid_keys = nvals - num_zero_keys;
    // for (uint64_t i = 0; i < (num_valid_keys/2); ++i) {
    //     auto idx = util::generate_random() % nvals;
    //     while (keys[idx] == 0) {
    //         idx = util::generate_random() % nvals; 
    //     }
    //     filter->remove(keys[idx]);
    //     keys[idx] = 0;
    // }

    // filter->contract();

    // num_valid_keys /= 2;
    // if (record_size) {
    //     uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(num_valid_keys));
    //     uint64_t filter_size = filter->size();
    //     file_size << next_po2 << "," << ((double)filter_size / (1024 * 1024)) << ",";
    // }

    if (record_delete) {
        // Perform deletes depending on the age
        uint64_t max_age = delete_targets.size();
        uint64_t delete_time = 0;
        uint64_t delete_start = delete_targets[target_age - 1];
        uint64_t delete_end = target_age >= delete_targets.size() ? nvals : delete_targets[target_age];
        if (delete_end - delete_start > 10'000) {
            delete_end = delete_start + 10'000;
        }
        double interval = (double)(delete_end - delete_start);
        
        for (uint64_t i = delete_start; i <= delete_end; ++i) {
            delete_time += util::timing([&]{
                filter->remove(keys[i]);
            });
        }

        double delete_time_us = ((double)delete_time) / (interval * 1000.0 /*us*/);
        file_delete << max_age - target_age << "," << delete_time_us << ",";
    }

    file_delete << std::endl;
    file_size << std::endl;

    file_delete.close();
    file_size.close();
    delete filter;

    return 0;
}