#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string>
#include <cstring>
#include <iostream>

#include  "CF.hpp"

namespace baseline_LDCF {

// Constructor
CuckooFilter::CuckooFilter(std::size_t number_of_buckets, std::size_t fingerprint_size, int current_level, uint32_t common_bits):
    child0(nullptr), child1(nullptr), number_of_buckets(nextPowerOfTwo(number_of_buckets)), fingerprint_size(fingerprint_size), 
    current_size(0), accept_values(true), current_level(current_level), common_bits_(common_bits) {
        buckets.reserve(this->number_of_buckets);
        
        if (this->fingerprint_size < 1) {
            this->fingerprint_size = 1;
        }

        full_slots.reserve(this->number_of_buckets * BUCKET_SIZE);

        // Initialize the empty slots to false
        for (std::size_t i = 0; i < this->number_of_buckets * BUCKET_SIZE; i++) {
            full_slots.push_back(false);
        }

        auto bits_per_bucket = BUCKET_SIZE * (this->fingerprint_size - current_level); 
        
        // NOLINTNEXTLINE
        auto bytes_per_bucket = (bits_per_bucket + 7) >> 3;
        if (bytes_per_bucket == 0) {
            bytes_per_bucket = 1;
        }
        for (std::size_t i = 0; i < this->number_of_buckets; i++) {
            // NOLINTNEXTLINE
            buckets[i].bit_array = new char[bytes_per_bucket]; // Allocate memory for the bucket
            // 0 out the bucket
            memset(buckets[i].bit_array, 0, bytes_per_bucket);
        }
    }

// Destructor
CuckooFilter::~CuckooFilter() {
    delete child0; 
    delete child1;
    
    for (std::size_t i = 0; i < number_of_buckets; ++i) {
        // NOLINTNEXTLINE
        delete[] buckets[i].bit_array;
    }
}

// Insert an item into the filter
std::optional<Victim> CuckooFilter::insert(const std::string &item, std::optional<uint32_t> given_fingerprint) {
    
    if (current_size >= capacity()) {
        return std::nullopt;
    }

    uint32_t index1 = hash(item) % number_of_buckets;
    uint32_t fingerprint;

    // If the fingerprint is given, use it, otherwise generate a new one 
    // -> with this we want to reduce the number of hash calls which are expensive
    if (given_fingerprint.has_value()){
        fingerprint = given_fingerprint.value();
    } else {
        fingerprint = hash(item);
        fingerprint = fingerprint & ((1 << fingerprint_size) - 1);
    }

    uint32_t index2 = (index1 ^ hash(fingerprint)) % number_of_buckets;

    // save f - current_level bits from the fingerprint
    uint32_t saved_bits = fingerprint & ((1 << current_level) - 1);

    // now we take f - current_level bits from the fingerprint
    fingerprint >>= current_level;

    uint32_t index_to_use = index1;

    auto current_level_fingerprint_size = fingerprint_size - current_level;

    // check how many of given fingerprint we already have in the buckets
    std::size_t counter = 0;
    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (full_slots[index1 * BUCKET_SIZE + i]) {
            auto result1 = buckets[index1].read(i, current_level_fingerprint_size);
            if (result1 == fingerprint) {
                counter++;
            }
        }
        if (full_slots[index2 * BUCKET_SIZE + i]) {
            auto result2 = buckets[index2].read(i, current_level_fingerprint_size);
            if (result2 == fingerprint) {
                counter++;
            }
        }
    }

    if (counter >= BUCKET_SIZE) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (!full_slots[index_to_use * BUCKET_SIZE + i]) {
            buckets[index_to_use].write(i, fingerprint, current_level_fingerprint_size);
            full_slots[index_to_use * BUCKET_SIZE + i] = true;
            current_size++;
            return std::nullopt;
        }
    }
    uint32_t index_of_victim = index_to_use;
    for (std::size_t i = 0; i < MAX_KICKS; i++) {
        
        std::size_t bucket_index = rand() % BUCKET_SIZE;
        
        std::uint32_t temp_fingerprint = buckets[index_to_use].read(bucket_index, current_level_fingerprint_size);
        if (i != 0) {
            fingerprint >>= current_level;
        }

        buckets[index_to_use].write(bucket_index, fingerprint, current_level_fingerprint_size);

        fingerprint = temp_fingerprint;

        fingerprint <<= current_level;
        fingerprint |= saved_bits;

        index_of_victim = index_to_use;

        index_to_use = (index_to_use ^ hash(fingerprint)) % number_of_buckets;

        for (std::size_t j = 0; j < BUCKET_SIZE; j++) {
            if (!full_slots[index_to_use * BUCKET_SIZE + j]) {
                fingerprint >>= current_level;
                buckets[index_to_use].write(j, fingerprint, current_level_fingerprint_size);
                full_slots[index_to_use * BUCKET_SIZE + j] = true;
                current_size++;
                return std::nullopt;
            }
        }
    }

    accept_values = false;

    return std::make_optional(Victim{fingerprint, index_of_victim});
}

