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
#include <immintrin.h>

#include "decls.hpp"
#include "hash.hpp"
#include "pc.hpp"
#include "rs.hpp"
#include "iterator.hpp"

namespace zeno {

class ZenoFilter {
    public:
    explicit ZenoFilter(uint64_t exp_size, uint64_t fp_bits, uint64_t value_bits, 
                        uint64_t expansion_coefficient, hashmode hash_mode, uint32_t seed, 
                        double threshold);
    ~ZenoFilter();
    ZenoFilter(const ZenoFilter& zeno) = delete;
    ZenoFilter& operator=(const ZenoFilter& zeno) = delete;
    ZenoFilter& operator=(ZenoFilter&& other) noexcept;

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
     * Increases the capacity of the underlying memory. 
     */

    /**
     * Increases the capacity of the underlying memory. The parameters are for a potential 
     * 'dangling' hash; the hash that caused expansion will not have been inserted to the larger 
     * filter. This function handles that case. 
     * @param new_hash The 'dangling' hash to be inserted in the larger filter
     * @param new_count The count of `new_hash`
     * @param flags The original flag used for insertion. 
     * @returns The number of reallocated entries. <= 0 if error. 
     */
    int64_t grow(uint64_t dangling_hash, uint64_t dangling_count, uint8_t flags);

    /**
     * Decreases the capacity of the underlying memory. The parameters are for a potential 
     * 'dangling' hash; the hash that caused expansion will not have been inserted to the larger 
     * filter. This function handles that case. 
     * @param flags The original flag used for insertion. 
     * @returns The number of reallocated entries. <= 0 if error. 
     */
    int64_t contract();

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
    uint64_t get_fp_bits() const {
        return metadata_->hash_bits;
    }
    uint64_t get_memory_usage() const {
        return metadata_->total_memory_usage + index_block_.capacity() * sizeof(qfblock*);
    }
    double get_space_amplification() const {
        return metadata_->space_amplification_;
    }
    uint64_t get_num_slots() const {
        return metadata_->nslots;
    }

    // Checks whether the filter is expanding. 
    bool is_filter_growing() const {
        return runtimedata_->resizing_region.load(std::memory_order_acquire) >= 0;
    }

    /**
     * Returns the accumulated time the filter spent on expansion in microseconds. 
     */
    uint64_t get_grow_time() const {
        return runtimedata_->total_grow_time;
    }

    inline
    uint64_t get_index_block_size() const {
        return index_block_.size();
    }

    void print_by_index(uint64_t index) const {
        print_from_index(index);
    }
    
    uint64_t calculate_nonempty_slots() const {
        uint64_t running_cluster_length = 0;
        uint64_t current_runend_index = 0;
        uint64_t current_index = 0;

        uint64_t nonempty_slot = 0;
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
            nonempty_slot += running_cluster_length;
            running_cluster_length = 0;
        }
        return nonempty_slot;
    }

    private:
    friend class iterator<ZenoFilter>;
    /** 
     * Class for computing fixed point operations. The results are always automatically converted to 
     * uint64_t. 
     */
#if defined(FIXED)
    class FixedPoint {
        public:
        static constexpr int kFractionalBits = 20;
        static constexpr uint64_t kScalingFactor = 1ULL << kFractionalBits;
        static constexpr uint64_t kBitMask = BITMASK(kFractionalBits);

        FixedPoint() = default;
        FixedPoint(double d) :
            value(static_cast<uint64_t>(d * kScalingFactor)),
            reciprocal(static_cast<uint64_t>((1./d) * kScalingFactor)) {}
        
        FixedPoint operator*(const double& other) const {
            return FixedPoint(to_double() * other);
        }
        
        FixedPoint operator*(const FixedPoint& other) const {
            return FixedPoint(to_double() * other.to_double());
        }

        FixedPoint& operator*=(const double& other) {
            FixedPoint fp = FixedPoint(to_double() * other);
            value = fp.value;
            reciprocal = fp.reciprocal;

            return *this;
        }
        
        uint64_t operator*(const uint64_t& other) const {
            return (value * other) >> kFractionalBits;  // floor
        }

        uint64_t operator/(const uint64_t& other) const {
            return (reciprocal * other) >> kFractionalBits;
        }

        friend uint64_t operator/(uint64_t lhs, const FixedPoint& rhs) {
            return (lhs * rhs.reciprocal + rhs.kBitMask) >> rhs.kFractionalBits;
        }

        uint64_t get_raw_value() const { return value; }
        double to_double() const { 
            return static_cast<double>(value) / kScalingFactor; 
        }

        private:
        uint64_t value;         // used for multiplication
        uint64_t reciprocal;    // used for division
    };  // class FixedPoint
#endif

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

    // INCREMENTAL
    static constexpr uint64_t kNumSlotsToLock = 1ULL << 12;
    static constexpr uint64_t kClusterSize = 1ULL << 11;

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

    struct qfruntime {
        uint64_t num_locks;
        spinlock_padded* locks;
        // Locks required for reallocating spinlocks
        std::atomic<bool> resize_pending{false};
        std::shared_mutex spinlock_mutex;
        // Locks for announcing expansion and location of the iterator
        std::atomic<int64_t> resizing_region{-1};
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
#if defined(WIDENING)
        // total number of expansions this filter has gone through, used for calculating the number
        // of slots in the widening regime. 
        uint64_t num_expansions;
#endif
        /**
         * MOD: metadata for resizable array
         * The exponent of the RA size of this period. The initial RA size is (1 << exp_ra_size_) 
         * and the RA size of the next epoch is (1 << exp_ra_size_) * 2^{1/r}. The value of 
         * `exp_ra_size_` increases by 1 on each period. 
         */
        uint64_t exp_ra_size_; 
        /* The reciprocal ratio of filter expansion. */
        uint64_t reciprocal_ratio_; 
        /**
         * By how much the filter expans on every epoch. When `current_epoch` rolls back to 0, the 
         * filter size becomes the next power of 2 regardless of the multiplicative result. 
         */
        double expansion_ratio_; 
        /* Space amplification after each expansion */
        double space_amplification_;
        double multiplicative_ratio_;
        /**
         * The exponent of the size of a single slot of the datablock. Given an index, a lookup 
         * slices the lower `exp_db_size_` bits from the index for internal lookup. 
         */
        uint64_t exp_db_size_; // = QF_BLOCK_OFFSET_BITS + EXP_NUM_QF_PER_UNIT
        /* Current epoch of expansion. Rolls back to 0 when it reaches the reciprocal ratio. */
        uint64_t current_epoch_;
        /*  Data of current location */
        uint64_t current_superblock_index_;
        /* Number of appended datablocks in this superblock */
        uint64_t current_appended_datablocks_;
        /**
         * Data of appended units. The number of data blocks in each superblock and the size of each 
         * data block, respectively. 
         */
        uint64_t appended_datablock_num_;
        uint64_t appended_datablock_size_;
        /**
         * Total number of entries in the array. Intentionally defined as a double, because if 
         * `expansion_ratio_` is too small, casting the multiplicative result against 
         * `expansion_ratio_` to uint64_t might lose the values under the decimal. 
         */
        double current_size_; // = 0
        /* Is the current epoch a power of 2? */
        bool is_period_;
    };  // struct qfmetadata

    struct ZeroBufferEntry {
        uint64_t hash;
        uint64_t count;
        uint64_t value;

        ZeroBufferEntry(uint64_t h, uint64_t c, uint64_t v)
        : hash(h), count(c), value(v) {}
    };  // struct ZeroBufferEntry

    /* Actual contents of the filter. */
    qfruntime*  runtimedata_;
    qfmetadata* metadata_;
    qfblock*    blocks_;

    /* The index block that serves as a directory for data blocks. */
    std::vector<qfblock*> index_block_;

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

#if defined(WIDENING)
    /**
     * Sets the number of expansions for this filter instance. Called on expansion.
     * @param num The number of expansions of this filter. 
     */
    inline void set_num_expansions(uint64_t num) {
        metadata_->num_expansions = num;
    }
#endif

    // Resizable array modification functions

    /**
     * Given a superblock index, calculates the number of preceding data blocks. 
     * @param k The superblock index. 
     * @returns The number of data blocks preceding the superblock. 
     */
    uint64_t calculate_A(const uint64_t& k) const;

    struct indexABC {
        uint64_t superblock_index;
        uint64_t datablock_index;
        uint64_t element_index;

        indexABC(uint64_t a, uint64_t b, uint64_t c) :
            superblock_index(a), datablock_index(b), element_index(c)
        {}

        void print() const {
            std::cout << "superblock index : " << superblock_index << " , " << \
                         "datablock index : " << datablock_index << " , " << \
                         "element index : " << element_index << std::endl;
        }
    };

    /**
     * Given an index, calculates the superblock index, data block index, and the element index 
     * within the data block. 
     * @note The function assumes the input `index` is an index that is at the granularity of an 
     * unit of the resizable array. The caller must adjust the `index` accordingly. 
     * @param index The index that we wish to calculate the additional index of.
     * @returns indexABC that holds the relevant index information. 
     */
    const indexABC entry_lookup_internal(const uint64_t& index) const;

    /**
     * Calculates the smallest power of 2 greater than `value`. 
     * @param value The number for which the next power of 2 will be computed.
     * @returns The next power of 2 if `value` is not already a power of 2. If `value` is already a 
     * power of 2, returns `value`. 
     */
    uint64_t calculateNextPowerOf2(const uint64_t& value) const {
        return (1ULL << (64ULL - __builtin_clzll(value - 1ULL)));
    }

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
        uint64_t datablock_index = (block_index >> kUnitOffsetBits) + 1;
                
        uint64_t k = 63ULL - __builtin_clzll(datablock_index);
        uint64_t floor_k = (k >> 1), mask_up = (1ULL << floor_k) - 1;
        uint64_t ceil_k = ((k + 1) >> 1), mask_low = (1ULL << ceil_k) - 1;
        uint64_t off = ((datablock_index & mask_low) << kUnitOffsetBits) + 
                    (block_index & BITMASK(kUnitOffsetBits));

        qfblock* qb_ptr = index_block_[calculate_A(k) + ((datablock_index >> ceil_k) & mask_up)];
        qfblock* qb = (qfblock*)(((char*)qb_ptr) + off * 
                                 (sizeof(qfblock) + kSlotsPerBlock * metadata_->bits_per_slot / 8));

        return qb;      
    }

    void set_slot(const uint64_t& index, const uint64_t& value);
    uint64_t get_slot(const uint64_t& index) const;

    uint64_t run_end(const uint64_t& hash_bucket_index) const;
    uint64_t run_end2(const uint64_t& hash_bucket_index) const;
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
     * Moves the lower-most bit of the quotient to the upper-most bit of the fingerprint.
     * @note This method requires the fingerprint to have been stripped off of the associated value. 
     * It is why the width calculation is done with`fingerprint_bits` instead of `bits_per_slot`. 
     * @param q The quotient part of the hashed representation of a key.
     * @param f The fingerprint of the hashed representation of a key.
     */
    inline
    uint64_t extend_fingerprint_length(uint64_t& q, uint64_t& f) const {
        int unary_count = __builtin_clzll((~f) << (64 - metadata_->fingerprint_bits)) + 1;
        uint64_t old_fp_len = metadata_->fingerprint_bits - unary_count;
        uint64_t parity = q & 1;
        uint64_t unary = (BITMASK(unary_count - 2) << 2) | parity;
        f = (unary << old_fp_len) | (f & BITMASK(old_fp_len));
        q >>= 1;

        return (q << metadata_->fingerprint_bits) | f;
    }

