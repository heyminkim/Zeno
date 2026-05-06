/*
 * This file is a rewritten header-only C++ version of the original C-based
 * project by Rob Johnson and Prahsant Pandey, licensed under the BSD 3-Clause 
 * License.
 *
 * Original Copyright (c) 2017, Rob Johnson and Prahsant Pandey
 * Copyright (c) 2025, Hyuhng Min Kim
 * All rights reserved.
 *
 * This software is distributed under the BSD 3-Clause License.
 * See LICENSE file for details.
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdlib.h>
#include <string>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmath>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utility>
#include <vector>
#include <cassert>
#include <cstring>
#include <bit>
#include <deque>
#include <stack>
#include <atomic>
#include <bitset>
#include <mutex>
#include <shared_mutex>
#include <thread>

#if defined(__x86_64__)
#include <immintrin.h>
#endif

#include "decls.hpp"
#include "hash.hpp"
#include "pc.hpp"
#include "rs.hpp"
#include "iterator.hpp"

namespace zeno {

class Quotient {
    public:
    explicit Quotient(uint64_t exp_size, uint64_t hash_bits, uint64_t value_bits, 
                 uint64_t reciprocal_ratio, hashmode hash_mode, uint32_t seed, double threshold);
    ~Quotient();
    Quotient(const Quotient& zeno) = delete;
    Quotient& operator=(const Quotient& zeno) = delete;
    Quotient& operator=(Quotient&& other) noexcept;

    /////////////////////////////
    // Modification functions. //
    /////////////////////////////

    /** 
     * Increments the counter for this key/value pair by count. 
     * @param key The input key. 
     * @param value The input value. 
     * @param count The number of key/value pairs inserted. 
     * @param flags Flags determining the filter's behavior under concurrency, as well as if the 
     * prefix is already hashed or not.
     * @returns
     *     >= 0: distance from the home slot to the slot in which the key is
     *           inserted (or 0 if count == 0).
     *     == kErrNoSpace: Zeno filter has reached capacity.
     *     == kErrCouldntLock: TRY_ONCE_LOCK has failed to acquire the lock.
     */
    int insert(uint64_t key, uint64_t value, uint64_t count, uint8_t flags);

    /**
     * Removes up to `count` instances of this key/value combination. If Zeno filter contains 
     * <= count instances, then they will all be removed, which is not an error. This method makes 
     * an attempt to remove the longest matching key/value pair. This includes the longest matching 
     * void sequence.
     * @param key The target key for removal. 
     * @param value The target value for removal. 
     * @param count The number of instances to be removed. 
     * @param flags Flags determining the filter's behavior under concurrency, as well as if the 
     * prefix is already hashed or not.
     * @returns
     *      >=  0: number of slots freed.
     *      == kErrDoesntExist: Specified item did not exist.
     *      == kErrCouldntLock: TRY_ONCE_LOCK has failed to acquire the lock.
     */
    int32_t remove(uint64_t key, uint64_t value, uint64_t count, uint8_t flags);

    /**
     * Increases the capacity of the underlying memory. The parameters are for a potential 
     * 'dangling' hash; the hash that caused expansion will not have been inserted to the larger 
     * filter. This function handles that case. 
     * TODO: Currently public for debugging purposes, but ideally should be 
     * private. 
     * @param new_hash The 'dangling' hash to be inserted in the larger filter
     * @param new_count The count of `new_hash`
     * @param flags The original flag used for insertion. 
     * @returns The number of reallocated entries. <= 0 if error. 
     */
    int64_t grow(uint64_t dangling_hash, uint64_t dangling_count, uint8_t flags);

    /**
     * Enable/disable automatic resizing. 
     * @param enabled Turns automatic resizing on if true and off if false. 
     */
    void set_auto_resize(bool enabled) {
        metadata_->auto_resize = enabled;
    }
    
    //////////////////////
    // Query functions. //
    //////////////////////

    /**
     * Lookup the value associated with key. Returns the count of that key/value pair in Zeno.
     * @param key The query key. 
     * @param value Holder for the associated value. 
     * @param flags Flags determining the filter's behavior under concurrency, as well as if the 
     * prefix is already hashed or not.
     */
    uint64_t query(uint64_t key, uint64_t& value, uint8_t flags);

    /**
     * Lookup the value associated with key. Returns the count of that key/value pair in Zeno.
     * @param key The query key. 
     * @param value Holder for the associated value. 
     * @param flags Flags determining the filter's behavior under concurrency, as well as if the 
     * prefix is already hashed or not.
     */
    uint64_t concurrent_query(uint64_t key, uint64_t& value, uint8_t flags);

    // Hashing info
    hashmode get_hashmode() const {
        return metadata_->hash_mode;
    }
    uint64_t get_hash_seed() const {
        return metadata_->seed;
    }
    __uint128_t get_hash_range() const {
        return metadata_->range;
    }
    uint64_t get_hash_bits() const {
        return metadata_->hash_bits;
    }
    uint64_t get_memory_usage() const {
        return metadata_->total_memory_usage;
    }
    double get_space_amplification() const {
        return 0;
    }
    void get_max_locked_region() const {
        std::cerr << "max locked region " << runtimedata_->max_locked_region << std::endl;
    }

    // Checks whether the filter is expanding. 
    bool is_filter_growing() const {
        // return runtimedata_->resizing_region.load(std::memory_order_acquire) >= 0;
        return resizing_region.load(std::memory_order_acquire) >= 0;
    }

    /**
     * Returns the accumulated time the filter spent on expansion in microseconds. 
     */
    uint64_t get_grow_time() const {
        return runtimedata_->total_grow_time;
    }

    inline
    uint64_t get_index_block_size() const {
        return 0;
    }

    inline
    void check_empty() const {
        for (uint64_t i = 0; i < metadata_->xnslots; ++i) {
            qfblock* qb = get_block(i / kSlotsPerBlock);
            // assert(qb->offset == 0);
            // assert(qb->occupieds[0] == 0);
            // assert(qb->runends[0] == 0);
            uint64_t* p = reinterpret_cast<uint64_t*>(&get_block(i / kSlotsPerBlock)->
                          slots[(i % kSlotsPerBlock) * metadata_->bits_per_slot / 8]);
            uint64_t t;
            memcpy(&t, p, sizeof(t));
            uint64_t occ;
            memcpy(&occ, qb->occupieds, sizeof(uint64_t));
            if (occ & (1ULL << (i % kSlotsPerBlock))) {
                print_by_index(i);
            }
        }
        std::cout << "emptiness checked" << std::endl;
    }

    void print_by_index(uint64_t index) const {
        print_from_index(index);
    }

    uint64_t get_expansion_time() const {
        return expansion_time;
    }

    void breakpoint() const {
        return;
    }

    /**
     * Reads all cluster lengths of the filter and inserts them to the result vector. 
     * @param result The vector that stores the resulting cluster sizes. 
     * @returns Nothing. 
     */
    void calculate_cluster(std::vector<uint64_t>& result) const {
        uint64_t running_cluster_length = 0;
        uint64_t current_runend_index = 0;
        uint64_t current_index = 0;
        while (true) {
            if (current_index > metadata_->nslots) break;
            if (!is_occupied(current_index)) {
                ++current_index;
                continue;
            }
            current_runend_index = run_end(current_index);
            running_cluster_length += current_runend_index - current_index + 1;
            for (uint64_t i = current_index + 1; i <= current_runend_index; ++i) {
                if (!is_occupied(i)) continue;
                else {
                    uint64_t new_runend_index = run_end(i);
                    running_cluster_length += new_runend_index - current_runend_index;
                    current_runend_index = new_runend_index;
                }
            }
            current_index = current_runend_index + 1;
            result.push_back(running_cluster_length);
            running_cluster_length = 0;
        }
    }

    void calculate_runend_diff() {
        uint64_t current_index = 0;
        while (true) {
            if (current_index > metadata_->nslots) break;
            if (!is_occupied(current_index)) {
                ++current_index;
                continue;
            }
            uint64_t runend_true = run_end(current_index);
            uint64_t runend_false;
            int64_t current_region = current_index / kNumSlotsToLock;
            int64_t last_region;
            uint64_t clusterend_index;
            int res = cluster_end_threadsafe(current_index, runend_false, last_region, clusterend_index, kWaitForLock);
            
            if (runend_true != runend_false) {
                std::cout << "index : " << current_index << std::endl;
                std::cout << "runendt " << runend_true << std::endl;
                std::cout << "runendf " << runend_false << std::endl;
                print_by_index(current_index);
                print_lock_status();
                abort();
            }

            for (uint64_t i = current_region; i <= last_region; ++i) zeno_unlock_region(i);

            // print_lock_status();
            for (uint64_t i = 0; i < runtimedata_->num_locks; ++i) {
                // zeno_unlock_region(i);
                if (is_region_locked(i)) {
                    std::cout << "locked region : " << i << std::endl;
                    abort();
                }
            }
            ++current_index;
        }
    }

    /**
     * Reads all cluster lengths of the filter and inserts them to the result vector. Uses the 
     * runend trick to make calculation faster. 
     * @param result The vector that stores the resulting cluster sizes. 
     * @returns Nothing. 
     */
    void calculate_cluster_runend(std::vector<uint64_t>& result) const {
        uint64_t running_cluster_length = 0;
        uint64_t current_runend_index = 0;
        uint64_t current_index = 0; 
        
        while (true) {
            if (current_index > metadata_->nslots) break;
            if (!is_occupied(current_index)) {
                ++current_index;
                continue;
            }
            current_runend_index = run_end(current_index);
            running_cluster_length += current_runend_index - current_index + 1;
            
            uint64_t new_runend_index = run_end(current_runend_index);
            while (new_runend_index > current_runend_index) {
                running_cluster_length += new_runend_index - current_runend_index;
                current_runend_index = new_runend_index;
                new_runend_index = run_end(current_runend_index);
            }
            current_index = current_runend_index + 1;
            result.push_back(running_cluster_length);
            running_cluster_length = 0;
        }
    }

    private:
    friend class iterator<Quotient>;

    static constexpr uint64_t kDistanceFromHomeSlotCutoff = 1000;
    static constexpr uint64_t kMagicNumber = 1018874902021329732;
    
    // Defines the number of slots that consists a single block. 
    // Must be >= 6. 6 seems fastest. 
    static constexpr uint32_t kBlockOffsetBits = 6;
    // The number of slots per single block. 
    static constexpr uint32_t kSlotsPerBlock = 1ULL << kBlockOffsetBits;
    // The number of metadata words required per single block. 
    static constexpr uint32_t kMetadataWordsPerBlock = (kSlotsPerBlock + 63) / 64;

    // Log of QF blocks that will fit into a single unit of resizable array. Can be any integer >= 1
    // but currently 6 to fit 4096 slots QF into a single unit of RA for easier concurrency control. 
    static constexpr uint32_t kUnitOffsetBits = 6;

    static constexpr uint64_t kNumSlotsToLock = 1ULL << 16;
    static constexpr uint64_t kClusterSize = 1ULL << 14;

    /**
     * The filter block structure. The size of `slots` will be determined at runtime according to 
     * the number of bits assigned per slot. 
     */
    struct __attribute__ ((__packed__)) qfblock {
    // struct qfblock {
        uint8_t offset;
        uint64_t occupieds[kMetadataWordsPerBlock];
        uint64_t runends[kMetadataWordsPerBlock];
        uint8_t slots[1];

        void print() const {
            std::cout << +offset << std::endl;
            std::cout << std::bitset<64>(occupieds[0]) << std::endl;
            std::cout << std::bitset<64>(runends[0]) << std::endl;
        }
    };  // struct qfblock

    // https://rigtorp.se/spinlock/
    struct spinlock {
        std::atomic<bool> lock_ = {0};

        void lock() noexcept {
            for (;;) {
                if (!lock_.exchange(true, std::memory_order_acquire)) {
                    return;
                }
                while (lock_.load(std::memory_order_relaxed)) {
#if defined(__x86_64__)
                    __builtin_ia32_pause();
#endif
                }
            }
        }

        /**
         * Attempts to lock the region, but if the expanding thread attemps to lock this region, 
         * fall back. This is to prevent the thread to falsely insert into a region that has been
         * already scanned through by the expanding thread. The thread should instead, by having
         * received a false value, insert its entry or continue its read on an index double the 
         * original one. 
         * @param region The region this thread is attempting to lock.
         * @param expanding_region The region the expanding thread has announced to lock. 
         */
        bool lock_conditionally(const int64_t region, 
                                const std::atomic<int64_t>& expanding_region) noexcept {
            for (;;) {
                if (!lock_.exchange(true, std::memory_order_acquire)) {
                    return true;
                }
                while (lock_.load(std::memory_order_relaxed)) {
                    if (expanding_region.load(std::memory_order_acquire) <= region) {
                        return false;
                    }
#if defined(__x86_64__)
                    __builtin_ia32_pause();
#endif
                }
            }
        }

        bool try_lock() noexcept {
            return !lock_.load(std::memory_order_relaxed) &&
                   !lock_.exchange(true, std::memory_order_acquire);
        }

        void unlock() noexcept {
            lock_.store(false, std::memory_order_release);
        }
    };  // struct spinlock

    static constexpr size_t kCacheLineSize = 64;

    struct alignas(kCacheLineSize) spinlock_padded {
        spinlock lock_;
        char padding[kCacheLineSize - sizeof(spinlock)];
    };  // struct spinlock_padded

    /**
     * The below struct is used to instrument the code.
     * It is not used in normal operations of the filter.
     */
    struct WaitTimeData {
        uint64_t total_time_single;
        uint64_t total_time_spinning;
        uint64_t locks_taken;
        uint64_t locks_acquired_single_attempt;
    };  // struct WaitTimeData

    struct qfruntime {
        uint64_t num_locks;
        spinlock_padded* locks;
        WaitTimeData* wait_times;
        // Locks required for reallocating spinlocks
        std::atomic<bool> resize_pending{false};
        std::shared_mutex spinlock_mutex;
        // Locks for announcing expansion and location of the iterator
        std::atomic<int64_t> resizing_region_{-1};
        // The upper region that the expansion thread is reading from
        std::atomic<int64_t> resizing_region_upper{-1};
        uint64_t max_locked_region = 0;
        uint64_t total_grow_time = 0;
    };  // struct qfruntime

    struct qfmetadata {
        uint64_t magic_endian_number;
        uint32_t auto_resize;
        enum hashmode hash_mode;
        uint32_t reserved;
        uint64_t total_size_in_bytes;
        uint32_t seed;
        uint64_t nslots;
        uint64_t xnslots;
        uint64_t hash_bits;
        uint64_t value_bits;
        uint64_t fingerprint_bits;
        uint64_t quotient_bits;
        uint64_t bits_per_slot;    // = fingerprint_bits + value_bits
        __uint128_t range;
        uint64_t nblocks;
        uint64_t nelts;
        uint64_t ndistinct_elts;
        uint64_t noccupied_slots;
        double expansion_threshold;
        /* For debugging. */
        uint64_t num_expansions;
        PartitionedCounter pc_noccupied_slots;
        /* Size of a qfblock in bytes. */
        uint64_t qfblock_size;
        // total amount of memory used in bytes
        uint64_t total_memory_usage;
    };  // struct qfmetadata

    /* Actual contents of the filter. */
    qfruntime*  runtimedata_;
    qfmetadata* metadata_;
    qfblock*    blocks_;

    struct alignas(64) PaddedCounter {
        std::atomic<int> count;
        PaddedCounter() : count(0) {}
    };

    inline static std::vector<PaddedCounter> active_threads{128};
    inline static std::atomic<int64_t> resizing_region{-1};
    inline static uint64_t expansion_time = 0;

    /**
     * The below struct is used to instrument the code.
     * It is not used in normal operations of Zeno filter.
     */
    struct ClusterData {
        uint64_t start_index;
        uint16_t length;
    };  // struct ClusterData

    ////////////////////////////////////
    // Modification helper functions. //
    ////////////////////////////////////

    /**
     * Increments the counter for this key/value pair by 1. 
     * @param hash Hashed representation of the key/value pair. 
     * @param flags The locking scheme. 
     * @returns The distance between the canonical slot and the inserted slot address. 
     */
    int insert1(uint64_t hash, uint8_t flags);

    /**
     * Increments the counter for this key/value pair by `count`. 
     * @param hash Hashed representation of the key/value pair. 
     * @param count The number by which we want to increase the count. 
     * @param flags The locking scheme. 
     * @returns The distance between the canonical slot and the inserted slot address. 
     */
    int insertN(uint64_t hash, uint64_t count, uint8_t flags);

    /**
     * Inserts a void sequence starting at index `begin` and ending at `end`. 
     * For a void sequence of gen N, |end - begin| = 2^{N - 1} - 1. 
     * Encodings:
     * gen 1: [BA]
     * gen 2: [A][A]
     * gen N (N > 2): [A][B]...[B][A], |B| = 2^{N - 1} - 2
     * where [ ] indicate entries in the same run. 
     * TODO: support payloads
     * @param begin The beginning index of the void sequence, inclusive. 
     * @param end The ending index of the void sequence, inclusive. 
     * @param count The number of void sequences to be inserted. 
     * @param flags The locking scheme. 
     */
    int insert_void_sequence(uint64_t begin, uint64_t end, uint64_t count, uint8_t flags);
    
    /**
     * Inserts a void entry A at canonical slot `hash_index` and actual runstart index 
     * `runstart_index`. Assumes the appropriate locks are held. 
     * @param hash_index The canonical slot index of the void entry.
     * @param runstart_index The actual runstart index of canonical slot `idx`. 
     * @param count The number of void entries to be inserted. 
     */
    int insert_A_internal(uint64_t& hash_index, uint64_t& runstart_index, uint64_t count);

    /**
     * Inserts a void entry B at canonical slot `hash_index` and actual runstart index 
     * `runstart_index`. Assumes the appropriate locks are held. 
     * @param hash_index The canonical slot index of the void entry.
     * @param runstart_index The actual runstart index of canonical slot `idx`. 
     * @param count The number of void entries to be inserted. 
     */
    int insert_B_internal(uint64_t& hash_index, uint64_t& runstart_index, uint64_t count);
    
    /**
     * Removes all instances of this key/value pair. Only deletes the key/value pair that are 
     * precisely identical; i.e., may only be used when deleting with a queried key/value pair. An 
     * example of this case is during expansion. 
     * @param key The target key for removal. 
     * @param value The target value for removal. 
     * @param flags Flags determining the filter's behavior under concurrency,
     * as well as if the prefix is already hashed or not.
     */
    int delete_key_value(uint64_t key, uint64_t value, uint8_t flags);

    /**
     * Removes the key and value associated with the given hash. 
     * @param hash Hashed representation of the key/value pair.
     * @param count The amount by which the key/value pair was inserted. 
     * @param flags The locking scheme. 
     * @returns The number of freed slots due to deletion.
     */
    int remove_internal(uint64_t hash, uint64_t count, uint8_t flags);

    /**
     * Removes the key and value associated with the given hash. This method removes the longest 
     * matching fingerprint. 
     * @param hash Hashed representation of the key/value pair.
     * @param count The amount by which the key/value pair was inserted. 
     * @param flags The locking scheme. 
     * @returns The number of freed slots due to deletion.
     */
    int remove_longest_internal(uint64_t hash, uint64_t &count, uint8_t flags);

    /**
     * Tries to acquire a lock once and return even if the lock is busy. If spin flag is set, then 
     * spin until the lock is available.
     * @param lock - The lock to acquire.
     * @param flags - Flags determining the filter's behavior under concurrency, as well as if `key` 
     * is already hashed or not. If `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, 
     * it tries to acquire the lock once.
     * @returns `true` if `lock` was successfully acquired and `false` otherwise.
     */
    inline bool spin_lock(spinlock_padded* lock, uint8_t flag) {
        if (GET_WAIT_FOR_LOCK(flag) != kWaitForLock) {
            return lock->lock_.try_lock();
        } else {
            lock->lock_.lock();
            return true;
        }
        return false;
    }

    /**
     * Tries to acquire a lock once and return even if the lock is busy. If spin flag is set, then 
     * spin until the lock is available. Fails if there is a contention with the expanding thread. 
     * @param lock - The lock to acquire. 
     * @param region - The region index this thread is trying to lock. 
     * @param expanding_region - The region the expanding thread is trying to lock. 
     * @param flags - Flags determining the filter's behavior under concurrency, as well as if `key` 
     * is already hashed or not. If `flag_wait_for_lock` is set to 1, the thread spins. Otherwise, 
     * it tries to acquire the lock once.
     * @returns `true` if `lock` was successfully acquired and `false` otherwise.
     */
    inline bool spin_lock_conditionally(spinlock_padded* lock, const int64_t region, 
                                        const std::atomic<int64_t>& expanding_region, uint8_t flag)
    {
        if (GET_WAIT_FOR_LOCK(flag) != kWaitForLock) {
            return lock->lock_.try_lock();
        } else {
            return lock->lock_.lock_conditionally(region, expanding_region);
        }
        return false;
    }

    /**
     * Tries to acquire a lock once and return even if the lock is busy. Used for normal operations 
     * of the inserting thread. If spin flag is set, waits until the spinloc
     * 
     */

    /**
     * Unlock the acquired lock.
     *
     * @param lock - The lock to unlock.
     */
    inline void spin_unlock(spinlock_padded* lock) {
        lock->lock_.unlock();
        return;
    }

    /**
     * Locks the portion of the filter indicated by `hash_bucket_index`.
     *
     * @param hash_bucket_index - The bucket indicating the target portion of
     * the filter.
     * @param small - idk
     * @param runtime_lock - The lock type (kWaitForLock, kTryOnceLock, etc)
     * @returns `true` if the portion was was successfully locked. `false` if it has failed 
     * acquiring a lock.
     */
    bool qf_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock);

    /**
     * Unlocks the portion of the filter indicated by `hash_bucket_index`.
     * @param hash_bucket_index - The bucket indicating the target portion of the filter.
     */
    void qf_unlock(uint64_t hash_bucket_index, bool small);

    /**
     * Locks the portion of the filter indicated by `hash_bucket_index`.
     *
     * @param hash_bucket_index - The bucket indicating the target portion of
     * the filter.
     * @param small - idk
     * @param runtime_lock - The lock type (kWaitForLock, kTryOnceLock, etc)
     * @returns `true` if the portion was was successfully locked. `false` has three cases: 
     * (1) If the filter is not expanding, it has failed acquiring a lock. 
     * (2) If the filter is expanding and it tried to lock the unexpanded region, it ran into a 
     * contention with the expanding thread and has bailed out. In this case, it should retry 
     * acquiring a lock in the expanded region.
     * (3) If the filter is expanding and it tried to lock the expanded region, it has failed 
     * acquiring a lock. 
     */
    bool zeno_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock);

    /**
     * Unlocks the portion of the filter indicated by `hash_bucket_index`.
     *
     * @param hash_bucket_index - The bucket indicating the target portion of the filter.
     * @param small - idk
     */
    void zeno_unlock(uint64_t hash_bucket_index, bool small);

    /**
     * Locks only the region of the filter indicated by `lock_region_index`. Does not provide 
     * fallbacks for collision with the expansion thread. 
     * @param lock_region_index - The index indicating the target region of the filter.
     * @param runtime_lock - The lock type (kWaitForLock, kTryOnceLock, etc)
     * @warning Must only be used by the expansion thread. 
     */
    inline bool zeno_lock_region(int64_t lock_region_index, uint8_t runtime_lock);

    /**
     * Unlocks only the region of the filter indicated by `lock_region_index`. 
     * @param lock_region_index - The index indicating the target region of the filter.
     */
    inline void zeno_unlock_region(int64_t lock_region_index);

    /**
     * Unlocks the regions given by the range indicated by `start_region` and `end_region`. 
     * To prevent deadlocks, the regions are unlocked backwards. 
     */
    inline void zeno_unlock_range(int64_t start_region, int64_t end_region);

    /**
     * Locks the region of the filter indicated by `lock_region_index`. Provides fallbacks for
     * collision with the expansion thread. Fails under two conditions: 
     * (1) kTryOnceLock was set and couldn't acquire lock
     * (2) This thread tried to acquire a lock that was locked by the expansion thread. 
     * If this method fails, the caller must handle the failure according to the failure condition.
     * @param lock_region_index - The index indicating the target region of the filter.
     * @param runtime_lock - The lock type (kWaitForLock, kTryOnceLock, etc)
     */
    inline bool zeno_lock_region_conditionally(int64_t lock_region_index, uint8_t runtime_lock);

    /**
     * Incrementally locks the cluster that begins from the index `index`. This is a pessimistic
     * lock, meaning that it assumes an insertion always adds 1 entry and pushes runs to the right 
     * by 1 slot, regardless of counting (with counting, runs may not pushed to the right). 
     * @param index - The index that we want to start locking. 
     * @param runend - Stored with the runend index for the given `index`. 
     * @param clusterend_region - Stored with the region index of the cluster end. Must be given the
     * start region index. 
     * @param clusterend - Stored with the index of the cluster end. 
     * @param runs - The lock type (kWaitForLock, kTryOnceLock, etc)
     */
    inline int zeno_lock_cluster(uint64_t index, uint64_t& runend, int64_t& clusterend_region, 
                                 uint8_t flags);

    /**
     * Checks whether a new region should be locked when processing the given index.
     * @param index The index currently being processed
     * @returns `true` if the next region must be locked as well, `false` otherwise
     */
    inline bool should_lock_next_region(uint64_t index) const {
        uint64_t hash_bucket_lock_offset  = index % kNumSlotsToLock;
        return kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize;
    }

    /**
     * Checks whether a region is locked.
     * @param region_index The region index to be queried.
     * @returns `true` if the region is locked, `false` otherwise
     */
    inline bool is_region_locked(int64_t region_index) const {
        if (runtimedata_->locks[region_index].lock_.lock_.load(std::memory_order_acquire)) 
            return true;
        else
            return false;
    }

    /**
     * Adds `cnt` to a metadata value. After changing metadatas to atomic, this method is no longer
     * thread-safe. 
     * @param metadata The metadata value to update. 
     * @param cnt The amount to update the metadata value by. 
     */
    inline void modify_metadata(uint64_t* metadata, int32_t cnt) {
        *metadata = *metadata + cnt;
        return;
    }

    /**
     * Adds `cnt` to a metadata value. Uses partitioned counter.
     * @param metadata The metadata value to update. 
     * @param cnt The amount to update the metadata value by. 
     */
    inline void modify_metadata(PartitionedCounter& metadata, int32_t cnt) {
        metadata.add(cnt);
        return;
    }

    /**
     * Returns the pointer to the block specified by `block_index`. 
     * @param block_index The block index with the lower `kBlockOffsetBits` removed. 
     * @returns The pointer to the target qf block. 
     */
    qfblock* get_block(const uint64_t block_index) const {
        return (qfblock *)(((char *)blocks_) + block_index * (sizeof(qfblock) + kSlotsPerBlock *
                                                              metadata_->bits_per_slot / 8));
    }

    void set_slot(const uint64_t& index, const uint64_t& value);
    uint64_t get_slot(const uint64_t& index) const;

    uint64_t run_end(const uint64_t& hash_bucket_index) const;
    uint64_t block_offset(const uint64_t& blockidx) const;
    int32_t offset_lower_bound(const uint64_t& slot_index) const;
    uint64_t find_first_empty_slot(uint64_t from) const;
    void shift_remainders(const uint64_t& start_index, const uint64_t& empty_index);

    /**
     * Calculates the run end of a given entry. This is a thread-safe implementation; it locks the
     * necessary regions incrementally, until the last index of the run. 
     * @param hash_bucket_index The canonical slot address of the key.
     * @param runend_index The resulting runend index.
     * @param clusterend_region The spinlock region that holds the cluster end. 
     * @returns Spinlock status. 
     */
    int run_end_threadsafe(const uint64_t& hash_bucket_index, uint64_t& runend_index, 
                               int64_t& clusterend_region, uint8_t flags);
    
    /**
     * Calculates the cluster end of a given entry. This is not a thread-safe implementation.
     * @param hash_bucket_index The index to start calculating the cluster end. 
     * @param padding The amount of slots added to the calculated cluster end. Needed to acquire the
     * correct spinlock region. 
     * @returns The spinlock region that holds the cluster end. 
     */
    int64_t cluster_end(const uint64_t hash_bucket_index, const uint64_t padding);

    /**
     * Calculates the cluster end of a given entry. This is a thread-safe implementation; it locks
     * the necessary regions incrementally, until the last index of the cluster. 
     */
    int cluster_end_threadsafe(const uint64_t& hash_bucket_index, uint64_t& runend_index, 
                               int64_t& clusterend_region, uint64_t& clusterend_index, 
                               uint8_t flags);
    
    
    //////////////////////////////////
    // Hash manipulation functions. //
    //////////////////////////////////

    /**
     * Inserts an unary delimiter at the end of the hash. For example, if the given hash is 64 bits 
     * and quotient_bits = 4 and bits_per_slot = 4, then
     *  qqqq ffff xxxx....xxxx
     * | qb | fp |  64-8 bits |
     * The resulting hash will be the original hash shifted to the lower bits with an unary 
     * delimiter at the end:
     *  0000....0000 qqqq ffff 1
     * |  64-7 bits | qb | fp |d|
     * The shifting to the lower bits is done because the internal functions `insert1` and `insertN`
     * assumes the hash to be in the lower bits. 
     * @deprecated Use `insert_unary` instead. 
     * @param hash The hash of a key. 
     */
    inline
    void insert_unary_deprecated(uint64_t& hash) const {
        // The last bit is converted to a unary counter by doing an `OR` with 1. 
        hash = hash >> (64ULL - metadata_->hash_bits) | 1ULL;
    }

    /**
     * Inserts an unary delimeter in between the index bits and the remainder
     * bits. For example, if the given hash is 64 bits and for the current 
     * filter configuration quotient_bits = 4 and bits_per_slot = 4, then
     *  qqqq ffff xxxx....xxxx
     * | qb | fp |  64-8 bits |
     * The resulting hash will be the original hash shifted to the lower bits
     * with an unary delimiter in between:
     *  0000....0000 qqqq 0 ffff
     * |  64-7 bits | qb |d| fp |
     * The shifting to the lower bits is done because the internal functions
     * `insert1` and `insertN` assumes the hash to be in the lower bits. 
     * @param hash The hash of a key. 
     */
    inline
    void insert_unary(uint64_t& hash) const {
        uint64_t quotient = hash >> (64ULL - metadata_->quotient_bits);
        // 65 instead of 64 because we account for one unary bit.
        // The BITMASK here should actually be on bits_per_slot - 1, but we
        // remove the `- 1` for optimization because MSB of the fingerprint will
        // always be 1 (the unary bit).
        uint64_t fingerprint = hash >> (65ULL - metadata_->hash_bits) &
                               BITMASK(metadata_->bits_per_slot - 1);
        hash = quotient << metadata_->bits_per_slot | fingerprint;
    }

    /**
     * Moves the upper-most bit of the fingerprint after the unary counter to the lower-most bit of 
     * the quotient. This increases the unary counter of the fingerprint. For example, if the 
     * quotient was 1010 and the fingerprint was 110011, the upper-most bit of the fingerprint (0) 
     * will be appended to the quotient, making the quotient 10100 and the fingerprint 111011. This 
     * method modifies the quotient and fingerprint values directly but also returns the resulting 
     * appended hash for convenience. 
     * @note This method requires the fingerprint to have been stripped off of the associated value. 
     * It is why the width calculation is done with`fingerprint_bits` instead of `bits_per_slot`. 
     * @param q The quotient part of the hashed representation of a key.
     * @param f The fingerprint of the hashed representation of a key.
     */
    inline
    uint64_t adjust_fingerprint_length(uint64_t& q, uint64_t& f) const {
        int unary_count = __builtin_clzll((~f) << (64 - metadata_->fingerprint_bits)) + 1;
        uint64_t new_fp_len = metadata_->fingerprint_bits - unary_count - 1;
        uint64_t parity = (f >> new_fp_len) & 1;

        uint64_t unary = BITMASK(unary_count);
        f = (f & BITMASK(new_fp_len)) | (unary << (new_fp_len + 1));
        q = (q << 1) | parity;

        return (q << metadata_->fingerprint_bits) | f;
    }

    /**
     * TODOX
     * Checks whether the stored fingerprint (with unary) matches that of the queried fingerprint 
     * (without unary).
     * @param stored_fingerprint The stored fingerprint with unary padding.
     * @param queried_fingerprint The queried hash without unary padding.
     * @returns The length of matching bits. 0 if there is no matching. 
     */
    inline
    uint64_t check_fingerprint(const uint64_t& stored_fingerprint,
                               const uint64_t& queried_fingerprint) const {
        // Length of unary padding. 
        const uint64_t p = __builtin_clzll((~stored_fingerprint)
                           << (64 - metadata_->bits_per_slot)) + 1;
        const uint64_t mask = BITMASK(metadata_->bits_per_slot - p);
        // std::cout << "    p         : " << p << std::endl;
        // std::cout << "    mask      : " << std::bitset<8>(mask) << std::endl;
        // std::cout << "    stored(o) : " << std::bitset<8>(stored_fingerprint) << std::endl;
        // std::cout << "    stored(m) : " << std::bitset<8>(stored_fingerprint & mask) << std::endl;
        // std::cout << "    querid(o) : " << std::bitset<8>(queried_fingerprint) << std::endl;
        // std::cout << "    querid(m) : " << std::bitset<8>(queried_fingerprint >> p) << std::endl;
        if ((stored_fingerprint & mask) == (queried_fingerprint >> p)) {
            return metadata_->bits_per_slot - p;
        } else {
            return 0;
        }
    }

    /**
     * Checks whether the stored fingerprint (with unary) matches that of the queried fingerprint 
     * (without unary). This method assumes the unary padding is located at the lsb of the 
     * fingerprint. 
     * @param stored_fingerprint The stored fingerprint with unary padding.
     * @param queried_fingerprint The queried hash without unary padding.
     * @returns True, if the upper bits of `stored_fingerprint` matches the upper bits of 
     * `queried_fingerprint`. False, otherwise.
     * @deprecated Use `check_fingerprint` instead. 
     */
    inline
    bool check_fingerprint_deprecated(const uint64_t& stored_fingerprint,
                                    const uint64_t& queried_fingerprint) const {
        // Length of unary padding. 
        const uint64_t p = std::countr_zero(stored_fingerprint) + 1;
        return (stored_fingerprint >> p) == (queried_fingerprint >> p);
    }

    /**
     * Shifts the filter's slots to the right starting from `first` to `last`, inclusive, by 
     * `distance` slots. Slots shifter over will be overwritten, while the empty slots resulting 
     * from the shift will be zeroed out.
     * @param first, last - The range of slots to shift. Both endpoints are included in the range.
     * @param distance - The number of slots to shift to the right by.
     */
    void shift_slots(int64_t first, uint64_t last, uint64_t distance);

    /**
     * Shifts the filter's `runends` bitmap to the right starting from `first` to `last`, inclusive, 
     * by `distance` bits. Bits shifter over will be overwritten, while the empty bits resulting 
     * from the shift will be zeroed out.
     * @param first, last - The range of bits to shift. Both endpoints are included in the range.
     * @param distance - The number of bits to shift to the right by.
     */
    void shift_runends(int64_t first, uint64_t last, uint64_t distance);

    bool shift_for_inserts(int operation, 
                           uint64_t slot_index, 
                           uint64_t overwrite_index,
                           const uint64_t* remainders, 
                           uint64_t total_remainders, 
                           uint64_t noverwrites);

    int shift_for_deletes(int operation, 
                          uint64_t bucket_index, 
                          uint64_t overwrite_index,
                          const uint64_t* remainders, 
                          uint64_t total_remainders, 
                          uint64_t old_length);

    /**
     * Deletes the entire run at `canonical_slot` and returns the contents of 
     * the run. 
     * @param canonical_slot The canonical slot of the run to be deleted.
     * @returns A buffer of the entire run.
     */
    uint64_t* delete_run(uint64_t canonical_slot, uint64_t& run_length);

    /**
     * Inserts an entire run at `canonical_slot`.
     * @param canonical_slot The canonical slot of the run to be inserted. 
     * @param buffer The buffer containing the contents of the entire run.
     * @param run_length The length of the run.
     * @returns Distance from the canonical slot to the runend index. 
     */
    uint64_t insert_run(uint64_t canonical_slot, uint64_t* buffer, 
                        uint64_t run_length);

    inline void find_next_n_empty_slots(uint64_t from, int64_t n, 
                                        uint64_t *indices) {
        while (n) {
            indices[--n] = find_first_empty_slot(from);
            from = indices[n] + 1;
        }
    }

    bool is_runend(const uint64_t& index) const {
        return (METADATA_WORD(runends, index) >> 
                    ((index % kSlotsPerBlock) % 64)) & 1ULL;
    }

    bool is_occupied(const uint64_t& index) const {
        return (METADATA_WORD(occupieds, index) >> 
                    ((index % kSlotsPerBlock) % 64)) & 1ULL;
    }

    bool is_empty(const uint64_t& slot_index) const {
        return offset_lower_bound(slot_index) == 0;
    }

    bool might_be_empty(const uint64_t& slot_index) const {
        return !is_occupied(slot_index) && !is_runend(slot_index);
    }

    ////////////////////////
    // Counter functions. //
    ////////////////////////

    /**
     Counter format:
     0 xs:    <empty string>
     1 x:     x
     2 xs:    xx
     3 0s:    000
     >2 xs:   xbc...cx  for x != 0, b < x, c != 0, x
     >3 0s:   0c...c00  for c != 0
     */
    uint64_t* encode_counter(uint64_t remainder, 
                             uint64_t counter, 
                             uint64_t* slots);

    /**
     * @returns The length of the encoding.
     */
    uint64_t decode_counter(uint64_t index, 
                            uint64_t& remainder, 
                            uint64_t& count) const;
    

    ////////////////////////
    // Utility functions. //
    ////////////////////////

    // Space usage info
    bool is_auto_resize_enabled() const {
        return metadata_->auto_resize;
    }
	uint64_t size_in_bytes() const {
        return metadata_->total_size_in_bytes;
    }
	uint64_t count_slots() const {
        return metadata_->nslots;
    }
	uint64_t count_occupied_slots() const {
        // metadata_->pc_noccupied_slots.sync();
        return metadata_->pc_noccupied_slots.get_counter();
        // return metadata_->noccupied_slots;
        // return metadata_->atomic_noccupied_slots;
    }

    // Bit size info
    uint64_t get_num_hash_bits() const {
        return metadata_->hash_bits;
    }
	uint64_t get_num_fingerprint_bits() const {
        return metadata_->fingerprint_bits;
    }
	uint64_t get_bits_per_slot() const {
        return metadata_->bits_per_slot;
    }

    // Number of (distinct) key-value pairs.
	uint64_t count_keys() const {
        return metadata_->nelts;
    }
	uint64_t count_distinct_prefixes() const {
        return metadata_->ndistinct_elts;
    }
    uint64_t count_key_value(uint64_t key, uint64_t value, uint8_t flags) const;

    //////////////////////////
    // Debugging functions. //
    //////////////////////////

    void debug_dump_block() const;

    /** 
     * Prints the contents of the current occupieds and runends from a given
     * index. 
     */
    void print_from_index(const uint64_t& index) const {
        std::cout << "=================================" << std::endl;
        std::cout << "occupied : " << index << std::endl;
        uint64_t runend = run_end(index);
        std::cout << "runend   : " << runend << std::endl;
        uint64_t runstart = run_end(index - 1);
        std::cout << "runstart : " << runstart << std::endl;
        for (int i = (index / 64) * 64 + 64 - 1; i >= (index / 64) * 64; --i) {
            if (i >= metadata_->xnslots) break;
            std::cout << (i % 10);
        }
        std::cout << std::endl;
        qfblock* qb = get_block(index / kSlotsPerBlock);
        uint64_t occupieds = qb->occupieds[0];
        std::cout << std::bitset<64>(occupieds) << std::endl;
        std::cout << "offset " << +qb->offset << std::endl;
        for (int i = (index / 64) * 64; i < (index / 64) * 64 + 64; ++i) {
            if (i >= metadata_->xnslots) break;
            std::cout << i << " " << std::bitset<16>(get_slot(i)) << " ";
            if (is_runend(i)) std::cout << std::endl;
        }
        std::cout << std::endl;
        std::cout << "in the runend: " << std::endl;

        for (int i = (runend / 64) * 64 + 64 - 1; i >= (runend / 64) * 64; --i) {
            if (i >= metadata_->xnslots) break;
            std::cout << (i % 10);
        }
        std::cout << std::endl;
        uint64_t runends = get_block(runend / kSlotsPerBlock)->runends[0];
        std::cout << std::bitset<64>(runends) << std::endl;
        for (int i = (runend / 64) * 64; i < (runend / 64) * 64 + 64; ++i) {
            if (i >= metadata_->xnslots) break;
            std::cout << i << " " << std::bitset<16>(get_slot(i)) << " ";
            if (is_runend(i)) std::cout << std::endl;
        }
        std::cout << "---------------------------------" << std::endl;
        std::cout << std::endl;
    }

    public:
    /**
     * Prints the current status of runtime locks. 
     * @warning This method is not thread-safe. If this method is called while the lock is being
     * resized, it will cause undefined behavior (most likely a segfault). 
     */
    void print_lock_status(bool verbose=false) const {
        std::cout << "printing" << std::endl;
        if (!verbose) {
            for (uint64_t zz = 0; zz < runtimedata_->num_locks; ++zz) {
                if (runtimedata_->locks[zz].lock_.lock_.load(std::memory_order_acquire)) {
                    std::cout << zz << " :: locked" << std::endl;
                }
            }
        } else {
            for (uint64_t zz = 0; zz < runtimedata_->num_locks; ++zz) {
                if (runtimedata_->locks[zz].lock_.lock_.load(std::memory_order_acquire)) {
                    std::cout << zz << " :: locked" << std::endl;
                } 
                else {
                    std::cout << zz << " :: unlocked" << std::endl;
                }
            }
        }
        std::cout << "printing finished" << std::endl;
    }

};  // class Zeno

