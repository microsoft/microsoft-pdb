//////////////////////////////////////////////////////////////////////////////
// Type Map implementation

#include "pdbimpl.h"
#include "dbiimpl.h"
#include "output.h"

#include <stdio.h>
#include <malloc.h>

#define IS_ST_LEAF(p)     (MapLeafStToSz(((lfEasy*)p)->leaf) != ((lfEasy*)p)->leaf)
#define SZ_NAME(p, n)     (IS_ST_LEAF(p) ? szForSt((ST)p->n) : (SZ)p->n)
#define NEW_SZ_NAME(p, n) (IS_ST_LEAF(p) ? szCopySt((ST)p->n) : szCopy((SZ)p->n))

#ifdef _DEBUG
BOOL rgbEnableDiagnostic[20];
#endif

TM::TM(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_)
    : ppdb1To(ppdb1To_), pdbi1To(pdbi1To_), ptpiTo(ptpiTo_), pipiTo(pipiTo_),
      mptiti(0), mpidid(0), m_fCorruptedFromTypePool(false), m_imod(imodNil) {}

BOOL TM::fInit(TI tiMin_, TI tiMac_, TI idMin_, TI idMac_)
{
    tiMin = tiMin_;
    tiMac = tiMac_;
    dassert(tiMin <= tiMac);
    ctiFrom = tiMac - tiMin;
    if (ctiFrom && !(mptiti = new (zeroed) TI[ctiFrom])) {
        ppdb1To->setOOMError();
        return FALSE;
    }

    idMin = idMin_;
    idMac = idMac_;
    dassert(idMin <= idMac);
    cidFrom = idMac - idMin;
    if (cidFrom && !(mpidid = new (zeroed) TI[cidFrom])) {
        ppdb1To->setOOMError();
        return FALSE;
    }

    return TRUE;
}

void TM::endMod()
{
    // do nothing: by default, TMs outlive modules
}

void TM::endDBI()
{
    delete this;
}

TM::~TM()
{
    if (mptiti) {
        delete [] mptiti;
    }

    if (mpidid) {
        delete [] mpidid;
    }
}

TMEQTS::TMEQTS(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_)
    : TM(ppdb1To_, pdbi1To_, ptpiTo_, pipiTo_)
{
    instrumentation(pdbi1To->info.cTMTS++);
}

BOOL TMEQTS::fInit()
{
    // to/from are equivalent in a TMEQTS

    TI tiMin_ = ptpiTo->QueryTiMin();
    TI tiMac_ = ptpiTo->QueryTiMac();

    assert(ptpiTo->SupportQueryTiForUDT());

    TI idMin_ = tiNil;
    TI idMac_ = tiNil;

    if (pipiTo != NULL) {
        idMin_ = pipiTo->QueryTiMin();
        idMac_ = pipiTo->QueryTiMac();

        assert(pipiTo->SupportQueryTiForUDT());
    }

    // initialize rest, but we don't need a mapping in the base class,
    // so indicate it through giving it no types to map.
    return TM::fInit(tiMin_, tiMin_, idMin_, idMin_);
}

TMTS::TMTS(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_)
    : TM(ppdb1To_, pdbi1To_, ptpiTo_, pipiTo_), ppdbFrom(0), pdbiFrom(0), ptpiFrom(0), pipiFrom(0)
{
    instrumentation(pdbi1To->info.cTMTS++);
}

BOOL TMTS::fInit(PDB* ppdbFrom_)
{
    dassert(ppdbFrom_);

    // given ppdbFrom, open ptpiFrom and pipiFrom

    ppdbFrom = ppdbFrom_;
    if (!ppdbFrom->OpenTpi(pdbRead pdbGetRecordsOnly, &ptpiFrom)) {
        return FALSE;
    }

    TI tiMin_ = ptpiFrom->QueryTiMin();
    TI tiMac_ = ptpiFrom->QueryTiMac();

    TI idMin_ = tiNil;
    TI idMac_ = tiNil;

    if (ppdbFrom->OpenIpi(pdbRead pdbGetRecordsOnly, &pipiFrom)) {
        idMin_ = pipiFrom->QueryTiMin();
        idMac_ = pipiFrom->QueryTiMac();
    }
    else {
        // It is okay so long as it is an old verioned PDB.
    }

    // initialize rest
    return TM::fInit(tiMin_, tiMac_, idMin_, idMac_);
}

void TMTS::ClosePdbFrom()
{
    if (mptiti) {
        delete [] mptiti;
    }

    if (mpidid) {
        delete [] mpidid;
    }

    if (ptpiFrom) {
        ptpiFrom->Close();
    }

    if (pipiFrom) {
        pipiFrom->Close();
    }

    if (ppdbFrom) {
        ppdbFrom->Close();
        ppdbFrom = NULL;
    }
}

TMTS::~TMTS()
{
    // xfer Src from the "from" pdb to the "to" pdb

    if (ppdbFrom) {
        ppdb1To->CopySrc(ppdbFrom);
    }

    if (ptpiFrom) {
        ptpiFrom->Close();
    }
    if (pipiFrom) {
        pipiFrom->Close();
    }
    if (pdbiFrom) {
        pdbiFrom->Close();
    }
    if (ppdbFrom) {
        ppdbFrom->Close();
    }
}

