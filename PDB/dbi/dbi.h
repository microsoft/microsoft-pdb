//////////////////////////////////////////////////////////////////////////////
// DBI implementation declarations
#pragma once

#pragma warning(push)
#pragma warning(disable:4200)

// header of the DBI Stream

// section contribution version, before V60 there was no section version
enum {
    DBISCImpvV60  = 0xeffe0000 + 19970605,
    DBISCImpv     = DBISCImpvV60,
    DBISCImpv2    = 0xeffe0000 + 20140516,
};


struct OMAP_DATA {
    DWORD       rva;
    DWORD       rvaTo;
};

// For type mismatch detection (TMD)

typedef struct TMD_INFO  TMD_INFO;
typedef TMD_INFO  *PTMD_INFO;

struct TMD_INFO {
    PTMD_INFO    next;
    TI           ti;      // type index in final PDB being produced
    USHORT       imod;
    wchar_t *    wszSrc;
    DWORD        line;
};

typedef struct TMD  TMD;
typedef TMD  *PTMD;

struct TMD {
    PTMD       next;
    PTMD_INFO  pInfo;
};

// Support for bit vector operations

#ifndef BITSPERBYTE
#define BITSPERBYTE (8)
#endif /* BITSPERBYTE */

struct BITVEC {

public:
    BITVEC() {
        cbits = 0;
        bv = 0;
    }
    ~BITVEC() {
        if (bv)
            delete [] bv;
    }
    BOOL fAlloc(size_t _cbits) {
        // Protect against interger overflow
        if (_cbits > s_cbMaxAlloc) {
            return FALSE;
        }
        bv = (unsigned char *) new (zeroed) CHAR[(_cbits + BITSPERBYTE - 1) / BITSPERBYTE];
        if (bv) {
            cbits = _cbits;
            return TRUE;
        }
        return FALSE;
    }
    BOOL fTestBit(size_t index) {
        assert(bv);
        if (index >= cbits)
            return FALSE;
        else
            return ((bv[index / BITSPERBYTE] >> (index % BITSPERBYTE)) & 1);
    }
    BOOL fSetBit(size_t index) {
        assert(bv);
        if (index >= cbits)
            return FALSE;
        else
            bv[index / BITSPERBYTE] |= (1 << (index % BITSPERBYTE));
        return TRUE;
    }

private:
    unsigned char *bv;
    size_t  cbits;
};

// header of the DBI Stream
struct DBIHdr {
        SN      snGSSyms;
        SN      snPSSyms;
        SN      snSymRecs;
        CB      cbGpModi;   // size of rgmodi substream
        CB      cbSC;       // size of Section Contribution substream
        CB      cbSecMap;
        CB      cbFileInfo;
        DBIHdr()
        {
                snGSSyms = snNil;
                snPSSyms = snNil;
                snSymRecs = snNil;
                cbGpModi = 0;
                cbSC = 0;
                cbSecMap = 0;
                cbFileInfo = 0;
        }
};

enum {
    DBIImpvV41  = 930803,
    DBIImpvV50  = 19960307,
    DBIImpvV60  = 19970606,
    DBIImpvV70  = 19990903,
    DBIImpvV110 = 20091201,
    DBIImpv     = DBIImpvV70,
};

struct NewDBIHdr {
    enum {
        hdrSignature = -1,
        hdrVersion = DBIImpv,
    };

    // unchanged since fInit
    ULONG       verSignature;

    // only used in finit or fSave
    ULONG       verHdr;

    // protected by m_csForHdr
    AGE         age;

    // protected by m_csForPGSI
    SN          snGSSyms;

    // protected by m_csForHdr
    union {
        struct {
            USHORT      usVerPdbDllMin : 8; // minor version and
            USHORT      usVerPdbDllMaj : 7; // major version and 
            USHORT      fNewVerFmt     : 1; // flag telling us we have rbld stored elsewhere (high bit of original major version)
        } vernew;                           // that built this pdb last.
        struct {
            USHORT      usVerPdbDllRbld: 4;
            USHORT      usVerPdbDllMin : 7;
            USHORT      usVerPdbDllMaj : 5;
        } verold;
        USHORT          usVerAll;
    };


    // protected by m_csForPGSI
    SN          snPSSyms;

    // protected by m_csForHdr
    USHORT      usVerPdbDllBuild;   // build version of the pdb dll
                                    // that built this pdb last.

    // protected by m_csForSymRec
    SN          snSymRecs;

    // protected by m_csForHdr
    USHORT      usVerPdbDllRBld;    // rbld version of the pdb dll
                                    // that built this pdb last.

    // protected by m_csForMods;
    CB          cbGpModi;   // size of rgmodi substream

    // protected by m_csSecContrib
    CB          cbSC;       // size of Section Contribution substream

    // only used in fInit or fSave
    CB          cbSecMap;
    CB          cbFileInfo;
    CB          cbTSMap;    // size of the Type Server Map substream
    ULONG       iMFC;       // index of MFC type server
    CB          cbDbgHdr;   // size of optional DbgHdr info appended to the end of the stream
    CB          cbECInfo;   // number of bytes in EC substream, or 0 if EC no EC enabled Mods

    // protected by m_csForHdr
    struct _flags {
        USHORT  fIncLink:1;     // true if linked incrmentally (really just if ilink thunks are present)
        USHORT  fStripped:1;    // true if PDB::CopyTo stripped the private data out
        USHORT  fCTypes:1;      // true if this PDB is using CTypes.
        USHORT  unused:13;      // reserved, must be 0.
    } flags;
    USHORT      wMachine;   // machine type

    ULONG       rgulReserved[ 1 ];      // pad out to 64 bytes for future growth.

    NewDBIHdr()
    {
        memset(this, 0, sizeof(*this));
        verSignature = ULONG(hdrSignature);
        verHdr = ULONG(hdrVersion);
        snGSSyms = snNil;
        snPSSyms = snNil;
        snSymRecs = snNil;
    }

    NewDBIHdr(const DBIHdr & dbihdrOld)
    {
        memset(this, 0, sizeof(*this));
        verSignature = ULONG(hdrSignature);
        verHdr = ULONG(hdrVersion);
        snGSSyms    = dbihdrOld.snGSSyms;
        snPSSyms    = dbihdrOld.snPSSyms;
        snSymRecs   = dbihdrOld.snSymRecs;
        cbGpModi    = dbihdrOld.cbGpModi;
        cbSC        = dbihdrOld.cbSC;
        cbSecMap    = dbihdrOld.cbSecMap;
        cbFileInfo  = dbihdrOld.cbFileInfo;
    }

    NewDBIHdr & 
    operator=(const DBIHdr & dbihdrOld)
    {
        snGSSyms    = dbihdrOld.snGSSyms;
        snPSSyms    = dbihdrOld.snPSSyms;
        snSymRecs   = dbihdrOld.snSymRecs;
        cbGpModi    = dbihdrOld.cbGpModi;
        cbSC        = dbihdrOld.cbSC;
        cbSecMap    = dbihdrOld.cbSecMap;
        cbFileInfo  = dbihdrOld.cbFileInfo;
        cbECInfo    = 0;
        return *this;
    }