#if defined(WIDENING)
    /**
     * Moves the upper-most bit of the fingerprint after the unary counter to the lower-most bit of 
     * the quotient. This increases the unary counter of the fingerprint. For example, if the 
     * quotient was 1010 and the fingerprint was 110011, the upper-most bit of the fingerprint (0) 
     * will be appended to the quotient, making the quotient 10100 and the fingerprint 111011. This 
     * method modifies the quotient and fingerprint values directly but also returns the resulting 
     * appended hash for convenience. 
     * @note - This method behaves differently between InfiniFilter and Zeno. Under InfiniFilter, 
     * 
     * @note - This method requires the value to have been removed from the associated fingerprint. 
     * It is why the width calculation is done with`fingerprint_bits` instead of `bits_per_slot`. 
     * @param q The quotient part of the hashed representation of a key.
     * @param f The fingerprint of the hashed representation of a key.
     * @param len (InfiniFilter) The new fingerprint length under the widening regime. (Zeno) The
     * old fingerprint length under the widening regime. 
     */
    inline
    uint64_t adjust_fingerprint_length_widening(uint64_t& q, uint64_t& f, uint64_t len) const {
        int unary_count = __builtin_clzll((~f) << (64 - len)) + 1;
        uint64_t new_fp_len = len - unary_count - 1;
        uint64_t parity = (f >> new_fp_len) & 1;

        uint64_t unary = BITMASK(unary_count + (metadata_->fingerprint_bits - len));
        f = (f & BITMASK(new_fp_len)) | (unary << (new_fp_len + 1));
        q = (q << 1) | parity;

        return (q << metadata_->fingerprint_bits) | f;
    }
#endif

    /**
     * Adjusts the canonical slot according to the current epoch.
     * @param index The hash of a key. This index in this hash is multiplied to
     * the corresponding index in the current epoch.
     */
    inline
    void sanitize_hash(uint64_t& index) const {
        uint64_t canonical_slot = 
                    static_cast<uint64_t>(metadata_->multiplicative_ratio_ * 
                    (index >> metadata_->bits_per_slot));
        uint64_t remainder = index & BITMASK(metadata_->bits_per_slot);
        index = canonical_slot << metadata_->bits_per_slot | remainder;
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
};  // class ZenoFilter

ZenoFilter::ZenoFilter(uint64_t exp_size, uint64_t fp_bits, uint64_t value_bits, 
                       uint64_t expansion_coefficient, hashmode hash_mode, uint32_t seed, 
                       double threshold = 0.8) {
    uint64_t num_slots, xnslots, nblocks;
    uint64_t fingerprint_bits, bits_per_slot;
    uint64_t buffer_size = 0;
    uint64_t qfblock_size = 0;
    uint64_t total_num_bytes;

    num_slots = 1ULL << exp_size;
    xnslots = num_slots + 10*sqrt((double)num_slots);
    nblocks = (xnslots + kSlotsPerBlock - 1) / kSlotsPerBlock;
    fingerprint_bits = fp_bits;
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
    metadata_ = new qfmetadata;
    total_num_bytes = sizeof(qfmetadata);
    blocks_ = nullptr;

    metadata_->total_memory_usage = total_num_bytes;

    metadata_->magic_endian_number = kMagicNumber;
    metadata_->auto_resize = 1;
    metadata_->hash_mode = hash_mode;
    metadata_->reserved = 0;
    metadata_->total_size_in_bytes = buffer_size;
    metadata_->seed = seed;
    metadata_->nslots = 1ULL << exp_size;
    metadata_->xnslots = xnslots;
    metadata_->hash_bits = fp_bits;
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

#if defined(WIDENING)
    metadata_->num_expansions = 0;
#endif

    metadata_->exp_db_size_ = kBlockOffsetBits + kUnitOffsetBits;   // = 12
    metadata_->reciprocal_ratio_ = expansion_coefficient;
    metadata_->expansion_ratio_ = pow(2., 1./(double)(expansion_coefficient));
    metadata_->multiplicative_ratio_ = 1.0;
    metadata_->exp_ra_size_ = exp_size - metadata_->exp_db_size_;
    metadata_->current_size_ = (double)(1ULL << metadata_->exp_ra_size_);
    metadata_->current_epoch_ = 0ULL;
    metadata_->current_superblock_index_ = 0ULL;
    metadata_->appended_datablock_num_ = 1ULL;
    metadata_->appended_datablock_size_ = 1ULL;
    metadata_->space_amplification_ = 0;

    // additional space to account for 10*sqrt(n) slots
    indexABC additional_index = entry_lookup_internal((xnslots) >> (metadata_->exp_db_size_));
    uint64_t blocks_to_add = additional_index.superblock_index
                           + additional_index.datablock_index + 1;
    
    uint64_t current_appended_datablocks = 0ULL;
    uint64_t index_block_size = calculate_A(metadata_->exp_ra_size_) + 1;

    for (uint64_t i = 0; i < blocks_to_add; ++i) {
        uint64_t new_size = qfblock_size * (metadata_->appended_datablock_size_ << kUnitOffsetBits);
        metadata_->total_memory_usage += new_size;
        uint8_t* datablock_buffer = new uint8_t[new_size]{};
        qfblock* qbi = reinterpret_cast<qfblock*>(datablock_buffer);
        index_block_.push_back(qbi);
        ++current_appended_datablocks;
        /**
         * This superblock is full; move on to the next superblock by increasing
         * either the data block number of data block size. 
         */
        if (current_appended_datablocks == metadata_->appended_datablock_num_) {
            if (metadata_->current_superblock_index_ & 1ULL) {
                metadata_->appended_datablock_num_ <<= 1;
            } else {
                metadata_->appended_datablock_size_ <<= 1;
            }
            current_appended_datablocks = 0;
            metadata_->current_superblock_index_ += 1;
        }
    }
    metadata_->current_appended_datablocks_ = current_appended_datablocks;
    metadata_->is_period_ = true;
    
    runtimedata_ = new qfruntime;
    runtimedata_->num_locks = (metadata_->xnslots / kNumSlotsToLock) + 2;
    // runtimedata_->locks = new spinlock_padded[runtimedata_->num_locks]{};
    size_t lock_bytes = runtimedata_->num_locks * sizeof(spinlock_padded);
    runtimedata_->locks = (spinlock_padded*)malloc(lock_bytes);
    if (runtimedata_->locks) memset(runtimedata_->locks, 0, lock_bytes);
}

inline ZenoFilter::~ZenoFilter() {
    if (runtimedata_) {
        while (runtimedata_->resizing_region.load(std::memory_order_acquire) != -1) {
            std::this_thread::yield();
        }
    }
    for (qfblock* qbi : index_block_) {
        delete[] qbi;
    }
    if (metadata_) {
        delete metadata_;
    }
    if (runtimedata_) {
        if (runtimedata_->locks) free(runtimedata_->locks);
        delete runtimedata_;
    }
}

inline ZenoFilter& ZenoFilter::operator=(ZenoFilter&& other) noexcept {
    metadata_ = other.metadata_;
    blocks_ = other.blocks_;
    runtimedata_ = other.runtimedata_;
    other.metadata_ = nullptr;
    other.blocks_ = nullptr;
    other.runtimedata_ = nullptr;
    return *this;
}

int ZenoFilter::insert(uint64_t key, uint64_t value, uint64_t count, uint8_t flags) {
    if (count_occupied_slots() >= metadata_->nslots * metadata_->expansion_threshold) {
        // No more space. Either grow or fail based on `auto_resize`
        if (metadata_->auto_resize) {
            int32_t grow_ret = 1;

            if (GET_NO_LOCK(flags) == kNoLock && GET_IS_FILTER_GROWING(flags) != kIsFilterGrowing) {
                grow_ret = grow(0, 0, flags);
            } else {
                int64_t expected_region = -1;
                int64_t max_region = (metadata_->nslots - 1) / kNumSlotsToLock;
                // We were the first one to trigger grow
                if (runtimedata_->resizing_region.compare_exchange_strong(expected_region, 
                                                  max_region, std::memory_order_acq_rel)) {
                    int64_t max_region_upper = metadata_->xnslots / kNumSlotsToLock;
                    runtimedata_->resizing_region_upper.store(max_region_upper, 
                                                              std::memory_order_release);
                    std::thread([this, flags]() {
                        this->grow(0, 0, flags);
                    }).detach();
                } else {
                    // Don't grow otherwise
                }
            }
            if (grow_ret <= 0) {
                if (grow_ret == kErrNoSpace) std::cerr << "Resize failed: no space" << std::endl;
                if (grow_ret == kErrNoFpBits) std::cerr << "Resize failed: no fp bits" << std::endl;
                if (grow_ret == kErrCouldntLock) std::cerr << "Resize failed: failed to lock" << std::endl;
                return grow_ret;
            }
        }
        else {
            return kErrNoSpace;
        }
    }

    if (count == 0) return 0;

    if (GET_NO_LOCK(flags) != kNoLock) {
        // while (runtimedata_->resize_pending.load(std::memory_order_acquire)) {
        while (runtimedata_->resizing_region.load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }
    }

    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = MurmurHash64A((void*)&key, sizeof(key), metadata_->seed);
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
        insert_unary(key);
        if (!metadata_->is_period_) sanitize_hash(key);
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
            std::cout << "ret " << ret << std::endl;
            std::cout << "failed thread : " << std::this_thread::get_id() << std::endl;
            ret = grow(hash, count, flags);
            if (ret > 0) {
                std::cerr << "Resize finished." << std::endl;
            } else {
                std::cerr << "Resize failed." << std::endl;
                ret = kErrNoSpace;
            }
        } else {
            std::cerr << "Zeno filter is filling up." << std::endl;
            ret = kErrNoSpace;
        }
    }

    return ret;
}

inline
int32_t ZenoFilter::remove(uint64_t key, uint64_t value, uint64_t count, 
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
        sanitize_hash(key);
    }
    uint64_t hash = (key << metadata_->value_bits) | 
                    (value & BITMASK(metadata_->value_bits));
    int32_t ret = 0;
    // Delete longest matching fingerprints until we have deleted up to `count`
    // entries or failed fo find matching fingerprint
    while (count) {
        ret = remove_longest_internal(hash, count, flags);
        if (ret < 0) return ret;
    }
    return ret;
}

int ZenoFilter::delete_key_value(uint64_t key, uint64_t value, uint8_t flags) {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
        sanitize_hash(key);
    }
    uint64_t hash = (key << metadata_->value_bits) | (value & BITMASK(metadata_->value_bits));

    return remove_internal(hash, std::numeric_limits<uint64_t>::max(), flags);
}

