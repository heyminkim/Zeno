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

// #include "vzf.hpp"
#include "aleph.hpp"
#include "../util/util.hpp"

using namespace zeno;

int main(int argc, char **argv) {
    uint64_t qbits = 8;
    uint64_t rbits = 5;
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = nslots;
    uint64_t expansions = 4;
    for (uint64_t i = 0; i < expansions; ++i) {
        nvals *= 2;
    }
    nvals = 60 * nvals / 100;
    std::cout << "nvals : " << nvals << std::endl;

    uint64_t* keys = nullptr;
    keys = (uint64_t*)malloc(nvals * sizeof(keys[0]));
    size_t total_bytes = nvals * sizeof(keys[0]);
    size_t done = 0;
    unsigned char* p = (unsigned char*)keys;
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
    // util::dump_vals("keys.data", keys, nvals);

    // keys = util::read_vals("keys.data", nvals);

    // VZF* zeno = new VZF(qbits, qbits + rbits, 0, 1, hashmode::Default, 0);
    Aleph* zeno = new Aleph(qbits, qbits + rbits, hashmode::Default, 0);
    zeno->set_auto_resize(true);

    bool ret = false;
    for (uint64_t i = 0; i < nvals; ++i) {
        ret = (zeno->insert(keys[i], 0, 1, kNoLock) >= 0);
    }

    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);
    std::cout << "---" << std::endl;
    zeno->contract();
    zeno->contract();
    zeno->contract();
    zeno->contract();

    return 0;

    uint64_t count = 0;
    uint64_t val;

    // Sanity check - query all entries before expansion
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            std::cout << "query failed at index " << i << std::endl;
            abort();
        }
    }

    std::cout << "[1] query success" << std::endl;

    // Manually grow twice
    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);
    zeno->grow(0, 0, kNoLock);

    // Sanity check - query all entries after expansion
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            std::cout << "query failed at index " << i << std::endl;
            abort();
        }
    }

    std::cout << "[2] query success" << std::endl;

    std::cout << "begin contraction" << std::endl;

    // Manually contract
    auto r = zeno->contract();

    // Query all entries after contraction
    uint64_t total = 0;
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            ++total;
        }
    }

    if (total) std::cout << "total failed queries : " << total << std::endl;
    else std::cout << "[3] query success" << std::endl;

    // Manually contract
    r = zeno->contract();

    // Query all entries after contraction
    total = 0;
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            ++total;
        }
    }

    if (total) std::cout << "total failed queries : " << total << std::endl;
    else std::cout << "[4] query success" << std::endl;

    // Manually contract
    r = zeno->contract();

    // Query all entries after contraction
    total = 0;
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            ++total;
        }
    }

    if (total) std::cout << "total failed queries : " << total << std::endl;
    else std::cout << "[5] query success" << std::endl;

    // Manually contract
    r = zeno->contract();

    // Query all entries after contraction
    total = 0;
    for (uint64_t i = 0; i < nvals; ++i) {
        count = zeno->query(keys[i], val, kNoLock);
        if (count == 0) {
            ++total;
        }
    }

    if (total) std::cout << "total failed queries : " << total << std::endl;
    else std::cout << "[6] query success" << std::endl;


    return 0;
}