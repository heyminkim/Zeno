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

using namespace zeno_headeronly;

static uint64_t timing(std::function<void()> fn) {
  const auto start = std::chrono::high_resolution_clock::now();
  fn();
  const auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
      .count();
}

uint64_t* read_vals(const std::string& filename, uint64_t& nvals) {
    std::ifstream in_file(filename, std::ios::binary);
    if (!in_file) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return nullptr;
    }
    in_file.read(reinterpret_cast<char*>(&nvals), sizeof(nvals)); // Read the count
    uint64_t* vals = (uint64_t*)malloc(nvals * sizeof(uint64_t));
    if (!vals) {
        std::cerr << "Memory allocation failed!" << std::endl;
        return nullptr;
    }
    in_file.read(reinterpret_cast<char*>(vals), nvals * sizeof(uint64_t));
    in_file.close();
    return vals;
}

void dump_vals(const std::string& filename, const uint64_t* vals, uint64_t nvals) {
    std::ofstream out_file(filename, std::ios::binary);
    if (!out_file) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }
    out_file.write(reinterpret_cast<const char*>(&nvals), sizeof(nvals)); // Store the count
    out_file.write(reinterpret_cast<const char*>(vals), nvals * sizeof(uint64_t));
    out_file.close();
}

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

    Zeno* zeno = new Zeno(qbits, qbits + rbits, 0, 2, hashmode::Default, 0);


    // std::vector<uint64_t> keys = {153336ULL, 12392ULL, 842ULL, 100042ULL, 71ULL};
    // uint64_t xvalue = 0;
    // uint64_t xcount = 4;

    // auto xret = zeno->insert(keys[0], 0, xcount, kNoLock);

    // int64_t grow_result = 0;

    // for (int x = 1; x <= 4; ++x) {
    //     grow_result = zeno->grow();
    //     std::cout << "grow result : " << grow_result << std::endl;
    //     zeno->insert(keys[x], 0, xcount, kNoLock);
        
    //     for (int y = 0; y <= x; ++y) {
    //         uint64_t count = zeno->query(keys[y], xvalue, kNoLock);
    //         if (count < xcount) {
    //             abort();
    //         }
    //     }
    // }



    // return 0;

    zeno->set_auto_resize(true);

    vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    RAND_bytes((unsigned char*)vals, sizeof(*vals) * nvals);
    srand(0);
    for (uint64_t i = 0; i < nvals; i++) {
        vals[i] = (1 * vals[i]) % zeno->get_hash_range();
    }

    uint64_t* queries;
    queries = (uint64_t*)malloc(nvals * sizeof(queries[0]));
    RAND_bytes((unsigned char*)queries, sizeof(*queries) * nvals);


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

    // zeno->insert(123011, 0, 3, kNoLock);
    // zeno->insert(1023658, 0, 1, kNoLock);
    // zeno->insert(18978, 0, 3, kNoLock);

    // zeno->insert(40212647544, 0, 1, kNoLock);

    // zeno->print_by_index(0);

    // zeno->insert(703310, 0, 1, kNoLock);
    // zeno->insert(638408, 0, 1, kNoLock);
    // zeno->insert(703310, 0, 1, kNoLock);
    // zeno->print_by_index(0);

    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);

    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);
    // zeno->grow(0, 0, 0);
    // zeno->print_by_index(0);

    // return 0;

    // uint64_t kval;

    // uint64_t c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;

    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;

    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;
    // zeno->print_by_index(0);

    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;
    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;
    // zeno->print_by_index(0);

    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;
    // zeno->grow(0, 0, 0);
    // c = zeno->query(638408, kval, kNoLock);
    // std::cout << "c : " << c << std::endl;
    // zeno->print_by_index(0);

    // return 0;

    uint64_t insert_time = 0;

    uint64_t add_by_thousand = 0;

    uint64_t last_index = 0;

    /* Insert keys in Zeno filter */
    for (uint64_t i = 0; i < nvals; i++) { // 66
        int ret = 0;
        insert_time += timing([&]{
            ret = zeno->insert(vals[i], 0, key_count, kNoLock);
        });
        if (ret < 0) {
            std::cout << i << std::endl;
            fprintf(stderr, "failed insertion for key: %lx %d.\n", vals[i], 50);
            if (ret == kErrNoSpace)
                fprintf(stderr, "Zeno filter is full.\n");
            else if (ret == kErrCouldntLock)
                fprintf(stderr, "TRY_ONCE_LOCK failed.\n");
            else
                fprintf(stderr, "Does not recognise return value.\n");
            std::cout << "insert time : " << insert_time / i << "(ns) per key" << std::endl;
            last_index = i;
            break;
        }
    }

    uint64_t count = 0;

    uint64_t positives = 0;


    /* Lookup inserted keys and counts. */
    for (uint64_t i = 0; i < nvals; i++) {
        uint64_t value; // dummy value
        count = zeno->query(vals[i], value, kNoLock);
        if (count < key_count) {
            std::cout << i << std::endl;
            std::cout << "failed lookup index : " << i << std::endl;
            std::cout << "failed lookup value : " << vals[i] << std::endl;
            fprintf(stderr, "failed lookup after insertion for %lx %lu.\n", vals[i],
                            count);
            return 0;
        }
    }

	// std::cout << query_time << std::endl;
    // std::cout << "query time : " << query_time / nvals << "(ns) per key" << std::endl;

    return 0;
}