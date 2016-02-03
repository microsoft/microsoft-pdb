#pragma once
#if !defined(__HEAP_INCLUDED__)
#define __HEAP_INCLUDED__

#if !defined(STRICT)
#define STRICT
#endif
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <malloc.h>

#ifdef HeapDump
#include <stdio.h>
#endif

#if defined(CRT_LEAKS)
#include <stdlib.h>
#include <crtdbg.h>
#endif

#if defined(PDB_SERVER) || defined(MSOBJ_DLL) || defined(STANDALONE_HEAP)
#define INLINE inline

class Heap {
    Heap() 
    {
#if !defined(CRT_LEAKS)
        hheap = HeapCreate(0, 0, 0);
        assert(hheap);
#else
        _CrtSetDbgFlag((_CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF) | _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG));
#endif
    }

    static Heap theHeap;

public:
    ~Heap() 
    {
#if defined(CRT_LEAKS)
        _CrtDumpMemoryLeaks();
#else
        verify (HeapDestroy(hheap));
#endif
    }
    static HANDLE hheap;
};

INLINE void* __cdecl operator new (size_t size)
{
#ifdef HeapDump
    printf("Allocation size = %d\n", size);
#endif
#if defined(CRT_LEAKS)
    return malloc(size);
#else
    if (Heap::hheap == NULL) {
        return NULL;
    }

    void* pv = nullptr;

    do {
        pv = HeapAlloc(Heap::hheap, 0, size);
    } while(!pv && _callnewh(size));

    return pv;
#endif
}

_Ret_bytecap_(_Size) INLINE void* __cdecl operator new[] (size_t _Size)
{
#ifdef HeapDump
    printf("Allocation size = %d\n", _Size);
#endif
#if defined(CRT_LEAKS)
    return malloc(size);
#else
    if (Heap::hheap == NULL) {
        return NULL;
    }

    void* pv = nullptr;

    do {
        pv = HeapAlloc(Heap::hheap, 0, _Size);
    } while(!pv && _callnewh(_Size));

    return pv;
#endif
}

INLINE void __cdecl operator delete (void *pv)
{
    // operator delete allows NULL parameter... must not pass to OS
    if (!pv)
        return;
#if defined(CRT_LEAKS)
    free(pv);
#else
    if (Heap::hheap != NULL) {
        verify (HeapFree(Heap::hheap, 0, pv));
    }
#endif
} 

INLINE void __cdecl operator delete[] (void *pv)
{
    // operator delete allows NULL parameter... must not pass to OS
    if (!pv)
        return;
#if defined(CRT_LEAKS)
    free(pv);
#else
    if (Heap::hheap != NULL) {
        verify (HeapFree(Heap::hheap, 0, pv));
    }
#endif
}

#else // if not (defined(PDB_SERVER) || defined(MSOBJ_DLL) || defined(STANDALONE_HEAP))

void __cdecl operator delete(void *);
void *__cdecl operator new(size_t);
void * __cdecl operator new[](size_t cb);
void __cdecl operator delete[](void * pv);

#endif 

enum FILL { zeroed = 0 };
inline void* __cdecl operator new(size_t size, FILL) {
    BYTE* pb = new BYTE[size];
    if (pb)
        memset(pb, 0, size);
    return pb;
}

inline void* __cdecl operator new[](size_t size, FILL) {
    BYTE* pb = new BYTE[size];
    if (pb)
        memset(pb, 0, size);
    return pb;
}


#endif    /* __HEAP_INCLUDED__ */
