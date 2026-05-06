/*
 *
 *      Author: 
 */
#ifndef BITHACK_H_
#define BITHACK_H_

//http://graphics.stanford.edu/~seander/bithacks.html
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

#ifndef haszero24
#define haszero24(x) (((x) - 0x000001000001000001000001ULL) & (~(x)) & 0x800000800000800000800000ULL)
#endif
#ifndef hasvalue24
#define hasvalue24(x,n) (haszero24((x) ^ (0x000001000001000001000001ULL * (n))))
#endif

#ifndef haszero32
#define haszero32(x) (((x) - 0x00000001000000010000000100000001ULL) & (~(x)) & 0x80000000800000008000000080000000ULL)
#endif
#ifndef hasvalue32
#define hasvalue32(x,n) (haszero32((x) ^ (0x00000001000000010000000100000001ULL * (n))))
#endif


#endif //BITHACK_H_
