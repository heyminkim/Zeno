#pragma once

#include <charconv>

#include "base.hpp"
#include "include/EBF/bloom.h"

namespace zeno_bench {

class EBF : public Filter {
    public:
    EBF(uint64_t exp_size) : valid(true) {
        // exp_size - 3 because we have 8 slots per bucket
        filter = new Ebloom_filter(exp_size - 3, HASH_NUM);
    }

    ~EBF() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        return filter->insert(key);
    }

    bool query(const uint64_t key) const {
        return filter->query(key);
    }

    bool remove(const uint64_t key) {
        filter->deleteEle(key);
        return true;
    }

    bool resize() {
        filter->expand();
        return true;
    }

    bool is_valid() const {
        return valid;
    }

    void auto_resize(const bool flag) {
        expandOrNot = flag;
    }

    void contract() {}

    uint64_t size() const {
        return 0;
    }

    std::string name(const bool verbose=false) const {
        return "EBF";
    }

    double avg_cluster_length() const {
        return 0;
    }

    private:
    Ebloom_filter* filter;
    bool valid;
};  // class EBF

}   // namespace zeno_bench