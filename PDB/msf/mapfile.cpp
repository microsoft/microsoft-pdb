#define UNICODE
#define _UNICODE
#include <tchar.h>

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <objidl.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <share.h>
#include <memory.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <assert_.h>

#include "mapfile.h"

#ifdef _DEBUG
#include "misc.h"
unsigned __int64 MemoryMappedFile::ComputeChecksum()
{
    unsigned __int64 sum = LHashPbCb(m_pb, m_ulLen.LowPart, 0x1000000);
    return sum;
}
#endif


static BOOL IsRemoteFile(const wchar_t *wszFilename)
{
    wchar_t wszPath[_MAX_PATH], wszDrv[_MAX_DRIVE+1], wszDir[_MAX_DIR], wszFName[_MAX_FNAME], wszExt[_MAX_EXT];

    if (NULL == _wfullpath(wszPath, wszFilename, _MAX_PATH))
    {
        // If _wfullpath fails, assume the worst
        return TRUE;
    }

    if (wszPath[0] == L'\\' && wszPath[1] == L'\\')
        return TRUE;

    if (0 != _wsplitpath_s(wszPath, wszDrv, _countof(wszDrv), wszDir, _countof(wszDir), wszFName, _countof(wszFName), wszExt, _countof(wszExt)))
    {
        // If _wsplitpath_s fails, assume the worst
        return TRUE;
    }

    wcsncat_s(wszDrv, _countof(wszDrv), L"\\", 1);

    DWORD dwDrvType = ::GetDriveTypeW(wszDrv);
    return (dwDrvType == DRIVE_UNKNOWN || dwDrvType == DRIVE_REMOTE);
}


static HANDLE MyCreateFile(
  const wchar_t *wszFilename,                 // file name
  DWORD dwDesiredAccess,                      // access mode
  DWORD dwShareMode,                          // share mode
  LPSECURITY_ATTRIBUTES lpSecurityAttributes, // SD
  DWORD dwCreationDisposition,                // how to create
  DWORD dwFlagsAndAttributes,                 // file attributes
  HANDLE hTemplateFile                        // handle to template file
)
{
    dwShareMode |= FILE_SHARE_DELETE;      // Allow others to delete this file

    HANDLE hFile = ::CreateFileW(wszFilename,
                                 dwDesiredAccess,
                                 dwShareMode,
                                 lpSecurityAttributes,
                                 dwCreationDisposition,
                                 dwFlagsAndAttributes,
                                 hTemplateFile);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD dwType = GetFileType(hFile);
        if (dwType != FILE_TYPE_DISK)
        {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
    }

    return hFile;
}


BOOL MemoryMappedFile::Init(const wchar_t *wszFilename, BOOL fWrite, BOOL& fCreated)
{
    DWORD    fdwShare;
    DWORD    fdwAccess;

    m_hFile  = INVALID_HANDLE_VALUE;
    m_pb     = NULL;
    m_fWrite = fWrite;

    if (m_fWrite) 
    {
        fdwAccess = GENERIC_READ | GENERIC_WRITE;
        fdwShare  = 0;
    }
    else
    {
        fdwAccess = GENERIC_READ;
        fdwShare  = FILE_SHARE_READ;
    }

    m_ulLen.QuadPart = 0;
    m_ulPos.QuadPart = 0;

#ifdef _DEBUG
    wcsncpy_s(m_wszFileName, _countof(m_wszFileName), wszFilename, _TRUNCATE);
#endif

    // Is this a remote file?
    m_fRemote = (m_fWrite && IsRemoteFile(wszFilename));

    // Try to open the file
    m_hFile = ::MyCreateFile(wszFilename, 
                            fdwAccess,
                            fdwShare, 
                            NULL, 
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        fCreated = FALSE;
        m_ulLen.QuadPart = GetFileSize(m_hFile, NULL);
        return fMapFileToMemory();
    }
    else if (fWrite)
    {
        // Try creating it
        m_hFile = ::MyCreateFile(wszFilename, 
                                fdwAccess,
                                fdwShare, 
                                NULL, 
                                OPEN_ALWAYS, 
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);

        if (m_hFile == INVALID_HANDLE_VALUE)
            return FALSE;

        fCreated = TRUE;

        ULARGE_INTEGER ulNewSize;
        ulNewSize.QuadPart = cbDefFileSize;
        if (!SetFileSize(ulNewSize))
        {
            // Could not create with default size
            // try with minimum size
            ulNewSize.QuadPart = cbMinFileSize;
            if (!SetFileSize(ulNewSize))
                // Bad luck, not enough space
                return FALSE;
        }

        return fMapFileToMemory();
    }

    return FALSE;
}

