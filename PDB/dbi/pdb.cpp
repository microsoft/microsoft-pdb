//////////////////////////////////////////////////////////////////////////////
// PDB Debug Information API Implementation


#include "pdbimpl.h"
#include "dbiimpl.h"
#include "cvexefmt.h"

#include "pdbguid.h"

#include "locator.h"

#ifndef _WIN64

#define _USE_32BIT_TIME_T       // Use 32 bit time_t (DbgHelp needs this because it uses system CRT)
#define _INC_TIME_INL           // Don't include inlined definitions of time()
#define _INC_WTIME_INL          // Don't include inlined definitions of wtime()
#include <time.h>

extern "C" _ACRTIMP time_t time( time_t *timer );

#else

#include <time.h>

#endif

#include "objbase.h"
#include "szcanon.h"
#include "pdbver.h"

#include "sha256.cpp"

extern
const
char    rgbTag[] = VER_FILEDESCRIPTION_STR "\0" VER_PRODUCTVERSION_STR;

extern
const
ushort  rgusVer[] = { VER_PRODUCTVERSION };

#define SetBits(a,b,c,d,e,f,g,h,i,j,k,l,m) \
    ((1<<a) & (1<<b) & (1<<c) & (1<<d) & (1<<e) & (1<<f) & (1<<g) & \
    (1<<h) & (1<<i) & (1<<j) & (1<<k) & (1<<l) & (1<<m))

const __int32 PDB1::c_mpEcEcBest[] =
{
    // Most interesting values are the ones that can be returned from the OpenValidate*
    // calls.  Priority order is:
    // 
    // EC_OK
    // EC_OUT_OF_MEMORY, EC_FILE_SYSTEM
    // EC_INVALID_AGE
    // EC_INVALID_SIG
    // EC_ACCESS_DENIED
    // EC_FORMAT
    // EC_DEBUG_INFO_NOT_IN_PDB
    // EC_DBG_NOT_FOUND
    // EC_NO_DEBUG_INFO
    // EC_INVALID_EXE_TIMESTAMP
    // EC_INVALID_EXECUTABLE
    // EC_NOT_FOUND
    //
    // and they remain stable (i.e. if they are compared to the same error, they return false)
    //    

    // (EC_OK, *) none are better than EC_OK
    0,
    
    // (EC_USAGE, *), all are better than EC_USAGE
    0xffffffff,
    
    // (EC_OUT_OF_MEMORY, *) hmmm, none better than OOM except OK
    SetBits(EC_OK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    
    // (EC_FILE_SYSTEM, *)
    SetBits(EC_OK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
    
    // (EC_NOT_FOUND, *) The interesting one.  Better than not many.
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM, EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, EC_DEBUG_INFO_NOT_IN_PDB,
        EC_DBG_NOT_FOUND, EC_NO_DEBUG_INFO, EC_INVALID_EXE_TIMESTAMP,
        EC_INVALID_EXECUTABLE, 0),

    // (EC_INVALID_SIG, *) Best one to return in most cases
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM, EC_INVALID_AGE,
        0, 0, 0, 0, 0, 0, 0, 0, 0),

    // (EC_INVALID_AGE, *) Best one to return in all cases
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0),

    // (EC_PRECOMP_REQUIRED, *) n/a
    0xffffffff,

    // (EC_OUT_OF_TI), *) n/a
    0xffffffff,

    // (EC_NOT_IMPLEMENTED, *), n/a
    0xffffffff,

    // (EC_V1_PDB, *), n/a
    0xffffffff,

    // (EC_FORMAT, *), n/a
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, 0, 0, 0, 0, 0, 0, 0),

    // (EC_LIMIT, *), n/a
    0xffffffff,

    // (EC_CORRUPT, *) n/a
    0xffffffff,

    // (EC_TI16, *) n/a
    0xffffffff,

    // (EC_ACCESS_DENIED, *)
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, 0, 0, 0, 0, 0, 0, 0, 0),

    // EC_ILLEGAL_TYPE_EDIT
    0xffffffff,

    // EC_INVALID_EXECUTABLE
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, EC_DEBUG_INFO_NOT_IN_PDB,
        EC_DBG_NOT_FOUND, EC_NO_DEBUG_INFO, EC_INVALID_EXE_TIMESTAMP,
        0, 0),

    // EC_DBG_NOT_FOUND,       
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, EC_DEBUG_INFO_NOT_IN_PDB,
        0, EC_NO_DEBUG_INFO, 0, 0, 0),

    // EC_NO_DEBUG_INFO,       
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, EC_DEBUG_INFO_NOT_IN_PDB,
        EC_DBG_NOT_FOUND, 0, 0, 0, 0),

    // EC_INVALID_EXE_TIMESTAMP
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, EC_DEBUG_INFO_NOT_IN_PDB,
        EC_DBG_NOT_FOUND, EC_NO_DEBUG_INFO, 0, 0, 0),

    // EC_CORRUPT_TYPEPOOL
    0xffffffff,

    // EC_DEBUG_INFO_NOT_IN_PDB
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, EC_FORMAT, 0,
        0, 0, 0, 0, 0),

    // EC_RPC
    0xffffffff,

    // EC_UNKNOWN
    0xffffffff,

    // EC_BAD_CACHE_PATH
    0xffffffff,

    // EC_CACHE_FULL
    0xffffffff,

    // EC_TOO_MANY_MOD_ADDTYPE
    0xffffffff,

    // EC_MINI_PDB, same as EC_FORMAT
    SetBits(EC_OK, EC_OUT_OF_MEMORY, EC_FILE_SYSTEM,EC_INVALID_AGE,
        EC_INVALID_SIG, EC_ACCESS_DENIED, 0, 0, 0, 0, 0, 0, 0),
};


#undef SetBits

extern const long  cbPgPdbDef;      // defined in pdbcommon.cpp

BOOL PDB::ExportValidateInterface(INTV intv_)
{
    return PDB1::ExportValidateInterface(intv_);
}

BOOL PDB::ExportValidateImplementation(IMPV impv_)
{
    return PDB1::ExportValidateImplementation(impv_);
}

INTV PDB::QueryInterfaceVersionStatic()
{
    return PDB1::QueryInterfaceVersionStatic();
}

IMPV PDB::QueryImplementationVersionStatic()
{
    return PDB1::QueryImplementationVersionStatic();
}

INTV PDB1::QueryInterfaceVersion()
{
    PDBLOG_FUNC();
    return intv;
}

IMPV PDB1::QueryImplementationVersion()
{
    PDBLOG_FUNC();
    return impv;
}

IMPV PDB1::QueryPdbImplementationVersion()
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_csPdbStream);

    return pdbStream.impv;
}

//////////////////////////////////////////////////////////////////////////////
// Program Database API Implementation


BOOL PDB::Open2W(const wchar_t *wszPDB, const char *szMode, OUT EC *pec,
       __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pec);
    dassert(pppdb);
    return PDB1::Open2W(wszPDB, szMode, pec, wszError, cchErrMax, pppdb);
}


BOOL PDB::OpenEx2W(const wchar_t *wszPDB, const char *szMode, long cbPage,
       OUT EC *pec,  __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pec);
    dassert(pppdb);

    return PDB1::OpenEx2W(wszPDB, szMode, (SIG) 0, cbPage, pec, wszError, cchErrMax, pppdb);
}

BOOL PDB::OpenValidate4(const wchar_t *wszPDB, const char *szMode,
       PCSIG70 pcsig70, SIG sig, AGE age, OUT EC *pec,
        __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pppdb);

    return PDB1::OpenValidate4(wszPDB, szMode, pcsig70, sig, age, pec, wszError, cchErrMax, pppdb);
}

BOOL PDB::OpenNgenPdb(const wchar_t *wszNgenImage, const wchar_t *wszPdbPath,
        OUT EC *pec, __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszNgenImage);
    dassert(wszPdbPath);
    dassert(pppdb);

    return PDB1::OpenNgenPdb(wszNgenImage, wszPdbPath, pec, wszError, cchErrMax, pppdb);
}

BOOL PDB::OpenValidate5(const wchar_t *wszExecutable,
       const wchar_t *wszSearchPath, void *pvClient,
       PfnPDBQueryCallback pfnQueryCallback, OUT EC *pec,
        __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszExecutable);
    dassert(pec);
    dassert(pppdb);

    return PDB1::OpenValidate5(wszExecutable, wszSearchPath, pvClient, pfnQueryCallback, pec, wszError, cchErrMax, pppdb);
}


BOOL PDB::OpenInStream(IStream *pIStream, const char *szMode, OUT EC *pec,
        __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(pIStream);
    dassert(pec);
    dassert(pppdb);

#ifdef SMALLPAGES
    return PDB1::OpenInStream(pIStream, szMode, 512, pec, wszError, cchErrMax, pppdb);
#else
    return PDB1::OpenInStream(pIStream, szMode, ::cbPgPdbDef, pec, wszError, cchErrMax, pppdb);
#endif
}

static const EC xlateMsfEc[MSF_EC_MAX] = {
    EC_OK,
    EC_OUT_OF_MEMORY,
    EC_NOT_FOUND,
    EC_FILE_SYSTEM,
    EC_FORMAT,
    EC_ACCESS_DENIED,
    EC_CORRUPT,
};

cassert(_countof(xlateMsfEc) == MSF_EC_MAX);

#if defined(CRT_LEAKS)
_CrtMemState s1, s2, s3;
#endif

#ifdef PDB_MT
Map<MSF *, PDB1 *, HcLPtr > PDB1::s_mpOpenedPDB;
CriticalSection PDB1::s_csForPDBOpen;
#endif

class CPDBErrorDefault : public IPDBError {
public:
    static IPDBError * Create( PDB *ppdb ) { 
        return new CPDBErrorDefault;
    }
    static IPDBError * CreateHandler( PDB *ppdb ) { 
        return s_pfnPDBErrorCreate( ppdb );
    }
    static BOOL SetCreateHandler( PfnPDBErrorCreate pfn ) {
        if (pfn == NULL) {
            // set it back to default
            pfn = &CPDBErrorDefault::Create;
        }
        s_pfnPDBErrorCreate = pfn;
        return TRUE;
    }
    EC QueryLastError( __out_ecount(cchMax) wchar_t * wszError, size_t cchMax) {
        PDBLOG_FUNC();

        if (wszError != NULL && wszErrLast != NULL) {
            wcsncpy_s(wszError, cchMax, wszErrLast, _TRUNCATE);
        }

        return ecLast;
    }
    void SetLastError(EC ec, const wchar_t * wszErr) {
        PDBLOG_FUNC();
        ecLast = ec;

        if (wszErr == NULL) {
            return;
        }

        if (wszErrLast == NULL) {
            wszErrLast = new wchar_t[cbErrMax];

            // It is fine if we run out of memory, since we are reporting
            // a failure and have already set the error code.
        }

        if (wszErrLast != NULL) {
            wcsncpy_s(wszErrLast, cbErrMax, wszErr, _TRUNCATE);
        }
    }
    void Destroy() {
        if (wszErrLast != NULL) {
            delete [] wszErrLast;
        }
        delete this;
    }
private:
    CPDBErrorDefault() : ecLast(EC_OK), wszErrLast(NULL) {};

    EC          ecLast;
    wchar_t    *wszErrLast;

    static PfnPDBErrorCreate s_pfnPDBErrorCreate;
};

PfnPDBErrorCreate CPDBErrorDefault::s_pfnPDBErrorCreate = &CPDBErrorDefault::Create;

BOOL PDB::SetErrorHandlerAPI( PfnPDBErrorCreate pfn )
{
    return CPDBErrorDefault::SetCreateHandler(pfn);
}


BOOL PDB1::OpenEx2W(const wchar_t *wszPDB, const char *szMode, SIG sigInitial,
       long cbPage, OUT EC *pec,  __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax,
       OUT PDB **pppdb)
{
    
    // a compile-time check to make sure we have the table filled out completely
    // need to do it here because c_mpEcEcBest is private
    cassert(EC_MAX == _countof(PDB1::c_mpEcEcBest));

    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pec);
    dassert(pppdb);

#if defined(CRT_LEAKS)
    _CrtMemCheckpoint( &s1 );
#endif

    *pec = EC_OK;

    MSF_EC msfEc = MSF_EC_OK;
    PDB1 *ppdb1;

    //  There are 4 mode that a PDB can be opened (in order of more restrictive to last restrictive
    //  Write-Exclusive   (WE)
    //  Read-Exclusive    (RE)  // default for read (multiple)
    //  Write-Shared      (WS)  // default for write
    //  Read+Write-Shared (RS) 
    //
    //  If the pdb has been opened (not on the timeout queue) already and has mode:
    //  WE - open will fail
    //  RE - open will succeed only if the requested mode is RS/RE , the pdb will have mode RE after open
    //  WS - open will succeed only if the requested mode is RS/WS, the pdb will have mode WS after open
    //  RS - open will succeed only if the requested mode is RS/RE/WS, the pdb will have the mode requested.

    bool  fRead = strchr(szMode, *pdbRead) != NULL;
    bool  fExclusive = strchr(szMode, *pdbExclusive) != NULL;
    bool  fWriteShared = strchr(szMode, *pdbWriteShared) != NULL;
    bool  fFullBuild = strchr(szMode, *pdbFullBuild) != NULL;
    bool  fNoTypeMerge = strchr(szMode, *pdbNoTypeMergeLink) != NULL;
    bool  fMinimalDbgInfo = strchr(szMode, *pdbMinimalLink) != NULL;
    bool  fVC120 = strchr(szMode, *pdbVC120) != NULL;

    if (fRead && (fVC120 || fNoTypeMerge || fMinimalDbgInfo)) {
        setError(pec, EC_USAGE, wszError, cchErrMax, wszPDB);
        return FALSE;
    }

    if (fNoTypeMerge && fMinimalDbgInfo) {
        setError(pec, EC_USAGE, wszError, cchErrMax, wszPDB);
        return FALSE;
    }

    if (fVC120 && (fNoTypeMerge || fMinimalDbgInfo)) {
        setError(pec, EC_USAGE, wszError, cchErrMax, wszPDB);
        return FALSE;
    }

    if (fExclusive && fWriteShared) {
        setError(pec, EC_ACCESS_DENIED, wszError, cchErrMax, wszPDB);
        return FALSE;
    }

    if (!fWriteShared && !fExclusive && !fRead) {
        fWriteShared = true;        // the default
    }

