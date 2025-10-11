#include <cmath>
#include <cstdlib>
#include <string>
#include <iomanip>
#include <openssl/rand.h>

#include "zeno.hpp"

#include "../util/util.hpp"

using namespace zeno;

int main() {
    uint64_t nvals = 31876710;

    std::string data_filename = "data.bin";
    uint64_t* data;
    data = (uint64_t*)malloc(nvals * sizeof(data[0]));
    RAND_bytes((unsigned char*)data, sizeof(*data) * nvals);
    srand(0);

    std::string queries_filename = "queries.bin";
    uint64_t* queries;
    queries = (uint64_t*)malloc(100'000 * sizeof(queries[0]));
    RAND_bytes((unsigned char*)queries, sizeof(*queries) * 100'000);

    util::dump_vals(data_filename, data, nvals);
    util::dump_vals(queries_filename, queries, 100'000);
    
    return 0;
}