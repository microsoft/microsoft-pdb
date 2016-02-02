//////////////////////////////////////////////////////////////////////////////
// PDB implementation declarations

struct PDBStream {
    IMPV    impv;
    SIG     sig;
    AGE     age;
    PDBStream() : impv(0), sig(0), age(0){}
};

inline SIG70 Sig70FromSig(SIG dwSig)
{
    PDBLOG_FUNC();
    SIG70 sig70;
    memset(&sig70, 0, sizeof(SIG70));
    sig70.Data1 = dwSig;

    return sig70;
}

static inline BOOL IsRealSig70(const SIG70& sig70)
{
    PDBLOG_FUNC();
    static SIG70 sig70Null;                     // Must be all zeros
    const int size = sizeof SIG70 - sizeof(sig70.Data1);
    return memcmp(&sig70.Data2, &sig70Null.Data2, size) != 0;
}

inline SIG SigFromSig70(const SIG70& sig70)
{
    PDBLOG_FUNC();
    assert(!IsRealSig70(sig70));
    return sig70.Data1;
}

inline BOOL IsEqualSig(const SIG70& sig70, PDB *ppdb)
{
    PDBLOG_FUNC();
    SIG70 sigPdb;
    // LF_TYPESERVER records do not have real GUIDs
    if(!IsRealSig70(sig70)) {
        return sig70.Data1 == ppdb->QuerySignature();
    }
    else if (ppdb->QuerySignature2(&sigPdb)) {
        return IsEqualGUID(sig70, sigPdb);
    }
    return FALSE;
}

struct PDBStream70 : public PDBStream {
    SIG70   sig70;

    PDBStream70() { memset(&sig70, 0, sizeof(SIG70)); }

    void operator =(const PDBStream & _pdbstream) {
        *(PDBStream*)this = _pdbstream;
    }
};


class PDB1 : public PDBCommon { // PDB (program database) implementation
public:


    static BOOL
           OpenEx2W(
               const wchar_t *wszPDB,
               const char *szMode,
               SIG sigInitial,
               long cbPage,
               OUT EC *pec,
               _Out_opt_cap_(cchErrMax) OUT wchar_t *wszError,
               size_t cchErrMax,
               OUT PDB **pppdb);

    static BOOL
           OpenValidate4(
               const wchar_t *wszPDB,
               const char *szMode,
               PCSIG70  pcsig70,
               SIG sig,
               AGE age,
               OUT EC *pec,
               _Out_opt_cap_(cchErrMax) OUT wchar_t *wszError,
               size_t cchErrMax,
               OUT PDB **pppdb);

    static BOOL
           OpenNgenPdb(
               const wchar_t *wszNgenImage,
               const wchar_t *swzPdbPath,
               OUT EC *pec,
               _Out_opt_cap_(cchErrMax) OUT wchar_t *wszError,
               size_t cchErrMax,
               OUT PDB **pppdb);

    static BOOL 
            OpenTSPdb(
               const wchar_t *szPDB,
               const wchar_t *szPath,
               const char *szMode,
               const SIG70& sig,
               AGE age,
               OUT EC *pec,
               _Out_opt_cap_(cchErrMax) OUT wchar_t *szError,
               size_t cchErrMax,
               OUT PDB **pppdb);
    static BOOL
           OpenInStream(
               IStream *pIStream,
               const char *szMode,
               CB cbPage,
               OUT EC *pec,
               _Out_opt_cap_(cchErrMax) OUT wchar_t *wszError,
               size_t cchErrMax,
               OUT PDB **pppdb);

    static BOOL SetPDBCloseTimeout(DWORDLONG t);
    static BOOL CloseAllTimeoutPDB();
    static BOOL ShutDownTimeoutManager();
    static BOOL RPC();

    INTV QueryInterfaceVersion();
    IMPV QueryImplementationVersion();
    IMPV QueryPdbImplementationVersion();

    SZ   QueryPDBName(_Out_z_cap_(_MAX_PATH) OUT char szPDBName[_MAX_PATH]);
    SIG  QuerySignature();
    AGE  QueryAge();
    BOOL GetEnumStreamNameMap(OUT Enum** ppenum);
    BOOL CreateDBI(SZ_CONST szTarget, OUT DBI** ppdbi);
    BOOL OpenDBI(SZ_CONST szTarget, SZ_CONST szMode, OUT DBI** ppdbi);
    BOOL OpenTpi(SZ_CONST szMode, TPI** pptpi);
    BOOL OpenIpi(SZ_CONST szMode, TPI** ppipi);
    BOOL Commit();
    BOOL Close();
    BOOL GetRawBytes(PFNfReadPDBRawBytes fSnarfRawBytes);
    EC QueryLastErrorExW(__out_ecount_opt(cchMax) wchar_t *wszError, size_t cchMax);
    BOOL QuerySignature2(PSIG70 psig70);
    BOOL ResetGUID(BYTE *pb, DWORD cb);

