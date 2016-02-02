//////////////////////////////////////////////////////////////////////////////
// TPI implementation declarations

#include "SymTypeUtils.h"

#pragma warning(push)
#pragma warning(disable:4200)

enum {intvVC2 = 920924};

struct HDR_16t { // type database header, 16-bit types:
    IMPV    vers;           // version which created this TypeServer
    TI16    tiMin;          // lowest TI
    TI16    tiMac;          // highest TI + 1
    CB      cbGprec;        // count of bytes used by the gprec which follows.
    SN      snHash;         // stream to hold hash values
    // rest of file is "REC gprec[];"
};

struct OffCb {  // offset, cb pair
    OFF off;
    CB  cb;
};

struct TpiHash {
    SN      sn;             // main hash stream
    SN      snPad;          // auxilliary hash data if necessary
    CB      cbHashKey;      // size of hash key
    unsigned __int32
            cHashBuckets;   // how many buckets we have
    OffCb   offcbHashVals;  // offcb of hashvals
    OffCb   offcbTiOff;     // offcb of (TI,OFF) pairs
    OffCb   offcbHashAdj;   // offcb of hash head list, maps (hashval,ti),
                            //  where ti is the head of the hashval chain.
};

struct HDR_VC50Interim {    // used for converting the interim v5 version
    IMPV    vers;           // to the current version.
    TI      tiMin;
    TI      tiMac;
    CB      cbGprec;
    SN      snHash;
};

struct HDR { // type database header:
    IMPV    vers;           // version which created this TypeServer
    CB      cbHdr;          // size of the header, allows easier upgrading and
                            //  backwards compatibility
    TI      tiMin;          // lowest TI
    TI      tiMac;          // highest TI + 1
    CB      cbGprec;        // count of bytes used by the gprec which follows.
    TpiHash tpihash;        // hash stream schema

    HDR &
    operator=(const HDR_16t & hdr16);

    HDR &
    operator=(const HDR_VC50Interim & hdrvc50);
    // rest of file is "REC gprec[];"
};


struct OHDR { // old C8.0 types-only program database header:
    char    szMagic[0x2C];
    INTV    vers;           // version which created this file
    SIG     sig;            // signature
    AGE     age;            // age (no. of times written)
    TI16    tiMin;          // lowest TI
    TI16    tiMac;          // highest TI + 1
    CB      cb;             // count of bytes used by the gprec which follows.
    // rest of file is "REC gprec[];"
};


const size_t    cbHdrMin = sizeof(HDR_16t);
struct TI_OFF_16t {
    TI16    ti;             // first ti at this offset
    OFF     off;            // offset of ti in stream
    TI_OFF_16t(){
        ti = 0;
        off = 0;
    }
    TI_OFF_16t(TI16 _ti, OFF _off){
        ti = _ti;
        off = _off;
    }
};



struct TI_OFF {
    TI  ti;                     // first ti at this offset
    OFF off;                    // offset of ti in stream
    TI_OFF(){
        ti = 0;
        off = 0;
    }
    TI_OFF(TI _ti, OFF _off){
        ti = _ti;
        off = _off;
    }
};


// UDT name hashing relies upon the following ordering and spacing
// of type records.
cassert(LF_CLASS+1 == LF_STRUCTURE);
cassert(LF_STRUCTURE+1 == LF_UNION);

#define cbDWAlign(cb)   (((cb) + 3) & ~3)

struct REC { // type record:
    BYTE    buf[];          // record contents
    void* operator new(size_t size, TPI1* ptpi1, PB pb);
#ifdef PDB_DEFUNCT_V8
    void* operator new(size_t size, TPI1* ptpi1, PC8REC pc8rec);
#endif
    REC(PB pb)
    {
        expect(fAlign(this));
        dassert(fAlign(cbForPb(pb)));
        memcpy(buf, pb, cbForPb(pb));
    }
    static BOOL fIsDefnUdt(PB pb);
    static BOOL fIsGlobalDefnUdt(PB pb);
    static BOOL fIsLocalDefnUdtWithUniqueName(PB pb);
    static BOOL fIsUdtSrcLine(PB pb);

