#pragma once

#include <cstdint>
#include <string>

namespace zeno_bench {

class Filter {
    public:
    Filter() = default;
    virtual ~Filter() = default;
    virtual bool insert(const uint64_t) = 0;
    virtual bool query(const uint64_t) const = 0;
    virtual bool remove(const uint64_t) = 0;
    virtual bool resize() = 0;
    virtual bool is_valid() const = 0;
    virtual void auto_resize(const bool) = 0;
    virtual void contract() = 0;
    virtual uint64_t size() const = 0;
    virtual std::string name(const bool=false) const = 0;
};  // class Filter

}   // namespace zeno_bench