void CuckooFilter::insert_with_index(uint32_t index, uint32_t fingerprint) {

    uint32_t index1 = index;

    uint32_t index2 = (index1 ^ hash(fingerprint)) % number_of_buckets;

    // save f - current_level bits from the fingerprint
    uint32_t saved_bits = fingerprint & ((1 << current_level) - 1);

    // now we take f - current_level bits from the fingerprint
    fingerprint >>= current_level;

    uint32_t index_to_use = index1;

    auto current_level_fingerprint_size = fingerprint_size - current_level;

    // check how many of given fingerprint we already have in the buckets
    std::size_t counter = 0;
    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (full_slots[index1 * BUCKET_SIZE + i]) {
            auto result1 = buckets[index1].read(i, current_level_fingerprint_size);
            if (result1 == fingerprint) {
                counter++;
            }
        }
        if (full_slots[index2 * BUCKET_SIZE + i]) {
            auto result2 = buckets[index2].read(i, current_level_fingerprint_size);
            if (result2 == fingerprint) {
                counter++;
            }
        }
    }

    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (!full_slots[index_to_use * BUCKET_SIZE + i]) {
            buckets[index_to_use].write(i, fingerprint, current_level_fingerprint_size);
            full_slots[index_to_use * BUCKET_SIZE + i] = true;
            current_size++;
            return;
        }
    }
    uint32_t index_of_victim = index_to_use;
    for (std::size_t i = 0; i < MAX_KICKS; i++) {
        std::size_t bucket_index = rand() % BUCKET_SIZE;
        std::uint32_t temp_fingerprint = buckets[index_to_use].read(bucket_index, current_level_fingerprint_size);
        if (i != 0) {
            fingerprint >>= current_level;
        }

        buckets[index_to_use].write(bucket_index, fingerprint, current_level_fingerprint_size);

        fingerprint = temp_fingerprint;

        fingerprint <<= current_level;
        fingerprint |= saved_bits;

        index_of_victim = index_to_use;

        index_to_use = (index_to_use ^ hash(fingerprint)) % number_of_buckets;

        for (std::size_t j = 0; j < BUCKET_SIZE; j++) {
            if (!full_slots[index_to_use * BUCKET_SIZE + j]) {
                fingerprint >>= current_level;
                buckets[index_to_use].write(j, fingerprint, current_level_fingerprint_size);
                full_slots[index_to_use * BUCKET_SIZE + j] = true;
                current_size++;
                return;
            }
        }
    }

    // We assume the function won't reach here, because the new CF is approx 50% full. 
    accept_values = false;

    return;
}

// Insert victim
void CuckooFilter::insert(const Victim& victim) {
    // now we take f - current_level bits from the fingerprint
    uint32_t fingerprint = victim.fingerprint;

    uint32_t index1 = victim.index;
    fingerprint >>= current_level;

    uint32_t index_to_use = index1;

    auto current_level_fingerprint_size = fingerprint_size - current_level;

    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (!full_slots[index_to_use * BUCKET_SIZE + i]) {
            buckets[index_to_use].write(i, fingerprint, current_level_fingerprint_size);
            full_slots[index_to_use * BUCKET_SIZE + i] = true;
            current_size++;
            return;
        }
    }

    // we should not be here
    throw std::runtime_error("Victim could not be inserted");
}

void CuckooFilter::split(uint64_t lookup_fingerprint) {
    auto current_level_fingerprint_size = fingerprint_size - current_level;
    uint32_t mask = 1 << current_level;
    uint64_t num_split = 0;
    for (std::size_t i = 0; i < full_slots.size(); ++i) {
        // Continue if slot is empty
        if (!full_slots[i]) continue;
        ++num_split;

        uint32_t bucket_index = i / BUCKET_SIZE;
        uint32_t slot_id = i % BUCKET_SIZE;

        uint32_t temp_fingerprint = buckets[bucket_index].read(slot_id, current_level_fingerprint_size);
        temp_fingerprint <<= current_level;
        temp_fingerprint |= common_bits_;

        // child0 and child1 should have been allocated already
        if ((mask & temp_fingerprint) == 0) {
            child0->insert_with_index(bucket_index, temp_fingerprint);
        } else {
            child1->insert_with_index(bucket_index, temp_fingerprint);
        }
    }

    std::vector<bool>().swap(full_slots);
    for (std::size_t i = 0; i < number_of_buckets; ++i) {
        delete[] buckets[i].bit_array;
    }
    std::vector<Bucket>().swap(buckets);
    number_of_buckets = 0;

    accept_values = false;
}

