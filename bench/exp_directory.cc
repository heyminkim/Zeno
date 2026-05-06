#include <cmath>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iomanip>
#include <vector>
#include <openssl/rand.h>

#include "util.hpp"
#include "base.hpp"

#include "zenofilter_template.hpp"
#include "fixed_directory_template.hpp"

#include "../util/cxxopts.hpp"

class FilterWrapper {
    public:
    virtual bool insert(uint64_t) = 0;
    virtual bool query(uint64_t) = 0;
    virtual void auto_resize(bool) = 0;
    virtual void resize() = 0;
    virtual std::string name(bool) const = 0;
    virtual uint64_t directory_size() const = 0;
};

class ZenoFilterAdapter : public FilterWrapper {
    public:
    zeno_bench::ZenoFilter* f;
    explicit ZenoFilterAdapter(zeno_bench::ZenoFilter* f) : f(f) {}

    bool insert(uint64_t k) override {
        return f->insert(k);
    }
    
    bool query(uint64_t k) override {
        return f->query(k);
    }

    void auto_resize(bool b) override {
        f->auto_resize(b);
    }

    void resize() override {
        f->resize();
    }

    std::string name(bool b) const override {
        return f->name(b);
    }

    uint64_t directory_size() const override {
        return f->directory_size();
    }
};

class FDAdapter : public FilterWrapper {
    public:
    zeno_bench::FD* f;
    explicit FDAdapter(zeno_bench::FD* f) : f(f) {}

    bool insert(uint64_t k) override {
        return f->insert(k);
    }
    
    bool query(uint64_t k) override {
        return f->query(k);
    }

    void auto_resize(bool b) override {
        f->auto_resize(b);
    }

    void resize() override {
        f->resize();
    }

    std::string name(bool b) const override {
        return f->name(b);
    }

    uint64_t directory_size() const override {
        return f->directory_size();
    }
};


int main(int argc, char** argv) {
    cxxopts::Options options("ZENO", "Execute Zeno filter with parameters");
    options.add_options()
      ("q,quotient", "Length of quotient in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("f,fingerprint", "Length of fingerprint in bits", cxxopts::value<uint64_t>()->default_value("12"))
      ("r,rratio", "Expansion ratio", cxxopts::value<uint64_t>()->default_value("1"))
      ("e,expansion", "Number of expected expansions", cxxopts::value<uint64_t>()->default_value("1"))
      ("i,id", "Filter id", cxxopts::value<uint64_t>()->default_value("1"))
      ("fn_size", "File name to write load factor results to", cxxopts::value<std::string>()->default_value("/dev/null"))
      ("fn_query", "File name to write load factor results to", cxxopts::value<std::string>()->default_value("/dev/null"))
    ;

    auto result = options.parse(argc, argv);

    // Configure options
    uint64_t qbits = result["quotient"].as<uint64_t>();
    uint64_t fbits = result["fingerprint"].as<uint64_t>();
    uint64_t rratio = result["rratio"].as<uint64_t>();
    uint64_t expansions = result["expansion"].as<uint64_t>();
    uint64_t id = result["id"].as<uint64_t>();

    // Files to write results
    std::string fn_size = result["fn_size"].as<std::string>();
    std::string fn_query = result["fn_query"].as<std::string>();

    std::ofstream file_size(fn_size, std::ios::app);
    std::ofstream file_query(fn_query, std::ios::app);

    if (!file_size || !file_query) {
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
    FilterWrapper* filter = nullptr;

    if (id == 1) {
        auto* f = new zeno_bench::ZenoFilter(qbits, qbits + fbits, rratio);
        f->auto_resize(auto_resize);
        filter = new ZenoFilterAdapter(f);
    } else if (id == 2) {
        auto* f = new zeno_bench::FD(qbits, qbits + fbits, rratio);
        f->auto_resize(auto_resize);
        filter = new FDAdapter(f);
    }
    file_size << filter->name(false) << ",";
    file_query << filter->name(false) << ",";

    // For storing query results
    bool ret = true;

    // Sampling Ratio
    double sampling_ratio = std::pow(2.0, 1.0/8.0);

    uint64_t query_time = 0;
    uint64_t num_queries = 100'000;

    // Execute evaluation
    for (uint64_t i = 0; i < nvals; ++i) {
        ret = filter->insert(util::generate_random_fast());

        if (!ret) {
            uint64_t next_po2 = 1ULL << (64 - __builtin_clzll(i));
            
            file_size << next_po2 << "," 
                      << ((double)filter->directory_size()) / 1024.0 / 1024.0 /*MB*/ << ",";
            filter->resize();

            volatile bool res;
            for (uint64_t j = 0; j < num_queries; ++j) {
                query_time += util::timing([&] {
                    res = filter->query(util::generate_random_fast());
                });
            }

            file_query << next_po2 << ","
                       << ((double)query_time) / ((double)num_queries * 1000.0 /*us*/) << ",";
            query_time = 0;

        }
    }

    file_size << std::endl;
    file_query << std::endl;
    file_size.close();
    file_query.close();

    return 0;
}