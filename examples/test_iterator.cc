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

#include "zeno.hpp"

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
	uint64_t nvals = 95 * nslots / 100;
	uint64_t key_count = 4;
	uint64_t *vals;

    Zeno* zeno = new Zeno(qbits, qbits + rbits, 0, 2, hashmode::Default, 0);

    vals = (uint64_t*)malloc(nvals * sizeof(vals[0]));
    RAND_bytes((unsigned char*)vals, sizeof(*vals) * nvals);
    srand(0);
	for (uint64_t i = 0; i < nvals; i++) {
		vals[i] = (1 * vals[i]) % zeno->get_hash_range();
	}

	uint64_t insert_time = 0;
	uint64_t num_elements = nvals;

	uint64_t add_by_thousand = 0;

	uint64_t last_index = 0;

	/* Insert keys in Zeno filter */
	for (uint64_t i = 0; i < num_elements; i++) { // 66
		int ret = 0;
		ret = zeno->insert(vals[i], 0, key_count, kNoLock);
		if (ret < 0) {
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

    iterator it(zeno, 0);

    it.print();

    uint64_t input = 1;

	uint64_t iteration = 0;

	uint64_t old_canonical_slot = it.get_canonical_slot();

    while (input) {
		std::cout << "\n==== inside iteration " << iteration << "====" << std::endl;
        it.print();
        uint64_t fp, val, cnt;
        it.get_entry(fp, val, cnt);
        std::cout << "fp : " << std::bitset<8>(fp) << std::endl;
        std::cout << "val : " << val << std::endl;
        std::cout << "cnt : " << cnt << std::endl;
        ++it;
		++iteration;
		it.print();

        std::cin >> input;
    }

    return 0;
}