TMR::TMR(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_)
    : TM(ppdb1To_, pdbi1To_, ptpiTo_, pipiTo_), m_ptmpct(0), mptiptype(0), pbTypes(0), pwti(0)
{
    instrumentation(pdbi1To->info.cTMR++);
    m_fIsTmpctOwner = false;
    signature = 0;
}

BOOL TMR::fInit(PB pbTypes_, CB cb, SZ szModule, ULONG sigTypes)
{
    PTYPE       ptypeMin = (PTYPE)pbTypes_;
    PTYPE       ptypeMax = (PTYPE)(pbTypes_ + cb);
    TI          tiMin_ = CV_FIRST_NONPRIM;
    bool        fPrecomp = false;
    lfPreComp * plfPreComp = NULL;

    f16bitTypes = sigTypes < CV_SIGNATURE_C11;

    if (f16bitTypes) {
        if (!WidenTi::fCreate(pwti, cb >> 4)) {
            ppdb1To->setOOMError();
            return FALSE;
        }
        // set up for checking the PCT use; we have to get a 32-bit
        // version of the lfPreComp_16t record if so.
        if (cb > 0) {
            PTYPE   p = PTYPE(pwti->pTypeWidenTi(0, PB(ptypeMin)));
            if (SZLEAFIDX(p->leaf) == LF_PRECOMP) {
                plfPreComp = (lfPreComp*)&p->leaf;
                fPrecomp = TRUE;
            }
        }
    }

    // check for PCT use, 32-bit type only; 16-bit case handled above
    if (fPrecomp || (fPrecomp = (cb > 0 && SZLEAFIDX(ptypeMin->leaf) == LF_PRECOMP))) {
        if (!plfPreComp)
            plfPreComp = (lfPreComp*)&ptypeMin->leaf;
        assert(plfPreComp->start == ::tiMin);
        // now skip the LF_PRECOMP record and advance tiMin_ over the TIs in the PCT
        ptypeMin = (PTYPE)pbEndType(ptypeMin);
        tiMin_ += plfPreComp->count;
    }

    // count types and check for PCT definition
    TI tiPreComp = tiNil;
    TI tiMac_ = tiMin_;
    PTYPE ptypeEndPreComp = 0;
    PTYPE ptype;

    for (ptype = ptypeMin; ptype < ptypeMax; ptype = (PTYPE)pbEndType(ptype)) {
        if (SZLEAFIDX(ptype->leaf) == LF_ENDPRECOMP) {
            tiPreComp = tiMac_;
            ptypeEndPreComp = ptype;
        }
        ++tiMac_;
    }

    if (tiPreComp != tiNil) {
        // This module is a PCT.  Create a TMPCT containing the types up to the
        // LF_PRECOMP record.  Adjust things to subsequently create a TMR which
        // uses the TMPCT just as if this module were just another PCT use.
        if (fPrecomp)   {
            // a pct referencing another pct is not supported - issue warning
            // to recompile -Zi or build pdb:none
            ppdb1To->setLastError(EC_NOT_IMPLEMENTED, szModule);
            return FALSE;
        }
        m_fIsTmpctOwner = true;
        CB cbPreComp = CB(PB(ptypeEndPreComp) - pbTypes_);  // REVIEW:WIN64 CAST
        lfEndPreComp* pepc = (lfEndPreComp*)&ptypeEndPreComp->leaf;
        if (!(m_ptmpct = new TMPCT(ppdb1To, pdbi1To, ptpiTo, pipiTo))) {
            ppdb1To->setOOMError();
            return FALSE;
        }

        // Initialize the TMPCT and add it to the OTM list with a temporary
        // name (the module name from the linker).  We will update it later
        // when processing the symbols for this object file.

        signature = pepc->signature;
        if (!m_ptmpct->fInit(pbTypes_, cbPreComp, szModule, sigTypes) ||
            !pdbi1To->fAddTmpct(pepc, tiPreComp, szModule, m_ptmpct))
        {
            return FALSE;
        }

        pbTypes_ = pbEndType(ptypeEndPreComp);
        ptypeMin = (PTYPE)pbTypes_;
        tiMin_ = tiPreComp + 1;
    }

    // had to defer finding precomp tmr until we look for an endprecomp
    // this is to generate a consistent link error regardless of module link order
    PTYPE ptypePreComp = PTYPE( ((PB)plfPreComp) - sizeof (ptypePreComp->len) );

    // we do not need to go after the TMPCT if we are the owner of it; we have it
    // already in m_ptmpct if so.

    assert(implies(fPrecomp, !IsTmpctOwner()));
    assert(implies(IsTmpctOwner(), m_ptmpct != NULL));

    if (fPrecomp && !pdbi1To->fGetTmpct(ptypePreComp, &m_ptmpct)) {
        SZ szErr = NEW_SZ_NAME(plfPreComp, name);
        ppdb1To->setLastError(EC_PRECOMP_REQUIRED, szErr);
        freeSz(szErr);
        return FALSE;
    }

    // initialize base
    if (!TM::fInit(tiMin_, tiMac_, tiNil, tiNil)) {
        return FALSE;
    }

    // establish mptiptype
    if (!(mptiptype = new PTYPE[ctiFrom])) {
        ppdb1To->setOOMError();
        return FALSE;
    }

    // save a copy of *pbTypes_ to isolate us from client memory usage
    if (f16bitTypes) {
        dassert(pwti);

        // we need to go through all of the types and count up how
        // much space to allocate for the expanded records
        cb = 0;
        TI  tiT = tiMin;

        for (ptype = ptypeMin;
            ptype < ptypeMax;
            ptype = PTYPE(pbEndType(ptype)), tiT++) {

            // this call will widen and cache the record if appropriate.
            PTYPE   ptypeT = PTYPE(pwti->pTypeWidenTi(tiT, PB(ptype)));

            if (ptypeT) {
                cb += CB(cbAlign(cbForType(ptypeT)));
            }
            else {
                ppdb1To->setOOMError();
                return FALSE;
            }
        }

        tiT = tiMin;

        if (!(pbTypes = new BYTE[cb])) {
            ppdb1To->setOOMError();
            return FALSE;
        }

        assert((int_ptr_t(pbTypes) & 3) == 0);

        PB pbTypesT = pbTypes;

        for (ptype = ptypeMin;
            ptype < ptypeMax;
            ptype = PTYPE(pbEndType(ptype)), tiT++) {

            // pick up the cached records
            PTYPE   ptypeT = PTYPE(pwti->pTypeWidenTi(tiT, PB(ptype)));

            if (ptypeT) {
                CB  cbT = cbForType(ptypeT);

                assert(pbTypesT < pbTypes + cb);
                assert(pbTypesT + cbT <= pbTypes + cb);

                memcpy(pbTypesT, PB(ptypeT), cbT);

                // pad out the record and be sure to update the len field.
                CB  cbPad = cbInsertAlign(pbTypesT + cbT, cbT);

                // advance the destination buffer pointer and adjust
                // the destination type record length field.
                PTYPE(pbTypesT)->len += ushort(cbPad);
                pbTypesT += cbT + cbPad;

                assert(((PTYPE(pbTypesT - (cbT + cbPad))->len + 2) & 3) == 0);
                assert(pbTypesT <= pbTypes + cb);
            }
            else {
                dassert(FALSE); // should never happen, since we have
                                // prescanned the types above.
            }
        }

        pwti->release();
        pwti = NULL;
    }
    else
    {
        cb = CB(PB(ptypeMax) - PB(ptypeMin));   // REVIEW:WIN64 CAST

        if (!(pbTypes = new BYTE[cb])) {
            ppdb1To->setOOMError();
            return FALSE;
        }

        memcpy(pbTypes, (PB)ptypeMin, cb);
    }

    cbTypes = cb;

    TI ti = tiMin;
    NameMap *pNameMap = NULL;

    if (ppdb1To->FMinimal()) {
        if (!pdbi1To->GetNameMap(true, &pNameMap)) {
            ppdb1To->setOOMError();
            return FALSE;
        }
    }

    for (ptype = (PTYPE)pbTypes, ptypeMax = (PTYPE)(pbTypes + cb);
        ptype < ptypeMax;
        ptype = (PTYPE)pbEndType(ptype)) {
        mptiptype[indexBias(ti, false)] = ptype;
        if (ptypeForTi(ti, false) != ptype) {
            ppdb1To->setCorruptError();
            return FALSE;
        }

        if (ptype->leaf == LF_UDT_SRC_LINE) {
            lfUdtSrcLine *plf = (lfUdtSrcLine *) &ptype->leaf;
            if (!mptiUdtIdRec.add(plf->type, ptype)) {
                ppdb1To->setOOMError();
                return FALSE;
            }
        }

        if (ppdb1To->FMinimal()) {
            PB pb = (PB) ptype;

            if (REC::fIsDefnUdt(pb)) {
                SZ szName = szUDTName(pb);

                if (REC::fIsLocalDefnUdtWithUniqueName(pb)) {
                    szName = szName + strlen(szName) + 1;
                }

                NI ni;

                if (!pNameMap->getNiUTF8(szName, &ni) || !mpnitiUdt.add(ni, ti)) {
                    ppdb1To->setOOMError();
                    return FALSE;
                }
            }
        }

        ++ti;
    }

    return TRUE;
}

