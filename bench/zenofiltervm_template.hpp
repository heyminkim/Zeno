#pragma once

#include "base.hpp"
#include "zenofiltervm.hpp"

#include <numeric>

namespace zeno_bench {

class ZenoFilterVM : public Filter {
    public:
    ZenoFilterVM(uint64_t exp_size, uint64_t hash_bits, uint64_t expansion_ratio=1, double threshold=0.9) 
    : valid(true) {
        expansion_ratio_ = expansion_ratio;
        filter = new zeno::ZenoFilterVM(exp_size, hash_bits, 0, expansion_ratio, zeno::hashmode::Default, 0,
                               threshold);
    }

    ~ZenoFilterVM() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        return filter->insert(key, 0, 1, zeno::kNoLock) >= 0;
    }

    bool query(const uint64_t key) const {
        uint64_t value;
        return filter->query(key, value, zeno::kNoLock) >= 1;
    }

    // Concurrency methods
    // Yields when filter is growing
    bool concurrent_insert_fallback(const uint64_t key) {
        bool res = filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        if (res) return res;
        else {
            while (filter->is_filter_growing()) {
                std::this_thread::yield();
            }
            return filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        }
    }

    bool concurrent_insert(const uint64_t key) {
        bool res = filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        return res;
    }

    bool concurrent_query(const uint64_t key) const {
        uint64_t value;
        return filter->concurrent_query(key, value, zeno::kWaitForLock) >= 1;
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

    private:
    zeno::ZenoFilterVM* filter;
    uint64_t expansion_ratio_;
    bool valid;
};  // class ZenoFilterVM

}   // namespace zeno_bench