int64_t ZenoFilter::grow(uint64_t dangling_hash, uint64_t dangling_count, uint8_t flags) {
    ++metadata_->current_epoch_;
    metadata_->is_period_ = false;

    // `divide_by` divides the current 'stretched' canonical slot index to the original value. 
    // `multiply_by` stretches the canonical slot index. 
#if defined(FIXED)
    FixedPoint divide_by = metadata_->multiplicative_ratio_;
    FixedPoint multiply_by = 1.0;
#else
    double divide_by = metadata_->multiplicative_ratio_;
    double multiply_by = 1.0;   // multiplies ceiling of divided canonical slot
#endif
    
    // The size of the filter after expansion. Disregard the additional sqrt slots and calculate the 
    // larger filter normally, then append the sqrt part at the end. `nslots` here is the size of 
    // the filter without sqrt. 
    uint64_t next_nslots = (uint64_t)(metadata_->nslots * metadata_->expansion_ratio_);

    // Does this period terminate in this expansion?
    bool period_terminates = false;

    // Current period terminates. Set `next_nslots` to the smallest power of 2
    // greater than `next_nslots`. 
    if (metadata_->current_epoch_ == metadata_->reciprocal_ratio_) {
        next_nslots = calculateNextPowerOf2(next_nslots);
        metadata_->current_epoch_ = 0;
        period_terminates = true;
        metadata_->exp_ra_size_ += 1;
    } else {
        multiply_by = divide_by * metadata_->expansion_ratio_;
    }

    // Calculate the number of data blocks to be added.
    uint64_t next_xnslots = next_nslots + 10*sqrt((double)next_nslots);
    indexABC additional_index = entry_lookup_internal((next_xnslots) >> (metadata_->exp_db_size_));
    uint64_t blocks_to_add = additional_index.superblock_index
                           + additional_index.datablock_index + 1
                           - index_block_.size();
    uint64_t appended_datablocks = metadata_->current_appended_datablocks_;

    /**
     * Zeno locks four regions for expansion; two regions for reads and two regions for writes.
     * These values keep track of the indexes and informs when to acquire or release a new lock.
     * Note: The two read regions might not be consecutive, depending on the run length. 
     */
    int64_t read_region_first = -1;
    int64_t read_region_second = -1;
    int64_t write_region_first = -1;
    int64_t write_region_second = -1;

    /** 
     * Allocate memory for the larger filter. The behavior differs between RARRAY and VMEM. Both
     * expands in-place, and the code accounts for fractional expansion as well. 
     */
#if defined(WIDENING)
    uint64_t new_fingerprint_length;
    uint64_t original_fingerprint_length;
    uint64_t original_quotient_length = metadata_->quotient_bits;
    /**
     * This buffers the first 6143 slots, which is the index that, when multiplied by 2, gets mapped
     * to the same rarray data block and causes problems. Index 6143 is in data block 1 and index
     * 6143*2=12286 is also in data block 1. Index 6143 is the first index that is mapped to the 
     * same data block. 
     */
    uint64_t buffer_length = metadata_->nslots > 6144 ? 6144 : metadata_->nslots - 1;

    // The index block that should be replaced with 
    int64_t expanding_block_index = index_block_.size() - 1;

    if (period_terminates) {
        /**
         * The "original" fingerprint length, given to the filter at the beginning. Used to compute
         * the new fingerprint length (`new_fingerprint_length`). After computation, it gets rolled
         * back into the fingerprint length of the previous period. 
        */
        original_fingerprint_length = metadata_->fingerprint_bits -
                                      2 * std::floor(std::log2(metadata_->num_expansions + 1));
        new_fingerprint_length = original_fingerprint_length + 
                                      2 * std::floor(std::log2(metadata_->num_expansions + 2));
        uint64_t new_bits_per_slot = new_fingerprint_length + metadata_->value_bits;
        uint64_t new_qfblock_size = sizeof(qfblock) + (kSlotsPerBlock * new_bits_per_slot / 8);
        original_fingerprint_length += 2 * std::floor(std::log2(metadata_->num_expansions + 1));

        // Update metadata
        metadata_->num_expansions += 1;
        metadata_->qfblock_size = new_qfblock_size;
    }
#endif
    for (uint64_t i = 0; i < blocks_to_add; ++i) {
        uint64_t new_size = metadata_->qfblock_size *
                            (metadata_->appended_datablock_size_ << kUnitOffsetBits);
        metadata_->total_memory_usage += new_size;
        uint8_t* buffer = new uint8_t[new_size]{};
        qfblock* qbi = reinterpret_cast<qfblock*>(buffer);
        index_block_.push_back(qbi);

        ++appended_datablocks;
        
        if (appended_datablocks == metadata_->appended_datablock_num_) {
            if (metadata_->current_superblock_index_ & 1ULL) {
                metadata_->appended_datablock_num_ <<= 1;
            } else {
                metadata_->appended_datablock_size_ <<= 1;
            }
            appended_datablocks = 0;
            metadata_->current_superblock_index_ += 1;
        }
    }
    metadata_->current_appended_datablocks_ = appended_datablocks;
    metadata_->nslots = next_nslots;
    metadata_->xnslots = next_xnslots;
    metadata_->nblocks = (metadata_->xnslots + kSlotsPerBlock - 1) / kSlotsPerBlock;

    uint64_t fingerprint, value, count, quotient, new_hash, canonical_slot;
    int64_t ret_numkeys = 0;

    // TODO: metadata adjustments are currently spread all over the code. Collect and make uniform.
    if (period_terminates) {
        uint64_t original_hash;
        // Temporary vector to store zero indexed values to avoid re-inserting
        std::vector<ZeroBufferEntry> zero_buffer;

        // Newly created void entry; used for comparison.
        uint64_t A = BITMASK(metadata_->fingerprint_bits);
        uint64_t B = A - 1;
        // new_void compares newly created void entries.
        uint64_t new_void_entry;
        uint64_t original_quotient;
        int32_t status = 0;

#if defined(WIDENING)
        new_void_entry = BITMASK(new_fingerprint_length) - 1;

        /**
         * If widening, buffer the first data block so that it doesn't mess up the occupieds and 
         * runends of the widened interpretation.
         * TODO: under wraparound, we must first get rid of the wrapped around entires before this
         * check.
         */
        
        ZenoFilter * buffer_zeno = nullptr;
        // Create an empty iterator
        iterator<ZenoFilter> buffer_it(this);
        buffer_it.get_last_canonical_slot(buffer_length / kSlotsPerBlock);
        if (!buffer_it.is_valid()) return 0;
        
        uint64_t widening_hash, buffer_canonical_slot;

        uint64_t buffer_size = run_end(buffer_it.get_canonical_slot()) + 1;
        uint64_t buffer_size_in_bits = 64 - __builtin_clzll(buffer_size);

        buffer_zeno = new ZenoFilter(buffer_size_in_bits, 
                              buffer_size_in_bits + metadata_->fingerprint_bits, 
                              metadata_->value_bits, metadata_->reciprocal_ratio_, 
                              metadata_->hash_mode, metadata_->seed);

        for (; buffer_it.is_valid() && buffer_it.get_canonical_slot() >= 0; --buffer_it) {
            uint64_t run_length;
            buffer_canonical_slot = buffer_it.get_canonical_slot();
            run_length = buffer_it.get_run_length();
            status = buffer_it.get_entry(fingerprint, value, count);
            original_hash = buffer_canonical_slot << metadata_->fingerprint_bits | fingerprint;

            /* Insert the first kv pair normally. */
            if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                return -1;
            }
            buffer_zeno->insert(original_hash, value, count, kNoLock | kKeyIsHash);

            /**
             * If there are remaining entries in the target run, delete the entire run and insert
             * the run to the slot. We do this because there is a problem with inserting multiple
             * entries at slot 0 using function shift_for_inserts(). If slot 0 is empty and a new
             * run is created at slot 0 that pushes subsequent runs' runends, it deletes the runends
             * instead of pushing them. Therefore we insert a single entry up front to 'warm up' the
             * slot and set the occupieds and runends bit of that slot. This is similar to how it is
             * done at function insertN(). 
             */
            if (run_length - count > 0) {
                uint64_t* p = delete_run(buffer_canonical_slot, run_length);
                buffer_zeno->insert_run(buffer_canonical_slot, p, run_length);
                delete[] p;
            }
        }

        /* Allocate first data block because it is guaranteed to be empty */
        qfblock* qb_zero = index_block_[0];
        delete[] qb_zero;
        uint8_t* qb_zero_buffer = new uint8_t[metadata_->qfblock_size * (1 << kUnitOffsetBits)]{};
        qb_zero = reinterpret_cast<qfblock*>(qb_zero_buffer);
        index_block_[0] = qb_zero;
#else
        new_void_entry = B;
#endif

        // Data structures to facilitate void entry expansion and cleanup.
        std::stack<uint64_t> void_stack;        // stack of valid void sequences
        std::stack<uint64_t> delete_stack;      // stack of deleted void sequences
        std::stack<uint64_t> zero_void_stack;   // stack for storing void sequences at slot 0

        int ret = 0;
        // The number of current void sequences we have examined
        int void_seq_num = 0;
        // The number of deleted void sequences
        int deleted_void_seq_num = 0;
        // The last canonical slot we have seen with any void sequences. When we encounter an A seq
        // at an odd slot, this value gets initialized to that index. Whenever we encounter further
        // void seqs, this value is updated accordingly. This is to keep track of void sequences 
        // that should be there but are completely deleted. 
        int last_valid_canonical_slot = 0;

        // Iterate through the filter and move items
        iterator<ZenoFilter> it(this, kMaxPosition);
        if (GET_NO_LOCK(flags) == kNoLock) {
            it.disable_region();
        }
        // The iterator is invalid. This may be due to buffering out all existing entries. 
        if (!it.is_valid()) {
#if defined(WIDENING)
            // The invalid iterator is due to empty filter. Return normally. 
            if (buffer_zeno == nullptr) {
                return 0;
            
            // The invalid iterator is due to buffering out all existing entries. Initialize 
            // iterator from the buffered filter. 
            } else {
                it = iterator<ZenoFilter>(buffer_zeno, kMaxPosition);
            }
#else
            return 0;
#endif
        }

        // We can safely get the canonical slot because we locked the last regions already.
        canonical_slot = it.get_canonical_slot();

        if (GET_NO_LOCK(flags) != kNoLock) {
            write_region_first = (canonical_slot << 1) / kNumSlotsToLock;
            write_region_second = write_region_first + 1;
            write_region_second = (write_region_second < runtimedata_->num_locks) 
                                ? write_region_second : runtimedata_->num_locks - 1;
            for (auto r = write_region_first; r <= write_region_second; ++r) {
                if (r <= read_region_second) continue;
                if (!zeno_lock_region(r, kWaitForLock)) {
                    for (auto q = r; q >= write_region_first; --q) {
                        zeno_unlock_region(q);
                    }
                    return kErrCouldntLock;
                }
            }
            runtimedata_->resizing_region_upper.store(write_region_second, std::memory_order_release);
        }

        // The slot that holds the index of the 'previous' cluster start. 
        uint64_t prev_cluster_start_index = 0;
        // The updated write_region_second.
        int64_t new_write_region_second = write_region_first;

        // Main loop for deleting existing entries and reinserting them to the larger filter
        do {
            // Potentially lock the reading region
            if (GET_NO_LOCK(flags) != kNoLock) {
                if (it.is_new_region()) {
                    int64_t new_region = it.current_region_;
                    if (new_region < read_region_first) {
                        // unlock from read_region_second to read_region_first
                        for (auto r = read_region_second; r >= read_region_first; --r) {
                            if (r >= write_region_first) continue;
                            zeno_unlock_region(r);
                        }

                        for (auto r = write_region_second; r >= write_region_first; --r) {
                            zeno_unlock_region(r);
                        }
                        --write_region_first;

                        runtimedata_->resizing_region.store(new_region, std::memory_order_release);
                        // std::cout << "(1) trying to acquire read lock...";
                        // auto startt = std::chrono::high_resolution_clock::now();
                        // lock from new_region to read_region_second
                        for (auto r = new_region; r <= read_region_second; ++r) {
                            if (!zeno_lock_region(r, kWaitForLock)) {
                                return kErrCouldntLock;
                            }
                        }
                        // auto end = std::chrono::high_resolution_clock::now();
                        // auto wait_time_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startt).count();
                        // std::cout << " took " << wait_time_us << " nanoseconds." << std::endl;
                        // runtimedata_->total_grow_time += wait_time_us;

                        // std::cout << "(2) trying to acquire write lock...";
                        // startt = std::chrono::high_resolution_clock::now();
                        for (auto r = write_region_first; r <= write_region_second; ++r) {
                            if (r <= read_region_second) continue;
                            if (!zeno_lock_region(r, kWaitForLock)) {
                                return kErrCouldntLock;
                            }
                        }
                        // end = std::chrono::high_resolution_clock::now();
                        // wait_time_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startt).count();
                        // std::cout << " took " << wait_time_us << " nanoseconds." << std::endl;
                        // runtimedata_->total_grow_time += wait_time_us;

                        read_region_first = new_region;
                        // std::cout << " (e) read  lock updated to " << read_region_first << std::endl;
                        // std::cout << " (e) write lock updated to " << write_region_first << std::endl;
                    }
                    if (it.is_valid()) {
                        if (!it.get_last_canonical_slot(it.current_block_)) {
                            it.invalidate();
                        } else {
                            // Successfully found valid canonical slot and updated metadata. 
                            canonical_slot = it.get_canonical_slot();
                        }
                    }
                    it.unmark_new_region();
                }
                // If we no longer need to lock the next region, unlock it
                uint64_t new_runend = run_end(canonical_slot);
                int64_t new_last_read_region = new_runend / kNumSlotsToLock;
                // Start unlocking old regions if this isn't the only region locked, and the new
                // second read region is smaller than the current second read region. 
                if (read_region_second != read_region_first && 
                    new_last_read_region < read_region_second) {
                    for (auto r = read_region_second; r > new_last_read_region; --r) {
                        // Don't unlock if this region is being written to
                        if (r >= write_region_first) continue;
                        zeno_unlock_region(r);
                    }
                    read_region_second = new_last_read_region;
                    // runtimedata_->resizing_region_upper.store(read_region_second, 
                    //                                           std::memory_order_release);
                    // std::cout << " (d) read end updated to " << read_region_second << std::endl;
                }
            }

            status = it.get_entry(fingerprint, value, count);

            // Delete the entry from the filter first
            original_hash = canonical_slot << metadata_->fingerprint_bits | fingerprint;
#if defined(WIDENING)
            uint64_t slot_runend = run_end(canonical_slot);
            // If widening is defined and the iterator points to buffer_zeno, delete from buffer
            if (it.filter_ == buffer_zeno) {
                if (!(buffer_zeno->delete_key_value(original_hash, value, kNoLock | kKeyIsHash))) {
                    return -1;
                }
            } else {
                if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                    // TODO: fix error code
                    return -1;
                }
            }
            // TODO: maybe erase?
            bool slot_is_occupied = is_occupied(canonical_slot);

#else
            int this_encoding_length = delete_key_value(original_hash, value, kNoLock | kKeyIsHash);
            if (this_encoding_length < 0) {
                // TODO: fix error code
                return -1;
            }
#endif

            // Get the original canonical slot
            quotient = static_cast<uint64_t>(std::ceil(static_cast<double>(canonical_slot) / divide_by));
            original_quotient = quotient;

            // Potentially lock the writing region
            if (GET_NO_LOCK(flags) != kNoLock) {
                if (canonical_slot < prev_cluster_start_index) {
                    new_write_region_second = (prev_cluster_start_index << 1) / kNumSlotsToLock;
                }

                uint64_t target_quotient = quotient << 1;

                // If the new canonical slot falls into a new region, unlock the current first
                // region and lock the new region. This is to prevent deadlocks.
                uint64_t new_write_region = target_quotient / kNumSlotsToLock;
                if (new_write_region < write_region_first) {
                    // If the previous region is already locked by the reader, update metadata and
                    // do no more locking or unlocking. 
                    if (new_write_region <= read_region_second) {
                        write_region_first = new_write_region;
                    } else {
                        for (auto r = write_region_second; r >= write_region_first; --r) {
                            if (r <= read_region_second) continue;
                            zeno_unlock_region(r);
                        }
                        write_region_first = new_write_region;
                        if (new_write_region_second < write_region_second) {
                            write_region_second = new_write_region_second;
                            prev_cluster_start_index = 0;
                        }
                        // std::cout << "(3) trying to acquire write lock...";
                        // auto startt = std::chrono::high_resolution_clock::now();
                        for (auto r = write_region_first; r <= write_region_second; ++r) {
                            if (r <= read_region_second) continue;
                            if (!zeno_lock_region(r, kWaitForLock)) {
                                return kErrCouldntLock;
                            }
                        }
                        // If write_region_second is updated, broadcast
                        if (prev_cluster_start_index == 0) {
                            runtimedata_->resizing_region_upper.store(write_region_second, 
                                                                      std::memory_order_release);
                        }
                        // auto end = std::chrono::high_resolution_clock::now();
                        // auto wait_time_us = std::chrono::duration_cast<std::chrono::nanoseconds>(end - startt).count();
                        // std::cout << " took " << wait_time_us << " nanoseconds." << std::endl;
                        // runtimedata_->total_grow_time += wait_time_us;
                        // std::cout << " (w) write lock updated to " << write_region_first << std::endl;
                    }
                }

                if (prev_cluster_start_index == 0 && it.get_current_slot() == canonical_slot) {
                    prev_cluster_start_index = canonical_slot;
                }
            }

#if defined(WIDENING)
            // Switch to widened fingerprint length
            metadata_->fingerprint_bits = new_fingerprint_length;
            metadata_->bits_per_slot = metadata_->fingerprint_bits + metadata_->value_bits;
            metadata_->quotient_bits += 1;
            metadata_->hash_bits = metadata_->quotient_bits + metadata_->fingerprint_bits;


            uint64_t block_index = slot_runend / kSlotsPerBlock;
            uint64_t datablock_index = (block_index >> kUnitOffsetBits) + 1;
            uint64_t k = 63ULL - __builtin_clzll(datablock_index);
            uint64_t floor_k = (k >> 1), mask_up = (1ULL << floor_k) - 1;
            uint64_t ceil_k = ((k + 1) >> 1);
            uint64_t directory_index = calculate_A(k) + ((datablock_index >> ceil_k) & mask_up);

            /**
             * Reallocation of a new data block under RARRAY happens under three conditions:
             * 1. The data block that the iterator encounters in this loop is different from the one
             * that was previously seen. Specifically, because the runend might spill over to the 
             * next data block, we check if the runend of the run is located at a new data block.
             * 2. The slot is not occupied (i.e., the run is completely removed)
             * 3. directory_index is not 0, because we have already reallocated a larger data block.
             */
            if (static_cast<int64_t>(directory_index) < expanding_block_index 
                /* targeting an entry from a new data block */
                && !slot_is_occupied /* completely removed the run */
                && directory_index != 0 /* first data block is already reallocated */) {
                datablock_index += 1;   /* Next datablock we want to reallocate */
                k = 63ULL - __builtin_clzll(datablock_index);
                floor_k = (k >> 1), mask_up = (1ULL << floor_k) - 1;
                ceil_k = ((k + 1) >> 1);
                directory_index = calculate_A(k) + ((datablock_index >> ceil_k) & mask_up);

                qfblock* qb_realloc = index_block_[directory_index];
                delete[] qb_realloc;
                uint8_t* qb_realloc_buffer = new uint8_t[metadata_->qfblock_size * 
                                                        ((1ULL << ceil_k) << kUnitOffsetBits)]{};
                qb_realloc = reinterpret_cast<qfblock*>(qb_realloc_buffer);
                index_block_[directory_index] = qb_realloc;
                --expanding_block_index;
            }
#endif
            // Queue for buffering void sequence inserts. Actual inserts are done after --it. 
            // Format: [first index, last index, count]
            std::deque<uint64_t> void_seq_to_insert;

            // Encountered an A sequence. This is either a start of a void seq
            // or an end of a void seq. 
            if (fingerprint == A) {
                // Original number of As
                uint64_t original_count_A = count;
                // Original number of Bs
                uint64_t original_count_B = 0;
                // Original number of void sequences to compare with (potential) Bs
                uint64_t original_void_seq_num = void_seq_num;
                // Odd index - start of a void seq. Add As into stack
                if (quotient & 1) {
                    void_seq_num += count;
                    while (count--) {
                        void_stack.push(quotient);
                    }
                }

                // There are still subsequent entries; this must be Bs. The Bs here might be a real
                // intermediate B from a gen N sequence or an added delimiter for deleted As. 
                if (it.is_occupied()) {
                    status = it.get_entry(fingerprint, value, count);
                    assert(fingerprint == B);
                    original_count_B = count;
                    // Number of deleted As
                    uint64_t deleted_count_A = 0;
                    // Delete the entry from the filter first
                    original_hash = canonical_slot << metadata_->bits_per_slot | fingerprint;
#if defined(WIDENING)
                    if (it.filter_ == buffer_zeno) {
                        if (!(buffer_zeno->delete_key_value(original_hash, value, kNoLock | kKeyIsHash))) {
                            return -1;
                        }
                    } else {
                        if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                            return -1;
                        }
                    }
#else
                    if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                        // TODO: fix error code
                        return -1;
                    }
#endif

                    // There are still subsequent entries; this must be As. These are the deleted As
                    if (it.is_occupied()) {
                        status = it.get_entry(fingerprint, value, count);
                        assert(fingerprint == A);
                        // Delete the entry from the filter first
                        original_hash = canonical_slot << metadata_->bits_per_slot | fingerprint;
#if defined(WIDENING)
                        if (it.filter_ == buffer_zeno) {
                            if (!(buffer_zeno->delete_key_value(original_hash, value, kNoLock | kKeyIsHash))) {
                                return -1;
                            }
                        } else {
                            if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                                return -1;
                            }
                        }
#else
                        if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                            // TODO: fix error code
                            return -1;
                        }
