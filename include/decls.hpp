#pragma once

#include <cstdint>

namespace zeno {

#define MAX_VALUE(nbits) ((1ULL << (nbits)) - 1)
#define BITMASK(nbits) ((nbits) == 64 ? 0XFFFFFFFFFFFFFFFF : MAX_VALUE(nbits))
#define METADATA_WORD(field, slot_index) (get_block((slot_index) / \
             kSlotsPerBlock)->field[((slot_index) % kSlotsPerBlock) / 64])
#define GET_NO_LOCK(flag) (flag & kNoLock)
#define GET_TRY_ONCE_LOCK(flag) (flag & kTryOnceLock)
#define GET_WAIT_FOR_LOCK(flag) (flag & kWaitForLock)
#define GET_KEY_HASH(flag) (flag & kKeyIsHash)
#define GET_SECOND_TRY_LOCK(flag) (flag & kSecondTryLock)
#define GET_IS_FILTER_GROWING(flag) (flag & kIsFilterGrowing)
#define REMAINDER_WORD(i) \
    (reinterpret_cast<uint64_t*>(&(get_block((i) / \
    metadata_->bits_per_slot)->slots[8 * ((i) % metadata_->bits_per_slot)])))

// Zeno filter supports three hashing modes:

// - DEFAULT uses a hash that may introduce false positives, but this can be useful when inserting 
// large keys that need to be hashed down to a small fingerprint. With this type of hash, you can 
// iterate over the hash values of all the keys in the filter, but you cannot iterate over the keys 
// themselves.

// - INVERTIBLE has no false positives, but the size of the hash output must be the same as the size 
// of the hash input, e.g. 17-bit keys hashed to 17-bit outputs. So this mode is generally only 
// useful when storing small keys in the filter. With this hashing mode, you can use iterators to 
// enumerate both all the hashes in the filter, or all the keys.

// - NONE, for when you've done the hashing yourself. WARNING: Zeno filter can exhibit very bad 
// performance if you insert a skewed distribution of inputs.
enum class hashmode {
    Default,
    Invertible,     // Deprecated: do not use.
    None
};  // class hashmode

// Zeno filter supports concurrent insertions and queries. Only the portion of Zeno filter being 
// examined or modified is locked, so it supports high throughput even with many threads.

// Zeno filter operations support 3 locking modes:
// - kNoLock: for single-threaded applications or applications that do their own concurrency 
// management.
// - kTryOnceLock: If you can't grab the lock on the first try, return with an error code.
// - kWaitForLock: Spin until you get the lock, then do the query or update.
// kSecondTryLock is for concurrent insertions while expansion. This is to indicate that this 
// insertion is to an expanded index, and should wait for the spin lock. It does not fall into
// spin_lock_conditionally(). 
static constexpr uint32_t kNoLock           = 0x1;
static constexpr uint32_t kTryOnceLock      = 0x2;
static constexpr uint32_t kWaitForLock      = 0x4;
static constexpr uint32_t kKeyIsHash        = 0x8;
static constexpr uint32_t kSecondTryLock    = 0x10;
static constexpr uint32_t kDummy            = 0x20;
static constexpr uint32_t kIsFilterGrowing  = 0x40;

// Status codes. 
static constexpr int32_t kErrNoSpace     = -1;
static constexpr int32_t kErrCouldntLock = -2;
static constexpr int32_t kErrDoesntExist = -3;
static constexpr int32_t kErrNoFpBits    = -4;

}   // namespace zeno