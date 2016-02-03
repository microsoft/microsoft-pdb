//
// CBitVect.h
//  define a relatively simple class to do bit vectors of an arbitrary size.
//
#pragma once
#include "two.h"

#if !defined(_cbitvect_h)
#define _cbitvect_h

#include <limits.h>

#if defined(_M_IX86) || defined(_M_ARM)
    #define NATIVE_INT  unsigned long
    #define N_UINT_BITS 32
    #define N_UINT_MAX  _UI32_MAX
    enum {
        lgcbitsNative = 5
    };

#elif defined(_M_IA64) || defined(_M_AMD64) || defined(_M_ARM64)
    #define NATIVE_INT  unsigned __int64
    #define N_UINT_BITS 64
    #define N_UINT_MAX  _UI64_MAX
    enum {
        lgcbitsNative = 6
    };
#else
    #pragma message("Warning: Unknown cpu, defaulting to 32-bit ints")
    #define NATIVE_INT  unsigned long
    #define N_UINT_BITS 32
    #define N_UINT_MAX  _UI32_MAX
    enum {
        lgcbitsNative = 5
    };
#endif

typedef NATIVE_INT          native_uint;
typedef unsigned __int32    uint32;

enum {
    cbitsNative = sizeof(native_uint) * CHAR_BIT,
};

#define CWordsFromCBits(x)      (((x) + N_UINT_BITS - 1) / N_UINT_BITS)
#define CWordsFromCBits32(x)    (((x) + 32 - 1) / 32)

#if N_UINT_BITS == 64
inline unsigned
cbits (native_uint w) {
    return bitcount64(w);
}

inline unsigned
cbits32 (unsigned __int32 u) {
    return bitcount(u);
}

#else

inline unsigned
cbits (native_uint w) {
    return bitcount(w);
}
#endif

inline unsigned IWordFromIBit (unsigned ibit) {
    return ibit / N_UINT_BITS;
}

inline native_uint IBitInIWordFromIBit (unsigned ibit) {
    return native_uint(ibit) % N_UINT_BITS;
}

#if N_UINT_BITS != 32
// Specific 32-bit versions
inline unsigned IWordFromIBit32 (unsigned ibit) {
    return ibit / 32;
}

inline native_uint IBitInIWordFromIBit32 (unsigned ibit) {
    return uint32(ibit) % 32;
}
#endif


template <int T>
class CBitVect
{
protected:
    native_uint _rgw[CWordsFromCBits(T)];

    native_uint &
    word (unsigned ibit) {
        return _rgw[IWordFromIBit (ibit)];
    }

    const native_uint &
    word (unsigned ibit) const {
        return _rgw[IWordFromIBit (ibit)];
    }

    native_uint
    bitmask (unsigned ibit) const {
        return native_uint(1) << IBitInIWordFromIBit (ibit);
    }

public:
    void
    SetAll (unsigned f) {
        memset (_rgw, f ? 0xff : 0, sizeof(_rgw));
    }

    CBitVect (BOOL f =FALSE) {
        SetAll (f);
    }

    CBitVect(const CBitVect & rgbitSrc) {
        memcpy (_rgw, rgbitSrc._rgw, sizeof(_rgw));
    }

    CBitVect &
    operator= (const CBitVect & rgbitSrc) {
        memcpy (_rgw, rgbitSrc._rgw, sizeof(_rgw));
        return *this;
    }

    unsigned
    operator[] (unsigned ibit) const {
        assert (ibit < T);
        return (ibit < T) && !!(word (ibit) & bitmask (ibit));
    }

    unsigned
    CBitsSet() const {
        unsigned wRet = 0;
        for (unsigned i = 0; i < CWordsFromCBits(T); i++)
            wRet += cbits (_rgw[i]);
        return wRet;
    }

    operator int () const {
        return CBitsSet();
    }

    void
    SetIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (ibit < T) {
            if (f) {
                word (ibit) |= bitmask (ibit);
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
    }

    unsigned
    OrIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (f && ibit < T) {
            word (ibit) |= bitmask (ibit);
        }
        return (*this)[ibit];
    }

    unsigned
    AndIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (ibit < T) {
            if (f) {
                word (ibit) &= N_UINT_MAX;
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
        return (*this)[ibit];
    }

    bool
    FAnyBitsSet() const {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            if (_rgw[iw]) {
                return true;
            }
        }
        return false;
    }