bool CuckooFilter::contains(const std::string &item, std::optional<uint32_t> given_fingerprint) const {
    std::size_t index1 = hash(item) % number_of_buckets;
    uint32_t fingerprint;

    // If the fingerprint is given, use it, otherwise generate a new one
    if (given_fingerprint.has_value()){
        fingerprint = given_fingerprint.value();
    } else {
        fingerprint = hash(item);
        fingerprint = fingerprint & ((1 << fingerprint_size) - 1);
    }

    std::size_t index2 = (index1 ^ hash(fingerprint)) % number_of_buckets;

    // now we take f - current_level bits from the fingerprint
    fingerprint >>= current_level;

    auto current_level_fingerprint_size = fingerprint_size - current_level;

    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        // check if those buckets are empty
        if (full_slots[index1 * BUCKET_SIZE + i]) {
            auto result1 = buckets[index1].read(i, current_level_fingerprint_size);
            if (result1 == fingerprint) {
                return true;
            }
        }
        if (full_slots[index2 * BUCKET_SIZE + i]) {
            auto result2 = buckets[index2].read(i, current_level_fingerprint_size);
            if (result2 == fingerprint) {
                return true;
            }
        }
    }

    return false;
}


bool CuckooFilter::remove(const std::string &item, std::optional<uint32_t> given_fingerprint) {
    std::size_t index1 = hash(item) % number_of_buckets;
    uint32_t fingerprint;

    // If the fingerprint is given, use it, otherwise generate a new one
    if (given_fingerprint.has_value()){
        fingerprint = given_fingerprint.value();
    } else {
        fingerprint = hash(item);
        fingerprint = fingerprint & ((1 << fingerprint_size) - 1);
    }

    std::size_t index2 = (index1 ^ hash(fingerprint)) % number_of_buckets;

    // now we take f - current_level bits from the fingerprint
    fingerprint >>= current_level;

    auto current_level_fingerprint_size = fingerprint_size - current_level;

    for (std::size_t i = 0; i < BUCKET_SIZE; i++) {
        if (buckets[index1].read(i, current_level_fingerprint_size) == fingerprint && full_slots[index1 * BUCKET_SIZE + i]) {
            full_slots[index1 * BUCKET_SIZE + i] = false; // no need to delete the fingerprint
            current_size--;
            return true;
        }
        if (buckets[index2].read(i, current_level_fingerprint_size) == fingerprint && full_slots[index2 * BUCKET_SIZE + i]) {
            full_slots[index2 * BUCKET_SIZE + i] = false; // no need to delete the fingerprint
            current_size--;
            return true;
        }
    }

    return false;
}

std::size_t CuckooFilter::capacity() const {
    return static_cast<std::size_t>(number_of_buckets * BUCKET_SIZE * LOAD_FACTOR);
}

// Size of the filter
std::size_t CuckooFilter::size() const {
    return current_size;
}

// Check if the filter is full
bool CuckooFilter::isFull() const {
    return current_size >= capacity() || !accept_values;
}

uint64_t MurmurHash64A ( const void* key, int32_t len, uint32_t seed )
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len/8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m; 
        k ^= k >> r; 
        k *= m; 

        h ^= k;
        h *= m; 
    }

    const unsigned char* data2 = (const unsigned char*)data;

    switch(len & 7)
    {
        case 7: h ^= (uint64_t)data2[6] << 48;
        case 6: h ^= (uint64_t)data2[5] << 40;
        case 5: h ^= (uint64_t)data2[4] << 32;
        case 4: h ^= (uint64_t)data2[3] << 24;
        case 3: h ^= (uint64_t)data2[2] << 16;
        case 2: h ^= (uint64_t)data2[1] << 8;
        case 1: h ^= (uint64_t)data2[0];
                        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

std::size_t CuckooFilter::hash(const std::string& item) {
    return MurmurHash64A(item.data(), static_cast<uint32_t>(item.size()), 0);
}

std::size_t CuckooFilter::hash(const std::size_t item) {
    return MurmurHash64A((void*)&item, sizeof(item), 0);
}

std::size_t CuckooFilter::nextPowerOfTwo(std::size_t n)  {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> BYTE_SIZE;
    n |= n >> BYTE_SIZE * 2;
    n |= n >> BYTE_SIZE * 4;
    n++;
    return n;
}

}   // namespace baseline_LDCF