    // protected by m_csForHdr
    void SetPdbVersion(int verMaj, int verMin, int verBuild, int verRbld)
    {
        vernew.fNewVerFmt = 1;
        vernew.usVerPdbDllMaj = verMaj;
        vernew.usVerPdbDllMin = verMin;
        usVerPdbDllBuild = verBuild;
        usVerPdbDllRBld = verRbld;
    }

};

// Make sure our version bitfields and unions don't consume more than a USHORT.
cassert(offsetof(NewDBIHdr,snPSSyms) - offsetof(NewDBIHdr,usVerAll) == sizeof(USHORT));

// optional header is stored at the end of the DBI stream
// Its presence is denoted by a nonzero value in the
// cbDbgHdr field of NewDBIHdr
struct DbgDataHdr {

    SN rgSnDbg[dbgtypeMax];

    DbgDataHdr()
    {
        memset(this, 0, sizeof(*this));
        for (int i=0; i<dbgtypeMax; i++) {
            rgSnDbg[i] = snNil;
        }
    }

    BOOL fInUse()
    {
        // return TRUE if at least one dbg-data stream is in use
        for (int i=0; i<dbgtypeMax; i++)
            if (rgSnDbg[i] != snNil)
                return TRUE;
        return FALSE;
    }

};

// helper class to use to encapsulate the 16 to 32-bit widening information
struct CvtSyms {
    BOOL            _fConverting;
    Buffer          _bufSyms;
    Array<OffMap>   _rgOffMap;

    static BOOL
    fCompOffMap(OffMap * pOffMap, void * off)
    {
        // safe cast for Win64; void * off is really an OFF, always 32bits.
        return ULONG(int_ptr_t(off)) <= pOffMap->offOld;
    }

    CvtSyms() : _fConverting(FALSE) { }

    BOOL
    fConverting() const { return _fConverting; }

    BOOL &
    fConverting() { return _fConverting; }

    Buffer &
    bufSyms() { return _bufSyms; }

    Array<OffMap> &
    rgOffMap() { return _rgOffMap; }

    OFF
    offSymNewForOffSymOld(ULONG offSymOld)
    {
        if (!_fConverting) {
            return offSymOld;
        }
        // binary search for the old=>new mapping
        unsigned
            iOffMap = _rgOffMap.binarySearch(fCompOffMap, PV(uint_ptr_t(offSymOld)));
        OffMap &
            offmap = _rgOffMap[iOffMap];

        // we expect a match to be found, but stale symbols in the globals from
        // incremental links do appear and we might as well not update them, in
        // order to not confuse the issue.
        if (offmap.offOld == offSymOld) {
            return offmap.offNew;
        }
        return offSymOld;
    }
};

class CvtStSymsToSz  {

    // Symbol offset map specially for converting from
    // ST symbols to SZ symbols in a mod. The array simply
    // holds differences in _rgOffMap, since most of the
    // symbols retain the length, except for S_COMPILE2
    // and S_OBJNAME, which can expand on translation
    Array<OffMap> _rgOffMap;            // Holds the map
    BOOL          _fValid;              // TRUE if the map is in _rgOffMap

public :

    CvtStSymsToSz()
    {
        _fValid = FALSE;
    }

    Array<OffMap> &rgOffMap()
    { 
        return _rgOffMap; 
    }

    BOOL fValid()
    {
        return _fValid;
    }

    BOOL SetValid()
    {
        return _fValid = TRUE;
    }

    OFF offSymNewForOffSymOld(ULONG offSymOld)
    {
        assert(_fValid);

        for(int i=_rgOffMap.size() - 1; i>=0; i--) {
            if (_rgOffMap[i].offOld <= offSymOld) {
                OffMap offmap = _rgOffMap[i];
                return offSymOld + (offmap.offNew - offmap.offOld);
            }
        }

        return offSymOld;
    }
};

struct TSM {
    DWORD   reserved;   //TPI*  pServer;    // currently open type map
    TI      tiBase;     // server base
    // following is a description of a type server from LF_TYPESERVER
    SIG     signature;      // signature
    AGE     age;            // age of database used by this module
    char    szNamePath[];     // name and reference path for a PDB

    SZ szName()     { return szNamePath; }
    SZ szPath()     { return szNamePath+strlen(szName()) + 1; }
    TI ti()         { return tiBase; }
    PB pbEnd() {
        return pbAlign(PB(szPath()) + strlen(szPath()) + 1);
    }
    void* operator new (size_t size, Buffer& bufTsm, _In_z_ SZ szName, _In_z_ SZ szPath);
    TSM(SIG sig_, AGE age_, _In_z_ SZ szName_, _In_z_ SZ szPath_, TI tiMin_)
        : tiBase(tiMin_), signature(sig_), age(age_)
    {
        memcpy(szName(), szName_, strlen(szName_) + 1);
        memcpy(szPath(), szPath_, strlen(szPath_) + 1);
    }
};

inline void* TSM::operator new(size_t size, Buffer& bufTsm, SZ szName_, SZ szPath_)
{
    if (!szName_ || !szPath_)
        return 0;
    PB pb;
    size_t cb = cbAlign(size + strlen(szName_) + 1 + strlen(szPath_) + 1);
    return bufTsm.Reserve(CB(cb), &pb) ? pb : 0;
}

#ifdef PDB_TYPESERVER

struct OPDB {   // open type servers - all pdb's are opened readonly
    PDB *   ppdb;
    OPDB *  next;
    TPI*    ptpi;

    OPDB(PDB* ppdb_, TPI * ptpi_, OPDB *next_ = 0) : ppdb(ppdb_), ptpi(ptpi_), next(next_) {}
    ~OPDB() {
        if (ptpi != 0)
            ptpi->Close();

        if (ppdb != 0)
            ppdb->Close();
    }
};

#endif

struct MemBlock {
    DWORD rva;
    DWORD cb;
    MemBlock(DWORD _rva = 0, DWORD _cb = 0)
        : rva(_rva), cb(_cb)
    {
        assert(rva + cb >= rva);
    }
    bool in(DWORD _rva) {
        return rva <= _rva && _rva - rva < cb;
    }
};

class Dbg1;
class Dbg1Data;

enum SPD {   // Symbol Packing Dispensation
    spdIdentical,   // name exists in global scope and is identical
    spdMismatch,    // name exists in global scope but doesn't match
    spdAdded,       // name nonexistent in global scope, was added.
    spdInvalid      // operation failed completely
};

// Get rid of the helper macro and suppress the deprecated warning
//
#if defined(QueryModFromAddr)
#undef QueryModFromAddr
#endif

typedef int (__cdecl *PFN_DBGCMP)(const void*, const void*);

#pragma warning(push)
#pragma warning(disable : 4996)

struct DBI1 : public DBICommon { // DBI (debug information) implemenation

public:

        INTV QueryInterfaceVersion();                                               // mts safe
        IMPV QueryImplementationVersion();                                          // mts safe

