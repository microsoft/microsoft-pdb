//////////////////////////////////////////////////////////////////////////////
// GSI implementation declarations 

struct HR;

// The version of HR that we persist to disk.
//
struct HRFile {
    __int32 off;
    __int32 cRef;
    inline HRFile(const HR & hr);
};

struct HR {    
    HR*     pnext;
    PSYM    psym;
    int     cRef;

    HR(HR* pNext, PSYM psym_) : pnext(pNext), psym(psym_), cRef(1)
        { expect(fAlign(this)); }
    
    HR & operator=(const HRFile & hrf) {
        // NB:
        // The order is important here for the only place this is used assigns
        // them in reverse order in order to go from HRFile to HR in-place in
        // a single buffer.
        //
        cRef = hrf.cRef;
        psym = PSYM(long_ptr_t(hrf.off));   // NB: must sign extend
        // pnext = NULL;                    // persisted pnext is meaningless,
                                            // but we will assign to it immediately
                                            // after this operator=() is called.
        return *this;
    }

    void* operator new (size_t size, GSI1* pgsi1);
};

// the structure on which we base offsets that we persist.  in order to match
// what the format is for Win32 based on struct HR above, we need
// to save out offsets based on the HRPersist structure below instead,
// so that the databases have exactly the same offset.  This forces us to
// fix the offsets on Win64 when we read them in by the bias of
// sizeof(HR) - sizeof(HROffsetCalc)
//
struct HROffsetCalc {
    __int32 pnext;
    __int32 psym;
    __int32 cRef;
};

#ifdef SMALLBUCKETS

// header of a GSI Hash

// section contribution structure
enum {  // section contribution version, before V70 there was no section version
    GSIHashSCImpvV70 = 0xeffe0000 + 19990810,
    GSIHashSCImpv    = GSIHashSCImpvV70,
};

struct GSIHashHdr {
    enum {
        hdrSignature = -1,
        hdrVersion = GSIHashSCImpv,
    };

    ULONG       verSignature;
    ULONG       verHdr;
    CB          cbHr;
    CB          cbBuckets;
};
#endif

inline
HRFile::HRFile(const HR & hr) :
        off(OFF(long_ptr_t(hr.psym))), 
        cRef(hr.cRef)
{
    assert(PSYM(long_ptr_t(hr.psym) & 0xffffffff) == hr.psym);
}

typedef HR*     PHR;
typedef HRFile* PHRFile;
typedef HR**    PPHR;

#include "gsicommon.h"



struct GSI1 : public _GSI { 
public:
    INTV QueryInterfaceVersion();
    IMPV QueryImplementationVersion();
    PSYM psymForPhr (HR *);
    PB NextSym(PB pbSym);
    PB HashSym(SZ_CONST szName, PB pbSym);
    PB NearestSym (ISECT, OFF, OFF*) {
        assert(!pdbi1->fWrite);
        return NULL;      //only supported for publics gsi
    }
    BOOL getEnumThunk( ISECT isect, OFF off, EnumThunk** ppenum )
    {
        assert(!pdbi1->fWrite);
        return FALSE;
    }
    unsigned long OffForSym( BYTE* pbSym );
    BYTE* SymForOff( unsigned long off );
    BOOL Close();
    BOOL fSave(SN* psn);
    BOOL fWriteHash(SN sn, CB* pcb);
    void fixSymRecs (void* pOld, void* pNew);
    void updateConvertedSymRecs();
    BOOL packRefForMiniPDB(const char *sz, WORD imod, DWORD isectCoff,
                           bool fLocal, bool fData, bool fUDT, bool fLabel,
                           bool fConst);
    BOOL removeGlobalRefsForMod(IMOD imod);
    BOOL packRefSym(PSYM psym, IMOD imod, OFF off, OFF *poff);
    BOOL packSym (PSYM psym, OFF *poff);
    BOOL packSymSPD (PSYM , OFF *, SPD &);
    BOOL decRefCnt(OFF off);
    BOOL packTokenRefSym(PSYM psym, IMOD imod, OFF off, OFF *poff);
    PB HashSymW(const wchar_t *wcsName, PB pbSym);
    virtual BOOL getEnumByAddr(EnumSyms **ppEnum) { 
         assert(!pdbi1->fWrite);
         *ppEnum = NULL; 
        return FALSE; 
    }
    BOOL getEnumSyms(EnumSyms ** ppEnum, PB pbSym);
protected:
    
#ifdef PDB_MT
    DWORD refcount;
#else
    struct Last {
        Last() : phr(0), iphr(0) { }
        HR* phr;
        int iphr;
    } last;
#endif

