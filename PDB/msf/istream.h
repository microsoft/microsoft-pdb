//////////////////////////////////////
// istream.h
#pragma once


#include "crtfubar.h"

class SymBase : public IUnknown
{
public: 
    long        m_refCount;

    SymBase() : m_refCount(0) {}
    virtual ~SymBase() {}

    //-----------------------------------------------------------
    // IUnknown support
    //-----------------------------------------------------------
    ULONG STDMETHODCALLTYPE AddRef() 
    {
        return (InterlockedIncrement((long *) &m_refCount));
    }

    ULONG STDMETHODCALLTYPE Release() 
    {
        long refCount = InterlockedDecrement((long *) &m_refCount);
        if (refCount == 0)
            delete this;

        return (refCount);
    }
};

class IStreamInternal : public IStream
{
public:
    enum StreamType {
        StreamTypeUnknown,
        StreamTypeRealIStream,
        StreamTypeFile,
        StreamTypeMemMappedFile,
    };


    // Implememtation for some of the IStream methods
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE CopyTo( 
        /* [unique][in] */ IStream __RPC_FAR *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbRead,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbWritten)
    {
        assert( false );
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE Revert( void)
    {
        assert( false );
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE LockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        assert( false );
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        assert( false );
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Stat( 
        /* [out] */ STATSTG __RPC_FAR *pstatstg,
        /* [in] */ DWORD grfStatFlag)
    {
        assert( false );
        return E_NOTIMPL;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Clone( 
        /* [out] */ IStream __RPC_FAR *__RPC_FAR *ppstm)
    {
        assert( false );
        return E_NOTIMPL;
    }

    // IStreamInternal requirements
    virtual StreamType GetType() = 0;
    virtual HANDLE     GetFileHandle() = 0;
};

class IStreamReal : public IStreamInternal, public SymBase
{
public:
    static IStreamInternal* Create( IStream *pRealStream)
    {
        IStreamReal *pIStream = new IStreamReal(pRealStream) ;
        return pIStream;
    }

    IStreamReal(IStream *pIStream) {
        pIStream->QueryInterface( 
            __uuidof(IStream),
            (void **)&m_pIStream);
    }

    ~IStreamReal() {
        m_pIStream->Release();
    }

    //-----------------------------------------------------------
    // IUnknown
    //-----------------------------------------------------------
    ULONG STDMETHODCALLTYPE AddRef() {return (SymBase::AddRef());}
    ULONG STDMETHODCALLTYPE Release() {return (SymBase::Release());}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppInterface)
    {
        return E_NOTIMPL;
    }

    //-----------------------------------------------------------
    // IStreamInternal
    //-----------------------------------------------------------
    StreamType GetType() {
        return StreamTypeRealIStream;
    }

    HANDLE GetFileHandle() {
        return INVALID_HANDLE_VALUE;
    }

    //-----------------------------------------------------------
    // IStream
    //-----------------------------------------------------------
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [length_is][size_is][out] */ void __RPC_FAR *pv_,
        /* [in] */ ULONG cb_,
        /* [out] */ ULONG __RPC_FAR *pcbRead_)
    {
        return m_pIStream->Read( pv_, cb_, pcbRead_ );
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
        /* [size_is][in] */ const void __RPC_FAR *pv_,
        /* [in] */ ULONG cb_,
        /* [out] */ ULONG __RPC_FAR *pcbWritten_)
    {
        return m_pIStream->Write(pv_, cb_, pcbWritten_);
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove_,
        /* [in] */ DWORD dwOrigin_,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition_)
    {
        return m_pIStream->Seek(dlibMove_, dwOrigin_, plibNewPosition_);
    }
    
    virtual HRESULT STDMETHODCALLTYPE SetSize( 
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        return m_pIStream->SetSize(libNewSize);
    }
    
    virtual HRESULT STDMETHODCALLTYPE Commit( 
        /* [in] */ DWORD grfCommitFlags)
    {
        return m_pIStream->Commit(grfCommitFlags);
    }

    virtual HRESULT STDMETHODCALLTYPE Stat( 
        /* [out] */ STATSTG __RPC_FAR *pstatstg,
        /* [in] */ DWORD grfStatFlag)
    {
        return m_pIStream->Stat(pstatstg, grfStatFlag);
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE CopyTo( 
        /* [unique][in] */ IStream __RPC_FAR *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbRead,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbWritten)
    {
        return m_pIStream->CopyTo(pstm, cb, pcbRead, pcbWritten);
    }

    virtual HRESULT STDMETHODCALLTYPE Revert( void)
    {
        return m_pIStream->Revert();
    }
    
    virtual HRESULT STDMETHODCALLTYPE LockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        return m_pIStream->LockRegion(libOffset, cb, dwLockType);
    }
    
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion( 
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        return m_pIStream->UnlockRegion(libOffset, cb, dwLockType);
    }
    
    virtual HRESULT STDMETHODCALLTYPE Clone( 
        /* [out] */ IStream __RPC_FAR *__RPC_FAR *ppstm)
    {
        return m_pIStream->Clone(ppstm);
    }

    
protected:
    IStream* m_pIStream;
};

class IStreamMemMappedFile: public IStreamInternal, public SymBase, public MemoryMappedFile
{
public:

