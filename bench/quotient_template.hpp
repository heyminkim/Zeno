#pragma once

#include "base.hpp"
#include "quotient.hpp"

namespace zeno_bench {

class Quotient : public Filter {
    public:
    Quotient(uint64_t exp_size, uint64_t hash_bits, double threshold=0.9) : valid(true) {
        fingerprint_bits_ = hash_bits - exp_size;
        filter = new zeno::Quotient(exp_size, hash_bits, 0, 0, zeno::hashmode::Default, 0, threshold);
    }

    ~Quotient() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        return filter->insert(key, 0, 1, zeno::kNoLock) >= 0;
    }

    bool query(const uint64_t key) const {
        uint64_t value;
        return filter->query(key, value, zeno::kNoLock) >= 1;
    }

    bool concurrent_insert(const uint64_t key) {
        bool res = filter->insert(key, 0, 1, zeno::kWaitForLock) >= 0;
        return res;
    }

    bool concurrent_query(const uint64_t key) const {
        uint64_t value;
        return filter->query(key, value, zeno::kWaitForLock) >= 1;
    }

    uint64_t report_time() const {
        return filter->get_expansion_time();
    }

    bool remove(const uint64_t key) {
        filter->remove(key, 0, 1, zeno::kNoLock);
        return true;
    }

    bool resize() {
        int64_t res = filter->grow(0, 0, zeno::kNoLock);
        if (res < 0) {
            valid = false;
            return false;
        } else {
            return true;
        }
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
        if (verbose) return "RSQF" + std::to_string(fingerprint_bits_);
        else return "RSQF";
    }

    private:
    zeno::Quotient* filter;
    uint64_t fingerprint_bits_;
    bool valid;
};  // class Quotient

}   // namespace zeno_bench