MemoryMappedFile::~MemoryMappedFile()
{
    // Unmap the file
    if (m_pv != NULL)
    {
        if ((!m_fRemote || FlushViewOfFile(m_pv, 0)) && UnmapViewOfFile(m_pv)) {
#if defined(MAPPEDMEM_LOGGING)
            LogMsg("FlushViewOfFile or UnmapViewOfFile failed, error = 0x%08x, base = 0x%p\n", GetLastError(), m_pv);
#endif

            m_pv = NULL;
        }
    }

    assert(m_pv == NULL);

    verify(!m_fWrite || m_hFile == INVALID_HANDLE_VALUE || SetFileSize(m_ulLen));
    verify(fInvalidate());
}

BOOL MemoryMappedFile::Detach(HANDLE *pHandle)
{
    // Unmap the file
    if (m_pv != NULL)
    {
        // Commit everything and lose
        // the memory mapping ...
        BOOL    fRet = FlushViewOfFile(m_pv, 0);

#if defined(MAPPEDMEM_LOGGING)
        if (!fRet) {
            LogMsg("FlushViewOfFile failed, error = 0x%08x, base = 0x%p\n", GetLastError(), m_pv);
        }
#endif

        fRet = UnmapViewOfFile(m_pv);

#if defined(MAPPEDMEM_LOGGING)
        if (!fRet) {
            LogMsg("UnmapViewOfFile failed, error = 0x%08x, base = 0x%p\n", GetLastError(), m_pv);
        }
#endif
        m_pv = NULL;
    }

    // Set the file pointer to the appropriate position
    if (::SetFilePointer(m_hFile, m_ulPos.LowPart, NULL, FILE_BEGIN) == m_ulPos.LowPart ) {
        *pHandle = m_hFile;
        m_hFile = INVALID_HANDLE_VALUE;
        return TRUE;
    }

#if defined(MAPPEDMEM_LOGGING)
    LogMsg("SetFilePointer failed, error = 0x%08x, offset = 0x%08x\n", GetLastError(), m_ulPos.LowPart);
#endif

    // Some error occured, try to map the file to memory again
    *pHandle = INVALID_HANDLE_VALUE;
    if ( fMapFileToMemory( ) ) {
        return TRUE;
    }

    // Something really bad happened, we can't even
    // map the memory back, i.e. this MemMapped file
    // is completely unusable!
    return FALSE;
}

BOOL MemoryMappedFile::fSetSize(ULARGE_INTEGER ulNewSize)
{
    DWORD dwFileSize = GetFileSize(m_hFile, NULL);

    // If we already have reserved the space ...
    if (ulNewSize.QuadPart <= dwFileSize)
    {
        m_ulLen.QuadPart = ulNewSize.QuadPart;
        return TRUE;
    }

#ifdef _DEBUG
    unsigned __int64 nCheckOld = ComputeChecksum();
#endif

    // Remove the previous mapping
    if (m_pv != NULL) 
    {
        if ((m_fRemote && !FlushViewOfFile(m_pv, 0)) || !UnmapViewOfFile(m_pv))
        {
#if defined(MAPPEDMEM_LOGGING)
            LogMsg("FlushViewOfFile or UnmapViewOfFile failed, error = 0x%08x, base = 0x%p\n", GetLastError(), m_pv);
#endif
            assert(FALSE);
            return FALSE;
        }
    }

    // Compute new size, criterion 
    // If filesize < 4MB, increment by 4MB
    // else double the file size.
    ULARGE_INTEGER ulMySize;
    ulMySize.QuadPart = dwFileSize;
    
    do
    {
        ulMySize.QuadPart += __max(dwFileSize, cbGrowth);

    } while (ulNewSize.QuadPart > ulMySize.QuadPart);

#if 0
    if ( ulMySize.QuadPart > 0x500000 ) {
        // Test : can't use memmapped I/O for files 
        //        larger than 5MB
        printf("switching over to CRT I/O\n");
        return FALSE;
    }
#endif

    // Try setting this file size, if we can't, then try adjusting it
    while (!SetFileSize(ulMySize) && ulMySize.QuadPart > ulNewSize.QuadPart)
        ulMySize.QuadPart -= __max(dwFileSize, cbGrowth)/0x04;

    if (ulMySize.QuadPart <= ulNewSize.QuadPart)
    {
        // We could not set an optimisticly big size, probably no space
        // available, try to allocate just as much as they asked for
        ulMySize.QuadPart = ulNewSize.QuadPart;
        if (!SetFileSize(ulMySize))
        {
            // Hard luck, we are out of space.
            return FALSE;
        }
    }

    assert(ulMySize.QuadPart >= ulNewSize.QuadPart);

    // Map the new file to the memory
    if (!fMapFileToMemory())
        return FALSE;

#ifdef _DEBUG
    unsigned __int64 nCheckNew = ComputeChecksum();
    assert(nCheckOld == nCheckNew);
#endif

    m_ulLen.QuadPart = ulNewSize.QuadPart;
    return TRUE;
}

