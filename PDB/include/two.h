#pragma once
#ifndef __TWO_INCLUDED__
#define __TWO_INCLUDED__

// return largest i such that 2^i <= u
inline int lg(unsigned u) {
    debug(unsigned u0 = u;)
    precondition(u > 0);

    int n = 0;
#define t(j) if (u >= (1 << (j))) { u >>= (j); n += (j); }
    t(16); t(8); t(4); t(2); t(1);
#undef t

    postcondition(u == 1);
    postcondition((1U << n) <= u0);
    postcondition(n == 31 || u0 < (1U << (n+1)));
    return n;
}

// return smallest n such that n = 2^i and u <= n
inline unsigned nextPowerOfTwo(unsigned u) {
    precondition(u > 0);

    int lgu = lg(u);
    int lguRoundedUp = lg(u + (1 << lgu) - 1);
    unsigned n = 1 << lguRoundedUp;

    postcondition(n/2 < u && u <= n);
    return n;
    // examples:
    // u lgu lgRU n
    // 1   0   0  1
    // 4   2   2  4
    // 5   2   3  8
    // 7   2   3  8
    // 8   3   3  8
}

inline unsigned nextMultiple(unsigned u, unsigned m) {
    return (u + m - 1) / m * m;
}

inline int nextMultiple(int i, unsigned m) {
    return (i + m - 1) / m * m;
}

// Return number of set bits in the word.
#if defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM64)

const unsigned __int64  _5_64 =     0x5555555555555555ui64;
const unsigned __int64  _3_64 =     0x3333333333333333ui64;
const unsigned __int64  _F1_64 =    0x0f0f0f0f0f0f0f0fui64;
const unsigned __int64  _F2_64 =    0x00ff00ff00ff00ffui64;
const unsigned __int64  _F4_64 =    0x0000ffff0000ffffui64;
const unsigned __int64  _F8_64 =    0x00000000ffffffffui64;

inline unsigned bitcount64( unsigned __int64 w ) {
    // In-place adder tree: perform 32 1-bit adds, 16 2-bit adds, 8 4-bit adds,
    // 4 8-bit adds, 2 16-bit adds, and 1 32-bit add.
    // From Dr. Dobb's Nov. 2000 letters, from Phil Bagwell, on reducing
    // the cost by removing some of the masks that can be "forgotten" due
    // to the max # of bits set (64) that will fit in a byte.
    //
    w -= (w >> 1) & _5_64;
    w = ((w >> 2) & _3_64)  + (w & _3_64);
    w = ((w >> 4) & _F1_64) + (w & _F1_64);
    w += w >> 8;
    w += w >> 16;
    w += w >> 32;
    return unsigned(w & 0xff);

#if 0
    w = ((w >> 1) & _5_64)  + (w & _5_64);
    w = ((w >> 2) & _3_64)  + (w & _3_64);
    w = ((w >> 4) & _F1_64) + (w & _F1_64);
    w = ((w >> 8) & _F2_64) + (w & _F2_64);
    w = ((w >>16) & _F4_64) + (w & _F4_64);
    w = ((w >>32) & _F8_64) + (w & _F8_64);
    return unsigned(w);
#endif
    }

#endif

const unsigned int  _5_32 =     0x55555555;
const unsigned int  _3_32 =     0x33333333;
const unsigned int  _F1_32 =    0x0f0f0f0f;
const unsigned int  _F2_32 =    0x00ff00ff;
const unsigned int  _F4_32 =    0x0000ffff;

inline unsigned bitcount(unsigned u) {
    // In-place adder tree: perform 16 1-bit adds, 8 2-bit adds, 4 4-bit adds,
    // 2 8=bit adds, and 1 16-bit add.
    // From Dr. Dobb's Nov. 2000 letters, from Phil Bagwell, on reducing
    // the cost by removing some of the masks that can be "forgotten" due
    // to the max # of bits set (32) that will fit in a byte.
    //
    u -= (u >> 1) & _5_32;
    u = ((u >> 2) & _3_32)  + (u & _3_32);
    u = ((u >> 4) & _F1_32) + (u & _F1_32);
    u += u >> 8;
    u += u >> 16;
    return u & 0xff;

#if 0
    u = ((u >> 1) & _5_32)  + (u & _5_32);
    u = ((u >> 2) & _3_32)  + (u & _3_32);
    u = ((u >> 4) & _F1_32) + (u & _F1_32);
    u = ((u >> 8) & _F2_32) + (u & _F2_32);
    u = ((u >>16) & _F4_32) + (u & _F4_32);
    return u;
#endif
}

#endif // !__TWO_INCLUDED__