Quotient::Quotient(uint64_t exp_size, uint64_t hash_bits, uint64_t value_bits, 
           uint64_t reciprocal_ratio, hashmode hash_mode, uint32_t seed, double threshold = 0.8) {
    uint64_t num_slots, xnslots, nblocks;
    uint64_t fingerprint_bits, bits_per_slot;
    uint64_t buffer_size = 0;
    uint64_t qfblock_size = 0;
    uint64_t total_num_bytes;

    num_slots = 1ULL << exp_size;
    xnslots = num_slots + 10*sqrt((double)num_slots);
    nblocks = (xnslots + kSlotsPerBlock - 1) / kSlotsPerBlock;
    fingerprint_bits = hash_bits;
    while (num_slots > 1 && fingerprint_bits > 0) {
        fingerprint_bits--;
        num_slots >>= 1;
    }
    assert(fingerprint_bits >= 2);
    bits_per_slot = fingerprint_bits + value_bits;
    assert(bits_per_slot > 1);
    
    // Allocate memory for qfblocks. 
    qfblock_size = sizeof(qfblock) + (kSlotsPerBlock * bits_per_slot / 8);

    uint8_t* buffer;
    buffer_size = nblocks * qfblock_size;
    buffer = new uint8_t[buffer_size]{};
    metadata_ = new qfmetadata;
    blocks_ = reinterpret_cast<qfblock*>(buffer);

    metadata_->total_memory_usage = total_num_bytes;

    metadata_->magic_endian_number = kMagicNumber;
    metadata_->auto_resize = 0;
    metadata_->hash_mode = hash_mode;
    metadata_->reserved = 0;
    metadata_->total_size_in_bytes = buffer_size;
    metadata_->seed = seed;
    metadata_->nslots = 1ULL << exp_size;
    metadata_->xnslots = xnslots;
    metadata_->hash_bits = hash_bits;
    metadata_->value_bits = value_bits;
    metadata_->fingerprint_bits = fingerprint_bits;
    metadata_->quotient_bits = exp_size;
    metadata_->bits_per_slot = bits_per_slot;
    metadata_->range = metadata_->nslots;
    metadata_->range <<= metadata_->fingerprint_bits;
    metadata_->nblocks = (metadata_->xnslots + kSlotsPerBlock - 1) / kSlotsPerBlock;
    metadata_->nelts = 0;
    metadata_->ndistinct_elts = 0;
    metadata_->noccupied_slots = 0;
    metadata_->qfblock_size = qfblock_size;
    metadata_->expansion_threshold = threshold;
    metadata_->num_expansions = 0;
    
    runtimedata_ = new qfruntime;
    runtimedata_->num_locks = (metadata_->xnslots / kNumSlotsToLock) + 2;
    // runtimedata_->locks = new spinlock_padded[runtimedata_->num_locks]{};
    size_t lock_bytes = runtimedata_->num_locks * sizeof(spinlock_padded);
    runtimedata_->locks = (spinlock_padded*)malloc(lock_bytes);
    if (runtimedata_->locks) memset(runtimedata_->locks, 0, lock_bytes);
#ifdef LOG_WAIT_TIME
    runtimedata_->wait_times = reinterpret_cast<WaitTimeData *>
                               (new WaitTimeData[runtimedata->num_locks + 1]{});
#endif
}

