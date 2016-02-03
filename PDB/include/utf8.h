/*++

Copyright (c) 1991-1999,  Microsoft Corporation  All rights reserved.

Module Name:

    utf.h

Abstract:

    This file contains the header information for the UTF module of NLS.

Revision History:
    
    02-06-96    JulieB    Created.

--*/
//
//  Constant Declarations.
//

#ifndef _UTF8_H_
#define _UTF8_H_

#define ASCII                 0x007f

#define UTF8_2_MAX            0x07ff  // max UTF8 2-byte sequence (32 * 64 = 2048)
#define UTF8_1ST_OF_2         0xc0    // 110x xxxx
#define UTF8_1ST_OF_3         0xe0    // 1110 xxxx
#define UTF8_1ST_OF_4         0xf0    // 1111 xxxx
#define UTF8_TRAIL            0x80    // 10xx xxxx

#define HIGHER_6_BIT(u)       ((u) >> 12)
#define MIDDLE_6_BIT(u)       (((u) & 0x0fc0) >> 6)
#define LOWER_6_BIT(u)        ((u) & 0x003f)

#define BIT7(a)               ((a) & 0x80)
#define BIT6(a)               ((a) & 0x40)

#define HIGH_SURROGATE_START  0xd800
#define HIGH_SURROGATE_END    0xdbff
#define LOW_SURROGATE_START   0xdc00
#define LOW_SURROGATE_END     0xdfff


#define  UCH_SURROGATE_FIRST         0xD800    // First surrogate
#define  UCH_HI_SURROGATE_FIRST      0xD800    // First High Surrogate
#define  UCH_PV_HI_SURROGATE_FIRST   0xDB80    // <Private Use High Surrogate, First>
#define  UCH_PV_HI_SURROGATE_LAST    0xDBFF    // <Private Use High Surrogate, Last>
#define  UCH_HI_SURROGATE_LAST       0xDBFF    // Last High Surrogate
#define  UCH_LO_SURROGATE_FIRST      0xDC00    // <Low Surrogate, First>
#define  UCH_LO_SURROGATE_LAST       0xDFFF    // <Low Surrogate, Last>
#define  UCH_SURROGATE_LAST          0xDFFF    // Last surrogate

#define IN_RANGE(v, r1, r2) ((r1) <= (v) && (v) <= (r2))
#define UCH_REPLACE                   0xFFFD     // REPLACEMENT CHARACTER

#define IsSurrogate(ch)     IN_RANGE(ch, UCH_SURROGATE_FIRST,    UCH_SURROGATE_LAST)
#define IsHighSurrogate(ch) IN_RANGE(ch, UCH_HI_SURROGATE_FIRST, UCH_HI_SURROGATE_LAST)
#define IsLowSurrogate(ch)  IN_RANGE(ch, UCH_LO_SURROGATE_FIRST, UCH_LO_SURROGATE_LAST)

#ifdef __cplusplus
extern "C" {
#endif

size_t UTF8ToUnicode(LPCSTR lpSrcStr, __out_ecount_opt(cchDest) LPWSTR lpDestStr, size_t cchDest);
size_t UTF8ToUnicodeCch(LPCSTR lpSrcStr, size_t cchSrc, __out_ecount_opt(cchDest) LPWSTR lpDestStr, size_t cchDest);
size_t UnicodeToUTF8(LPCWSTR lpSrcStr, __out_ecount_opt(cchDest) LPSTR lpDestStr, size_t cchDest);
size_t UnicodeToUTF8Cch(LPCWSTR lpSrcStr, size_t cchSrc, __out_ecount_opt(cchDest) LPSTR lpDestStr, size_t cchDest);
size_t UnicodeLengthOfUTF8 (PCSTR pUTF8);
size_t UTF8LengthOfUnicode (PCWSTR pUni);
size_t UnicodeLengthOfUTF8Cb (PCSTR pUTF8, size_t cbUTF);
size_t UTF8LengthOfUnicodeCch (PCWSTR pUni, size_t cchUni);

#ifdef __cplusplus
}
#endif

#endif
