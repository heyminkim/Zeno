#include <cstdint>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "CF.hpp"

TEST(BucketTest, ReadWriteFingerprintSizes2to32) {
    const std::size_t bucket_capacity = 4;
    srand(static_cast<unsigned int>(time(nullptr))); // Seed random number generator

    for (std::size_t fingerprint_size = 2; fingerprint_size <= 32; ++fingerprint_size) {
        Bucket bucket;
        auto bits_per_bucket = bucket_capacity * fingerprint_size;
        auto bytes_per_bucket = (bits_per_bucket + 7) >> 3;
        bucket.bit_array = new char[bytes_per_bucket](); // Allocate memory for the bucket and zero initialize
        std::vector<uint32_t> fingerprints;

        // Write random fingerprints to the bucket and test read/write consistency
        for (std::size_t i = 0; i < bucket_capacity; ++i) {
            uint32_t random_fingerprint = rand() & ((1U << fingerprint_size) - 1); // Generate random fingerprint of size `fingerprint_size`
            fingerprints.push_back(random_fingerprint);
            bucket.write(i, random_fingerprint, fingerprint_size);
            EXPECT_EQ(bucket.read(i, fingerprint_size), random_fingerprint);
        }

        // Verify the fingerprints read from the bucket match what was written
        for (std::size_t i = 0; i < bucket_capacity; ++i) {
            auto random_fingerprint = fingerprints[i];
            EXPECT_EQ(bucket.read(i, fingerprint_size), random_fingerprint);
        }

        // Rewrite the fingerprints and test read/write consistency again
        for (std::size_t i = 0; i < bucket_capacity; ++i) {
            uint32_t random_fingerprint = rand() & ((1U << fingerprint_size) - 1); // Generate new random fingerprint of size `fingerprint_size`
            fingerprints[i] = random_fingerprint;
            bucket.write(i, random_fingerprint, fingerprint_size);
            EXPECT_EQ(bucket.read(i, fingerprint_size), random_fingerprint);
        }

        delete[] bucket.bit_array;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
