#pragma once

#include <cstdint>

namespace zeno {

// MurmurHash2, 64-bit versions, by Austin Appleby
// The same caveats as 32-bit MurmurHash2 apply here - beware of alignment and endian-ness issues if 
// used across multiple platforms.
uint64_t MurmurHash64A ( const void* key, int32_t len, uint32_t seed )
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len/8);

    while(data != end)
    {
        uint64_t k = *data++;

        k *= m; 
        k ^= k >> r; 
        k *= m; 

        h ^= k;
        h *= m; 
    }

    const unsigned char* data2 = (const unsigned char*)data;

    switch(len & 7)
    {
        case 7: h ^= (uint64_t)data2[6] << 48;
        case 6: h ^= (uint64_t)data2[5] << 40;
        case 5: h ^= (uint64_t)data2[4] << 32;
        case 4: h ^= (uint64_t)data2[3] << 24;
        case 3: h ^= (uint64_t)data2[2] << 16;
        case 2: h ^= (uint64_t)data2[1] << 8;
        case 1: h ^= (uint64_t)data2[0];
                        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}


// For any 1<k<=64, let mask=(1<<k)-1. hash_64() is a bijection on [0,1<<k),
// which means
// hash_64(x, mask)==hash_64(y, mask) if and only if x==y. hash_64i() is
// the inversion of
// hash_64(): hash_64i(hash_64(x, mask), mask) == hash_64(hash_64i(x,
// mask), mask) == x.

// Thomas Wang's integer hash functions. See
// <https://gist.github.com/lh3/59882d6b96166dfc3d8d> for a snapshot.
static uint64_t hash_64(uint64_t key, uint64_t mask) {
    key = (~key + (key << 21)) & mask; // key = (key << 21) - key - 1;
    key = key ^ key >> 24;
    key = ((key + (key << 3)) + (key << 8)) & mask; // key265
    key = key ^ key >> 14;
    key = ((key + (key << 2)) + (key << 4)) & mask; // key21
    key = key ^ key >> 28;
    key = (key + (key << 31)) & mask;
    return key;
}

}   // namespace zeno