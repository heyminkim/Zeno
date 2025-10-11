#pragma once

#include <charconv>

#include "base.hpp"
#include "include/E2CF/src/EECuckooFilter.h"

namespace zeno_bench {

class E2CF : public Filter {
    public:
    E2CF(uint64_t exp_size, uint64_t fp_bits) : valid(true) {
        uint32_t capacity = 1U << (exp_size);
        double target_fpr = std::pow(2, -static_cast<double>(fp_bits));
        filter = new EECuckooFilter(capacity, target_fpr);
    }

    ~E2CF() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        char buffer[8];
        memcpy(buffer, &key, sizeof(key));
        return filter->insertItemNR(buffer);
    }

    bool query(const uint64_t key) const {
        char buffer[8];
        memcpy(buffer, &key, sizeof(key));
        return filter->queryItem(buffer);
    }

    bool remove(const uint64_t key) {
        char buffer[8];
        memcpy(buffer, &key, sizeof(key));
        filter->deleteItem(buffer);
        return true;
    }

    bool resize() {
        return true;
    }

    bool is_valid() const {
        return valid;
    }

    void auto_resize(const bool flag) {

    }

    void contract() {}

    uint64_t size() const {
        return static_cast<uint64_t>(filter->size_in_mb() * 1024 * 1024);
    }

    std::string name(const bool verbose=false) const {
        return "E2CF";
    }


    private:
    EECuckooFilter* filter;
    bool valid;
};  // class E2CF

}   // namespace zeno_bench