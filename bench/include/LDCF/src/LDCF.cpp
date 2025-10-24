#include <cstddef>
#include <cstdint>
#include <iostream>
#include <bitset>   
#include <cmath>
#include <sys/types.h>
#include <bitset>

#include "CF.hpp"
#include "LDCF.hpp"

namespace baseline_LDCF {

// Constructor
LogarithmicDynamicCuckooFilter::LogarithmicDynamicCuckooFilter(double false_positive_rate, std::size_t set_size, std::size_t expected_levels):
    size_(0) {
    number_of_buckets = set_size / (BUCKET_SIZE * expected_levels);
    auto single_CF_capacity = LOAD_FACTOR * number_of_buckets * BUCKET_SIZE;
    double b_2 = 2 * 4;

    // std::cout << std::endl << " ===== allocating filter =====" << std::endl;
    // std::cout << "fpr : " << false_positive_rate << std::endl;

    auto single_false_positive_rate = 1 - pow(1 - false_positive_rate, single_CF_capacity / set_size);
    this->fingerprint_size = log2(b_2/single_false_positive_rate);
    this->fingerprint_size = ceil(this->fingerprint_size + expected_levels);
    if (this->fingerprint_size > BYTE_SIZE * 4) {
        this->fingerprint_size = BYTE_SIZE * 4; // max fingerprint size
    }
    // std::cout << "fingerprint_size " << this->fingerprint_size << std::endl;
    // std::cout << "number_of_buckets " << this->number_of_buckets << std::endl;

    root = new CuckooFilter(number_of_buckets, this->fingerprint_size, 0, 0);
}

// Destructor
LogarithmicDynamicCuckooFilter::~LogarithmicDynamicCuckooFilter() {
    delete root;
}

// Insert an item into the filter
bool LogarithmicDynamicCuckooFilter::insert(const std::string &item) {
    int current_level = 0;
    auto *current_CF = root;
    uint32_t fingerprint = CuckooFilter::hash(item);
    // fingerprint = fingerprint & ((1 << current_CF->getFingerprintSize()) - 1);

    while (current_CF->isFull()) {
        if (getPrefix(fingerprint, current_level, current_CF->getFingerprintSize())) {
            // if (current_CF->child0 == nullptr) {
            //     current_CF->child0 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_);
            // }
            if (current_CF->child0) current_CF = current_CF->child0;
            else {
                if (fingerprint_size == current_level + 1) return false;
                current_CF->child0 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_);
                current_CF->child1 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_ + (1UL << current_level));
                current_CF->split();
            }
        } else {
            // if (current_CF->child1 == nullptr) {
            //     current_CF->child1 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_ + (1UL << current_level));
            // }
            if (current_CF->child1) current_CF = current_CF->child1;
            else {
                if (fingerprint_size == current_level + 1) return false;
                current_CF->child0 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_);
                current_CF->child1 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_ + (1UL << current_level));
                current_CF->split();
            }
        }
        current_level++;
    }

    auto victim = current_CF->insert(item, fingerprint);
    // victim is not empty - there is an overflow and we must allocate new CFs. 
    if (victim.has_value()) {
        // std::cout << current_CF->current_size << std::endl;
        if (fingerprint_size == current_level + 1) return false;
        current_CF->child0 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_);
        current_CF->child1 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_ + (1UL << current_level));
        if (getPrefix(victim->fingerprint, current_level, current_CF->getFingerprintSize())) {
            current_CF->child0->insert(victim.value());
        } else {
            current_CF->child1->insert(victim.value());
        }
        // std::cout << "orig fp is  " << std::bitset<64>(fingerprint) << std::endl;
        // std::cout << "kicking out " << std::bitset<64>(victim->fingerprint) << std::endl;
        current_CF->split(fingerprint);
    } else if (current_CF->isFull()) {
        if (fingerprint_size == current_level + 1) return false;
        current_CF->child0 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_);
        current_CF->child1 = new CuckooFilter(number_of_buckets, fingerprint_size, current_level + 1, current_CF->common_bits_ + (1UL << current_level));
        current_CF->split();
    }
    // There should be a fallback for when the second insertion fails, but not implemented
    size_++;
    return true;
}

// Check if an item is in the filter
bool LogarithmicDynamicCuckooFilter::contains(const std::string &item) const {
    CuckooFilter *current_CF = root;
    int current_level = 0;
    uint32_t fingerprint = CuckooFilter::hash(item);
    fingerprint = fingerprint & ((1 << current_CF->getFingerprintSize()) - 1);
    while (true) {
        if (!current_CF->isFull()) {
            if (current_CF->contains(item, fingerprint)) {
                return true;
            } else {
                // std::cout << "CLEVEL " << current_CF->current_level << std::endl;
                // std::cout << "<1>" << std::endl;
                // root->print_state(0);
                return false;
            }
        }
        if (getPrefix(fingerprint, current_level, current_CF->getFingerprintSize())) {
            if (current_CF->child0 == nullptr) {
                // std::cout << "<2>" << std::endl;
                // root->print_state(0);
                return false;
            }
            // std::cout << "moved to 0" << std::endl;
            current_CF = current_CF->child0;
        } else {
            if (current_CF->child1 == nullptr) {
                // std::cout << "<3>" << std::endl;
                // root->print_state(0);
                return false;
            }
            // std::cout << "moved to 1" << std::endl;
            current_CF = current_CF->child1;
        }
        current_level++;
    }
}

// Remove an item from the filter
bool LogarithmicDynamicCuckooFilter::remove(const std::string &item) {
    CuckooFilter *current_CF = root;
    int current_level = 0;
    uint32_t fingerprint = CuckooFilter::hash(item);
    fingerprint = fingerprint & ((1 << current_CF->getFingerprintSize()) - 1);
    while (true) {
        if (current_CF->contains(item, fingerprint)) {
            size_--;
            current_CF->acceptValues(true);
            return current_CF->remove(item, fingerprint);
        }
        if (getPrefix(fingerprint, current_CF->current_level, current_CF->getFingerprintSize())) {
            if (current_CF->child0 == nullptr) {
                return false;
            }
            current_CF = current_CF->child0;
        } else {
            if (current_CF->child1 == nullptr) {
                return false;
            }
            current_CF = current_CF->child1;
        }
        current_level++;
    }
}

bool LogarithmicDynamicCuckooFilter::getPrefix(std::size_t fingerprint, int current_level, std::size_t fingerprintSize) {
    // put the one to the position of the current level
    uint32_t mask = 1 << current_level;
    return (fingerprint & mask) == 0;
}

}   // namespace baseline_LDCF