    static IStreamInternal* Create(const wchar_t*szFileName, BOOL fWrite, HANDLE *ph, BOOL *pfCreated)
    {
        IStreamMemMappedFile *pIStream = new IStreamMemMappedFile;

        if (pIStream == NULL || !pIStream->Init(szFileName, fWrite, *pfCreated))
        {
            if (pIStream != NULL)
                delete pIStream;

            *ph = INVALID_HANDLE_VALUE;
            return NULL;
        }

        *ph = pIStream->m_hFile;
        return pIStream;
    }

    IStreamMemMappedFile() {
    }

    ~IStreamMemMappedFile() {
    }

    //-----------------------------------------------------------
    // IUnknown
    //-----------------------------------------------------------
    ULONG STDMETHODCALLTYPE AddRef() {return (SymBase::AddRef());}
    ULONG STDMETHODCALLTYPE Release() {return (SymBase::Release());}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppInterface)
    {
        return E_NOTIMPL;
    }

    //-----------------------------------------------------------
    // IStreamInternal
    //-----------------------------------------------------------
    StreamType GetType() {
        return StreamTypeMemMappedFile;
    }

    HANDLE GetFileHandle() {
        return m_hFile;
    }

    //-----------------------------------------------------------
    // IStream
    //-----------------------------------------------------------
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [length_is][size_is][out] */ void __RPC_FAR *pv_,
        /* [in] */ ULONG cb_,
        /* [out] */ ULONG __RPC_FAR *pcbRead_)
    {
        HRESULT hr = S_OK;
        if ( pv_ == NULL )
            return STG_E_INVALIDPOINTER;
        ULONG cbRead = MemoryMappedFile::Read(pv_, UINT(cb_));
        if ( cbRead == 0 && cbRead != cb_) {
            hr = S_FALSE;
        }
        if ( pcbRead_ )
            *pcbRead_ = cbRead;
        return hr;
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
        /* [size_is][in] */ const void __RPC_FAR *pv_,
        /* [in] */ ULONG cb_,
        /* [out] */ ULONG __RPC_FAR *pcbWritten_)
    {
        HRESULT hr = S_OK;
        if (pv_ == NULL )
            return STG_E_INVALIDPOINTER;
        ULONG cbWritten = MemoryMappedFile::Write(pv_, UINT(cb_));
        if ( cbWritten == 0 && cbWritten != cb_ ) {
            hr = S_FALSE;
        }
        if ( pcbWritten_ )
            *pcbWritten_ = cbWritten;
        return hr;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove_,
        /* [in] */ DWORD dwOrigin_,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition_)
    {
        ULARGE_INTEGER pos = MemoryMappedFile::Seek(dlibMove_, int( dwOrigin_ ) );
        if ( plibNewPosition_ )
            *plibNewPosition_ = pos;
        return pos.QuadPart == -1 ? STG_E_INVALIDFUNCTION : S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE SetSize( 
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        return MemoryMappedFile::fSetSize(libNewSize) ? S_OK : STG_E_INVALIDFUNCTION;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Commit( 
        /* [in] */ DWORD grfCommitFlags)
    {
        return MemoryMappedFile::fFlush() ? S_OK : E_FAIL;
    }
    
};

class IStreamFileCRTAPI: public IStreamInternal, public SymBase
{
public: 
    IStreamFileCRTAPI( int _fd, HANDLE _hFileMap = NULL) : fd( _fd ), hFileMap( _hFileMap ) {
    }

    ~IStreamFileCRTAPI() {
        if (hFileMap != NULL)
            ::CloseHandle(hFileMap);

        _close( fd );
    }

    //-----------------------------------------------------------
    // IUnknown
    //-----------------------------------------------------------
    ULONG STDMETHODCALLTYPE AddRef() {return (SymBase::AddRef());}
    ULONG STDMETHODCALLTYPE Release() {return (SymBase::Release());}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppInterface)
    {
        return E_NOTIMPL;
    }
    //-----------------------------------------------------------
    // IStreamInternal
    //-----------------------------------------------------------
    StreamType GetType() {
        return StreamTypeFile;
    }

    HANDLE GetFileHandle() {
        return (HANDLE)_get_osfhandle(fd);
    }

    //-----------------------------------------------------------
    // IStream
    //-----------------------------------------------------------
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [length_is][size_is][out] */ void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbRead)
    {
        HRESULT hr = S_OK;
        if ( pv == NULL )
            return STG_E_INVALIDPOINTER;
        int cbRead = _read( fd, pv, UINT(cb) );
        if ( cbRead < 0 ) {
            cbRead = 0;
            hr = S_FALSE;
        }
        if ( pcbRead )
            *pcbRead = cbRead;
        return hr;
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
        /* [size_is][in] */ const void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbWritten)
    {
        HRESULT hr = S_OK;
        if ( pv == NULL )
            return STG_E_INVALIDPOINTER;
        int cbWritten = _write( fd, pv, UINT(cb) );
        if ( cbWritten < 0 ) {
            cbWritten = 0;
            hr = S_FALSE;
        }
        if ( pcbWritten )
            *pcbWritten = cbWritten;
        return hr;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
    {
        __int64 ret = _lseeki64( fd, dlibMove.QuadPart, int( dwOrigin ) );
        if ( plibNewPosition )
            plibNewPosition->QuadPart = ret;
        return ret < 0 ? STG_E_INVALIDFUNCTION : S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE SetSize( 
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        return _chsize_s( fd, libNewSize.QuadPart ) != 0 ? STG_E_INVALIDFUNCTION : S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Commit( 
        /* [in] */ DWORD grfCommitFlags)
    {
        return S_OK;
    }
    
protected:
    int fd;
    HANDLE hFileMap;
};

class IStreamFileWinAPI: public IStreamInternal, public SymBase
{
public:
    IStreamFileWinAPI(HANDLE h) : hFile(h)
    {
    }

    ~IStreamFileWinAPI()
    {
        if (hFile != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hFile);
        }
    }

    //-----------------------------------------------------------
    // IUnknown
    //-----------------------------------------------------------

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return SymBase::AddRef();
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        return SymBase::Release();
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppInterface)
    {
        return E_NOTIMPL;
    }

    //-----------------------------------------------------------
    // IStreamInternal
    //-----------------------------------------------------------

    StreamType GetType()
    {
        return StreamTypeFile;
    }

    HANDLE GetFileHandle()
    {
        return hFile;
    }

    //-----------------------------------------------------------
    // IStream
    //-----------------------------------------------------------
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
        /* [length_is][size_is][out] */ void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbRead)
    {
        HRESULT hr = S_OK;

        if (pv == NULL) {
            return STG_E_INVALIDPOINTER;
        }

        DWORD cbRead;

        if (!ReadFile(hFile, pv, UINT(cb), &cbRead, NULL)) {
            cbRead = 0;
            hr = S_FALSE;
        }

        if (pcbRead) {
            *pcbRead = cbRead;
        }

        return hr;
    }
    
    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
        /* [size_is][in] */ const void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbWritten)
    {
        HRESULT hr = S_OK;

        if (pv == NULL) {
            return STG_E_INVALIDPOINTER;
        }

        DWORD cbWritten;

        if (!WriteFile(hFile, pv, UINT(cb), (DWORD *) &cbWritten, NULL)) {
            cbWritten = 0;
            hr = S_FALSE;
        }

        if (pcbWritten) {
            *pcbWritten = cbWritten;
        }

        return hr;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek( 
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
    {
        DWORD dwMoveMethod = 0;

        switch (dwOrigin) {
            case SEEK_SET: dwMoveMethod = FILE_BEGIN; break;
            case SEEK_CUR: dwMoveMethod = FILE_CURRENT; break;
            case SEEK_END: dwMoveMethod = FILE_END; break;
        }

        BOOL f = SetFilePointerEx(hFile, dlibMove, (PLARGE_INTEGER) plibNewPosition, dwMoveMethod);

        return f ? S_OK : STG_E_INVALIDFUNCTION;
    }
    
    virtual HRESULT STDMETHODCALLTYPE SetSize( 
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        LARGE_INTEGER liCur;
        LARGE_INTEGER liZero = {0};

        if (SetFilePointerEx(hFile, liZero, &liCur, FILE_CURRENT) == 0) {
            return STG_E_INVALIDFUNCTION;
        }

        LARGE_INTEGER li;

        if (SetFilePointerEx(hFile, *(LARGE_INTEGER *) &libNewSize, &li, FILE_BEGIN) == 0 ||
            SetEndOfFile(hFile) == 0 ||
            SetFilePointerEx(hFile, liCur, &li, FILE_BEGIN) == 0) {
            return STG_E_INVALIDFUNCTION;
        }

        return S_OK;
    }
    
    virtual HRESULT STDMETHODCALLTYPE Commit( 
        /* [in] */ DWORD grfCommitFlags)
    {
        return S_OK;
    }
    
protected:
    HANDLE hFile;
};

class IStreamFile
{
public:
    static IStreamInternal* Create(const wchar_t *wszFilename, BOOL fWrite, BOOL fCreate, BOOL *pfCreated )
    {
        // Using only the CRT file API is problematic because it has a limit of 2048 simultaneously open file handles
        // Using only the Windows file API is problematic because it causes performance issues
        // Default to using the CRT file API, and fall back on the slower Windows file API when necessary
        IStreamInternal* iStream = NULL;

        iStream = CreateCRTAPI(wszFilename, fWrite, fCreate, pfCreated);
        if(iStream) { return iStream; }

        iStream = CreateWinAPI(wszFilename, fWrite, fCreate, pfCreated);
        return iStream;
    }

    static IStreamInternal* Create(HANDLE hFile)
    {
        int fd = _open_osfhandle ( 
            INT_PTR(hFile),
            _O_BINARY | _O_RDONLY | _O_NOINHERIT
            );

        if ( fd > 0 ) {
            return new IStreamFileCRTAPI(fd);
        }

        return NULL;
    }

private:
    static IStreamInternal* CreateCRTAPI(const wchar_t *wszFilename, BOOL fWrite, BOOL fCreate, BOOL *pfCreated )
    {
        HANDLE hFileMap = NULL;
        int fd = -1;

        *pfCreated = FALSE;

        if (fWrite) {
            _wsopen_s(&fd, wszFilename, O_BINARY | O_NOINHERIT | O_RDWR, SH_DENYRW, 0);

            if (fd == -1 && fCreate) {
                _wsopen_s(&fd, wszFilename, O_BINARY | O_NOINHERIT | O_RDWR | O_CREAT, SH_DENYRW, S_IREAD | S_IWRITE);
                *pfCreated = (fd != -1);
            }
        }

        else {
            // Try to open it with semantics that allow rename of file for Windows NT.

            SECURITY_ATTRIBUTES sa = {
                sizeof SECURITY_ATTRIBUTES,
                NULL,
                false
            };

            HANDLE  hfile = ::CreateFileW(
                wszFilename,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_DELETE,
                &sa,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
                );

            if ( INVALID_HANDLE_VALUE == hfile ) {
                fd = -1;
            }
            else {
                hFileMap = ::CreateFileMappingW(
                    hfile,
                    &sa,
                    PAGE_READONLY | SEC_COMMIT,
                    0,
                    1,
                    NULL
                    );

                fd = _open_osfhandle ( 
                    INT_PTR(hfile),
                    _O_BINARY | _O_RDONLY | _O_NOINHERIT
                    );
            }
        }

        if (fd > 0) {
            HANDLE hFile = (HANDLE)_get_osfhandle(fd);
            assert(hFile != INVALID_HANDLE_VALUE);

            DWORD dwType = GetFileType(hFile);
            if (dwType != FILE_TYPE_DISK) {
                _close(fd);
                fd = 0;
            }
        }

        if (fd > 0) {
            IStreamFileCRTAPI * pStream = new IStreamFileCRTAPI(fd, hFileMap);
            if (pStream != NULL) {
                // UNDONE: should report out of memory
                return pStream;
            }
            _close(fd);
        }
        return NULL;
    }

    static IStreamInternal* CreateWinAPI(const wchar_t *wszFilename, BOOL fWrite, BOOL fCreate, BOOL *pfCreated )
    {
        *pfCreated = FALSE;

        HANDLE  hfile = ::CreateFileW(
            wszFilename,
            fWrite ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
            fWrite ? 0 : (FILE_SHARE_READ | FILE_SHARE_DELETE),
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (hfile == INVALID_HANDLE_VALUE) {
            if (!fWrite || !fCreate) {
                return NULL;
            }

            hfile = ::CreateFileW(
                wszFilename,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

            if (hfile == INVALID_HANDLE_VALUE) {
                return NULL;
            }

            *pfCreated = TRUE;
        }

        if (GetFileType(hfile) != FILE_TYPE_DISK) {
            ::CloseHandle(hfile);
            return NULL;
        }

        IStreamFileWinAPI * pStream = new IStreamFileWinAPI(hfile);

        if (pStream == NULL) {
            ::CloseHandle(hfile);
            return NULL;
        }

        return pStream;
    }
};