    BOOL fSame(PB pb)
    {
        return cb() == cbForPb(pb) && memcmp(buf, pb, cbForPb(pb)) == 0;
    }

    BOOL fSameUDT(SZ_CONST sz, BOOL fCase);
    BOOL fSameUdtSrcLine(TI ti, unsigned short leaf);

    static HASH hashUdtName(PB pb);
    CBREC cb() { return cbForPb(buf); }
    static CBREC cbForPb(PB pb) { return *(CBREC*)pb + sizeof(CBREC); }
};

#ifdef PDB_DEFUNCT_V8
// UNDONE: No one seems to use it, probably can remove it
struct C8REC { // type record:
    HASH    hash;           // hash of record
    BYTE    buf[];          // record contents

    C8REC(PB pb, HASH hash_) : hash(hash_) { memcpy(buf, pb, cbForPb(pb)); }
    BOOL fSame(PB pb, HASH hash_) {
        return hash == hash_ && memcmp(buf, pb, cbForPb(pb)) == 0;
    }
    CBREC cb() { return cbForPb(buf); }
    static CBREC cbForPb(PB pb) { return *(CBREC*)pb + sizeof(CBREC); }
};
#endif

struct CHN { // chain of type records within one hash bucket:
    PCHN    pNext;      // next chain element
    TI      ti;         // this link's records' TI

    CHN(PCHN pNext_, TI ti_) : pNext(pNext_), ti(ti_)
    {
        expect(fAlign(this));
    }
};

struct TPI1 : public TPI { // type info:
public:
    enum {
        impv40 = 19950410,
        impv41 = 19951122,
        impv50Interim = 19960307,
        impv50 = 19961031,
        impv70 = 19990903,
        impv80 = 20040203,
        curImpv = impv80,
    };

    // Structure to cache full hash of the record. This allows us to skip records
    // that are in the same bucket because of a clash as a result of
    // 1. truncating the hash to fit the hash table
    // 2. hash generated using partial information from the record - UDT or source line record
    // Store the EX structure in Array. The hash is calculated as soon as the item is added to array
    struct PRECEX
    {
        PRECEX() : recHash(), prec()
        {
        }
        PRECEX(const PRECEX& other) : recHash(other.recHash), prec(other.prec)
        {
        }
        
        PRECEX& operator=(const PRECEX& other)
        {
            recHash = other.recHash;
            prec = other.prec;
            return *this;
        }
        PRECEX(PRECEX&& other) : recHash(other.recHash), prec(other.prec)
        {
            other.recHash = 0;
            other.prec = nullptr;
        }
        PRECEX& operator=(PRECEX&& other)
        {
            recHash = other.recHash;
            prec = other.prec;
            other.recHash = 0;
            other.prec = nullptr;
            return *this;
        }
        PRECEX(PREC p) : recHash(), prec(p)
        {
            if (prec != nullptr)
            {
                recHash = hashPrecFull(prec);
            }
        }
        PRECEX& operator=(PREC p)
        {
            prec = p;
            if (prec != nullptr)
            {
                recHash = hashPrecFull(prec);
            }
            return *this;
        }
        LHASH recHash;
        PREC prec;
    };

    INTV QueryInterfaceVersion();
    IMPV QueryImplementationVersion();
    BOOL QueryTiForCVRecord(PB pb, OUT TI* pti);
    BOOL QueryCVRecordForTi(TI ti, PB pb, CB* pcb);
    BOOL QueryPbCVRecordForTi(TI ti, OUT PB* ppb);
    virtual BOOL Close();
    BOOL Commit();

    // MTS NOTE: tiMin never changes after creation
    TI   QueryTiMin() { return tiMin(); }
    TI   QueryTiMac() { 
        MTS_PROTECT(m_cs);  
        return tiMac(); 
    }

    CB   QueryCb()    { 
        MTS_PROTECT(m_cs);     
        return CB(cbTotal()); 
    }