    DWORD iphrHash;

    struct EnumGSISyms : public EnumSyms {
        EnumGSISyms(GSI1 * _pgsi) : pgsi(_pgsi) { reset(); };
        EnumGSISyms(GSI1 * _pgsi, HR * _phr, int _iphr) : pgsi(_pgsi), phr(_phr), iphr(_iphr) {};
        void release() {
            delete this;
        }
        void reset() {
            phr = NULL;
            iphr = -1;
        }
        BOOL next() {
            if (phr) {
                phr = phr->pnext;
            }
            if (!phr) {
                while (++iphr < pgsi->iphrHash && !(phr = pgsi->rgphrBuckets[iphr])) {};
            }
            return (phr != NULL);
        }

        void get(BYTE ** ppbSym) {
            assert(phr != NULL);
            *ppbSym = (PB)pgsi->psymForPhr(phr);
        }
        BOOL prev() {
            // this doesn't work!!!
            assert(FALSE);
            return FALSE;
        }
        BOOL clone( EnumSyms **ppEnum) {
            *ppEnum = new EnumGSISyms(*this);
            return (*ppEnum != NULL);
        }
        BOOL locate(long isect, long off) {
            // this doesn't work!!!
            assert(FALSE);
            return FALSE;
        }
    private:
        GSI1 * pgsi;
        HR * phr;
        DWORD iphr;
    };


    PDB1* ppdb1;
    DBI1* pdbi1;
    HR** rgphrBuckets;
    POOL_AlignNative poolSymHash;
    
    BOOL fInit(SN sn_);
    GSI1(PDB1* ppdb1_, DBI1* pdbi1_); 
    ~GSI1();
    BOOL readHash(SN sn, OFF offPoolInStream, CB cb); 
    BOOL fFindRec(_In_z_ ST st, HR*** pphr);
    BOOL fInsertNewSym(HR** pphr, PSYM psym, OFF *poff = 0);
    BOOL fUnlinkHR(HR** pphr);
    BOOL fEnsureSn(SN* psn)
    {
        return ppdb1->fEnsureSn(psn);
    }
    bool fixHashIn(PB pb, DWORD nEntries, CB cbSyms);

#ifdef SMALLBUCKETS
    bool CompressBuckets(Buffer* pbufBuckets, OFF *pbuckets);
    bool ExpandBuckets(SN sn, DWORD cbStart, DWORD cbBuckets, OFF *pbuckets);
#endif

    BOOL fEnsureNoSn(SN* psn)
    {
        return ppdb1->fEnsureNoSn(psn);
    }
private:
    enum {impv = (IMPV) 930803};

    inline BOOL readStream(SN sn_); 
    inline BOOL writeStream(SN* psn);
    inline void incRefCnt(HR** pphr);
    BOOL fGetFreeHR(HR** pphr);
    virtual BOOL delFromAddrMap(PSYM psym);
    virtual BOOL addToAddrMap(PSYM psym);
    HASH hashSt(_In_z_ ST st);
    HASH hashSz(SZ_CONST sz);
    OFF offForSym(PSYM psym)
    {
        return pdbi1->offForSym(psym);
    }
    BOOL fpsymFromOff(OFF off, PSYM *ppsym)
    {
        return pdbi1->fpsymFromOff( off, ppsym);
    }

