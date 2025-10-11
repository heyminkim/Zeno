#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <openssl/rand.h>
#include <chrono>
#include <functional>
#include <fstream>

#include "zeno.hpp"

#include "../util/util.hpp"

using namespace zeno;

int main() {
    uint64_t qbits = 12;
    uint64_t rbits = 8;

    Zeno* zeno = new Zeno(qbits, qbits + rbits, 0, 1, hashmode::Default, 0);

    zeno->set_auto_resize(true);

    uint8_t flags = kNoLock | kKeyIsHash;
    {
    // Setup: 10 gen 2 sequences, 3 gen 3 sequences, 4 non-void entries for each 4 slots.
    std::cout << std::endl << "testing gen N void seq cleanup" << std::endl;
    zeno->insert(508ULL, 0, 3, flags);
    zeno->grow(0, 0, kNoLock);  // becomes gen 1
    zeno->insert(252ULL + 512ULL, 0, 10, flags);
    zeno->grow(0, 0, kNoLock);  // becomes gen 2
    zeno->grow(0, 0, kNoLock);  // becomes gen 3

    uint64_t base_index = 1 << 3;
    uint64_t key_at_8 = (base_index << 8) + 200;
    uint64_t key_at_9 = ((base_index + 1) << 8) + 200;
    uint64_t key_at_10 = ((base_index + 2) << 8) + 200;
    uint64_t key_at_11 = ((base_index + 3) << 8) + 200;

    // Test 1.
    // 1. Ending A seq
    // 2. Target run with only As
    // 3. Don't completely remove longest matching void seq
    // zeno->remove(key_at_8, 0, 2, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA2A][A10AB3B][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A11A][B11B][B11B][A8AB3B][B3B][B3B][B3B][A3A]

    // Test 2.
    // 1. Ending A seq
    // 2. Target run with only As
    // 3. Completely remove longest matching void seq
    // zeno->remove(key_at_8, 0, 10, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA10A][A10AB3B][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A3A][B3B][B3B][B3B][B3B][B3B][B3B][A3A]

    // Test 3.
    // 1. Ending A seq
    // 2. Target run with only As
    // 3. Completely remove longest matching void seq + some of remaining void seq
    // zeno->remove(key_at_8, 0, 12, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA12A][A10AB3B][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A][B][B][B][B][B][B][A]

    // Test 4.
    // 1. Ending A seq
    // 2. Target run with only As
    // 3. Completely remove longest matching void seq + all of remaining void seq
    // zeno->remove(key_at_8, 0, 13, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA13A][A10AB3B][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [][][][][][][][]

    // Test 5.
    // 1. Starting A seq
    // 2. Target run with only As
    // 3. Don't completely remove longest matching void seq
    // zeno->remove(key_at_11, 0, 2, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3B][B3B][A3ABA2A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A11A][B11B][B11B][A10AB][B][B][B][A]

    // Test 6.
    // 1. Starting A seq
    // 2. Target run with only As
    // 3. Completely remove longest matching void seq
    // zeno->remove(key_at_11, 0, 3, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3B][B3B][A3ABA3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A10A][B10B][B10B][A10A]

    // Test 7.
    // 1. Starting A seq
    // 2. Target run with As and Bs
    // 3. Don't completely remove longest matching void seq
    // zeno->remove(key_at_9, 0, 4, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA4A][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A9A][B9B][B9B][A6AB3B][B3B][B3B][B3B][A3A]

    // Test 8.
    // 1. Starting A seq
    // 2. Target run with As and Bs
    // 3. Completely remove longest matching void seq
    // zeno->remove(key_at_9, 0, 10, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA10A][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A3A][B3B][B3B][B3B][B3B][B3B][B3B][A3A]

    // Test 9.
    // 1. Starting A seq
    // 2. Target run with As and Bs
    // 3. Completely remove longest matching void seq + some of remaining void seq
    // zeno->remove(key_at_9, 0, 12, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA12A][B3B][A3A]
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);    // Expected: [A][B][B][B][B][B][B][A]

    // Test 10.
    // 1. Starting A seq
    // 2. Target run with As and Bs
    // 3. Completely remove longest matching void seq + all of remaining void seq
    zeno->remove(key_at_9, 0, 13, flags);
    zeno->print_by_index(0);    // Expected: [A13A][][B3B][A3A]
    zeno->grow(0, 0, kNoLock);
    zeno->print_by_index(0);    // Expected: [A][B][B][B][B][B][B][A]


    }

    return 0;
}