void TMR::endMod()
{
    delete this;
}

void TMR::endDBI()
{
    dassert(FALSE);
}

TMR::~TMR()
{
    if (pbTypes) {
        delete [] pbTypes;
    }
    if (mptiptype) {
        delete [] mptiptype;
    }
    if (pwti) {
        pwti->release();
    }
}

TMPCT::TMPCT(PDB1* ppdb1To_, DBI1* pdbi1To_, TPI* ptpiTo_, TPI* pipiTo_)
    : TMR(ppdb1To_, pdbi1To_, ptpiTo_, pipiTo_)
{
    m_fIsTmpct = true;
    instrumentation(pdbi1To->info.cTMR--);
    instrumentation(pdbi1To->info.cTMPCT++);
}

BOOL TMPCT::fInit(PB pbTypes_, CB cb, SZ szModule, ULONG sig)
{
    return TMR::fInit(pbTypes_, cb, szModule, sig);
}

void TMPCT::endMod()
{
    dassert(FALSE);
}

void TMPCT::endDBI()
{
    delete this;
}

TMPCT::~TMPCT()
{
}

debug(void dumpType(PTYPE ptype););

const CB    cbTypeBuf   = 256;  // this buffer will be large enough for most records

// Update rti (a TI reference with the TI of an equivalent record in ptpiTo,
// and return TRUE if successful.
//
BOOL TM::fMapRti(IMOD imod, TI& rti, bool fID)
{
    MTS_PROTECT(m_cs);

    TI tiOrig = rti;

    instrumentation(pdbi1To->info.cTypesMapped++);

    if (fID && (pipiTo == NULL)) {
        ppdb1To->setLastError(EC_FORMAT);
        return FALSE;
    }

    bool fDefnUdt = false;

    if (!fMapRti(rti, fID, 0, &fDefnUdt)) {
        return FALSE;
    }

    // For a non fwdref UDT, convert its corresponding LF_UDT_SRC_LINE
    // to LF_UDT_MOD_SRC_LINE, and then write it into PDB.

    if (fID || !fDefnUdt || (pipiTo == NULL)) {
        // Don't bother if it is not a UDT definition or this PDB has no ID stream.

        return TRUE;
    }

    // Check whether source string has already been added into string table. 

    DWORD dwLine = 0;
    TI    srcId = 0;
    NI    ni = 0;

    if (!QuerySrcIdLineForUDT(tiOrig, srcId, dwLine)) {
        // It is OK if we can't get source info on this UDT definition.

        return TRUE;
    }

    if (!m_mpSrcIdToNi.map(srcId, &ni)) {
        char *szSrc = NULL;

        if (!QuerySrcLineForUDT(tiOrig, &szSrc, &dwLine)) {
            // It is OK if we can't get source/line on this UDT definition.

            return TRUE;
        }

        // Add the source string into PDB's string table.

        NameMap *pNameMap = NULL;

        if (!pdbi1To->GetNameMap(true, &pNameMap)) {
            ppdb1To->setOOMError();
            return FALSE;
        }

        if (pdbi1To->cSrcMapEntries() > 0) {
            USES_STACKBUFFER(MAX_PATH * sizeof(wchar_t));

            wchar_t *wszSrc = GetSZUnicodeFromSZUTF8(szSrc);
            Buffer   bufMappedName;

            if (pdbi1To->fSrcMap(wszSrc, bufMappedName)) {
                wszSrc = reinterpret_cast<wchar_t *>(bufMappedName.Start());

                if (!pNameMap->getNiW(wszSrc, &ni)) {
                    ppdb1To->setOOMError();
                    return FALSE;
                }
            }
        }

        if ((ni == 0) && !pNameMap->getNiUTF8(szSrc, &ni)) {
            ppdb1To->setOOMError();
            return FALSE;
        }

        delete [] szSrc;

        if (!m_mpSrcIdToNi.add(srcId, ni)) {
            ppdb1To->setOOMError();
            return FALSE;
        }
    }

    // Compose LF_UDT_MOD_SRC_LINE record and write it into PDB.

    BYTE rgb[sizeof(unsigned short) + sizeof(lfUdtModSrcLine)] = {0};

    *((unsigned short *) rgb) = sizeof(lfUdtModSrcLine);
    
    lfUdtModSrcLine *plf = (lfUdtModSrcLine *) (rgb + sizeof(unsigned short));

    plf->leaf = LF_UDT_MOD_SRC_LINE;
    plf->imod = imod + 1;
    plf->type = rti;
    plf->src  = ni;
    plf->line = dwLine;

    TI tiDummy = 0;

    return pipiTo->QueryTiForCVRecord(rgb, &tiDummy);
}