        BOOL DeleteMod(SZ_CONST szModule);                                          // mts safe
#if 0  // NYI
        BOOL QueryCountMod(long *pcMod);
#endif
        BOOL QueryNextMod(Mod* pmod, Mod** ppmodNext);                              // mts safe
        BOOL OpenGlobals(OUT GSI** ppgsi);
        BOOL OpenPublics(OUT GSI** ppgsi);
        BOOL AddSec(ISECT isect, USHORT flags, OFF off, CB cb);
        BOOL AddPublic(SZ_CONST szPublic, ISECT isect, OFF off);
        BOOL RemovePublic(SZ_CONST szPublic);
        BOOL QueryModFromAddr(ISECT isect, OFF off, OUT Mod** ppmod,
                OUT ISECT * pisect, OUT OFF* poff, OUT CB* pcb);
        BOOL QueryModFromAddr2(ISECT isect, long off, OUT Mod** ppmod,
                    OUT ISECT* pisect, OUT long* poff, OUT long* pcb,
                    OUT ULONG * pdwCharacteristics);
        BOOL QueryModFromAddrEx(USHORT isect, DWORD off, OUT Mod** ppmod,
                    OUT USHORT* pisect, OUT ULONG* pisectCoff, OUT ULONG* poff,
                    OUT ULONG* pcb, OUT ULONG * pdwCharacteristics);
        BOOL QueryAddrForSec(OUT USHORT* pisect, OUT long* poff,
            IMOD imod, long cb, DWORD dwDataCrc, DWORD dwRelocCrc);
        BOOL QueryAddrForSecEx(OUT USHORT* pisect, OUT long* poff, IMOD imod,
                long cb, DWORD dwDataCrc, DWORD dwRelocCrc, DWORD dwCharacteristics);
        BOOL QuerySupportsEC();
        BOOL FStripped();
        BOOL QuerySecMap(OUT PB pb, CB* pcb);
        BOOL QueryFileInfo(OUT PB pb, CB* pcb);
        BOOL QueryFileInfo2(OUT PB pb, CB* pcb);
        void DumpMods();
        void DumpSecContribs();
        void DumpSecMap();

        BOOL Close();
        BOOL AddThunkMap(OFF* poffThunkMap, UINT nThunks, CB cbSizeOfThunk,
            SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable);

#ifdef PDB_MT
#pragma message(MTS_MESSAGE("Not thread safe - EnumSC use of bufSC is not MTS"))
#endif

        BOOL getEnumContrib(OUT Enum** ppenum);
        BOOL getEnumContrib2(OUT Enum** ppenum);

        wchar_t * szGetTsInfo(PTYPE pts, wchar_t *szNameBuf, wchar_t *szFullMapped, SIG70 *pSig70, AGE *pAge);

        BOOL QueryTypeServer(ITSM itsm, OUT TPI** pptpi);
        BOOL QueryItsmForTi(TI ti, OUT ITSM* ptpi);
        BOOL QueryNextItsm(ITSM itsm, OUT ITSM *inext);
        BOOL QueryLazyTypes();
        BOOL SetLazyTypes(BOOL fLazy);    // lazy is default and can only be turned off
        void FlushTypeServers();

        BOOL FindTypeServers(OUT EC* pec, _Out_opt_cap_(cbErrMax) OUT char szError[cbErrMax]);
        void DumpTypeServers();

        AGE QueryAge() const {
            MTS_PROTECT(m_csForHdr);
            return dbihdr.age;
        }

        void * QueryHeader() const {
            // This is not thread safe. The server use QueryHeader2
            return PV(&dbihdr);
        }
        BOOL QueryHeader2(CB cb, PB pb, CB *pcbOut) {
            dassert(pcbOut);
            MTS_PROTECT(m_csForHdr);

            *pcbOut = sizeof(dbihdr);
            if (pb) {
                if (cb < sizeof(dbihdr)) {
                    return FALSE;
                }
                memcpy(pb, &dbihdr, sizeof(dbihdr));
            }
            return TRUE;
        }
        BOOL OpenDbg(DBGTYPE dbgtype, OUT Dbg **ppdbg);
        BOOL QueryDbgTypes(OUT DBGTYPE *pdbgtype, OUT long* pcDbgtype);
        BOOL QueryPdb(OUT PDB** pppdb);
        BOOL AddLinkInfo(IN PLinkInfo);
        BOOL QueryLinkInfo(PLinkInfo, OUT long * pcb);

        BOOL OpenModW(const wchar_t* szModule, const wchar_t* szFile, OUT Mod** ppmod);
        BOOL DeleteModW(const wchar_t* szModule);
        BOOL AddPublicW(const wchar_t* szPublic, USHORT isect, long off, CV_pubsymflag_t cvpsf =0);
        BOOL QueryTypeServerByPdbW(const wchar_t* szPdb, OUT ITSM* pitsm);
        BOOL AddLinkInfoW(IN PLinkInfoW);
        BOOL AddPublic2(const char* szPublic, USHORT isect, long off, CV_pubsymflag_t cvpsf =0);
        USHORT QueryMachineType() const;
        void SetMachineType(USHORT wMachine);
        BOOL QueryNoOfMods(long *cMods) {
            *cMods = imodMac;
            return TRUE;
        }
        BOOL QueryMods(Mod **ppmodNext, long cMods);
        BOOL QueryImodFromAddr(USHORT isect, long off, OUT IMOD* pimod,
                    OUT USHORT* pisect, OUT long* poff, OUT long* pcb, 
                    OUT ULONG * pdwCharacteristics);
        BOOL QueryImodFromAddrEx(USHORT isect, ULONG off, OUT IMOD* pimod,
                    OUT USHORT* pisect, OUT ULONG* pisectCoff, OUT ULONG* poff,
                    OUT ULONG* pcb, OUT ULONG * pdwCharacteristics);
        BOOL OpenModFromImod(USHORT imod, OUT Mod **ppmod) {
            return openModByImod(imodForXimod(imod), ppmod);
        }

        BOOL QueryModTypeRef(IMOD imod, MODTYPEREF *pmtr);

        BOOL FAddSourceMappingItem(
            const wchar_t * szMapTo,
            const wchar_t * szMapFrom,
            ULONG           grFlags
        );

        BOOL FSetPfnNotePdbUsed(void *, DBI::PFNNOTEPDBUSED);
        BOOL FSetPfnNoteTypeMismatch(void *, DBI::PFNNOTETYPEMISMATCH);
        BOOL FSetPfnTmdTypeFilter(void *, DBI::PFNTMDTYPEFILTER);
        BOOL FCTypes();
        BOOL FSetPfnQueryCallback(void *, PFNDBIQUERYCALLBACK);

        void RemoveDataForRva(ULONG rva, ULONG cb);

        void SetSCVersion2() { fSCv2 = true; }
        void UnsetSCVersion2() { fSCv2 = false; }

private:
        // Helper functions

        template<typename T>
        BOOL QueryImodFromAddrHelper(USHORT isect, ULONG off, OUT IMOD* pimod,
                    OUT USHORT* pisect, OUT ULONG* pisectCoff, OUT ULONG* poff,
                    OUT ULONG* pcb, OUT ULONG * pdwCharacteristics);

        DWORD SizeOfSCEntry()
        {
            size_t cb = fSCv2 ? sizeof(SC2) : sizeof(SC);

            return (DWORD) cb;
        }

        BOOL QueryTypeServerByPdbUTF8(const char* szPdb, OUT ITSM* piTsm);
        static PLinkInfo GetUTF8LinkInfo(PLinkInfo pli);
        static PLinkInfo GetUTF8LinkInfo(PLinkInfoW pli);

#ifdef PDB_TYPESERVER
        // used in PDB1::OpenDBIEx()
        void SetFindFunc(PfnFindDebugInfoFile pfn_) {
            m_pfnFindDIF = pfn_;
        }
        void GetFindFunc(PfnFindDebugInfoFile *pfn_) {
            *pfn_ = m_pfnFindDIF;
        }
#endif

