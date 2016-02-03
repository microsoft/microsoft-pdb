// szst.h - functions to convert from zero terminated strings (sz) to length preceeded
// strings (st)

#ifndef __SZST_INCLUDED__
#define __SZST_INCLUDED__

#include <malloc.h>
#include "utf8.h"

inline SZ szCopySt(ST_CONST stFrom)
{
    size_t  cch = *PB(stFrom);
    SZ sz = new char[cch + 1];
    if (sz) {
        memcpy(sz, stFrom + 1, cch);
        sz[cch] = 0;
    }
    return sz;
}

// convert from ST to SZ w/ user supplied buffer
inline SZ szFromSt(_Out_ char szTo[256], ST_CONST stFrom)
{
    size_t  cch = *PB(stFrom);
    memcpy(szTo, stFrom + 1, cch);
    szTo[cch] = 0;
    return &szTo[0];
}

//  convert from ST to SZ inplace
inline SZ stToSzInPlace(_Inout_ ST pstr)
{
    PB pb = (PB)pstr;
    CB cb = *pb;
    memmove(pb, pb + 1, cb);
    *(pb + cb) = '\0';

    return (SZ)pb;
}

//  convert from SZ to ST inplace
inline ST szToStInPlace(_Inout_ SZ pstr)
{
    PB pb = (PB)pstr;
    CB cb = static_cast<CB>(strlen(pstr));
    assert(cb < 256);
    memmove(pb + 1, pb, cb);
    *pb = (BYTE)cb;

    return (ST)pb;
}


// Return a 0-terminated string copied from the 0-terminated string,
// or 0 if memory allocation failure.
//
inline SZ szCopy(SZ_CONST szFrom)
{
    size_t cch = strlen(szFrom);
    SZ sz = new char[cch + 1];
    if (sz)
        memcpy(sz, szFrom, cch + 1);
    return sz;
}

// Free a 0-terminated string previously allocated by szCopySt() or szCopy().
//
inline void freeSz(_In_z_ SZ sz)
{
    assert(sz);
    if (sz)
        delete [] sz;
}

inline bool fSTEqual(ST_CONST st1, ST_CONST st2)
{
    if (*st1 !=  *st2)
        return false;

    return (memcmp(st1, st2, (size_t) *(unsigned char *)st1) == 0);
}

inline bool fSTSZEqual(ST_CONST st, SZ_CONST sz)
{
    unsigned char szSize = (unsigned char) strlen(sz);
    if (*(unsigned char *)st != szSize)
        return false;

    return (memcmp(st + 1, sz, szSize) == 0);
}

inline wchar_t *wszCopy(const wchar_t *wszFrom)
{
    size_t cch = wcslen(wszFrom) + 1;
    wchar_t *wsz = new wchar_t[cch];
    if (wsz)
        memcpy(wsz, wszFrom, cch * sizeof(wchar_t));
    return wsz;
    
}

inline void freeWsz(const wchar_t *wsz)
{
    assert(wsz);
    delete []wsz;
}

///////////////////////////////////////////////////////////////////
// UTF functions

typedef char *UTFSZ;
typedef const char *UTFSZ_CONST;

#define utflen(s)          UnicodeLengthOfUTF8(s)

#define USES_STACKBUFFER(x)   SafeStackAllocator<x> ___allocator;

#define cbMBCSFromCch(cch)           ((cch) * sizeof(char) * 2)
#define cbUnicodeFromCch(cch)        ((cch) * sizeof(wchar_t) )
#define cbUTFFromCch(cch)            ((cch) * sizeof(char) * 4)

// Note: __allocator changed to ___allocator for Windows SDK update for Vista (2006); specstrings.h added definition for __allocator
#define GetSZMBCSFromSZUnicode(u)  _GetSZMBCSFromSZUnicode(u, ___allocator)
#define GetSZUnicodeFromSZMBCS(m)  _GetSZUnicodeFromSZMBCS(m, ___allocator)
#define GetSZUTF8FromSZUnicode(u)  _GetSZUTF8FromSZUnicode(u, ___allocator)
#define GetSZUnicodeFromSZUTF8(t)  _GetSZUnicodeFromSZUTF8(t, ___allocator)
#define GetSZUnicodeFromSTMBCS(m)  _GetSZUnicodeFromSTMBCS(m, ___allocator)
#define GetSZUTF8FromSZMBCS(m)     _GetSZUTF8FromSZMBCS(m, ___allocator)
#define GetSZMBCSFromSZUTF8(t)     _GetSZMBCSFromSZUTF8(t, ___allocator)

SZ       _GetSZMBCSFromSZUnicode(const wchar_t *wsz, Allocator& ___allocator);
wchar_t *_GetSZUnicodeFromSZMBCS(SZ_CONST sz,        Allocator& ___allocator);
UTFSZ    _GetSZUTF8FromSZUnicode(const wchar_t *wsz, Allocator& ___allocator);
wchar_t *_GetSZUnicodeFromSZUTF8(UTFSZ_CONST utfsz,  Allocator& ___allocator);
wchar_t *_GetSZUnicodeFromSTMBCS(ST_CONST st,        Allocator& ___allocator);
UTFSZ    _GetSZUTF8FromSZMBCS   (SZ_CONST sz,        Allocator& ___allocator);
SZ       _GetSZMBCSFromSZUTF8   (UTFSZ_CONST utfsz,  Allocator& ___allocator);

