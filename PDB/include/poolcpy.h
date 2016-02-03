#pragma once
#ifndef __POOL_COPY__
#define __POOL_COPY__

#include "szst.h"
#include "pool.h"

// Return a 0-terminated string copied from the 0-terminated string,
// or 0 if memory allocation failure.
//
inline SZ szCopyPool(SZ_CONST szFrom, POOL<4>& pool)
{
    size_t cch = strlen(szFrom);
    SZ sz = new (pool) char[cch + 1];
    if (sz)
        memcpy(sz, szFrom, cch + 1);
    return sz;
}

inline SZ szCopyPool(SZ_CONST szFrom, POOL<8>& pool)
{
    size_t cch = strlen(szFrom);
    SZ sz = new (pool) char[cch + 1];
    if (sz)
        memcpy(sz, szFrom, cch + 1);
    return sz;
}

#endif