    BOOL CopyToW2(
        const wchar_t *         szDst,
        DWORD                   dwCopyFilter,
        PfnPDBCopyQueryCallback pfnCallBack,
        void *                  pvClientContext
        );

    wchar_t *QueryPDBNameExW(__out_ecount_opt(cchMax) wchar_t *wszPDB, size_t cchMax);

    BOOL OpenDBIEx(
        SZ_CONST                szTarget,
        SZ_CONST                szMode,
        OUT DBI**               ppdbi,
        PfnFindDebugInfoFile    pfn=0
        );

    BOOL OpenSrc(OUT Src** ppsrc);
    void CopySrc( PDB* ppdbFrom );


    // set errors
    void setLastError(EC ec, const char *szErr);
    void setLastError(EC ec, const wchar_t *wszErr = NULL);

    // various convenient error stuff
    void setWriteError();
    void setReadError();
    void setOOMError();
    void setUsageError();
    void setAccessError();
    void setCorruptError();
    void setCorruptTypePoolError(PDB * pdb);

    BOOL OpenStreamEx(const char * szStream, const char * szMode, Stream ** ppstream);

    BOOL RegisterPDBMapping(const wchar_t *szPDBFrom, const wchar_t *szPDBTo);
    wchar_t *QueryPDBMapping(const wchar_t *szPDBFrom);

    BOOL RemoveStreamName(NI ni);

    bool FCheckTypeMismatch() const
    {
        return m_fPdbTypeMismatch;
    }

    BOOL EnablePrefetching();
    BOOL FLazy();
    BOOL FMinimal();

protected:
#pragma warning(disable:4355)
    PDB1(MSF *pmsf_, const wchar_t *wszPDB) : 
        pmsf(pmsf_), ptpi1(0),  pipi(0), pdbi1(0),  m_pnamemap(0),
        nmt(niForNextFreeSn, this), dwNameMapOpenedCount(0), pPDBError(NULL)
#ifdef PDB_MT
        , psrcimpl(0), dwPDBOpenedCount(1), dwTPIOpenedCount(0)
        , dwIPIOpenedCount(0), dwDBIOpenedCount(0), dwSrcImplOpenedCount(0)
#endif

    {
        PDB_wfullpath(wszPDBName, wszPDB, _MAX_PATH);
        m_fNewNameMap = m_fPdbCTypes = m_fPdbTypeMismatch = false;
        m_fContainIDStream = m_fNoTypeMerge = m_fMinimalDbgInfo = false;
        m_fVC120 = m_fPrefetching = false;
    }

private:

    // these variables are initialized on open. Protected by sz_csForPDBOpen
    MSF *       pmsf;  // MSF, may be 0 for a C8.0 PDB
    wchar_t     wszPDBName[_MAX_PATH];
    IPDBError * pPDBError;
    PDBStream70 pdbStream;

    // used in PDB1::savePdbStream(), PDB1::loadPdbStream(), PDB1::GetEnumStreamNameMap(), PDB1::OpenStream()
    NMTNI       nmt;

    // used for linker option "/pdbmap"
    struct PDBMAPPING {
        wchar_t *szFrom;
        wchar_t *szTo;
    };
    Array<struct PDBMAPPING> m_rgPDBMapping;

    // the various opened stuff the PDB is keeping track of
    NameMap *   m_pnamemap;
    TPI1 *      ptpi1;
    DBI1 *      pdbi1;
    TPI1 *      pipi;    // ID pool interface

    // these variables are initialized on open. Protected by sz_csForPDBOpen
    bool        m_fRead                 : 1;
    bool        m_fWriteShared          : 1;
    bool        m_fPdbCTypes            : 1;
    bool        m_fPdbTypeMismatch      : 1;
    bool        m_fNewNameMap           : 1;
    bool        m_fFullBuild            : 1;
    bool        m_fContainIDStream      : 1;
    bool        m_fNoTypeMerge          : 1;
    bool        m_fMinimalDbgInfo       : 1;
    bool        m_fVC120                : 1;

    // Flag to indicate whether to preload all type, public and
    // global records when TPI/GSI/PSGSI streams get initialized.
    bool        m_fPrefetching;

#ifdef PDB_MT
    class PDBTimeoutManager {
    public:
        PDBTimeoutManager() : m_hThread(NULL), m_hNotEmpty(NULL) {}
            
        BOOL NotifyReopen(PDB1 * ppdb1);
        void NotifyReuse(PDB1 * ppdb1);
        BOOL NotifyClose(PDB1 * ppdb1);

        BOOL NotifyReopen(TPI1 * ptpi1);
        void NotifyReuse(TPI1 * ptpi1);
        BOOL NotifyClose(TPI1 * ptpi1);

        BOOL SetTimeout(ULONGLONG t);
        BOOL CloseAllTimeoutPDB();
        BOOL ShutDown( );

    private:
        HANDLE    m_hThread;
        HANDLE    m_hNotEmpty;
        ULONGLONG m_timeout;                // Time in 100 ns units for which to hold 
                                            // PDB open even after it was closed ...

