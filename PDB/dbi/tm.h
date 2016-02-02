//////////////////////////////////////////////////////////////////////////////
// Type map implementation

// Type maps are created and used to map the type indices used by symbols in
// the current module, into type indices corresponding to type records
// stored in the types section of the output PDB.
//
// Type maps are organized according to this class hierarchy:
//  TM      // abstract base class
//   TMTS   // TypeServer type map (for "cl /Zi" modules)
//   TMR    // single-module type record type map (for "cl /Z7" modules)
//    TMPCT // precompiled type type map (for "cl /Yc /Z7" modules)
//
// TM lifetimes.  Whereas each TMR object is created to pack types for a single
// /Z7 module before its subsequent destruction at Mod1::Close, both TMTS (/Zi) and
// TMPCT (/Yc /Z7) type map objects, once created, are kept alive and associated
// with the current DBI object until DBI1::Close.
//
// For TMTSs, this is done for efficiency: if several modules reference types
// in some other PDB, it would be wasteful to continually open and close that
// PDB and continually remap its types.  Consider:
//
//  // mod1.cpp                     // mod2.cpp
//  #include "x.h"                  #include "x.h"
//  X x1;                           X x2;
//
//  // Generated CV information (/Zi):
//  mod1.obj:                       mod2.obj:
//   symbols:                        symbols:
//    S_GDATA32 x1 ti(X)              S_GDATA32 x2 ti(X)
//   types:                           S_GDATA32 p  ti(int**)
//    LF_TYPESERVER foo.pdb          types:
//                                     LF_TYPESERVER foo.pdb
//
// Here if mod1.obj and then mod2.obj are packed using DBI::OpenMod, etc., it
// would be inefficient to open, use, establish a type map for X and the types
// X transitively refers to, and then close foo.pdb, for mod1.obj, only to do
// the same thing over again for mod2.obj.  Rather, the TMTS for mod1.obj
// can be completely reused by mod2.obj, which can then further augment the
// type map with additional types (e.g. int** in the example above).
//
//
// In contrast, TMPCTs persist across modules, not for efficiency, but rather
// to ensure a correct packing of modules which use the C7 PCT (precompiled
// types) feature.  PCTs arise when the /Yc /Z7 and /Yu /Z7 flags are specified.
// For the /Yc /Z7 ("PCH create"), one module (the "PCT module") is compiled
// and all types it sees are written to its module's .obj's types section.
// For subsequent /Yu /Z7 ("PCH use") modules, a special record referencing
// types in the PCT module is emitted rather than repeating types known to be
// in the PCT module's type information.  Thus, a module's symbols may refer to
// type records located in the PCT module rather than the current module's
// type records.
//
// Therefore type information, including raw types and the current partial
// type map, must be retained across modules.  Consider:
//
//  // pct.cpp          // a.cpp            // b.cpp
//  #include "x.h"      #include "x.h"      #include "x.h"
//  #pragma hdrstop     #pragma hdrstop     #pragma hdrstop
//  #include "y.h"      #include "a.h"      #include "b.h"
//  ...                 ...                 ...
//
//  // Generated CV info (/Yc /Z7 pct.cpp, /Yu /Z7 a.cpp, /Yu /Z7 b.cpp):
//  pct.obj:            a.obj:              b.obj:
//   symbols:            symbols:            symbols:
//    ...                 ...                 ...
//   types:              types:              types:
//    <0x1000-0x4000>     LF_PRECOMP(pct.obj) LF_PRECOMP(pct.obj)
//    LF_ENDPRECOMP       <0x4001-...>        <0x4001-...>
//    <0x4001-...>
//
// In the example above, we see that each module contains types with type
// indices (ti's) starting at 0x1000; however a.obj and b.obj's modules
// do not actually contain copies of the types known to be in pct.obj; rather
// they reference those types with a PRECOMP record.  Note that pct.obj's
// type 0x4001 is probably different from a.obj's and likewise from b.obj's.
//
// To deal with this kind of module type information, cvpack or link must
// ensure that modules containing LF_ENDPRECOMP are passed to DBI::OpenMod
// and Mod::AddTypes before other modules whose types' LF_PRECOMP's refer
// to PCT modules.
//
// For its part, DBI1 will keep alive (across modules) any TMPCTs that get
// created when types containing LF_ENDPRECOMPs are seen.  Thus subsequent
// modules which contain LF_PRECOMP records referencing those types
// (e.g. compiled "cl /Yu /Z7") can simply load these types from their TMPCT.
// Therefore, for modules containing LF_PRECOMP records, we use a TMPCT to help
// initialize each module's TMR.
//
//
// By the way, someday we may elect to further extend the lifetime of TMTS and
// TMPCT type maps to make them persistent across link/cvpack invocations...