        BOOL ReportTypeMismatches();
        
        // used by GSI1 and PSGSI1 inits and DBI1::fixupRefSymsForImod
        BOOL fReadSymRecs();                                                                
        BOOL fConvertSymRecs(CB);       // used in fReadSymRecs()

#ifdef PDB_DEFUNCT_V8
        BOOL fConvertSymRecsToSZ(CB);
        BOOL fReadAllGlobals();
#endif
        // used in fReadSymRec and fReadSymRecPage and DBI1::offForSym
        bool fValidPsym(PSYM psym)                                                          
        {
            MTS_ASSERT(m_csForSymRec);

            // Verifies that the pointer itself is contained; not the contents
            // of the symbol record pointed to.
            //
            PB  pbBase = bufSymRecs.Start();
            return
                reinterpret_cast<PB>(psym) >= pbBase &&
                reinterpret_cast<PB>(psym) - pbBase <  
                    (fWrite ? bufSymRecs.Size() : ppdb1->pmsf->GetCbStream(dbihdr.snSymRecs));
        }

        //======================
        // used by GSI1
        //======================
        OFF offForSym(PSYM psym)                                                            // mts safe
        {
            MTS_PROTECT(m_csForSymRec);
            assert(psym);
            expect(bufSymRecs.Contains(psym));
            if (fValidPsym(psym)) {
                return static_cast<OFF>(reinterpret_cast<PB>(psym) - bufSymRecs.Start());
            }
            else {
                return static_cast<OFF>(-1);
            }
        }
        BOOL fpsymFromOff(OFF off, PSYM *ppsym);
        BOOL fAddSym(PSYM psymIn, OUT PSYM* psymOut);

        //=========================
        // used by GSI1 and PSGSI1
        //=========================
        BOOL fReadSymRec(PSYM);

        //======================
        // used by Mod1
        //======================
        // used in Mod1::fInit()
        BOOL invalidateSCforMod(IMOD imod);

        inline SZ szObjFile(IMOD imod); 
        inline SZ szModule(IMOD imod);
        inline CB cbSyms(IMOD imod);
        inline CB cbLines(IMOD imod);
        inline CB cbC13Lines(IMOD imod);
        inline SN Sn(IMOD imod);

        bool InitFrameDataStream();

        bool AddFrameData(FRAMEDATA* pframe, DWORD n);
        bool LoadFrameData();
        bool SaveFrameData();

        void fixupRefSymsForImod(unsigned imod, CvtSyms &);

        // used in Mod1::fUpdateSecContrib()
        BOOL addSecContrib(SC2& scIn);       

        // used in Mod1::QuerySrcFile, Mod1::QueryPdbFile and DBI1::DumpMods
        BOOL srcForImod(unsigned imod, _Out_z_cap_(_MAX_PATH) char szSrcFile[ _MAX_PATH ], long* pcb);       // mts safe
        BOOL pdbForImod(unsigned imod, _Out_z_cap_(_MAX_PATH) char szPdbFile[ _MAX_PATH ], long* pcb);       // mts safe

        // used in Mod1::fProcessSyms()
        BOOL setSrcForImod(unsigned imod, SZ_CONST szSrcFile);                        // mts safe
        BOOL setPdbForImod(unsigned imod, SZ_CONST szPdbFile);                        // mts safe
        inline BOOL packRefToGS (PSYM psym, IMOD imod, OFF off, OFF *poff);
        inline BOOL packRefToGSForMiniPDB (const char *sz, WORD imod, DWORD isectCoff,
                                           bool fLocal, bool fData, bool fUDT, bool fLabel,
                                           bool fConst);
        inline BOOL packSymToGS (PSYM psym, OFF *poff);
        inline BOOL packSymSPD (PSYM, OFF*, SPD &);
        inline BOOL packTokenRefToGS (PSYM psym, IMOD imod, OFF off, OFF *poff);

        // used in AddPublic* in DBI1 and MOD1 
        inline BOOL packSymToPS (PSYM psym);

        inline BOOL removeGlobalRefsForMod(IMOD imod);

        //==========================
        // Type server helpers
        //==========================
        // used by Mod1::AddTypes
        BOOL fGetTmts(PTYPE pts, UTFSZ_CONST szObjFile, TM** pptm, BOOL fQueryOnly);
        
        // used by DBI1::fGetTmts
        BOOL fOpenTmts(const wchar_t *szName, const SIG70& sig70, AGE age, UTFSZ_CONST szObjFile, TM** pptm, AGE &agePdb);

        // used by DBI1::fOpenTmts and DBI1::QueryTypeServer
        BOOL fOpenPdb(const SIG70& sig70, AGE age, const wchar_t *szName, UTFSZ_CONST szObjFile, PDB** pppdb, BOOL fQuery=FALSE);

        BOOL fGetTmpct(PTYPE ppc, TMPCT** pptmpct);
        BOOL fAddTmpct(lfEndPreComp* pepc, TI tiPreComp, SZ_CONST szModule, TMPCT* ptmpct);
        BOOL fUpdateTmpct(SZ_CONST szModule, SZ_CONST szInternalName, TM* ptm);
        BOOL fFindTm(OTM* potm, const wchar_t *szName, const SIG70& sig70, AGE age, TI tiPreComp, TM** pptm);
        static void fixSymRecs (void* pdbi, void* pOld, void* pNew);

        ITSM ItsmFromTi(TI ti) {
            return (ITSM)(ti>>24);
        }
#ifdef PDB_TYPESERVER
        BOOL AddTypeServer(SIG, AGE, SZ, SZ, OUT ITSM *pitsm);

        TI TiForTiItsm(TI ti, ITSM itsm) {
            assert((ti & (0xff<<24)) == 0);
            return CV_IS_PRIMITIVE(ti) ? ti : ((itsm<<24) | ti);
        }
        TI ConvertItem(TI ti, ITSM itsm) {
            return TiForTiItsm(OriginalTi(ti), itsm);
        }
        TI OriginalTi(TI ti) {
            return ti & 0x00ffffff;
        }
#endif //PDB_TYPESERVER
#ifdef INSTRUMENTED
        void DumpSymbolPages();
#endif

        void SetIncLink() {  // mark this as an incremental link build
            MTS_PROTECT(m_csForHdr);
            dbihdr.flags.fIncLink = true;
        }
        void SetStripped(bool f) {
            MTS_PROTECT(m_csForHdr);
            dbihdr.flags.fStripped = f;
        }

