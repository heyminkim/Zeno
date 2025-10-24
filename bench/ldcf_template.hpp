#pragma once

#include <cmath>
#include <string>

#include "base.hpp"
#include "include/LDCF/src/LDCF.hpp"

namespace zeno_bench {

class LDCF : public Filter {
    public:
    LDCF(uint64_t exp_size, uint64_t fp_bits) : valid(true) {
        uint32_t capacity = 1U << (exp_size);
        double target_fpr = std::pow(2, -static_cast<double>(fp_bits)); 
        filter = new baseline_LDCF::LogarithmicDynamicCuckooFilter(target_fpr, capacity, 1);
    }

    ~LDCF() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        bool res = filter->insert(std::to_string(key));
        if (!res) valid = false;
        return res;
    }

    bool query(const uint64_t key) const {
        return filter->contains(std::to_string(key));
    }

    bool remove(const uint64_t key) {
        filter->remove(std::to_string(key));
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

    void print_contents() const {
        filter->print_contents();
    }

    uint64_t size() const {
        return filter->Size();
    }

    std::string name(const bool verbose=false) const {
        return "LDCF";
    }

    private:
    baseline_LDCF::LogarithmicDynamicCuckooFilter* filter;
    bool valid;
};  // class LDCF

}   // namespace zeno_bench