    friend BOOL DBI1::OpenGlobals(OUT GSI** ppgsi);
    friend void* HR::operator new(size_t, GSI1*);
};

struct PSGSIHDR {
    CB  cbSymHash;
    CB  cbAddrMap;
    UINT nThunks;
    CB cbSizeOfThunk; 
    ISECT isectThunkTable;
    OFF offThunkTable; 
    UINT nSects;
    PSGSIHDR() : cbSymHash(0), cbAddrMap(0), nThunks(0), cbSizeOfThunk(0),
        isectThunkTable(0), offThunkTable(0) {}
};

struct PSGSI1: public GSI1 {
public:
    PB NearestSym (ISECT isect, OFF off, OUT OFF* disp);
    BOOL getEnumThunk( ISECT isect, OFF off, EnumThunk** ppenum );
    BOOL Close();
    BOOL fSave(SN* psn);
    BOOL packSym(PSYM psym);
    BOOL removeSym(SZ_CONST szName);
    BOOL getEnumByAddr(EnumSyms **ppEnum) { 
         assert(!pdbi1->fWrite);
         if (pdbi1->fWrite) {
             return FALSE;
         }
        *ppEnum = NULL;
        if ( readThunkMap() )
            *ppEnum = new EnumPubsByAddr(bufCurAddrMap, bufThunkMap, this);
        return *ppEnum != NULL;
    }
private:
    PSGSI1 (PDB1* ppdb1_, DBI1* pdbi1_, BOOL fWrite_)
        : GSI1(ppdb1_, pdbi1_), fCreate(FALSE)
        , fWrite(fWrite_) 
    {}
    ~PSGSI1();
    BOOL fInit(SN sn_);
    inline BOOL readStream();
    BOOL readAddrMap(bool fConvert = true); 
    BOOL delFromAddrMap(PSYM psym);
    BOOL addToAddrMap(PSYM psym);
    BOOL writeStream(SN* psn, Buffer& bufAddrMap);
    inline void fixupAddrMap(Buffer& buf, long_ptr_t disp);
    void fixupAddrMapForConvertedSyms(Buffer& buf, long_ptr_t off);
    BOOL readSymsInAddrMap (Buffer& buf);
    BOOL mergeAddrMap();
    inline void sortBuf(Buffer& buf);
    inline void sortBuf2(Buffer& buf);

    inline BOOL fReadSymRec(PSYM psym) {
        return pdbi1->fReadSymRec(psym);
    }
    class EnumPubsByAddr : public EnumSyms { 
        typedef PSYM *PPSYM;
    public :
        EnumPubsByAddr(Buffer& bufAddrMap, Buffer& bufThunkMap, PSGSI1* pgsi) 
            : m_iPubs(-1), m_iThunks(-2), m_bufAddrMap(bufAddrMap), m_bufThunkMap(bufThunkMap), 
              m_psgsi1(pgsi)
        { }
        EnumPubsByAddr(const EnumPubsByAddr& enm) 
            : m_bufAddrMap( enm.m_bufAddrMap ), m_bufThunkMap( enm.m_bufThunkMap ),
              m_iPubs( enm.m_iPubs ), m_iThunks( enm.m_iThunks ), m_psgsi1( enm.m_psgsi1)
        { }
        void release() {
            delete this;
        }
        void reset() {
            m_iPubs = -1;
            m_iThunks = -2;
        }

        BOOL next();
        BOOL prev();
        void get( BYTE** ppbsym );

        BOOL clone( EnumSyms **ppEnum ) {
            return ( *ppEnum = new EnumPubsByAddr(*this) ) != NULL;
        }

        BOOL locate( long isect, long off );