    BOOL
    FAnyBitsInCommon (const CBitVect & rgbits) const {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            if (_rgw[iw] & rgbits._rgw[iw]) {
                return fTrue;
            }
        }
        return fFalse;
    }

    CBitVect
    operator | (const CBitVect & rgbits) const {
        CBitVect<T> rgbitRet(*this);
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            rgbitRet._rgw[iw] |= rgbits._rgw[iw];
        }
        return rgbitRet;
    }

    CBitVect &
    operator |= (const CBitVect & rgbits) {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            _rgw[iw] |= rgbits._rgw[iw];
        }
        return *this;
    }

    CBitVect
    operator & (const CBitVect & rgbits) const {
        CBitVect<T> rgbitRet(*this);
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            rgbitRet._rgw[iw] &= rgbits._rgw[iw];
        }
        return rgbitRet;
    }

    CBitVect &
    operator &= (const CBitVect & rgbits) {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            _rgw[iw] &= rgbits._rgw[iw];
        }
        return *this;
    }
};


class CBitVectGSI
{
protected:
    DWORD         _cbits;
    native_uint * _rgw;

    native_uint &
    word (unsigned ibit) {
        return _rgw[IWordFromIBit (ibit)];
    }

    const native_uint &
    word (unsigned ibit) const {
        return _rgw[IWordFromIBit (ibit)];
    }

    native_uint
    bitmask (unsigned ibit) const {
        return native_uint(1) << IBitInIWordFromIBit (ibit);
    }

public:
    void
    SetAll (unsigned f) {
        memset (_rgw, f ? 0xff : 0, CbRaw());
    }

    unsigned
    CbRaw() const {
        return CWordsFromCBits(_cbits) * sizeof(native_uint);
    }

    BYTE * PbRaw() const {
        return (BYTE *) _rgw;
    }

    CBitVectGSI (BOOL f = FALSE, DWORD dw = 32) {
        _cbits = dw;
        _rgw = new native_uint[CWordsFromCBits(dw)];

        if (_rgw == NULL)
            _cbits = 0;
        else
            SetAll (f);
    }

    ~CBitVectGSI () {
        if (_rgw != NULL) {
            delete [] _rgw;
        }
    }

    unsigned
    operator[] (unsigned ibit) const {
        assert (ibit < _cbits);
        return (ibit < _cbits) && !!(word (ibit) & bitmask (ibit));
    }

    unsigned
    CBitsSet() const {
        unsigned wRet = 0;
        for (unsigned i = 0; i < CWordsFromCBits(_cbits); i++)
            wRet += cbits (_rgw[i]);
        return wRet;
    }

    operator int () const {
        return CBitsSet();
    }

    void
    SetIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (ibit < _cbits) {
            if (f) {
                word (ibit) |= bitmask (ibit);
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
    }

    unsigned
    OrIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (f && ibit < _cbits) {
            word (ibit) |= bitmask (ibit);
        }
        return (*this)[ibit];
    }

    unsigned
    AndIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (ibit < _cbits) {
            if (f) {
                word (ibit) &= N_UINT_MAX;
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
        return (*this)[ibit];
    }
};


#if defined(_M_IX86) || defined(_M_ARM)
// for 32-bit builds, CBitVect<> and CBitVect32<> are identical
#define CBitVect32     CBitVect
#define CBitVect32GSI  CBitVectGSI
#else
// otherwise, we have to define a CBitVect32<>

template <int T>
class CBitVect32
{
protected:
    uint32  _rgw[CWordsFromCBits32(T)];

    uint32 &
    word (unsigned ibit) {
        return _rgw[IWordFromIBit32 (ibit)];
    }

    const uint32 &
    word (unsigned ibit) const {
        return _rgw[IWordFromIBit32 (ibit)];
    }

    uint32
    bitmask (unsigned ibit) const {
        return uint32(1) << IBitInIWordFromIBit32 (ibit);
    }

public:
    void
    SetAll (unsigned f) {
        memset (_rgw, f ? 0xff : 0, sizeof(_rgw));
    }

    CBitVect32 (BOOL f =FALSE) {
        SetAll (f);
    }

    CBitVect32(const CBitVect32 & rgbitSrc) {
        memcpy (_rgw, rgbitSrc._rgw, sizeof(_rgw));
    }

    CBitVect32 &
    operator= (const CBitVect32 & rgbitSrc) {
        memcpy (_rgw, rgbitSrc._rgw, sizeof(_rgw));
        return *this;
    }