#ifdef PDB_MT
    // MSF will return the same pmsf if it has already open.
    // It will fail if we try to open it as write, but it is opened as read.
    // In that case it will report access denied, but still give us the pmsf without ref-counting it
    MTS_PROTECT(s_csForPDBOpen);
    MSF *pmsf = MSF::Open(wszPDB, !fRead, &msfEc, cbPage, msffAccess);   
#else   
    MSF *pmsf = MSF::Open(wszPDB, !fRead, &msfEc, cbPage);
#endif

    if (pmsf != NULL) {
#ifdef PDB_MT
        if (s_mpOpenedPDB.map(pmsf, &ppdb1)) {
            if (msfEc != MSF_EC_ACCESS_DENIED) {
                verify(pmsf->Close());      // found a PDB with the same MSF, close the one I opened
            }

            // If the PDB is on the timeout queue, we need to try to close and reopen it if:  
            // 1) The MSF was opened as read before and we now need to write (msfEc == MSF_EC_ACCESS_DENIED)
            // 2) The full build flag is different
            // 3) It needs to do the following mode transition (note that these are all illegal if the PDB is not on timeout):
            //    RE => W*
            //    WE => *	
            //    WS => *E      
            //    RS => WE	
            if (msfEc == MSF_EC_ACCESS_DENIED ||
                (fFullBuild != ppdb1->m_fFullBuild) ||
                ((!fWriteShared || !ppdb1->m_fWriteShared) && (!fRead || !ppdb1->m_fRead)))
            {
                if (!s_pdbTimeoutManager.NotifyReopen(ppdb1)) {
                    setError(pec, EC_ACCESS_DENIED, wszError, cchErrMax, wszPDB);
                    return FALSE;
                }
                
                // reopen the msf and open the pdb
                pmsf = MSF::Open(wszPDB, !fRead, &msfEc, cbPage);
                if (pmsf == NULL) {
                    goto MsfError;
                }

                assert(!s_mpOpenedPDB.contains(pmsf));

                goto OpenPDB;
            } 

            s_pdbTimeoutManager.NotifyReuse(ppdb1);

            // Allow RS => RE/WS transition
            if (ppdb1->m_fRead && ppdb1->m_fWriteShared) {
                ppdb1->m_fRead = fRead;
                ppdb1->m_fWriteShared = fWriteShared;
            }
 
            ppdb1->dwPDBOpenedCount++;
        } 
        else 

#endif
        {
#ifdef PDB_MT
OpenPDB:
#endif
            if (!(ppdb1 = new PDB1(pmsf, wszPDB))) {
                setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, L"");
                verify(pmsf->Close());
                return FALSE;
            }

            ppdb1->pPDBError = CPDBErrorDefault::CreateHandler( ppdb1 );
            if (ppdb1->pPDBError == NULL) {
                setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, L"");
                goto HandleError;
            }

            ppdb1->m_fRead = fRead;
            ppdb1->m_fWriteShared = fWriteShared;
            ppdb1->m_fFullBuild = fFullBuild;
            ppdb1->m_fPdbCTypes = strchr(szMode, *pdbCTypes) != NULL;
            ppdb1->m_fPdbTypeMismatch = strchr(szMode, *pdbTypeMismatchesLink) != NULL;

            // UNDONE: only here temporary so the NCB can use it.  
            // Should remove when we update everything to the new namemap format
            ppdb1->m_fNewNameMap = strchr(szMode, *pdbNewNameMap) != NULL;

            if (fRead) {
                // Read the pdb stream and validate the implementation
                // format and the signature.

                if (!ppdb1->loadPdbStream(pmsf, wszPDB, pec, wszError, cchErrMax)) {
                    goto HandleError;
                }
            }
            else {
                CB cbPdbStream = pmsf->GetCbStream(snPDB);

                if (cbPdbStream != cbNil) {
                    if (!ppdb1->loadPdbStream(pmsf, wszPDB, pec, wszError, cchErrMax)) {
                        goto HandleError;
                    }

#if 0
                    if (!ppdb1->m_fContainIDStream) {
                        setError(pec, EC_FORMAT, wszError, cchErrMax, wszPDB);
                        goto HandleError;
                    }
#endif

                    if (fVC120 && ppdb1->pdbStream.impv != impvVC110) {
                        setError(pec, EC_FORMAT, wszError, cchErrMax, wszPDB);
                        goto HandleError;
                    }

                    if (!fFullBuild) {
                        if (fNoTypeMerge != ppdb1->m_fNoTypeMerge) {
                            setError(pec, EC_FORMAT, wszError, cchErrMax, wszPDB);
                            goto HandleError;
                        }
                        if (fMinimalDbgInfo != ppdb1->m_fMinimalDbgInfo) {
                            setError(pec, EC_MINI_PDB, wszError, cchErrMax, wszPDB);
                            goto HandleError;
                        }
                    } else {
                        ppdb1->m_fNoTypeMerge = fNoTypeMerge;
                        ppdb1->m_fMinimalDbgInfo = fMinimalDbgInfo;
                    }

                    ppdb1->pdbStream.age++;
                }
                else {
                    // Create a new PDB.

                    bool fRepro = strchr(szMode, 'z') != NULL;
                    bool fCompress = strchr(szMode, 'C') != NULL;

                    ppdb1->pdbStream.impv = impv;
                    ppdb1->pdbStream.sig = fRepro ? 1 : ((sigInitial != 0) ? sigInitial : (SIG) time(0));

                    if (fRepro) {
                        memset(&ppdb1->pdbStream.sig70, -1, sizeof(ppdb1->pdbStream.sig70));
                    }
                    else {
                        if (!FUuidCreate(&ppdb1->pdbStream.sig70)) {
                            setError(pec, EC_USAGE, wszError, cchErrMax, wszPDB);
                            goto HandleError;
                        }
                    }

                    // Starting with VC60, ages count from 1 instead of 0
                    // (VC50 pdb's have an age of 0 in their dbi header. In order
                    // to be able to read such pdbs without using the enhanced VC60
                    // validation scheme, we reserve 0 as a special age value
                    // and start counting from 1)

                    ppdb1->pdbStream.age = 1;

                    // "Claim" the snPDB stream as the "semaphore" that
                    // the PDB exists and has valid PDB-stuff in it.
                    // Also reserve snTpi, snDBI and snIpi, at least until
                    // we store them in named streams.

                    if (!pmsf->ReplaceStream(snPDB, 0, 0) ||
                        !pmsf->ReplaceStream(snTpi, 0, 0) ||
                        !pmsf->ReplaceStream(snDbi, 0, 0) ||
                        !pmsf->ReplaceStream(snIpi, 0, 0)) {
                        setError(pec, EC_FILE_SYSTEM, wszError, cchErrMax, wszPDB);
                        goto HandleError;
                    }

                    if (fCompress) {
                        // We only will set file system compression on PDB
                        // files we create; simply writing to an existing one
                        // won't set it.  (conscious decision!)
                        //
                        pmsf->FSetCompression(MSF::ctFileSystem, true);
                    }

                    ppdb1->m_fContainIDStream = true;
                    ppdb1->m_fNoTypeMerge = fNoTypeMerge;
                    ppdb1->m_fMinimalDbgInfo = fMinimalDbgInfo;
                    ppdb1->m_fVC120 = fVC120;
                }

                if (!ppdb1->Commit()) {
                    *pec = ppdb1->QueryLastErrorExW(wszError, cchErrMax);
                    goto HandleError;
                }
            }

            // Don't allow writing into anything before
            // impvVC70 for the SZ - ST confusion
#pragma message("TODO:Remove check for deprecated vc70 impv")
            if ((ppdb1->pdbStream.impv < impvVC70Dep) && (strchr(szMode, 'w') != NULL)) {
                setError(pec, EC_FORMAT, wszError, cchErrMax, wszPDB);
                goto HandleError;
            }

#ifdef PDB_MT
            if (!s_mpOpenedPDB.add(pmsf, ppdb1)) {
                setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, L"");
                goto HandleError;
            }
#endif
        }

        *pppdb = ppdb1;
        return TRUE;
    }

#ifdef PDB_MT
MsfError:
#endif
    setError(pec, xlateMsfEc[msfEc], wszError, cchErrMax, wszPDB);
    return FALSE;

HandleError:
    delete ppdb1;
    verify(pmsf->Close());
    return FALSE;

}

BOOL PDB1::OpenInStream(IStream* pIStream, const char *szMode, CB cbPage,
       OUT EC *pec, __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();

    dassert(pec);
    dassert(pIStream);
    dassert(szMode);
    dassert(pppdb);

    *pec = EC_OK;

    MSF_EC msfEc;
    PDB1 *ppdb1;

    bool fRead = strchr(szMode, 'r') != NULL;

    MSF *pmsf = MSF::Open(pIStream, !fRead, &msfEc, cbPage);

    if (pmsf == NULL) {
        // REVIEW : how should you report an error if this fails?
        setError(pec, xlateMsfEc[msfEc], wszError, cchErrMax, L"");
        return FALSE;
    }

#if 0
    //
    // ifdef'ed out to avoid dragging in ole32 just for the CoTaskMemFree
    //
    STATSTG statstg;
    if ( FAILED( pIStream->Stat( &statstg, STATFLAG_DEFAULT ) ) ) {
        setError(pec, EC_FILE_SYSTEM, wszError, cchErrMax, L"");
        verify(pmsf->Close());
        return FALSE;
    }
    char szPath[ _MAX_PATH ];
    if ( statstg.pwcsName ) {
        wcstombs( szPath, statstg.pwcsName, sizeof( szPath ) );
        CoTaskMemFree( statstg.pwcsName );
    }
#endif

    if (!(ppdb1 = new PDB1(pmsf, L""))) {
        verify(pmsf->Close());
        setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, L"");
        return FALSE;
    }

    ppdb1->m_fRead = fRead;
    ppdb1->m_fFullBuild = strchr(szMode, 'f') != NULL;

    ppdb1->pPDBError = CPDBErrorDefault::CreateHandler(ppdb1);
    if (ppdb1->pPDBError == NULL) {
        setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, L"");
        return FALSE;
    }


    if (fRead) {
        // read the pdb stream and validate the implementation format and the signature
        if (!ppdb1->loadPdbStream(pmsf, L"", pec, wszError, cchErrMax)) {
            verify(pmsf->Close());
            return FALSE;
        }
    }
    else {
        CB cbPdbStream = pmsf->GetCbStream(snPDB);

        if (cbPdbStream != cbNil) {
            if (!ppdb1->loadPdbStream(pmsf, L"", pec, wszError, cchErrMax)) {
                verify(pmsf->Close());
                return FALSE;
            }

            ppdb1->pdbStream.age++;         // we just newed ppdb1, no need to MTS protect
        }
        else {
            // Create a new PDB.

            bool fRepro = strchr(szMode, 'z') != NULL;

            // we just newed ppdb1, no need to MTS protect

            ppdb1->pdbStream.impv = impv;
            ppdb1->pdbStream.sig = fRepro ? 1 : (SIG) time(0);

            if (fRepro) {
                memset(&ppdb1->pdbStream.sig70, -1, sizeof(ppdb1->pdbStream.sig70));
            }
            else {
                if (!FUuidCreate(&ppdb1->pdbStream.sig70)) {
                    setError(pec, EC_USAGE, wszError, cchErrMax, L"");
                    return FALSE;
                }
            }

            // Starting with VC60, ages count from 1 instead of 0
            // (VC50 pdb's have an age of 0 in their dbi header. In order
            // to be able to read such pdbs without using the enhanced VC60
            // validation scheme, we reserve 0 as a special age value
            // and start counting from 1)

            ppdb1->pdbStream.age = 1;

            // "Claim" the snPDB stream as the "semaphore" that
            // the PDB exists and has valid PDB-stuff in it.
            // Also reserve snTpi, snDBI and snIpi, at least until
            // we store them in named streams.

            if (!pmsf->ReplaceStream(snPDB, 0, 0) ||
                !pmsf->ReplaceStream(snTpi, 0, 0) ||
                !pmsf->ReplaceStream(snDbi, 0, 0) ||
                !pmsf->ReplaceStream(snIpi, 0, 0))
            {
                setError(pec, EC_FILE_SYSTEM, wszError, cchErrMax, L"");
                return FALSE;
            }

            ppdb1->m_fContainIDStream = true;
        }

        if (!ppdb1->Commit()) {
            *pec = ppdb1->QueryLastErrorExW(wszError, cchErrMax);
            return FALSE;
        }
    }

    *pppdb = ppdb1;

    return TRUE;
}

BOOL PDB1::OpenValidate4(const wchar_t *wszPDB, const char *szMode,
       PCSIG70 pcsig70, SIG sig, AGE age, OUT EC *pec,
        __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pec);
    dassert(pppdb);

    *pppdb = NULL;

    PDB *ppdb;

    if (!OpenEx2W(wszPDB, szMode, (SIG) 0, cbMsfPageDefault, pec, wszError, cchErrMax, &ppdb)) {
        return FALSE;
    }

    if ((pcsig70 != NULL) && IsRealSig70(*pcsig70)) {
        SIG70 sig70;

        if (!ppdb->QuerySignature2(&sig70) || !IsEqualGUID(*pcsig70, sig70)) {
            ppdb->Close();
            setError(pec, EC_INVALID_SIG, wszError, cchErrMax, wszPDB);

            return FALSE;
        }
    }
    else {
        if (pcsig70 != NULL && sig == 0)
            sig = SigFromSig70(*pcsig70);

        if (ppdb->QuerySignature() != sig) {
            ppdb->Close();
            setError(pec, EC_INVALID_SIG, wszError, cchErrMax, wszPDB);

            return FALSE;
        }
    }

    if (age > ppdb->QueryAge()) {
        ppdb->Close();
        setError(pec, EC_INVALID_AGE, wszError, cchErrMax, wszPDB);

        return FALSE;
    }

    // If we open just in order to read types, we are done.
    // (In this case we need not validate the DBI stream)

    BOOL fGetRecordsOnly = strchr(szMode, 'c') != 0;

    if (!fGetRecordsOnly) {
        // For an acceptable match the PDB age should be greater than or equal to the image age,
        // and the DBI age should be equal to the image age. Old pdb files may have a DBI age
        // of 0; in such a case no additional validation is performed

        DBI *pdbi;

        if (!ppdb->OpenDBI(NULL, pdbRead, &pdbi)) {
            ppdb->Close();
            setError(pec, EC_INVALID_SIG, wszError, cchErrMax, wszPDB);

            return FALSE;
        }

        AGE ageT = pdbi->QueryAge();

        pdbi->Close();

        if ((ageT != 0) && (ageT != age)) {
            ppdb->Close();
            setError(pec, EC_INVALID_AGE, wszError, cchErrMax, wszPDB);

            return FALSE;
        }
    }

    *pppdb = ppdb;

    return TRUE;
}

