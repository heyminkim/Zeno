#ifndef LOG_DCF_HPP
#define LOG_DCF_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <optional>

#include "CF.hpp"

namespace baseline_LDCF {

/**
 * A logarithmic dynamic cuckoo filter implementation.
 */
class LogarithmicDynamicCuckooFilter {
public:
    /**
     * Constructor.
     * 
     * @param false_positive_rate The desired false positive rate.
     * @param set_size The expected number of items in the set.
     * @param expected_levels The expected number of levels in the filter.
     */
    LogarithmicDynamicCuckooFilter(double false_positive_rate, std::size_t set_size, std::size_t expected_levels);

    /**
     * Destructor.
     */
    ~LogarithmicDynamicCuckooFilter();

    /**
     * Insert an item into the filter.
     * 
     * @param item The item to insert.
     */
    void insert(const std::string &item);

    /**
     * Check if an item is in the filter.
     * 
     * @param item The item to check.
     * @return True if the item is in the filter, false otherwise.
     */
    [[nodiscard]] bool contains(const std::string &item) const;

    /**
     * Remove an item from the filter.
     * 
     * @param item The item to remove.
     * @return True if the item was removed, false otherwise.
     */
    bool remove(const std::string &item);

    /**
     * Get the filter's size.
     * 
     * @return The number of items in the filter.
     */
    [[nodiscard]] std::size_t size() const { return size_; }

    /**
     * Get the filter's capacity.
     * 
     * @return The maximum number of items the filter can hold.
     */
    [[nodiscard]] std::size_t capacity() const;

private:
    std::size_t size_;

    std::size_t number_of_buckets;
    std::size_t fingerprint_size;

    CuckooFilter* root;

    /**
     * Get the prefix of the fingerprint.
     * 
     * @param fingerprint The fingerprint.
     * @param current_level The current level.
     * @param fingerprintSize The size of the fingerprint.
     * @return The prefix of the fingerprint.
     */
    static bool getPrefix(std::size_t fingerprint, int current_level, std::size_t fingerprintSize);
};

}   // namespace baseline_LDCF

#endif // LOG_DCF_HPP