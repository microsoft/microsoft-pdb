// mdalign.h  - Machine Dependent Alignment functions
#pragma once
#ifndef __MDALIGN_INLCUDED__
#define __MDALIGN_INCLUDED__

typedef unsigned __int32    _file_align_t;

#if defined(_M_IA64) || defined(_M_ALPHA64) || defined(_M_AMD64) || defined(_M_ARM64)
typedef __int64     _md_int_t;
#else
typedef __int32     _md_int_t;
#endif

// returns the delta
inline CB dcbAlign(size_t cb)
{
    return CB((-_md_int_t(cb)) & (sizeof(_file_align_t) - 1));
}

inline size_t cbAlign(size_t cb)
{
    return ((cb + sizeof(_file_align_t) - 1)) & ~(sizeof(_file_align_t) - 1);
}

inline PB pbAlign(PB pb)
{
    return PB((_md_int_t(pb) + sizeof(_file_align_t) - 1) & ~(_md_int_t(sizeof(_file_align_t) - 1)));
}

inline BOOL fAlign(_md_int_t i)
{
    return BOOL( !(i & (sizeof(_file_align_t) - 1)) );
}

inline BOOL fAlign(void* pv)
{
    return fAlign(_md_int_t(pv));
}

inline unsigned cbInsertAlign(PB pb, unsigned len)
{
    unsigned    align   = (4 - len) & 3;
    unsigned    alignT  = align;
    char        cPad    = (char)(LF_PAD0 + align);

    while (align--) {
        *pb++ = cPad--;
        }
    return alignT;
}

// Align via the native pointer size, not to be used for aligned data on
// disk, as it will be different for different platforms.
//
inline CB dcbAlignNative(size_t cb)
{
    return CB((-_md_int_t(cb)) & (sizeof(_md_int_t) - 1));
}

inline size_t cbAlignNative(size_t cb)
{
    return ((cb + sizeof(_md_int_t) - 1)) & ~(sizeof(_md_int_t) - 1);
}

inline PB pbAlignNative(PB pb)
{
    return PB((_md_int_t(pb) + sizeof(_md_int_t) - 1) & ~(_md_int_t(sizeof(_md_int_t) - 1)));
}

inline BOOL fAlignNative(_md_int_t i)
{
    return BOOL( !(i & (sizeof(_md_int_t) - 1)) );
}

inline BOOL fAlignNative(void* pv)
{
    return fAlign(_md_int_t(pv));
}

#endif // !__MDALIGN_INCLUDED__