BOOL PDB1::OpenNgenPdb(const wchar_t *wszNgenImage, const wchar_t *wszPdbPath,
       OUT EC *pec, __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszNgenImage);
    dassert(wszPdbPath);
    dassert(pec);
    dassert(pppdb);

    *pppdb = NULL;

    LOCATOR locator;
    
    wchar_t  *wszPdb = NULL;
    SIG70    *psig70 = NULL;
    AGE       age;

    // Crack the ngen image and get the PDB info saved in its debug directory.
    //
    // FGetNgenPdbInfo() also validates that
    //
    //   1. the image is really an ngen image; and in its debug directory,
    //   2. the PDB name is not NULL;
    //   3. the signature is a real GUID signature; and
    //   4. the age is not zero.

    if (!locator.FGetNgenPdbInfo(wszNgenImage, &wszPdb, &psig70, &age)) {
        setError(pec, locator.Ec(), wszError, cchErrMax, locator.WszError());
        return FALSE;
    }

    // Get the PDB's full-path name.

    wchar_t wszDrive[_MAX_DRIVE];
    wchar_t wszDir[_MAX_DIR];

    _wsplitpath_s(wszPdbPath, wszDrive, _countof(wszDrive), wszDir, _countof(wszDir), NULL, 0, NULL, 0);
    
    wchar_t wszFName[_MAX_FNAME];
    wchar_t wszExt[_MAX_EXT];
    
    _wsplitpath_s(wszPdb, NULL, 0, NULL, 0, wszFName, _countof(wszFName), wszExt, _countof(wszExt));

    wchar_t wszPdbFullPath[PDB_MAX_PATH];

    _wmakepath_s(wszPdbFullPath, _countof(wszPdbFullPath), wszDrive, wszDir, wszFName, wszExt);

    // Open the PDB file.
    
    PDB *ppdb;

    if (!OpenEx2W(wszPdbFullPath, pdbWrite, (SIG) 0, cbMsfPageDefault, pec, wszError, cchErrMax, &ppdb)) {
        return FALSE;
    }

    // Update the GUID and PDB age.

    PDB1 *ppdb1 = (PDB1 *)ppdb;

    memcpy(&ppdb1->pdbStream.sig70, psig70, sizeof(SIG70));
    ppdb1->pdbStream.age = age;

    *pppdb = ppdb;

    return TRUE;
}

BOOL PDB1::OpenTSPdb(const wchar_t *szPDB, const wchar_t *szPath, const char *szMode,
       const SIG70& sig70, AGE age, OUT EC *pec,  __out_ecount_opt(cchErrMax) wchar_t *szError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    wchar_t szErrorPrev[cbErrMax];
    EC ecPrev = EC_OK;
    *pppdb = NULL;

    // first try opening the pdb along the szPath
    if (szPath) {
        wchar_t  szPDBSansPath[_MAX_FNAME];
        wchar_t  szPDBExt[_MAX_EXT];
        wchar_t  szPDBLocal[PDB_MAX_FILENAME];
        wchar_t  szPathDrive[_MAX_DRIVE];
        wchar_t  szPathDir[_MAX_DIR];

        wcsncpy_s(szPDBLocal, _countof(szPDBLocal), szPDB, _TRUNCATE);
        _wsplitpath_s(szPDBLocal, NULL, 0, NULL, 0, szPDBSansPath, _countof(szPDBSansPath), szPDBExt, _countof(szPDBExt));

        // canonicalize the path so we can search for a trailing '\\'

        wchar_t  szPathLocal[_MAX_PATH];
        wcsncpy_s(szPathLocal, _countof(szPathLocal), szPath, _TRUNCATE);
        CCanonFile::SzCanonFilename(szPathLocal);
        wchar_t *szSlash = wcsrchr ( szPathLocal, L'\\');
    
        // add the trailing slash if necesssary (when called with the output
        // from _splitpath, there is already a trailing slash on szPath)
        //
        size_t dwPathLength = wcslen(szPathLocal);
        if (!(szSlash && (dwPathLength == size_t((szSlash + 1) - szPathLocal)))) {
            size_t dwPathEnd = (dwPathLength == _MAX_PATH - 1)? dwPathLength - 1 : dwPathLength;
            szPathLocal[dwPathEnd] = L'\\';
            szPathLocal[dwPathEnd + 1] = L'\0';
        } 
        _wsplitpath_s(szPathLocal, szPathDrive, _countof(szPathDrive), szPathDir, _countof(szPathDir), NULL, 0, NULL, 0);

        wchar_t  szOrigPDBSansPath[_MAX_FNAME];
        wcsncpy_s(szOrigPDBSansPath, _countof(szOrigPDBSansPath), szPDBSansPath, _TRUNCATE);
        unsigned id = 2;

        while (1) {
            _wmakepath_s(szPDBLocal, _countof(szPDBLocal), szPathDrive, szPathDir, szPDBSansPath, szPDBExt);
            OpenValidate4(
                    szPDBLocal,
                    szMode,
                    &sig70,
                    0,
                    age,
                    &ecPrev,
                    szErrorPrev,
                    cbErrMax,
                    pppdb);

            // TFS 430290 -- When creating link repro, linker may copy multiple same named pdb files into
            // the link repro folder and name them, for example, vc80.pdb, vc80.2.pdb, vc80.3.pdb, and so
            // on, while in the modules debug record still says that the referenced PDB file is vc80.pdb.
            // So when vc80.pdb is found not to match, we should try other vc80.n.pdb.

            if (!*pppdb && (ecPrev == EC_INVALID_SIG)) {
                wchar_t  szDotN[_MAX_FNAME];
                swprintf_s(szDotN, _countof(szDotN), L".%u", id++);
                wcsncpy_s(szPDBSansPath, _countof(szPDBSansPath), szOrigPDBSansPath, _TRUNCATE);
                wcsncat_s(szPDBSansPath, _countof(szPDBSansPath), szDotN, _TRUNCATE);
            }
            else {
                break;
            }
        }
    }

    if (!*pppdb) {
        // try opening pdb as originally referenced
        OpenValidate4(
                szPDB,
                szMode,
                &sig70,
                0,
                age,
                pec,
                szError,
                cbErrMax,
                pppdb);

        if (!*pppdb) {
            // see if we have a better error from the previous open attempt
            //
            if ( szPath && !FBetterError(ecPrev, *pec) ) {
                assert(ecPrev != EC_OK);
                setError(pec, ecPrev, szError, cbErrMax, szErrorPrev);
            }

            return FALSE;
        }
    }

    return TRUE;
}

BOOL PDB1::savePdbStream()
{
    PDBLOG_FUNC();

    MTS_PROTECT(m_csPdbStream);

    Buffer  buf;
    size_t  cbPdbStream;
    
    if (fIsPdb70()) {
        cbPdbStream = sizeof(PDBStream70);
    }
    else {
        cbPdbStream = sizeof(PDBStream);
    }

    if (!buf.Append((PB)&pdbStream, static_cast<CB>(cbPdbStream)) || !nmt.save(&buf)) {
        setLastError(EC_OUT_OF_MEMORY);
        return FALSE;
    }

    if (m_fContainIDStream) {
        DWORD dw = m_fVC120 ? impvVC110 : impvVC140;

        if (!buf.Append((PB)(&dw), sizeof(DWORD))) {
            setLastError(EC_OUT_OF_MEMORY);
            return FALSE;
        }

        if (m_fNoTypeMerge) {
            dw = featNoTypeMerge;

            if (!buf.Append((PB)(&dw), sizeof(DWORD))) {
                setLastError(EC_OUT_OF_MEMORY);
                return FALSE;
            }
        }

        if (m_fMinimalDbgInfo) {
            dw = featMinimalDbgInfo;

            if (!buf.Append((PB)(&dw), sizeof(DWORD))) {
                setLastError(EC_OUT_OF_MEMORY);
                return FALSE;
            }
        }
    }

    return pmsf->ReplaceStream(snPDB, buf.Start(), buf.Size());
}


BOOL PDB1::loadPdbStream(MSF *pmsf, const wchar_t *wszPDB, EC *pec,  __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax)
{
    PDBLOG_FUNC();
    MTS_ASSERT_SINGLETHREAD();

    // MTS NOTE: don't have to protect this function, since this is only call
    // when we are opening a new PDB, we just created "this", so no other thread
    // has access to "this and call this function

    Buffer  buf(0, 0, FALSE);       // no padding necessary
    PB      pb, pbEnd;
    CB      cbStream = pmsf->GetCbStream(snPDB);

    if (cbStream < sizeof PDBStream) {
        goto formatError;
    }

    if (!buf.Reserve(cbStream, &pb)) {
        setError(pec, EC_OUT_OF_MEMORY, wszError, cchErrMax, wszPDB);
        return FALSE;
    }

    if (!pmsf->ReadStream(snPDB, pb, cbStream)) {
        goto formatError;
    }

    assume(cbStream >= 0);

    pdbStream = *(PDBStream*)pb;
    pbEnd = pb + cbStream;
    pb += sizeof(PDBStream);

    if (cbStream == sizeof(PDBStream) || pdbStream.impv == impvVC2) {
        // if we only have a PDBStream sized thing, it is a vc2 pdb.
        // We don't read them anymore

        goto formatError;
    }

    if (pdbStream.impv >= impvVC4 && pdbStream.impv <= impvVC140) {
#pragma message("TODO:Remove interim VC70 IMPV code")
        if (pdbStream.impv > impvVC70Dep) {
            // handle the 7.0 signature

            if (cbStream <= sizeof(PDBStream70)) {
                goto formatError;
            }

            pdbStream.sig70 = *reinterpret_cast<PCSIG70>(pb);
            pb += static_cast<CB>(sizeof(PDBStream70) - sizeof(PDBStream));
        }

        if (!nmt.reload(&pb, static_cast<CB>(pbEnd - pb))) {
            return FALSE;
        }

        while (pbEnd >= pb + 4) {
            DWORD sig = *(DWORD UNALIGNED *) pb;
            
            if (sig == impvVC110) {
                m_fContainIDStream = true;

                // No other signature appened for vc110 PDB.

                break;
            }

            if (sig == impvVC140) {
                m_fContainIDStream = true;
            }
            else if (sig == featNoTypeMerge) {
                m_fNoTypeMerge = true;
            }
            else if (sig == featMinimalDbgInfo) {
                m_fMinimalDbgInfo = true;
            }

            pb += sizeof(DWORD);
        }

        if (trace((trStreams, "streams {\n"))) {
            EnumNMTNI e(nmt);
            while (e.next()) {
                SZ_CONST sz;
                NI ni;
                e.get(&sz, &ni);
                trace((trStreams, "%-20s %7d\n", sz, pmsf->GetCbStream((SN)ni)));
            }
            trace((trStreams, "}\n"));
        }

        return TRUE;
    }

formatError:
    setError(pec, EC_FORMAT, wszError, cchErrMax, wszPDB);
    return FALSE;
}


void PDB1::setLastError(EC ec, const char *szErr)
{
    PDBLOG_FUNC();
    wchar_t wszErrLast[cbErrMax];

    if ((szErr == NULL) ||
        (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED,
                             szErr, -1, wszErrLast, cbErrMax) == 0)) {
        // No description or conversion failed

        setLastError(ec, (const wchar_t *) NULL);
        return;
    }

    setLastError(ec, wszErrLast);
}

void PDB1::setLastError(EC ec, const wchar_t *wszErr)
{
    pPDBError->SetLastError(ec, wszErr);
}


void PDB1::setOOMError()
{
    PDBLOG_FUNC();
    setLastError(EC_OUT_OF_MEMORY);
}

void PDB1::setUsageError()
{
    PDBLOG_FUNC();
    setLastError(EC_USAGE);
}

void PDB1::setAccessError()
{
    PDBLOG_FUNC();
    setLastError(EC_ACCESS_DENIED, wszPDBName);
}
    
void PDB1::setWriteError()
{
    PDBLOG_FUNC();
    setLastError(EC_FILE_SYSTEM, wszPDBName);
}

void PDB1::setReadError()
{
    // We're not too specific just now
    PDBLOG_FUNC();
    setWriteError();
}

void PDB1::setCorruptError()
{
    setLastError(EC_CORRUPT);
}

void PDB1::setCorruptTypePoolError(PDB * ppdbFrom) 
{          
    wchar_t wszPdbName[_MAX_PATH];

    wszPdbName[0] = L'\0';
    if (ppdbFrom) {
        ppdbFrom->QueryPDBNameExW(wszPdbName, _countof(wszPdbName));
        setLastError(EC_CORRUPT_TYPEPOOL, wszPdbName);
    } else {
        setCorruptError();
    }
}

EC PDB1::QueryLastErrorExW( __out_ecount_opt(cchMax) wchar_t *wszError, size_t cchMax)
{
    return pPDBError->QueryLastError(wszError, cchMax);
}


SZ PDB1::QueryPDBName(char szPDB[PDB_MAX_PATH])
{
    PDBLOG_FUNC();
    // MTS NOTE: wszPDBName is assigned at the constructor only, will never change
    dassert(szPDB);
    
    if (WideCharToMultiByte(CP_ACP, 0, wszPDBName, -1, szPDB, PDB_MAX_PATH, NULL, NULL) == 0) {
        // Conversion failed

        szPDB[0] = '\0';
    }

    return szPDB;
}


wchar_t *PDB1::QueryPDBNameExW(__out_ecount(cchMax) wchar_t *wszPDB, size_t cchMax)
{
    PDBLOG_FUNC();
    // MTS NOTE: wszPDBName is assigned at the constructor only, will never change
    dassert(wszPDB);

    if (cchMax > 0) {
        wcsncpy_s(wszPDB, cchMax, wszPDBName, _TRUNCATE);
    }

    return wszPDB;
}


SIG PDB1::QuerySignature()
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_csPdbStream);

    return pdbStream.sig;
}