inline BOOL IsASCIIString(SZ_CONST sz)
{
    // Check if a UTF8 string is ASCII also
    BOOL fResult = TRUE;

    for (const char *pch = sz; *pch && fResult; pch++)
        fResult = (__isascii(*pch));

    return fResult;
}

inline BOOL IsASCIIString(SZ_CONST sz, size_t cch)
{
    // Check if a UTF8 string is ASCII also
    BOOL fResult = TRUE;
    const char *pch = sz;

    for ( size_t i = 0; i < cch && fResult; i++)
        fResult = (__isascii(pch[i]));

    return fResult;
}

#if 0

inline size_t GetMBCSLengthOfString(const char *pchStart, size_t cb = 0)
{
    size_t cbMBCS = cb ? cb : strlen(pchStart) + 1;
    const char *pchEnd = pchStart + cbMBCS, *pch = pchStart; 

    for(size_t n = 0; pch < pchEnd; n++ ) {
        // Caveat - CharNextExA does not increment the pointer
        // if it already points to NULL, problem if cb != 0
        pch = (*pch ? CharNextExA(CP_ACP, pch, 0) : pch + 1);
    }

    return n;
}

#endif

inline size_t UnicodeLengthOfMBCS(SZ_CONST szMBCS, size_t cb = 0)
{
    size_t cbMBCS = (cb == 0 ? strlen(szMBCS) + 1 : cb);

    size_t len = MultiByteToWideChar(
        CP_ACP,
        MB_PRECOMPOSED,
        szMBCS,
        static_cast<int>(cbMBCS),
        NULL,
        0
        );

    assert(len != 0);
    return len;
}

inline wchar_t *_GetSZUnicodeFromSZUTF8(SZ_CONST szUtf8, __out_ecount(len) wchar_t *wcs_, size_t len);

inline char *_GetSZMBCSFromSZUnicode(const wchar_t *szUnicode, __out_ecount(cbMBCS) char *szMBCS, size_t cbMBCS)
{
    // Convert to MBCS and return a temp buffer
    if (!szMBCS)
        return NULL;

    cbMBCS = WideCharToMultiByte(
        CP_ACP, 
        0, 
        szUnicode,
        -1, 
        szMBCS,
        static_cast<int>(cbMBCS), 
        NULL,
        NULL);

    if (!cbMBCS) {
        szMBCS = NULL;
    }

    return szMBCS;
}

inline wchar_t *_GetSZUnicodeFromSZMBCS(SZ_CONST szMBCS, __out_ecount(cchUnicode) wchar_t *pwcs, size_t cchUnicode)
{
    // Convert to WCS and return a temp buffer
    if(pwcs == NULL)
        return pwcs;

    cchUnicode = MultiByteToWideChar(
        CP_ACP, 
        MB_PRECOMPOSED, 
        szMBCS, 
        -1, 
        pwcs, 
        static_cast<int>(cchUnicode)
        );
    
    if (!cchUnicode) {
        pwcs =  NULL;
    }

    return pwcs;
}

inline UTFSZ _GetSZUTF8FromSZUnicode(const wchar_t *szUnicode, _Inout_opt_cap_(cbUTF8) UTFSZ szUtf8, size_t cbUTF8)
{
    if (szUtf8 == NULL)
        return szUtf8;

    if (!UnicodeToUTF8(szUnicode, szUtf8, cbUTF8)) {
        szUtf8 = NULL;
    }

    return szUtf8;
}

inline wchar_t *_GetSZUnicodeFromSZUTF8(SZ_CONST szUtf8, __out_ecount(cchUnicode) wchar_t *szUnicode, size_t cchUnicode )
{
    if (szUnicode == NULL)
        return NULL;

    if (!UTF8ToUnicode(szUtf8, szUnicode, cchUnicode)) {
        szUnicode = NULL;
    }

    return szUnicode;
}

inline wchar_t *_GetSZUnicodeFromSTMBCS(ST_CONST stMBCS, __out_ecount(cchUnicode) wchar_t *szUnicode, size_t cchUnicode)
{
    // Convert to WCS and return a temp buffer
    char    szMBCS[256];
    szFromSt(szMBCS, stMBCS);
    return _GetSZUnicodeFromSZMBCS(szMBCS, szUnicode, cchUnicode);
}

inline UTFSZ _GetSZUTF8FromSZMBCS(SZ_CONST szMBCS, _Inout_opt_cap_(cbUTF) UTFSZ szUTF, size_t cbUTF)
{
    USES_STACKBUFFER(0x400);

    wchar_t *wcs = GetSZUnicodeFromSZMBCS(szMBCS);
    if (wcs == NULL)
        return NULL;

    return _GetSZUTF8FromSZUnicode(wcs, szUTF, cbUTF);
}