    unsigned
    operator[] (unsigned ibit) const {
        assert (ibit < T);
        return (ibit < T) && !!(word (ibit) & bitmask (ibit));
    }

    unsigned
    CBitsSet() const {
        unsigned wRet = 0;
        for (unsigned i = 0; i < CWordsFromCBits32(T); i++)
            wRet += cbits32 (_rgw[i]);
        return wRet;
    }

    operator int () const {
        return CBitsSet();
    }

    void
    SetIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (ibit < T) {
            if (f) {
                word (ibit) |= bitmask (ibit);
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
    }

    unsigned
    OrIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (f && ibit < T) {
            word (ibit) |= bitmask (ibit);
        }
        return (*this)[ibit];
    }

    unsigned
    AndIBit (unsigned ibit, unsigned f) {
        assert (ibit < T);
        if (ibit < T) {
            if (f) {
                word (ibit) &= _UI32_MAX;
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
        return (*this)[ibit];
    }

    bool
    FAnyBitsSet() const {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            if (_rgw[iw]) {
                return true;
            }
        }
        return false;
    }

    BOOL
    FAnyBitsInCommon (const CBitVect32 & rgbits) const {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            if (_rgw[iw] & rgbits._rgw[iw]) {
                return fTrue;
            }
        }
        return fFalse;
    }

    CBitVect32
    operator | (const CBitVect32 & rgbits) const {
        CBitVect32<T>   rgbitRet(*this);
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            rgbitRet._rgw[iw] |= rgbits._rgw[iw];
        }
        return rgbitRet;
    }

    CBitVect32 &
    operator |= (const CBitVect32 & rgbits) {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            _rgw[iw] |= rgbits._rgw[iw];
        }
        return *this;
    }

    CBitVect32
    operator & (const CBitVect32 & rgbits) const {
        CBitVect32<T>   rgbitRet(*this);
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            rgbitRet._rgw[iw] &= rgbits._rgw[iw];
        }
        return rgbitRet;
    }

    CBitVect32 &
    operator &= (const CBitVect32 & rgbits) {
        for (unsigned iw = 0; iw < _countof(_rgw); iw++) {
            _rgw[iw] &= rgbits._rgw[iw];
        }
        return *this;
    }
};


class CBitVect32GSI
{
protected:
    DWORD    _cbits;
    uint32 * _rgw;

    uint32 &
    word (unsigned ibit) {
        return _rgw[IWordFromIBit32 (ibit)];
    }

    const uint32 &
    word (unsigned ibit) const {
        return _rgw[IWordFromIBit32 (ibit)];
    }

    uint32
    bitmask (unsigned ibit) const {
        return uint32(1) << IBitInIWordFromIBit32 (ibit);
    }

public:
    void
    SetAll (unsigned f) {
        memset (_rgw, f ? 0xff : 0, CbRaw());
    }

    unsigned
    CbRaw() const {
        return CWordsFromCBits32(_cbits) * sizeof(uint32);
    }

    BYTE * PbRaw() const {
        return (BYTE *) _rgw;
    }

    CBitVect32GSI (BOOL f = FALSE, DWORD dw = 32) {
        _cbits = dw;
        _rgw = new uint32[CWordsFromCBits32(dw)];

        if (_rgw == NULL)
            _cbits = 0;
        else
            SetAll (f);
    }

    ~CBitVect32GSI() {
        if (_rgw != NULL) {
            delete [] _rgw;
        }
    }

    unsigned
    operator[] (unsigned ibit) const {
        assert (ibit < _cbits);
        return (ibit < _cbits) && !!(word (ibit) & bitmask (ibit));
    }

    unsigned
    CBitsSet() const {
        unsigned wRet = 0;
        for (unsigned i = 0; i < CWordsFromCBits32(_cbits); i++)
            wRet += cbits32 (_rgw[i]);
        return wRet;
    }

    operator int () const {
        return CBitsSet();
    }

    void
    SetIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (ibit < _cbits) {
            if (f) {
                word (ibit) |= bitmask (ibit);
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
    }

    unsigned
    OrIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (f && ibit < _cbits) {
            word (ibit) |= bitmask (ibit);
        }
        return (*this)[ibit];
    }

    unsigned
    AndIBit (unsigned ibit, unsigned f) {
        assert (ibit < _cbits);
        if (ibit < _cbits) {
            if (f) {
                word (ibit) &= _UI32_MAX;
            }
            else {
                word (ibit) &= ~bitmask (ibit);
            }
        }
        return (*this)[ibit];
    }
};


#endif
#endif
