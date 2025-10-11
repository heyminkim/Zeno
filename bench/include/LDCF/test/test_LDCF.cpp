#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string>
#include <sys/types.h>
#include "LDCF.hpp"

class LogarithmicDynamicCuckooFilterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize the LogarithmicDynamicCuckooFilter with a fixed seed for deterministic behavior
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

TEST_F(LogarithmicDynamicCuckooFilterTest, BasicFunctionalityTest) {
    LogarithmicDynamicCuckooFilter ldCF(0.1, 4, 1);
    EXPECT_EQ(ldCF.size(), 0);

    ldCF.insert("test");
    EXPECT_EQ(ldCF.size(), 1);

    EXPECT_EQ(ldCF.contains("test"), true);
    EXPECT_EQ(ldCF.remove("test"), true);

    EXPECT_EQ(ldCF.size(), 0);
    EXPECT_EQ(ldCF.contains("test"), false);
}

TEST_F(LogarithmicDynamicCuckooFilterTest, MultipleInsertsTest) {
    // Create small logarithmic dynamic cuckoo filter
    LogarithmicDynamicCuckooFilter ldCF(0.1, 4, 1);
    EXPECT_EQ(ldCF.size(), 0);

    // Insert 4 items
    ldCF.insert("test1");
    EXPECT_EQ(ldCF.size(), 1);
    EXPECT_EQ(ldCF.contains("test1"), true);

    ldCF.insert("test2");
    EXPECT_EQ(ldCF.size(), 2);

    ldCF.insert("test3");
    EXPECT_EQ(ldCF.size(), 3);

    ldCF.insert("test4");
    EXPECT_EQ(ldCF.size(), 4);

    // Check if the items are in the filter
    EXPECT_EQ(ldCF.contains("test2"), true);
    EXPECT_EQ(ldCF.contains("test3"), true);
    EXPECT_EQ(ldCF.contains("test4"), true);
}

TEST_F(LogarithmicDynamicCuckooFilterTest, BigDurabilityTest) {
    // Test with a large number of inserts
    LogarithmicDynamicCuckooFilter ldCF(0.01, 10000, 4);
    EXPECT_EQ(ldCF.size(), 0);

    auto k = 10000;
    for (std::size_t i = 2; i < k; ++i) {
        std::string item = "test" + std::to_string(i);
        ldCF.insert(item);
        EXPECT_EQ(ldCF.contains(item), true);
    }

    // Check if the items are in the filter
    for (std::size_t i = 2; i < k; ++i) {
        std::string item = "test" + std::to_string(i);     
        EXPECT_EQ(ldCF.contains(item),true);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