    BOOL QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti);
    BOOL QueryTiForUDTW(const wchar_t *wcs, BOOL fCase, OUT TI* pti);

    BOOL QueryModSrcLineForUDT(const TI tiUdt, unsigned short *pimod, _Deref_out_z_ char **psz, DWORD *pLine);
    BOOL QuerySrcIdLineForUDT(const TI tiUdt, TI& srcId, DWORD& line);
    BOOL QueryModSrcLineForUDTDefn(const TI tiUdt, unsigned short *pimod, TI *pSrcId, DWORD *pLine);

    BOOL GetStringForId(const TI id, _Inout_opt_ char *sz, size_t *pcb, _Out_opt_ char **pszRaw,
                        TM *ptm = NULL, PTYPE (*pfn)(TM *, TI) = NULL);

    BOOL SupportQueryTiForUDT(void)
    {
        return (fEnableQueryTiForUdt);
    }
    // old 16-bit TI apis are now stubbed out.
    BOOL QueryTi16ForCVRecord(PB pb, OUT TI16* pti) { return fSetTi16Err(); }
    BOOL QueryCVRecordForTi16(TI16 ti, PB pb, CB* pcb) { return fSetTi16Err(); }
    BOOL QueryPbCVRecordForTi16(TI16 ti, OUT PB* ppb) { return fSetTi16Err(); }
    TI16 QueryTi16Min() { return TI16(min(::ti16Max, tiMin())); }
    TI16 QueryTi16Mac() { 
        MTS_PROTECT(m_cs);
        return TI16(min(::ti16Max, tiMac())); 
    }
    BOOL QueryTi16ForUDT(const char *sz, BOOL fCase, OUT TI16* pti) { return fSetTi16Err(); }

    // there are a couple of new apis for detecting 16-bit vs 32-bit type pools
    BOOL fIs16bitTypePool()
    {
        return f16bitPool;
    }
#ifdef PDB_TYPESERVER
    void UseTsm( ITSM itsm_, DBI1* pdbi_ ) {  
        itsm = itsm_; pdbi = pdbi_; 
    }
#endif
    BOOL AreTypesEqual( TI ti1, TI ti2 );
    BOOL IsTypeServed( TI ti );

private:

#ifdef PDB_TYPESERVER
    ITSM itsm;
    DBI1* pdbi;

    TI InternalTi( TI ti ) { return pdbi ? pdbi->OriginalTi( ti ) : ti; }
    TI ExternalTi( TI ti ) { return pdbi ? pdbi->TiForTiItsm( ti, itsm ) : ti; }
    BOOL fHasExTi( TI ti ) { return (pdbi==0 || pdbi->ItsmFromTi( ti ) == itsm) && fHasTi( InternalTi( ti ) ); }
    void ConvertTypeRange( TYPTYPE* ptype, ITSM itsm = 0 );
    BOOL AlternateTS( TI ti, TPI** pptsi );
    BOOL FindTiFromAltTpi( TI rti, TPI* ptpiFrom, OUT TI* pti );
#endif // !PDB_TYPESERVER

    BOOL fIsModifiedClass(PTYPE ptA, PTYPE ptB);
    BOOL fIsMatchingType(TI tiA, TI tiB);

protected:

    virtual ~TPI1() {
        if (mphashpchn)
            delete [] mphashpchn;
    }

private:

    friend PDB1;
    TPI1(MSF* pmsf_, PDB1* ppdb1_, SN sn_);

    enum ACSL { // access level available with an open of existing TPI
        acslNone,               // no access, unknown IMPV
        acslRead,               // read only
        acslReadWriteConvert,   // read/write requires conversion
        acslReadWrite           // full read/write (current version)
    };

    BOOL    fOpen(SZ_CONST szMode);
    BOOL    fLoad();
    BOOL    fCreate();
    BOOL    fInit();
    BOOL    fInitReally();
    BOOL    fInitHashToPchnMap();
    BOOL    fRehashV40ToPchnMap();
    BOOL    fInitTiToPrecMap();
    BOOL    fLoadTiOff();
    BOOL    RecordTiOff (TI ti, OFF off);
    BOOL    fLoadRecBlk (TI ti);
    TI      tiForCVRecord(PB pb);
    PRECEX  precForTi(TI ti);
    void    cvRecordForTi(TI ti, PB pb, CB *pcb);
    BOOL    fCommit();
    ACSL    acslValidateHdr() const;

