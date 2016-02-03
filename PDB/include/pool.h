// Pool memory allocator
#pragma once
#ifndef __POOL_INCLUDED__
#define __POOL_INCLUDED__

#include "pdbimpl.h"

#include "mdalign.h"

#include "SysPage.h"

#pragma warning(disable:4200)

// A 'hard' typedef for use as the placement new operand to avoid conflicts
// with the C++14 sized deallocator.
enum class blkpool_t : size_t {};

struct BLK { // block in an allocation pool
    size_t  cbFree;
    BYTE*   pFree;
    BLK*    pNext;
    BYTE    buf[];

    BLK(size_t cb) {
        cbFree = cb;
        pFree = buf;
        pNext = 0;
    }
    void* alloc(size_t cb) {
        assert(cbAlign(cb) == cb);
        // The only place that calls BLK::alloc() already aligns memory
        // properly, so don't bother doing it again.
        // cb = cbAlign(cb);   // Round up for alignment
        if (cb <= cbFree) {
            cbFree -= cb;
            BYTE* p = pFree;
            pFree += cb;
            return p;
        }
        return 0;
    }
    void flush() {
        cbFree = 0;
    }
    CB cb() {
        return CB(pFree - buf); // REVIEW:WIN64 CAST
    }
};

inline size_t cbRoundUp(size_t cb, size_t cbMult) {
    return (cb + cbMult-1) & ~(cbMult-1);
}

inline void* __cdecl operator new(size_t size, blkpool_t cb) {
    return new char[size + static_cast<size_t>(cb)];
}

template <size_t cbAlignVal>
struct POOL { // allocation pool
    BLK*    pHead;
    BLK*    pTail;
    size_t  cbTotal;

    POOL() {
        assert(bitcount(cbAlignVal) == 1);
        init();
    }
    ~POOL() {
        uninit();
    }
    size_t cbAlign(size_t cb) {
        return ((cb + cbAlignVal - 1)) & ~(cbAlignVal - 1);
    }
    void* alloc(size_t cbT) {
        size_t cb = cbRoundUp(cbT, cbAlignVal); // Round up for alignment
        if (cb < cbT) {
            // overflow
            return NULL;
        }
        void* p = pTail->alloc(cb);
        if (!p) {
            size_t  cbBlkAlloc = cbRoundUp(cb + cbSysPage, cbSysPage);
            if (cbBlkAlloc < cb) {
                return NULL;
            }
            if (pTail->pNext = new (static_cast<blkpool_t>(cbBlkAlloc)) BLK(cbBlkAlloc)) {
                pTail = pTail->pNext;
                p = pTail->alloc(cb);
            }
        }
        if (p) {
            cbTotal += cb;
        }
        return p;
    }
    void discard(CB cb) {
        cbTotal -= cbRoundUp(cb, cbAlignVal);
    }
    void blkFlush() {
        dassert(pTail);
        pTail->flush();
    }
    size_t cb() {
        return cbTotal;
    }

    void xferPool(POOL& poolSrc) {
        // we need to uninit us first and copy the data from the
        // source pool, then init the source pool w/o an uninit()
        uninit();

        pHead = poolSrc.pHead;
        pTail = poolSrc.pTail;
        cbTotal = poolSrc.cbTotal;

        poolSrc.init();
    }

private:
    void init() {
        pHead = new (static_cast<blkpool_t>(0)) BLK(static_cast<size_t>(0));
        pTail = pHead;
        cbTotal = 0;
    }
    void uninit() {
        BLK* pNext;
        for (BLK* pblk = pHead; pblk; pblk = pNext) {
            pNext = pblk->pNext;
            delete pblk;
        }
    }

    POOL(const POOL&) {
        assert(0);
    } // error to copy a pool
};

typedef POOL<4>             POOL_Align4;    // for things we have to align to 4.
typedef POOL<sizeof(void*)> POOL_AlignNative;   // for things we align natively (ptrs).

template <size_t cbAlignVal>
inline void * __cdecl operator new(size_t size, POOL<cbAlignVal> & pool) {
    return pool.alloc(size);
}
template <size_t cbAlignVal>
inline void * __cdecl operator new[](size_t size, POOL<cbAlignVal> & pool) {
    return pool.alloc(size);
}

#endif // !__POOL_INCLUDED__
