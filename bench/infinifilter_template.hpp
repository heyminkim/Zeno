#pragma once

#include "base.hpp"
#include "infinifilter.hpp"

namespace zeno_bench {

class InfiniFilter : public Filter {
    public:
    InfiniFilter(uint64_t exp_size, uint64_t hash_bits, double threshold=0.9) : valid(true) {
        fingerprint_bits_ = hash_bits - exp_size;
        filter = new zeno::InfiniFilter(exp_size, hash_bits, zeno::hashmode::Default, 0, threshold);
    }

    ~InfiniFilter() {
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

    void auto_resize(const bool flag) {
        filter->set_auto_resize(flag);
    }

    void contract() {
        return;
    }

    uint64_t size() const {
        return filter->get_memory_usage();
    }

    double size_ratio() const {
        double total = static_cast<double>(filter->get_memory_usage());
        double base = static_cast<double>(filter->get_main_memory_usage());
        return total / base;
    }

    std::string name(const bool verbose=false) const {
        if (verbose) return "InfiniFilter" + std::to_string(fingerprint_bits_);
        else return "InfiniFilter";
    }

    // double avg_cluster_length() const {
    //     std::vector<uint64_t> cluster_lengths;
    //     filter->calculate_cluster(cluster_lengths);

    //     uint64_t sum = std::accumulate(cluster_lengths.begin(), cluster_lengths.end(), 0);
    //     double avg = static_cast<double>(sum) / cluster_lengths.size();
    //     return avg;
    // }

    private:
    zeno::InfiniFilter* filter;
    uint64_t fingerprint_bits_;
    bool valid;
};  // class InfiniFilter

}   // namespace zeno_bench