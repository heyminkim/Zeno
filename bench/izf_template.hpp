#pragma once

#include "base.hpp"
#include "izf.hpp"

namespace zeno_bench {

class IZF : public Filter {
    public:
    IZF(uint64_t exp_size, uint64_t hash_bits, uint64_t expansion_ratio=1, double threshold=0.8)
    : valid(true) {
        expansion_ratio_ = expansion_ratio;
        filter = new zeno::IZF(exp_size, hash_bits, 0, expansion_ratio, zeno::hashmode::Default, 0,
                               threshold);
    }

    ~IZF() {
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

    void contract() {}

    uint64_t size() const {
        return filter->get_memory_usage();
    }

    double space_amplification() const {
        double nonempty_slots = (double)filter->calculate_nonempty_slots();
        double nslots = (double)filter->get_num_slots();
        return nslots / nonempty_slots;
    }

    std::string name(const bool verbose=false) const {
        if (verbose) return "IZF" + std::to_string(expansion_ratio_);
        else return "IZF";
    }

    private:
    zeno::IZF* filter;
    uint64_t expansion_ratio_;
    bool valid;
};  // class IZF

}   // namespace zeno_bench