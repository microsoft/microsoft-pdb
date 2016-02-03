#pragma once

#include "assert_.h"

class CriticalSectionNop
{
public:
    CriticalSectionNop(DWORD = 0) {}
    ~CriticalSectionNop()  {}
    BOOL Enter() { return TRUE; }
    void Leave() {}

    BOOL TryEnter() { return FALSE; }
    BOOL FLocked() { return FALSE; }
    BOOL FAcquired() { return TRUE; }
};



class CriticalSectionNoLog
{
public:
    enum {
        scPreAlloc = 0x80000000,
    };

    CriticalSectionNoLog(DWORD dwSpinCount = 0) {
        verify(Init(dwSpinCount));
    }
    ~CriticalSectionNoLog()  {
        DeleteCriticalSection(&m_sec);
    }
    BOOL Enter() {
        EnterCriticalSection(&m_sec);
        return TRUE;
    }
    void Leave() {
        LeaveCriticalSection(&m_sec);
    }

    BOOL TryEnter() {
        return TryEnterCriticalSection(&m_sec);
    }

    BOOL FLocked() {
        return m_sec.LockCount != 0;
    }

    BOOL FAcquired() {
        return m_sec.OwningThread == reinterpret_cast<HANDLE>(static_cast<INT_PTR>(GetCurrentThreadId()));
    }

protected:
    CRITICAL_SECTION m_sec;

    BOOL Init(DWORD dwSpinCount) {
        return InitializeCriticalSectionAndSpinCount(&m_sec, dwSpinCount);
    }
};

#if 0
class CriticalSectionWithLog : public CriticalSectionNoLog 
{
public:
    enum {
        scPreAlloc = 0x80000000,
    };

    CriticalSectionWithLog(DWORD dwSpinCount = 0) : CriticalSectionNoLog(dwSpinCount) {
        PDBLOG(LOG_CTOR);
    }
    ~CriticalSectionWithLog()  {
        PDBLOG(LOG_DTOR);
    }
    BOOL Enter() {
        CriticalSectionNoLog::Enter();
        PDBLOG(LOG_LOCK);
        return TRUE;
    }
    void Leave() {
        PDBLOG(LOG_RELEASE);
        CriticalSectionNoLog::Leave();
    }

    BOOL TryEnter() {
        BOOL ret = CriticalSectionNoLog::TryEnter();
        PDBLOG(ret? LOG_LOCK : LOG_LOCK_FAILED);
        return ret;
    }

    BOOL FLocked() {
        return CriticalSectionNoLog::FLocked();
    }

    BOOL FAcquired() {
        return CriticalSectionNoLog::FAcquired();
    }
};

typedef CriticalSectionWithLog CriticalSection;

#else

typedef CriticalSectionNoLog CriticalSection;

#endif 

template<class T>
class AutoLockTemplate
{
public:
    AutoLockTemplate(T *pCS) {
        m_pCS = pCS;
        m_pCS->Enter();
    }
    ~AutoLockTemplate() {
        m_pCS->Leave();
    }

protected:
    T *m_pCS;
};

typedef AutoLockTemplate<CriticalSection> AutoLock;

template<class T>
class AutoLockWithConditionTemplate
{
public:
    AutoLockWithConditionTemplate(T *pCS, bool f) {
        m_f = f;

        if (f) {
            m_pCS = pCS;
            m_pCS->Enter();
        }
    }
    ~AutoLockWithConditionTemplate() {
        if (m_f) {
            m_pCS->Leave();
        }
    }

protected:
    T *m_pCS;
    bool m_f;
};
