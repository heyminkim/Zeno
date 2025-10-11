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
#include <thread>

#include "zeno.hpp"

using namespace zeno;

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
    uint64_t qbits = 16;
    if (argc >= 2) {
        qbits = atoi(argv[1]);
    }
    uint64_t rbits = 16;
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    uint64_t expansions = 4;
    for (uint64_t i = 0; i < expansions; ++i) { 
        nvals *= 2;
    }
    nvals = 90 * nvals / 100;
    uint64_t key_count = 1;
    uint64_t *vals;

    Zeno* zeno = new Zeno(qbits, qbits + rbits, 0, 1, hashmode::Default, 0);

    zeno->set_auto_resize(true);

    // vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    // RAND_bytes((unsigned char*)vals, sizeof(*vals) * nvals);
    // srand(0);
    // for (uint64_t i = 0; i < nvals; i++) {
    //     vals[i] = (1 * vals[i]) % zeno->get_hash_range();
    // }
    // dump_vals("rsqf.data", vals, nvals);
    // vals = read_vals("rsqf.data", nvals);


    vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    // RAND_bytes((unsigned char*)keys, sizeof(*keys) * nvals);
    size_t total_bytes = nvals * sizeof(vals[0]);
    size_t done = 0;
    unsigned char* p = (unsigned char*)vals;
    while (done < total_bytes) {
        int this_chunk = (total_bytes - done > (size_t)INT_MAX)
                       ? INT_MAX
                       : (int)(total_bytes - done);
        if (RAND_bytes(p + done, this_chunk) != 1) {
            std::cerr << "RAND_bytes failed at offset " << done << std::endl;
            abort();
        }
        done += this_chunk;
    }

    uint64_t insert_time = 0;
    uint64_t last_index = 0;

    std::cout << "nvals : " << nvals << std::endl;

    bool print_index = false;
    /* Insert keys in Zeno filter */
    for (uint64_t i = 0; i < nvals; i++) {
        // std::cout << i << " ";
        int ret = 0;
        insert_time += timing([&]{
            ret = zeno->insert(vals[i], 0, key_count, kWaitForLock);
        });
        if (ret == -30000) {
            std::cout << "current index : " << i << std::endl;
        } else if (ret < 0) {
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

    std::cout << "insertion finished" << std::endl;
    std::cout << "insert time : " << insert_time / nvals << "(ns) per key" << std::endl;

    uint64_t count = 0;
    uint64_t num_failures = 0;

    /* Lookup inserted keys and counts. */
    for (uint64_t i = 0; i < nvals; i++) {
        uint64_t value; // dummy value
        count = zeno->query(vals[i], value, kNoLock);
        if (count < key_count) {
            std::cout << "failed lookup index : " << i << std::endl;
            std::cout << "failed lookup value : " << vals[i] << std::endl;
            // fprintf(stderr, "failed lookup after insertion for %lx %lu.\n", vals[i],
            //                 count);
            // return 0;
            zeno->insert(vals[i], 0, 1, kNoLock | kDummy);
            ++num_failures;
        }
    }
    std::cout << "Failed lookup on " << num_failures << " keys" << std::endl;


    for (uint64_t i = 0; i < nvals; ++i) {
        int ret = zeno->remove(vals[i], 0, key_count, kNoLock);
        // if (ret < 0) {
        //     std::cout << i << std::endl;
        // }
    }
    zeno->check_empty();

    return 0;
}