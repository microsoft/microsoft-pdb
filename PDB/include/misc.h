#pragma once
#ifndef __MISC_INCLUDED__
#define __MISC_INCLUDED__

#include "pdbtypdefs.h"

#if (defined(_M_ALPHA) || defined(_M_IA64) || defined(_M_AMD64))
// for fAlign
#include "cvinfo.h"         // for LF_PAD0 used in mdalign.h
#include "mdalign.h"
#endif


template <class PUL, class PUS> struct Hasher {
    static inline LHASH lhashPbCb(PB pb, size_t cb, ULONG ulMod) {
        ULONG   ulHash  = 0;

        // hash leading dwords using Duff's Device
        size_t  cl      = cb >> 2;
        PUL     pul     = PUL(pb);
        PUL     pulMac  = pul + cl;
        size_t  dcul    = cl & 7;

        switch (dcul) {
            do {
                dcul = 8;
                ulHash ^= pul[7];
        case 7: ulHash ^= pul[6];
        case 6: ulHash ^= pul[5];
        case 5: ulHash ^= pul[4];
        case 4: ulHash ^= pul[3];
        case 3: ulHash ^= pul[2];
        case 2: ulHash ^= pul[1];
        case 1: ulHash ^= pul[0];
        case 0: ;
            } while ((pul += dcul) < pulMac);
        }

        pb = PB(pul);

        // hash possible odd word
        if (cb & 2) {
            ulHash ^= *(PUS)pb;
            pb =  PB(PUS(pb) + 1);
        }

        // hash possible odd byte
        if (cb & 1) {
            ulHash ^= *(pb++);
        }

        const ULONG toLowerMask = 0x20202020;
        ulHash |= toLowerMask;
        ulHash ^= (ulHash >> 11);

        return (ulHash ^ (ulHash >> 16)) % ulMod;
    }
    static inline HASH hashPbCb(PB pb, size_t cb, ULONG ulMod) {
        return HASH(lhashPbCb(pb, cb, ulMod));
    }
};

// Hash the buffer.  Text in the buffer is hashed in a case insensitive way.
//
// On alignment sensitive machines, unaligned buffers are handed over to
// Hasher<ULONG UNALIGNED*, USHORT UNALIGNED*>::hashPbCb();.
//
inline HASH HashPbCb(PB pb, size_t cb, ULONG ulMod)
{
#if (defined(_M_ALPHA) || defined(_M_IA64) || defined(_M_AMD64))
    if (!fAlign(pb))
        return Hasher<ULONG UNALIGNED*, USHORT UNALIGNED*>::hashPbCb(pb, cb, ulMod);
#endif
    return Hasher<ULONG*, USHORT*>::hashPbCb(pb, cb, ulMod);
}

inline LHASH LHashPbCb(PB pb, size_t cb, ULONG ulMod)
{
#if (defined(_M_ALPHA) || defined(_M_IA64) || defined(_M_AMD64))
    if (!fAlign(pb))
        return Hasher<ULONG UNALIGNED*, USHORT UNALIGNED*>::lhashPbCb(pb, cb, ulMod);
#endif
    return Hasher<ULONG*, USHORT*>::lhashPbCb(pb, cb, ulMod);
}

inline HASH __fastcall hashSz(_In_z_ SZ sz) {
    return HashPbCb(PB(sz), strlen(sz), (ULONG)-1);
}

// copied from the FE
template <class PUL, class PUS> class HasherV2 {
private:
	static inline ULONG HashULONG(ULONG u) 
	{ 
		// From Numerical Recipes in C, second edition, pg 284. 
		return u * 1664525L + 1013904223L; 
	} 
public:
	static inline LHASH lhashPbCb(PB pb, size_t cb, ULONG ulMod) {
		ULONG hash = 0xb170a1bf; 

		// Hash 4 characters/one ULONG at a time. 
		while (cb >= 4) { 
			cb -= 4; 
			hash += *(PUL)pb;
			hash += (hash << 10); 
			hash ^= (hash >> 6); 
			pb += 4;
		} 

		// Hash the rest 1 by 1. 
		while (cb > 0) { 
			cb -= 1; 
			hash += *(unsigned char*)pb; 
			hash += (hash << 10); 
			hash ^= (hash >> 6); 
			pb += 1; 
		} 

		return HashULONG(hash) % ulMod;
    }
};

inline LHASH LHashPbCbV2(PB pb, size_t cb, ULONG ulMod)
{
#if (defined(_M_ALPHA) || defined(_M_IA64) || defined(_M_AMD64))
    if (!fAlign(pb))
        return HasherV2<ULONG UNALIGNED*, USHORT UNALIGNED*>::lhashPbCb(pb, cb, ulMod);
#endif
    return HasherV2<ULONG*, USHORT*>::lhashPbCb(pb, cb, ulMod);
}

#endif // !__MISC_INCLUDED__
