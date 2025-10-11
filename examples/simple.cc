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

#include "aleph.hpp"

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
    uint64_t qbits = 12;
    uint64_t rbits = 8;
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    uint64_t expansions = 10;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 95 * nvals / 100;
    uint64_t key_count = 1;
    uint64_t *vals;

    Aleph* aleph = new Aleph(qbits, qbits + rbits, hashmode::Default, 0);

    aleph->set_auto_resize(true);

    uint64_t key = 141120ULL;
    uint64_t value;

    aleph->insert(key, 0, 2, kNoLock);
    bool success = aleph->query(key, value, kNoLock);

    if (success) {
        std::cout << "1 Query success" << std::endl;
    } else {
        std::cout << "1 Query failed" << std::endl;
    }

    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock);
    aleph->grow(0, 0, kNoLock); // Oldest entries become void here

    success = aleph->query(key, value, kNoLock);

    if (success) {
        std::cout << "2 Query success" << std::endl;
    } else {
        std::cout << "2 Query failed" << std::endl;
    }

    uint64_t secondary_result = aleph->query_secondary(key, value, kNoLock);
    std::cout << "secondary result : " << secondary_result << std::endl;
    aleph->print_by_index(172108);

    aleph->grow(0, 0, kNoLock);  // becomes 2 copies here

    secondary_result = aleph->query_secondary(key, value, kNoLock);
    std::cout << "1 secondary result : " << secondary_result << std::endl;

    aleph->print_by_index(172108);
    aleph->print_by_index(696396);

    int32_t del_val = aleph->remove(key, 0, 1, kNoLock);

    aleph->print_by_index(172108);
    aleph->print_by_index(696396);

    std::cout << "res " << del_val << std::endl;
    success = aleph->query(key, value, kNoLock);

    if (success) {
        std::cout << "3 Query success" << std::endl;
    } else {
        std::cout << "3 Query failed" << std::endl;
    }

    aleph->grow(0, 0, kNoLock);

    aleph->print_by_index(172108);
    aleph->print_by_index(696396);

    return 0;
}