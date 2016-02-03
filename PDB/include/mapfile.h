#pragma once
#if !defined verify
#ifdef _DEBUG
#define verify(x)       assert((x))
#else
#define verify(x)       (x)
#endif
#endif  // verify

#if defined(MAPPEDMEM_LOGGING)

#define LogMsg(szFmt, arg1, arg2) \
    printf("Function: %s: " szFmt, __FUNCTION__, arg1, arg2), fflush(stdout)

#endif




// define MAPPED_IO to get mapped file I/O in makefile

class MemoryMappedFile 
{
    enum 
    { 
        cbMinFileSize   = 0x4000,        // Minimum file size is 16KB
        cbDefFileSize   = 0x400000,      // default file size 4MB
        cbGrowth        = 0x400000,      // increase file size by 4MB
    };

protected:

#ifdef _DEBUG
    wchar_t m_wszFileName[260];
    unsigned __int64 ComputeChecksum();
#endif
    union { void *m_pv; BYTE *m_pb; };

    HANDLE          m_hFile;
    ULARGE_INTEGER  m_ulLen;        // user's know of this length
    ULARGE_INTEGER  m_ulPos;
    BOOL            m_fWrite;
    BOOL            m_fRemote;      // Is this a remote file?

    BOOL fInvalidate()
    {
        if (m_pb != NULL && UnmapViewOfFile(m_pv)) {
#if defined(MAPPEDMEM_LOGGING)
            LogMsg("UnmapViewOfFile failed, error = 0x%08x, base = 0x%p\n", GetLastError(), m_pv);
#endif
            m_pv = NULL;
        }

        if (m_hFile != INVALID_HANDLE_VALUE && CloseHandle(m_hFile))
            m_hFile = INVALID_HANDLE_VALUE;

        return (m_pv == NULL && m_hFile == INVALID_HANDLE_VALUE);
    }

    BOOL SetFileSize(ULARGE_INTEGER ulNewSize);
    BOOL fMapFileToMemory();

public :

    // Standard construction / destruction
    MemoryMappedFile() { }

    ~MemoryMappedFile();

    BOOL Init(const _TCHAR *szFilename, BOOL fWrite, BOOL& fCreated);
    BOOL Detach(HANDLE *pHandle);            // Detach from the m_hFile

    bool operator !()
    {
        return (m_hFile == INVALID_HANDLE_VALUE) || (m_pb == NULL);
    }

    ULONG Read(void *buffer, ULONG count)
    {
        ULONG read = 0;

        if (m_pv == NULL) {
            assert( FALSE );
            return read;
        }

        // make sure we are not reading beyond EOF
        if (m_ulPos.QuadPart < m_ulLen.QuadPart) {      

            read = __min(count, ULONG(m_ulLen.QuadPart - m_ulPos.QuadPart));
            assert(m_ulPos.QuadPart + read <= m_ulLen.QuadPart); 

            DWORD   dwExcept;
            DWORD   cmsSleep = 0;

            do {
                __try {
                    dwExcept = 0;
                    memmove(buffer, m_pb + m_ulPos.QuadPart, read);
                }
                __except ( dwExcept = GetExceptionCode(),
                           (dwExcept == EXCEPTION_ACCESS_VIOLATION ||
                           dwExcept == EXCEPTION_IN_PAGE_ERROR) )
                {
                    ::Sleep(cmsSleep);
                    cmsSleep += 200;
                }
            } while (dwExcept == EXCEPTION_IN_PAGE_ERROR && cmsSleep <= 5000);

            if (dwExcept) {
                assert(FALSE);
                return 0;
            }

            m_ulPos.QuadPart += read;
        }

        return read;
    }

    ULONG Write(const void *buffer, ULONG count)
    {
        assert(m_ulPos.QuadPart + count <= m_ulLen.QuadPart); // Assert for now, later we can extend the file

        if (!m_fWrite)
            return 0;

        if (m_pv == NULL) {
            assert( FALSE );
            return 0;
        }

        DWORD   dwExcept;
        DWORD   cmsSleep = 0;

        do {
            __try {
                dwExcept = 0;
                memmove(m_pb + m_ulPos.QuadPart, buffer, count);
            }
            __except ( dwExcept = GetExceptionCode(),
                       (dwExcept == EXCEPTION_ACCESS_VIOLATION ||
                       dwExcept == EXCEPTION_IN_PAGE_ERROR) )
            {
                ::Sleep(cmsSleep);
                cmsSleep += 200;
            }
        } while (dwExcept == EXCEPTION_IN_PAGE_ERROR && cmsSleep <= 5000);

        if (dwExcept) {
            assert(FALSE);
            return 0;
        }

        m_ulPos.QuadPart += count;
        return count;
    }

    ULARGE_INTEGER Seek(LARGE_INTEGER offset, DWORD dwOrigin)
    {
        ULARGE_INTEGER whence;

        switch(dwOrigin)
        {
        case SEEK_SET: whence.QuadPart = 0; break;
        case SEEK_CUR: whence = m_ulPos; break;
        case SEEK_END: whence = m_ulLen; break;
        default: 
            whence.QuadPart = -1;
            return whence;
        }

        whence.QuadPart += offset.QuadPart;

        // Seek beyond the EOF??
        if ((whence.QuadPart >= m_ulLen.QuadPart) && !fSetSize(whence)) {
            whence.QuadPart = -1;
            return whence;
        }

        return (m_ulPos = whence);
    }

    BOOL fSetSize(ULARGE_INTEGER ulNewSize);
    BOOL fFlush();
};