inline Quotient::~Quotient() {
    // if (runtimedata_) {
    //     while (resizing_region.load(std::memory_order_acquire) != -1) {
    //         std::this_thread::yield();
    //     }
    // }
    if (metadata_) {
        delete metadata_;
    }
    if (runtimedata_) {
        if (runtimedata_->locks) free(runtimedata_->locks);
        delete runtimedata_;
    }
    if (blocks_) {
        delete blocks_;
    }
}

inline Quotient& Quotient::operator=(Quotient&& other) noexcept {
    metadata_ = other.metadata_;
    blocks_ = other.blocks_;
    runtimedata_ = other.runtimedata_;
    other.metadata_ = nullptr;
    other.blocks_ = nullptr;
    other.runtimedata_ = nullptr;
    return *this;
}

int Quotient::insert(uint64_t key, uint64_t value, uint64_t count, uint8_t flags) {
    if (count_occupied_slots() >= metadata_->nslots * metadata_->expansion_threshold) {
        // No more space. Either grow or fail based on `auto_resize`
        if (metadata_->auto_resize) {
            int32_t grow_ret = 1;

            if (GET_NO_LOCK(flags) == kNoLock) {
                grow_ret = grow(0, 0, flags);
            } else {
                int64_t expected_region = -1;

                if (resizing_region.compare_exchange_strong(expected_region, 0, 
                    std::memory_order_seq_cst)) {

                    for (auto& counter : active_threads) {
                        while (counter.count.load(std::memory_order_seq_cst) > 0) {
                            std::this_thread::yield(); 
                        }
                    }
                    expansion_time += util::timing([&] {
                        grow_ret = grow(0, 0, flags);
                    });
                    
                    resizing_region.store(-1, std::memory_order_release);
                    resizing_region.notify_all();
                } else {
                    while (resizing_region.load(std::memory_order_acquire) == 0) {
                        resizing_region.wait(0, std::memory_order_relaxed);
                    }
                }
            }
        }
        else {
            return kErrNoSpace;
        }
    }

    if (count == 0) return 0;

    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = MurmurHash64A((void*)&key, sizeof(key), metadata_->seed);
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
        key = key >> (64ULL - metadata_->hash_bits);
    }
    uint64_t hash = (key << metadata_->value_bits) | (value & BITMASK(metadata_->value_bits));
    
    int ret = 1;

    if (count == 1) {
        ret = insert1(hash, flags);
    } else {
        ret = insertN(hash, count, flags);
    }

    // check for fullness based on the distance from the home slot to the slot
    // in which the key is inserted
    if (ret == kErrNoSpace ) { // || ret > kDistanceFromHomeSlotCutoff
        float load_factor = count_occupied_slots() / (float)metadata_->nslots;
        if (metadata_->auto_resize) {
            fprintf(stdout, "Resizing filter...\n");
            ret = grow(hash, count, flags);
            if (ret > 0) {
                std::cerr << "Resize finished." << std::endl;
            } else {
                std::cerr << "Resize failed." << std::endl;
                ret = kErrNoSpace;
            }
        } else {
            std::cerr << "RSQF is filling up." << std::endl;
            ret = kErrNoSpace;
        }
    }

    return ret;
}

