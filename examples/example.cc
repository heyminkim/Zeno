#include <iostream>
#include <openssl/rand.h>

#include "zenofiltervm.hpp"

using namespace zeno;

int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Please specify the log of the number of slots, " \
                        "the number of remainder bits, and the growth coefficient " \
                        "of Zeno filter.\n");
        exit(1);
    }
    uint64_t qbits = atoi(argv[1]);
    uint64_t rbits = atoi(argv[2]);
    uint64_t coeff = atoi(argv[3]);
    uint64_t nslots = (1ULL << qbits);
    uint64_t nvals = 95 * 4 * nslots / 100;     // expand twice
    uint64_t key_count = 1;
    uint64_t *vals;

    ZenoFilterVM* zeno = new ZenoFilterVM(qbits, qbits + rbits, 0, coeff, hashmode::Default, 0);
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

    int ret = 0;

    /* Insert keys in Zeno filter */
    for (uint64_t i = 0; i < nvals; i++) {
        ret = zeno->insert(vals[i], 0, key_count, kNoLock);
        if (ret < 0) {
            std::cout << i << std::endl;
            fprintf(stderr, "failed insertion for key: %lx %d.\n", vals[i], 50);
            if (ret == kErrNoSpace)
                std::cerr << "Zeno filter is full." << std::endl;
            else if (ret == kErrCouldntLock)
                std::cerr << "TRY_ONCE_LOCK failed." << std::endl;
            else
                std::cerr << "Does not recognise return value." << std::endl;
            break;
        }
    }

    std::cout << "Insertion success for " << nvals << " keys." << std::endl;

    uint64_t count = 0;

    /* Lookup inserted keys and counts. */
    for (uint64_t i = 0; i < nvals; i++) {
        uint64_t value; // dummy value
        count = zeno->query(vals[i], value, kNoLock);
        if (count < key_count) {
            std::cout << i << std::endl;
            std::cout << "failed lookup index : " << i << std::endl;
            std::cout << "failed lookup value : " << vals[i] << std::endl;
            std::cerr << "failed lookup after insertion for " <<  vals[i] << "," << count \
                      << std::endl;
            return 0;
        }
    }
    std::cout << "Queries success for " << nvals << " keys." << std::endl;

    delete zeno;

    return 0;
}