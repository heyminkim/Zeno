#pragma once

#include "base.hpp"
#include "vzf_con.hpp"

#include <numeric>

namespace zeno_bench {

/**
 * Implementation of VZF that always locks for all operations. 
 */
class VZFC : public Filter {
    public:
    VZFC(uint64_t exp_size, uint64_t hash_bits, uint64_t expansion_ratio=1, 
         uint64_t nslots_to_lock=12, double threshold=0.9) 
    : valid(true) {
        expansion_ratio_ = expansion_ratio;
        nslots_to_lock = 1ULL << nslots_to_lock;
        filter = new zeno::VZFC(exp_size, hash_bits, 0, expansion_ratio, zeno::hashmode::Default, 0,
                               nslots_to_lock, threshold);
    }

    ~VZFC() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        bool res = filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        if (res) return res;
        else {
            while (filter->is_filter_growing()) {
                std::this_thread::yield();
            }
            return filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        }
    }

    bool query(const uint64_t key) const {
        uint64_t value;
        return filter->concurrent_query(key, value, zeno::kWaitForLock) >= 1;
    }

    bool remove(const uint64_t key) {
        filter->remove(key, 0, 1, zeno::kWaitForLock);
        return true;
    }

    bool resize() {
        return filter->grow(0, 0, zeno::kWaitForLock) >= 0;
    }

    bool is_valid() const {
        return valid;
    }

    void auto_resize(const bool flag) {
        filter->set_auto_resize(flag);
    }

    void contract() {}

    uint64_t size() const {
        return filter->get_memory_usage();
    }

    std::string name(const bool verbose=false) const {
        if (verbose) return "VZFC" + std::to_string(expansion_ratio_);
        else return "VZFC";
    }

    // Concurrency methods. 
    bool is_filter_growing() const {
        return filter->is_filter_growing();
    }

    bool concurrent_insert(const uint64_t key) {
        bool res = filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        return res;
    }

    private:
    zeno::VZFC* filter;
    uint64_t expansion_ratio_;
    bool valid;
};  // class VZFC

}   // namespace zeno_bench