inline SZ _GetSZMBCSFromSZUTF8(SZ_CONST szUTF8, _Inout_opt_cap_(cbMBCS) SZ szMBCS, size_t cbMBCS)
{
    USES_STACKBUFFER(0x400);

    wchar_t *szUnicode = GetSZUnicodeFromSZUTF8(szUTF8);
    if (szUnicode == NULL)
        return NULL;

    return _GetSZMBCSFromSZUnicode(szUnicode, szMBCS, cbMBCS);
}

inline int SZMBCSToSZUTF8(SZ_CONST szMBCS, _Inout_opt_cap_(cbMax) SZ szUTF8, CB cbMax)
{
    USES_STACKBUFFER(0x400);

    wchar_t *wcs = GetSZUnicodeFromSZMBCS(szMBCS);

    int lenUTF8 = (wcs != NULL) ? int(UTF8LengthOfUnicode(wcs)) : (cbMax = 0);
    if (cbMax >= lenUTF8) 
        return (_GetSZUTF8FromSZUnicode(wcs, szUTF8, cbMax) == NULL) ? 0 : lenUTF8;
    
    return 0;
}

inline size_t MBCSToUTF8(SZ_CONST szMBCS, size_t cbMBCS, _Inout_opt_cap_(cbMax) SZ szUTF8, size_t cbMax)
{
    SafeStackAllocator<0x400> ___allocator;

    // cbMBCS == length of MBCS string in bytes
    size_t  cbUTF8 = 0, cchUnicode = UnicodeLengthOfMBCS(szMBCS, cbMBCS);
    wchar_t *wcs = ___allocator.Alloc<wchar_t>(cchUnicode);

    if (wcs != NULL) {
        cchUnicode = MultiByteToWideChar(
            CP_ACP,
            0,
            szMBCS, 
            static_cast<int>(cbMBCS),
            wcs,
            static_cast<int>(cchUnicode)
            );
        if (cchUnicode) {
            cbUTF8 = UnicodeToUTF8Cch(wcs, cchUnicode, szUTF8, cbMax);
        }
    }

    return cbUTF8;
}

inline size_t STMBCSToSZUTF8(_In_ ST stMBCS, _Inout_opt_cap_(cbMax) SZ szUTF8, size_t cbMax)
{
    size_t  cbMBCS = BYTE(stMBCS[0]) + 1;
    char szMBCS[256];

    assert(cbMBCS < 0x100);
    return MBCSToUTF8(szFromSt(szMBCS, stMBCS), cbMBCS, szUTF8, cbMax);
}


inline SZ _GetSZMBCSFromSZUnicode(const wchar_t *wsz, Allocator& ___allocator)
{
    size_t cbAlloc = cbMBCSFromCch(wcslen(wsz)+1);
    return _GetSZMBCSFromSZUnicode(wsz, (SZ)___allocator.AllocBytes(cbAlloc), cbAlloc);
}

inline wchar_t *_GetSZUnicodeFromSZMBCS(SZ_CONST sz, Allocator& ___allocator)
{
    size_t cchAlloc = UnicodeLengthOfMBCS(sz);
    return _GetSZUnicodeFromSZMBCS(sz, (wchar_t *)___allocator.AllocBytes(cbUnicodeFromCch(cchAlloc)), cchAlloc);
}

inline UTFSZ _GetSZUTF8FromSZUnicode(const wchar_t *wsz, Allocator& ___allocator)
{
    size_t cbAlloc = UTF8LengthOfUnicode(wsz);
    return _GetSZUTF8FromSZUnicode(wsz, (UTFSZ)___allocator.AllocBytes(cbAlloc), cbAlloc);
}

inline wchar_t *_GetSZUnicodeFromSZUTF8(UTFSZ_CONST utfsz, Allocator& ___allocator)
{
    size_t cchAlloc = UnicodeLengthOfUTF8(utfsz);
    return _GetSZUnicodeFromSZUTF8(utfsz, (wchar_t *)___allocator.AllocBytes(cbUnicodeFromCch(cchAlloc)), cchAlloc);
}

inline wchar_t *_GetSZUnicodeFromSTMBCS(ST_CONST st, Allocator& ___allocator)
{
    size_t cchAlloc = *(BYTE*)st + 1;
    return _GetSZUnicodeFromSTMBCS(st, (wchar_t *)___allocator.AllocBytes(cbUnicodeFromCch(cchAlloc)), cchAlloc);
}

inline UTFSZ _GetSZUTF8FromSZMBCS(SZ_CONST sz, Allocator& ___allocator)
{
    size_t cbAlloc = cbUTFFromCch(UnicodeLengthOfMBCS(sz));
    return _GetSZUTF8FromSZMBCS(sz, (UTFSZ)___allocator.AllocBytes(cbAlloc), cbAlloc);
}

inline SZ _GetSZMBCSFromSZUTF8(UTFSZ_CONST utfsz, Allocator& ___allocator)
{
    size_t cbAlloc = cbUTFFromCch(utflen(utfsz));
    return _GetSZUTF8FromSZMBCS(utfsz, (SZ)___allocator.AllocBytes(cbAlloc), cbAlloc);
}



#endif  // SZST_INCLUDED