inline BOOL fGlobalUDTDecl(PTYPE ptype)
{
    assert(!fNeedsSzConversion(ptype));

    unsigned    leaf = ptype->leaf;
    if ((leaf >= LF_CLASS) && (leaf <= LF_ENUM)) {
        void *  pv = &ptype->leaf;
        return (leaf <= LF_UNION) ?
            ((lfClass*)pv)->property.fwdref && !((lfClass*)pv)->property.scoped :
            ((lfEnum*)pv)->property.fwdref && !((lfEnum*)pv)->property.scoped;
    }
    return FALSE;
}

PTYPE TMEQTS::ptypeForTi(TI ti, bool fID) const
{
    assert(isValidIndex(ti, fID) && !CV_IS_PRIMITIVE(ti));
    PB pb = NULL;
    TPI* p = fID ? pipiTo : ptpiTo;
    verify(p->QueryPbCVRecordForTi(ti, &pb));
    return PTYPE(pb);
}

BOOL TMEQTS::QuerySrcLineForUDT(TI tiUdt, char **psz, DWORD *pLine)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    return ((TPI1 *) pipiTo)->QueryModSrcLineForUDT(tiUdt, NULL, psz, pLine);
}

BOOL TMEQTS::QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    return ((TPI1 *) pipiTo)->QuerySrcIdLineForUDT(tiUdt, srcId, line);
}