BOOL MemoryMappedFile::SetFileSize(ULARGE_INTEGER ulNewSize)
{
    assert(ulNewSize.QuadPart < 0x100000000);   // This code can't handle sizes beyond 32 bit.
                                                // so your max file size is limited to 4GB

    // Seek to a position
    if (::SetFilePointer(m_hFile, ulNewSize.LowPart, NULL, FILE_BEGIN) != ulNewSize.LowPart) {
        DWORD   dwErr = GetLastError();
#if defined(MAPPEDMEM_LOGGING)
        LogMsg("SetFilePointer failed, error = 0x%08x, offset = 0x%08x\n", dwErr, ulNewSize.LowPart);
#endif
        assert(dwErr != NO_ERROR);
        return FALSE;
    }

    // Set the eof there
    BOOL    fRet = ::SetEndOfFile(m_hFile);
#if defined(MAPPEDMEM_LOGGING)
    if (!fRet) {
        LogMsg("SetEndOfFile failed, error = 0x%08x, offset = 0x%08x\n", GetLastError(), ulNewSize.LowPart);
    }
#endif
    return fRet;
}

BOOL MemoryMappedFile::fMapFileToMemory()
{
    // Map all the file to the memory
    DWORD fdwProtect = PAGE_READONLY;
    DWORD fdwAccess = FILE_MAP_READ;

    if (m_fWrite)
    {
        fdwProtect = PAGE_READWRITE;
        fdwAccess  = FILE_MAP_ALL_ACCESS;
    }

    HANDLE hMapObject = ::CreateFileMappingW(m_hFile, NULL, fdwProtect, 0, 0, NULL);

    if (hMapObject == NULL) {
#if defined(MAPPEDMEM_LOGGING)
        LogMsg("CreateFileMapping failed, error = 0x%08x, protect mask = 0x%08x\n", GetLastError(), fdwProtect);
#endif
        return FALSE;
    }

    m_pv = ::MapViewOfFileEx(hMapObject, fdwAccess, 0, 0, 0, NULL);

#if defined(MAPPEDMEM_LOGGING)
    if (m_pv == NULL) {
        LogMsg("MapViewOfFileEx failed, error = 0x%08x, access mask = 0x%08x\n", GetLastError(), fdwAccess);
    }
#endif

    BOOL    fClose = CloseHandle(hMapObject);
#if defined(MAPPEDMEM_LOGGING)
    if (!fClose) {
        LogMsg("CloseHandle failed, error = 0x%08x\n", GetLastError(), 0);
    }
#endif


    if (m_pv == NULL) {
        return FALSE;
    }

    return TRUE;
}

BOOL MemoryMappedFile::fFlush()
{
    assert(m_pv);
    assert(m_ulLen.QuadPart < 0x100000000);     // This code can't handle sizes beyond 32 bit.
                                                // so your max file size is limited to 4GB
    BOOL    fRet = FlushViewOfFile(m_pv, m_ulLen.LowPart);

#if defined(MAPPEDMEM_LOGGING)
    if (!fRet) {
        LogMsg("FlushViewOfFile failed, error = 0x%08x, offset = 0x%08x\n", GetLastError(), m_ulLen.LowPart);
    }
#endif
    return fRet;
}
