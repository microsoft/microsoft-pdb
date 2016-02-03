#pragma once

class Event {
public :
    Event( BOOL fManualReset = FALSE ) : m_hEvent( NULL ) {
        verify( Init(fManualReset) );
    }
    ~Event( ) {
        if ( m_hEvent ) {
            CloseHandle( m_hEvent );
        }
    }
    BOOL Set() {
        return SetEvent( m_hEvent );
    }
    BOOL Reset() {
        return ResetEvent( m_hEvent );
    }
    BOOL Wait( DWORD dwmsMaxWait ) {
        DWORD dwRet = WaitForSingleObject( m_hEvent, dwmsMaxWait );
        return dwRet == WAIT_OBJECT_0;
    }
    HANDLE GetHandle() {
        return m_hEvent;
    }
protected:
    HANDLE m_hEvent;
    BOOL Init( BOOL fManualReset ) {
        m_hEvent = CreateEventW(NULL, fManualReset, FALSE, NULL);
        return m_hEvent != NULL;
    }
};

class Mutex {
public:
    Mutex() : m_hMutex( 0 ), m_tidOwner( 0 ) {
        verify( Init() );
    }
    ~Mutex( ) {
        if ( m_hMutex ) {
            CloseHandle( m_hMutex );
        }
    }
    BOOL Enter() {
        DWORD dwRet = WaitForSingleObject( m_hMutex, INFINITE );
        if ( dwRet == WAIT_OBJECT_0 ) {
            m_tidOwner = GetCurrentThreadId();
        }
        return dwRet == WAIT_OBJECT_0;
    }
    BOOL Leave() {
        BOOL fResult;
        if ( fResult = ReleaseMutex( m_hMutex ) ) {
            m_tidOwner = 0;
        }
        return fResult;
    }
    HANDLE GetHandle() {
        return m_hMutex;
    }
protected:
    HANDLE m_hMutex;
    DWORD  m_tidOwner;
    BOOL Init() {
        m_hMutex = CreateMutexW( NULL, FALSE, NULL );
        return m_hMutex != NULL;
    }
};

class Semaphore {
public:
    Semaphore( DWORD nMaxThreads ) : m_hSemaphore( 0 ), m_nMaxThreads( nMaxThreads ) {
        m_hSemaphore = CreateSemaphoreA(NULL, 0, nMaxThreads, NULL);
    }
    ~Semaphore() {
        CloseHandle(m_hSemaphore);
    }
    BOOL SetCount( DWORD count ) {
        return ReleaseSemaphore(m_hSemaphore, count, NULL);
    }
    BOOL Acquire( DWORD dwmsMaxWait ) {
        DWORD dwRet = WaitForSingleObject( m_hSemaphore, dwmsMaxWait );
        return dwRet == WAIT_OBJECT_0;
    }
    BOOL Release() {
        return ReleaseSemaphore(m_hSemaphore, 1, NULL);
    }
    BOOL ReleaseAll() {
        return ReleaseSemaphore(m_hSemaphore, m_nMaxThreads, NULL);
    }
    HANDLE GetHandle() {
        return m_hSemaphore;
    }
private:
    HANDLE m_hSemaphore;
    DWORD  m_nMaxThreads;
};