#if defined(NEVER)
    BOOL    fReplaceRecord(TI tiUdt, PREC precDst, PB pbSrc, CB cb);
    OFF     offRecFromTiPrec(TI, PREC);
    CB      cbForRecsInBlk(TI tiMin, TI tiLast);
#endif

    BOOL    fSetTi16Err()
    {
        ppdb1->setLastError(EC_TI16, "Cannot access 32-bit type pool with 16-bit APIs");
        return FALSE;
    }

    typedef LHASH (__fastcall * PFNTPIHASH)(PB, unsigned);

    static LHASH __fastcall hashBuf(PB pb, unsigned cBuckets)
    {
        return HashPbCb(pb, REC::cbForPb(pb), cBuckets);
    }

    static LHASH __fastcall hashBufv8(PB pb, unsigned cBuckets)
    {
        return SigForPbCb(pb, REC::cbForPb(pb), 0) % cBuckets;
    }


    LHASH hashUdtName(const char *sz) const
    {
        return hashSz(sz);
    }

    LHASH hashSz(SZ_CONST sz) const
    {
        size_t  cch = strlen(sz);
        return LHashPbCb((PB)sz, cch, hdr.tpihash.cHashBuckets);
    }

    LHASH hashTypeIndex(const TI ti) const
    {
        return LHashPbCb((PB)&ti, sizeof(TI), hdr.tpihash.cHashBuckets);
    }

    LHASH hashPrec(PREC prec) const;
    // Calculate full record hash for record
    // TODO: Make this non static
    static LHASH hashPrecFull(PREC prec);

    TI  tiMin()     { return hdr.tiMin; }
    TI  tiMac()     { 
        MTS_ASSERT(m_cs);
        return hdr.tiMac; 
    }
    TI  tiNext()
    { 
        MTS_ASSERT(m_cs);
        if(fValidTi(QueryTiMac()))
            return hdr.tiMac++;

        ppdb1->setLastError(EC_OUT_OF_TI);
        return T_VOID;
    }

    size_t  cbTotal()   { 
        MTS_ASSERT(m_cs);
        return poolRec.cb(); 
    }
    size_t  cbRecords() { return cbTotal() - (QueryTiMac()-QueryTiMin()) * sizeof(REC); }
    BOOL    fHasTi(TI ti)   { return (ti >= tiMin() && ti < tiMac()); }
    BOOL    fValidTi(TI ti) { return (ti >= ::tiMin && ti < ::tiMax); }

    BOOL getSrcInfoRecordIdForUDT(const TI tiUdt, unsigned short leaf, TI& id);

public:
    enum {
        cchnV7       =   0x1000, // for v7 and previous, we have 4k buckets
        cchnV8       =  0x3ffff, // default to 256k - 1buckets
        cprecInit    =   0x1000, // start with 4k prec pointers
        cchnMax      =  0x40000, // deliberate maximum bucket count == 256k
        };

private:
    // unchange since open/init
    PDB1*   ppdb1;          // used for error reporting
    MSF*    pmsf;           // our multistream file, 0 => loaded from C8.0 PDB

    CB      cbClean;        // orig hdr.cb

    // protected by m_cs
    Array<PRECEX>
            mptiprec;       // map from TI to record
    PCHN*   mphashpchn;     // map from record hash to its hash bucket chain
    POOL_Align4
            poolRec;        // REC pool (explicitly aligned to 4 bytes)
    BLK*    pblkPoolCommit; // last block in poolRec already committed
    POOL_Align4
            poolRecClean;   // REC pool (clean)
    POOL_AlignNative
            poolChn;        // CHN pool

#ifdef PDB_DEFUNCT_V8
    POOL_Align4
            poolC8Rec;      // REC pool c8 pdb records we have to align
