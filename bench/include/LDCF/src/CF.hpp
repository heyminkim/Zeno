#ifndef CUCKOO_FILTER_HPP
#define CUCKOO_FILTER_HPP

#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>
#include <optional>
#include <string>
#include <iostream>
#include <bitset>

namespace baseline_LDCF {

const int MAX_KICKS = 500;
const double LOAD_FACTOR = 0.935;
const int BUCKET_SIZE = 4;

const int BYTE_SIZE = 8;

/**
 * Bucket item
 * Used to store fingerprints in the filter
 */
struct Bucket {
    char *bit_array;

    // Write fingerprint to the bucket
    void write(std::size_t position, uint32_t fingerprint, std::size_t fingerprint_size) const {
        std::size_t bit_offset = position * fingerprint_size;
        std::size_t byte_offset = bit_offset / BYTE_SIZE;
        bit_offset %= BYTE_SIZE;

        uint64_t mask = (1ULL << fingerprint_size) - 1;
        fingerprint &= mask;

        auto *target = reinterpret_cast<uint64_t*>(bit_array + byte_offset);
        *target &= ~(mask << bit_offset);
        *target |= (static_cast<uint64_t>(fingerprint) << bit_offset);
    }

    // Read fingerprint from the bucket
    [[nodiscard]] uint32_t read(std::size_t position, std::size_t fingerprint_size) const {
        std::size_t bit_offset = position * fingerprint_size;
        std::size_t byte_offset = bit_offset / BYTE_SIZE;
        bit_offset %= BYTE_SIZE;

        uint64_t mask = (1ULL << fingerprint_size) - 1;

        const auto *target = reinterpret_cast<const uint64_t*>(bit_array + byte_offset);
        uint64_t fingerprint = (*target >> bit_offset) & mask;

        return static_cast<uint32_t>(fingerprint);
    }
};

/**
 * Victim item
 * Represents a victim item in the filter -> an item that was kicked out during insertion
 */
struct Victim {
    uint32_t fingerprint;
    uint32_t index;
};

/**
 * Cuckoo Filter implementation
 */
class CuckooFilter {
public:

    int current_level;
    uint32_t common_bits_;

    CuckooFilter* child0;
    CuckooFilter* child1;

    /**
     * Constructor
     * @param number_of_buckets Number of buckets in the filter
     * @param fingerprint_size Size of the fingerprint in bits
     */
    CuckooFilter(std::size_t number_of_buckets,  std::size_t fingerprint_size, int current_level, uint32_t common_bits);

    /**
     * Destructor
     */
    ~CuckooFilter();

    /**
     * Copy constructor
     * @param other The CuckooFilter object to copy from
     */
    CuckooFilter(const CuckooFilter& other);

    /**
     * Copy assignment operator
     * @param other The CuckooFilter object to assign from
     * @return Reference to the assigned CuckooFilter object
     */
    CuckooFilter& operator=(const CuckooFilter& other);

    /**
     * Move constructor
     * @param other The CuckooFilter object to move from
     */
    CuckooFilter(CuckooFilter&& other) noexcept ;

    /**
     * Move assignment operator
     * @param other The CuckooFilter object to move assign from
     * @return Reference to the moved CuckooFilter object
     */
    CuckooFilter& operator=(CuckooFilter&& other) noexcept ;

    /**
     * Insert an item into the filter
     * @param item Item to insert
     * @return std::nullopt if the filter is full, otherwise the victim item and its index
     */
    std::optional<Victim> insert(const std::string &item, std::optional<uint32_t> given_fingerprint = std::nullopt);

    /**
     * Check if an item is in the filter
     * @param victim victim to insert
     */
    void insert(const Victim &victim);

    /**
     * Inserts an item into the filter given the index. 
     */
    void insert_with_index(uint32_t index, uint32_t fingerprint);

    /**
     * Creates two new CFs and distributes its entries to them. 
     */
    void split(uint64_t lookup_fingerprint = 0);

    /**
     * Check if an item is in the filter
     * @param item Item to check
     * @return True if the item is in the filter, false otherwise
     */
    [[nodiscard]] bool contains(const std::string &item, std::optional<uint32_t> given_fingerprint = std::nullopt) const;

    /**
     * Remove an item from the filter
     * @param item Item to remove
     * @return True if the item was removed, false otherwise
     */
    bool remove(const std::string &item, std::optional<uint32_t> given_fingerprint = std::nullopt);

    /**
     * Get the filter's size
     * @return The number of items in the filter
     */
    [[nodiscard]] std::size_t size() const;

    /**
     * Get the filter's capacity
     * @return The maximum number of items the filter can hold
     */
    [[nodiscard]] std::size_t capacity() const;

    /**
     * Get the filter's fingerprint size
     * @return The size of the fingerprint in bits
     */
    [[nodiscard]] std::size_t getFingerprintSize() const { return fingerprint_size; }

    /**
     * Check if the filter is full
     * @return True if the filter is full, false otherwise
     */
    [[nodiscard]] bool isFull() const;

    /**
     * Accept values
     * @param accept True if the filter should accept values, false otherwise
     */
    void acceptValues(bool accept) { accept_values = accept; }

    /**
     * Hash a string
     * @param item The string to hash
     * @return The hash of the string
     */
    static std::size_t hash(const std::string& item) ;

    /**
     * Hash a size_t
     * @param item The size_t to hash
     * @return The hash of the size_t
     */
    static std::size_t hash(std::size_t item) ;

    void print_state(int num_tabs) const {
        std::string tabs(num_tabs, '\t');
        std::cout << tabs << "========" << std::endl;
        std::cout << tabs << "curr level " << current_level << std::endl;
        std::cout << tabs << "common bit " << std::bitset<8>(common_bits_) << std::endl;
        std::cout << tabs << (accept_values ? "not full" : "full") << std::endl;
        if (child0) child0->print_state(num_tabs + 1);
        if (child1) child1->print_state(num_tabs + 1);
    }

private:
    std::size_t number_of_buckets;
    std::size_t fingerprint_size;
    std::size_t max_kicks;
    std::size_t current_size;

    bool accept_values;

    std::vector<Bucket> buckets;

    std::vector<bool> full_slots; 

    /**
     * Get next power of two
     * @param n The number to get the next power of two for
     * @return The next power of two
     */
    static std::size_t nextPowerOfTwo(std::size_t n) ;
};

}   // namespace baseline_LDCF

#endif // CUCKOO_FILTER_HPP