#endif

                        deleted_void_seq_num += count;
                        deleted_count_A = count;
                        while (count--) {
                            delete_stack.push(quotient);
                        }
                    }

                    // The conditions we must check for the number of deleted Bs differ depending on
                    // whether this is the starting A seq or ending A seq. 
                    if (quotient & 1) {
                        // If there were gen N sequences, the Bs here must indicate real Bs; any 
                        // difference here means there are deleted void seqs. 
                        if (original_void_seq_num) {
                            uint64_t deleted_count = original_void_seq_num - original_count_B;
                            deleted_void_seq_num += deleted_count;
                            while (deleted_count--) {
                                delete_stack.push(quotient);
                            }
                        
                        // This is a start of a fresh void sequence with no other existing void seqs
                        // The existence of a B here simply denotes a delimiter. 
                        } else {
                            assert(original_count_B == 1);
                        }
                    } else {
                        // There can't be more ending As than the number of existing void seqs. 
                        assert(original_count_A <= void_seq_num);
                        // All existing void seqs end here. The existence of a B here simply denotes
                        // a delimiter.
                        if (original_count_A == void_seq_num) {
                            assert(original_count_B == 1);
                        } else {
                            uint64_t deleted_count = original_void_seq_num - original_count_A - original_count_B;
                            deleted_void_seq_num += deleted_count;
                            while (deleted_count--) {
                                delete_stack.push(quotient);
                            }
                        }
                    }
                }

                // Even index - end of a void seq. Pop As from stack and insert
                // void sequence accordingly. 
                if (!(quotient & 1)) {
                    uint64_t end_quotient, deleted_quotient;
                    // TODO: bulk load void entries with same end quotient
                    while (original_count_A--) {
                        end_quotient = void_stack.top();
                        void_stack.pop();
                        // Check if there are deleted sequences. If the deleted quotient is greater
                        // than the void_stack index, then this void sequence could not have been
                        // deleted. Continue with inserting the doubled void sequence. Otherwise,
                        // delete the void sequence by not inserting the doubled void sequence. 
                        if (deleted_void_seq_num) {
                            deleted_quotient = delete_stack.top();
                            // This void seq has not been deleted. 
                            if (end_quotient < deleted_quotient) {
                                if (canonical_slot == 0) {
                                    zero_void_stack.push(quotient << 1);
                                    zero_void_stack.push((end_quotient << 1) + 1);
                                } else {
                                    void_seq_to_insert.push_back(quotient << 1);
                                    void_seq_to_insert.push_back((end_quotient << 1) + 1);
                                    void_seq_to_insert.push_back(1);
                                }
                            // This void seq has been deleted. 
                            } else {
                                --deleted_void_seq_num;
                                delete_stack.pop();
                            }
                        } else {
                            if (canonical_slot == 0) {
                                zero_void_stack.push(quotient << 1);
                                zero_void_stack.push((end_quotient << 1) + 1);
                            } else {
                                void_seq_to_insert.push_back(quotient << 1);
                                void_seq_to_insert.push_back((end_quotient << 1) + 1);
                                void_seq_to_insert.push_back(1);
                            }
                        }
                        --void_seq_num;
                    }
                    assert(delete_stack.size() == deleted_void_seq_num);
                }

            // Encountered a B sequence. This is either a gen 1 void seq or the intermediate 
            // sequence of gen N. 
            } else if (fingerprint == B) {
                // If the run is occupied after deletion of B, it must be the case that an A follows
                // which indicates a gen 1 void seq.
                if (it.is_occupied()) {
#if defined(WIDENING)
                    if (it.filter_ == buffer_zeno) {
                        if ((buffer_zeno->remove_internal(original_hash + 1 /*A*/, 1, kNoLock)) < 0) {
                            return -1;
                        }
                    } else {
                        if (remove_internal(original_hash + 1 /*A*/, 1, kNoLock) < 0) {
                            return -1;
                        }
                    }
#else
                    if (remove_internal(original_hash + 1 /*A*/, 1, kNoLock) < 0) {
                        return -1;
                    }
#endif
                    void_seq_to_insert.push_back(quotient << 1);
                    void_seq_to_insert.push_back((quotient << 1) + 1);
                    void_seq_to_insert.push_back(count);

                // This is the last B in the run; an intermediate sequence of gen N. Calculate the
                // number of void seqs that would have been deleted. 
                } else {
                    if (count < void_seq_num) {
                        deleted_void_seq_num += void_seq_num - count;
                        for (uint64_t d = 0; d < void_seq_num - count; ++d) {
                            delete_stack.push(quotient);
                        }
                    }
                }
            } else {
#if defined(WIDENING)
                new_hash = adjust_fingerprint_length_widening(quotient, fingerprint, original_fingerprint_length);
#else
                new_hash = adjust_fingerprint_length(quotient, fingerprint);
#endif
                // If new index is not zero, insert normally
                if (new_hash >> metadata_->bits_per_slot) {
                    // New void sequence is created.
                    if (fingerprint == new_void_entry) {
                        ret = insert_void_sequence(quotient, quotient, count, kNoLock);

                    // Insertion of ordinary hash.
                    } else {
                        ret = insert(new_hash, value, count, kNoLock | kKeyIsHash | kIsFilterGrowing);
                    }
                    if (ret < 0) {
                        std::cerr << "Failed to insert key: " << new_hash << " into the new filter." 
                                  << std::endl;
                        return ret;
                    }
                
                // If new index is zero, buffer inserts to prevent cyclic insert
                } else {
                    zero_buffer.push_back(ZeroBufferEntry(new_hash, count, value));
                }
            }

