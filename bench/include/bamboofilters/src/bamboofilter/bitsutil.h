
#pragma once

#include <stdint.h>

// inspired from
// http://www-graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
#ifndef haszero4
#define haszero4(x) (((x) - 0x1111ULL) & (~(x)) & 0x8888ULL)
#endif
#ifndef hasvalue4
#define hasvalue4(x,n) (haszero4((x) ^ (0x1111ULL * (n))))
#endif

#ifndef haszero8
#define haszero8(x) (((x) - 0x01010101ULL) & (~(x)) & 0x80808080ULL)
#endif
#ifndef hasvalue8
#define hasvalue8(x,n) (haszero8((x) ^ (0x01010101ULL * (n))))
#endif

#ifndef haszero12
#define haszero12(x) (((x) - 0x0010010010010ULL) & (~(x)) & 0x8008008008000ULL)
#endif
#ifndef hasvalue12
#define hasvalue12(x,n) (haszero12((x) ^ (0x0010010010010ULL * (n))))
#endif

#ifndef haszero16
#define haszero16(x) (((x) - 0x0001000100010001ULL) & (~(x)) & 0x8000800080008000ULL)
#endif
#ifndef hasvalue16
#define hasvalue16(x,n) (haszero16((x) ^ (0x0001000100010001ULL * (n))))
#endif

inline uint64_t upperpower2(uint64_t x)
{
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}
