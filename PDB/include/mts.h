#ifndef __MTS_H__
#define __MTS_H__

#include "assert_.h"

#if !defined(_DEBUG) || !defined(PDB_MT)
#define MTS_ASSERT_SINGLETHREAD()
#define USE_ASSURE_SINGLETHREAD()
#else
#define MTS_ASSERT_SINGLETHREAD() AutoAssureSingleThread _autost##n(_ast)
#define USE_ASSURE_SINGLETHREAD() AssureSingleThread _ast

class AssureSingleThread {
public:
    AssureSingleThread() : tid(0) {};
private:
    LONG tid;
    friend class AutoAssureSingleThread;
};

class AutoAssureSingleThread {
public:
    AutoAssureSingleThread(AssureSingleThread& ast) : m_ptid(&ast.tid) {
        LONG tid = InterlockedExchange(m_ptid, (LONG)GetCurrentThreadId());
        fIgnore = (tid == GetCurrentThreadId());      // re-entrent
        assert(tid == NULL || fIgnore);
    };
    ~AutoAssureSingleThread() {
        LONG tid = InterlockedExchange(m_ptid, (fIgnore? (LONG)GetCurrentThreadId() : 0));
        assert(tid == GetCurrentThreadId());
    }
private:
    LONG * m_ptid;
    bool fIgnore;
};
#endif // _DEBUG

// PDB_MT ONLY
#ifndef PDB_MT

#define MTS_PROTECT_T(T, lock)
#define MTS_PROTECTNOLOG(lock)
#define MTS_PROTECT(lock)
#define MTS_PROTECT_COND(lock, f)

#define MTS_ENTER(lock) 
#define MTS_LEAVE(lock)

#define MTS_ASSERT(lock)

#include "pdblog.h"
#include "critsec.h"

#else

#define _MTS_STRINGIZING(x) #x
#define MTS_STRINGIZING(x) _MTS_STRINGIZING(x)
#define _MTS_MESSAGE(a, b, x) "TODO: " a "(" MTS_STRINGIZING(b) "): " x

#define _MTS_PROTECT2(T, lock, n) AutoLockTemplate<T> autolock##n(&lock);
#define _MTS_PROTECT(T, lock, n) _MTS_PROTECT2(T, lock, n)

#define _MTS_PROTECT2_COND(T, lock, f, n) AutoLockWithConditionTemplate<T> autolock##n(&lock, f);
#define _MTS_PROTECT_COND(T, lock, f, n) _MTS_PROTECT2_COND(T, lock, f, n)

#define MTS_MESSAGE(x) _MTS_MESSAGE(__FILE__, __LINE__, x)

#ifdef PDB_LOGGING
#define MTS_PROTECTNOLOG(lock) _MTS_PROTECT(CriticalSectionNoLog, lock, __COUNTER__)
#endif

#define MTS_ENTER(lock) verify(lock.Enter())
#define MTS_LEAVE(lock) verify(lock.Leave())

#include "pdblog.h"
#include "critsec.h"
#include "syncro.h"

#define MTS_PROTECT_T(T, lock) _MTS_PROTECT(T, lock, __COUNTER__)
#define MTS_PROTECT(lock) MTS_PROTECT_T(CriticalSection, lock)
#define MTS_ASSERT(lock) assert(lock.FAcquired())

#define MTS_PROTECT_T_COND(T, lock, f) _MTS_PROTECT_COND(T, lock, f, __COUNTER__)
#define MTS_PROTECT_COND(lock, f) MTS_PROTECT_T_COND(CriticalSection, lock, f)

#endif // PDB_MT
#endif 