#if defined(WIDENING)
            // Switch back to original fingerprint length and corresponding metadata
            metadata_->fingerprint_bits = original_fingerprint_length;
            metadata_->bits_per_slot = metadata_->fingerprint_bits + metadata_->value_bits;
            metadata_->quotient_bits -= 1;
            metadata_->hash_bits = metadata_->quotient_bits + metadata_->fingerprint_bits;
#endif
            --it;
            /**
             * Insert the buffered void sequences in the queue
             */
            while (!void_seq_to_insert.empty()) {
                uint64_t start_quotient = void_seq_to_insert.front();
                void_seq_to_insert.pop_front();
                uint64_t end_quotient = void_seq_to_insert.front();
                void_seq_to_insert.pop_front();
                uint64_t count = void_seq_to_insert.front();
                void_seq_to_insert.pop_front();
                insert_void_sequence(start_quotient, end_quotient, count, kNoLock);
            }
            /**
             * The iterator gets marked as `new_region` when it reaches a new locking region. This
             * is because with concurrency, it is not thread safe to read the canonical slot of a 
             * previous region without having locked it. If there is no concurrency, ignore this
             * flag and proceed as normal. This is invasive to the iterator class. 
             */
            if (GET_NO_LOCK(flags) == kNoLock) {
                if (it.is_new_region()) {
                    it.get_last_canonical_slot(it.current_block_);
                    it.unmark_new_region();
                }
            }

            ++ret_numkeys;
            canonical_slot = it.get_canonical_slot();

#if defined(WIDENING)
            /**
             * Switch iterator to buffered filter iff:
             * 1. the current iterator is invalid or looks into the region of the buffered filter
             * 2. there exists a valid buffered filter
             * 3. the iterator doesn't already index the buffered filter
             */
            if ((canonical_slot <= buffer_length || !it.is_valid()) && buffer_zeno != nullptr && 
                it.filter_ != buffer_zeno) { 
                it = iterator<ZenoFilter>(buffer_zeno, kMaxPosition);
                canonical_slot = it.get_canonical_slot();
            }
#endif

        } while (it.is_valid());

        // WIDENING
#if defined(WIDENING)
        metadata_->fingerprint_bits = new_fingerprint_length;
        metadata_->bits_per_slot = metadata_->fingerprint_bits + metadata_->value_bits;
        metadata_->quotient_bits += 1;
        metadata_->hash_bits = metadata_->quotient_bits + metadata_->fingerprint_bits;
#endif

        // Insert the buffered zero-indexed entries to the filter. 
        for (auto new_hash : zero_buffer) {
            int ret;
            if (new_hash.hash == B) {
                ret = insert(B, 0, new_hash.count, kNoLock | kKeyIsHash);
                ret = insert(A, 0, new_hash.count, kNoLock | kKeyIsHash);
            } else {
                ret = insert(new_hash.hash, new_hash.value, new_hash.count, kNoLock | kKeyIsHash);
            }
            if (ret < 0) {
                std::cerr << "Failed to insert key: " << new_hash.hash << " into the new filter." 
                          << std::endl;
                return ret;
            }
            ++ret_numkeys;
        }
        // Insert the buffered void entries to the filter.
        while (!zero_void_stack.empty()) {
            uint64_t end_quotient = zero_void_stack.top();
            zero_void_stack.pop();
            uint64_t start_quotient = zero_void_stack.top();
            zero_void_stack.pop();
            insert_void_sequence(start_quotient, end_quotient, 1, kNoLock);
        }

        // Unlock the remaining locked regions
        if (GET_NO_LOCK(flags) != kNoLock) {
            for (auto r = write_region_second; r >= read_region_first; --r) {
                zeno_unlock_region(r);
            }
        }

    // Expansion within an epoch. 
    } else {
        // Iterate through the filter and move items
        iterator<ZenoFilter> it(this, kMaxPosition);
        if (!it.is_valid()) return 0;
        canonical_slot = it.get_canonical_slot();
        uint64_t run_length;
        do {
            // Delete entire run associated with this canonical slot
            uint64_t* p = delete_run(canonical_slot, run_length);
            // Get the original canonical slot
            quotient = static_cast<uint64_t>(std::ceil(static_cast<double>(canonical_slot)/divide_by));
            // Get the new canonical slot
            quotient = multiply_by * quotient;
            // Insert the buffered run into the new canonical slot
            // TODO: fix return logic
            insert_run(quotient, p, run_length);
            --it;
            if (GET_NO_LOCK(flags) == kNoLock) {
                if (it.is_new_region()) {
                    it.get_last_canonical_slot(it.current_block_);
                    it.unmark_new_region();
                }
            }
            canonical_slot = it.get_canonical_slot();
            delete[] p;
            // TODO
            ++ret_numkeys;
        } while (it.is_valid() && canonical_slot);
    }

    // There is a 'dangling' hash to be inserted. 
    if (dangling_count) {
        fingerprint = dangling_hash & BITMASK(metadata_->bits_per_slot);
        value = fingerprint & BITMASK(metadata_->value_bits);
        fingerprint >>= metadata_->value_bits;
        canonical_slot = dangling_hash >> metadata_->bits_per_slot;
    #if defined(FIXED)
        quotient = canonical_slot / divide_by;
    #else
        quotient = static_cast<uint64_t>(std::ceil(static_cast<double>(canonical_slot)/divide_by));
    #endif

        if (period_terminates) {
#if defined(WIDENING)
            new_hash = adjust_fingerprint_length_widening(quotient, fingerprint, 
                                                          original_fingerprint_length);
#else
            new_hash = adjust_fingerprint_length(quotient, fingerprint);
#endif
        } else {
            // quotient *= multiply_by;
            quotient = multiply_by * quotient;
            new_hash = quotient << metadata_->fingerprint_bits | fingerprint;
        }

        int ret = insert(new_hash, value, count, kNoLock | kKeyIsHash);
        if (ret < 0) {
            std::cerr << "Failed to insert key: " << new_hash << " into the new filter." << std::endl;
            return ret;
        }
        ++ret_numkeys;
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        runtimedata_->resize_pending.store(true, std::memory_order_release);

        for (uint64_t l = 0; l < runtimedata_->num_locks; ++l) {
            while (runtimedata_->locks[l].lock_.lock_.load(std::memory_order_acquire)) {
                // print_lock_status();
                std::this_thread::yield();
            }
        }
    }

    // Cleanup
    if (period_terminates) {
        metadata_->is_period_ = true;
        metadata_->multiplicative_ratio_ = 1.0;
        // metadata_->quotient_bits += 1;
#if defined(WIDENING)
        metadata_->fingerprint_bits = new_fingerprint_length;
#else
        metadata_->quotient_bits += 1;
#endif

        metadata_->hash_bits = metadata_->quotient_bits + metadata_->fingerprint_bits;
        metadata_->range <<= 1;
    } else {
        metadata_->is_period_ = false;
        metadata_->multiplicative_ratio_ *= metadata_->expansion_ratio_;
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        runtimedata_->resizing_region.store(-1, std::memory_order_release);
        runtimedata_->resizing_region_upper.store(-1, std::memory_order_release);
        runtimedata_->resize_pending.store(false, std::memory_order_release);
    }

    return ret_numkeys;
}

