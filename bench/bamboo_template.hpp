#pragma once

#include <charconv>

#include "base.hpp"
#include "include/bamboofilters/src/bamboofilter/bamboofilter.hpp"

namespace zeno_bench {

class Bamboo : public Filter {
    public:
    Bamboo(uint64_t exp_size, uint64_t threshold = 2) : valid(true) {
        uint32_t capacity = 1U << exp_size;
        filter = new BambooFilter(capacity, threshold);
    }

    ~Bamboo() {
        delete filter;
    }

    bool insert(const uint64_t key) {
        char buffer[8];
        memcpy(buffer, &key, sizeof(key));
        return filter->Insert(buffer);
    }

    bool query(const uint64_t key) const {
        char buffer[8];
        memcpy(buffer, &key, sizeof(key));
        return filter->Lookup(buffer);
    }

    bool remove(const uint64_t key) {
        filter->Delete(reinterpret_cast<char*>(key));
        return true;
    }

    bool resize() {
        filter->Extend();
        return true;
    }

    bool is_valid() const {
        return valid;
    }

    void auto_resize(const bool flag) {

    }

    void contract() {}

    uint64_t size() const {
        // return filter->Size();
        return 0;
    }

    std::string name(const bool verbose=false) const {
        return "Bamboo";
    }

    private:
    BambooFilter* filter;
    uint64_t expansion_threshold;
    bool valid;
};  // class Bamboo

}   // namespace zeno_bench