#endif // PDB_DEFUNCT_V8

    // protected by m_cs
    Buffer  bufAlign;       // used to pack incoming unaligned records
    TI_OFF  tioffLast;      // last set of values
    Buffer  bufTiOff;       // buffer that holds all TI_OFF pairs

    // unchange since open/init
    int     cTiOff;         // count of original tioffs
    TI      tiCleanMac;     // highest TI for the clean pool

    BOOL    fWrite;         // TRUE => can modify the type database
    BOOL    fAppend;        // TRUE => can only add new types
    BOOL    fGetTi;         // TRUE => can query TIs given CV records
    BOOL    fGetCVRecords;  // TRUE => can query CV records given TI
    BOOL    fInitd;         // TRUE => fInit has ever been called
    BOOL    fInitResult;    // (only valid if fInitd); TRUE => fInit has succeeded
    BOOL    fReplaceHashStream; // TRUE when we have to completely rewrite the hash stream
    BOOL    fEnableQueryTiForUdt;   // TRUE when we have enabled the querytiforude capability

    // protected by m_cs 
    HDR     hdr;            // file header

    // unchange since open init
    CB      cbHdr;          // used since sizeof hdr is no longer appropriate to use
    BOOL    f16bitPool;     // TRUE => we are reading from a 16-bit type pool,
                            //  also implies that we are converting the records on the fly
    // protected by m_cs
    WidenTi *
            pwti;           // our api for widening the type records of 16-bit pools

    Map<NI,TI,HcNi>
            mpnitiHead;     // map the head of UDT chain to a TI.  Used
                            //  for when the UDT chain needs a particular TI
                            //  as the current type schema for a UDT.
    NameMap *
            pnamemap;       // used to handle UDT names that need a hash fixup

    mutable PFNTPIHASH
            m_pfnHashTpiRec;


#ifdef PDB_MT
    mutable CriticalSection
            m_cs;
#endif

#ifdef HASH_REPORT
    size_t  cRecComparisons;
    size_t  cRecUDTComparisons;
#endif

    SN      m_sn;

    BOOL getNameMap() {
        assert ( ppdb1 );
        MTS_ASSERT(m_cs);
        if (pnamemap == 0) {
            return NameMap::open(ppdb1, fWrite, &pnamemap);
        }
        return TRUE;
    }

    CB cbHashKey() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.cbHashKey;
    }

    OFF offHashValues() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbHashVals.off;
    }

    CB cbHashValues() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbHashVals.cb;
    }

    OFF offTiOff() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbTiOff.off;
    }

    CB cbTiOff() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbTiOff.cb;
    }

    CB cbHashAdj() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbHashAdj.cb;
    }

    OFF offHashAdj() const {
        MTS_ASSERT(m_cs);
        return hdr.tpihash.offcbHashAdj.off;
    }
    CB cbGpRec() const {
        MTS_ASSERT(m_cs);
        return hdr.cbGprec;
    }

    CB & cbGpRec() {
        MTS_ASSERT(m_cs);
        return hdr.cbGprec;
    }

    BOOL fEnsureSn(SN* psn)
    {
        return ppdb1->fEnsureSn(psn);
    }

    BOOL fEnsureNoSn(SN* psn)
    {
        return ppdb1->fEnsureNoSn(psn);
    }

    // protected by m_cs
    Buffer  bufMapHash;     // map of hash values to any type records we add on this pass
    CB      cbMapHashCommit;// length of hash value map already committed

    SN      snHash()        { 
        MTS_ASSERT(m_cs);
        return hdr.tpihash.sn; 
    }
    BOOL    fGetSnHash()
    {
        MTS_ASSERT(m_cs);
        return fEnsureSn(&(hdr.tpihash.sn));
    }

    PB GetAlignedCVRecord(PB pb);
    BOOL AddNewTypeRecord(PB pb, TI *pti);
    BOOL LookupNonUDTCVRecord(PB pb, TI *pti);
    BOOL fLegalTypeEdit(PB pb, TI ti);
    BOOL PromoteTIForUDT(_In_z_ SZ szName, PCHN *ppchnHead, PCHN pchnPrev, BOOL fAdjust, TI tiUdt);

    friend struct REC;
};

#pragma warning(pop)