int64_t ZenoFilter::contract() {
    uint64_t new_qbits = metadata_->quotient_bits - 1ULL;
    ZenoFilter new_filter(new_qbits, metadata_->hash_bits - 1ULL, metadata_->value_bits, 
                          metadata_->reciprocal_ratio_, metadata_->hash_mode, metadata_->seed,
                          metadata_->expansion_threshold);
    new_filter.set_auto_resize(metadata_->auto_resize);

    // Temporary vector to store zero indexed values to avoid re-inserting
    std::vector<ZeroBufferEntry> zero_buffer;

    // Newly created void entry; used for comparison.
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;
    // new_void compares newly created void entries.
    uint64_t new_void_entry = B;
    uint64_t original_quotient;
    int32_t status = 0;

    // Data structures to facilitate void entry expansion and cleanup.
    std::stack<uint64_t> void_stack;        // stack of valid void sequences
    std::stack<uint64_t> delete_stack;      // stack of deleted void sequences
    std::stack<uint64_t> zero_void_stack;   // stack for storing void sequences at slot 0

    int ret = 0;
    // The number of current void sequences we have examined
    int void_seq_num = 0;
    // The number of deleted void sequences
    int deleted_void_seq_num = 0;

    // Iterate through the filter and move items
    iterator<ZenoFilter> it(this, kMaxPosition);
    it.disable_region();
    if (!it.is_valid()) {
        std::cerr << "Iterator failed to initialize" << std::endl;
        return 0;
    }

    uint64_t original_hash, fingerprint, value, count, quotient, new_hash, canonical_slot;
    int64_t ret_numkeys = 0;

    canonical_slot = it.get_canonical_slot();

    // Main loop for moving entries over to the smaller filter
    do {
        status = it.get_entry(fingerprint, value, count);
        original_hash = canonical_slot << metadata_->fingerprint_bits | fingerprint;
        int this_encoding_length = delete_key_value(original_hash, value, kNoLock | kKeyIsHash);
        if (this_encoding_length < 0) {
            return -1;
        }
        quotient = canonical_slot;

        // Queue for buffering void sequence inserts. Actual inserts are done after --it. 
        // Format: [first index, last index, count]
        std::deque<uint64_t> void_seq_to_insert;

        // Encountered an A sequence. This is either a start of a void seq
        // or an end of a void seq. 
        if (fingerprint == A) {
            // Original number of As
            uint64_t original_count_A = count;
            // Original number of Bs
            uint64_t original_count_B = 0;
            // Original number of void sequences to compare with (potential) Bs
            uint64_t original_void_seq_num = void_seq_num;
            // Odd index - start of a void seq. Add As into stack
            if (quotient & 1) {
                void_seq_num += count;
                while (count--) {
                    void_stack.push(quotient);
                }
            }

            // There are still subsequent entries; this must be Bs. The Bs here might be a real
            // intermediate B from a gen N sequence or an added delimiter for deleted As. 
            if (it.is_occupied()) {
                status = it.get_entry(fingerprint, value, count);
                assert(fingerprint == B);
                original_count_B = count;
                // Number of deleted As
                uint64_t deleted_count_A = 0;
                // Delete the entry from the filter first
                original_hash = canonical_slot << metadata_->bits_per_slot | fingerprint;
                if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                    // TODO: fix error code
                    return -1;
                }

                // There are still subsequent entries; this must be As. These are the deleted As
                if (it.is_occupied()) {
                    status = it.get_entry(fingerprint, value, count);
                    assert(fingerprint == A);
                    // Delete the entry from the filter first
                    original_hash = canonical_slot << metadata_->bits_per_slot | fingerprint;
                    if (!delete_key_value(original_hash, value, kNoLock | kKeyIsHash)) {
                        // TODO: fix error code
                        return -1;
                    }

                    deleted_void_seq_num += count;
                    deleted_count_A = count;
                    while (count--) {
                        delete_stack.push(quotient);
                    }
                }

                // The conditions we must check for the number of deleted Bs differ depending on
                // whether this is the starting A seq or ending A seq. 
                if (quotient & 1) {
                    // If there were gen N sequences, the Bs here must indicate real Bs; any 
                    // difference here means there are deleted void seqs. 
                    if (original_void_seq_num) {
                        uint64_t deleted_count = original_void_seq_num - original_count_B;
                        deleted_void_seq_num += deleted_count;
                        while (deleted_count--) {
                            delete_stack.push(quotient);
                        }
                    
                    // This is a start of a fresh void sequence with no other existing void seqs
                    // The existence of a B here simply denotes a delimiter. 
                    } else {
                        assert(original_count_B == 1);
                    }
                } else {
                    // There can't be more ending As than the number of existing void seqs. 
                    assert(original_count_A <= void_seq_num);
                    // All existing void seqs end here. The existence of a B here simply denotes
                    // a delimiter.
                    if (original_count_A == void_seq_num) {
                        assert(original_count_B == 1);
                    } else {
                        uint64_t deleted_count = original_void_seq_num - original_count_A - original_count_B;
                        deleted_void_seq_num += deleted_count;
                        while (deleted_count--) {
                            delete_stack.push(quotient);
                        }
                    }
                }
            }

            // Even index - end of a void seq. Pop As from stack and insert
            // void sequence accordingly. 
            if (!(quotient & 1)) {
                uint64_t end_quotient, deleted_quotient;
                // TODO: bulk load void entries with same end quotient
                while (original_count_A--) {
                    end_quotient = void_stack.top();
                    void_stack.pop();
                    // Check if there are deleted sequences. If the deleted quotient is greater
                    // than the void_stack index, then this void sequence could not have been
                    // deleted. Continue with inserting the doubled void sequence. Otherwise,
                    // delete the void sequence by not inserting the doubled void sequence. 
                    if (deleted_void_seq_num) {
                        deleted_quotient = delete_stack.top();
                        // This void seq has not been deleted. 
                        if (end_quotient < deleted_quotient) {
                            if (canonical_slot == 0) {
                                zero_void_stack.push(quotient >> 1);
                                zero_void_stack.push(end_quotient >> 1);
                            } else {
                                void_seq_to_insert.push_back(quotient >> 1);
                                void_seq_to_insert.push_back(end_quotient >> 1);
                                void_seq_to_insert.push_back(1);
                            }
                        // This void seq has been deleted. 
                        } else {
                            --deleted_void_seq_num;
                            delete_stack.pop();
                        }
                    } else {
                        if (canonical_slot == 0) {
                            zero_void_stack.push(quotient >> 1);
                            zero_void_stack.push(end_quotient >> 1);
                        } else {
                            void_seq_to_insert.push_back(quotient >> 1);
                            void_seq_to_insert.push_back(end_quotient >> 1);
                            void_seq_to_insert.push_back(1);
                        }
                    }
                    --void_seq_num;
                }
                assert(delete_stack.size() == deleted_void_seq_num);
            }

        // Encountered a B sequence. This is either a gen 1 void seq or the intermediate 
        // sequence of gen N. 
        } else if (fingerprint == B) {
            // If the run is occupied after deletion of B, it must be the case that an A follows
            // which indicates a gen 1 void seq. We must revive this entry. 
            if (it.is_occupied()) {
                if (remove_internal(original_hash + 1 /*A*/, 1, kNoLock) < 0) {
                    return -1;
                }
                new_hash = extend_fingerprint_length(quotient, fingerprint);
                ret = new_filter.insert(new_hash, value, count, kNoLock | kKeyIsHash);

            // This is the last B in the run; an intermediate sequence of gen N. Calculate the
            // number of void seqs that would have been deleted. 
            } else {
                if (count < void_seq_num) {
                    deleted_void_seq_num += void_seq_num - count;
                    for (uint64_t d = 0; d < void_seq_num - count; ++d) {
                        delete_stack.push(quotient);
                    }
                }
            }
        } else {
            new_hash = extend_fingerprint_length(quotient, fingerprint);
            // If new index is not zero, insert normally
            if (new_hash >> metadata_->bits_per_slot) {
                ret = new_filter.insert(new_hash, value, count, kNoLock | kKeyIsHash);
                if (ret < 0) {
                    std::cerr << "Failed to insert key: " << new_hash << " into the new filter." 
                                << std::endl;
                    return ret;
                }
            // If new index is zero, buffer inserts to prevent cyclic insert
            } else {
                zero_buffer.push_back(ZeroBufferEntry(new_hash, count, value));
            }
        }

        --it;

        /**
         * Insert the buffered void sequences in the queue
         */
        uint64_t num_single_seq = 0;
        uint64_t start_quotient, end_quotient;
        while (!void_seq_to_insert.empty()) {
            start_quotient = void_seq_to_insert.front();
            void_seq_to_insert.pop_front();
            end_quotient = void_seq_to_insert.front();
            void_seq_to_insert.pop_front();
            uint64_t count = void_seq_to_insert.front();
            void_seq_to_insert.pop_front();
            if (end_quotient == start_quotient) num_single_seq += count;
            else new_filter.insert_void_sequence(start_quotient, end_quotient, count, kNoLock);
        }
        if (num_single_seq) {
            new_filter.insert_void_sequence(start_quotient, start_quotient, num_single_seq, kNoLock);
        }

        canonical_slot = it.get_canonical_slot();
    } while (it.is_valid());

    for (auto new_hash : zero_buffer) {
        int ret;
        ret = new_filter.insert(new_hash.hash, new_hash.value, new_hash.count, kNoLock | kKeyIsHash);
    }
    while (!zero_void_stack.empty()) {
        uint64_t end_quotient = zero_void_stack.top();
        zero_void_stack.pop();
        uint64_t start_quotient = zero_void_stack.top();
        zero_void_stack.pop();
        new_filter.insert_void_sequence(start_quotient, end_quotient, 1, kNoLock);
    }

    for (qfblock* qbi : index_block_) {
        delete[] qbi;
    }
    if (metadata_) {
        delete metadata_;
    }
    if (runtimedata_) {
        if (runtimedata_->locks) free(runtimedata_->locks);
        delete runtimedata_;
    }

    *this = std::move(new_filter);

    return 0;
}