inline
int32_t Quotient::remove(uint64_t key, uint64_t value, uint64_t count, 
                     uint8_t flags) {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
    }
    uint64_t hash = (key << metadata_->value_bits) | 
                    (value & BITMASK(metadata_->value_bits));
    int32_t ret = 0;
    ret = remove_longest_internal(hash, count, flags);
    return ret;
}

int Quotient::delete_key_value(uint64_t key, uint64_t value, uint8_t flags) {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
    }
    uint64_t hash = (key << metadata_->value_bits) | (value & BITMASK(metadata_->value_bits));

    return remove_internal(hash, std::numeric_limits<uint64_t>::max(), flags);
}

int64_t Quotient::grow(uint64_t dangling_hash, uint64_t dangling_count, uint8_t flags) {
    uint64_t q_bits = metadata_->hash_bits - metadata_->fingerprint_bits + 1ULL;
    if (metadata_->fingerprint_bits <= 3) {
        return kErrNoFpBits;
    }
    
    Quotient new_filter(q_bits, metadata_->hash_bits, metadata_->value_bits,
                    0ULL /* if not Zeno, there is no reciprocal ratio*/,
                    metadata_->hash_mode, metadata_->seed);
    new_filter.set_auto_resize(metadata_->auto_resize);

    uint64_t fingerprint, hash, value, count, quotient;
    int64_t ret_numkeys = 0;
    int32_t status = 0;
    iterator<Quotient> it(this, 0);

    uint64_t canonical_slot;

    for (; it.is_valid(); ++it) {
        canonical_slot = it.get_canonical_slot();
        it.get_entry(fingerprint, value, count);
        hash = (canonical_slot << metadata_->fingerprint_bits) | fingerprint;

        int ret = new_filter.insert(hash, value, count, kNoLock | kKeyIsHash);
        if (ret < 0) {
            std::cerr << "Failed to insert key: " << hash << " into the new filter." << std::endl;
            return ret;
        }
        ++ret_numkeys;
    }
    
    // There is a 'dangling' hash to be inserted.
    if (dangling_count) {
        uint64_t fingerprint = dangling_hash & BITMASK(metadata_->bits_per_slot);
        value = fingerprint & BITMASK(metadata_->value_bits);
        fingerprint >>= metadata_->value_bits;

        canonical_slot = dangling_hash >> metadata_->bits_per_slot;

        hash = (canonical_slot << metadata_->fingerprint_bits) | fingerprint;

        int ret = new_filter.insert(hash, value, dangling_count, kNoLock | kKeyIsHash);
        if (ret < 0) {
            std::cerr << "Failed to insert key: " << hash << " into the new filter." << std::endl;
            return ret;
        }
        ++ret_numkeys;
    }

    delete[] runtimedata_->locks;
    delete runtimedata_;
    delete metadata_;
    delete blocks_;
    *this = std::move(new_filter);

    return ret_numkeys;
}

// TODO: change logic to query for longest matching kv, similar to remove_internal
uint64_t Quotient::query(uint64_t key, uint64_t& value, uint8_t flags) {
    size_t thread_idx;
    if (GET_NO_LOCK(flags) != kNoLock) {
        thread_idx = std::hash<std::thread::id>{}(std::this_thread::get_id()) % active_threads.size();
        while (true) {
            active_threads[thread_idx].count.fetch_add(1, std::memory_order_seq_cst);
            if (resizing_region.load(std::memory_order_seq_cst) != -1) {
                active_threads[thread_idx].count.fetch_sub(1, std::memory_order_seq_cst);
                while (resizing_region.load(std::memory_order_acquire) == 0) {
                    resizing_region.wait(0, std::memory_order_relaxed);
                }
                continue;
            }
            break;
        }
    }

    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        if (metadata_->hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));}
        else if (metadata_->hash_mode == hashmode::Invertible)
            key = hash_64(key, BITMASK(metadata_->hash_bits));
    }
    uint64_t hash = key;
    uint64_t hash_remainder   = hash & BITMASK(metadata_->fingerprint_bits);
    uint64_t hash_bucket_index = hash >> metadata_->fingerprint_bits;

    int64_t start_region;      // For storing the first region to lock/unlock
    int64_t clusterend_region; // For storing the last region to lock/unlock

    // If a query falls into an expanding region, wait until migration is done to the new region. 
    if (GET_NO_LOCK(flags) != kNoLock) {
        
        clusterend_region = hash_bucket_index / kNumSlotsToLock;
        start_region = clusterend_region;

        // if (!zeno_lock_region_conditionally(start_region, flags | kSecondTryLock)) {
        //     return kErrCouldntLock;
        // }
        // std::cout << "query locking " << clusterend_region << std::endl;
        qf_lock(hash_bucket_index, true, flags);
        // std::cout << "query locked  " << clusterend_region << std::endl;
    }

    if (!is_occupied(hash_bucket_index)) {
        if (GET_NO_LOCK(flags) != kNoLock) {
            // zeno_unlock_region(start_region);
            qf_unlock(hash_bucket_index, true);
            active_threads[thread_idx].count.fetch_sub(1, std::memory_order_release);
        }
        return 0;
    }

    int64_t runstart_index = hash_bucket_index == 0 
                           ? 0 
                           : run_end(hash_bucket_index - 1) + 1;
    if (runstart_index < hash_bucket_index)
        runstart_index = hash_bucket_index;

    uint64_t current_remainder, current_count, current_end;
    uint64_t runend_index = run_end(hash_bucket_index);
    // if (GET_NO_LOCK(flags) == kNoLock) runend_index = run_end(hash_bucket_index);
    // else {
    //     if (zeno_lock_cluster(hash_bucket_index, runend_index, clusterend_region, flags | kSecondTryLock) 
    //         == kErrCouldntLock) { // Regions are unlocked inside
    //         return kErrCouldntLock;
    //     } else {
    //         // Locked region
    //     }
    // }

    do {
        current_end = decode_counter(runstart_index, current_remainder, current_count);
        value = current_remainder & BITMASK(metadata_->value_bits);
        current_remainder = current_remainder >> metadata_->value_bits;
        if (current_remainder == hash_remainder) {
            if (GET_NO_LOCK(flags) != kNoLock) {
                // while (true) {
                //     zeno_unlock_region(clusterend_region);
                //     if (clusterend_region == start_region) break;
                //     --clusterend_region;
                // }

                // std::cout << "query unlock " << hash_bucket_index << std::endl;
                qf_unlock(hash_bucket_index, true);
                active_threads[thread_idx].count.fetch_sub(1, std::memory_order_release);
            }
            return current_count;
        }
        runstart_index = current_end + 1;
    } while (runend_index != current_end);

    if (GET_NO_LOCK(flags) != kNoLock) {
        // while (true) {
        //     zeno_unlock_region(clusterend_region);
        //     if (clusterend_region == start_region) break;
        //     --clusterend_region;
        // }

        // std::cout << "query unlock " << hash_bucket_index << std::endl;
        qf_unlock(hash_bucket_index, true);
        active_threads[thread_idx].count.fetch_sub(1, std::memory_order_release);
    }

    return 0;
}

