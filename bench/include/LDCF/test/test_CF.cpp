#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <sys/types.h>

#include "CF.hpp"

class CuckooFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the CuckooFilter with a fixed seed for deterministic behavior
        srand(42);
    }

    void TearDown() override {
        // teardown code
    }

    // Helper function to generate a fingerprint of a given string 
    uint32_t generateFingerprint(const std::string& item, std::size_t fingerprint_size) {
        std::hash<std::string> hash_fn;
        std::size_t hash_value = hash_fn(item);
        uint32_t fingerprint = hash_value & ((1 << fingerprint_size) - 1);

        return fingerprint;
    }
};

TEST_F(CuckooFilterTest, BasicFunctionalityTest) {
    CuckooFilter cf(100, 4, 0);
    EXPECT_EQ(cf.size(), 0);

    EXPECT_EQ(cf.insert("test"), std::nullopt);
    EXPECT_EQ(cf.size(), 1);

    EXPECT_EQ(cf.contains("test"), true);
    EXPECT_EQ(cf.remove("test"), true);
    
    EXPECT_EQ(cf.size(), 0);
    EXPECT_EQ(cf.contains("test"), false);

    EXPECT_EQ(cf.isFull(), false);
}

TEST_F(CuckooFilterTest, MultipleInsertsTest) {
    // Create small cuckoo filter
    CuckooFilter cf(24, 4, 0);
    EXPECT_EQ(cf.size(), 0);

    // Insert 4 items
    EXPECT_EQ(cf.insert("test1"), std::nullopt);
    EXPECT_EQ(cf.size(), 1);
    EXPECT_EQ(cf.contains("test1"), true);

    EXPECT_EQ(cf.insert("test2"), std::nullopt);
    EXPECT_EQ(cf.size(), 2);

    EXPECT_EQ(cf.insert("test3"), std::nullopt);
    EXPECT_EQ(cf.size(), 3);

    EXPECT_EQ(cf.insert("test4"), std::nullopt);
    EXPECT_EQ(cf.size(), 4);

    // Check if the items are in the filter
    EXPECT_EQ(cf.contains("test2"), true);
    EXPECT_EQ(cf.contains("test3"), true);
    EXPECT_EQ(cf.contains("test4"), true);
}

TEST_F(CuckooFilterTest, SmallerFilterTest) {
    // Create small cuckoo filter
    CuckooFilter cf(4, 4, 0);
    EXPECT_EQ(cf.size(), 0);

    // Insert 4 items
    EXPECT_EQ(cf.insert("test1"), std::nullopt);
    EXPECT_EQ(cf.size(), 1);

    EXPECT_EQ(cf.insert("test2"), std::nullopt);
    EXPECT_EQ(cf.size(), 2);

    EXPECT_EQ(cf.insert("test3"), std::nullopt);
    EXPECT_EQ(cf.size(), 3);

    EXPECT_EQ(cf.insert("test4"), std::nullopt);
    EXPECT_EQ(cf.size(), 4);

    // Check if the items are in the filter
    EXPECT_EQ(cf.contains("test1"), true);
    EXPECT_EQ(cf.contains("test2"), true);
    EXPECT_EQ(cf.contains("test3"), true);
    EXPECT_EQ(cf.contains("test4"), true);
}


TEST_F(CuckooFilterTest, FingerprintSizeDurabilityTest) {
    // insert with more values of fingerprint size
    for (std::size_t i = 2; i < 33; i++) {
        // save the victims
        std::set<uint32_t> victims;
        CuckooFilter cf(100, i, 0);
        EXPECT_EQ(cf.size(), 0);

        int j = 0;
        while (!cf.isFull() && j < 100000){
            std::string item = "test" + std::to_string(j);
            auto result = cf.insert(item);
            if (result != std::nullopt) {
                victims.insert(result->fingerprint);
                j--;
                break;
            }
            else {
                EXPECT_EQ(cf.contains(item), true);
            }
            j++;
        }

        for (int k = 0; k < j; k++) {
            std::string item = "test" + std::to_string(k);
            uint32_t fingerprint = generateFingerprint(item, i);
            // Check if the item is in the filter but not in the victims
            if (victims.find(fingerprint) == victims.end()) {
                EXPECT_EQ(cf.contains(item), true);
            }
        }
    }
}

TEST_F(CuckooFilterTest, BigDurabilityTest) {
    // insert with more values of fingerprint size
    for (std::size_t i = 2; i < 33; i++) {
        // save the victims
        std::set<uint32_t> victims;
        CuckooFilter cf(10000, i, 0);
        EXPECT_EQ(cf.size(), 0);

        int j = 0;
        while (!cf.isFull() && j < 100000) {
            std::string item = "test" + std::to_string(j);
            auto result = cf.insert(item);
            if (result != std::nullopt) {
                victims.insert(result->fingerprint);
                j--;
                break;
            }
            j++;
        }

        for (int k = 0; k < j; k++) {
            std::string item = "test" + std::to_string(k);
            uint32_t fingerprint = generateFingerprint(item, i);
            // Check if the item is in the filter but not in the victims
            if (victims.find(fingerprint) == victims.end()) {
                EXPECT_EQ(cf.contains(item), true);
            }
        }
    }
}

TEST_F(CuckooFilterTest, MultipleLevelsTest) {
    // try with CuckooFilter with other levels than 0
    CuckooFilter cf(100, 4, 3);
    EXPECT_EQ(cf.size(), 0);

    // Insert 20 items
    for (int i = 0; i < 20; i++) {
        std::string item = "test" + std::to_string(i);
        EXPECT_EQ(cf.insert(item), std::nullopt);
    }

    // Check if the items are in the filter
    for (int i = 0; i < 20; i++) {
        std::string item = "test" + std::to_string(i);
        EXPECT_EQ(cf.contains(item), true);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}