// TODO: change logic to query for longest matching kv, similar to remove_internal
uint64_t ZenoFilter::query(uint64_t key, uint64_t& value, uint8_t flags) {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        if (metadata_->hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));}
        else if (metadata_->hash_mode == hashmode::Invertible)
            key = hash_64(key, BITMASK(metadata_->hash_bits));
        if (!metadata_->is_period_) sanitize_hash(key);
    }
    uint64_t hash = key;
    uint64_t hash_remainder   = hash & BITMASK(metadata_->fingerprint_bits);
    uint64_t hash_bucket_index = hash >> metadata_->fingerprint_bits;

    int64_t start_region;      // For storing the first region to lock/unlock

    // If a query falls into an expanding region, wait until migration is done to the new region. 
    if (GET_NO_LOCK(flags) != kNoLock) {
        start_region = hash_bucket_index / kNumSlotsToLock;

        while (runtimedata_->resize_pending.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        if (GET_SECOND_TRY_LOCK(flags) == kSecondTryLock) {
            while (runtimedata_->resizing_region_upper.load(std::memory_order_acquire) >= 
                   start_region) {
                std::this_thread::yield();
            }
        }
        
        if (!zeno_lock_region_conditionally(start_region, flags)) {
            return 0;
            // hash = adjust_fingerprint_length(hash_bucket_index, hash_remainder);
            // // Now every attempt to lock a region is a second try
            // flags |= kSecondTryLock;
            // start_region = hash_bucket_index / kNumSlotsToLock;
            // if (start_region > runtimedata_->num_locks) {
            //     std::cerr << "error!!!!!" << std::endl;
            //     std::cerr << "start_region:" << start_region << std::endl;
            //     std::cerr << "num_locks   :" << runtimedata_->num_locks << std::endl;
            //     abort();
            // }
            // while (runtimedata_->resizing_region_upper.load(std::memory_order_acquire) >= 
            //        start_region) {
            //     std::this_thread::yield();
            // }
            // if (!zeno_lock_region_conditionally(start_region, flags)) {
            //     return kErrCouldntLock;
            // }
        } else {
            // Successfully locked target region
        }
    }

    if (!is_occupied(hash_bucket_index)) {
        if (GET_NO_LOCK(flags) != kNoLock) {
            zeno_unlock_region(start_region);
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

    // TODO
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;
    do {
        current_end = decode_counter(runstart_index, current_remainder, current_count);
        value = current_remainder & BITMASK(metadata_->value_bits);
        current_remainder = current_remainder >> metadata_->value_bits;
        if (current_remainder == A || current_remainder == B || 
            check_fingerprint(current_remainder, hash_remainder)) {
            if (GET_NO_LOCK(flags) != kNoLock) {
                zeno_unlock_region(start_region);
            }
            return current_count;
        }
        runstart_index = current_end + 1;
    } while (runend_index != current_end);

    if (GET_NO_LOCK(flags) != kNoLock) {
        zeno_unlock_region(start_region);
    }

    return 0;
}

inline
int ZenoFilter::insert1(uint64_t hash, uint8_t flags) {
    int ret_distance = 0;
    uint64_t hash_remainder = hash & BITMASK(metadata_->bits_per_slot);
    uint64_t hash_bucket_index = hash >> metadata_->bits_per_slot;
    uint64_t hash_bucket_block_offset = hash_bucket_index % kSlotsPerBlock;
    int64_t start_region;      // For storing the first region to lock/unlock
    int64_t clusterend_region; // For storing the last region to lock/unlock

    if (GET_NO_LOCK(flags) != kNoLock) {
        clusterend_region = hash_bucket_index / kNumSlotsToLock;
        start_region = clusterend_region;

        while (runtimedata_->resize_pending.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // std::cout << "(1) Attempting lock on region " << start_region << " ";
        if (!zeno_lock_region_conditionally(start_region, flags)) {
            hash = adjust_fingerprint_length(hash_bucket_index, hash_remainder);
            hash_bucket_block_offset = hash_bucket_index % kSlotsPerBlock;
            // Now every attempt to lock a region is a second try
            flags |= kSecondTryLock;
            clusterend_region = hash_bucket_index / kNumSlotsToLock;
            start_region = clusterend_region;
            // std::cout << "(2) failed.. retrying on region " << start_region << std::endl;
            while (runtimedata_->resizing_region_upper.load(std::memory_order_acquire) >= 
                   clusterend_region) {
                std::this_thread::yield();
            }
            if (!zeno_lock_region_conditionally(clusterend_region, flags)) {
                return kErrCouldntLock;
            }
        } else {
            // std::cout << "(B) " << +(get_block(hash_bucket_index / kSlotsPerBlock)->offset) << std::endl;
        }
    }
    uint64_t esi = 1, pop = 0;
    if (is_empty(hash_bucket_index)) {
        METADATA_WORD(runends, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        set_slot(hash_bucket_index, hash_remainder);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        
        ret_distance = 0;
        // modify_metadata(&metadata_->nelts, 1);
        // modify_metadata(&metadata_->ndistinct_elts, 1);
        // modify_metadata(&metadata_->noccupied_slots, 1);
        // PC
        modify_metadata(metadata_->pc_noccupied_slots, 1);
    } else {
        uint64_t runend_index;
        // INCREMENTAL
        if (GET_NO_LOCK(flags) == kNoLock) runend_index = run_end(hash_bucket_index);
        else {
            if (zeno_lock_cluster(hash_bucket_index, runend_index, clusterend_region, flags) 
                == kErrCouldntLock) { // Regions are unlocked inside
                hash = adjust_fingerprint_length(hash_bucket_index, hash_remainder);
                hash_bucket_block_offset = hash_bucket_index % kSlotsPerBlock;
                flags |= kSecondTryLock;
                clusterend_region = hash_bucket_index / kNumSlotsToLock;
                start_region = clusterend_region;
                while (runtimedata_->resizing_region_upper.load(std::memory_order_acquire) >= 
                    clusterend_region) {
                    std::this_thread::yield();
                }
                // std::cout << "(4) failed locking cluster. retrying on region " << start_region << std::endl;
                if (!zeno_lock_region_conditionally(clusterend_region, flags)) {
                    return kErrCouldntLock;
                }
                // If new index is empty, insert fingerprint and escape
                if (is_empty(hash_bucket_index)) {
                    METADATA_WORD(runends, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
                    set_slot(hash_bucket_index, hash_remainder);
                    METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
                    ret_distance = 0;
                    modify_metadata(metadata_->pc_noccupied_slots, 1);

                    zeno_unlock_region(clusterend_region);
                    return ret_distance;
                }
                // Otherwise, proceed locking subsequent regions belonging to the cluster
                if (zeno_lock_cluster(hash_bucket_index, runend_index, clusterend_region, flags)
                    == kErrCouldntLock) {
                    return kErrCouldntLock;
                }
            } else {
                // std::cout << "(3) successfully locked cluster to " << clusterend_region << std::endl;
            }
        }
        
        // runend_index = run_end(hash_bucket_index);

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
            esi = empty_slot_index;
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
        pop = operation;
        // modify_metadata(&metadata_->nelts, 1);
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        while (true) {
            zeno_unlock_region(clusterend_region);
            if (clusterend_region == start_region) break;
            --clusterend_region;
        }
    }

    return ret_distance;
}

inline
int ZenoFilter::insertN(uint64_t hash, uint64_t count, uint8_t flags) {
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
            ret_distance = runstart_index - hash_bucket_index;
            }
        }
        METADATA_WORD(occupieds, hash_bucket_index) |= 1ULL << (hash_bucket_block_offset % 64);
        
    }

    if (GET_NO_LOCK(flags) != kNoLock) {
        zeno_unlock(hash_bucket_index, /*small*/ false);
    }

    return ret_distance;
}

inline
int ZenoFilter::insert_void_sequence(uint64_t begin, uint64_t end, uint64_t count, uint8_t flags) {
    // TODO
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;
    // Inserting gen 1 void sequence of length 1. Encoding: [BA]
    // This insertion always stays at the very front of any run. 
    if (begin == end) {
        if (GET_NO_LOCK(flags) != kNoLock) {
            if (!zeno_lock(begin, /*small*/ true, flags)) {
                return kErrCouldntLock;
            }
        }

        uint64_t new_values[67];
        int64_t runstart_index = begin == 0
                               ? 0
                               : run_end(begin - 1) + 1;
        bool ret;

        // Empty bucket. Because we are scanning right to left, if a slot has
        // its is_occupied bit not set, we are guaranteed that the slot does not
        // contain any entry from any existing runs. 
        if (!is_occupied(begin)) {
            uint64_t* p = encode_counter(A, 1, &new_values[67]);
            p = encode_counter(B, count, p);

            ret = shift_for_inserts(0,
                                    begin,
                                    runstart_index, 
                                    p, 
                                    &new_values[67] - p,
                                    0);
            METADATA_WORD(occupieds, begin) |= 1ULL << (begin % 64);
            if (!ret) return kErrNoSpace;
            // TODO: increase metadata for number of void entries
        } else {
            uint64_t runend_index = run_end(begin);
            uint64_t current_remainder, current_count;

            uint64_t current_end = decode_counter(runstart_index,
                                                  current_remainder,
                                                  current_count);
            while ((current_remainder != A && current_remainder != B) &&
                   (current_end != runend_index)) {
                    runstart_index = current_end + 1;
                    current_end = decode_counter(runstart_index,
                                                 current_remainder,
                                                 current_count);
            }

            // Found an A sequence from gen N. Add in the new count.
            if (current_remainder == A) {
                uint64_t* p = encode_counter(A, current_count + 1,
                                             &new_values[67]);
                p = encode_counter(B, count, p);
                ret = shift_for_inserts(is_runend(current_end) ? 1 : 2, 
                                        begin,
                                        runstart_index, 
                                        p, 
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
            
            // Found a B sequence from gen N. Push the gen N sequence. 
            } else if (current_remainder == B) {
                uint64_t* p = encode_counter(A, 1, &new_values[67]);
                p = encode_counter(B, count, p);
                ret = shift_for_inserts(2, /* Insert to bucket */
                                        begin,
                                        runstart_index, 
                                        p, 
                                        &new_values[67] - p,
                                        0);
            // We did not encounter any void sequences. Insert gen 1 sequence at
            // the end of the run
            } else {
                uint64_t* p = encode_counter(A, 1, &new_values[67]);
                p = encode_counter(B, count, p);
                ret = shift_for_inserts(1, 
                                        begin, 
                                        current_end + 1,
                                        p,
                                        &new_values[67] - p,
                                        0);
            }
            
            
        }
    
    // Inserting gen N (N>1) void sequence. Encoding: [A][B]..[B][A]
    } else {
        if (GET_NO_LOCK(flags) != kNoLock) {
            if (!zeno_lock(begin, /*small*/ true, flags) ||
                !zeno_lock(end, /*small*/ true, flags)) {
                return kErrCouldntLock;
            }
        }

        bool ret;
        uint64_t runstart_index = begin == 0
                                ? 0
                                : run_end(begin - 1) + 1;

        // TODO: fix ret logic
        ret = insert_A_internal(begin, runstart_index, count);
        runstart_index = run_end(begin) + 1;
        for (uint64_t i = begin + 1; i < end; ++i) {
            ret = insert_B_internal(i, runstart_index, count);
            runstart_index = run_end(i) + 1;
        }
        ret = insert_A_internal(end, runstart_index, count);

    }

    return 0;
}

inline
int ZenoFilter::insert_A_internal(uint64_t& hash_index, uint64_t& runstart_index, uint64_t count) {
    // TODO
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;
    
    uint64_t new_values[67];
    bool ret;
    if (!is_occupied(hash_index)) {
        if (count == 1) {
            return insert1((hash_index << metadata_->fingerprint_bits) | A, kNoLock | kKeyIsHash);
        } else {
            return insertN((hash_index << metadata_->fingerprint_bits) | A, count, kNoLock | kKeyIsHash);
        }
        // TODO: increase metadata for number of void entries
    } else {
        uint64_t runend_index = run_end(hash_index);
        uint64_t current_remainder, current_count;

        uint64_t current_end = decode_counter(runstart_index,
                                              current_remainder,
                                              current_count);
        while ((current_remainder != A && current_remainder != B) &&
                (current_end != runend_index)) {
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index,
                                             current_remainder,
                                             current_count);
        }

        // Found an A sequence from gen N. Add in the new count.
        if (current_remainder == A) {
            uint64_t* p = encode_counter(A, current_count + count, &new_values[67]);
            ret = shift_for_inserts(current_end == runend_index ? 1 : 2, 
                                    hash_index,
                                    runstart_index, 
                                    p, 
                                    &new_values[67] - p,
                                    current_end - runstart_index + 1);
        
        // Found a B sequence. This is either a gen 1 seq or a middle seq of gen N. 
        } else if (current_remainder == B) {
            // gen 1 seq. There is always a delimiting A. 
            if (current_end != runend_index) {
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index,
                                             current_remainder,
                                             current_count);

                uint64_t* p = encode_counter(A, current_count + count, &new_values[67]);
                ret = shift_for_inserts(current_end == runend_index ? 1 : 2, 
                                        hash_index,
                                        runstart_index, 
                                        p, 
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
            
            // gen N seq. This is always the end of a run.
            } else {
                assert(current_end == runend_index);
                uint64_t* p = encode_counter(A, count, &new_values[67]);
                ret = shift_for_inserts(1, 
                                        hash_index, 
                                        current_end + 1,
                                        p,
                                        &new_values[67] - p,
                                        0);
            }
        // We did not encounter any void sequences. Insert A sequence at the end
        // of the run
        } else {
            uint64_t* p = encode_counter(A, count, &new_values[67]);
            ret = shift_for_inserts(1, 
                                    hash_index, 
                                    current_end + 1,
                                    p,
                                    &new_values[67] - p,
                                    0);
        }
    }
    return ret;
}

inline
int ZenoFilter::insert_B_internal(uint64_t& hash_index, uint64_t& runstart_index, uint64_t count) {
    // TODO
    uint64_t A = BITMASK(metadata_->fingerprint_bits);
    uint64_t B = A - 1;

    uint64_t new_values[67];
    bool ret;
    if (!is_occupied(hash_index)) {
        if (count == 1) {
            return insert1((hash_index << metadata_->fingerprint_bits) | B, kNoLock | kKeyIsHash);
        } else {
            return insertN((hash_index << metadata_->fingerprint_bits) | B, count, kNoLock | kKeyIsHash);
        }
        // TODO: increase metadata for number of void entries
    } else {
        uint64_t runend_index = run_end(hash_index);
        uint64_t current_remainder, current_count;

        uint64_t current_end = decode_counter(runstart_index,
                                              current_remainder,
                                              current_count);
        while ((current_remainder != A && current_remainder != B) &&
                (current_end != runend_index)) {
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index,
                                             current_remainder,
                                             current_count);
        }

        // Found an A sequence from gen N. Check if there are following Bs.
        if (current_remainder == A) {
            // There are no following Bs; append B at the end.
            if (current_end == runend_index) {
                uint64_t* p = encode_counter(B, count, &new_values[67]);
                ret = shift_for_inserts(1,
                                        hash_index,
                                        current_end + 1,
                                        p,
                                        &new_values[67] - p,
                                        0);
            // There are some following Bs. This is always the end of a run.
            } else {
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index,
                                             current_remainder,
                                             current_count);
                assert(current_end == runend_index);

                uint64_t* p = encode_counter(B, current_count + count, 
                                             &new_values[67]);
                ret = shift_for_inserts(1,
                                        hash_index,
                                        runstart_index,
                                        p,
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
            }
        
        // Found a B sequence. This is either a gen 1 seq or a middle seq of gen N. 
        } else if (current_remainder == B) {
            // gen 1 seq. There is always a delimiting A. 
            if (current_end != runend_index) {
                // Skip over the A sequence
                runstart_index = current_end + 1;
                current_end = decode_counter(runstart_index,
                                             current_remainder,
                                             current_count);
                // There are following Bs
                if (current_end != runend_index) {
                    // Read the B sequence
                    runstart_index = current_end + 1;
                    current_end = decode_counter(runstart_index,
                                                 current_remainder,
                                                 current_count);
                    
                    uint64_t* p = encode_counter(B, current_count + count, &new_values[67]);

                    ret = shift_for_inserts(1,
                                            hash_index,
                                            runstart_index,
                                            p,
                                            &new_values[67] - p,
                                            current_end - runstart_index + 1);

                // This is the first B seq to be inserted after the A seq. This
                // is always the end of a run. 
                } else {
                    uint64_t* p = encode_counter(B, count, &new_values[67]);
                    ret = shift_for_inserts(1,
                                            hash_index,
                                            current_end + 1,
                                            p,
                                            &new_values[67] - p,
                                            0);
                }
                
            // gen N seq. This is always the end of a run. 
            } else {
                uint64_t* p = encode_counter(B, current_count + count, 
                                             &new_values[67]);
                ret = shift_for_inserts(1,
                                        hash_index,
                                        runstart_index,
                                        p,
                                        &new_values[67] - p,
                                        current_end - runstart_index + 1);
            }

        // We did not encounter any void sequences. Insert B sequence at the end
        // of the run
        } else {
            uint64_t* p = encode_counter(B, count, &new_values[67]);
            ret = shift_for_inserts(1, 
                                    hash_index, 
                                    current_end + 1,
                                    p,
                                    &new_values[67] - p,
                                    0);
            runend_index += &new_values[67] - p;
        }
    }
    return ret;
}

inline
int ZenoFilter::remove_longest_internal(uint64_t hash, uint64_t &count, 
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

    if (GET_NO_LOCK(runtime_lock) != kNoLock) {
        zeno_unlock(hash_bucket_index, /*small*/ false);
    }

    return ret_numfreedslots;
}


inline int ZenoFilter::remove_internal(uint64_t hash, uint64_t count, uint8_t runtime_lock) {
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

inline
uint64_t ZenoFilter::calculate_A(const uint64_t& k) const {
    return (1ULL << (k >> 1ULL)) * (2ULL + (k & 1ULL)) - 2ULL;
}

inline
const ZenoFilter::indexABC ZenoFilter::entry_lookup_internal(const uint64_t& index) const {
    const uint64_t j = index + 1;
    uint64_t k = 64 - __builtin_clzll(j) - 1;
    uint64_t floor_k = (k >> 1), mask_up = (1ULL << floor_k) - 1;
    uint64_t ceil_k = ((k + 1) >> 1), mask_low = (1ULL << ceil_k) - 1;
    return indexABC(
        calculate_A(k), ((j >> ceil_k) & mask_up), (j & mask_low)
    );
}

inline bool ZenoFilter::zeno_lock(uint64_t hash_bucket_index, bool small, uint8_t runtime_lock) {
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
        if (region < runtimedata_->resizing_region.load(std::memory_order_acquire)) {
            if (!spin_lock_conditionally(&runtimedata_->locks[region], region, runtimedata_->resizing_region, runtime_lock)) {
                return false;
            }
            if (kNumSlotsToLock - hash_bucket_lock_offset <= kClusterSize) {
                if (!spin_lock_conditionally(&runtimedata_->locks[region + 1], 
                                             region, runtimedata_->resizing_region, runtime_lock)) {
                    spin_unlock(&runtimedata_->locks[region]);
                    return false;
                }
            }
        } else {
            if (runtimedata_->resizing_region.load(std::memory_order_acquire) == -1 || 
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

inline void ZenoFilter::zeno_unlock(uint64_t hash_bucket_index, bool small) {
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

inline bool ZenoFilter::zeno_lock_region(int64_t lock_region_index, uint8_t runtime_lock) {
    if (!spin_lock(&runtimedata_->locks[lock_region_index], runtime_lock)) {
        return false;
    }
    return true;
}

inline void ZenoFilter::zeno_unlock_region(int64_t lock_region_index) {
    spin_unlock(&runtimedata_->locks[lock_region_index]);
}

inline void ZenoFilter::zeno_unlock_range(int64_t start_region, int64_t end_region) {
    while (true) {
        zeno_unlock_region(end_region);
        if (end_region == start_region) break;
        --end_region;
    }
}

inline bool ZenoFilter::zeno_lock_region_conditionally(int64_t lock_region_index, uint8_t runtime_lock) {
    // Read-lock of the spinlock array must be done before calling this method. 
    if (lock_region_index <= runtimedata_->resizing_region.load(std::memory_order_acquire)) {
        if (!spin_lock_conditionally(&runtimedata_->locks[lock_region_index], lock_region_index, 
                                     runtimedata_->resizing_region, runtime_lock)) {
            return false;
        }
    } else {
        if (runtimedata_->resizing_region.load(std::memory_order_acquire) == -1 ||
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

inline uint64_t ZenoFilter::get_slot(const uint64_t& index) const {
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

inline void ZenoFilter::set_slot(const uint64_t& index, const uint64_t& value) {
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

inline uint64_t ZenoFilter::block_offset(const uint64_t& blockidx) const {
	/* If we have extended counters and a 16-bit (or larger) offset field, then 
    we can safely ignore the possibility of overflowing that field. */
	if (sizeof(std::declval<qfblock>().offset) > 1 ||
        get_block(blockidx)->offset < BITMASK(8 * sizeof(std::declval<qfblock>().offset)))
        { return get_block(blockidx)->offset; }
	return run_end(kSlotsPerBlock * blockidx - 1) - kSlotsPerBlock * blockidx + 1;
}

inline uint64_t ZenoFilter::run_end(const uint64_t& hash_bucket_index) const {
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

inline uint64_t ZenoFilter::run_end2(const uint64_t& hash_bucket_index) const {
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

    if (metadata_->quotient_bits == 14 && hash_bucket_index == 63) {
        std::cout << "breaking" << std::endl;
        abort();
    }

    uint64_t runend_index = kSlotsPerBlock * runend_block_index + runend_block_offset;
    if (runend_index < hash_bucket_index)
        return hash_bucket_index;
    else
        return runend_index;
}

inline int ZenoFilter::run_end_threadsafe(const uint64_t& hash_bucket_index, uint64_t& runend_index,
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

inline int64_t ZenoFilter::cluster_end(const uint64_t hash_bucket_index, const uint64_t padding) { 
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

inline int ZenoFilter::cluster_end_threadsafe(const uint64_t& hash_bucket_index, 
                                              uint64_t& runend_index, int64_t& clusterend_region, 
                                              uint64_t& clusterend_index, uint8_t flags) { 
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

inline int ZenoFilter::zeno_lock_cluster(uint64_t index, uint64_t& runend, int64_t& clusterend_region, 
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

    if (runtimedata_->resizing_region.load(std::memory_order_acquire) >= 0 
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

        if (runtimedata_->resizing_region.load(std::memory_order_acquire) >= 0 
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
    if (runtimedata_->resizing_region.load(std::memory_order_acquire) >= 0 
        && GET_SECOND_TRY_LOCK(flags) != kSecondTryLock 
        && clusterend_region - start_region >= 2) {
        zeno_unlock_range(start_region, clusterend_region);
        return kErrCouldntLock;
    }
    return 0;
}

inline int32_t ZenoFilter::offset_lower_bound(const uint64_t& slot_index) const {
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

inline uint64_t ZenoFilter::find_first_empty_slot(uint64_t from) const {
    do {
        int32_t t = offset_lower_bound(from);
        assert(t >= 0);
        if (t == 0)
            break;
        from = from + t;
    } while(1);
    return from;
}

inline void ZenoFilter::shift_remainders(const uint64_t& start_index, const uint64_t& empty_index) {
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

inline void ZenoFilter::shift_slots(int64_t first, uint64_t last, uint64_t distance) {
    if (distance == 1) {
        shift_remainders(first, last + 1);
    } else {
        for (int64_t i = last; i >= first; i--)
            set_slot(i + distance, get_slot(i));
    }
}


inline void ZenoFilter::shift_runends(int64_t first, uint64_t last, uint64_t distance) {
    assert(last < metadata_->xnslots && distance < 64);
    uint64_t first_word = first / 64;
    uint64_t bstart = first % 64;
    uint64_t last_word = (last + distance + 1) / 64;
    uint64_t bend = (last + distance + 1) % 64;

    if (last_word != first_word) {
        // The code in the original RSQF implementation had a weird issue with overwriting parts of 
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
bool ZenoFilter::shift_for_inserts(int operation, 
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
int ZenoFilter::shift_for_deletes(int operation, 
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
uint64_t* ZenoFilter::delete_run(uint64_t canonical_slot, uint64_t& run_length) {
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
uint64_t ZenoFilter::insert_run(uint64_t canonical_slot, uint64_t* buffer, uint64_t run_length) {
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
uint64_t ZenoFilter::count_key_value(uint64_t key, uint64_t value, uint8_t flags) const {
    if (GET_KEY_HASH(flags) != kKeyIsHash) {
        auto hash_mode = get_hashmode();
        if (hash_mode == hashmode::Default) {
            // Use the upper `hash_bit` bits of the hashed result
            key = (MurmurHash64A((void*)&key, sizeof(key), metadata_->seed) >>
                  (64ULL - metadata_->hash_bits));
        } else if (hash_mode == hashmode::Invertible) {
            key = hash_64(key, BITMASK(63));
        }
        sanitize_hash(key);
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

uint64_t* ZenoFilter::encode_counter(uint64_t remainder, uint64_t counter, uint64_t* slots) {
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

uint64_t ZenoFilter::decode_counter(uint64_t index, uint64_t& remainder, uint64_t& count) const {
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

}   // namespace zeno