class TM { // abstract type map
public:
    TM(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_);
    BOOL fMapRti(IMOD imod, TI& rti, bool fID);
    virtual void endMod();
    virtual void endDBI();
    virtual bool IsTmpctOwner() const {
        return false;
    }
    virtual bool IsTmpct() const {
        return false;
    }
    virtual bool IsTMR() const {
        return false;
    }
    virtual bool IsTMEQTS() const {
        return false;
    }
    virtual ~TM() = 0;
    BOOL fNotOutOfTIs() {
        return ptpiTo->QueryTiMac() < ::tiMax;
    }

    bool fCorruptedFromTypePool() const {
        return m_fCorruptedFromTypePool;
    }
    bool & fCorruptedFromTypePool() {
        return m_fCorruptedFromTypePool;
    }
    virtual PTYPE ptypeForTi(TI ti, bool fID) const pure;
    virtual BOOL QuerySrcLineForUDT(TI tiUdt, _Deref_out_z_ char **psz, DWORD *pLine) pure; 
    virtual BOOL QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line) pure; 
    virtual BOOL QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti) pure;
    virtual BOOL fVerifyIndex(TI ti, bool fID) {
        return isValidIndex(ti, fID);
    }
    virtual BOOL fIdMapped(TI id) const {
        return false;
    }
    TI  TiMin() const {
        return tiMin;
    }
    TI  TiMac() const {
        return tiMac;
    }
    TI  IdMin() const {
        return idMin;
    }
    TI  IdMac() const {
        return idMac;
    }
    IMOD Imod() const {
        return m_imod;
    }
    void SetImod(IMOD imod) {
        m_imod = imod;
    }
    BOOL fTiMapped(TI ti) const;

protected:
    BOOL fInit(TI tiMin_, TI tiMac_, TI idMin_, TI idMac_);
    virtual BOOL fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt) = 0;
    virtual inline TI& rtiMapped(TI ti, bool fID) const;
    inline TI indexBias(TI ti, bool fID) const {
        dassert(isValidIndex(ti, fID));
        return ti - (fID ? idMin : tiMin);
    }
    inline BOOL isValidIndex(TI ti, bool fID) const {
        return ti < (fID ? idMac : tiMac);
    }
    
    PDB1 *      ppdb1To;            // 'to' PDB
    DBI1 *      pdbi1To;            // 'to' DBI
    TPI *       ptpiTo;             // 'to' TypeServer
    TPI *       pipiTo;             // 'to' ID server
    TI *        mptiti;             // memorization of mapping to project PDB TIs
    TI *        mpidid;             // memorization of mapping to project PDB IDs
    TI          tiMin;              // minimum TI in this module
    TI          tiMac;              // maximum TI in this module + 1
    TI          idMin;              // minimum ID in this module
    TI          idMac;              // maximum ID in this module + 1
    unsigned    ctiFrom;            // tiBias(tiMac)
    unsigned    cidFrom;
    IMOD        m_imod;             // first module that creates this TM
    bool        m_fDefnUdt;         // whether the current type is an UDT definition
    bool        m_fCorruptedFromTypePool;
                                    // Whether we encountered a bad type in the from pool

    Map<TI, NI, HcNi>
                m_mpSrcIdToNi;      // map src file id to NI

#ifdef  PDB_MT
    mutable CriticalSection m_cs;
#endif

public:
    virtual PDB *
    PPdbFrom() const {
        return NULL;
    }
};

class TMTS sealed : public TM { // type map for modules which use a different TypeServer
public:
    TMTS(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_);
    BOOL fInit(PDB* ppdbFrom);
    ~TMTS();
    inline PTYPE ptypeForTi(TI ti, bool fID) const;
    inline BOOL QuerySrcLineForUDT(TI tiUdt, _Deref_out_z_ char **psz, DWORD *pLine);
    inline BOOL QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line);
    BOOL QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti);
    PDB *   PPdbFrom() const { return ppdbFrom; }
    TPI *   PTpiFrom() const { return ptpiFrom; }
    TPI *   PIpiFrom() const { return pipiFrom; }
    void    ClosePdbFrom();

