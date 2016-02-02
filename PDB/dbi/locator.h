//////////////////////////////////////////////////////////////////////////////
// Locator implementation declarations

class LOCATOR
{
public:
    LOCATOR();
    ~LOCATOR();

    DWORD CbCv() const;
    EC Ec() const;
    DWORD FoCv() const;
    bool FCrackExeOrDbg(const wchar_t *, bool = false);
    bool FGetNgenPdbInfo(const wchar_t *, _Deref_out_opt_ wchar_t **, GUID **, AGE *);
    bool FLocateDbg(const wchar_t *);
    bool FLocatePdb(const wchar_t *);
    PDB *Ppdb() const;
    void SetPfnQueryCallback(PfnPDBQueryCallback);
    void SetPvClient(void *);
    const wchar_t *WszDbgPath() const;
    const wchar_t *WszError() const;

private:
    struct NB10I                       // NB10 debug info
    {
        DWORD   dwSig;                 // NB10
        DWORD   dwOffset;              // offset, always 0
        SIG     sig;
        AGE     age;
        char    szPdb[_MAX_PATH];
    };

    struct RSDSI                       // RSDS debug info
    {
        DWORD   dwSig;                 // RSDS
        GUID    guidSig;
        DWORD   age;
        char    szPdb[_MAX_PATH * 3];
    };

    union CV
    {
        DWORD   dwSig;
        NB10I   nb10i;
        RSDSI   rsdsi;
    };

    typedef BOOL (__stdcall *PFNSYMBOLSERVERW)(const wchar_t *, const wchar_t *, const void *, DWORD, DWORD, wchar_t *);
    typedef BOOL (__stdcall *PFNSYMBOLSERVERSETOPTIONS)(UINT_PTR, DWORDLONG);
    typedef BOOL (__stdcall *PFNSYMBOLSERVERSTOREFILEW)(const wchar_t *, const wchar_t *, void *, DWORD, DWORD, wchar_t *, size_t, DWORD);

    class REGISTRY
    {
public:
        REGISTRY(HKEY);
        ~REGISTRY();

        const wchar_t *WszSearchPath();

private:
        void Init();

        HKEY         m_hkey;
        bool         m_fInit;
        wchar_t *    m_wszSearchPath;  // Allocated with new []

        static CriticalSection m_cs;
    };

    class SYMSRV
    {
public:
        SYMSRV();
        ~SYMSRV();

        PFNSYMBOLSERVERSTOREFILEW   m_pfnsymbolserverstorefilew;

        bool SymbolServer(LOCATOR *, const wchar_t *, const wchar_t *, const GUID *, DWORD, DWORD, DWORD,  __out_ecount(PDB_MAX_FILENAME) wchar_t *, DWORD *pdwError);

private:
        void Init();

        bool                        m_fInit;
        HMODULE                     m_hmod;
        PFNSYMBOLSERVERW            m_pfnsymbolserverw;
        PFNSYMBOLSERVERSETOPTIONS   m_pfnsymbolserversetoptions;

        static CriticalSection      m_cs;
    };

    friend  class SYMSRV;

    static void Close(FILE *);
    bool FDecodeCvData(const void *, size_t);
    bool FLocateCvFilePathHelper(const wchar_t *, const wchar_t *, size_t, bool);
    bool FLocateDbgDefault();
    bool FLocateDbgPath(const wchar_t *);
    bool FLocateDbgRegistry();
    bool FLocateDbgServer();
    bool FLocateDbgSymsrv(const wchar_t *);
    bool FLocateDbgValidate(const wchar_t *);
    bool FLocatePdbDefault(const wchar_t *);
    bool FLocatePdbPath(const wchar_t *);
    bool FLocatePdbRegistry();
    bool FLocatePdbServer();
    bool FLocatePdbSymsrv(const wchar_t *);
    bool FOpenValidate4(const wchar_t *);
    bool FReadAt(FILE *, DWORD, DWORD, size_t, void *, bool = false);
    bool FReadCodeViewDebugData();
    bool FReadDebugDirInfo();    
    bool FReadHeader(FILE *, DWORD, size_t, void *);
    bool FReadMiscDebugData();
    bool FRestrictDBG();
    bool FRestrictOriginalPath();
    bool FRestrictReferencePath();
    bool FRestrictRegistry();
    bool FRestrictSymsrv();
    bool FRestrictSystemRoot();
    bool FSaveFileNames(const wchar_t *);
    void NotifyDebugDir(BOOL, const struct _IMAGE_DEBUG_DIRECTORY *);
    void NotifyMiscPath(const wchar_t *);
    void NotifyOpenDBG(const wchar_t *, PDBErrors, const wchar_t *);
    void NotifyOpenPDB(const wchar_t *, PDBErrors, const wchar_t *);
    PDBCALLBACK QueryCallback(POVC);
    void ReadMiscPath(PB, FILE *, DWORD, DWORD, DWORD);
    void SetError(EC, const wchar_t * = NULL);