        Map<PDB1 *, ULONGLONG, HcLPtr > m_mpPDBTimeout;

        DWORD CleanupThread();
        BOOL  CloseTimedOutPDBs(bool fFlush = false);
        static DWORD WINAPI CleanupThreadStart(LPVOID);

    };
    friend class PDBTimeoutManager;

    static PDBTimeoutManager s_pdbTimeoutManager;

    // use to protect access to the Stream table for OpenStreamEx
    mutable Mutex m_mutexStreamOpenClose;	// Protects Stream Opens/Close
    Map<NI, Strm *, LHcNi> m_mpOpenedStream;

    // use to protect access to pdbStream
    mutable CriticalSection m_csPdbStream;

    // used in PDB1::OpenEx2W and PDB1::Close() - to keep track of opened PDBs
    static CriticalSection s_csForPDBOpen;
    static Map<MSF *, PDB1 *, HcLPtr > s_mpOpenedPDB;
    DWORD dwPDBOpenedCount;

    // used in PDB1::OpenTpi() and PDB1::internal_CloseTPI()
    mutable CriticalSection m_csForTPI;
    DWORD dwTPIOpenedCount;

    // used in PDB1::OpenIpi() and PDB1::internal_CloseTPI()
    mutable CriticalSection m_csForIPI;
    DWORD dwIPIOpenedCount;

    // used in PDB1::OpenDBIEx() and PDB1::internal_CloseDBI()
    mutable CriticalSection m_csForDBI;
    DWORD dwDBIOpenedCount;

    // used in PDB1::OpenSrc() and PDB1::internal_CloseSrc()
    mutable CriticalSection m_csForSrcImpl;
    DWORD dwSrcImplOpenedCount;
    Src *       psrcimpl;

    // used in NMP::open() and NMP::close()
    mutable CriticalSection m_csForNameMap;     

    USE_ASSURE_SINGLETHREAD();

#endif
    DWORD dwNameMapOpenedCount;

    BOOL internal_Close();          // used by PDB1::Close() - to do hard close  
    BOOL internal_CloseTPI(TPI1 *p); // used by TPI1::Close() - to do TPI/IPI opened count
    BOOL internal_CloseDBI();       // used by DBI1::Close() - to do DBI opened count
#ifdef PDB_MT
    BOOL internal_CloseSrc();       // used by SrcImpl::Close() - to do Src opened count
    static BOOL s_internal_CloseSrc(PDB * ppdb) {
        return ((PDB1 *)ppdb)->internal_CloseSrc();
    }
#endif

    // used by PDB1::OpenEx2W() and PDB1::OpenInStream()
    BOOL loadPdbStream(MSF *pmsf, const wchar_t *wszPDB, EC *pec, __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax);

    // used by PDB1::Commit()
    BOOL savePdbStream();   

    BOOL fEnsureSn(SN* psn);
    BOOL fEnsureNoSn(SN* psn);

    static BOOL patchSigAndAge(const wchar_t *wszPdb, SIG newSig, const SIG70 & newSig70, AGE newAge, AGE newDbiAge, bool fStripped);

    static BOOL niForNextFreeSn(void* pv, OUT NI* pni);

    static BOOL eraseNamedStream(const wchar_t *wszPdb, PfnPDBCopyQueryCallback pfnCallBack, void *pvContext);

    // bit vector for mapping between two error codes to get the best one to return
    //
    static const __int32    c_mpEcEcBest[];

    static inline bool FBetterError(EC ec1, EC ec2) {
        // return whether ec1 is better than ec2.
        // since the data it is based on is stable (i.e.
        // if ec1 == ec2, return false), you need to be
        // aware of the ordering of ec1 and ec2
        //
        assert(ec1 < EC_MAX);
        assert(ec2 < EC_MAX);
        return (c_mpEcEcBest[ec1] & (1 << ec2)) != 0;
    }

    friend DBI1;
    friend Mod1;
    friend GSI1;
    friend PSGSI1;
    friend TPI1;
    friend NameMap;
    friend class NMP;
    friend class Strm;

public:

    BOOL fIsSZPDB() const {
        MTS_PROTECT(m_csPdbStream);         // assume changes to impv only goes up, never goes down
        return pdbStream.impv > impvVC98;   // VC98 was the last ST pdb implementation
    }

    bool fIsPdb70() const {
        MTS_PROTECT(m_csPdbStream);             // assume changes to impv only goes up, never goes down
        return pdbStream.impv > impvVC70Dep;    // real 7.0 implementation, not the deprecated one!
    }

    bool fIsPdb110() const {
        return m_fContainIDStream;
    }
};

#pragma warning(default:4355)

inline void* operator new(size_t size, PDB1* ppdb1)
{
    PB pb = new BYTE[size];
    if (!pb)
        ppdb1->setOOMError();
    return pb;
}

inline void* operator new[](size_t size, PDB1* ppdb1)
{
    PB pb = new BYTE[size];
    if (!pb)
        ppdb1->setOOMError();
    return pb;
}