inline
int Quotient::insert1(uint64_t hash, uint8_t flags) {
    int ret_distance = 0;
    uint64_t hash_remainder = hash & BITMASK(metadata_->bits_per_slot);
    uint64_t hash_bucket_index = hash >> metadata_->bits_per_slot;
    uint64_t hash_bucket_block_offset = hash_bucket_index % kSlotsPerBlock;
    int64_t start_region;      // For storing the first region to lock/unlock
    int64_t clusterend_region; // For storing the last region to lock/unlock

    size_t thread_idx;

    if (GET_NO_LOCK(flags) != kNoLock) {
        clusterend_region = hash_bucket_index / kNumSlotsToLock;
        start_region = clusterend_region;

        thread_idx = std::hash<std::thread::id>{}(std::this_thread::get_id()) % active_threads.size();
        while (true) {
            active_threads[thread_idx].count.fetch_add(1, std::memory_order_seq_cst);
            if (resizing_region.load(std::memory_order_seq_cst) != -1) {
                active_threads[thread_idx].count.fetch_sub(1, std::memory_order_seq_cst);
                while (resizing_region.load(std::memory_order_acquire) == 0) {
                    resizing_region.wait(0, std::memory_order_relaxed);
                }
                continue;
            }
            break;
        }

        // if (!zeno_lock_region_conditionally(start_region, flags | kSecondTryLock)) {
        //     return kErrCouldntLock;
        // }
        // std::cout << "insert locking " << clusterend_region << std::endl;
        

        // 0130mod
        qf_lock(hash_bucket_index, true, flags);
        // if (!zeno_lock_region_conditionally(start_region, flags)) {
        //     return kErrCouldntLock;
        // }

        // std::cout << "insert locked  " << clusterend_region << std::endl;
    }
    if (is_empty(hash_bucket_index)) {
        METADATA_WORD(runends, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        set_slot(hash_bucket_index, hash_remainder);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        
        ret_distance = 0;
        modify_metadata(metadata_->pc_noccupied_slots, 1);
    } else {
        // 0130 mod
        uint64_t runend_index = run_end(hash_bucket_index);
        // uint64_t runend_index;
        // if (GET_NO_LOCK(flags) == kNoLock) runend_index = run_end(hash_bucket_index);
        // else {
        //     if (zeno_lock_cluster(hash_bucket_index, runend_index, clusterend_region, flags)
        //         == kErrCouldntLock) {
        //         return kErrCouldntLock;
        //     } else {

        //     }
        // }

        // INCREMENTAL
        // if (GET_NO_LOCK(flags) == kNoLock) runend_index = run_end(hash_bucket_index);
        // else {
        //     if (zeno_lock_cluster(hash_bucket_index, runend_index, clusterend_region, flags | kSecondTryLock) 
        //         == kErrCouldntLock) { // Regions are unlocked inside
        //         return kErrCouldntLock;
        //     } else {
        //         // Locked region
        //         std::cout << clusterend_region << "? ";
        //     }
        // }

        int operation = 0; /* Insert into empty bucket */
        uint64_t insert_index = runend_index + 1;
        uint64_t new_value = hash_remainder;

        uint64_t runstart_index = hash_bucket_index == 0 
                                ? 0 
                                : run_end(hash_bucket_index - 1) + 1;

        if (is_occupied(hash_bucket_index)) {
            /* Find the counter for this remainder if it exists. */
            uint64_t current_remainder = get_slot(runstart_index);
            uint64_t zero_terminator = runstart_index;

            /* The counter for 0 is special. */
            if (current_remainder == 0) {
                uint64_t t = runstart_index + 1;
                while (t < runend_index && get_slot(t) != 0)
                    t++;
                if (t < runend_index && get_slot(t+1) == 0)
                    zero_terminator = t+1; /* Three or more 0s */
                else if (runstart_index < runend_index &&  get_slot(runstart_index + 1) == 0)
                    zero_terminator = runstart_index + 1; /* Exactly two 0s */
                /* Otherwise, exactly one 0 
                   (i.e. zero_terminator == runstart_index) */

                /* May read past end of run, but that's OK because loop below
                    can handle that */
                if (hash_remainder != 0) {
                    runstart_index = zero_terminator + 1;
                    current_remainder = get_slot(runstart_index);
                }
            }

            /* Skip over counters for other remainders. */
            while (current_remainder < hash_remainder && runstart_index <= runend_index) {
                /* If this remainder has an extended counter, skip over it. */
                if (runstart_index < runend_index && 
                        get_slot(runstart_index + 1) < current_remainder) {
                    runstart_index = runstart_index + 2;
                    while (runstart_index < runend_index &&
                                get_slot(runstart_index) != current_remainder)
                        runstart_index++;
                    runstart_index++;

                    /* This remainder has a simple counter. */
                } else {
                    runstart_index++;
                }

                /* This may read past the end of the run, but the while loop
                    condition will prevent us from using the invalid result in
                    that case. */
                current_remainder = get_slot(runstart_index);
            }

            /* If this is the first time we've inserted the new remainder,
                and it is larger than any remainder in the run. */
            if (runstart_index > runend_index) {
                operation = 1;
                insert_index = runstart_index;
                new_value = hash_remainder;
                // modify_metadata(&metadata_->ndistinct_elts, 1);

            /* This is the first time we're inserting this remainder, but
                there are larger remainders already in the run. */
            } else if (current_remainder != hash_remainder) {
                operation = 2; /* Inserting */
                insert_index = runstart_index;
                new_value = hash_remainder;
                // modify_metadata(&metadata_->ndistinct_elts, 1);

            /* Cases below here: we're incrementing the (simple or
                extended) counter for this remainder. */

            /* If there's exactly one instance of this remainder. */
            } else if (runstart_index == runend_index || 
                (hash_remainder > 0 && get_slot(runstart_index + 1) > hash_remainder) ||
                (hash_remainder == 0 && zero_terminator == runstart_index)) {
                operation = 2; /* Insert */
                insert_index = runstart_index;
                new_value = hash_remainder;

            /* If there are exactly two instances of this remainder. */
            } else if ((hash_remainder > 0 && get_slot(runstart_index + 1) == hash_remainder) ||
                        (hash_remainder == 0 && zero_terminator == runstart_index + 1)) {
                operation = 2; /* Insert */
                insert_index = runstart_index + 1;
                new_value = 0;

            /* Special case for three 0s */
            } else if (hash_remainder == 0 && zero_terminator == runstart_index + 2) {
                operation = 2; /* Insert */
                insert_index = runstart_index + 1;
                new_value = 1;

            /* There is an extended counter for this remainder. */
            } else {

                /* Move to the LSD of the counter. */
                insert_index = runstart_index + 1;
                while (get_slot(insert_index+1) != hash_remainder)
                    insert_index++;

                /* Increment the counter. */
                uint64_t digit, carry;
                do {
                    carry = 0;
                    digit = get_slot(insert_index);
                    // Convert a leading 0 (which is special) to a normal encoded digit
                    if (digit == 0) {
                        digit++;
                        if (digit == current_remainder)
                            digit++;
                    }

                    // Increment the digit
                    digit = (digit + 1) & BITMASK(metadata_->bits_per_slot);

                    // Ensure digit meets our encoding requirements
                    if (digit == 0) {
                        digit++;
                        carry = 1;
                    }
                    if (digit == current_remainder)
                        digit = (digit + 1) & BITMASK(metadata_->bits_per_slot);
                    if (digit == 0) {
                        digit++;
                        carry = 1;
                    }

                    set_slot(insert_index, digit);
                    insert_index--;
                } while(insert_index > runstart_index && carry);

                /* If the counter needs to be expanded. */
                if (insert_index == runstart_index && (carry > 0 || 
                    (current_remainder != 0 && digit >= current_remainder)))
                {
                    operation = 2; /* insert */
                    insert_index = runstart_index + 1;
                    // To prepend a 0 before the counter if the MSD is greater than the rem
                    if (!carry) {
                        new_value = 0;

                    // Increment the new value because we don't use 0 to encode counters
                    } else if (carry) { 
                        new_value = 2;

                        // If the rem is greater than or equal to the new_value, then fail
                        if (current_remainder > 0)
                            assert(new_value < current_remainder);
                    }
                } else {
                    operation = -1;
                }
            }
        } else {
            // modify_metadata(&metadata_->ndistinct_elts, 1);
        }

        if (operation >= 0) {
            uint64_t empty_slot_index = find_first_empty_slot(runend_index + 1);
            shift_remainders(insert_index, empty_slot_index);

            set_slot(insert_index, new_value);
            ret_distance = insert_index - hash_bucket_index;

            shift_runends(insert_index, empty_slot_index-1, 1);
            switch (operation) {
                case 0:
                    METADATA_WORD(runends, insert_index) |= 
                        (1ULL << ((insert_index % kSlotsPerBlock) % 64));
                    break;
                case 1:
                    METADATA_WORD(runends, insert_index-1) &= 
                        ~(1ULL << (((insert_index-1) % kSlotsPerBlock) % 64));
                    METADATA_WORD(runends, insert_index) |= 
                        1ULL << ((insert_index % kSlotsPerBlock) % 64);
                    break;
                case 2:
                    METADATA_WORD(runends, insert_index) &= 
                        ~(1ULL << ((insert_index % kSlotsPerBlock) % 64));
                    break;
                default:
                    std::cerr << "Invalid operation " << operation << std::endl;
                    abort();
            }
            /* 
            * Increment the offset for each block between the hash bucket index
            * and block of the empty slot  
            * */
            uint64_t i;
            for (i = hash_bucket_index / kSlotsPerBlock + 1; i <=
                    empty_slot_index/kSlotsPerBlock; i++) {
                if (get_block(i)->offset < BITMASK(8*sizeof(std::declval<qfblock>().offset)))
                    get_block(i)->offset++;
                assert(get_block(i)->offset != 0);
            }
            // modify_metadata(&metadata_->noccupied_slots, 1);
            modify_metadata(metadata_->pc_noccupied_slots, 1);
        }
        // modify_metadata(&metadata_->nelts, 1);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        // while (true) {
        //     zeno_unlock_region(clusterend_region);
        //     if (clusterend_region == start_region) break;
        //     --clusterend_region;
        // }
        // 0130 mod
        qf_unlock(hash_bucket_index, true);
        active_threads[thread_idx].count.fetch_sub(1, std::memory_order_release);
        // while (true) {
        //     zeno_unlock_region(clusterend_region);
        //     if (clusterend_region == start_region) break;
        //     --clusterend_region;
        // }
        // active_threads[thread_idx].count.fetch_sub(1, std::memory_order_relaxed);
    }

    return ret_distance;
}

inline
int Quotient::insertN(uint64_t hash, uint64_t count, uint8_t flags) {
    int ret_distance = 0;
    uint64_t hash_remainder = hash & BITMASK(metadata_->bits_per_slot);
    uint64_t hash_bucket_index = hash >> metadata_->bits_per_slot;
    uint64_t hash_bucket_block_offset = hash_bucket_index % kSlotsPerBlock;

    if (GET_NO_LOCK(flags) != kNoLock) {
        if (!zeno_lock(hash_bucket_index, /*small*/ false, flags))
            return kErrCouldntLock;
    }

    uint64_t runend_index = run_end(hash_bucket_index);
    
    /* Empty slot */
    if (might_be_empty(hash_bucket_index) && runend_index == hash_bucket_index) {
        METADATA_WORD(runends, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        set_slot(hash_bucket_index, hash_remainder);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        
        // modify_metadata(&metadata_->ndistinct_elts, 1);
        // modify_metadata(&metadata_->noccupied_slots, 1);
        // modify_metadata(&metadata_->nelts, 1);
        modify_metadata(metadata_->pc_noccupied_slots, 1);
        if (count > 1) {
            insertN(hash, count - 1, kNoLock);
        }
    } else { /* Non-empty slot */
        uint64_t new_values[67];
        int64_t runstart_index = hash_bucket_index == 0 
                               ? 0 
                               : run_end(hash_bucket_index - 1) + 1;
        bool ret;
        /* Empty bucket, but its slot is occupied. */
        if (!is_occupied(hash_bucket_index)) { 
            uint64_t* p = encode_counter(hash_remainder, count, &new_values[67]);

            ret = shift_for_inserts(0, 
                                    hash_bucket_index, 
                                    runstart_index, 
                                    p, 
                                    &new_values[67] - p, 
                                    0);
            if (!ret) return kErrNoSpace;
            // modify_metadata(&metadata_->ndistinct_elts, 1);
            ret_distance = runstart_index - hash_bucket_index;
        } else { /* Non-empty bucket */

            uint64_t current_remainder, current_count, current_end;

            /* Find the counter for this remainder, if one exists. */
            current_end = decode_counter(runstart_index, current_remainder, current_count);
            while ((current_remainder < hash_remainder) && (!is_runend(current_end))) {
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index, current_remainder, current_count);    
            }

            // If we reached the end of the run w/o finding a counter for this remainder, then 
            // append a counter for this remainder to the run.
            if (current_remainder < hash_remainder) {
                uint64_t* p = encode_counter(hash_remainder, count, &new_values[67]);
                ret = shift_for_inserts(1, /* Append to bucket */
                                        hash_bucket_index, 
                                        current_end + 1, 
                                        p, 
                                        &new_values[67] - p, 
                                        0);
                if (!ret)
                    return kErrNoSpace;
                // modify_metadata(&metadata_->ndistinct_elts, 1);
                ret_distance = (current_end + 1) - hash_bucket_index;
                /* Found a counter for this remainder.  Add in the new count. */
            } else if (current_remainder == hash_remainder) {
                uint64_t* p = encode_counter(hash_remainder, 
                                             current_count + count, 
                                             &new_values[67]);
                ret = shift_for_inserts(is_runend(current_end) ? 1 : 2, 
                                        hash_bucket_index, 
                                        runstart_index, 
                                        p, 
                                        &new_values[67] - p, 
                                        current_end - runstart_index + 1);
                if (!ret)
                    return kErrNoSpace;
                ret_distance = runstart_index - hash_bucket_index;
                /* No counter for this remainder, but there are larger remainders, so we're not 
                   appending to the bucket. */
            } else {
                uint64_t* p = encode_counter(hash_remainder, count, &new_values[67]);
                ret = shift_for_inserts(2, /* Insert to bucket */
                                        hash_bucket_index, 
                                        runstart_index, 
                                        p, 
                                        &new_values[67] - p, 
                                        0);
                if (!ret)
                    return kErrNoSpace;
                // modify_metadata(&metadata_->ndistinct_elts, 1);
            ret_distance = runstart_index - hash_bucket_index;
            }
        }
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        
        // modify_metadata(&metadata_->nelts, count);
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        zeno_unlock(hash_bucket_index, /*small*/ false);
    }

    return ret_distance;
}

inline
int Quotient::remove_longest_internal(uint64_t hash, uint64_t &count, 
                                  uint8_t runtime_lock) {
    int ret_numfreedslots = 0;
    uint64_t hash_remainder = hash & BITMASK(metadata_->bits_per_slot);
    uint64_t hash_bucket_index = hash >> metadata_->bits_per_slot;
    uint64_t current_remainder, current_count, current_end;
    uint64_t new_values[67];

    if (GET_NO_LOCK(runtime_lock) != kNoLock) {
        if (!zeno_lock(hash_bucket_index, /*small*/ false, runtime_lock))
            return kErrCouldntLock;
    }

    /* Empty bucket */
    if (!is_occupied(hash_bucket_index))
        return -1;

    uint64_t runstart_index = hash_bucket_index == 0 
                            ? 0 
                            : run_end(hash_bucket_index - 1) + 1;
    uint64_t original_runstart_index = runstart_index;
    uint64_t runend_index = run_end(hash_bucket_index);
    int only_item_in_the_run = 0;

    uint64_t longest_matching_index = 0;
    uint64_t longest_matching_bit = 0;

    /*Find the counter for this remainder, if one exists.*/
    // TODO: maybe keep the void entry encodings somewhere else, instead of
    // having to compute them every time?
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;

    current_end = decode_counter(runstart_index, current_remainder, 
                                 current_count);

    while (current_remainder != A && current_remainder != B) {
        uint64_t matching_bits = check_fingerprint(current_remainder,
                                                   hash_remainder);
        if (matching_bits > longest_matching_bit) {
            longest_matching_index = runstart_index;
            longest_matching_bit = matching_bits;
        }

        if (current_end == runend_index) break;

        runstart_index = current_end + 1;
        current_end = decode_counter(runstart_index, current_remainder, 
                                     current_count);
    }

    // There is a matching fingerprint
    if (longest_matching_bit) {
        runstart_index = longest_matching_index;
        current_end = decode_counter(runstart_index, current_remainder, 
                                    current_count);
        if (original_runstart_index == runstart_index && is_runend(current_end))
            only_item_in_the_run = 1;

        /* endode the new counter */
        uint64_t *p = encode_counter(current_remainder /* not hash_remainder */, 
                                     count > current_count ? 0 : current_count - count,
                                     &new_values[67]);
        count = count > current_count ? count - current_count : 0;
        ret_numfreedslots = shift_for_deletes(only_item_in_the_run,
                                            hash_bucket_index,
                                            runstart_index,
                                            p,
                                            &new_values[67] - p,
                                            current_end - runstart_index + 1);
    
    // There is no matching fingerprint but void sequence with A exists
    } else if (current_remainder == A) {
        uint64_t void_seq_start_index = runstart_index;
        uint64_t new_values[67];
        // There is no following B; must insert B and append A's at the end. Since this must be the 
        // last void entry that is being deleted, count here will always be count <= current_count
        if (is_runend(current_end)) {
            uint64_t* p = encode_counter(A, count, &new_values[67]);
            p = encode_counter(B, 1, p);
            count = 0;
            ret_numfreedslots += shift_for_inserts(1, 
                                                   hash_bucket_index, 
                                                   current_end + 1, 
                                                   p, 
                                                   &new_values[67] - p, 
                                                   0);

        // There are intermediate Bs. 
        } else {
            // Original runstart index of the first A seq
            uint64_t runstart_A = runstart_index;
            // Original count of the first A seq
            uint64_t count_A = current_count;
            // Read subsequent void run. This may be either A or B. 
            runstart_index = current_end + 1;
            current_end = decode_counter(runstart_index, current_remainder, current_count);

            // There are no following As after the Bs
            if (current_end == runend_index) {
                assert(count <= (count_A + current_count /* number of Bs */));
                // If the entire sequence can be deleted
                if (count == (count_A + current_count)) {
                    only_item_in_the_run = runstart_A == original_runstart_index ? 1 : 0;
                    uint64_t* p = encode_counter(A, 0, &new_values[67]);
                    count = 0;
                    ret_numfreedslots = shift_for_deletes(only_item_in_the_run,
                                                          hash_bucket_index,
                                                          runstart_A,
                                                          p, 
                                                          &new_values[67] - p,
                                                          current_end - runstart_A + 1);
                } else {
                    uint64_t encode_count = count >= count_A ? count_A : count;
                    // We don't set count to 0 because we might fallback into deleting more Bs
                    count = count - encode_count;
                    uint64_t* p = encode_counter(A, encode_count, &new_values[67]);
                    ret_numfreedslots = shift_for_inserts(1, 
                                                          hash_bucket_index,
                                                          current_end + 1,
                                                          p, 
                                                          &new_values[67] - p,
                                                          0);
                }

            // There are following As after the Bs
            } else {
                uint64_t runstart_B = runstart_index;
                // Original count of the B seq
                uint64_t count_B = current_count;
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index, current_remainder, current_count);
                assert(current_end == runend_index);
                assert(count <= (count_A - current_count + count_B));

                // Simply append As at the end of the run
                if (count <= count_A - current_count) {
                    uint64_t* p = encode_counter(A, count + current_count, &new_values[67]);
                    count = 0;
                    ret_numfreedslots = shift_for_inserts(1, 
                                                            hash_bucket_index,
                                                            runstart_index,
                                                            p,
                                                            &new_values[67] - p,
                                                            current_end - runstart_index + 1);

                // Append As at the end of the run and delete some Bs
                } else if (count < count_A - current_count + count_B) {
                    uint64_t encode_count = count_A;
                    uint64_t delete_count = count - (count_A - current_count);
                    // Append As
                    uint64_t* p = encode_counter(A, encode_count, &new_values[67]);
                    ret_numfreedslots += shift_for_inserts(1, hash_bucket_index, runstart_index,
                    p, &new_values[67] - p, current_end - runstart_index + 1);

                    // Delete Bs
                    uint64_t* q = encode_counter(B, count_B - delete_count, &new_values[67]);
                    ret_numfreedslots += shift_for_deletes(0, hash_bucket_index, runstart_B,
                    q, &new_values[67] - q, runstart_index - runstart_B);

                    count = 0;

                // Delete the entire run
                } else if (count == (count_A - current_count + count_B)) {
                    only_item_in_the_run = runstart_A == original_runstart_index ? 1 : 0;
                    uint64_t* p = encode_counter(A, 0, &new_values[67]);
                    count = 0;
                    ret_numfreedslots = shift_for_deletes(only_item_in_the_run, hash_bucket_index,
                    runstart_A, p, &new_values[67] - p, current_end - runstart_A + 1);

                } else {
                    std::cerr << "Invalid delete sequence." << std::endl;
                    abort();
                }

            }

        }
    // There is no matching fingerprint but void sequence with B exists
    } else if (current_remainder == B) {
        // Delete gen 1 void sequence
        if (current_end != runend_index) {
            // Delete Bs
            uint64_t* p = encode_counter(current_remainder /* B */,
                                         count > current_count ? 0 
                                         : current_count - count,
                                         &new_values[67]);
            ret_numfreedslots += shift_for_deletes(0,
                                            hash_bucket_index,
                                            runstart_index,
                                            p,
                                            &new_values[67] - p,
                                            current_end - runstart_index + 1);

            // Delete one A if we removed all Bs (i.e., all existing gen 1
            // void sequences in this run have been deleted)
            if (count >= current_count) {
                // Because we are deleting a gen 1 void sequence, there must
                // be at least 1 following A
                current_end = decode_counter(runstart_index,
                                             current_remainder, 
                                             current_count);
                p = encode_counter(current_remainder /* A */,
                                   current_count - 1,
                                   &new_values[67]);
                if (original_runstart_index == runstart_index &&
                    is_runend(current_end)) {
                    only_item_in_the_run = 1;
                }
                ret_numfreedslots += shift_for_deletes(only_item_in_the_run,
                                        hash_bucket_index,
                                        runstart_index,
                                        p,
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
            }
            count = count > current_count ? count - current_count : 0;
            
        // Delete B inside gen N void sequence (N > 3)
        } else {
            if (original_runstart_index == runstart_index &&
                is_runend(current_end)) {
                only_item_in_the_run = 1;
            }

            uint64_t *p = encode_counter(current_remainder, 
                        count > current_count ? 0 : current_count - count,
                        &new_values[67]);
            // Here, it cannot be the case that count > current_count, because this is the last void
            // seq to be deleted. If such a case happens, set the result count to 0 to avoid issuing
            // recursive deletions. 
            count = 0;
            ret_numfreedslots = shift_for_deletes(only_item_in_the_run,
                                        hash_bucket_index,
                                        runstart_index,
                                        p,
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
        }

    // There is no matching fingerprint and no void sequence
    } else {
        ret_numfreedslots = -1;
    }
    

    // update the nelements.
    // modify_metadata(&qf->runtimedata->pc_nelts, -count);

    if (GET_NO_LOCK(runtime_lock) != kNoLock) {
        zeno_unlock(hash_bucket_index, /*small*/ false);
    }

    return ret_numfreedslots;
}


inline int Quotient::remove_internal(uint64_t hash, uint64_t count, uint8_t runtime_lock) {
    int ret_numfreedslots = 0;
    uint64_t hash_remainder = hash & BITMASK(metadata_->bits_per_slot);
    uint64_t hash_bucket_index = hash >> metadata_->bits_per_slot;
    uint64_t current_remainder, current_count, current_end;
    uint64_t new_values[67];

    if (GET_NO_LOCK(runtime_lock) != kNoLock) {
        if (!zeno_lock(hash_bucket_index, /*small*/ false, runtime_lock))
            return kErrCouldntLock;
    }

    /* Empty bucket */
    if (!is_occupied(hash_bucket_index))
        return -1;

    uint64_t runstart_index = hash_bucket_index == 0 
                            ? 0 
                            : run_end(hash_bucket_index - 1) + 1;
    uint64_t original_runstart_index = runstart_index;
    int only_item_in_the_run = 0;

    /*Find the counter for this remainder, if one exists.*/
    current_end = decode_counter(runstart_index, current_remainder, current_count);
    while (current_remainder < hash_remainder && !is_runend(current_end)) {
        runstart_index = current_end + 1;
        current_end = decode_counter(runstart_index, current_remainder, current_count);
    }
    /* remainder not found in the given run */
    if (current_remainder != hash_remainder)
        return -1;

    if (original_runstart_index == runstart_index && is_runend(current_end))
        only_item_in_the_run = 1;

    /* endode the new counter */
    uint64_t *p = encode_counter(hash_remainder,
                                count > current_count ? 0 : current_count - count,
                                &new_values[67]);
    ret_numfreedslots = shift_for_deletes(only_item_in_the_run,
                                            hash_bucket_index,
                                            runstart_index,
                                            p,
                                            &new_values[67] - p,
                                            current_end - runstart_index + 1);

    // update the nelements.
    // modify_metadata(&qf->runtimedata->pc_nelts, -count);

    if (GET_NO_LOCK(runtime_lock) != kNoLock) {
        zeno_unlock(hash_bucket_index, /*small*/ false);
    }

    return ret_numfreedslots;
}

inline bool Quotient::qf_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % kNumSlotsToLock;
    // Read-lock the spinlock array. This blocks the expansion thread from modifying the spinlock
    // array when threads may be spinning on it. 
    if (small) {
#ifdef LOG_WAIT_TIME
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock],
                    hash_bucket_index / num_slots_to_lock, runtime_lock))
            return false;
        if (num_slots_to_lock - hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1],
                        hash_bucket_index / num_slots_to_lock + 1, runtime_lock)) {
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
                return false;
            }
        }