    EC                              m_ec;
    wchar_t                         m_wszError[cbErrMax];

    void *                          m_pvClient;
    PfnPDBQueryCallback             m_pfnQueryCallback;

    bool                            m_fPfnNotifyDebugDir;
    PfnPDBNotifyDebugDir            m_pfnNotifyDebugDir;

    bool                            m_fPfnNotifyMiscPath;
    PfnPDBNotifyMiscPath            m_pfnNotifyMiscPath;

    bool                            m_fPfnNotifyOpenDBG;
    PfnPDBNotifyOpenDBG             m_pfnNotifyOpenDBG;

    bool                            m_fPfnNotifyOpenPDB;
    PfnPDBNotifyOpenPDB             m_pfnNotifyOpenPDB;

    PfnPDBReadExecutableAt          m_pfnReadExecutableAt;
    PfnPDBReadExecutableAtRVA       m_pfnReadExecutableAtRVA;
    PfnPDBReadCodeViewDebugData     m_pfnReadCodeViewDebugData;
    PfnPDBReadMiscDebugData         m_pfnReadMiscDebugData;

    bool                            m_fPfnRestrictDBG;
    PfnPdbRestrictDBG               m_pfnPdbRestrictDBG;

    bool                            m_fPfnRestrictOriginalPath;
    PfnPdbRestrictOriginalPath      m_pfnPdbRestrictOriginalPath;

    bool                            m_fPfnRestrictReferencePath;
    PfnPdbRestrictReferencePath     m_pfnPdbRestrictReferencePath;

    bool                            m_fPfnRestrictRegistry;
    PfnPDBRestrictRegistry          m_pfnRestrictRegistry;

    bool                            m_fPfnRestrictSymsrv;
    PfnPDBRestrictSymsrv            m_pfnRestrictSymsrv;

    bool                            m_fPfnRestrictSystemRoot;
    PfnPDBRestrictSystemRoot        m_pfnRestrictSystemRoot;


    FILE *                          m_fdExe;
    wchar_t *                       m_wszExePath;     // Allocated with wcsdup()
    wchar_t                         m_wszExeFName[_MAX_FNAME];
    wchar_t                         m_wszExeExt[_MAX_EXT];
    const wchar_t *                 m_wszExt;

    bool                            m_fStripped;
    DWORD                           m_dwSizeOfImage;
    DWORD                           m_dwTimeStampExe;
    DWORD                           m_dwTimeStampDbg;
    wchar_t *                       m_wszMiscPath;    // Allocated with new []

    FILE *                          m_fdDbg;
    wchar_t *                       m_wszDbgPath;     // Allocated with wcsdup()

    bool                            m_fCvInExe;
    DWORD                           m_cbCv;
    DWORD                           m_rvaCv;
    DWORD                           m_foCv;

    bool                            m_fGuid;
    GUID                            m_guidSig;
    SIG                             m_sig;
    AGE                             m_age;
    wchar_t *                       m_wszPdb;         // Allocated with new []

    PDB *                           m_ppdb;

    wchar_t *                       m_wszCache;       // Allocated with wcsdup()

    static const wchar_t * const    rgwszEnvName[3];

    static  REGISTRY                m_registryUser;
    static  REGISTRY                m_registryMachine;
    static  SYMSRV                  m_symsrv;
};