        // Used by Dbg1
        Dbg1Data *fGetDbgData(int iDbg, ULONG cbElement, PFN_DBGCMP pfn);
        BOOL fSaveDbgData(int iDbg, Buffer &buffer);

protected:
        friend PDB1;
        DBI1(PDB1* ppdb1_, BOOL fWrite_, bool fCTypes_, bool fTypeMismatch_);
        ~DBI1();
        BOOL fInit(BOOL fCreate);
        BOOL fSave();
        BOOL fCheckReadWriteMode(BOOL fWrite);
        BOOL openModByImod(IMOD imod, OUT Mod** ppmod);
        // used in DBI1::OpenModW
        IMOD imodForModNameW(const wchar_t* szModule, const wchar_t* szObjFile);
        MODI* pmodiForImod(IMOD imod) { 
            MTS_ASSERT(m_csForMods);
            return (0 <= imod && imod < imodMac) ? rgpmodi[imod] : 0; 
        }
        void NoteModCloseForImod(IMOD imod);
        BOOL initFileInfo(IMOD imod, IFILE ifileMac);
        BOOL addFileInfo(IMOD imod, IFILE ifile, SZ_CONST szFile);
        BOOL addFilename(SZ_CONST szFile, ICH *pich);
        static void fixBufGpmodi(void* pDBI1, void* pOld, void* pNew);
        static void fixBufBase(void* pv, void* pvOld, void* pvNew);
        BOOL fValidateSnDbg(SN *psnDbg);
        MSF * getpmsf() {return ppdb1->pmsf;}
#ifdef PDB_DEFUNCT_V8
        BOOL fLoadImageHeader(IMAGE_SEPARATE_DEBUG_HEADER *pImageHeader);
#endif

        // used in DBI1::QuerySupportsEC()
        BOOL fEnCSymbolsPresent();
        BOOL IsLinkedIncrementally();

        // used in DBI1::fSave()
        BOOL fWriteModi(Buffer &);

        // used by DBI1::Close(), PDB1::OpenDBIEx, PDB1::CreateDBI
        BOOL internal_Close();          // do hard close
private:

    typedef NMT NMT_CTG;                // contiguous name table.  Current
                                        // behavior of NMT is contiguous.

#pragma pack(push, 1)
        typedef struct {
            IMOD imod;
            DWORD dwDataCrc;
            DWORD dwRelocCrc;
            CB  cb;
            DWORD dwCharacteristics;
        } IModSec;
#pragma pack(pop)

        enum {impv = DBIImpv};

        // unchange since open
        PDB1*   ppdb1;                  // PDB that opened me

        // used only when fWrite, always exclusive write for DBI
        OTM*    potmTSHead;             // list of open TMTSs
        OTM*    potmPCTHead;            // list of open TMPCTs

        // protected by m_csForMods;
        IMOD    imodMac;                // next available IMOD

#ifndef PDB_MT
        IMOD    imodLastQNM;            // imod last found by QueryNextMod
        IMOD    imodLastFMN;            // imod last found by imodForModName
#endif

        // protected by m_csForMods;
        Buffer  bufGpmodi;              // buffer backing gpmodi, the catenation of the modi
        Buffer  bufRgpmodi;             // buffer backing rgpmodi, to map imod to pmodi

        // protected by m_csSecContribs
        Buffer  bufSC;                  // buffer for section contribution info

        // unchanged since open/init
        OFF     offSC;                  // offset in dbistream where sec contribs begin

        // protected by m_csSecContribs - used only by DBI1::QueryAddrForSec
        // Maps from IModSec to index in seccontribs.
        // The first one has zero characteristics in the key.
        // Both used by EnC.
        typedef HashClassCRC<IModSec> hcSC;
        Map<IModSec, DWORD, hcSC> mpSecToSC_ZeroChar;
        Map<IModSec, DWORD, hcSC> mpSecToSC;

        // Used by QueryAddrForSecEx() and QueryAddrForSec()
        BOOL DBI1::fCreateSecToSCMap(Map<IModSec, DWORD, hcSC> & map, IMOD imod, bool fZeroChar);

        // if in write mode, it's always exclusive
        // if in read mode, bufSecMap doesn't change after fInit
        Buffer  bufSecMap;              // buffer for section map

        // protected by m_csForMods
        Buffer  bufFilenames;           // buffer of modules' contributor filenames

        // protected by m_csSecContribs
        PB     pbscEnd;                 // end of valid SC entries
        bool   fSCv2;                   // sec contrib entry contains coff section #

        // protected by m_csForMods
        MODI**  rgpmodi;                // module information for imod in [0..imodMac).

        // protected by m_csForPGSI
        GSI1*   pgsiGS;                 // gsi of global syms
        PSGSI1* pgsiPS;                 // gsi of public syms

        // protected by m_csForSymRec
        VirtualBuffer  bufSymRecs;      // buffer for symbol recs (publics and globals)
        BITVEC* pbvSymRecPgs;           // bit vector that gives the pages loaded
        unsigned cSymRecPgs;            // count of pages of symrecs

#ifdef PDB_DEFUNCT_V8
        size_t  m_cbSymRecs;            // cached count of bytes of symbols in stream.
#endif

        Array< FRAMEDATA >
                    m_rgFrameData;  // array of frame data from Dbg newFPO stream
        Array< MemBlock >
                    m_rgrvaRemovals;// array of frame data from Dbg newFPO stream
        Dbg*        pdbgFrameData;

        // protected by mulitple CS
        NewDBIHdr   dbihdr;             // new header

        // protected by m_csForDbg
        DbgDataHdr  dbghdr;             // debug data header (for FPO, OMAP, FIXUP, etc)

        // protected by m_csForSymRec (except for using cvtsyms.fConverting(), which is unchanged since fInit)
        CvtSyms     cvtsyms;            // info for conversions
        WidenTi *   pwti;               // our conversion interface
        ISet        isetModConverted;   // keep track of modules that have been converted
                                        // already.
#ifdef PDB_TYPESERVER
        InBuf<TSM>  bufTSMap;           // buffer for type server map
        ArrayPtrBuf<TPI>
                    bufTSMServer;       // buffer for type server pointers
        OPDB *      popdbHead;                  // list of open type server pdb's
        PfnFindDebugInfoFile m_pfnFindDIF;      // callback to help find files
#endif

        // protected by m_csForMods
        NMT         nmtEC;

        static const char
                    c_szLinkInfoStr[];  // stream name where we store LinkInfo

#ifdef PDB_TYPESERVER
        BOOL fInsertPdb(PDB* ppdb, TPI* ptpi)     // insert a new pdb,tpi
        {
            return (popdbHead = new OPDB(ppdb, ptpi, popdbHead)) != 0;
        }
        void ClosePdbs()
        {
            for (OPDB* popdb = popdbHead; popdb != 0; popdb = popdbHead) {
                popdbHead = popdb->next;
                delete popdb;
            }
        }
        BOOL FindValidate(const char *szPDB, const char *szObjFile);
        ITSM itsmMfcFromSzPdbName(SZ_CONST szPdb, UINT iTsmCur);
#endif

        // protected by m_csForMods - used only in DBI1::addFilename
        NMT_CTG nmtFileInfo;            // so we can _quickly_ reload/add files!

        // unchanged since open/init
        BOOL    fWrite;
        BOOL    fSCCleared;
        bool    m_fCTypes;
        bool    m_fTypeMismatch;

        // protected by m_csForFrameData
        BOOL    fFrameDataLoaded;
#ifdef _DEBUG
        bool fFrameDataAdded;
#endif

#ifdef PDB_DEFUNCT_V8
        BOOL fGlobalsConverted;         // TRUE if this is 6.0 PDB and
                                        // we have converted globals to 7.0
#endif

