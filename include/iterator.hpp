#pragma once

#include <cstdint>
#include <limits>

#include "rs.hpp"

namespace zeno {

// Status codes of iterator. 
static constexpr int32_t kIteratorInvalid = -4;

// Indicator for backwards iterator. 
static constexpr uint64_t kMaxPosition = std::numeric_limits<uint64_t>::max();

/**
 * A bidirectional iterator over Zeno filter. 
 */
template <class T>
class iterator {
    public:
    iterator() = delete;
    iterator(T* filter, uint64_t position);
    iterator& operator=(const iterator& other) {
        filter_ = other.filter_;
        canonical_slot_ = other.canonical_slot_;
        current_ = other.current_;
        runend_ = other.runend_;
        current_block_ = other.current_block_;
        current_region_ = other.current_region_;
        num_slots_to_lock_ = other.num_slots_to_lock_;
        is_valid_ = other.is_valid_;
        is_new_region_ = other.is_new_region_;
        return *this;
    }
    iterator& operator=(iterator&& other) = default;
    iterator& operator++();

    ~iterator() = default;

    inline
    uint64_t get_canonical_slot() const {
        return canonical_slot_;
    }

    inline
    uint64_t get_run_length() const {
        return runend_ - current_ + 1;
    }

    /**
     * Returns the current slot the iterator is in. If the run has not been shifted to the right,
     * the returned value is equal to canonical_slot_. Otherwise, current_ is larger. 
     */
    inline
    uint64_t get_current_slot() const {
        return current_;
    }

    inline
    bool is_valid() const {
        return is_valid_;
    }

    inline
    bool is_end() const {
        if (current_ >= filter_->metadata_->xnslots) return true;
        return false;
    }

    inline
    bool is_occupied() const {
        if (filter_->is_occupied(canonical_slot_)) return true;
        return false;
    }

    /**
     * Disables the concurrency mechanism when the filter is not multithreading. 
     */
    inline
    void disable_region() {
        current_region_ = -1;
    }

    void print() const {
        std::cout << std::endl;
        std::cout << " === Printing iterator stats... ===" << std::endl;
        std::cout << "canonical slot : " << canonical_slot_ << std::endl;
        std::cout << "current slot : " << current_ << std::endl;
        std::cout << "runend slot : " << runend_ << std::endl;
    }

    /**
     * Retrieves the fingerprint, value, and the count of a slot from current
     * index specified as `current_`. 
     * @param fingerprint Stores the retrieved fingerprint.
     * @param value Stores the retrieved value.
     * @param count Stores the retrieved count.
     * @returns 0, if the iterator is still valid. kIteratorInvalid, otherwise.
     */
    int32_t get_entry(uint64_t& fingerprint, uint64_t& value, uint64_t& count);

    /**
     * Retrieves the original hash, value, and the count of a slot from current
     * index specified as `current_`. This appends the canonical slot address
     * and the fingerprint to reconstruct the hash. 
     * @param hash Stores the reconstructed hash.
     * @param value Stores the retrieved value.
     * @param count Stores the retrieved count.
     * @returns 0, if the iterator is still valid. kIteratorInvalid, otherwise.
     */
    int32_t get_hash(uint64_t& hash, uint64_t& value, uint64_t& count);

    private:
    friend T;

    /**
     * Initializes an empty iterator simply associated with a filter. In order to make a backwards
     * iterator, this must be followed with get_last_canonical_slot.
     */
    iterator(T* filter) {
        filter_ = filter;
    }
    
    // Intentionally not constant, because backwards iteration may remove keys
    // and values from the filter. 
    T* filter_;
    uint64_t canonical_slot_ = 0;       // Canonical slot of current run
    uint64_t current_ = 0;              // Current slot in the run
    uint64_t runend_ = 0;               // Runend slot of current run
    uint64_t current_block_ = 0;        // Current block being iterated
    int64_t current_region_ = 0;       // Current spinlock region being iterated
    uint64_t num_slots_to_lock_ = 0;    // Region size of the filter
    bool is_valid_ = true;              // Is the iterator valid?
    bool is_new_region_ = false;         // Has the iterator reached a new region?

    /**
     * Implementation of the backwards iteration. Because the backwards iterator
     * assumes deletion of the key that it is pointing to for proper operation,
     * it is not exposed as a public interface. 
     */
    iterator& operator--();

    /**
     * Retrieves the index of the last canonical slot and updates the metadata
     * of this iterator accordingly. 
     * @param block_index The index of the block to be examined.
     * @returns True, if the block is populated and could successfuly find the
     * last canonical slot. False, if the search has reached the first block and
     * cannot proceed any further. 
     */
    bool get_last_canonical_slot(uint64_t block_index);