        PB readSymbol() {
            assert( ( -1 < m_iPubs ) && ( m_iPubs < int( m_bufAddrMap.Size() / sizeof( PPSYM ) ) ) );
            PPSYM ppsym = reinterpret_cast<PPSYM>(m_bufAddrMap.Start()) + m_iPubs;
            if ( m_psgsi1->fIsDummyThunkSym(*ppsym) || m_psgsi1->fReadSymRec(*ppsym) ) {
                return PB(*ppsym);
            }
            return NULL;
        }

    protected:
        
        bool traversingThunks() { return m_iThunks != -2; }
        bool nextThunk()        { return ( ++m_iThunks < int(m_psgsi1->psgsihdr.nThunks) ); }
        bool prevThunk()        { return ( --m_iThunks > -1 ); }
        void resetThunk()       { m_iThunks = -2; }

        ptrdiff_t  m_iPubs;
        ptrdiff_t  m_iThunks;
        Buffer &m_bufAddrMap;
        Buffer &m_bufThunkMap;
        PSGSI1 *m_psgsi1;
    };
    friend EnumPubsByAddr;

    // unchanged since fInit
    BOOL fCreate;
    BOOL fWrite;

    // either read (unchange), or write
    PSGSIHDR    psgsihdr;
    Buffer bufCurAddrMap;

    // used when pdbi1->fWrite
    Buffer bufNewAddrMap;
    Buffer bufDelAddrMap;
    Buffer bufResultAddrMap;

    // unchanged since fInit
    SN  sn;     // need to remember stream for incremental merge
    friend BOOL DBI1::OpenPublics(OUT GSI** ppgsi);

    // protected by m_csForLoad (or pdbi1->fWrite) (or readThunkMap is called)
    Buffer bufThunkMap;
    Buffer bufSectMap;
    BYTE   rgbThunkTableSym[ 0x100 ];   // Space that holds the dummy public symbol
                                        // describing the thunk table.

    BOOL fIsDummyThunkSym( PSYM psym ) { return PB( psym ) == rgbThunkTableSym; }
    BOOL readThunkMap(); 
    BOOL addThunkMap(OFF* poffThunkMap, UINT nThunks, CB cbSizeOfThunk, 
        SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable); 
    friend BOOL DBI1::AddThunkMap(OFF* poffThunkMap, UINT nThunks, CB cbSizeOfThunk, 
        SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable); 

#ifdef PDB_MT
    POOL_AlignNative thunkpool;
    PB * rgFakePubDef;
    mutable CriticalSection m_csForFakePubDef;
    mutable CriticalSection m_csForLoad;
#else
    Buffer bufThunkSym;
#endif

    PB pbInThunkTable (ISECT isect, OFF off, OUT OFF* pdisp);
    BOOL fInThunkTable(ISECT isect, OFF off);
    UINT iThunk(OFF off);
    OFF offThunkMap(OFF off);
    void mapOff(OFF off, OUT ISECT * pisect, OUT OFF* poff);
    PB pbFakePubdef(PB pb, ISECT isectThunk, OFF offThunk, OFF disp);

    friend BOOL DBI1::QuerySupportsEC();
    friend BOOL DBI1::IsLinkedIncrementally();

    CB cbSizeOfThunkMap() 
    {
        return (CB) (sizeof(OFF) * psgsihdr.nThunks);
    }
    CB cbSizeOfThunkTable() 
    {
        return (CB) (psgsihdr.cbSizeOfThunk * psgsihdr.nThunks);
    }
    CB cbSizeOfSectMap() 
    {
        return (CB) (sizeof(SO) * psgsihdr.nSects);
    }
};

class PSEnumThunk: public EnumThunk {
public:
    virtual void release();
    virtual void reset();
    virtual BOOL next();
    virtual void get( OUT USHORT* pisect, OUT long* poff, OUT long* pcb );
    PSEnumThunk( OFF offThunk, OFF* poffStart, PSGSIHDR psgsihdr );
private:
    OFF m_offThunk;
    OFF* m_poffStart;
    OFF* m_poffEnd;
    OFF* m_poffThunk;
    PSGSIHDR m_psgsihdr;
};