        // protected by m_csForMods - used by DBI1::imodForModNameW
        Buffer bufNames;             // Unicode module names
        struct ModNames {
            size_t  offwszName;
            size_t  offwszFile;
        };
        Array< ModNames >   rgModNames;

#ifdef PDB_MT
        mutable CriticalSection m_csSecContribs;    
        mutable CriticalSection m_csForPGSI;
        mutable CriticalSection m_csForNameMap;
        mutable CriticalSection m_csForMods;
        mutable CriticalSection m_csForSymRec;
        mutable CriticalSection m_csForHdr;
        mutable CriticalSection m_csForDbg;
        mutable CriticalSection m_csForFrameData;
        mutable CriticalSection m_csForTpi;
        mutable CriticalSection m_csForIpi;
        mutable CriticalSection m_csForOTM;
        mutable CriticalSection m_csForTmd;
#endif

        Dbg1Data *rgOpenedDbg[dbgtypeMax];
        BOOL fDbgDataVerified[dbgtypeMax];

        // verify the debug data -- so far only called by DBI1::fGetDbgData() in dbi.cpp
        BOOL VerifyDbgData(Buffer * buf, int dbgtype)
        {
            if (fDbgDataVerified[dbgtype])
                return TRUE;

            PB pbStart = buf->Start(), pbEnd = buf->End();

            switch(dbgtype)
            {
                case dbgtypeNewFPO:
                {
                    NameMap * pnmap = ppdb1->m_pnamemap;

                    // If we haven't read in name map, skip verifying now.
                    if (!pnmap)
                        break;

                    fDbgDataVerified[dbgtype] = TRUE;

                    PB pb = pbStart;
                    while(pb != pbEnd) {
                        NI ni = ((FRAMEDATA *)pb)->frameFunc;
                        if (!pnmap->isValidNi(ni)) {
                            return FALSE;
                        }
                        pb += sizeof(FRAMEDATA);
                        if (pb > pbEnd) {
                            return FALSE;
                        }
                    }

                    return TRUE;
                }

                case dbgtypeFPO:
                case dbgtypeException:
                case dbgtypeFixup:
                case dbgtypeOmapToSrc:
                case dbgtypeOmapFromSrc:
                case dbgtypeSectionHdr:
                case dbgtypeTokenRidMap:
                case dbgtypeXdata:
                case dbgtypePdata:
                case dbgtypeSectionHdrOrig:
                default:
                    break;
            }
            return TRUE;
        }

        // protected by m_csForNameMap
        AutoClosePtr<NameMap> m_pNameMap;

        // used in Mod1::processC13Strings
        BOOL GetNameMap(const BOOL fWrite, NameMap **pNameMap)
        {
            MTS_PROTECT(m_csForNameMap);

            if (!m_pNameMap && !NameMap::open(ppdb1, fWrite, &m_pNameMap)) {
                return FALSE;
            }

            *pNameMap = (NameMap *)m_pNameMap;
            return TRUE;
        }

        TPI * ptpi;

        TPI * GetTpi() {
            MTS_PROTECT(m_csForTpi);
            if (ptpi == NULL) {
                if (!ppdb1->OpenTpi(fWrite? pdbWrite : pdbRead, &ptpi)) {
                    return NULL;
                }
            }
            return ptpi;
        }

        TPI * pipi;

        TPI * GetIpi() {
            MTS_PROTECT(m_csForIpi);
            if (pipi == NULL) {
                if (!ppdb1->OpenIpi(fWrite? pdbWrite : pdbRead, &pipi)) {
                    return NULL;
                }
            }
            return pipi;
        }

        // used in DBI1::fInit
        BOOL reloadFileInfo(PB pb, CB cbReloadBuf);

        BOOL clearDBI();
        BOOL writeSymRecs();

#ifdef PDB_DEFUNCT_V8
        BOOL finalizeSC(OUT CB* cbOut);
#endif

        BOOL getSecContribs(PB *ppbStart);

        BOOL fReadSymRecPage (size_t iPg);
        inline BOOL decRefCntGS (OFF off);

        struct SrcMap {
            wchar_t *   szFrom;
            size_t      cchszFrom;
            wchar_t *   szTo;
            size_t      cchSzTo;
            DWORD       grFlags;
        };

        Array<SrcMap>   m_rgsrcmap;
        bool    fSrcMap(const wchar_t * szFile, Buffer & buf);
        
        size_t  cSrcMapEntries() const { return m_rgsrcmap.size(); }

        struct DBICallBackContext {
            void *                 pvContext;
            PFNDBIQUERYCALLBACK    pfnQueryCallback;
            PFNNOTEPDBUSED         pfnNotePdbUsed;
            PFNNOTETYPEMISMATCH    pfnNoteTypeMismatch;
            PFNTMDTYPEFILTER       pfnTmdTypeFilter;
        } m_dbicbc;

        POOL_AlignNative  poolTmd;  // pool for type mismatch detection
        PTMD *            m_rgpTmdBuckets;
        USHORT            m_tmdImod;
        TM *              m_tmdPtm;
        TI                m_tmdTiObj;

        BOOL   FTypeMismatchDetection(_In_z_ SZ szUdt, TI tiUdt, LHASH hash);

        friend Mod1;
        friend TM;
        friend TMTS;
        friend TMR;
        friend TMPCT;
        friend GSI1;
        friend PSGSI1;
        friend EnumSC<SC>;
        friend EnumSC<SC2>;
        friend Dbg1;
        friend Dbg1Data;
        friend TPI1;
       
#if defined(INSTRUMENTED)
        LOG log;

        struct DBIInfo {
            DBIInfo() { memset(this, 0, sizeof *this); }
            int cModules;
            int cSymbols;
            int cTypesMapped;
            int cTypesMappedRecursively;
            int cTypesQueried;
            int cTypesAdded;
            int cTMTS;
            int cTMR;
            int cTMPCT;
        } info;
#endif
};


#pragma warning(pop)

struct MODI50 {
        unsigned __int32 pmod;                  // currently open mod
        SC40 sc;                      // this module's first section contribution
        USHORT fWritten : 1;        // TRUE if mod has been written since DBI opened
        USHORT unused: 7;           // spare
        USHORT iTSM: 8;             // index into TSM list for this mods server
        SN sn;                      // SN of module debug info (syms, lines, fpo), or snNil
        CB cbSyms;                  // size of local symbols debug info in stream sn
        CB cbLines;                 // size of line number debug info in stream sn
        CB cbFpo;                   // size of frame pointer opt debug info in stream sn
        IFILE ifileMac;             // number of files contributing to this module
        unsigned __int32 mpifileichFile;        // array [0..ifileMac) of offsets into dbi.bufFilenames
        char rgch[];                // szModule followed by szObjFile

        SZ szModule() const { return (SZ)rgch; }
        SZ szObjFile() const { return (SZ)rgch + strlen(szModule()) + 1; }
        PB pbEnd() const {
                SZ szObjFile_ = szObjFile();
                return pbAlign(PB(szObjFile_) + strlen(szObjFile_) + 1);
        }
};

struct MODI;    // fwd ref

struct MODI_60_Persist              // version of MODI persisted in win32, v6.
{
        unsigned __int32    pmod;