    bool is_new_region() const {
        return is_new_region_;
    }

    /**
     * Invalidates the iterator. 
     * Originally made an invalid combination of current_ and runend_, now just marks the flag false
     */
    void invalidate() {
        is_valid_ = false;
    }

    /**
     * Marks the iterator has arrived at a new region. 
     */
    void mark_new_region() {
        is_new_region_ = true;
    }

    /**
     * Unmarks the is_new_region_ flag
     */
    void unmark_new_region() {
        is_new_region_ = false;
    }
};  // class iterator

template <class T>
inline bool iterator<T>::get_last_canonical_slot(uint64_t block_index) {
    uint64_t occupieds = filter_->get_block(block_index)->occupieds[0];
    uint64_t num_canonical_slots = __builtin_popcountll(occupieds);

    while ((!num_canonical_slots) && (block_index != 0)) {
        --block_index;
        occupieds = filter_->get_block(block_index)->occupieds[0];
        num_canonical_slots = __builtin_popcountll(occupieds);
    }

    // We have reached the first block and it has no canonical slots; we cannot proceed any further. 
    if (!num_canonical_slots) {
        // Make an invalid combination of `current_` and `runend_` to mark this iterator as invalid. 
        invalidate();
        return false;
    }

    canonical_slot_ = block_index * filter_->kSlotsPerBlock + 
                      bitselect(occupieds, num_canonical_slots - 1);

    current_ = canonical_slot_ == 0 
             ? 0 
             : filter_->run_end(canonical_slot_ - 1) + 1;

    runend_ = filter_->run_end(canonical_slot_);

    current_block_ = canonical_slot_ / filter_->kSlotsPerBlock;

    return true;
}

template <class T>
inline iterator<T>::iterator(T* filter, uint64_t position) {
    filter_ = filter;
    num_slots_to_lock_ = filter_->kNumSlotsToLock;
    is_valid_ = true;
    is_new_region_ = false;

    if (position == kMaxPosition) {
        // Index of the last 'valid' slot
        uint64_t index = filter_->metadata_->xnslots - 1;
        uint64_t last_block = index / filter_->kSlotsPerBlock;

        uint64_t occupieds = filter_->get_block(last_block)->occupieds[0];

        if (!get_last_canonical_slot(last_block)) {
            // If iterator creation fails, the caller must first check the
            // validity of this iterator. 
            invalidate();
        }
        current_region_ = canonical_slot_ / num_slots_to_lock_;
    } else {
        assert(position < filter_->metadata_->xnslots);
        if (!filter_->is_occupied(position)) {
            uint64_t block_index = position / filter_->kSlotsPerBlock;
            uint64_t rank_idx = bitrank(filter_->get_block(block_index)->occupieds[0], 
                                        position % filter_->kSlotsPerBlock);
            uint64_t idx = bitselect(filter_->get_block(block_index)->occupieds[0], rank_idx);
            if (idx == 64) {
                while (idx == 64 && block_index < filter_->metadata_->nblocks) {
                    ++block_index;
                    idx = bitselect(filter_->get_block(block_index)->occupieds[0], 0);
                }
            }
            position = block_index * filter_->kSlotsPerBlock + idx;
        }

        canonical_slot_ = position;
        runend_ = filter_->run_end(canonical_slot_);
        current_ = position == 0 ? 0 : filter_->run_end(position - 1) + 1;
        current_block_ = canonical_slot_ / filter_->kSlotsPerBlock;
        if (current_ < position) current_ = position;
        current_region_ = canonical_slot_ / num_slots_to_lock_;
    }
}

template <class T>
inline iterator<T>& iterator<T>::operator++() {
    uint64_t current_remainder, current_count;
    current_ = filter_->decode_counter(current_, current_remainder, current_count);
    if (!filter_->is_runend(current_)) {
        ++current_;

        if (is_end()) {
            invalidate();
            return *this;
        }
    } else {
        uint64_t rank = bitrank(filter_->get_block(current_block_)->occupieds[0], canonical_slot_);
        uint64_t next_run = bitselect(filter_->get_block(current_block_)->occupieds[0], rank);

        if (next_run == 64) {
            rank = 0;
            while (next_run == 64 && current_block_ < filter_->metadata_->nblocks) {
                ++current_block_;
                next_run = bitselect(filter_->get_block(current_block_)->occupieds[0], rank);
            }
        }

        if (current_block_ == filter_->metadata_->nblocks) {
            invalidate();
            return *this;
        }

        canonical_slot_ = current_block_ * filter_->kSlotsPerBlock + next_run;
        ++current_;
        if (current_ < canonical_slot_) {
            current_ = canonical_slot_;
        }
        runend_ = filter_->run_end(canonical_slot_);
    }

    return *this;
}

template <class T>
inline iterator<T>& iterator<T>::operator--() {
    // This run has been deleted. Move on to the next block or invalidate iterator. 
    if (!filter_->is_occupied(canonical_slot_)) {
        uint64_t occupieds = filter_->get_block(current_block_)->occupieds[0];
        uint64_t intrablock_offset = canonical_slot_ % filter_->kSlotsPerBlock;
        int rank = bitrank(occupieds, intrablock_offset);

        if (rank == 0) {
            if (current_block_ == 0) {
                invalidate();
                return *this;
            }
            --current_block_;

            /**
             * Mark that the iterator has fallen into a new qfblock. If there is no lock, proceed
             * with get_last_canonical_slot. If there is a lock and the filter is expanding 
             * concurrently, the caller must check if it has fallen into a new region. Then it may
             * proceed with get_last_canonical_slot.
             */
            int64_t maybe_new_region = current_block_ * filter_->kSlotsPerBlock / num_slots_to_lock_;
            if (maybe_new_region < current_region_) {
                current_region_ = maybe_new_region;
                mark_new_region();
                return *this;
            }

            /**
             * If this is not a new region but only a new block, proceed as normal. 
             */
            // Could not find any valid canonical slots
            if (!get_last_canonical_slot(current_block_)) {
                return *this;
            } else {
                // Successfully found valid canonical slot and updated metadata. 
            }
        } else {
            // rank - 1 because one "must have been deleted". This may not hold true if the 
            // backwards iterator is used for some other purpose other than deletion. 
            canonical_slot_ = current_block_ * filter_->kSlotsPerBlock +
                                bitselect(occupieds, rank - 1);
            current_ = canonical_slot_ == 0 
                        ? 0 
                        : filter_->run_end(canonical_slot_ - 1) + 1;
            
            runend_ = filter_->run_end(canonical_slot_);
        }
        
    } else {
        // Entry in the same run is deleted but run is not deleted. The iterator stays in its 
        // current position but `runend_` is updated accordingly. 
        if (runend_ != filter_->run_end(canonical_slot_)) {
            runend_ = filter_->run_end(canonical_slot_);
        
        /**
         * There is a cyclic insertion. A cyclic insertion happens in fractional expansion, when the 
         * new index is equal to the previous index because the multiplicative ratio is too small to
         * change the index. We find there is a cyclic insertion by realizing that, after deletion 
         * and migration of a previous key, the previous runend and the new runend are equivalent. 
         * Since all other entries in the run will stay in the same slot, we skip over the entire 
         * run. Note that cyclic insertion may only happen between epochs and never between periods, 
         * unless it is slot 0. This allows us to skip through the entire run. 
         */
        
        } else {
            uint64_t occupieds = filter_->get_block(current_block_)->occupieds[0];
            uint64_t intrablock_offset = canonical_slot_ % filter_->kSlotsPerBlock;
            int rank = bitrank(occupieds, intrablock_offset);

            // There is cyclic insertion at the first run. 
            if (rank == 1) {
                // There are no more blocks to be iterated over.
                if (current_block_ == 0) {
                    invalidate();
                    return *this;
                }
                --current_block_;
                // Could not find any valid canonical slots.
                if (!get_last_canonical_slot(current_block_)) {
                    return *this;
                } else {
                    // Successfully found valid canonical slot and updated metadata. 
                }
            } else {
                canonical_slot_ = current_block_ * filter_->kSlotsPerBlock +
                                  bitselect(occupieds, rank - 2);
                current_ = canonical_slot_ == 0
                         ? 0
                         : filter_->run_end(canonical_slot_ - 1) + 1;
                runend_ = filter_->run_end(canonical_slot_);
            }
        }
    }

    return *this;
}

template <class T>
inline int32_t iterator<T>::get_entry(uint64_t& fingerprint, uint64_t& value, uint64_t& count) {
    fingerprint = value = count = 0;
    uint64_t remainder;
    filter_->decode_counter(current_, remainder, count);

    value = remainder & BITMASK(filter_->metadata_->value_bits);
    fingerprint = remainder >> filter_->metadata_->value_bits;

    return 0;
}

template <class T>
inline int32_t iterator<T>::get_hash(uint64_t& hash, uint64_t& value, uint64_t& count) {
    get_entry(hash, value, count);
    hash = canonical_slot_ << filter_->get_bits_per_slot() | hash;

    return 0;
}

}   // namespace zeno