BOOL PDB1::ResetGUID(BYTE *pb, DWORD cb)
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_csPdbStream);

    if (pb == NULL || cb == 0) {
        setLastError(EC_USAGE);
        return FALSE;
    }

    // Check if the PDB file has already existed. If so, don't reset its GUID.

    SIG70 sig70Repro;

    memset(&sig70Repro, -1, sizeof(SIG70));

    if (memcmp(&pdbStream.sig70, &sig70Repro, sizeof(SIG70)) != 0) {
        return TRUE;
    }

    // Calculate SHA256 hash

    BYTE *pbHash = NULL;
    DWORD cbHash;

    if (!FCalculateSHA256(pb, cb, &pbHash, &cbHash)) {
        if (pbHash != NULL) {
            delete [] pbHash;
        }

        return FALSE;
    }

    assert(cbHash >= sizeof(SIG70));

    BYTE *pbSig = pbHash + (cbHash - sizeof(SIG));

    memcpy(&pdbStream.sig70, pbHash, sizeof(SIG70));
    pdbStream.sig = *((DWORD *) pbSig);

    delete [] pbHash;

    return TRUE;
}


BOOL PDB1::QuerySignature2(PSIG70 psig70)
{
    PDBLOG_FUNC();

    MTS_PROTECT(m_csPdbStream);

    if (pdbStream.impv >= impvVC70)
        memcpy(psig70, &pdbStream.sig70, sizeof(SIG70));
    else 
        *psig70 = Sig70FromSig(pdbStream.sig);

    return TRUE;
}


AGE PDB1::QueryAge()
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_csPdbStream);

    return pdbStream.age;
}


BOOL PDB1::CreateDBI(SZ_CONST szTarget, OUT DBI** ppdbi)
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_csForDBI);

#ifdef PDB_MT
    if (pdbi1) {
        // we should have exclusive write access when creating DBIs
        // there is already created and opened
        setAccessError();
        return FALSE;
    }
    assert(dwDBIOpenedCount == 0);
#endif
    pdbi1 = new (this) DBI1(this, TRUE, m_fPdbCTypes, m_fPdbTypeMismatch);    // write 
    if (pdbi1) {
        *ppdbi = (DBI*)pdbi1;
        if (pdbi1->fInit(TRUE)) {           //  create
            *ppdbi = (DBI*)pdbi1;
#ifdef PDB_MT
            dwDBIOpenedCount++;
#endif
            return TRUE;
        }
        pdbi1->internal_Close();            // do a hard close
        pdbi1 = NULL;
    }
    return FALSE;
}


BOOL PDB1::OpenDBI(SZ_CONST szTarget, SZ_CONST szMode, OUT DBI** ppdbi)
{
    PDBLOG_FUNC();
    return OpenDBIEx(szTarget, szMode, ppdbi);
}


BOOL PDB1::OpenDBIEx(SZ_CONST szTarget, SZ_CONST szMode, OUT DBI** ppdbi, PfnFindDebugInfoFile pfn_)
{
    PDBLOG_FUNC();
    BOOL    fWrite = !!strchr(szMode, *pdbWrite);
    BOOL    fWriteShared = !!strchr(szMode, *pdbWriteShared);

#ifdef PDB_MT
    if (fWriteShared || (fWrite && m_fRead)) {
        setAccessError();
        return FALSE;
    }

    MTS_PROTECT(m_csForDBI);

    if (pdbi1) {
        
        if (fWrite || pdbi1->fWrite) {
            // DBI always open with exclusive write
            setAccessError();
            return FALSE;
        }

        *ppdbi = pdbi1;
        dwDBIOpenedCount++;
        return TRUE;
    }
    assert(dwDBIOpenedCount == 0);
#endif
    
    pdbi1 = new (this) DBI1(this, fWrite, m_fPdbCTypes, m_fPdbTypeMismatch);
    if (pdbi1) {
#ifdef PDB_TYPESERVER
        pdbi1->SetFindFunc( pfn_ );
#endif
        if (pdbi1->fInit(FALSE)) {      // never create
            *ppdbi = (DBI*)pdbi1;
#ifdef PDB_MT
            dwDBIOpenedCount++;
#endif
            return TRUE;
        }
        pdbi1->internal_Close();        // do a hard close
        pdbi1 = NULL;
    }
    return FALSE;
}

// used only by DBI1::Close()
BOOL PDB1::internal_CloseDBI() {
#ifdef PDB_MT
    MTS_PROTECT(m_csForDBI);

    assert(pdbi1);
    assert(dwDBIOpenedCount != 0);
    if (--dwDBIOpenedCount != 0) {
        return FALSE;                   // tell DBI1::Close not to close itself just yet
    }
#endif
    pdbi1 = NULL;
    return TRUE;                    // tell DBI1::Close to really close itself              
}

BOOL PDB1::OpenTpi(SZ_CONST szMode, OUT TPI** pptpi) {
    PDBLOG_FUNC();
    MTS_PROTECT(m_csForTPI);

    if (ptpi1) {
        *pptpi = (TPI*)ptpi1;
#ifdef PDB_MT
        dwTPIOpenedCount++;
#endif
        return TRUE;
    }
    dassert(pmsf);

    if (!(ptpi1 = new (this) TPI1(pmsf, this, snTpi)))
        return FALSE;
    else if (ptpi1->fOpen(szMode)) {
        *pptpi = (TPI*)ptpi1;
#ifdef PDB_MT
        dwTPIOpenedCount++;
#endif
        return TRUE;
    }
    else {
        // failure code set elsewhere
        delete ptpi1;
        ptpi1 = 0;
        return FALSE;
    }
}

BOOL PDB1::OpenIpi(SZ_CONST szMode, OUT TPI** ppipi) {
    PDBLOG_FUNC();
    MTS_PROTECT(m_csForIPI);

    if (pipi) {
        *ppipi = pipi;
#ifdef PDB_MT
        dwIPIOpenedCount++;
#endif
        return TRUE;
    }
    dassert(pmsf);

    // For old version PDB, there is no ID stream.

    if (!fIsPdb110()) {
        // !!! HACK HACK HACK !!!
        // 
        // When 16.1 ships, Vulcan will use mspdb80.dll unfortunately.  If a
        // given PDB contains ID stream, after bbopt is done, the ID stream
        // is still kept in the updated PDB but this fact is not reflected in
        // the PDB stream, which would cause trouble to subsequent access to
        // the PDB because when opening a PDB we check whether it contains ID
        // stream by examining PDB stream.
        //
        // After Vulcan is updated to use mspdb100.dll shipped in 16.1, this
        // hack can be removed.  For now we need it to allow Windows to build
        // (where a BBT-updated PDB will be used to store type info from some
        // subsequent compilations), and to allow debugger able to access info
        // stored in ID stream in BBT updated PDBs.
        //
        // The hack is: if the stream that is supposed to contain ID records
        // is not empty and doesn't have a name, we try open it as if it was
        // an ID stream and initialize it.  If this works, then the chance
        // that this PDB really doesn't contain an ID stream is very very low.

        if (pmsf->GetCbStream(snIpi) == 0) {
            // ID stream is empty
            pipi = NULL;
            return FALSE;
        }

        // Check if stream "snIpi" has a name

        bool fStreamHasName = false;
        EnumNameMap *penum;

        if (!GetEnumStreamNameMap((Enum **) &penum)) {
            pipi = NULL;
            return FALSE;
        }

        while (penum->next()) {
            const char *sz;
            NI ni;

            penum->get(&sz, &ni);

            if (ni == snIpi) {
                assert(sz != NULL);
                fStreamHasName = true;
                break;
            }
        }

        penum->release();

        if (fStreamHasName) {
            pipi = NULL;
            return FALSE;
        }
    }

    if (!(pipi = new (this) TPI1(pmsf, this, snIpi)))
        return FALSE;
    else if (pipi->fOpen(szMode)) {
        *ppipi = pipi;
#ifdef PDB_MT
        dwIPIOpenedCount++;
#endif
        if (!fIsPdb110()) {
            // Set the flag now that we have discovered that this PDB really
            // contains an ID stream.
            m_fContainIDStream = true;
        }
        return TRUE;
    }
    else {
        // failure code set elsewhere
        delete pipi;
        pipi = NULL;
        return FALSE;
    }
}

// used by TPI1::Close() only
BOOL PDB1::internal_CloseTPI(TPI1* p) {
    PDBLOG_FUNC();

    assert(p);
    assert(ptpi1 || pipi);

    DWORD* pdwRefCnt = NULL;

#ifdef PDB_MT
    // do the ref counting and directect TPI1::Close() to delete itself or not
    if (p == ptpi1) {
        MTS_PROTECT(m_csForTPI);
        pdwRefCnt = &dwTPIOpenedCount;
    }
    else if (p == pipi) {
        MTS_PROTECT(m_csForIPI);
        pdwRefCnt = &dwIPIOpenedCount;
    }
    else {
        assert(false);
    }

    assert(*pdwRefCnt != 0);
    if (--(*pdwRefCnt) != 0) {
        return FALSE;           // tell TPI1::Close() not to delete itself
    }
#endif

    if (p == ptpi1) {
        ptpi1 = NULL;
    }
    else {
        pipi = NULL;
    }

    return TRUE;                // tell TPI1::Close() to delete itself
}

BOOL PDB1::Commit()
{
    PDBLOG_FUNC();
    assert(pmsf);
    
    if (savePdbStream() &&
        pmsf->Commit())
        return TRUE;
    else {
        setWriteError();
        return FALSE;
    }
}


BOOL PDB1::Close()
{
    PDBLOG_FUNC();

#ifdef PDB_MT
    MTS_PROTECT(s_csForPDBOpen);
    debug(PDB1 * ppdb1 = NULL);
    assert(s_mpOpenedPDB.map(pmsf, &ppdb1));
    assert(ppdb1 == this);
    if (--dwPDBOpenedCount != 0) {
        return TRUE;
    }
    return s_pdbTimeoutManager.NotifyClose(this);
#else    
    return internal_Close();
#endif


}

BOOL PDB1::internal_Close() {
    MTS_ASSERT(s_csForPDBOpen);
    //MTS_PROTECT(m_csForDBI);
    //MTS_PROTECT(m_csForTPI);
    //MTS_PROTECT(m_csForNameMap);
#ifdef PDB_MT
    debug(PDB1 * ppdb1);
    assert(s_mpOpenedPDB.map(pmsf, &ppdb1));
    s_mpOpenedPDB.remove(pmsf);
#endif

    if (pdbi1) {
#ifdef PDB_MT
        // obviously, someone didn't close the DBI before closing the PDB
        PDBLOG(LOG_CUSTOM, "closing DBI with ref count &d", dwDBIOpenedCount);
        dwDBIOpenedCount = 1;           // fake to have only one ref and call close
#endif
        pdbi1->Close();
    }

    if (ptpi1) {
#ifdef PDB_MT
        // obviously, someone didn't close the TPI before closing the PDB
        PDBLOG(LOG_CUSTOM, "closing TPI with ref count %d", dwTPIOpenedCount);
        dwTPIOpenedCount = 1;           // fake to have only one ref and call close
#endif
        ptpi1->Close();
    }

    if (pipi) {
#ifdef PDB_MT
        // obviously, someone didn't close the IPI before closing the PDB
        PDBLOG(LOG_CUSTOM, "closing IPI with ref count %d", dwIPIOpenedCount);
        dwIPIOpenedCount = 1;           // fake to have only one ref and call close
#endif
        pipi->Close();
    }

    if (m_pnamemap) {
        // obviously, someone didn't close the NameMap before closing the PDB
        PDBLOG(LOG_CUSTOM, "closing namemap with ref count &d", dwNameMapOpenedCount);
        dwNameMapOpenedCount = 1;       // fake to have only one ref and call close
        m_pnamemap->close();
    }
       
#ifdef PDB_MT
    EnumMap<NI, Strm *, LHcNi> e(m_mpOpenedStream);
    while (e.next()) {
        NI ni;
        Strm * pstream;
        e.get(&ni, &pstream);
        delete pstream;
    }
#endif

    if ( pPDBError != NULL ) {
        pPDBError->Destroy();
    }

    for (size_t i = 0; i < m_rgPDBMapping.size(); i++) {
        freeWsz(m_rgPDBMapping[i].szFrom);
        freeWsz(m_rgPDBMapping[i].szTo);
    }

    BOOL fResult = FALSE;
    if (!pmsf || pmsf->Close()) {
        pmsf = NULL;
        delete this;
        fResult = TRUE;
    }
    else {
        setLastError(EC_UNKNOWN);
    }

#if defined(CRT_LEAKS)
    // Send all reports to STDOUT
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );

    _CrtMemCheckpoint( &s2 );

    if ( _CrtMemDifference( &s3, &s1, &s2 ) ) {
        _CrtMemDumpStatistics( &s3 );
    }
#endif

    return fResult;
}


BOOL PDB1::GetRawBytes(PFNfReadPDBRawBytes pfnSnarfRawBytes)
{
    PDBLOG_FUNC();
    assert(pmsf);

    if (pmsf->GetRawBytes(pfnSnarfRawBytes)) {
        return TRUE;
    }
    else {
        setReadError();
        return FALSE;
    }
}


BOOL PDB1::fEnsureSn(SN* psn)
{
    PDBLOG_FUNC();
    if (*psn == snNil) {
        // get a new stream, but skip stream numbers in [0..snSpecialMax)
        for (;;) {
            *psn = pmsf->GetFreeSn();
            if (*psn == snNil) {
                setLastError(EC_LIMIT);
                return FALSE;
                }
            else if (*psn >= snSpecialMax)
                return TRUE;
            else if (!pmsf->ReplaceStream(*psn, 0, 0)) {
                setWriteError();
                return FALSE;
                }
        }
    }
    else
        return TRUE;
}


BOOL PDB1::fEnsureNoSn(SN* psn)
{
    PDBLOG_FUNC();
    if (*psn != snNil) {
        if (!pmsf->DeleteStream(*psn)) {
            setWriteError();
            return FALSE;
            }
        *psn = snNil;
    }
    return TRUE;
}


// return size of Dbg type element in bytes
long cbDbgtype(DBGTYPE dbgtype)
{
    PDBLOG_FUNC();

    int cb = 0;

    switch (dbgtype) {
    case dbgtypeFPO:
        cb = sizeof(FPO_DATA);
        break;

    case dbgtypeException:
        cb = sizeof(IMAGE_FUNCTION_ENTRY);
        break;

    case dbgtypeFixup:
        cb = sizeof(XFIXUP_DATA);
        break;

    case dbgtypeOmapToSrc:
    case dbgtypeOmapFromSrc:
        cb = sizeof(OMAP_DATA);
        break;

    case dbgtypeSectionHdr:
    case dbgtypeSectionHdrOrig:
        cb = sizeof(IMAGE_SECTION_HEADER);
        break;

    case dbgtypeTokenRidMap:
        cb = sizeof(ULONG);
        break;

    case dbgtypeXdata:
        cb = 1;
        break;

    case dbgtypePdata:
        cb = 1;
        break;

    case dbgtypeNewFPO:
        cb = sizeof(FRAMEDATA);
        break;

    default:
        assert (0);
    }

    return cb;
}

