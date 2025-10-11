#pragma once

#include "base.hpp"
#include "vzf.hpp"

#include <numeric>

namespace zeno_bench {

class VZF : public Filter {
    public:
    VZF(uint64_t exp_size, uint64_t hash_bits, uint64_t expansion_ratio=1, double threshold=0.9) 
    : valid(true) {
        expansion_ratio_ = expansion_ratio;
        filter = new zeno::VZF(exp_size, hash_bits, 0, expansion_ratio, zeno::hashmode::Default, 0,
                               threshold);
    }

    ~VZF() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        return filter->insert(key, 0, 1, zeno::kNoLock) >= 0;
    }

    bool query(const uint64_t key) const {
        uint64_t value;
        return filter->query(key, value, zeno::kNoLock) >= 1;
    }

    bool remove(const uint64_t key) {
        filter->remove(key, 0, 1, zeno::kNoLock);
        return true;
    }

    bool resize() {
        return filter->grow(0, 0, zeno::kNoLock) >= 0;
    }

    bool is_valid() const {
        return valid;
    }

    void contract() {
        filter->contract();
    }

    void auto_resize(const bool flag) {
        filter->set_auto_resize(flag);
    }

    uint64_t size() const {
        return filter->get_memory_usage();
    }

    std::string name(const bool verbose=false) const {
        if (verbose) return "VZF" + std::to_string(expansion_ratio_);
        else return "VZF";
    }

    // double avg_cluster_length() const {
    //     std::vector<uint64_t> cluster_lengths;
    //     filter->calculate_cluster(cluster_lengths);

    //     uint64_t sum = std::accumulate(cluster_lengths.begin(), cluster_lengths.end(), 0);
    //     double avg = static_cast<double>(sum) / cluster_lengths.size();
    //     return avg;
    // }

    private:
    zeno::VZF* filter;
    uint64_t expansion_ratio_;
    bool valid;
};  // class VZF

}   // namespace zeno_bench