PTYPE TMTS::ptypeForTi(TI ti, bool fID) const
{
    dassert(isValidIndex(ti, fID)  && !CV_IS_PRIMITIVE(ti));
    PB pb = NULL;
    TPI* p = fID ? pipiFrom : ptpiFrom;
    verify(p->QueryPbCVRecordForTi(ti, &pb));
    return (PTYPE) pb;
}

BOOL TMTS::QuerySrcLineForUDT(TI tiUdt, char **psz, DWORD *pLine)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    if (pipiFrom == NULL) {
        return FALSE;
    }

    return ((TPI1 *) pipiFrom)->QueryModSrcLineForUDT(tiUdt, NULL, psz, pLine);
}

BOOL TMTS::QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    if (pipiFrom == NULL) {
        return FALSE;
    }

    return ((TPI1 *) pipiFrom)->QuerySrcIdLineForUDT(tiUdt, srcId, line);
}

// Update rti (a TI reference for a type record stored in a TypeServer) with
// the TI of an equivalent record in ptpiTo, and return TRUE if successful.
//
BOOL TMTS::fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt)
{
    expect(isValidIndex(rti, fID));

    instrumentation(pdbi1To->info.cTypesMappedRecursively++);

    // return immediately if TI is primitive, is bogus, or has already been mapped
    if (CV_IS_PRIMITIVE(rti)) {
        return TRUE;
    }

    if (!isValidIndex(rti, fID)) {
        // Leave a flag that this pool has some corruption.
        // REVIEW:  later, we may want to provide some more data here
        //

        fCorruptedFromTypePool() = true;
        rti = tiNil;
        return TRUE;
    }

    if (rtiMapped(rti, fID) != tiNil) {
        rti = rtiMapped(rti, fID);
        return TRUE;
    }

    // read type record from the 'from' TypeServer or ID server
    TPI* pFrom = fID ? pipiFrom : ptpiFrom;
    SafeStackAllocator<1024>    allocator;
    CB cb = 0;
    if (!pFrom->QueryCVRecordForTi(rti, NULL, &cb)) {
        return FALSE;
    }
    
    // alloc a buffer
    PTYPE   ptype = reinterpret_cast<PTYPE>(allocator.Alloc<BYTE>(cb));

    if (NULL == ptype) {
        ppdb1To->setOOMError();
        return FALSE;
    }
    if (!pFrom->QueryCVRecordForTi(rti, (PB)ptype, &cb)) {
        return FALSE;
    }

    // recursively map all TIs within the type record into the 'to' TypeServer
#if _DEBUG // {

#if !defined( PDB_LIBRARY )
    if (rgbEnableDiagnostic[0]) {
        OutputInit();

        StdOutPrintf(L"%d< %04X       ", depth, rti);
        dumpType(ptype);
    }
#endif

#endif // }

    for (TypeTiIter tii(ptype); tii.next(); ) {
        if (!TMTS::fMapRti(tii.rti(), tii.fId(), depth + 1, NULL)) {
            return FALSE;
        }
    }

    if ((pfDefnUdt != NULL) && !fID) {
        *pfDefnUdt = REC::fIsDefnUdt((PB) ptype);
    }

    // find TI for resulting record within the 'to' TypeServer
    TPI* pTo = fID ? pipiTo : ptpiTo;

    instrumentation(pdbi1To->info.cTypesQueried++);
    instrumentation(TI tiMacWas = pTo->QueryTiMac());

    TI  tiTo;
    if (!pTo->QueryTiForCVRecord((PB)ptype, &tiTo) &&
        (ppdb1To->QueryLastErrorExW(NULL, 0) != EC_OUT_OF_TI)) {
        return FALSE;       // if we run out of type indices keep trying to pack
    }

    instrumentation(pdbi1To->info.cTypesAdded += (tiMacWas != pTo->QueryTiMac()));

#if _DEBUG // {

#if !defined( PDB_LIBRARY )
    if (rgbEnableDiagnostic[0]) {
        StdOutPrintf(L"%d> %04X->%04X ", depth, rti, tiTo);
        dumpType(ptype);
    }
#endif

#endif // }


    // update rti and maps
    rti = rtiMapped(rti, fID) = tiTo;

    return TRUE;
}

inline CV_prop_t getPropOfUDTType(PTYPE ptype)
{
    switch (SZLEAFIDX(ptype->leaf)) {
    case LF_CLASS:
    case LF_STRUCTURE:
    case LF_UNION:
        return ((lfClass*) (&ptype->leaf))->property;
    case LF_ENUM:
        return ((lfEnum*) (&ptype->leaf))->property;
    case LF_ALIAS:
        {
            CV_prop_t   propT = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            assert(!propT.fwdref);
            return propT;
        }
    };

    // return a property with fwdref set so that it will
    // flagged as a decl, not a defn.
    CV_prop_t   prop = { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 };
    assert(prop.fwdref);
    return prop;
}