class PDBCopy {
public:
    PDBCopy(
        PDB *ppdbIn_,
        PDB *ppdbOut_,
        DBI *pdbiIn_,
        DBI *pdbiOut_,
        DWORD dwCopyFilter_,
        PfnPDBCopyQueryCallback pfn,
        void * pvClientContext_
        );
    ~PDBCopy();
    BOOL DoCopy();
    EC QueryLastErrorExW(__out_ecount(cchMax) wchar_t *wszError, size_t cchMax);
    void SetLastError(PDB *ppdb);

private:
    BOOL     BuildImodToIsectCoffMap();
    void     CloseMods(Mod ** ppmod);
    BOOL     CopyDbg();
    BOOL     CopyDbgtype(DBGTYPE dbgtype);
    BOOL     CopyDbiHdr();
    BOOL     CopyMods();
    BOOL     CopyPrivateSymbols(Mod *pmodIn, Mod *pmodOut);
    BOOL     CopyPublics();
    BOOL     CopySecMap();
    BOOL     CopySubsection(Mod *pmodIn, Mod *pmodOut, enum DEBUG_S_SUBSECTION_TYPE e);
    BOOL     CreateStringSubSection(enum DEBUG_S_SUBSECTION_TYPE e, BYTE *pb, DWORD cb, Mod *pmodOut);
    BYTE *   PBNextRecord(BYTE *pb, enum DEBUG_S_SUBSECTION_TYPE e);
    BOOL     ProcessModInMiniPDB(Mod *pmodIn, Mod *pmodOut);
    BYTE *   PBStringIndex(BYTE *pb, enum DEBUG_S_SUBSECTION_TYPE e);
    BOOL     RegisterPDBMapping();

    PDB *     ppdbIn;
    PDB *     ppdbOut;
    DBI *     pdbiIn;
    DBI *     pdbiOut;
    Mod **    rgpmodIn;
    Mod **    rgpmodOut;

    PfnPDBCopyQueryCallback pfnPdbCopyQueryCallback;
    void *    pvClientContext;

    ULONG     cMods;
    DWORD     dwCopyFilter;
    EC        ecLast;
    wchar_t * wszErrLast;

    bool      fPdbMappingRegistered;

    Map<DWORD, Array<DWORD> *, HcNi> mpImodToRgIsectCoff;
};


PDBCopy::PDBCopy (
    PDB *ppdbIn_, 
    PDB *ppdbOut_, 
    DBI *pdbiIn_, 
    DBI *pdbiOut_, 
    DWORD dwCopyFilter_,
    PfnPDBCopyQueryCallback pfn,
    void * pv)
{
    assert (ppdbIn_);
    assert (ppdbOut_);
    assert (pdbiIn_);
    assert (pdbiOut_);

    ppdbIn = ppdbIn_;
    ppdbOut = ppdbOut_;
    pdbiIn = pdbiIn_;
    pdbiOut = pdbiOut_;
    dwCopyFilter = dwCopyFilter_;
    ecLast = EC_OK;
    rgpmodIn = rgpmodOut = 0;
    pfnPdbCopyQueryCallback = pfn;
    pvClientContext = pv;
    wszErrLast = NULL;
    cMods = 0;
    fPdbMappingRegistered = false;
}


PDBCopy::~PDBCopy()
{
    // Close Mod interfaces and release buffers
    CloseMods(rgpmodIn);
    CloseMods(rgpmodOut);

    if (rgpmodIn) {
        delete [] rgpmodIn;
    }

    if (rgpmodOut) {
        delete [] rgpmodOut;
    }

    if (wszErrLast) {
        delete [] wszErrLast;
    }

    EnumMap<DWORD, Array<DWORD> *, HcNi> e(mpImodToRgIsectCoff);

    while (e.next()) {
        DWORD imod;
        Array<DWORD> *prg;

        e.get(&imod, &prg);

        if (prg != NULL) {
            delete prg;
        }
    }
}


void PDBCopy::CloseMods(Mod **ppmod)
{
    if (ppmod) {
        for (ULONG i=0; i < cMods + 1; i++) {
            if (ppmod[i]) {
                BOOL fRet = ppmod[i]->Close();
                assert(fRet);
            }
        }
    }
}


BOOL PDBCopy::DoCopy()
{
    return CopyDbiHdr() &&
        CopyMods() &&
        CopyPublics() &&
        CopySecMap() &&
        CopyDbg();
}


EC PDBCopy::QueryLastErrorExW(__out_ecount_opt(cchMax) wchar_t *wszError, size_t cchMax)
{
    if (wszError != NULL && cchMax > 0 && wszErrLast != NULL) {
        wcsncpy_s(wszError, cchMax, wszErrLast, _TRUNCATE);
    }

    return ecLast;
}

void PDBCopy::SetLastError(PDB *ppdb)
{
    wchar_t wszErr[cbErrMax] = {0};

    ecLast = ppdb->QueryLastErrorExW(wszErr, cbErrMax);

    if (wszErr[0] != L'\0') {
        if (wszErrLast == NULL) {
            wszErrLast = new wchar_t[cbErrMax];

            // It is fine if we run out of memory, since we are reporting
            // a failure and have already set the error code.
        }

        if (wszErrLast != NULL) {
            wcsncpy_s(wszErrLast, cbErrMax, wszErr, _TRUNCATE);
        }
    }
}

BOOL PDBCopy::CopyDbiHdr()
{
    assert(pdbiIn  != NULL);
    assert(pdbiOut != NULL);

    pdbiOut->SetMachineType(pdbiIn->QueryMachineType());

    return TRUE;
}


BYTE * PDBCopy::PBNextRecord(BYTE *pb, enum DEBUG_S_SUBSECTION_TYPE e)
{
    switch (e) {
        case DEBUG_S_FILECHKSMS:
            pb += sizeof(DWORD) + 2 + *(pb + sizeof(DWORD));
            pb += dcbAlign((size_t) pb);
            return pb;

        case DEBUG_S_SYMBOLS:
            return pbEndSym((PSYM) pb);

        default:
            assert(false);
            return NULL;
    }
}


BYTE * PDBCopy::PBStringIndex(BYTE *pb, enum DEBUG_S_SUBSECTION_TYPE e)
{
    switch (e) {
        case DEBUG_S_FILECHKSMS:
            return pb;

        case DEBUG_S_SYMBOLS:
            switch (((PSYM) pb)->rectyp) {
                case S_DEFRANGE:
                case S_DEFRANGE_SUBFIELD:
                    return pb + offsetof(DEFRANGESYM, program);

                case S_FILESTATIC:
                    return pb + offsetof(FILESTATICSYM, modOffset);

                default:
                    return NULL;
            }

        default:
            assert(false);
            return NULL;
    }
}


