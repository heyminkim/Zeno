#pragma once

#include <cstdint>
#include <iostream>
#include <atomic>

namespace zeno {

class PartitionedCounter {
    public:
    PartitionedCounter(uint32_t num_partitions = 0, int32_t threshold = 100) 
    : threshold_(threshold)
    {
        int numCPUs = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
        if (numCPUs < 1) {
            std::cerr << "sysconf failed to get CPU count" << std::endl;
        }
        num_counters_ = num_partitions == 0 ? numCPUs
                                            : std::min(numCPUs, static_cast<int>(num_partitions));
        local_counters_ = new LocalCounter[num_counters_]{};
    }
    ~PartitionedCounter() {
        sync();
        if (local_counters_) delete[] local_counters_;
    }

    void add(int64_t count) {
        uint32_t id = getPartitionId();
        int64_t new_value = local_counters_[id].counter.fetch_add(count, std::memory_order_relaxed) 
                          + count;

        // If local counter goes over or below the threshold, apply changes to the global counter
        if (new_value > threshold_ || new_value < -threshold_) {
            int64_t local = local_counters_[id].counter.exchange(0, std::memory_order_acq_rel);
            global_counter_.fetch_add(local, std::memory_order_relaxed);
        }
    }
    void sync() {
        for (uint32_t i = 0; i < num_counters_; ++i) {
            auto& local = local_counters_[i];
            int64_t val = local.counter.exchange(0, std::memory_order_acq_rel);
            global_counter_.fetch_add(val, std::memory_order_relaxed);
        }
    }
    int64_t get_counter() const {
        return global_counter_.load();
    }

    void reset() {
        sync();
        global_counter_.store(0, std::memory_order_relaxed);
    }

    private:
    struct alignas(64) LocalCounter {
        std::atomic<int64_t> counter{0};
    };

    uint32_t num_counters_;
    int32_t threshold_;
    std::atomic<int64_t> global_counter_{0};
    LocalCounter* local_counters_ = nullptr;

    uint32_t getPartitionId() const {
        int cpuid = sched_getcpu();
        return static_cast<uint32_t>(cpuid % num_counters_);
    }
};  // class PartitionedCounter

}   // namespace zeno