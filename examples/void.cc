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


int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Please specify the log of the number of slots and " \
                        "the number of remainder bits in Zeno filter.\n");
        exit(1);
    }
    uint64_t qbits = atoi(argv[1]);
    uint64_t rbits = atoi(argv[2]);
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = 95 * 4 * nslots / 100;
    uint64_t key_count = 1;
    uint64_t *vals;

    Zeno* zeno = new Zeno(qbits, qbits + rbits, 0, 1, hashmode::Default, 0);

    zeno->set_auto_resize(true);

    vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    RAND_bytes((unsigned char*)vals, sizeof(*vals) * nvals);
    srand(0);
    for (uint64_t i = 0; i < nvals; i++) {
        vals[i] = (1 * vals[i]) % zeno->get_hash_range();
    }

    // FOR DUMPING VALUES
    // std::cout << "dumping values...";
    // dump_vals("vals_20_8.bin", vals, nvals);
    // dump_vals("queries_20_8.bin", queries, nvals);
    // std::cout << "complete" << std::endl;
    // END FOR DUMPING VALUES

    // FOR READING VALUES
    // uint64_t read_amount = 0;
    // vals = read_vals("vals_20_8.bin", read_amount);
    // assert(read_amount == nvals);
    // queries = read_vals("queries_20_8.bin", read_amount);
    // assert(read_amount == nvals);
    // END FOR READING VALUES

    uint8_t flags = kNoLock | kKeyIsHash;

    // Test 1. Increment unary delimiter 
    // zeno->insert(703310, 0, 1, kNoLock);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);

    // return 0;

    // Test 2. Creating single gen1 void sequence
    // Expected: [BA] = 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 1, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // return 0;

    // Test 3. Creating single gen1 void sequence with count 2
    // Expected: [BBA] = 11111110 11111110 11111111 (A is simple delimiter)
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 2, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // return 0;

    // Test 4. Creating single gen1 void sequence with count 3
    // Expected: [B0BA] = 11111110 00000000 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 3, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // return 0;

    // Test 5. Creating single gen1 void sequence with count 4
    // Expected: [B1BA] = 11111110 00000010 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // return 0;

    // Test 6. Creating single gen1 void sequence with count 5
    // Expected: [B2BA] = 11111110 00000011 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 5, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // return 0;

    // Test 7. Insert at void sequence
    // Expected: [KB2BA] = 00111111 11111110 00000010 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_two = (1ULL << 9) + 63;
    // zeno->insert(key_at_two, 0, 1, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 8. Insert at void sequence with length 6
    // Expected: [K4KB2BA] = 00111111 00000100 00111111 11111110 00000010 11111110 11111111
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_two = (1ULL << 9) + 63;
    // zeno->insert(key_at_two, 0, 6, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 9. Delete at void sequence
    // target living entry, but keep sequence not completely deleted
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_two = (1ULL << 9) + 63;
    // zeno->insert(key_at_two, 0, 6, flags);
    // zeno->print_by_index(0);

    // uint64_t key_to_delete = (((1 << 8) + 63) << 1) + 1;
    // std::cout << std::bitset<12>(key_to_delete) << std::endl;
    // zeno->remove(key_to_delete, 0, 2, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 10. Delete at void sequence
    // target living entry, and remove all entries
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_two = (1ULL << 9) + 63;
    // zeno->insert(key_at_two, 0, 6, flags);
    // zeno->print_by_index(0);

    // uint64_t key_to_delete = (((1 << 8) + 63) << 1) + 1;
    // zeno->remove(key_to_delete, 0, 6, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 11. Delete at void sequence
    // target void entry, and remove M < N entries
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_one = (((1 << 8) + 63) << 1) + 1;
    // zeno->insert(key_at_one, 0, 6, flags);
    // zeno->print_by_index(0);

    // uint64_t key_to_delete = (((1 << 8) + 24) << 1) + 1;
    // zeno->remove(key_to_delete, 0, 3, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 12. Delete at void sequence
    // target void entry, and remove all void entries
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_one = (((1 << 8) + 63) << 1) + 1;
    // zeno->insert(key_at_one, 0, 6, flags);
    // zeno->print_by_index(0);

    // uint64_t key_to_delete = (((1 << 8) + 24) << 1) + 1;
    // zeno->remove(key_to_delete, 0, 4, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 13. Delete entire run (including void/nonvoid)
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_two = (1ULL << 9) + 63;
    // zeno->insert(key_at_two, 0, 6, flags);
    // zeno->print_by_index(0);

    // uint64_t key_to_delete = (((1 << 8) + 63) << 1) + 1;
    // zeno->remove(key_to_delete, 0, 10, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 14. Insert 1 at run with void sequence. 
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_one = (((1 << 8) + 63) << 1) + 1;
    // zeno->insert(key_at_one, 0, 1, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Test 15. Insert N at run with void sequence. 
    // uint64_t key_before_void = 508ULL;
    // zeno->insert(key_before_void, 0, 4, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);
    // uint64_t key_at_one = (((1 << 8) + 63) << 1) + 1;
    // zeno->insert(key_at_one, 0, 4, flags);
    // zeno->print_by_index(0);

    // return 0;

    // Overall correctness testing. 

    // Simulate entry that will go void at next expansion
    // uint64_t key = 508ULL;
    // int ret;
    // ret = zeno->insert(key, 0, 1, flags);
    // ret = zeno->insert(key, 0, 1, flags);
    // ret = zeno->insert(key, 0, 1, flags);

    // // will map to index 1
    // uint64_t nonvoid_key = 395302;
    // ret = zeno->insert(nonvoid_key, 0, 1, kNoLock);

    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);
    // zeno->print_by_index(0);

    // std::cout << "removing nonvoid key..." << std::endl;
    // zeno->remove(nonvoid_key, 0, 1, kNoLock);   // Remove inserted nonvoid key
    // zeno->print_by_index(0);
    // zeno->remove(588, 0, 1, flags); // Remove void entry
    // zeno->print_by_index(0);
    // zeno->remove(710, 0, 1, flags); // Remove void entry
    // zeno->print_by_index(0);
    // zeno->remove(513, 0, 1, flags); // Remove void entry
    // zeno->print_by_index(0);

    // std::cout << "--------------------------------------"<< std::endl<< std::endl<< std::endl<< std::endl<< std::endl<< std::endl<< std::endl << std::endl;

    // uint64_t void_B = 256 + 254;
    // zeno->insert(void_B, 0, 1, flags); // mimic B at index 1
    // zeno->print_by_index(0);
    // zeno->insert(256 + 104, 0, 3, flags);   // insertion to index 1
    // zeno->print_by_index(0);
    // zeno->remove(256 + 208, 0, 1, flags); // delete longest matching
    // zeno->print_by_index(0);
    // zeno->remove(256 + 210, 0, 1, flags); // this should remove void sequence
    // zeno->print_by_index(0);
    // zeno->remove(256 + 209, 0, 2, flags); // delete remaining longest matching
    // zeno->print_by_index(0);

    // Everything should be empty here..



    // std::cout << std::endl << "testing gen N void seq" << std::endl;
    // zeno->insert(508ULL, 0, 3, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);  // becomes gen 1
    // zeno->insert(765ULL, 0, 1, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);  // becomes gen 2
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);  // becomes gen 3
    // zeno->print_by_index(0);

    // uint64_t key1 = (1 << 10) + 171;
    // uint64_t key2 = (1 << 10) + (1 << 8) + 171;
    // zeno->insert(key1, 0, 1, flags);
    // zeno->insert(key2, 0, 1, flags);
    // zeno->print_by_index(0);

    // zeno->remove(1196ULL, 0, 2, flags); // Deletes key1 and 1 A (gen 2)
    // zeno->print_by_index(0);

    // zeno->remove(1420ULL, 0, 1, flags);  // Deletes gen 1 void seq
    // zeno->print_by_index(0);
    // zeno->remove(1453ULL, 0, 2, flags);  // Deletes key2 and gen 2 void seq
    // zeno->print_by_index(0);



    {
    // Setup: 10 gen 2 sequences, 3 gen 3 sequences
    // [A13A][A10AB3B][B3B][A3A]
    // std::cout << std::endl << "testing gen N void seq deletion" << std::endl;
    // zeno->insert(508ULL, 0, 3, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);  // becomes gen 1
    // zeno->insert(252ULL + 512ULL, 0, 10, flags);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, kNoLock);  // becomes gen 2
    // zeno->print_by_index(0);
    // std::cout << std::endl << std::endl << std::endl;
    // zeno->grow(0, 0, kNoLock);  // becomes gen 3
    // zeno->print_by_index(0);

    // uint64_t base_index = 1 << 3;
    // uint64_t key_at_8 = (base_index << 8) + 200;
    // uint64_t key_at_9 = ((base_index + 1) << 8) + 200;
    // uint64_t key_at_10 = ((base_index + 2) << 8) + 200;
    // uint64_t key_at_11 = ((base_index + 3) << 8) + 200;

    // Test 1. Delete As at run with only As. Remove some As.
    // Expected: [A13AA2A][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 2, flags);
    // zeno->print_by_index(0);

    // Test 2. Delete As at run with only As. Remove entire As at once.
    // Expected: [][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 13, flags);
    // zeno->print_by_index(0);

    // Test 3. Delete As at run with only As. Remove some first, remove remaining after. 
    // zeno->remove(key_at_8, 0, 3, flags);
    // zeno->print_by_index(0);    // Expected: [A10AA3A][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 10, flags);
    // zeno->print_by_index(0);    // Expected: [][A10AB3B][B3B][A3A]

    // Test 4. Delete As at run with As and Bs. Remove some As.
    // Expected: [A13A][A10AB3BA7A][B3B][A3A]
    // zeno->remove(key_at_9, 0, 7, flags);
    // zeno->print_by_index(0);

    // Test 5. Delete As at run with As and Bs. Remove entire As at once.
    // Expected: [A13A][B3B][B3B][A3A]
    // zeno->remove(key_at_9, 0, 10, flags);
    // zeno->print_by_index(0);

    // Test 6. Delete As at run with As and Bs. Remove some first, remove remaining after. 
    // zeno->remove(key_at_9, 0, 7, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA7A][B3B][A3A]
    // zeno->remove(key_at_9, 0, 3, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][B3B][B3B][A3A]

    // Test 7. Delete As at run with As and Bs. All As and Bs at once. 
    // Expected: [A13A][][B3B][A3A]
    // zeno->remove(key_at_9, 0, 13 /* will work for any >= 13 */, flags);
    // zeno->print_by_index(0);

    // Test 8. Delete Bs at run with only Bs. Remove some Bs.
    // Expected: [A13A][A10AB3B][B1B][A3A]
    // zeno->remove(key_at_10, 0, 2, flags);
    // zeno->print_by_index(0);

    // Test 9. Delete Bs at run with only Bs. Remove entire Bs at once.
    // Expected: [A13A][A10AB3B][][A3A]
    // zeno->remove(key_at_10, 0, 3, flags);
    // zeno->print_by_index(0);
    }

    {
    // Setup: 10 gen 2 sequences, 3 gen 3 sequences, 4 non-void entries for each 4 slots. V stands
    // for 'value' here. 
    // [V4VA13A][V4VA10AB3B][V4VB3B][V4VA3A]
    std::cout << std::endl << "testing gen N void seq deletion" << std::endl;
    zeno->insert(508ULL, 0, 3, flags);
    zeno->print_by_index(0);
    zeno->grow(0, 0, kNoLock);  // becomes gen 1
    zeno->insert(252ULL + 512ULL, 0, 10, flags);
    zeno->print_by_index(0);
    zeno->grow(0, 0, kNoLock);  // becomes gen 2
    zeno->print_by_index(0);
    std::cout << std::endl << std::endl << std::endl;
    zeno->grow(0, 0, kNoLock);  // becomes gen 3
    zeno->print_by_index(0);

    uint64_t base_index = 1 << 3;
    uint64_t key_at_8 = (base_index << 8) + 200;
    uint64_t key_at_9 = ((base_index + 1) << 8) + 200;
    uint64_t key_at_10 = ((base_index + 2) << 8) + 200;
    uint64_t key_at_11 = ((base_index + 3) << 8) + 200;

    // Test 1. Delete As at run with only As. Remove some As.
    // Expected: [A13ABA2A][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 2, flags);
    // zeno->print_by_index(0);

    // Test 2. Delete As at run with only As. Remove entire As at once.
    // Expected: [A13ABA13A][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 13, flags);
    // zeno->print_by_index(0);

    // Test 3. Delete As at run with only As. Remove some first, remove remaining after. 
    // zeno->remove(key_at_8, 0, 3, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA3A][A10AB3B][B3B][A3A]
    // zeno->remove(key_at_8, 0, 10, flags);
    // zeno->print_by_index(0);    // Expected: [A13ABA13A][A10AB3B][B3B][A3A]

    // Test 4. Delete As at run with As and Bs. Remove some As.
    // Expected: [A13A][A10AB3BA7A][B3B][A3A]
    // zeno->remove(key_at_9, 0, 7, flags);
    // zeno->print_by_index(0);

    // Test 5. Delete As at run with As and Bs. Remove entire As at once.
    // Expected: [A13A][A10AB3BA10A][B3B][A3A]
    // zeno->remove(key_at_9, 0, 10, flags);
    // zeno->print_by_index(0);

    // Test 6. Delete As at run with As and Bs. Remove some first, remove remaining after. 
    // zeno->remove(key_at_9, 0, 7, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA7A][B3B][A3A]
    // zeno->remove(key_at_9, 0, 3, flags);
    // zeno->print_by_index(0);    // Expected: [A13A][A10AB3BA10A][B3B][A3A]

    // Test 7. Delete As at run with As and Bs. All As and Bs at once. 
    // Expected: [A13A][][B3B][A3A]
    // zeno->remove(key_at_9, 0, 13 /* should be strictly <= 13 */, flags);
    // zeno->print_by_index(0);

    // Test 8. Delete Bs at run with only Bs. Remove some Bs.
    // Expected: [A13A][A10AB3B][B1B][A3A]
    // zeno->remove(key_at_10, 0, 2, flags);
    // zeno->print_by_index(0);

    // Test 9. Delete Bs at run with only Bs. Remove entire Bs at once.
    // Expected: [A13A][A10AB3B][][A3A]
    zeno->remove(key_at_10, 0, 3, flags);
    zeno->print_by_index(0);
    }

    return 0;
}