        SC sc;                      // this module's first section contribution
        USHORT fWritten : 1;        // TRUE if mod has been written since DBI opened
        USHORT fECEnabled : 1;      // TRUE if mod has EC symbolic information
        USHORT unused: 6;           // spare
        USHORT iTSM: 8;             // index into TSM list for this mods server
        SN sn;                      // SN of module debug info (syms, lines, fpo), or snNil
        CB cbSyms;                  // size of local symbols debug info in stream sn
        CB cbLines;                 // size of line number debug info in stream sn
        CB cbC13Lines;              // size of C13 style line number info in stream sn
        IFILE ifileMac;             // number of files contributing to this module

        unsigned __int32    mpifileichFile;        // array [0..ifileMac) of offsets into dbi.bufFilenames

        struct ECInfo {
            NI      niSrcFile;            // NI for src file name
            NI      niPdbFile;            // NI for path to compiler PDB
        }   ecInfo;
        char rgch[];                // szModule followed by szObjFile

        SZ szModule() const { return (SZ)rgch; }
        SZ szObjFile() const { return (SZ)rgch + strlen(szModule()) + 1; }
        PB pbEnd() const {
                SZ szObjFile_ = szObjFile();
                return pbAlign(PB(szObjFile_) + strlen(szObjFile_) + 1);
        }

        MODI_60_Persist & operator = (const MODI &);
};

typedef MODI_60_Persist *   PMODI60;

struct MODI
{
        Mod* pmod;                  // currently open mod
        SC sc;                      // this module's first section contribution
        USHORT fWritten : 1;        // TRUE if mod has been written since DBI opened
        USHORT fECEnabled : 1;      // TRUE if mod has EC symbolic information
        USHORT unused: 6;           // spare
        USHORT iTSM: 8;             // index into TSM list for this mods server
        SN sn;                      // SN of module debug info (syms, lines, fpo), or snNil
        CB cbSyms;                  // size of local symbols debug info in stream sn
        CB cbLines;                 // size of line number debug info in stream sn
        CB cbC13Lines;              // size of C13 style line number info in stream sn
        IFILE ifileMac;             // number of files contributing to this module
        ICH* mpifileichFile;        // array [0..ifileMac) of offsets into dbi.bufFilenames
        struct ECInfo {
            NI      niSrcFile;            // NI for src file name
            NI      niPdbFile;            // NI for path to compiler PDB
        }   ecInfo;
        char rgch[];                // szModule followed by szObjFile

        bool isECEnabled() const { return fECEnabled != 0; }
        NI niECSrcFile() const { return ecInfo.niSrcFile; }
        NI niECPdbFile() const { return ecInfo.niPdbFile; }
        void* operator new (size_t size, Buffer& bufGpmodi, SZ_CONST szModule, SZ_CONST szObjFile);
        MODI(SZ_CONST szModule, SZ_CONST szObjFile);
        ~MODI();
        SZ szModule() const { return (SZ)rgch; }
        SZ szObjFile() const { return (SZ)rgch + strlen(szModule()) + 1; }
        PB pbEnd() const {
                SZ szObjFile_ = szObjFile();
                return pbAlignNative(PB(szObjFile_) + strlen(szObjFile_) + 1);
        }

        MODI(const MODI50& modi50)
        {
            pmod = 0;
            sc = modi50.sc;
            fWritten = modi50.fWritten;
            fECEnabled = 0;
            unused = 0;
            iTSM = modi50.iTSM;
            sn = modi50.sn;
            cbSyms = modi50.cbSyms;
            cbLines = modi50.cbLines;
            cbC13Lines = 0;           // field was formerly cbFpo and was never used in 50 pdbs
            ifileMac = modi50.ifileMac;
            mpifileichFile = 0;
            ecInfo.niSrcFile = 0;
            ecInfo.niPdbFile = 0;
            memcpy(rgch, modi50.rgch, size_t(SZ(modi50.pbEnd())-modi50.rgch));    // REVIEW:WIN64 CAST
        }

        MODI(const MODI_60_Persist & modi)
        {
            pmod = 0;
            sc = modi.sc;
            fWritten = modi.fWritten;
            fECEnabled = modi.fECEnabled;
            unused = 0;
            iTSM = modi.iTSM;
            sn = modi.sn;
            cbSyms = modi.cbSyms;
            cbLines = modi.cbLines;
            cbC13Lines = modi.cbC13Lines;
            ifileMac = modi.ifileMac;
            mpifileichFile = 0;
            ecInfo.niSrcFile = modi.ecInfo.niSrcFile;
            ecInfo.niPdbFile = modi.ecInfo.niPdbFile;
            memcpy(rgch, modi.rgch, size_t(SZ(modi.pbEnd())-modi.rgch));    // REVIEW:WIN64 CAST
        }
};

typedef MODI *  PMODI;

// Maximum delta between a persisted MODI and an internal one, as laid out
// in bufGpmodi (aligned to sizeof(void*))
const size_t    dcbModi60 =
    // obviously has to include this
    sizeof(MODI) - sizeof(MODI_60_Persist)
    // this is for the max delta between the 4-byte aligned persisted MODIs
    // and the 8-byte win64 internal aligned MODIs.  All of the 4 byte aligned
    // records had at most 3 bytes added, and the 8 byte ones will have at
    // most 7 bytes added, netting 4 bytes extra at most per record.
    + sizeof(_md_int_t) - sizeof(_file_align_t);

inline MODI_60_Persist & MODI_60_Persist::operator = (const MODI & modi) {
    pmod = 0;
    sc = modi.sc;
    fWritten = modi.fWritten;
    fECEnabled = modi.fECEnabled;
    unused = 0;
    iTSM = modi.iTSM;
    sn = modi.sn;
    cbSyms = modi.cbSyms;
    cbLines = modi.cbLines;
    cbC13Lines = modi.cbC13Lines;
    ifileMac = modi.ifileMac;
    mpifileichFile = 0;
    ecInfo.niSrcFile = modi.ecInfo.niSrcFile;
    ecInfo.niPdbFile = modi.ecInfo.niPdbFile;
    memcpy(rgch, modi.rgch, size_t(SZ(modi.pbEnd())-modi.rgch));    // REVIEW:WIN64 CAST
    return *this;
}

inline void* MODI::operator new(size_t size, Buffer& bufGpmodi, SZ_CONST szModule, SZ_CONST szObjFile)
{
    if (!szModule)
            return 0;
    if (!szObjFile)
            szObjFile = "";
    PB pb;
    size_t cb = cbAlignNative(size + strlen(szModule) + 1 + strlen(szObjFile) + 1);
    return bufGpmodi.Reserve(CB(cb), &pb) ? pb : 0;
}

inline MODI::MODI(SZ_CONST szModule_, SZ_CONST szObjFile_)
    : pmod(0), fWritten(FALSE), fECEnabled(FALSE), sn(snNil), cbSyms(0),
      cbLines(0), cbC13Lines(0), ifileMac(0), mpifileichFile(0), iTSM(0)
{
    expect(fAlignNative(this));
    ecInfo.niSrcFile = niNil;
    ecInfo.niPdbFile = niNil;
    memcpy(szModule(), szModule_, strlen(szModule_) + 1);
    memcpy(szObjFile(), szObjFile_, strlen(szObjFile_) + 1);
}

inline MODI::~MODI()
{
    if(mpifileichFile)
        delete [] mpifileichFile;

    mpifileichFile = 0;
}