BOOL PDBCopy::CreateStringSubSection(enum DEBUG_S_SUBSECTION_TYPE e, BYTE *pbIn, DWORD cb, Mod *pmodOut)
{
    assert(pbIn != NULL);
    assert(cb != 0);

    Buffer bufStr;
    BYTE bZero = 0;

    if (!bufStr.Append(&bZero, 1)) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    NameMap *pnmpIn = NULL;

    if (!NameMap::open(ppdbIn, FALSE, &pnmpIn)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    PB pb = pbIn;
    PB pbEnd = pbIn + cb;

    // Enumerate through file checksum records.  Compose string
    // table subsection and update file name string index.

    while (pb < pbEnd) {
        PB pbStrIdx = PBStringIndex(pb, e);

        if (pbStrIdx != NULL) {
            DWORD off = *(DWORD *) pbStrIdx;
            const char *sz;

            if (!pnmpIn->getName(off, &sz)) {
                SetLastError(ppdbIn);
                return FALSE;
            }

            DWORD cch = (DWORD) strlen(sz) + 1;
            DWORD index = bufStr.Size();

            if (!bufStr.Append((PB) sz, cch)) {
                ecLast = EC_OUT_OF_MEMORY;
                return FALSE;
            }

            *(DWORD *) pbStrIdx = index;
        }

        pb = PBNextRecord(pb, e);
    }

    // Add string table subsection.

    enum DEBUG_S_SUBSECTION_TYPE eOut = DEBUG_S_STRINGTABLE;
    DWORD sigC13 = CV_SIGNATURE_C13;
    DWORD cbStr = bufStr.Size();
    Buffer bufOut;

    if (!bufOut.Append((PB) &sigC13, sizeof(DWORD)) ||
        !bufOut.Append((PB) &eOut, sizeof(DWORD)) ||
        !bufOut.Append((PB) &cbStr, sizeof(DWORD)) ||
        !bufOut.Append(bufStr.Start(), cbStr) ||
        !bufOut.AppendFmt("f", dcbAlign(cbStr))) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    if (!pmodOut->AddSymbols(bufOut.Start(), bufOut.Size())) {
        SetLastError(ppdbOut);
        return FALSE;
    }

    return TRUE;
}


BOOL PDBCopy::CopySubsection(Mod *pmodIn, Mod *pmodOut, enum DEBUG_S_SUBSECTION_TYPE e)
{
    Mod1::PfnQuerySubsection pfn = NULL;

    switch (e) {
        case DEBUG_S_LINES:
            pfn = &Mod1::QueryLines3;
            break;
        case DEBUG_S_FILECHKSMS:
            pfn = &Mod1::QueryFileChecksums;
            break;
        default:
            assert(0);
            return FALSE;
    }

    Mod1 *pmod1In = (Mod1 *) pmodIn;
    DWORD cb;

    if (!(pmod1In->*pfn)(0, NULL, &cb)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    if (cb == 0) {
        return TRUE;
    }

    Buffer buf;

    if (!buf.Reserve(cb)) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    DWORD cbRead;

    if (!(pmod1In->*pfn)(cb, buf.Start(), &cbRead) || (cb != cbRead)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    // If this is file checksum info, need to synthesize and add string subsection.

    if (e == DEBUG_S_FILECHKSMS) {
        if (!CreateStringSubSection(e, buf.Start(), buf.Size(), pmodOut)) {
            return FALSE;
        }
    }

    Buffer bufOut;
    DWORD sigC13 = CV_SIGNATURE_C13;
    
    if (!bufOut.Append((PB) &sigC13, sizeof(DWORD)) ||
        !bufOut.Append((PB) &e, sizeof(DWORD)) ||
        !bufOut.Append((PB) &cb, sizeof(DWORD)) ||
        !bufOut.Append(buf.Start(), cb) ||
        !bufOut.AppendFmt("f", dcbAlign(cb))) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    if (!pmodOut->AddSymbols(bufOut.Start(), bufOut.Size())) {
        SetLastError(ppdbOut);
        return FALSE;
    }

    return TRUE;
}


BOOL PDBCopy::BuildImodToIsectCoffMap()
{
    if (mpImodToRgIsectCoff.count() != 0) {
        return TRUE;
    }

    // Insert a dummy entry in case of empty section contribution list.

    if (!mpImodToRgIsectCoff.add(0, NULL)) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    assert(ppdbIn->FMinimal());

    EnumContrib *pEnumSC;

    if (!pdbiIn->getEnumContrib2((Enum **)&pEnumSC)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    Map<DWORD, bool, HcNi> mpImodSeenNonComdat;

    mpImodSeenNonComdat.reset();

    while (pEnumSC->next()) {
        IMOD   imod;
        ISECT  isect;
        DWORD  off;
        DWORD  cb;
        ULONG  dwCharacteristics;
        DWORD  isectCoff;

        pEnumSC->get2(&imod, &isect, &off, &isectCoff, &cb, &dwCharacteristics);

        bool fNonComdat = ((dwCharacteristics & IMAGE_SCN_LNK_COMDAT) == 0);

        if (fNonComdat) {
            if (!mpImodSeenNonComdat.add(imod, 0)) {
                ecLast = EC_OUT_OF_MEMORY;
                return FALSE;
            }
        }

        Array<DWORD> *prg;

        if (!mpImodToRgIsectCoff.map(imod, &prg)) {
            prg = new Array<DWORD>();

            if ((prg == NULL) || !mpImodToRgIsectCoff.add(imod, prg)) {
                ecLast = EC_OUT_OF_MEMORY;
                return FALSE;
            }
        }

        BOOL f;

        if (fNonComdat) {
            f = prg->insertAt(0, isectCoff);
        } else {
            f = prg->append(isectCoff);
        }

        if (!f) {
            ecLast = EC_OUT_OF_MEMORY;
            return FALSE;
        }
    }

    pEnumSC->release();

    // For MOD whose contributions are all COMDATs, we need to add debug
    // record in its only non-COMDAT .debug$S section into the PDB.

    EnumMap<DWORD, Array<DWORD> *, HcNi> e(mpImodToRgIsectCoff);

    while (e.next()) {
        DWORD imod;
        Array<DWORD> *prg;
        bool fDummy;

        e.get(&imod, &prg);

        if (prg == NULL) {
            continue;
        }

        if (!mpImodSeenNonComdat.map(imod, &fDummy)) {
            if (!prg->insertAt(0, (DWORD) -1)) {
                // Prepend an -1 so to include the non-comdat .debug$S section
                // in case all other .debug$S sections are COMDATs.

                ecLast = EC_OUT_OF_MEMORY;
                return FALSE;
            }
        }
    }

    return TRUE;
}


#define COPY_SYMBOL_TO_TEMP_BUFFER  \
    {  \
        CB cb = (CB)((PB)psymNext - (PB)psym);  \
        if (!bufTemp.Append((PB)psym, cb)) {  \
            ecLast = EC_OUT_OF_MEMORY;  \
            return FALSE;  \
        }  \
    }

BOOL PDBCopy::CopyPrivateSymbols(
    Mod *pmodIn,
    Mod *pmodOut)
{
    if ((dwCopyFilter & copyCustomModSyms) != 0) {
        return ProcessModInMiniPDB(pmodIn, pmodOut);
    }

    // Set up to filter annotations

    PfnPDBCopyFilterAnnotations pfnFilter = NULL;

    if ((dwCopyFilter & copyKeepAnnotation) && pfnPdbCopyQueryCallback) {
        pfnFilter = PfnPDBCopyFilterAnnotations((*pfnPdbCopyQueryCallback)(pvClientContext, pccFilterAnnotations));
    }

    Buffer bufIn, bufTemp;
    CB     cbSyms;

    // Get all the symbols in the input MOD.

    if (!pmodIn->QuerySymbols(NULL, &cbSyms)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    if (!bufIn.Reserve(cbSyms)) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    if (!pmodIn->QuerySymbols(bufIn.Start(), &cbSyms)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    bool fKeepAnnotation = ((dwCopyFilter & copyKeepAnnotation) != 0);
    bool fCheckLastString = ((dwCopyFilter & copyKeepAnnotation2) != 0);

    PSYM psymEndToCopy = NULL;
    PSYM psymNext;

    for (PSYM psym = (PSYM)(bufIn.Start() + sizeof(ULONG));
         (PB) psym < bufIn.End();
         psym = psymNext)
    {
        psymNext = (PSYM) pbEndSym(psym);

        if (psymNext > (PSYM)bufIn.End()) {
            ecLast = EC_CORRUPT;
            return FALSE;
        }

        if (fKeepAnnotation && (psym->rectyp == S_ANNOTATION)) {
            // Process annotation symbol.

            if (pfnFilter) {
                ANNOTATIONSYM *s = (ANNOTATIONSYM *) psym;

                if (s->csz == 0) {
                    // Skip annotation symbol that doesn't have a string.

                    continue;
                }

                char *szFirst = (char *) s->rgsz;
                char *sz = NULL;

                if (fCheckLastString) {
                    char *szLast = szFirst;

                    for (unsigned short isz = 0; isz < s->csz - 1; isz++) {
                        szLast += strlen(szLast) + 1;
                    }

                    size_t cbSz = strlen(szFirst) + strlen(szLast)
                                  + 2;  // one for ';' and one for '\0'

                    sz = new char[cbSz];
                    if (sz == NULL) {
                        ecLast = EC_OUT_OF_MEMORY;
                        return FALSE;
                    }

                    strcpy_s(sz, cbSz, szFirst);
                    strcat_s(sz, cbSz, ";");
                    strcat_s(sz, cbSz, szLast);
                }
                else {
                    sz = szFirst;
                }

                // Convert names to wchar_t for filter call

                Buffer bufWideStr;
                size_t cch = UnicodeLengthOfUTF8(sz);

                if (!bufWideStr.Reserve(static_cast<CB>(cbUnicodeFromCch(cch)))) {
                    ecLast = EC_OUT_OF_MEMORY;
                    return FALSE;
                }

                wchar_t *wsz = (wchar_t *)bufWideStr.Start();
                _GetSZUnicodeFromSZUTF8(SZ_CONST(sz), wsz, cch);

                if (fCheckLastString) {
                    delete [] sz;
                }

                // A filter return of true means copy this annotation.
                // A filter return of false means discard it.

                if (!pfnFilter(pvClientContext, wsz)) {
                    continue;
                }
            }

            COPY_SYMBOL_TO_TEMP_BUFFER;
        }
        else if ((psym->rectyp == S_GPROC32) || (psym->rectyp == S_LPROC32)) {
            // Process global/local function symbol.

            PROCSYM32 *s = (PROCSYM32 *) psym;
            PSYM psymEnd = (PSYM) (bufIn.Start() + s->pEnd);

            if (psym->rectyp == S_GPROC32) {
                // Don't copy this global function symbol if no record for
                // separation code follows.

                PSYM psymSep = (PSYM) pbEndSym(psymEnd);

                if ((PB) psymSep >= bufIn.End()) {
                    continue;
                }

                if (psymSep->rectyp != S_SEPCODE) {
                    continue;
                }

                assert(((SEPCODESYM *) psymSep)->sectParent == s->seg);
                assert(((SEPCODESYM *) psymSep)->offParent == s->off);
            }

            psymEndToCopy = psymEnd;

            // Clear the type ID.

            *((UNALIGNED DWORD *) &(s->typind)) = 0;

            COPY_SYMBOL_TO_TEMP_BUFFER;
        }
        else if ((psym->rectyp == S_END) && (psym == psymEndToCopy)) {
            // Copy the S_END symbol that marks the ending scope of the
            // already copied S_GPROC32, S_LPROC32, or S_SEPCODE symbol.

            psymEndToCopy = NULL;

            COPY_SYMBOL_TO_TEMP_BUFFER;
        }
        else if (psym->rectyp == S_SEPCODE) {
            // Copy S_SEPCODE symbol.

            psymEndToCopy = (PSYM) (bufIn.Start() + ((SEPCODESYM *) psym)->pEnd);

            COPY_SYMBOL_TO_TEMP_BUFFER;
        }
    }

    // We are done if there is no annotation symbol.

    CB  cbSymbols = bufTemp.Size();
    if (cbSymbols == 0) {
        return TRUE;
    }

    // Create a symbol sebsection for saved annotations.

    Buffer bufOut;
    BYTE   pbSigSymbolSubSection[4] = {DEBUG_S_SYMBOLS, 0, 0, 0};

    if (!bufOut.Append(bufIn.Start(), sizeof(ULONG)) ||
        !bufOut.Append(pbSigSymbolSubSection, 4) ||
        !bufOut.Append((PB)(&cbSymbols), sizeof(CB)) ||
        !bufOut.Append(bufTemp.Start(), cbSymbols) ||
        !bufOut.AppendFmt("f", dcbAlign(cbSymbols))) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    // Call MOD1::AddSymbols() to add all the info saved above
    // into the output MOD.

    if (!pmodOut->AddSymbols(bufOut.Start(), bufOut.Size())) {
        SetLastError(ppdbOut);
        return FALSE;
    }

    return TRUE;
}

#undef COPY_SYMBOL_TO_TEMP_BUFFER

BOOL PDBCopy::CopyMods()
{
    Mod *pmodIn = NULL;
    Mod *pmodOut = NULL;
    ISECT isect;
    OFF off;
    CB  cbMod;
    DWORD dwCharacteristics;

    // see how many mods we have
    while (pdbiIn->QueryNextMod(pmodIn, &pmodIn) && pmodIn) {
        cMods ++;
    }

    if (pmodIn) {
        // QueryNextMod failed for some reason...
        // TODO: Maybe we need to close open mods before returning
        SetLastError(ppdbIn);
        return FALSE;
    }

    assert(rgpmodIn == NULL);
    assert(rgpmodOut == NULL);

    rgpmodIn = new (zeroed) Mod* [cMods + 1];    // add one to facilitate indexing by imod (1-based)
    rgpmodOut = new (zeroed) Mod* [cMods + 1];

    if (!rgpmodIn || !rgpmodOut) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    PfnPDBCopyReportProgress pfnProgress = NULL;

    if ((dwCopyFilter & copyCustomModSyms) != 0) {
        // Need to register PDB mapping when to convert mini PDB to full PDB

        if (!RegisterPDBMapping()) {
            return FALSE;
        }

        // Also need to query the callback for reporting progress

        assert(pfnPdbCopyQueryCallback != NULL);

        pfnProgress = PfnPDBCopyReportProgress((*pfnPdbCopyQueryCallback)(pvClientContext, pccReportProgress));

        if (pfnProgress == NULL) {
            return FALSE;
        }
    }

    pmodIn = NULL;

    // enumerate modules and add them to the target pdb

    for (ULONG nMod = 0; nMod < cMods; nMod ++) {
        wchar_t szModName[_MAX_PATH];
        wchar_t szFileName[_MAX_PATH];
        long cchName = _countof(szModName);
        long cchFile = _countof(szFileName);

        IMOD imod;

        pdbiIn->QueryNextMod(pmodIn, &pmodIn);

        assert (pmodIn);

        if( !pmodIn->QueryImod(&imod) ||
            !pmodIn->QueryNameW(szModName, &cchName) ||
            !pmodIn->QueryFileW(szFileName, &cchFile) ||
            !pmodIn->QuerySecContrib(&isect, &off, &cbMod, &dwCharacteristics)) {
            SetLastError(ppdbIn);
            pmodIn->Close();
            return FALSE;
        }

        assert (imod < cMods + 1);

        // Copy Module and primary seccontrib.
        // Note: Currently the fist SC added to a Mod is the one to be returned
        // by QuerySecContrib. That's why we add this SC separately, before
        // processing the sorted SC's
        if (!pdbiOut->OpenModW(szModName, szFileName, &pmodOut) ||
            !pmodOut->AddSecContrib(isect, off, cbMod, dwCharacteristics)) {
            SetLastError(ppdbOut);
            return FALSE;
        }

        // Filter private symbols.

        if (!CopyPrivateSymbols(pmodIn, pmodOut)) {
            pmodIn->Close();
            return FALSE;
        }

        rgpmodIn[imod] = pmodIn;
        rgpmodOut[imod] = pmodOut;

        if (pfnProgress != NULL) {
            pfnProgress(pvClientContext, imod, cMods);
        }
    }

    // Copy seccontribs. This is necessary because debuggers may depend
    // on QueryModFromAddr for displaying symbols in call stacks.

    EnumContrib *pEnumSC;
    BOOL fRet;

    if (ppdbIn->FMinimal()) {
        fRet = pdbiIn->getEnumContrib2((Enum **)&pEnumSC);
    } else {
        fRet = pdbiIn->getEnumContrib((Enum **)&pEnumSC);
    }

    if (!fRet) { 
        SetLastError(ppdbIn);
        if (ecLast == EC_OK) {
            return TRUE;
        }
        return FALSE;
    }

    while (pEnumSC->next()) {
        IMOD imodSC;
        ISECT isectSC;
        OFF offSC;
        CB  cbSC;
        ULONG dwCharacteristicsSC;
        DWORD isectCoffSC;

        if ((dwCopyFilter & copyCustomModSyms) != 0) {
            assert(ppdbIn->FMinimal());
            pEnumSC->get2(&imodSC, &isectSC, (DWORD *) &offSC, &isectCoffSC, (DWORD *) &cbSC, &dwCharacteristicsSC);
        } else {
            assert(!ppdbIn->FMinimal());
            pEnumSC->get(&imodSC, &isectSC, &offSC, &cbSC, &dwCharacteristicsSC);
        }

        // SC returned by QuerySecContrib has already been added
        // Avoid adding it again here
        rgpmodIn[imodSC]->QuerySecContrib(&isect, &off, &cbMod, &dwCharacteristics);

        if (isectSC != isect ||  offSC != off){
            if (!rgpmodOut[imodSC]->AddSecContrib(isectSC, offSC,
                    cbSC, dwCharacteristicsSC)) {
                SetLastError(ppdbOut);
                pEnumSC->release();
                return FALSE;
            }
        }
    }

    pEnumSC->release();

    return TRUE;
}


BOOL PDBCopy::CopySecMap()
{
    PB pb;
    CB cb;

    if (!pdbiIn->QuerySecMap(NULL, &cb)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    if (cb == 0) {
        // Nothing to copy

        return TRUE;
    }

    Buffer  bufSecMap;

    if (!bufSecMap.Reserve(cb, &pb)) {
        ecLast = EC_OUT_OF_MEMORY;
        return FALSE;
    }

    pdbiIn->QuerySecMap(pb, &cb);

    PB pbEnd = pb + cb;
    for (OMFSegMapDesc* pDesc =(OMFSegMapDesc*)(pb + sizeof (OMFSegMap));
        (PB) pDesc < pbEnd;
        pDesc++) {
            if (!pdbiOut->AddSec(pDesc->frame,
                pDesc->flags.fAll,  pDesc->offset, pDesc->cbSeg)) {
                SetLastError(ppdbOut);
                return FALSE;
            }
    }

    return TRUE;
}


BOOL PDBCopy::CopyPublics()
{
    GSI *pgsi;

    if (!pdbiIn->OpenPublics(&pgsi)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    // Set up to filter publics
    //
    PfnPDBCopyFilterPublics pfnFilter = NULL;
    if (pfnPdbCopyQueryCallback) {
        pfnFilter = PfnPDBCopyFilterPublics((*pfnPdbCopyQueryCallback)(pvClientContext, pccFilterPublics));
    }


    PB pbSym = 0;
    Buffer  bufNameWide;    // wide version of current name
    Buffer  bufNameNew;     // mbcs/utf8 version of possible new name

    if (pfnFilter && (!bufNameWide.Reserve(8*1024) || !bufNameNew.Reserve(4*1024))) {
        ecLast = EC_OUT_OF_MEMORY;
        pgsi->Close();
        return FALSE;
    }
    wchar_t *   wszOrig = (wchar_t *)bufNameWide.Start();

    while (pbSym = GSINextSym(pgsi, pbSym)) {
        PSYM pSym;
        PUBSYM32 *pPubSym;
        pSym = (PSYM)pbSym;

        switch (pSym->rectyp) {
        case S_PUB32:
            {
                pPubSym = (PUBSYM32 *) pSym;
                bool    fCopyPublic = true;
                SZ      szCurName = SZ(pPubSym->name);

                if (pfnFilter) {
                    // Convert names to wchar_t for filter call
                    CB cch = CB(UnicodeLengthOfUTF8(szCurName));
                    if (CB(cbUnicodeFromCch(cch)) > bufNameWide.Size()) {
                        if (!bufNameWide.Reserve(cbUnicodeFromCch(cch) - bufNameWide.Size())) {
                            ecLast = EC_OUT_OF_MEMORY;
                            pgsi->Close();
                            return FALSE;
                        }
                        wszOrig = (wchar_t *)bufNameWide.Start();
                    }
                    _GetSZUnicodeFromSZUTF8(SZ_CONST(szCurName), wszOrig, cch);

                    wchar_t     wszNew[4096] = { L'\0' };

                    // A filter return of true means copy symbol with either
                    // current name or supplied new name.
                    // A filter return of false means discard it.
                    //
                    if (pfnFilter(
                            pvClientContext, 
                            dwCopyFilter, 
                            pPubSym->off, 
                            pPubSym->seg, 
                            pPubSym->pubsymflags.grfFlags,
                            wszOrig,
                            wszNew,
                            _countof(wszNew)))
                    {
                        if (wszNew[0] != L'\0') {
                            // Use a new name supplied by filter function

                            CB cb = CB(UTF8LengthOfUnicode(wszNew));
                            if (cb > bufNameNew.Size()) {
                                if (!bufNameNew.Reserve(cb - bufNameNew.Size())) {
                                    ecLast = EC_OUT_OF_MEMORY;
                                    pgsi->Close();
                                    return FALSE;
                                }
                            }
                            szCurName = SZ(bufNameNew.Start());
                            _GetSZUTF8FromSZUnicode(wszNew, szCurName, cb);
                        }
                    }
                    else {
                        fCopyPublic = false;
                    }
                }

                if (fCopyPublic) {
                    if (!pdbiOut->AddPublic2(SZ_CONST(szCurName), pPubSym->seg, pPubSym->off, pPubSym->pubsymflags.grfFlags)) {
                        SetLastError(ppdbOut);
                        pgsi->Close();
                        return FALSE;
                    }
                }
                break;
            }
        default:
            assert(0);
        }
    }

    pgsi->Close();
    return TRUE;
}


BOOL PDBCopy::CopyDbg()
{
    dassert(dwCopyFilter & (copyRemovePrivate | copyCustomModSyms));

    // Copy all Dbg types, except debug fixups
    // (debug fixups are "private")

    return (CopyDbgtype(dbgtypeFPO) &&
        CopyDbgtype(dbgtypeTokenRidMap) &&
        CopyDbgtype(dbgtypeXdata) &&
        CopyDbgtype(dbgtypePdata) &&
        CopyDbgtype(dbgtypeNewFPO) &&
        CopyDbgtype(dbgtypeSectionHdrOrig) &&
        CopyDbgtype(dbgtypeException) &&
        CopyDbgtype(dbgtypeOmapToSrc) &&
        CopyDbgtype(dbgtypeOmapFromSrc) &&
        CopyDbgtype(dbgtypeSectionHdr));
}


BOOL PDBCopy::CopyDbgtype(DBGTYPE dbgtype)
{
    Dbg *pdbgIn = NULL;
    Dbg *pdbgOut =  NULL;
    if (!pdbiIn->OpenDbg(dbgtype, &pdbgIn)) {
        // dbgtype nonexistent, no need to copy anything
        return TRUE;
    }
    if (!pdbiOut->OpenDbg(dbgtype, &pdbgOut)) {
        pdbgIn->Close();
        SetLastError(ppdbOut);
        return FALSE;
    }

    long cElements = pdbgIn->QuerySize();
    Buffer buf(0, 0, FALSE);        // no padding necessary
    BOOL fRet = TRUE;

    if (cElements > 0) {
        if (!buf.Reserve(cElements * cbDbgtype (dbgtype))) {
            ecLast = EC_OUT_OF_MEMORY;
            fRet = FALSE;
        }
        else if (!pdbgIn->QueryNext(cElements, buf.Start())) {
            SetLastError(ppdbIn);
            fRet = FALSE;
        }
        else {
            if (dbgtype == dbgtypeNewFPO) {
                // Copy the program strings from in to out as well.

                NameMap *   pnmpIn = NULL;
                NameMap *   pnmpOut = NULL;

                if (!NameMap::open(ppdbIn, FALSE, &pnmpIn)) {
                    SetLastError(ppdbIn);
                    fRet = FALSE;
                }
                else if (!NameMap::open(ppdbOut, TRUE, &pnmpOut)) {
                    SetLastError(ppdbOut);
                    fRet = FALSE;
                }
                else {
                    FRAMEDATA * pfd = reinterpret_cast<FRAMEDATA*>(buf.Start());
                    FRAMEDATA * pfdMax = pfd + cElements;

                    for ( ; fRet && pfd < pfdMax; pfd++ ) {
                        const char *szPrg;
                        // Note that we are using a "feature" of 7.0+ namemaps
                        // of returning pointers to the utf-8 encoded text in the
                        // existing namemap 
                        fRet = pnmpIn->getName(pfd->frameFunc, &szPrg) &&
                            pnmpOut->getNiUTF8(szPrg, &pfd->frameFunc);
                    }
                }

                if (pnmpIn) pnmpIn->close();

                if (pnmpOut) pnmpOut->close();
            }

            if (fRet && !pdbgOut->Append(cElements, buf.Start())) {
                SetLastError(ppdbOut);
                fRet = FALSE;
            }
        }
    }

    pdbgIn->Close();
    pdbgOut->Close();
    return fRet;
}


BOOL PDBCopy::ProcessModInMiniPDB(Mod *pmodIn, Mod *pmodOut)
{
    // Callbacks to client for private symbols and types

    assert(pfnPdbCopyQueryCallback != NULL);

    static PfnPDBCopyFilterModTypes pfnFilterT = NULL;
    static PfnPDBCopyFilterCustomModSyms pfnFilterS = NULL;
    static PfnPDBCopyReportMissingPDB pfnReportMissingPDB = NULL;

    if (pfnFilterT == NULL) {
        pfnFilterT = PfnPDBCopyFilterModTypes(
                (*pfnPdbCopyQueryCallback)(pvClientContext, pccFilterModTypes));

        assert(pfnFilterT != NULL);

        if (pfnFilterT == NULL) {
            return FALSE;
        }
    }

    if (pfnFilterS == NULL) {
        pfnFilterS = PfnPDBCopyFilterCustomModSyms(
                (*pfnPdbCopyQueryCallback)(pvClientContext, pccFilterCustomModSyms));

        assert(pfnFilterS != NULL);

        if (pfnFilterS == NULL) {
            return FALSE;
        }
    }

    if (pfnReportMissingPDB == NULL) {
        pfnReportMissingPDB = PfnPDBCopyReportMissingPDB(
                (*pfnPdbCopyQueryCallback)(pvClientContext, pccReportMissingPDB));

        assert(pfnReportMissingPDB != NULL);

        if (pfnReportMissingPDB == NULL) {
            return FALSE;
        }
    }

    // First register compiler generated type records

    IMOD imod;

    if (!pmodIn->QueryImod(&imod)) {
        return FALSE;
    }

    PB pbTyp;
    DWORD cbTyp;

    if (!pfnFilterT(pvClientContext, imod, &cbTyp, &pbTyp)) {
        SetLastError(ppdbIn);
        return FALSE;
    }

    if ((cbTyp != 0) && !pmodOut->AddTypes(pbTyp, cbTyp)) {
        SetLastError(ppdbOut);

        if (ecLast == EC_NOT_FOUND) {
            SIG70  sig70;
            AGE    age;
            PTYPE  ptype = (PTYPE) (pbTyp + sizeof(DWORD));

            wchar_t  szTmp[_MAX_PATH];
            wchar_t  szFullMapped[_MAX_PATH];
            const wchar_t *szTS = ((DBI1 *) pdbiOut)->szGetTsInfo(ptype, szTmp, szFullMapped, &sig70, &age);

            pfnReportMissingPDB(pvClientContext, szTS);

            // If this callback returns, that means the client has ignored the
            // missing compile-time PDB issue and moved on.  So we just proceed
            // as if all went well here.

            ecLast = EC_OK;
            return TRUE;
        }

        return FALSE;
    }

    // Next add private symbols from COFF sections that actually contribute to PE

    if (!BuildImodToIsectCoffMap()) {
        return FALSE;
    }

    Array<DWORD> *prgIsectCoff;
    Map<BYTE *, DWORD, HcLPtr> mpEmitted;

    if (!mpImodToRgIsectCoff.map(imod, &prgIsectCoff)) {
        prgIsectCoff = new Array<DWORD>();

        if ((prgIsectCoff == NULL) ||
            !prgIsectCoff->append((DWORD) -1) ||
            !mpImodToRgIsectCoff.add(imod, prgIsectCoff)) {
            ecLast = EC_OUT_OF_MEMORY;
            return FALSE;
        }
    }

    for (DWORD i = 0; i < prgIsectCoff->size(); i++) {
        DWORD isectCoff = (*prgIsectCoff)[i];
        PB    pb;
        DWORD cb;

        if (!pfnFilterS(pvClientContext, imod, isectCoff, &cb, &pb)) {
            SetLastError(ppdbIn);
            return FALSE;
        }

        if (cb == 0) {
            continue;
        }

        assert(pb != NULL);

        DWORD dummy;

        if (mpEmitted.map(pb, &dummy)) {
            continue;
        }

        if (!mpEmitted.add(pb, 0)) {
            ecLast = EC_OUT_OF_MEMORY;
            return FALSE;
        }

        if (!CreateStringSubSection(DEBUG_S_SYMBOLS, pb, cb, pmodOut)) {
            return FALSE;
        }

        Buffer buf;
        DWORD sigC13 = CV_SIGNATURE_C13;
        DWORD type = DEBUG_S_SYMBOLS;
        
        if (!buf.Append((PB) &sigC13, sizeof(DWORD)) ||
            !buf.Append((PB) &type, sizeof(DWORD)) ||
            !buf.Append((PB) &cb, sizeof(DWORD)) ||
            !buf.Append(pb, cb) ||
            !buf.AppendFmt("f", dcbAlign(cb))) {
            ecLast = EC_OUT_OF_MEMORY;
            return FALSE;
        }

        if (!pmodOut->AddSymbols(buf.Start(), buf.Size())) {
            SetLastError(ppdbOut);
            return FALSE;
        }
    }

    // Last, copy over src and line info

    if (!CopySubsection(pmodIn, pmodOut, DEBUG_S_LINES) ||
        !CopySubsection(pmodIn, pmodOut, DEBUG_S_FILECHKSMS)) {
        return FALSE;
    }

    return TRUE;
}


BOOL PDBCopy::RegisterPDBMapping()
{
    if (fPdbMappingRegistered) {
        return TRUE;
    }

    PfnPDBCopyFilterPdbMappings pfnFilter = NULL;

    assert(pfnPdbCopyQueryCallback != NULL);

    pfnFilter = PfnPDBCopyFilterPdbMappings((*pfnPdbCopyQueryCallback)(pvClientContext, pccFilterPdbMappings));

    if (pfnFilter == NULL) {
        return FALSE;
    }

    DWORD cPairs;

    if (!pfnFilter(pvClientContext, &cPairs, NULL, NULL)) {
        return FALSE;
    }

    if (cPairs != 0) {
        wchar_t **pszFrom = new (zeroed) wchar_t* [cPairs];
        wchar_t **pszTo = new (zeroed) wchar_t* [cPairs];

        if (!pszFrom || !pszTo) {
            ecLast = EC_OUT_OF_MEMORY;
            return FALSE;
        }

        DWORD cPairsRead;

        if (!pfnFilter(pvClientContext, &cPairsRead, pszFrom, pszTo) ||
            (cPairs != cPairsRead)) {
            return FALSE;
        }

        for (DWORD i = 0; i < cPairs; i++) {
            if (!ppdbOut->RegisterPDBMapping(pszFrom[i], pszTo[i])) {
                return FALSE;
            }
        }

        delete [] pszFrom;
        delete [] pszTo;
    }

    fPdbMappingRegistered = true;
    return TRUE;
}


BOOL PDB1::CopyToW2(
    const wchar_t *         wszPdbOut,
    DWORD                   dwCopyFilter,
    PfnPDBCopyQueryCallback pfnCallBack,
    void *                  pvClientContext
    )
{
    bool fRemovePrivate = (dwCopyFilter & copyRemovePrivate) != 0;
    bool fCreateNewSig = (dwCopyFilter & copyCreateNewSig) != 0;
    bool fRemoveNamedStream = (dwCopyFilter & copyRemoveNamedStream) != 0;
    bool fCustomModSyms = (dwCopyFilter & copyCustomModSyms) != 0;

    BOOL fRet = TRUE;

#ifdef PDB_MT
    // make sure the source PDB is exclusive write, or read only (aka non-write-shared)
    if (m_fWriteShared) {
        setAccessError();
        return FALSE;
    }
#endif

    if (!fRemovePrivate && !fCustomModSyms) {
        // No filter, copy entire file

#if defined(_CORESYS) || defined(_KERNELX)
        if (!::CopyFileExW(wszPDBName, wszPdbOut, NULL, NULL, NULL, 0)) {
#else
        typedef HRESULT (__stdcall *PfnCopyFileExW)(const wchar_t *, const wchar_t *, LPPROGRESS_ROUTINE, void *, bool *, DWORD);

        PfnCopyFileExW pfn = NULL;

        HMODULE hlib = LoadLibraryExW(L"kernel32.dll", NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

        if (hlib == NULL) {
            hlib = LoadLibraryExW(L"api-ms-win-core-file-l2-1-1.dll", NULL,  LOAD_WITH_ALTERED_SEARCH_PATH);
        }

        if (hlib == NULL) {
            setLastError(EC_UNKNOWN, wszPdbOut);
            return FALSE;
        }

        *(FARPROC *) &pfn = GetProcAddress(hlib, "CopyFileExW");

        if (pfn == 0) {
            setLastError(EC_UNKNOWN, wszPdbOut);
            return FALSE;
        }

        if (!(*pfn)(wszPDBName, wszPdbOut, NULL, NULL, NULL, 0)) {
#endif
            // CopyFile has failed -- provide generic error information

            setLastError(EC_FILE_SYSTEM, wszPdbOut);
            return FALSE;
        }

        // See if we need to unset the R/O bit

        DWORD dwAttrib = ::GetFileAttributesW(wszPdbOut);

        if (dwAttrib & FILE_ATTRIBUTE_READONLY) {
            ::SetFileAttributesW(wszPdbOut, dwAttrib & ~FILE_ATTRIBUTE_READONLY);
        }

        if (fCreateNewSig) {
            SIG70 sig70 = { 0 };

            if (!FUuidCreate(&sig70)) {
                return FALSE;
            }

            if (!patchSigAndAge(wszPdbOut, (SIG) time(0), sig70, 1, 1, fRemovePrivate)) {
                return FALSE;
            }
        }
    }
    else {
        if (!fIsSZPDB()) {
            // Don't let people copypdb copy older PDBs.
    
            return FALSE;
        }
    
        DBI1 *pdbiIn = 0;
    
        if (!OpenDBI(NULL, pdbRead, (DBI**) &pdbiIn)) {
            return FALSE;
        }
    
        // Create a new PDB and add all the non-private
        // information from the source pdb
    
        EC ecT;
        wchar_t wszErrT[cbErrMax];
        PDB *ppdbOut = NULL;
    
        // open the pdb exclusively.  no MT problem.  page size 4K.
        fRet = OpenEx2W(wszPdbOut, pdbFullBuild pdbWrite pdbExclusive, (SIG) 0, ::cbPgPdbDef * 4, &ecT, wszErrT, cbErrMax, &ppdbOut);
    
        DBI *pdbiOut = NULL;
        TPI *ptpiOut = NULL;
        TPI *pipiOut = NULL;
    
        if (fRet) {
            if ((!fCustomModSyms || (ppdbOut->OpenTpi(pdbWrite, &ptpiOut) &&
                                     ppdbOut->OpenIpi(pdbWrite, &pipiOut))) &&
                 ppdbOut->CreateDBI(NULL, &pdbiOut)) {
                // When to replace per module private symbols, we need to open type and ID pools.

                PDBCopy pdbCopy(this, ppdbOut, pdbiIn, pdbiOut, dwCopyFilter, pfnCallBack, pvClientContext);
    
                if (!pdbCopy.DoCopy()) {
                    wchar_t wszErrLast[cbErrMax];
                    EC ecLast = pdbCopy.QueryLastErrorExW(wszErrLast, cbErrMax);

                    setLastError(ecLast, wszErrLast);
                    fRet = FALSE;
                }
            }
            else {
                // Failed to create DBI.  Copy error information from ppdbOut.

                wchar_t wszErrLast[cbErrMax];
                EC ecLast = ppdbOut->QueryLastErrorExW(wszErrLast, cbErrMax);

                setLastError(ecLast, wszErrLast);
                fRet = FALSE;
            }
        }
        else {
            setLastError(ecT, wszErrT);
            fRet = FALSE;
        }
    
        AGE ageDbi = pdbiIn->QueryAge();
    
        pdbiIn->Close();

        if (fCustomModSyms) {
            assert(pipiOut != NULL);
            assert(ptpiOut != NULL);

            fRet &= pipiOut->Close();
            fRet &= ptpiOut->Close();
        }

        if (pdbiOut != NULL) {
            fRet &= pdbiOut->Close();
        }
    
        if (ppdbOut != NULL) {
            fRet &= ppdbOut->Commit();
            fRet &= ppdbOut->Close();
        }
    
        if (fRet) {
            if (!fCreateNewSig) {
                SIG     sig = QuerySignature();
                AGE     age = QueryAge();
                SIG70   sig70 = {0};
    
                QuerySignature2(&sig70);
    
                fRet &= patchSigAndAge(wszPdbOut, sig, sig70, age, ageDbi, fRemovePrivate);
            }
        }
    }

    if (fRet && fRemoveNamedStream) {
        fRet = eraseNamedStream(wszPdbOut, pfnCallBack, pvClientContext);
    }

    return fRet;
}


BOOL PDB1::patchSigAndAge(const wchar_t *wszPDB, SIG sig, const SIG70 & sig70, AGE age, AGE ageDbi, bool fStripped)
{

    EC ec;
    wchar_t wszErr[cbErrMax];
    PDB *ppdb;

    // open the pdb exclusively.  no MT problem
    if (!OpenEx2W(wszPDB, pdbWrite pdbExclusive, (SIG) 0, ::cbPgPdbDef, &ec, wszErr, cbErrMax, &ppdb)) {
        return FALSE;
    }

    PDB1 *ppdb1;

#ifdef _CPPRTTI
    ppdb1 = dynamic_cast<PDB1 *> (ppdb);
    if (!ppdb1) {
        ppdb->Close();
        return FALSE;
    }
#else
    ppdb1 = (PDB1 *) ppdb;
#endif

    ppdb1->pdbStream.sig = sig;
    ppdb1->pdbStream.age = age;
    ppdb1->pdbStream.sig70 = sig70;

    // Patch the new dbi age into the snDbi stream, update fStripped
    //
    MSF *pmsfOut = ppdb1->pmsf;

    NewDBIHdr dbihdr;

    if (!pmsfOut ||
        !pmsfOut->ReadStream(snDbi, &dbihdr, sizeof(dbihdr)) ||
        (dbihdr.hdrVersion != DBIImpv)) {
        ppdb->Close();
        return FALSE;
    }

    dbihdr.age = ageDbi;
    dbihdr.flags.fStripped = fStripped;

    if (!pmsfOut->WriteStream(snDbi, 0, &dbihdr, sizeof(dbihdr)) ||
        !ppdb->Commit()) {
        ppdb->Close();
        return FALSE;
    }

    return ppdb->Close();
}


BOOL PDB1::RegisterPDBMapping(const wchar_t *wszPDBFrom, const wchar_t *wszPDBTo)
{
    struct PDBMAPPING t;

    t.szFrom = wszCopy(wszPDBFrom);
    t.szTo   = wszCopy(wszPDBTo);

    if (!t.szFrom || !t.szTo || !m_rgPDBMapping.append(t)) {
        setOOMError();
        return FALSE;
    }

    return TRUE;
}


// This function is only used by PDBCOPY to erase specified named stream from a PDB.

BOOL PDB1::eraseNamedStream(const wchar_t *wszPdb,
                            PfnPDBCopyQueryCallback pfnCallBack,
                            void *pvContext)
{
    // Set up to filter stream names

    PfnPDBCopyFilterStreamNames pfnFilter = NULL;

    if (pfnCallBack) {
        pfnFilter = PfnPDBCopyFilterStreamNames(
                (*pfnCallBack)(pvContext, pccFilterStreamNames));
    }

    assert(pfnFilter != NULL);
    if (pfnFilter == NULL) {
        return FALSE;
    }

    EC ec;
    wchar_t wszErr[cbErrMax];
    PDB *ppdb;

    // Open the pdb exclusively, no MT problem.

    if (!OpenEx2W(wszPdb, pdbWrite pdbExclusive, (SIG) 0, ::cbPgPdbDef, &ec, wszErr, cbErrMax, &ppdb)) {
        return FALSE;
    }

    PDB1 *ppdb1;

#ifdef _CPPRTTI
    ppdb1 = dynamic_cast<PDB1 *> (ppdb);
    if (!ppdb1) {
        ppdb->Close();
        return FALSE;
    }
#else
    ppdb1 = (PDB1 *) ppdb;
#endif

    // Enumerate name map, processing each named stream.

    EnumNameMap *penum;

    if (!ppdb1->GetEnumStreamNameMap((Enum **) &penum)) {
        return FALSE;
    }

    BOOL fRet = TRUE;

    while (penum->next()) {
        const char *sz;
        NI ni;

        penum->get(&sz, &ni);

        if (ppdb1->pmsf->GetCbStream((SN)ni) == cbNil) {
            continue;
        }

        assert(sz != NULL);

        // Convert names to wchar_t for filter call

        Buffer bufWideStr;
        size_t cch = UnicodeLengthOfUTF8(sz);

        if (!bufWideStr.Reserve(static_cast<CB>(cbUnicodeFromCch(cch)))) {
            return FALSE;
        }

        wchar_t *wsz = (wchar_t *)bufWideStr.Start();
        _GetSZUnicodeFromSZUTF8(SZ_CONST(sz), wsz, cch);

        if (pfnFilter(pvContext, wsz)) {
            fRet = ppdb1->pmsf->NonTransactionalEraseStream((SN)ni);
            fRet &= ppdb1->RemoveStreamName(ni);
            break;
        }
    }

    penum->release();

    if (!fRet || !ppdb->Commit()) {
        ppdb->Close();
        return FALSE;
    }

    return ppdb->Close();
}

wchar_t *PDB1::QueryPDBMapping(const wchar_t *wszPDBFrom)
{
    for (size_t i = 0; i < m_rgPDBMapping.size(); i++) {
        if (!_wcsicmp(m_rgPDBMapping[i].szFrom, wszPDBFrom)) {
            return m_rgPDBMapping[i].szTo;
        }
    }

    return NULL;
}

BOOL PDB1::RemoveStreamName(NI ni)
{
    return nmt.deleteNiSzo(ni);
}

BOOL PDB1::EnablePrefetching()
{
    if (m_fRead) {
        m_fPrefetching = true;
        return TRUE;
    }
    return FALSE;
}

BOOL PDB1::FLazy()
{
    if (m_fNoTypeMerge) {
        return TRUE;
    }

    return FALSE;
}

BOOL PDB1::FMinimal()
{
    if (m_fMinimalDbgInfo) {
        return TRUE;
    }

    return FALSE;
}

// Implementing a timeout to allow holding PDBs open
// for a while, even after they are closed ...

BOOL PDB::SetPDBCloseTimeout(DWORDLONG t) 
{
    return PDB1::SetPDBCloseTimeout(t);
}

BOOL PDB::CloseAllTimeoutPDB() 
{
    return PDB1::CloseAllTimeoutPDB();
}

BOOL PDB::RPC()
{
    return PDB1::RPC();
}

BOOL PDB::ShutDownTimeoutManager()
{
    return PDB1::ShutDownTimeoutManager();
}

BOOL PDB1::RPC()
{
    return FALSE;
}

#ifndef PDB_MT

BOOL PDB1::SetPDBCloseTimeout(DWORDLONG t) 
{
    return FALSE;
}
BOOL PDB1::CloseAllTimeoutPDB() 
{
    return FALSE;
}
BOOL PDB1::ShutDownTimeoutManager() 
{
    return FALSE;
}

#else

BOOL PDB1::SetPDBCloseTimeout(DWORDLONG t) 
{
    return s_pdbTimeoutManager.SetTimeout(t);
}
BOOL PDB1::CloseAllTimeoutPDB() 
{
    return s_pdbTimeoutManager.CloseAllTimeoutPDB();
}
BOOL PDB1::ShutDownTimeoutManager()
{
    return s_pdbTimeoutManager.ShutDown();
}

PDB1::PDBTimeoutManager PDB1::s_pdbTimeoutManager;

BOOL PDB1::PDBTimeoutManager::NotifyReopen(PDB1 * ppdb1) 
{
    MTS_ASSERT(PDB1::s_csForPDBOpen);
    if (ppdb1->dwPDBOpenedCount != 0 || !m_mpPDBTimeout.contains(ppdb1)) {
        return FALSE;
    }
    m_mpPDBTimeout.remove(ppdb1);
    ppdb1->internal_Close();
    return TRUE;
}

void PDB1::PDBTimeoutManager::NotifyReuse(PDB1 * ppdb1) 
{
    MTS_ASSERT(PDB1::s_csForPDBOpen);
    if (ppdb1->dwPDBOpenedCount != 0) {
        assert(!m_mpPDBTimeout.contains(ppdb1));
        return;
    }
    assert(m_mpPDBTimeout.contains(ppdb1));
    m_mpPDBTimeout.remove(ppdb1);
    return;
}

BOOL PDB1::PDBTimeoutManager::NotifyClose(PDB1 * ppdb1) 
{
    MTS_ASSERT(PDB1::s_csForPDBOpen);
    if (m_timeout == 0) {
        return ppdb1->internal_Close();
    }
    assert(!m_mpPDBTimeout.contains(ppdb1));
    assert(m_hNotEmpty != NULL);
    if (!SetEvent(m_hNotEmpty)) {
        return ppdb1->internal_Close();
    }
    ULONGLONG ulCurrentTime;
    GetSystemTimeAsFileTime((FILETIME *)&ulCurrentTime);
    if (!m_mpPDBTimeout.add(ppdb1, ulCurrentTime)) {
        return ppdb1->internal_Close();
    }
    return TRUE;
}

BOOL PDB1::PDBTimeoutManager::SetTimeout(ULONGLONG t) 
{
    MTS_PROTECT(PDB1::s_csForPDBOpen);
    m_timeout = t;

    if (m_timeout != 0 && m_hThread == NULL) {
        m_hNotEmpty = CreateEventW(NULL, TRUE, FALSE, NULL);
        if (m_hNotEmpty == NULL) {
            m_timeout = 0;
            return FALSE;
        }
        m_hThread = CreateThread(NULL, 0, &PDBTimeoutManager::CleanupThreadStart, this, 0, NULL);
        if (m_hThread == NULL) {
            m_timeout = 0;
            CloseHandle(m_hNotEmpty);
            m_hNotEmpty = NULL;
            return FALSE;
        }
    }
    return TRUE;
}

DWORD PDB1::PDBTimeoutManager::CleanupThreadStart(LPVOID p)
{
    PDBTimeoutManager *pThis = reinterpret_cast<PDBTimeoutManager *>(p);
    return pThis->CleanupThread();
}

DWORD PDB1::PDBTimeoutManager::CleanupThread()
{
    ULONGLONG ulPeriod = (m_timeout + 1) / 2;

    HANDLE hTimer = CreateWaitableTimerW(NULL, TRUE, NULL);

    if (hTimer == NULL) {
        return GetLastError();
    }
    
    while (WaitForSingleObject(m_hNotEmpty, INFINITE) == WAIT_OBJECT_0) {
        ULONGLONG ulCurrentTime;
        GetSystemTimeAsFileTime((FILETIME *)&ulCurrentTime);
        ULONGLONG ulTimer = ulCurrentTime + ulPeriod;

        if (!SetWaitableTimer(hTimer, (LARGE_INTEGER *)&ulTimer, 0, NULL, NULL, FALSE)) {
            continue;        
        }
        if (WaitForSingleObject(hTimer, INFINITE) == WAIT_FAILED) {
            continue;
        }

        MTS_PROTECT(PDB1::s_csForPDBOpen);
        CloseTimedOutPDBs();
    
        if (m_mpPDBTimeout.count() == 0) {
            ResetEvent(m_hNotEmpty);
        }
    }

    DWORD err = GetLastError();
    CloseHandle(hTimer);
    return err;
}

BOOL PDB1::PDBTimeoutManager::CloseTimedOutPDBs(bool fFlush)
{
    MTS_ASSERT(PDB1::s_csForPDBOpen);

    ULONGLONG ulCurrentTime;
    GetSystemTimeAsFileTime((FILETIME *)&ulCurrentTime);

    EnumMap<PDB1 *, ULONGLONG, HcLPtr > e(m_mpPDBTimeout);
    while (e.next()) {
        PDB1 * ppdb1;
        ULONGLONG ulCloseTime;
        e.get(&ppdb1, &ulCloseTime);
        if (fFlush || (ulCurrentTime - ulCloseTime > m_timeout)) {
            m_mpPDBTimeout.remove(ppdb1);
            ppdb1->internal_Close();
        }
    }
    return TRUE;
}

BOOL PDB1::PDBTimeoutManager::CloseAllTimeoutPDB() {
    MTS_PROTECT(PDB1::s_csForPDBOpen);
    return CloseTimedOutPDBs(true);
}

BOOL PDB1::PDBTimeoutManager::ShutDown( ) 
{
    MTS_PROTECT(PDB1::s_csForPDBOpen);

    // Terminate the timeout thread if it is still alive

    if (m_hThread != NULL) {
        if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT) {
            PREFAST_SUPPRESS(6258, "This is no biggie")
            TerminateThread( m_hThread, 1 );
        }
        CloseHandle(m_hThread);
    }

    CloseHandle( m_hNotEmpty );

    m_hThread = NULL;
    m_hNotEmpty = NULL;
    m_timeout = 0;

    return CloseTimedOutPDBs(true);
}

#endif // PDB_MT