#else
        int64_t region = hash_bucket_index / kNumSlotsToLock;
        if (!spin_lock(&runtimedata_->locks[region], runtime_lock)) {
            return false;
        }
        if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
            if (!spin_lock(&runtimedata_->locks[region + 1], runtime_lock)) {
                spin_unlock(&runtimedata_->locks[region]);
                return false;
            }
        }
#endif
    } else {
#ifdef LOG_WAIT_TIME
        if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock], runtime_lock)) {
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1], runtime_lock)) {
            spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
#else
        if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize) {
            if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock], runtime_lock)) {
            if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1], runtime_lock)) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
            if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
            return false;
        }
#endif
    }
    return true;
}

inline void Quotient::qf_unlock(uint64_t hash_bucket_index, bool small) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % kNumSlotsToLock;
    if (small) {
        if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1]);
        }
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
    } else {
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1]);
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
        if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
    }
}

inline bool Quotient::zeno_lock(uint64_t hash_bucket_index, 
                            bool small, 
                            uint8_t runtime_lock) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % kNumSlotsToLock;
    // Read-lock the spinlock array. This blocks the expansion thread from modifying the spinlock
    // array when threads may be spinning on it. 
    // Wait if spinlock array resize is happening
    while (runtimedata_->resize_pending.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    if (small) {
#ifdef LOG_WAIT_TIME
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock],
                    hash_bucket_index / num_slots_to_lock, runtime_lock))
            return false;
        if (num_slots_to_lock - hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1],
                        hash_bucket_index / num_slots_to_lock + 1, runtime_lock)) {
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
                return false;
            }
        }
#else
        int64_t region = hash_bucket_index / kNumSlotsToLock;
        if (region < resizing_region.load(std::memory_order_acquire)) {
            if (!spin_lock_conditionally(&runtimedata_->locks[region], region, resizing_region, runtime_lock)) {
                return false;
            }
            if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
                if (!spin_lock_conditionally(&runtimedata_->locks[region + 1], 
                                             region, resizing_region, runtime_lock)) {
                    spin_unlock(&runtimedata_->locks[region]);
                    return false;
                }
            }
        } else {
            if (resizing_region.load(std::memory_order_acquire) == -1 || 
                GET_SECOND_TRY_LOCK(runtime_lock)) {
                if (!spin_lock(&runtimedata_->locks[region], runtime_lock))
                    return false;
                if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
                    if (!spin_lock(&runtimedata_->locks[region + 1], runtime_lock)) {
                        spin_unlock(&runtimedata_->locks[region]);
                        return false;
                    }
                }
            } else {
                return false;
            }
        }
#endif
    } else {
#ifdef LOG_WAIT_TIME
        if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size) {
            if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock], runtime_lock)) {
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock + 1], runtime_lock)) {
            spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock]);
            if (hash_bucket_index >= num_slots_to_lock && hash_bucket_lock_offset <= cluster_size)
                spin_unlock(&runtimedata->locks[hash_bucket_index / num_slots_to_lock - 1]);
            return false;
        }
#else
        if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize) {
            if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1], runtime_lock))
                return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock], runtime_lock)) {
            if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
            return false;
        }
        if (!spin_lock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1], runtime_lock)) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
            if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
                spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
            return false;
        }
#endif
    }
    return true;
}

inline void Quotient::zeno_unlock(uint64_t hash_bucket_index, bool small) {
    uint64_t hash_bucket_lock_offset  = hash_bucket_index % kNumSlotsToLock;
    if (small) {
        if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1]);
        }
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
    } else {
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock + 1]);
        spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock]);
        if (hash_bucket_index >= kNumSlotsToLock && hash_bucket_lock_offset <= kClusterSize)
            spin_unlock(&runtimedata_->locks[hash_bucket_index / kNumSlotsToLock - 1]);
    }
}

inline bool Quotient::zeno_lock_region(int64_t lock_region_index, uint8_t runtime_lock) {
    if (!spin_lock(&runtimedata_->locks[lock_region_index], runtime_lock)) {
        return false;
    }
    return true;
}

inline void Quotient::zeno_unlock_region(int64_t lock_region_index) {
    spin_unlock(&runtimedata_->locks[lock_region_index]);
}

inline void Quotient::zeno_unlock_range(int64_t start_region, int64_t end_region) {
    while (true) {
        zeno_unlock_region(end_region);
        if (end_region == start_region) break;
        --end_region;
    }
}

inline bool Quotient::zeno_lock_region_conditionally(int64_t lock_region_index, uint8_t runtime_lock) {
    // Read-lock of the spinlock array must be done before calling this method. 
    if (lock_region_index <= resizing_region.load(std::memory_order_acquire)) {
        if (!spin_lock_conditionally(&runtimedata_->locks[lock_region_index], lock_region_index, 
                                     resizing_region, runtime_lock)) {
            return false;
        }
    } else {
        if (resizing_region.load(std::memory_order_acquire) == -1 ||
            GET_SECOND_TRY_LOCK(runtime_lock)) {
            if (!spin_lock(&runtimedata_->locks[lock_region_index], runtime_lock)) {
                return false;
            }
        } else {
            return false;
        }
    }
    // No need for spin_unlocks because no region would have been locked in failure. 
    return true;
}

