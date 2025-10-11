#pragma once

#include "base.hpp"
#include "zeno.hpp"

namespace zeno_bench {

class Zeno : public Filter {
    public:
    Zeno(uint64_t exp_size, uint64_t hash_bits, uint64_t expansion_ratio=1, double threshold=0.9) 
    : valid(true) {
        expansion_ratio_ = expansion_ratio;
        filter = new zeno_headeronly::Zeno(exp_size, hash_bits, 0, expansion_ratio, zeno_headeronly::hashmode::Default, 0,
                               threshold);
    }

    ~Zeno() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        return filter->insert(key, 0, 1, zeno_headeronly::kNoLock) >= 0;
    }

    bool query(const uint64_t key) const {
        uint64_t value;
        return filter->query(key, value, zeno_headeronly::kNoLock) >= 1;
    }

    bool remove(const uint64_t key) {
        filter->remove(key, 0, 1, zeno_headeronly::kNoLock);
        return true;
    }

    bool resize() {
        return filter->grow(0, 0, zeno_headeronly::kNoLock) >= 0;
    }

    bool is_valid() const {
        return valid;
    }

    void auto_resize(const bool flag) {
        filter->set_auto_resize(flag);
    }

    void contract() {}

    uint64_t size() const {
        return 0;
    }

    std::string name(const bool verbose=false) const {
        if (verbose) return "Zeno" + std::to_string(expansion_ratio_);
        else return "Zeno";
    }

    private:
    zeno_headeronly::Zeno* filter;
    uint64_t expansion_ratio_;
    bool valid;
};  // class Zeno

}   // namespace zeno_bench