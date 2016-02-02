//////////////////////////////////////////////////////////////////////////////
// Mod implementation declarations
class IDebugSSectionReader;
class IDebugSSectionWriter;
class IDebugSFileInfo;
class IDebugSStringTable;
class remapper;
struct TPI1;

struct Mod1 : public ModCommon { // Mod (one module's debug info) implementation
public:
    INTV QueryInterfaceVersion();
    IMPV QueryImplementationVersion();
    BOOL AddTypes(PB pbTypes, CB cb);
    BOOL AddTimeStamp(DWORD timestamp);
    BOOL AddSymbols(PB pbSyms, CB cb);
    BOOL AddSymbols2(PB pbSyms, DWORD cb, DWORD isectCoff);
    BOOL AddJustSymbols(PB pbSyms, CB cb, bool fFixLevel);
    BOOL AddPublic(SZ_CONST szPublic, ISECT isect, OFF off);
    BOOL AddSecContrib(ISECT isect, OFF off, CB cb, DWORD dwCharacteristics);
    BOOL AddSecContribEx(USHORT isect, long off, long cb, ULONG dwCharacteristics, DWORD dwDataCrc, DWORD dwRelocCrc);
    BOOL AddSecContrib2(USHORT isect, DWORD off, DWORD isectCoff, DWORD cb, DWORD dwCharacteristics);
    BOOL AddSecContrib2Ex(USHORT isect, DWORD off, DWORD isectCoff, DWORD cb, ULONG dwCharacteristics, DWORD dwDataCrc, DWORD dwRelocCrc);
    BOOL QueryCrossScopeExports(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryCrossScopeImports(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryILLines(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryInlineeLines(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryFileChecksums(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryFuncMDTokenMap(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryTypeMDTokenMap(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryMergedAssemblyInput(DWORD cb, PB pb, DWORD *pcb);
    BOOL TranslateFileId(DWORD id, DWORD *pid);
    BOOL QuerySymbols(PB pbSym, CB* pcb);
    BOOL IsTypeServed(DWORD index, BOOL fID);
    BOOL QueryTypes(PB pb, DWORD *pcb);
    BOOL QueryIDs(PB pb, DWORD *pcb);
    BOOL QueryCVRecordForTi(DWORD index, BOOL fID, PB pb, DWORD *pcb);
    BOOL QueryPbCVRecordForTi(DWORD index, BOOL fID, PB *ppb);
    BOOL QuerySrcLineForUDT(TI ti, _Deref_out_z_ char **pszSrc, DWORD *pLine);
    BOOL QueryTiForUDT(const char *sz, BOOL fCase, TI *pti);
    BOOL QueryCoffSymRVAs(PB pb, DWORD *pcb);
    BOOL QueryLines(PB pbLines, CB* pcb);
    BOOL QueryLines2(CB cbLines, PB pbLines, CB* pcbLines);
    BOOL QueryLines3(DWORD cb, PB pb, DWORD *pcb);
    BOOL QueryTpi(OUT TPI** pptpi); // return this Mod's Tpi
    BOOL QueryIpi(OUT TPI** ppipi); // return this Mod's Ipi
    BOOL RemoveGlobalRefs();
    BOOL VerifySymbols(PB pbSym, CB cb);
    BOOL SetPvClient(void *pvClient_)
    {
        // PDB_MT : should not be used when loaded in mspdbsrv.exe.
        pvClient = pvClient_;
        return TRUE;
    }
    BOOL GetPvClient(OUT void** ppvClient_)
    {
        // PDB_MT : should not be used when loaded in mspdbsrv.exe.
        *ppvClient_ = pvClient;
        return TRUE;
    }
    BOOL QuerySecContrib(OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb, OUT DWORD* pdwCharacteristics);
    BOOL QueryDBI(OUT DBI** ppdbi)
    {
        *ppdbi = pdbi1;
        return TRUE;
    }
    BOOL QueryImod(OUT IMOD* pimod)
    {
        *pimod = ximodForImod(imod);
        return TRUE;
    }
    BOOL QueryItsm(OUT USHORT* pitsm) 
    { 
#ifdef PDB_TYPESERVER
        *pitsm = pdbi1->pmodiForImod(imod)->iTSM; 
        return TRUE; 
#else
        *pitsm = 0;
        return TRUE;
#endif
    }

    BOOL QuerySupportsEC();
    BOOL Close();
    BOOL ReplaceLines(BYTE* pbLines, long cb);
    BOOL InsertLines(BYTE* pbLines, long cb);
    OFF offSymNewForOffSymOld(OFF offOld, bool fVC40Offset);

    BOOL MergeTypes(BYTE *pbSyms, DWORD cb);

    BOOL EnCNeedReloadCompilerGeneratedPDB();
    BOOL EnCReleaseCompilerGeneratedPDB(BYTE* pbTypes, DWORD cb);
    BOOL EnCReloadCompilerGeneratedPDB(BYTE* pbTypes, DWORD cb);

    // used only for ppdb1->m_fNoTypeMerge
    SN          snType;
    union {
        SN      snId;
        //SN      snZ7Pch;
        IMOD    imodTypeRef;
    };
    ushort  fOwnTM       : 1;
    ushort  fRefTM       : 1;
    ushort  fOwnTMR      : 1;
    ushort  fOwnTMPCT    : 1;
    ushort  fRefTMPCT    : 1;
    ushort  fNoType      : 1;

    typedef BOOL (Mod1::*PfnQuerySubsection)(DWORD, BYTE *, DWORD *);

private:
    enum {impv = (IMPV) 930803};

    // unchange since Constructor
    PDB1 *      ppdb1;
    DBI1 *      pdbi1;
    IMOD        imod;

    // used only when pdbi1->fWrite
    TM *        ptm;

    // used only when pdbi1->fWrite
    BOOL        fSymsAdded_S                ;    // for use by AddSymbols() only
    BOOL        fSymsAdded_T                ;    // for use by MergeTypes() only
    UINT        fAddLines                   : 1;

    // protected by m_csForC13
    UINT        fCoffToC13Failed            : 1;

    // used when pdbi1->fWrite, tells if C13 symbols need to be fixed up for string offsets
    UINT        fSymsNeedStringTableFixup   : 1;

    // used only when pdbi1->fWrite
    UINT        fECSymSeen                  : 1;
    UINT        fIsCFile                    : 1;
    UINT        fIsCPPFile                  : 1;

    // have we verified symbol records for this mod?
    UINT        fSymRecordsVerified         : 1;

    // does this mod's local sym reference any type?
    UINT        fHasTypeRef                 : 1;

#ifdef PDB_TYPESERVER
    ITSM        itsm;
#endif

#ifdef PDB_MT
    mutable CriticalSection m_csForC13;
#endif

    void *      pvClient;                   // Should not be used when loaded in mspdbsrv.exe
    // used only when pdbi1->fWrite
    Buffer      bufSyms;
    Buffer      bufLines;                   // Used for reading C13 lines with old API

    // protected by m_csForC13 (or pdbi1->fWrite)
    RefBuf      rbufC13Lines;              
    Buffer      bufC13Fpo;
    Buffer      bufC13Strings;

    // Cache lines info converted from COFF lines while writing
    COMRefPtr<IDebugSSectionWriter> m_pLinesWriter1st;      // Saves DebugSFileChkSMS

    // used only when pdbi1->fWrite
    Buffer      bufSymsOut;
    Buffer      bufGlobalRefs;

    // protected by  m_csForC13
    Buffer      bufFileInfo;
    Array<DWORD>             mpIFileToIDFile;
    Map<DWORD, DWORD, HcNi>  mpIDFileToIFile;

    // used only when pdbi1->fWrite
    SC2         sc;

    // protected by pdbi1->m_csForMods (or pdbi1->fWrite)
    WidenTi *   pwti;

    // used only when pdbi1->fWrite
    ULONG       sigSyms_S;          // for use by AddSymbols() only
    ULONG       sigSyms_T;          // for use by MergeTypes() only

    // protected by pdbi1->m_csForMods
    CvtSyms     cvtsyms;
    // protected by pdbi1->m_csForMods
    CvtStSymsToSz cvtstsyms;        // Similar to cvtsyms, except used for ST to SZ conversion
    Buffer        bufSZSyms;        // converted symbols, used for reading.

    DWORD       dwTimeStamp;

    BOOL        fReadAndConvertSyms(PB pbDst, CB* pcb);

    Mod1(PDB1* ppdb1_, DBI1* pdbi1_, IMOD imod_);
    ~Mod1();
    BOOL fAddSymbols(PB pb, DWORD cb, DWORD isectCoff);
    BOOL fAddSymRefToGSI(PB pb, DWORD cb, DWORD isectCoff);
    BOOL fAddModTypeRefSym();
    BOOL fInit();
    BOOL fCommit();
    BOOL fConvertIdToType(PSYM psym);
    BOOL fCopyTpiStream(MSF *pmsfFrom, SN snFrom, SN *psnTo);
    BOOL fCopyTM();
    BOOL fCopyTMR(TMR *ptmr, SN *psn);
    BOOL fUpdateLines();
    BOOL fUpdateSyms();
    inline BOOL fUpdateSecContrib();

    // patch TIs or IDs that are not referenced by symbol records
    BOOL fUpdateTiOrIdReferencedByNonSyms(remapper& mapStrings);
    
    BOOL fProcessSyms();
    BOOL fProcessGlobalRefs();
    BOOL fEnsureSn(SN* psn)
    {
        return ppdb1->fEnsureSn(psn);
    }

    BOOL fEnsureNoSn(SN* psn)
    {
        return ppdb1->fEnsureNoSn(psn);
    }

    // used by MLI::EmitFileInfo()
    BOOL initFileInfo(IFILE ifileMac) { return pdbi1->initFileInfo(imod, ifileMac); }
    BOOL addFileInfo(IFILE ifile, SZ_CONST szFile);

    BOOL fReadPbCb(PB pb, CB* pcb, OFF off, CB cb);
    CB cbGlobalRefs();
    BOOL fQueryGlobalRefs(PB pbGlobalRefs, CB cb);
    BOOL fQueryCVRecordForTi(DWORD index, BOOL fID, PTYPE *pptype, DWORD *pcb);
    bool fUdtIsDefn(TI ti) const;
    bool fUdtIsESU(TI ti) const;
    bool fUdtNameMatch(TI tiUdtFrom, const UDTSYM * pudt) const;
    BOOL packId(PSYM psym);
    BOOL packType(PSYM psym);
    inline BOOL fCopySymOut(PSYM psym);
    inline BOOL fCopySymOut(PSYM psym, PSYM * ppsymOut);
    inline BOOL fCopyGlobalRef(OFF off);
    inline void EnterLevel(PSYM psym, DWORD & offParent, int & iLevel);
    inline void ExitLevel(PSYM psym, DWORD & offParent, int & iLevel);

    SZ szObjFile() { return pdbi1->szObjFile(imod); }
    SZ szModule()  { return pdbi1->szModule(imod); }
    CB cbSyms()    { return pdbi1->cbSyms(imod); }
    CB cbLines()   { return pdbi1->cbLines(imod); }
    CB cbC13Lines()          { return pdbi1->cbC13Lines(imod); }
    CB cbStream() { 
        SN sn = pdbi1->Sn(imod);
        if (sn == snNil) {
            return 0;
        }
        return ppdb1->pmsf->GetCbStream(sn);
    }
    bool processC13(bool fWrite);
    bool processC13Strings(bool fWrite, IDebugSSectionReader* pStringReader, remapper& mapStrings, IDebugSStringTable** ppTable);

    friend BOOL DBI1::openModByImod(IMOD imod, OUT Mod** ppmod);
    friend BOOL MLI::EmitFileInfo(Mod1* pmod1);

    bool InitC13Reader(Buffer& rbuf, IDebugSSectionReader **ppReader);
    // New line numbers support
    bool GetEnumLines(EnumLines** ppenum);
    bool QueryLineFlags(OUT DWORD* pdwFlags); // what data is present?
    bool QueryFileNameInfo(
                    IN DWORD        fileId,                 // source file identifier
                    __out_ecount_full_opt(*pccFilename) wchar_t*    szFilename,             // file name string 
                    IN OUT DWORD*   pccFilename,            // length of string
                    OUT DWORD*      pChksumType,            // type of chksum
                    OUT BYTE*       pbChksum,               // pointer to buffer for chksum data
                    IN OUT DWORD*   pcbChksum               // number of bytes of chksum (in/out)
                    );

    bool GetEnumILLines(EnumLines** ppenum);
    bool QueryILLineFlags(OUT DWORD* pdwFlags);

    // Use internally by GetC11LinesFromC13
    bool QueryFileNameInfoInternal(
                    IDebugSFileInfo* pC13Files,
                    IN DWORD        fileId,                 // source file identifier
                    __out_ecount_full_opt(*pccFilename) wchar_t*    szFilename,             // file name string 
                    IN OUT DWORD*   pccFilename,            // length of string
                    OUT DWORD*      pChksumType,            // type of chksum
                    OUT BYTE*       pbChksum,               // pointer to buffer for chksum data
                    IN OUT DWORD*   pcbChksum,              // number of bytes of chksum (in/out)
                    NameMap*        pNameMapIn = NULL
                    );
    bool fInitRefBuffer(RefBuf *prbuf);
    bool findC13Lines();
    bool Mod1::fQueryC13LinesBuf(DWORD cb, PB pb, DWORD *pcb, enum DEBUG_S_SUBSECTION_TYPE e);

    // Long File name support
    BOOL fConvertSymRecsStToSz(PB pbSrc, CB cbSrc);
    BOOL fReadAndConvertStSyms(PB pb, CB *pcb);
    BOOL fRemapParents(Buffer &buf);

    BOOL fForcedReadSymbolInfo(PB pb, CB& cb);
    BOOL ConvertFileNamesInLineInfoFmMBCSToUnicode(PB pb, CB& cb, Buffer& buffer);
    BOOL AddPublicW(const wchar_t* szPublic, USHORT isect, OFF off, CV_pubsymflag_t cvpsf =0);
    BOOL AddLinesW(const wchar_t* szSrc, USHORT isect, OFF offCon, CB cbCon, OFF doff,
                          LINE32 lineStart, PB pbCoff, CB cbCoff);
    BOOL QueryNameW(_Out_z_cap_(PDB_MAX_PATH) OUT wchar_t szName[PDB_MAX_PATH], OUT CB* pcch);
    BOOL QueryFileW(_Out_z_cap_(PDB_MAX_PATH) OUT wchar_t szFile[PDB_MAX_PATH], OUT CB* pcch);
    BOOL QuerySrcFileW(_Out_z_cap_(PDB_MAX_PATH) OUT wchar_t szFile[PDB_MAX_PATH], OUT CB* pcch);
    BOOL QueryPdbFileW(_Out_z_cap_(PDB_MAX_PATH) OUT wchar_t szFile[PDB_MAX_PATH], OUT CB* pcch);
    BOOL AddPublic2(const char* szPublic, USHORT isect, long off, CV_pubsymflag_t cvpsf =0);
    void ReportDelayLoadError(DWORD ec, PDelayLoadInfo pdli);
    bool CheckFCreateReader(PB pb, CB cb, IDebugSSectionReader** ppReader, DWORD sig);
    bool CheckFCreateWriter(bool flag, IDebugSSectionWriter** ppWriter, DWORD sig, bool flag2);
    BOOL GetC11LinesFromC13(Buffer& bufC11Lines);
    bool QueryLineFlagsHelper(OUT DWORD* pdwFlags, bool fIL);
    BOOL fReadFromStream(SN sn, PB pb, DWORD* pcb);
    BOOL GetTmts(BYTE* pbTypes, DWORD cb, TM** pptm, BOOL fQueryOnly);

    static int ExceptionFilter(DWORD ec, _EXCEPTION_POINTERS *pei, Mod1 * pmod);
};