inline uint64_t Quotient::get_slot(const uint64_t& index) const {
    assert(index < metadata_->xnslots);
    /* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
     * to generate buggy code.  :/  */
    uint64_t* p = reinterpret_cast<uint64_t*>(&get_block(index / kSlotsPerBlock)->slots[(index %
                                                    kSlotsPerBlock) * metadata_->bits_per_slot / 8]);
    // you cannot just do *p to get the value, undefined behavior
    uint64_t pvalue;
    memcpy(&pvalue, p, sizeof(pvalue));
    return (pvalue >> (((index % kSlotsPerBlock) * metadata_->bits_per_slot) % 8)) &
                                BITMASK(metadata_->bits_per_slot);
}

inline void Quotient::set_slot(const uint64_t& index, const uint64_t& value) {
    assert(index < metadata_->xnslots);
    /* Should use __uint128_t to support up to 64-bit remainders, but gcc seems
     * to generate buggy code.  :/  */
    uint64_t* p = reinterpret_cast<uint64_t*>(&get_block(index / kSlotsPerBlock)->
                  slots[(index % kSlotsPerBlock) * metadata_->bits_per_slot / 8]);
    // This results in undefined behavior:
    // uint64_t t = *p;
    uint64_t t;
    memcpy(&t, p, sizeof(t));
    uint64_t mask = BITMASK(metadata_->bits_per_slot);
    uint64_t v = value;
    int32_t shift = ((index % kSlotsPerBlock) * metadata_->bits_per_slot) % 8;
    mask <<= shift;
    v <<= shift;
    t &= ~mask;
    t |= v;
    // This results in undefined behavior:
    // *p = t;
    memcpy(p, &t, sizeof(t));
}

inline uint64_t Quotient::block_offset(const uint64_t& blockidx) const {
	/* If we have extended counters and a 16-bit (or larger) offset field, then 
    we can safely ignore the possibility of overflowing that field. */
	if (sizeof(std::declval<qfblock>().offset) > 1 ||
        get_block(blockidx)->offset < BITMASK(8 * sizeof(std::declval<qfblock>().offset)))
        { return get_block(blockidx)->offset; }
	return run_end(kSlotsPerBlock * blockidx - 1) - kSlotsPerBlock * blockidx + 1;
}

inline uint64_t Quotient::run_end(const uint64_t& hash_bucket_index) const {
    uint64_t bucket_block_index = hash_bucket_index / kSlotsPerBlock;
	uint64_t bucket_intrablock_offset = hash_bucket_index % kSlotsPerBlock;
	uint64_t bucket_blocks_offset = block_offset(bucket_block_index);

    uint64_t bucket_intrablock_rank = bitrank(get_block(bucket_block_index)->occupieds[0],
                                                bucket_intrablock_offset);

	if (bucket_intrablock_rank == 0) {
        if (bucket_blocks_offset <= bucket_intrablock_offset)
        	return hash_bucket_index;
        else
        	return kSlotsPerBlock * bucket_block_index + bucket_blocks_offset - 1;
	}

	uint64_t runend_block_index  = bucket_block_index + bucket_blocks_offset / kSlotsPerBlock;
	uint64_t runend_ignore_bits = bucket_blocks_offset % kSlotsPerBlock;
	uint64_t runend_rank = bucket_intrablock_rank - 1;
    uint64_t runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                                        runend_ignore_bits, runend_rank);
	if (runend_block_offset == kSlotsPerBlock) {
        if (bucket_blocks_offset == 0 && bucket_intrablock_rank == 0) {
            /* The block begins in empty space, and this bucket is in that region of empty space */
            return hash_bucket_index;
        } else {
            do {
                runend_rank -= 
                        popcntv(get_block(runend_block_index)->runends[0], runend_ignore_bits);
                runend_block_index++;
                runend_ignore_bits = 0;
                runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                                 runend_ignore_bits, runend_rank);
            } while (runend_block_offset == kSlotsPerBlock);
        }
    }

    uint64_t runend_index = kSlotsPerBlock * runend_block_index + runend_block_offset;
    if (runend_index < hash_bucket_index)
        return hash_bucket_index;
    else
        return runend_index;
}

inline int Quotient::run_end_threadsafe(const uint64_t& hash_bucket_index, uint64_t& runend_index,
                                    int64_t& clusterend_region, uint8_t flags) {
    // This region is already locked. 
    int64_t current_locked_region = clusterend_region;
    // Set the new clusterend region as current region (default)
    uint64_t bucket_block_index = hash_bucket_index / kSlotsPerBlock;
	uint64_t bucket_intrablock_offset = hash_bucket_index % kSlotsPerBlock;
	uint64_t bucket_blocks_offset = block_offset(bucket_block_index);

    uint64_t bucket_intrablock_rank = bitrank(get_block(bucket_block_index)->occupieds[0],
                                                bucket_intrablock_offset);

	if (bucket_intrablock_rank == 0) {
        if (bucket_blocks_offset <= bucket_intrablock_offset) 
            runend_index = hash_bucket_index;
        else
        	runend_index =  kSlotsPerBlock * bucket_block_index + bucket_blocks_offset - 1;
        clusterend_region = current_locked_region;
        return 0;   // No err
	}

	uint64_t runend_block_index  = bucket_block_index + bucket_blocks_offset / kSlotsPerBlock;
	uint64_t runend_ignore_bits = bucket_blocks_offset % kSlotsPerBlock;
	uint64_t runend_rank = bucket_intrablock_rank - 1;

    // Overflowed to next qfblock. Is this a new region?
    int64_t last_region_to_lock = runend_block_index * kSlotsPerBlock / kNumSlotsToLock;
    if (last_region_to_lock > current_locked_region) {
        // if (!zeno_lock_region(last_region_to_lock, flags)) {
        if (!zeno_lock_region_conditionally(last_region_to_lock, flags)) {
            return kErrCouldntLock;
        }
        clusterend_region = last_region_to_lock;
        current_locked_region = last_region_to_lock;
    }
    
    uint64_t runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                                        runend_ignore_bits, runend_rank);
	if (runend_block_offset == kSlotsPerBlock) {
        if (bucket_blocks_offset == 0 && bucket_intrablock_rank == 0) {
            /* The block begins in empty space, and this bucket is in that
             * region of empty space */
            runend_index = hash_bucket_index;
            // clusterend_region was already updated..
            return 0;   // No err
        } else {
            do {
                runend_rank -= 
                        popcntv(get_block(runend_block_index)->runends[0], runend_ignore_bits);
                runend_block_index++;
                last_region_to_lock = runend_block_index * kSlotsPerBlock / kNumSlotsToLock;
                if (last_region_to_lock > current_locked_region) {
                    // if (!zeno_lock_region(last_region_to_lock, flags)) {
                    if (!zeno_lock_region_conditionally(last_region_to_lock, flags)) {
                        clusterend_region = current_locked_region;
                        return kErrCouldntLock;
                    }
                    current_locked_region = last_region_to_lock;
                }
                runend_ignore_bits = 0;
                runend_block_offset = bitselectv(get_block(runend_block_index)->runends[0],
                                                 runend_ignore_bits, runend_rank);
            } while (runend_block_offset == kSlotsPerBlock);
        }
    }

    clusterend_region = current_locked_region;

    runend_index = kSlotsPerBlock * runend_block_index + runend_block_offset;
    if (runend_index < hash_bucket_index) runend_index = hash_bucket_index;

    return 0;
}

inline int64_t Quotient::cluster_end(const uint64_t hash_bucket_index, const uint64_t padding) { 
    // Acquire the runend index of the given hash_bucket_index. 
    uint64_t runend_index = run_end(hash_bucket_index + padding);

    // Acquire the runend index of the runend index acquired above. 
    uint64_t current_runend_index = runend_index;
    uint64_t new_runend_index = current_runend_index;
    new_runend_index = run_end(current_runend_index);

    // Repeat acquiring the runend index of the runend index until reaching the end of the cluster.
    while (new_runend_index > current_runend_index) {
        current_runend_index = new_runend_index;
        new_runend_index = run_end(current_runend_index);
    }
    return new_runend_index / kNumSlotsToLock;
}

inline int Quotient::cluster_end_threadsafe(const uint64_t& hash_bucket_index, uint64_t& runend_index, 
                                        int64_t& clusterend_region, uint64_t& clusterend_index, 
                                        uint8_t flags) { 
    // Acquire the runend index of the given hash_bucket_index. 
    int res = run_end_threadsafe(hash_bucket_index, runend_index, clusterend_region, flags);
    if (res == kErrCouldntLock) return res;

    // Acquire the runend index of the runend index acquired above. 
    uint64_t current_runend_index = runend_index;
    uint64_t new_runend_index = current_runend_index;
    res = run_end_threadsafe(current_runend_index, new_runend_index, clusterend_region, flags);
    if (res == kErrCouldntLock) return res;

    // Repeat acquiring the runend index of the runend index until reaching the end of the cluster.
    while (new_runend_index > current_runend_index) {
        current_runend_index = new_runend_index;
        res = run_end_threadsafe(current_runend_index, new_runend_index, clusterend_region, flags);
        if (res == kErrCouldntLock) return res;
    }
    clusterend_index = new_runend_index;
    return 0;
}

inline int Quotient::zeno_lock_cluster(uint64_t index, uint64_t& runend, int64_t& clusterend_region, 
                                   uint8_t flags) {
    uint64_t clusterend = 0;
    uint64_t start_region = clusterend_region;
    
    // Required: clusterend_region is already locked. 
    // This finds the runend of the given index. 
    if (cluster_end_threadsafe(index, runend, clusterend_region, clusterend, flags)
        == kErrCouldntLock) {
        zeno_unlock_range(start_region, clusterend_region);
        return kErrCouldntLock;
    }

    if (resizing_region.load(std::memory_order_acquire) >= 0 
        && GET_SECOND_TRY_LOCK(flags) != kSecondTryLock 
        && clusterend_region - start_region >= 2) {
        zeno_unlock_range(start_region, clusterend_region);
        return kErrCouldntLock;
    }

    uint64_t clusterstart = clusterend + 1;
    uint64_t dummy_index = 0;   // We don't want to modify runend_index we found earlier
    
    // Now lock any subsequent regions that may have to be locked
    while (is_occupied(clusterstart)) {
        if (clusterstart / kNumSlotsToLock > clusterend_region) {
            ++clusterend_region;
            if (!zeno_lock_region_conditionally(clusterend_region, flags)) {
                zeno_unlock_range(start_region, clusterend_region);
                return kErrCouldntLock;
            }
        }

        if (cluster_end_threadsafe(clusterstart, dummy_index, clusterend_region, clusterend, flags)
            == kErrCouldntLock) {
            zeno_unlock_range(start_region, clusterend_region);
            return kErrCouldntLock;
        }
        clusterstart = clusterend + 1;

        if (resizing_region.load(std::memory_order_acquire) >= 0 
            && GET_SECOND_TRY_LOCK(flags) != kSecondTryLock 
            && clusterend_region - start_region >= 2) {
            zeno_unlock_range(start_region, clusterend_region);
            return kErrCouldntLock;
        }
    }

    if (clusterstart / kNumSlotsToLock > clusterend_region) {
        ++clusterend_region;
        if (!zeno_lock_region_conditionally(clusterend_region, flags)) {
            zeno_unlock_range(start_region, clusterend_region);
            return kErrCouldntLock;
        }
    }
    if (resizing_region.load(std::memory_order_acquire) >= 0 
        && GET_SECOND_TRY_LOCK(flags) != kSecondTryLock 
        && clusterend_region - start_region >= 2) {
        zeno_unlock_range(start_region, clusterend_region);
        return kErrCouldntLock;
    }
    return 0;
}

inline int32_t Quotient::offset_lower_bound(const uint64_t& slot_index) const {
    const qfblock* b = get_block(slot_index / kSlotsPerBlock);
    const uint64_t slot_offset = slot_index % kSlotsPerBlock;
    const uint64_t boffset = b->offset;
    const uint64_t occupieds = b->occupieds[0] & BITMASK(slot_offset + 1);
    assert(kSlotsPerBlock == 64);
    if (boffset <= slot_offset) {
        const uint64_t runends = (b->runends[0] & BITMASK(slot_offset)) >> boffset;
        return __builtin_popcountll(occupieds) - __builtin_popcountll(runends);
    }
    return boffset - slot_offset + __builtin_popcountll(occupieds);
}

inline uint64_t Quotient::find_first_empty_slot(uint64_t from) const {
    do {
        int32_t t = offset_lower_bound(from);
        assert(t >= 0);
        if (t == 0)
            break;
        from = from + t;
    } while(1);
    return from;
}

inline void Quotient::shift_remainders(const uint64_t& start_index, const uint64_t& empty_index) {
	uint64_t last_word = (empty_index + 1) * metadata_->bits_per_slot / 64;
	const uint64_t first_word = start_index * metadata_->bits_per_slot / 64;
	int bend = ((empty_index + 1) * metadata_->bits_per_slot) % 64;
	const int bstart = (start_index * metadata_->bits_per_slot) % 64;

    assert(first_word <= last_word);
	while (last_word != first_word) {
        *REMAINDER_WORD(last_word) = shift_into_b(*REMAINDER_WORD(last_word - 1),
                                                  *REMAINDER_WORD(last_word),
                                                  0, bend, metadata_->bits_per_slot);
        --last_word;
        bend = 64;
	}
    *REMAINDER_WORD(last_word) = shift_into_b(0, *REMAINDER_WORD(last_word),
                                              bstart, bend, metadata_->bits_per_slot);
}

inline void Quotient::shift_slots(int64_t first, uint64_t last, uint64_t distance) {
    if (distance == 1) {
        shift_remainders(first, last + 1);
    } else {
        for (int64_t i = last; i >= first; i--)
            set_slot(i + distance, get_slot(i));
    }
}


inline void Quotient::shift_runends(int64_t first, uint64_t last, uint64_t distance) {
    assert(last < metadata_->xnslots && distance < 64);
    uint64_t first_word = first / 64;
    uint64_t bstart = first % 64;
    uint64_t last_word = (last + distance + 1) / 64;
    uint64_t bend = (last + distance + 1) % 64;

    if (last_word != first_word) {
        // The code in the original Quotient implementation had a weird issue with overwriting parts of 
        // the bitmap that it shouldn't have touched. The issue came up when `distance > 1`, and is 
        // fixed now.
        const uint64_t first_runends_replacement = METADATA_WORD(runends, first) & (~BITMASK(bstart));
        METADATA_WORD(runends, 64*last_word) = 
            shift_into_b(last_word == first_word + 1 ? first_runends_replacement : 
                         METADATA_WORD(runends, 64 * (last_word - 1)), 
                         METADATA_WORD(runends, 64 * last_word), 0, bend, distance);
        bend = 64;
        --last_word;
        while (last_word != first_word) {
            METADATA_WORD(runends, 64 * last_word) = 
                shift_into_b(last_word == first_word + 1 ? first_runends_replacement : 
                             METADATA_WORD(runends, 64 * (last_word - 1)),
                             METADATA_WORD(runends, 64 * last_word), 0, bend, distance);
            --last_word;
        }
    }
    METADATA_WORD(runends, 64 * last_word) = shift_into_b(0LL, 
                                                          METADATA_WORD(runends, 64 * last_word),
                                                          bstart, bend, distance);
}

