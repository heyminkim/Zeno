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

#include "vzf.hpp"

using namespace zeno;

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

    VZF* zeno = new VZF(qbits, qbits + rbits, 0, 1, hashmode::Default, 0);

    zeno->set_auto_resize(true);

    uint64_t key = 0b11111110;
    // key <<= (64 - 12 - 7);

    zeno->insert(key, 0, 1, kNoLock | kKeyIsHash);
    zeno->insert(key + 1, 0, 1, kNoLock | kKeyIsHash);

    zeno->print_by_index(0);
    
    zeno->grow(0, 0, kNoLock);

    zeno->print_by_index(0);

    return 0;
}