// StrConvBuf.h - advanced functions to convert strings between utf8, unicode, mbcs, etc.
// that use Buffers instead of raw pointers.
// useful for when working in a loop where _alloca() is bad or you want to easily
// manage the memory being used, i.e. have it go away automatically.
//
#pragma once
#include "szst.h"       // we use some of the underlying support here too.

//
// Use a buffer so that we can avoid use of _alloca() where appropriate,
// as in loops or places where we want ease of use with respect to the
// lifetime of the WSZ generated.
//
// NB: this should be done also for UTF-8 and MBCS targets as necessary.
//
inline wchar_t * _GetSZUnicodeFromSZUTF8(SZ_CONST szUtf8, Buffer & bufwsz) {
    size_t cchUnicode = utflen(szUtf8);
    size_t cbUnicode = sizeof(wchar_t) * cchUnicode;
    if ( static_cast<size_t>(bufwsz.Size()) < cbUnicode ) {
        if ( !bufwsz.Reserve(static_cast<CB>(cbUnicode - bufwsz.Size())) ) {
            return NULL;
        }
    }
    return _GetSZUnicodeFromSZUTF8(
        szUtf8,
        reinterpret_cast<wchar_t*>(bufwsz.Start()),
        cchUnicode
        );
}