inline
bool Quotient::shift_for_inserts(int operation, 
                           uint64_t slot_index, 
                           uint64_t overwrite_index,
                           const uint64_t* remainders, 
                           uint64_t total_remainders, 
                           uint64_t noverwrites) {
    uint64_t empties[67] = {0,};
    uint64_t i;
    int64_t j;
    int64_t ninserts = total_remainders - noverwrites;
    uint64_t insert_index = overwrite_index + noverwrites;

    if (ninserts > 0) {
        /* First, shift things to create n empty spaces where we need them. */
        find_next_n_empty_slots(insert_index, ninserts, empties);
        if (empties[0] >= metadata_->xnslots) {
            return false;
        }
        uint64_t last_index = empties[ninserts - 1] ? empties[ninserts - 1] - 1 : 0;
        for (j = 0; j < ninserts - 1; j++)
            shift_slots(empties[j+1] + 1, empties[j] - 1, j + 1);
        shift_slots(insert_index, last_index, ninserts);

        for (j = 0; j < ninserts - 1; j++)
            shift_runends(empties[j+1] + 1, empties[j] - 1, j + 1);
        shift_runends(insert_index, last_index, ninserts);

        for (i = noverwrites; i < total_remainders - 1; i++)
            METADATA_WORD(runends, overwrite_index + i) &= 
                    ~(1ULL << (((overwrite_index + i) % kSlotsPerBlock) % 64));

        switch (operation) {
            case 0: /* insert into empty bucket */
                assert (noverwrites == 0);
                METADATA_WORD(runends, overwrite_index + total_remainders - 1) |=
                    1ULL << (((overwrite_index + total_remainders - 1) % kSlotsPerBlock) % 64);
                break;
            case 1: /* append to bucket */
                METADATA_WORD(runends, overwrite_index + noverwrites - 1)      &=
                    ~(1ULL << (((overwrite_index + noverwrites - 1) % kSlotsPerBlock) % 64));
                METADATA_WORD(runends, overwrite_index + total_remainders - 1) |=
                    1ULL << (((overwrite_index + total_remainders - 1) % kSlotsPerBlock) % 64);
                break;
            case 2: /* insert into bucket */
                METADATA_WORD(runends, overwrite_index + total_remainders - 1) &=
                    ~(1ULL << (((overwrite_index + total_remainders - 1) % kSlotsPerBlock) % 64));
                break;
            default:
                std::cerr << "Invalid operation " << operation << std::endl;
                abort();
        }

        uint64_t npreceding_empties = 0;
        for (i = slot_index / kSlotsPerBlock + 1; i <= empties[0]/kSlotsPerBlock; i++) {
            while ((int64_t)npreceding_empties < ninserts &&
                         empties[ninserts - 1 - npreceding_empties]  / kSlotsPerBlock < i)
                npreceding_empties++;

            if (get_block(i)->offset + ninserts - npreceding_empties < BITMASK(8*sizeof(std::declval<qfblock>().offset)))
                get_block(i)->offset += ninserts - npreceding_empties;
            else
                get_block(i)->offset = (uint8_t) BITMASK(8*sizeof(std::declval<qfblock>().offset));
        }
    }

    for (i = 0; i < total_remainders; i++) {
        set_slot(overwrite_index + i, remainders[i]); 
    }

    // modify_metadata(&metadata_->noccupied_slots, ninserts);
    modify_metadata(metadata_->pc_noccupied_slots, ninserts);

    return true;
}

inline
int Quotient::shift_for_deletes(int operation, 
                           uint64_t bucket_index, 
                           uint64_t overwrite_index,
                           const uint64_t* remainders, 
                           uint64_t total_remainders, 
                           uint64_t old_length) {
    uint64_t i;

    // Update the slots
    for (i = 0; i < total_remainders; i++)
        set_slot(overwrite_index + i, remainders[i]);

    // If this is the last thing in its run, then we may need to set a new runend bit
    if (is_runend(overwrite_index + old_length - 1)) {
      if (total_remainders > 0) { 
        // If we're not deleting this entry entirely, then it will still the last entry in this run
        METADATA_WORD(runends, overwrite_index + total_remainders - 1) |= 1ULL << ((overwrite_index + total_remainders - 1) % 64);
      } else if (overwrite_index > bucket_index &&
             !is_runend(overwrite_index - 1)) {
        // If we're deleting this entry entirely, but it is not the first entry in this run,
        // then set the preceding entry to be the runend
        METADATA_WORD(runends, overwrite_index - 1) |= 1ULL << ((overwrite_index - 1) % 64);
      }
    }

    // shift slots back one run at a time
    uint64_t original_bucket = bucket_index;
    uint64_t current_bucket = bucket_index;
    uint64_t current_slot = overwrite_index + total_remainders;
    uint64_t current_distance = old_length - total_remainders;
    int ret_current_distance = current_distance;

    while (current_distance > 0) {
        if (is_runend(current_slot + current_distance - 1)) {
            do {
                current_bucket++;
            } while (current_bucket < current_slot + current_distance &&
                             !is_occupied(current_bucket));
        }

        if (current_bucket <= current_slot) {
            set_slot(current_slot, get_slot(current_slot + current_distance));
            if (is_runend(current_slot) != is_runend(current_slot + current_distance))
                METADATA_WORD(runends, current_slot) ^= 1ULL << (current_slot % 64);
            current_slot++;

        } else if (current_bucket <= current_slot + current_distance) {
            uint64_t i;
            for (i = current_slot; i < current_slot + current_distance; i++) {
                set_slot(i, 0);
                METADATA_WORD(runends, i) &= ~(1ULL << (i % 64));
            }

            current_distance = current_slot + current_distance - current_bucket;
            current_slot = current_bucket;
        } else {
            current_distance = 0;
        }
    }
    
    // reset the occupied bit of the hash bucket index if the hash is the
    // only item in the run and is removed completely.
    if (operation && !total_remainders)
        METADATA_WORD(occupieds, bucket_index) &= ~(1ULL << (bucket_index % 64));

    // update the offset bits.
    // find the number of occupied slots in the original_bucket block.
    // Then find the runend slot corresponding to the last run in the
    // original_bucket block.
    // Update the offset of the block to which it belongs.
    uint64_t original_block = original_bucket / kSlotsPerBlock;
    if (old_length > total_remainders) {
        const int64_t last_slot_in_initial_cluster = current_slot;
        while (original_block < last_slot_in_initial_cluster / kSlotsPerBlock) {
			uint64_t last_occupieds_hash_index = kSlotsPerBlock * original_block + (kSlotsPerBlock - 1);
			uint64_t runend_index = run_end(last_occupieds_hash_index);
			// runend spans across the block
			// update the offset of the next block
			if (runend_index / kSlotsPerBlock == original_block) { // if the run ends in the same block
				get_block(original_block + 1)->offset = 0;
			} else { // if the last run spans across the block
                const uint32_t max_offset = (uint32_t) BITMASK(8*sizeof(std::declval<qfblock>().offset));
                const uint32_t new_offset = runend_index - last_occupieds_hash_index;
				get_block(original_block + 1)->offset = new_offset < max_offset ? new_offset : max_offset;
			}
			original_block++;
		}
    }

    int num_slots_freed = old_length - total_remainders;
    // modify_metadata(&metadata_->noccupied_slots, - num_slots_freed);
    modify_metadata(metadata_->pc_noccupied_slots, - num_slots_freed);
    /*qf->metadata->noccupied_slots -= (old_length - total_remainders);*/
    if (!total_remainders) {
        // modify_metadata(&metadata_->ndistinct_elts, -1);
        /*qf->metadata->ndistinct_elts--;*/
    }

    return ret_current_distance;
}

inline
uint64_t* Quotient::delete_run(uint64_t canonical_slot, uint64_t& run_length) {
    uint64_t runstart_index = canonical_slot == 0 
                            ? 0 
                            : run_end(canonical_slot - 1) + 1;
    uint64_t runend_index = run_end(canonical_slot);
    run_length = runend_index - runstart_index + 1;
    uint64_t* buffer = new uint64_t[run_length]{};
    for (uint64_t i = runstart_index; i <= runend_index; ++i) {
        buffer[i - runstart_index] = get_slot(i);
    }
    uint64_t numfreedslots = shift_for_deletes(1 /* only item in the run*/,
                                               canonical_slot,
                                               runstart_index,
                                               nullptr,
                                               0,
                                               run_length);
    return buffer;
}

inline
uint64_t Quotient::insert_run(uint64_t canonical_slot, uint64_t* buffer, uint64_t run_length) {
    uint64_t runstart_index = canonical_slot == 0 
                            ? 0 
                            : run_end(canonical_slot - 1) + 1;
    int operation = 0;  /* Insert to empty slot */
    if (is_occupied(canonical_slot)) {
        operation = 1;  /* Append to run */
        runstart_index = run_end(canonical_slot) + 1;
    }
    bool ret = shift_for_inserts(operation, 
                                 canonical_slot,
                                 runstart_index,
                                 buffer,
                                 run_length,
                                 0);
    METADATA_WORD(occupieds, canonical_slot) |= 1ULL << (canonical_slot % 64);
    if (!ret) return kErrNoSpace;
    return ret;
}

inline
uint64_t Quotient::count_key_value(uint64_t key, uint64_t value, uint8_t flags) const {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
    }
    // Defer checking value to the do-while loop
    uint64_t hash = (key << metadata_->value_bits) | (value & BITMASK(metadata_->value_bits));
    uint64_t hash_remainder   = key & BITMASK(metadata_->fingerprint_bits);
    int64_t hash_bucket_index = key >> metadata_->fingerprint_bits;

    if (!is_occupied(hash_bucket_index))
        return 0;

    int64_t runstart_index = (hash_bucket_index == 0) ? 0 : run_end(hash_bucket_index-1) + 1;
    if (runstart_index < hash_bucket_index)
        runstart_index = hash_bucket_index;

    uint64_t current_remainder, current_count, current_end;
    do {
        current_end = decode_counter(runstart_index, current_remainder, current_count);
        if (current_remainder == hash_remainder)
            return current_count;
        runstart_index = current_end + 1;
    } while (!is_runend(current_end));

    return 0;
}

uint64_t* Quotient::encode_counter(uint64_t remainder, uint64_t counter, uint64_t* slots) {
    uint64_t digit = remainder;
    uint64_t base = (1ULL << metadata_->bits_per_slot) - 1;
    if (remainder == base) base -= 1;   /* if encoding for A (11..1), disallow counter to be B */
    uint64_t *p = slots;

    if (counter == 0)
        return p;

    *--p = remainder;

    if (counter == 1)
        return p;

    if (counter == 2) {
        *--p = remainder;
        return p;
    }

    if (counter == 3 && remainder == 0) {
        *--p = remainder;
        *--p = remainder;
        return p;    
    }

    if (counter == 3 && remainder > 0) {
        *--p = 0;
        *--p = remainder;
        return p;
    }

    if (remainder == 0)
        *--p = remainder;
    else 
        base--;

    if (remainder)
        counter -= 3;
    else
        counter -= 4;
    do {
        digit = counter % base;
        digit++; /* Zero not allowed */
        if (remainder && digit >= remainder)
            digit++; /* Cannot overflow since digit is mod 2^r-2 */
        *--p = digit;
        counter /= base;
    } while (counter);

    if (remainder && digit >= remainder)
        *--p = 0;

    *--p = remainder;

    return p;
}

uint64_t Quotient::decode_counter(uint64_t index, uint64_t& remainder, uint64_t& count) const {
    uint64_t base;
    uint64_t rem;
    uint64_t cnt;
    uint64_t digit;
    uint64_t end;

    remainder = rem = get_slot(index);

    if (is_runend(index)) { /* Entire run is "0" */
        count = 1; 
        return index;
    }

    digit = get_slot(index + 1);

    if (is_runend(index + 1)) {
        count = digit == rem ? 2 : 1;
        return index + (digit == rem ? 1 : 0);
    }

    if (rem > 0 && digit >= rem) {
        count = digit == rem ? 2 : 1;
        return index + (digit == rem ? 1 : 0);
    }

    if (rem > 0 && digit == 0 && get_slot(index + 2) == rem) {
        count = 3;
        return index + 2;
    }

    if (rem == 0 && digit == 0) {
        if (get_slot(index + 2) == 0) {
            count = 3;
            return index + 2;
        } else {
            count = 2;
            return index + 1;
        }
    }

    cnt = 0;
    base = (1ULL << metadata_->bits_per_slot) - (rem ? 2 : 1);

    /**
     * If rem > 0, base = B = 11...10. If rem = A, the counter cannot be B (we disallow it at 
     * encode_counter). If the encountered digit at index + 1 is a B (digit == base), this should
     * not be decoded as an escape sequence but should be decoded as an A with count = 1. 
     */
    if (rem == (base + 1)) {
        if (digit == base) {
            count = 1;
            return index;
        }
        base -= 1; /* decrement base (disallow counter to be B) */
    }


    end = index + 1;
    while (digit != rem && !is_runend(end)) {
        if (digit > rem)
            digit--;
        if (digit && rem)
            digit--;
        cnt = cnt * base + digit;

        end++;
        digit = get_slot(end);
    }

    if (rem) {
        count = cnt + 3;
        return end;
    }

    if (is_runend(end) || get_slot(end + 1) != 0) {
        count = 1;
        return index;
    }

    count = cnt + 4;
    return end + 1;
}

inline
void Quotient::debug_dump_block() const {
    qfblock* qb = nullptr;
    for (uint64_t i = 0; i < metadata_->nblocks; ++i) {
        const uint64_t block_ind = i;
        qb = get_block(block_ind);
        std::cout << "============================= block " << i << " offset=" << +qb->offset << std::endl;
        for (uint32_t j = 0; j < kSlotsPerBlock; j++) {
            const uint64_t ind = block_ind + j;
            if (ind >= metadata_->xnslots)
                break;
            const uint64_t slot = get_slot(ind);
            std::cout << '@' << ind << '(' << j << "):" << is_occupied(ind) << ',' << is_runend(ind) << ',';
            for (int32_t k = metadata_->bits_per_slot - 1; k >= 0; k--)
                std::cout << ((slot >> k) & 1);
            std::cout << ' ';
        }
        std::cout << std::endl << std::endl;
    }
}

}   // namespace zeno