// REVIEW : We should really be using RefCountedObjects here. 
class Dbg1Data {
public:
    Dbg1Data (DBI1 *pdbi1_, int iDbg_, ULONG cbElement_, PFN_DBGCMP pfnCmp_) :
        pdbi1(pdbi1_), iDbg(iDbg_), cbElement(cbElement_), pfnCmp(pfnCmp_), refcount (0),
        // Buffer gets no pad on first alloc, pad subsequent allocs for write mode appends
        bufDbg(0, 0, false, true)
    {
    }

    ULONG AddRef() { 
        MTS_PROTECT(m_cs);
        return ++refcount;
    }

    BOOL Close();
    long QuerySize();
    BOOL QueryRange(ULONG iCur, ULONG celt, void *rgelt);
    BOOL Find(void *pelt);
    BOOL Clear();
    BOOL Append(ULONG celt, const void *rgelt);
    BOOL Replace(ULONG iCur, ULONG celt, const void *rgelt);
    long QueryElementSize() {
        return cbElement;
    }

    Buffer &GetBuffer() {
        return bufDbg;
    }
protected:

    BOOL Sort();
    BOOL fSave();

    DBI1 *     pdbi1;
    ULONG      iDbg;             // index to rgDbg array
    ULONG      cbElement;        // size of a single element in bytes
    PFN_DBGCMP pfnCmp;           // data comparison function
    Buffer     bufDbg;
    ULONG      refcount;
#ifdef PDB_MT
    mutable CriticalSection m_cs;
#endif
};

class Dbg1: public Dbg {
public:
    Dbg1(DBI1 *pdbi1, int iDbg, ULONG cbElement, PFN_DBGCMP pfnCmp) 
        : m_pDbgData(NULL), m_iCur(0) 
    {
        m_pDbgData = pdbi1->fGetDbgData(iDbg, cbElement, pfnCmp);
    }

    BOOL fInit() {
        return m_pDbgData != NULL && m_pDbgData->AddRef();
    }
    BOOL Close() {
        BOOL fResult = m_pDbgData->Close();
        delete this;
        return fResult;
    }
    long QuerySize() {
        return m_pDbgData->QuerySize();
    }
    void Reset() {
        m_iCur = 0;
    }
    BOOL Skip(ULONG celt) {
        if (celt > nRemaining())
            return FALSE;
        m_iCur += celt;
        return TRUE;
    }
    BOOL QueryNext(ULONG celt, void *rgelt) {
        if (celt == 0) {
            return TRUE;    // trivially true
        }
        else if (celt > nRemaining()) {
            return FALSE;
        }
        return m_pDbgData->QueryRange(m_iCur, celt, rgelt);
    }
    BOOL Find(void *pelt) {
        return m_pDbgData->Find(pelt);
    }
    BOOL Clear() {
        if (m_pDbgData->Clear()) {
            Reset();
            return TRUE;
        }
        return FALSE;
    }
    BOOL Append(ULONG celt, const void *rgelt) {
        return m_pDbgData->Append(celt, rgelt);
    }
    BOOL ReplaceNext(ULONG celt, const void *rgelt) {
        if (celt > nRemaining()) {
            return FALSE;
        }
        return m_pDbgData->Replace(m_iCur, celt, rgelt);
    }
    BOOL Clone(Dbg **ppDbg) {
        // UNDONE: When this is clone in read/write mode, 
        // the m_iCur might not be in sync with the actual data size
        return (*ppDbg = new Dbg1(*this)) != NULL;
    }
    long QueryElementSize() {
        return m_pDbgData->QueryElementSize();
    }
 
protected:
    Dbg1Data *m_pDbgData;       // Reference to the real data (REVIEW: should really be a RefPtr<>)
    ULONG     m_iCur;           // enumeration index

    Dbg1(Dbg1 const& dbg) : m_pDbgData(dbg.m_pDbgData), m_iCur(dbg.m_iCur) {
        m_pDbgData->AddRef();
    }
    ULONG nRemaining() {
        return QuerySize() - m_iCur;
    }
};


class DbgFpo1 : public Dbg1 {
public:
    DbgFpo1(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeFPO, sizeof(FPO_DATA), cmpDbgFpo) {}
private:
    static int __cdecl cmpDbgFpo (const void* elem1, const void* elem2) {
        FPO_DATA *p1 = (FPO_DATA *)elem1;
        FPO_DATA *p2 = (FPO_DATA *)elem2;
        return p1->ulOffStart - p2->ulOffStart;
    }
};

class DbgFunc1 : public Dbg1 {
public:
    DbgFunc1(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeException, sizeof(IMAGE_FUNCTION_ENTRY), cmpDbgFunc) {}
private:
    static int __cdecl cmpDbgFunc (const void* elem1, const void* elem2)    {
    IMAGE_FUNCTION_ENTRY *p1 = (IMAGE_FUNCTION_ENTRY *)elem1;
    IMAGE_FUNCTION_ENTRY *p2 = (IMAGE_FUNCTION_ENTRY *)elem2;
    return p1->StartingAddress - p2->StartingAddress;
    }
};

class DbgXFixup1 : public Dbg1 {
public:
    DbgXFixup1(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeFixup, sizeof(XFIXUP_DATA), NULL) {}
};

class DbgOmap1 : public Dbg1 {
public:
    DbgOmap1(DBI1 *pdbi1, BOOL fToSrc) :
      Dbg1(pdbi1,
          (fToSrc ? dbgtypeOmapToSrc : dbgtypeOmapFromSrc),
          sizeof(OMAP_DATA),
          cmpDbgOmap) {}
private:
    static int __cdecl cmpDbgOmap (const void* elem1, const void* elem2)    {
        OMAP_DATA *p1 = (OMAP_DATA *)elem1;
        OMAP_DATA *p2 = (OMAP_DATA *)elem2;
        return p1->rva - p2->rva;
    }
};

class DbgSect1 : public Dbg1 {
public:
    DbgSect1(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeSectionHdr, sizeof(IMAGE_SECTION_HEADER), NULL) {}
};


class DbgTokenRidMap : public Dbg1 {
public:
    DbgTokenRidMap(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeTokenRidMap, sizeof(ULONG), NULL) {}
};

// XData and PData are blobs
//
class DbgXdata : public Dbg1 {
public:
    DbgXdata(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeXdata, 1, NULL) { }
};

class DbgPdata : public Dbg1 {
public:
    DbgPdata(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypePdata, 1, NULL) { }
};

class DbgFrameData1 : public Dbg1 {
public:
    DbgFrameData1(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeNewFPO, sizeof(FRAMEDATA), cmpDbgFrameData) {}
private:
    static int __cdecl cmpDbgFrameData (const void* elem1, const void* elem2) {
        FRAMEDATA *p1 = (FRAMEDATA *)elem1;
        FRAMEDATA *p2 = (FRAMEDATA *)elem2;
        return p1->ulRvaStart - p2->ulRvaStart;
    }
};

class DbgSectOrig : public Dbg1 {
public:
    DbgSectOrig(DBI1 *pdbi1) : Dbg1(pdbi1, dbgtypeSectionHdrOrig, sizeof(IMAGE_SECTION_HEADER), NULL) {}
};

#pragma warning(pop)