inline BOOL fUDTDefn(PTYPE ptype)
{
    return !getPropOfUDTType(ptype).fwdref;
}

// make sure that the conditions we require for a quick check
// for UDT type records holds true.
cassert(LF_CLASS+1 == LF_STRUCTURE);
cassert(LF_STRUCTURE+1 == LF_UNION);
cassert(LF_UNION+1 == LF_ENUM);

inline bool fIsUDT(unsigned leaf)
{
    return SZLEAFIDX(leaf) == LF_ALIAS || (SZLEAFIDX(leaf) >= LF_CLASS && SZLEAFIDX(leaf) <= LF_ENUM);
}

namespace _tm {
    struct UnnamedTag {
        const char *    sz;
        size_t          cb;
    };
}

BOOL fUDTAnon(PTYPE ptype)
{
    static const _tm::UnnamedTag utag[] = {
        { "::<unnamed-tag>", 15 },
        { "::__unnamed", 11 }
    };

    assert(!fNeedsSzConversion(ptype));
    UTFSZ sz = szUDTName(PB(ptype));
    if (sz) {
        size_t  cbSz = 0;
        for (unsigned iutag = 0; iutag < _countof(utag); iutag++) {
            if (strcmp(sz, utag[iutag].sz + 2) == 0) {
                return TRUE;
            }

            if (cbSz == 0) {
                cbSz = strlen(sz);
            }

            if (cbSz >= utag[iutag].cb) {
                // can only be part of composite class name ie foo::`utag[iut].sz'
                // check the last cb bytes for a match of utag[iut].sz.
                char *pchMax = sz + cbSz;
                if (memcmp(utag[iutag].sz, pchMax - utag[iutag].cb, utag[iutag].cb) == 0) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}


// do the UDT refs when we have from and to type server equivalence.
//
BOOL TMEQTS::fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt)
{
    instrumentation(pdbi1To->info.cTypesMappedRecursively++);

    // return immediately if TI is primitive or has already been mapped
    if (CV_IS_PRIMITIVE(rti))
        return TRUE;

    if (!isValidIndex(rti, fID)) {
        // Leave a flag that this pool has some corruption.
        // REVIEW:  later, we may want to provide some more data here
        //
        fCorruptedFromTypePool() = true;
        rti = tiNil;
        return TRUE;
    }

    PTYPE   ptype = ptypeForTi(rti, fID);

    for (TypeTiIter tii(ptype); tii.next(); ) {
        if (!fMapRti(tii.rti(), tii.fId(), depth + 1, NULL)) {
            return FALSE;
        }
    }

    if (!fID) {
        if (pfDefnUdt != NULL) {
            *pfDefnUdt = REC::fIsDefnUdt((PB) ptype);
        }

        if (REC::fIsGlobalDefnUdt((PB) ptype) || REC::fIsLocalDefnUdtWithUniqueName((PB) ptype)) {
            // The type record should have already existed in the PDB.
            // But in case that the PDB contains multiple definitions
            // of this UDT, we have to promote this definition to the
            // head of the hash chain.

            TI tiTo;

            if (!ptpiTo->QueryTiForCVRecord((PB)ptype, &tiTo) || (tiTo != rti)) {
                ppdb1To->setLastError(EC_CORRUPT);
                return FALSE;
            }
        }
    }

    return TRUE;
}

// Update rti (a TI reference for a type record stored directly in this
// module's types records) with the TI of an equivalent record in ptpiTo,
// and return TRUE if successful.
//
BOOL TMR::fMapRti(TI& rti, bool fID, int depth, bool *pfDefnUdt)
{
    bool f = (depth == 0) && (m_ptmpct != NULL);

    MTS_PROTECT_COND((f ? m_ptmpct->m_cs : m_cs), f);

    expect(isValidIndex(rti, false));

    instrumentation(pdbi1To->info.cTypesMappedRecursively++);

    // return if TI is primitive
    if (CV_IS_PRIMITIVE(rti)) {
        return TRUE;
    }

    if (!isValidIndex(rti, false)) {
        // Leave a flag that this pool has some corruption.
        // REVIEW:  later, we may want to provide some more data here
        //
        fCorruptedFromTypePool() = true;
        rti = tiNil;
        return TRUE;
    }

    // return if the type has been mapped already or if it contains a cycle
    TI& rtiMap = rtiMapped(rti, false);
    if (rtiMap != tiNil) {
        rti = rtiMap;
        return TRUE;
    }

    PTYPE ptype = ptypeForTi(rti, false);

#if _DEBUG // {
#if !defined( PDB_LIBRARY )
    if (rgbEnableDiagnostic[0]) {
        StdOutPrintf(L"%d<%04X       ", depth, rti);
        dumpType(ptype);
    }
#endif
#endif // }

    // masm generated functions which return themselves, sigh
    if (ptype->leaf == LF_PROCEDURE) {
        lfProc* pproc = (lfProc*)&ptype->leaf;
        if (pproc->rvtype == rti)
            pproc->rvtype = T_VOID;
    }

    // Recursively map all TIs within the type record into the 'to' TypeServer
    // (overwriting in place the TI fields of this type record).
    // (Before we recurse, mark this type as T_NOTTRANS so we won't stack
    // overflow if the type graph happens to contain a cycle.)
    rtiMap = T_NOTTRANS;
    for (TypeTiIter tii(ptype); tii.next(); )
        if (!TMR::fMapRti(tii.rti(), tii.fId(), depth + 1, NULL))
            return FALSE;

    if ((pfDefnUdt != NULL) && !fID) {
        *pfDefnUdt = REC::fIsDefnUdt((PB) ptype);
    }

    // find TI for resulting record within the 'to' TypeServer

    TPI* pTo = fID ? pipiTo : ptpiTo;

    instrumentation(pdbi1To->info.cTypesQueried++);
    instrumentation(TI tiMacWas = pTo->QueryTiMac());

    TI tiTo;
    if (!pTo->QueryTiForCVRecord((PB)ptype, &tiTo) &&
        (ppdb1To->QueryLastErrorExW(NULL, 0) != EC_OUT_OF_TI))
        return FALSE;       // if we run out of type indices keep trying to pack

    instrumentation(pdbi1To->info.cTypesAdded += (tiMacWas != pTo->QueryTiMac()));

#if _DEBUG // {
#if !defined( PDB_LIBRARY )
    if (rgbEnableDiagnostic[0]) {
        StdOutPrintf(L"%d>%04X->%04X ", depth, rti, tiTo);
        dumpType(ptype);
    }
#endif
#endif // }

    // update rti and maps
    rti = rtiMapped(rti, false) = tiTo;

    return TRUE;
}

inline TI& TM::rtiMapped(TI ti, bool fID) const
{
    dassert(isValidIndex(ti, fID));

    TI id = indexBias(ti, fID);

    return fID ? mpidid[id] : mptiti[id];
}

inline TI& TMR::rtiMapped(TI ti, bool) const
{
    assert(isValidIndex(ti, false));

    if (ti < tiMin) {
        // forward to TMPCT
        assert(m_ptmpct);
        return m_ptmpct->rtiMapped(ti, false);
    }
    assert(mptiti);
    return mptiti[indexBias(ti, false)];
}

BOOL TM::fTiMapped(TI ti) const
{
    TI tiMapped = rtiMapped(ti, false);

    if (tiMapped < 2) {
        rtiMapped(ti, false) = tiMapped + 1;
        return FALSE;
    }

    return TRUE;
}

inline PTYPE TMR::ptypeForTi(TI ti, bool) const
{
    PTYPE ptype = NULL;
    assert(isValidIndex(ti, false) && !CV_IS_PRIMITIVE(ti));
    if (ti < tiMin) {
        // forward to TMPCT
        assert(m_ptmpct);
        ptype = m_ptmpct->ptypeForTi(ti, false);
    } else {
        assert(mptiptype);
        ptype = mptiptype[indexBias(ti, false)];
    }

    if (fConvertTypeRecStToSz(ptype))
        return ptype;
    else
        return NULL;
}

PTYPE pfn_ptypeForId(TM *ptm, TI ti)
{
    return ptm->ptypeForTi(ti, true);
}

inline BOOL TMR::QuerySrcLineForUDT(TI tiUdt, char **psz, DWORD *pLine)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    if (tiUdt < tiMin) {
        return m_ptmpct->QuerySrcLineForUDT(tiUdt, psz, pLine);
    }

    PTYPE ptype = NULL;

    if (!mptiUdtIdRec.map(tiUdt, &ptype)) {
        return FALSE;
    }

    assert(ptype->leaf == LF_UDT_SRC_LINE);

    lfUdtSrcLine *plf = (lfUdtSrcLine *) &ptype->leaf;

    if (pLine != NULL) {
        *pLine = plf->line;
    }

    size_t cch = 0;

    if (!((TPI1 *) pipiTo)->GetStringForId(plf->src, NULL, &cch, NULL, this, pfn_ptypeForId)) {
        return FALSE;
    }

    char *sz = new char[cch + 1];

    if (sz == NULL) {
        ppdb1To->setOOMError();
        return FALSE;
    }

    if (!((TPI1 *) pipiTo)->GetStringForId(plf->src, sz, NULL, NULL, this, pfn_ptypeForId)) {
        return FALSE;
    }

    sz[cch] = '\0';

    if (psz != NULL) {
        *psz = sz;
    }

    return TRUE;
}

inline BOOL TMR::QuerySrcIdLineForUDT(TI tiUdt, TI& srcId, DWORD& line)
{
    assert(isValidIndex(tiUdt, false) && !CV_IS_PRIMITIVE(tiUdt));

    if (tiUdt < tiMin) {
        return m_ptmpct->QuerySrcIdLineForUDT(tiUdt, srcId, line);
    }

    PTYPE ptype = NULL;

    if (!mptiUdtIdRec.map(tiUdt, &ptype)) {
        return FALSE;
    }

    assert(ptype->leaf == LF_UDT_SRC_LINE);

    lfUdtSrcLine *plf = (lfUdtSrcLine *) &ptype->leaf;

    srcId = plf->src;
    line = plf->line;

    return TRUE;
}

BOOL TMR::QueryTiForUDT(const char* sz, BOOL fCase, OUT TI* pti)
{
    if (!ppdb1To->FMinimal()) {
        return FALSE;
    }

    if (m_ptmpct != NULL && m_ptmpct->QueryTiForUDT(sz, fCase, pti)) {
        return TRUE;
    }

    NameMap *pNameMap = NULL;

    if (!pdbi1To->GetNameMap(true, &pNameMap)) {
        ppdb1To->setOOMError();
        return FALSE;
    }

    NI ni;

    if (!pNameMap->containsUTF8(sz, &ni)) {
        return FALSE;
    }

    return mpnitiUdt.map(ni, pti);
}

BOOL TMEQTS::QueryTiForUDT(const char* sz, BOOL fCase, OUT TI* pti)
{
    return ptpiTo->QueryTiForUDT((const char *) sz, fCase, pti);
}

BOOL TMTS::QueryTiForUDT(const char* sz, BOOL fCase, OUT TI* pti)
{
    return ptpiFrom->QueryTiForUDT((const char *) sz, fCase, pti);
}

#if _DEBUG
#if !defined( PDB_LIBRARY )

static const struct lfsz {
    USHORT  lf;
    SZ      sz;
} mplfszLeaf[] = {
#define LFNAME(x)   { x, #x }
    LFNAME(LF_MODIFIER), LFNAME(LF_POINTER), LFNAME(LF_ARRAY),
    LFNAME(LF_CLASS), LFNAME(LF_STRUCTURE), LFNAME(LF_UNION),
    LFNAME(LF_ENUM), LFNAME(LF_PROCEDURE), LFNAME(LF_MFUNCTION),
    LFNAME(LF_VTSHAPE), LFNAME(LF_COBOL0), LFNAME(LF_COBOL1),
    LFNAME(LF_BARRAY), LFNAME(LF_LABEL), LFNAME(LF_NULL),
    LFNAME(LF_NOTTRAN), LFNAME(LF_DIMARRAY), LFNAME(LF_VFTPATH),
    LFNAME(LF_PRECOMP), LFNAME(LF_ENDPRECOMP), LFNAME(LF_SKIP),
    LFNAME(LF_ARGLIST), LFNAME(LF_DEFARG), LFNAME(LF_LIST),
    LFNAME(LF_FIELDLIST), LFNAME(LF_DERIVED), LFNAME(LF_BITFIELD),
    LFNAME(LF_METHODLIST), LFNAME(LF_DIMCONU), LFNAME(LF_DIMCONLU),
    LFNAME(LF_DIMVARU), LFNAME(LF_DIMVARLU), LFNAME(LF_REFSYM),
    LFNAME(LF_BCLASS), LFNAME(LF_VBCLASS), LFNAME(LF_IVBCLASS),
    LFNAME(LF_ENUMERATE), LFNAME(LF_FRIENDFCN), LFNAME(LF_INDEX),
    LFNAME(LF_MEMBER), LFNAME(LF_STMEMBER), LFNAME(LF_METHOD),
    LFNAME(LF_NESTTYPE), LFNAME(LF_VFUNCTAB), LFNAME(LF_FRIENDCLS),
    LFNAME(LF_NUMERIC), LFNAME(LF_CHAR), LFNAME(LF_SHORT),
    LFNAME(LF_USHORT), LFNAME(LF_LONG), LFNAME(LF_ULONG),
    LFNAME(LF_REAL32), LFNAME(LF_REAL64), LFNAME(LF_REAL80),
    LFNAME(LF_REAL128), LFNAME(LF_QUADWORD), LFNAME(LF_UQUADWORD),
    LFNAME(LF_REAL48), LFNAME(LF_ONEMETHOD), LFNAME(LF_ALIAS), 
    LFNAME(LF_DATE), LFNAME(LF_DECIMAL), LFNAME(LF_UTF8STRING),
    { 0, "???" }
    // TODO : Add ST types leaves here
};

#define szFmt1 L">%08x"
#define szFmt2 L" %08x"

void dumpType(PTYPE ptype)
{
    for (int i = 0; mplfszLeaf[i].lf; i++) {
        if (mplfszLeaf[i].lf == ptype->leaf) {
            StdOutPrintf(L"%-14s", mplfszLeaf[i].sz);
            break;
        }
    }

    TypeTiIter tii(ptype);
    tii.next();
    for (TI* pti = (TI*)ptype; pti < (TI*)pbEndType(ptype); ++pti) {
        if (pti == &tii.rti()) {
            StdOutPrintf(szFmt1, *pti);
            tii.next();
        }
        else
            StdOutPrintf(szFmt2, *pti);

        // put out 32 bytes of data max (8 ushorts or 4 ulongs)
        if (pti > (TI*)ptype + (16 / sizeof(TI))) {
            StdOutPrintf(L"+");
            break;
        }
    }

    StdOutPrintf(L"\n");
}

#endif // PDB_LIBRARY
#endif // _DEBUG