private:
    BOOL fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt);
    PDB* ppdbFrom;      // 'from' PDB
    DBI* pdbiFrom;      // 'from' DBI
    TPI* ptpiFrom;      // 'from' TPI
    TPI* pipiFrom;      // 'from' IPI
};

// The UDT reference work requires tracking all UDT refs, even when
// the from and to type pools are the same (mintypeinfo is the culprit).
// All this class does is snoop the top level type indices in fMapRti and do
// the same processing that the TMTS would do.
class TMEQTS sealed : public TM { // type map for modules which use the same TypeServer
public:
    TMEQTS(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_);
    BOOL fInit();

    inline PTYPE
    ptypeForTi(TI ti, bool fID) const;

    inline BOOL
    QuerySrcLineForUDT(TI tiUdt, _Deref_out_z_ char **psz, DWORD *pLine);

    inline BOOL
    QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line);

    BOOL
    QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti);
    
    PDB *
    PPdbFrom() const {
        return ppdb1To;
    }

    bool
    IsTMEQTS() const {
        return true;
    }

    BOOL fVerifyIndex(TI ti, bool fID) {
        return isValidIndex(ti, fID);
    }

private:
    BOOL fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt);

    inline BOOL
    isValidIndex(TI ti, bool fID) const {
        return ti < (fID ? pipiTo->QueryTiMac() : ptpiTo->QueryTiMac());
    }
};

class TMR : public TM { // type map for module with type records
public:
    TMR(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_);
    BOOL fInit(PB pbTypes_, CB cb, _In_z_ SZ szModule, ULONG sig);
    void endMod();
    void endDBI();
    bool IsTmpctOwner() const {
        return m_fIsTmpctOwner;
    }
    bool IsTMR() const {
        return true;
    }
    SIG Sig() {
        return signature;
    }
    ~TMR();
    inline PTYPE ptypeForTi(TI ti, bool fID) const;
    inline BOOL QuerySrcLineForUDT(TI tiUdt, _Deref_out_z_ char **psz, DWORD *pLine);
    inline BOOL QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line);
    BOOL QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti);
    TMPCT * Ptmpct() const {
        return m_ptmpct;
    }
    virtual BOOL fIdMapped(TI id) const {
        return rtiMapped(id, true) != tiNil;
    }
    void GetRawTypes(PB *ppb, CB *pcb) {
        if (ppb) {
            *ppb = pbTypes;
        }

        if (pcb) {
            *pcb = cbTypes;
        }
    }

protected:
    BOOL fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt);
    virtual inline TI& rtiMapped(TI ti, bool fID) const;

    TMPCT*      m_ptmpct;   // (if non-0) type map for PCT types
    PTYPE*      mptiptype;  // mapping from old TI to old type record address

    Map<TI, TYPTYPE*, LHcNi>
        mptiUdtIdRec;       // mapping from old UDT TI to old ID record address

    Map<NI, TI, LHcNi>
        mpnitiUdt;          // mapping from NI of UDT name to UDT TI

private:
    PB          pbTypes;    // group type records referenced by the PCT's mptiptype
    CB          cbTypes;    // size of the above type records
    WidenTi*    pwti;       // widening of type records interface
    SIG         signature;  // signature on the TMPCT

protected:
    bool        m_fIsTmpctOwner; // true if is a PCT owner (the PCH object)

private:
    bool        f16bitTypes;
};

class TMPCT sealed : public TMR { // type map for a PCT module
public:
    TMPCT(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_);
    BOOL fInit(PB pbTypes_, CB cb, _In_z_ SZ szModule, ULONG sig);
    void endMod();
    void endDBI();
    ~TMPCT();
    bool IsTmpct() const {
        return m_fIsTmpct;
    }

private:
    bool        m_fIsTmpct;
};

struct OTM {            // DBI1 helper to find some currently Open TM
    OTM(OTM* pNext_, const wchar_t *wszName_, const SIG70& sig70_, AGE age, TI tiPreComp_, TM* ptm_);
    ~OTM();

    bool
    fUpdateName(const wchar_t * wszNewName);

    OTM *           pNext;      // next OTM in this list
    SIG70           sig70;      // signature on this TM if in GUID format
    const wchar_t*  wszName;    // name of the TM (TMTS: PDB name; TMPCT: module name)
    TM *            ptm;        // TM (TMTS or TMPCT)
    TI              tiPreComp;  // TI in the PCT (0 if PDB)
    AGE             age;        // Age of the "from TPI" if it is a TPI and not a PCT
};
