#pragma once
#include <mrengine.h>

enum {
    sigMagic = ' SRD',
    sigFileStream = ' srd',
    cbitsName = 256,
    cbMapInit = 4096,
    cbFileInfoInit = 4096,
    sgnLess = -1,
    sgnEqual = 0,
    sgnGreater = 1,
    cchMrePathMax = _MAX_PATH + 40,
    cbStreamPage = 4096,
};

bool
__forceinline
FNonNullSz(const wchar_t * sz) {
    return (sz != NULL) && ((*sz) != L'\0');
}

bool
__forceinline
FNonNullSz(const char * sz) {
    return (sz != NULL) && ((*sz) != '\0');
}
