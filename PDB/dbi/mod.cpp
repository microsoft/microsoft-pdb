//////////////////////////////////////////////////////////////////////////////
// PDB Debug Information API Mod Implementation

#include "pdbimpl.h"
#include "dbiimpl.h"
#include <stdio.h>
#include "comenvi.h"
#include "cvhelper.h"
#include "utf8.h"
#include "debug_s.h"
#include "cvlines.h"
#include "set.h"

static void setDSRError(PDB1 * ppdb1, DSR_EC ec) {
    static EC xlateDsrEcToPdbEc[] = {
        EC_OK,
        EC_USAGE,
        EC_OUT_OF_MEMORY,
        EC_CORRUPT,
        EC_CORRUPT,
    };
    cassert(_countof(xlateDsrEcToPdbEc) == DSR_EC_MAX);
    assert(ec < DSR_EC_MAX);
    ppdb1->setLastError(xlateDsrEcToPdbEc[ec]);
};

// Declarations of some functions defined in sttosz.cpp
CB ConvertSymRecFmMBCSToUTF8(PSYM psymSrc, PSYM psymDest, CB cbDest);
BOOL fConvertSymRecStToSz(PB pbSrc, CB cbSrc, PB pbDest, CB *pcbDest, Array<OffMap>& rgOffMap);
BOOL fConvertSymRecStToSzWithSig(PB pbSrc, CB cbSrc, CvtSyms& cvtsyms);

Mod1::Mod1(PDB1* ppdb1_, DBI1* pdbi1_, IMOD imod_)
    : ppdb1(ppdb1_), pdbi1(pdbi1_), ptm(0), imod(imod_), fSymsAdded_S(FALSE),
    pwti(0), fECSymSeen(false), fAddLines(FALSE), fSymsAdded_T(FALSE),
    fCoffToC13Failed(false), snType(snNil), snId(snNil), imodTypeRef(imodNil),
    fOwnTM(0), fRefTM(0), fOwnTMR(0), fOwnTMPCT(0), fRefTMPCT(0),
    fSymsNeedStringTableFixup(false), fSymRecordsVerified(false),
    fHasTypeRef(FALSE)
#ifdef PDB_TYPESERVER
    , itsm(0)
#endif
{
    instrumentation(pdbi1->info.cModules++);
}

BOOL Mod1::fInit()
{
    MTS_ASSERT(pdbi1->m_csForMods);

    if (pdbi1->fWrite) {
        assert(!pdbi1->cvtsyms.fConverting());
        MODI* pmodi = pdbi1->pmodiForImod(imod);
        if (pmodi) {
            // invalidate the section contribution for this module
            pmodi->sc.isect = isectNil;
            if (!pdbi1->invalidateSCforMod(imod))
                return FALSE
                ;
            // We anticipate the new group of symbols will be the same
            // size as last time.
            expect(fAlign(cbSyms()));
            if (cbSyms() > 0 && !bufSyms.SetInitAlloc(cbSyms())) {
                ppdb1->setOOMError();
                return FALSE;
            }
        }

        if (!fInitRefBuffer(&rbufC13Lines)) {
            return FALSE;
        }

        if (!initFileInfo(0)) {
            // clear the file info as well
            return FALSE;
        }
    }
    else {
        CB cbModStream   = cbStream();
        CB cbModSyms     = cbSyms();
        CB cbModLines    = cbLines();
        CB cbModC13Lines = cbC13Lines();
    
        if (cbModStream < cbModSyms ||
            cbModStream - cbModSyms < cbModLines ||
            cbModStream - cbModSyms - cbModLines < cbModC13Lines) {
            ppdb1->setCorruptError();
            return FALSE;
        }
    }

    if (cvtsyms.fConverting() = pdbi1->cvtsyms.fConverting()) {
        if (pwti || WidenTi::fCreate(pwti, 1, wtiSymsNB10)) {
            return TRUE;
        }
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!pdbi1->fWrite && ppdb1->m_fNoTypeMerge) {
        MODTYPEREF mtr = {0};

        if (!pdbi1->QueryModTypeRef(imod, &mtr)) {
            return FALSE;
        }

        if (mtr.rectyp != S_MOD_TYPEREF) {
            fNoType = 1;
            return TRUE;
        }

        if (mtr.fOwnTMR) {
            fOwnTMR = 1;
            snType = (SN) mtr.word0;

            if (mtr.fRefTMPCT) {
                fRefTMPCT = 1;
                imodTypeRef = mtr.word1;
            }

            if (mtr.fOwnTMPCT) {
                fOwnTMPCT = 1;
                //snZ7Pch = *(unsigned short *) (pbEnd + sizeof(unsigned short));
            }
        }
        else if (mtr.fOwnTM) {
            fOwnTM = 1;
            snType = mtr.word0;
            snId = mtr.word1;
        }
        else if (mtr.fRefTM) {
            fRefTM = 1;
            imodTypeRef = mtr.word0;
        }
        else {
            assert(mtr.fNone);
            fNoType = 1;
        }
    }

    return TRUE;
}

Mod1::~Mod1()
{
    if (ptm) {
        assert(pdbi1->fWrite || ppdb1->FLazy() || ppdb1->FMinimal());
        ptm->endMod();
    }

    if (pwti)
        pwti->release();
}

INTV Mod1::QueryInterfaceVersion()
{
    return intv;
}

IMPV Mod1::QueryImplementationVersion()
{
    return impv;
}

BOOL Mod1::QuerySupportsEC()
{
    // We don't support EnC on ST PDB's anymore
    if (!IS_SZ_FORMAT_PDB(ppdb1))
        return FALSE;


    MTS_PROTECT(pdbi1->m_csForMods);
    MODI *pmodi = pdbi1->pmodiForImod(imod);
    return pmodi && pmodi->fECEnabled;
}

BOOL Mod1::EnCNeedReloadCompilerGeneratedPDB()
{
    assert(ppdb1->m_fMinimalDbgInfo);
    assert(ptm != NULL);

    return (!ptm->IsTMR() && !ptm->IsTMEQTS() && ptm->PPdbFrom() == NULL);
}

BOOL Mod1::EnCReleaseCompilerGeneratedPDB(BYTE* pbTypes, DWORD cb)
{
    assert(ppdb1->m_fMinimalDbgInfo);

    if (ptm == NULL) {
        BOOL fRet = GetTmts(pbTypes, cb, &ptm, true);

        if (ptm == NULL) {
            return fRet;
        }
    }

    assert(!ptm->IsTMEQTS() && !ptm->IsTMR());

    TMTS *ptmts = (TMTS *) ptm;

    // Do nothing if already closed

    if (ptmts->PPdbFrom() == NULL) {
        return TRUE;
    }

    ptmts->ClosePdbFrom();

    return TRUE;
}

BOOL Mod1::EnCReloadCompilerGeneratedPDB(BYTE* pbTypes, DWORD cb)
{
    dassert(pbTypes);
    dassert(ppdb1->m_fMinimalDbgInfo);
    dassert(ptm != NULL);
    dassert(!ptm->IsTMR() && !ptm->IsTMEQTS());

    PTYPE pts = (PTYPE) (pbTypes + sizeof(ULONG));

    SafeStackAllocator<0x210> allocator;

    wchar_t *szT = allocator.Alloc<wchar_t>(pts->len);
    wchar_t *szFullMapped = allocator.Alloc<wchar_t>(_MAX_PATH);

    if (NULL == szT || NULL == szFullMapped) {
        ppdb1->setOOMError();
        return FALSE;
    }

    SIG70  sig70;
    AGE    age;
    const wchar_t * szTSName = pdbi1->szGetTsInfo(pts, szT, szFullMapped, &sig70, &age);
    
    if (szTSName == NULL) {
        return FALSE;
    }

    PDB*  ppdbFrom;

    if (!pdbi1->fOpenPdb(sig70, age, szTSName, szObjFile(), &ppdbFrom)) {
        return FALSE;
    }

    TMTS *ptmts = (TMTS *)ptm;
    return ptmts->fInit(ppdbFrom);
}

BOOL Mod1::GetTmts(BYTE* pbTypes, DWORD cb, TM** pptm, BOOL fQueryOnly)
{
    // check for c7 signature - cannot handle pre c7

    ULONG sig = *(ULONG*)pbTypes;

    assert(IS_SZ_FORMAT_PDB(ppdb1));

    if (sig != CV_SIGNATURE_C7 &&
        sig != CV_SIGNATURE_C11 &&
        sig != CV_SIGNATURE_C13)
    {
        ppdb1->setCorruptError();
        return FALSE;
    }

    pbTypes += sizeof(ULONG);
    cb -= sizeof(ULONG);

    if (cb == 0) {
        // If there are no types, bail now...  The compiler sometimes emits
        // just the CV_SIGNATURE_C7 DWORD.
        return TRUE;
    }

    fHasTypeRef = TRUE;

    PTYPE ptype = (PTYPE) pbTypes;

    if (SZLEAFIDX(ptype->leaf) == LF_TYPESERVER) {
        lfTypeServer* pts = (lfTypeServer*)&ptype->leaf;

        if (pdbi1->QueryLazyTypes() && sig == CV_SIGNATURE_C11) {
            // using lazy types, just register the type server

#ifdef PDB_TYPESERVER
#if 0       // We got rid of creating type servers long time ago ...
            char szName[ _MAX_PATH ];
            return pdbi1->AddTypeServer(pts->signature, pts->age,
                szFromSt(szName, (ST)pts->name), szObjFile(), &itsm);
#endif
#else
            // We should never be here, 'cause we don't
            // use Lazy types anymore, hence no change
            assert(FALSE);
#endif
        }
        else {
            pdbi1->fGetTmts(ptype, szObjFile(), pptm, fQueryOnly);
        }
    }
    else if (ptype->leaf == LF_TYPESERVER2)
    {
        pdbi1->fGetTmts(ptype, szObjFile(), pptm, fQueryOnly);
    }
    else {
        assert(fQueryOnly == false);

        TPI* ptpi = pdbi1->GetTpi();
        TPI* pipi = pdbi1->GetIpi();

        if (ptpi != NULL) {
            if (*pptm = new (ppdb1) TMR(ppdb1, pdbi1, ptpi, pipi)) {
                if (!((TMR*) *pptm)->fInit(pbTypes, cb, szModule(), sig)) {
                    delete *pptm;
                    *pptm = NULL;
                }
            }
        }
    }

    if (!fQueryOnly && (*pptm == NULL)) {
        return FALSE;
    }

    return TRUE;
}

BOOL Mod1::AddTypes(PB pbTypes, CB cb)
{
    dassert(pbTypes);

    if (ptm != NULL) {
        if (EnCNeedReloadCompilerGeneratedPDB()) {
            return EnCReloadCompilerGeneratedPDB(pbTypes, cb);
        }
        else {
            // The linker might do this if it has two module of the same name in one lib
            ppdb1->setLastError(EC_TOO_MANY_MOD_ADDTYPE);
            return FALSE;
        }
    }

#if 0
    if (fSymsAdded){
        ppdb1->setUsageError();
        return FALSE;
    }
#endif

    BOOL fRet = GetTmts(pbTypes, cb, &ptm, false);
    
    if (ptm == NULL) {
        return fRet;
    }

    if (ppdb1->m_fNoTypeMerge && ptm->Imod() == imodNil) {
        ptm->SetImod(imod);

        if (ptm->IsTmpctOwner()) {
            ((TMR *) ptm)->Ptmpct()->SetImod(imod);
        }
    }

    return TRUE;
}

BOOL Mod1::AddTimeStamp(DWORD timestamp)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    dwTimeStamp = timestamp;

    return TRUE;
}

int
Mod1::ExceptionFilter(DWORD ec, _EXCEPTION_POINTERS *pei, Mod1 * pmod1)
{

    if (ec == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ||
        ec == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND)) {
        // Handle this exception

        assert(pei);
        assert(pei->ExceptionRecord);
        assert(pei->ExceptionRecord->ExceptionInformation[0]);
    
        
        // Report the missing/bad dll error
        PDelayLoadInfo pdli = PDelayLoadInfo(pei->ExceptionRecord->ExceptionInformation[0]);
        pmod1->ReportDelayLoadError(ec, pdli);

        return EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

void
Mod1::ReportDelayLoadError(DWORD ec, PDelayLoadInfo pdli)
{
    assert(pdli);

    char    szErr[_MAX_PATH * 2];

    // It was a delay-load exception.

    if (ec == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND)) {
        sprintf_s(szErr, _countof(szErr), "'%s'", pdli->szDll);
    }
    else {
        const char *    sz;
        char            szord[sizeof(" (ordinal)") + 17];

        // if it's an ordinal, compose the string "ordinal xx",
        // otherwise just pass in the name of the missing entrypoint.

        if (pdli->dlp.fImportByName) {
            sz = pdli->dlp.szProcName;
        }
        else {
            char szVal[17];

            sz = szord;

            sprintf_s(szord, sizeof(szord), "%d%s", pdli->dlp.dwOrdinal, szVal, " (ordinal)");
        }
        sprintf_s(szErr, _countof(szErr), "'%s'!'%s'", pdli->szDll, sz);
    }
    ppdb1->setLastError(EC_NOT_FOUND, szErr);
}

bool Mod1::CheckFCreateReader(PB pb, CB cb, IDebugSSectionReader** ppReader, DWORD sig)
{
#ifdef BSC_DLL
    return false; // We don't want msbscxx.dll to be dependent on msobj71.dll
#else
    DWORD ec;
    DSR_EC dsr_ec;
    __try { // make sure that ole libs are present
        if (!IDebugSSectionReader::FCreateReader(pb, cb, ppReader, sig, &dsr_ec)) {
            setDSRError(ppdb1, dsr_ec);
            return false;
        }
        return true;
    } 
    __except (ExceptionFilter(ec = GetExceptionCode(), GetExceptionInformation(), this)) {
        return false;
    }
#endif
}

bool Mod1::CheckFCreateWriter(bool flag, IDebugSSectionWriter** ppWriter, DWORD sig, bool flag2)
{
#ifdef BSC_DLL
    return false; // We don't want msbscxx.dll to be dependent on msobj71.dll
#else
    DWORD ec;
    __try { // make sure that ole libs are present
        return IDebugSSectionWriter::FCreateWriter(flag, ppWriter, sig, flag2);
    } 
    __except (ExceptionFilter(ec = GetExceptionCode(), GetExceptionInformation(), this)) {
        return false;
    }
#endif
}

template <class Type>
BOOL AppendSubSectionToBuffer(Type & buffer, IDebugSSubSection * pSubSection)
{
    PB    pbSec = NULL;
    DWORD cbSec = static_cast<DWORD>(pSubSection->GetRawBytes(&pbSec));

    if (!buffer.Append(pbSec, cbSec)) {
        return FALSE;
    }

    if (!buffer.AppendFmt("f", (int)dcbAlign(static_cast<size_t>(buffer.Size())))) {
        return FALSE;
    }

    return TRUE;
}


BOOL Mod1::MergeTypes(PB pb, DWORD cb)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    if (ptm == NULL) {
        return TRUE;
    }

    if (!fSymsAdded_T) {
        fSymsAdded_T = TRUE;
        sigSyms_T = *(ULONG*) pb;
    }

    if (sigSyms_T != CV_SIGNATURE_C11 && sigSyms_T != CV_SIGNATURE_C13) {
        // Return an error code to let the linker know that we can't do
        // early type merging in this case.

        return FALSE;
    }

    if ((sigSyms_T == CV_SIGNATURE_C13) && (*(ULONG*) pb != CV_SIGNATURE_C13)) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    if (sigSyms_T == CV_SIGNATURE_C13) {
        COMRefPtr<IDebugSSectionReader> pReader;
        if (!CheckFCreateReader(pb, cb, &pReader, CV_SIGNATURE_C13)) {
            return FALSE;
        }

        COMRefPtr<IDebugSSubSectionEnum> pEnum;
        if (!pReader->GetSectionEnum(&pEnum)) {
            setDSRError(ppdb1, pReader->GetLastError());
            return FALSE;
        }

        while (pEnum->Next()) {
            COMRefPtr<IDebugSSubSection> pSubSection;
            pEnum->Get(&pSubSection);

            if (pSubSection == NULL) {
                setDSRError(ppdb1, pReader->GetLastError());
                return FALSE;
            }

            PB pbSec = NULL;
            DWORD cbSec = 0;

            if (pSubSection->Type() != DEBUG_S_SYMBOLS) {
                continue;
            }

            cbSec = static_cast<DWORD>(pSubSection->GetData(&pbSec));

            PSYM psymNext;
            PB   pbMac = pbSec + cbSec;

            for (PSYM psym = (PSYM) pbSec; (PB) psym < pbMac; psym = psymNext) {
                psymNext = (PSYM) pbEndSym(psym);

                if ((PB) psymNext > pbMac) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }

                if (ptm && ptm->IsTmpctOwner() && (psym->rectyp == S_OBJNAME)) {
                    UTFSZ_CONST szObj = szCopy((char *) ((OBJNAMESYM *) psym)->name);

                    if (!pdbi1->fUpdateTmpct(szModule(), szObj, ptm)) {
                        return FALSE;
                    }
                }

                if (!packType(psym)) {
                    return FALSE;
                }
            }
        }
    }
    else {
        assert(sigSyms_T == CV_SIGNATURE_C11);

        PSYM psymNext;
        PB   pbMac = pb + cb;
        PB   pbMin = (*(ULONG *) pb == CV_SIGNATURE_C11) ? pb + sizeof(ULONG) : pb;

        for (PSYM psym = (PSYM) pbMin; (PB) psym < pbMac; psym = psymNext) {
            psymNext = (PSYM) pbEndSym(psym);

            if ((PB) psymNext > pbMac) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            if (ptm && ptm->IsTmpctOwner() && (psym->rectyp == S_OBJNAME_ST)) {
                UTFSZ_CONST szObj = szCopySt((char *) ((OBJNAMESYM *) psym)->name);

                if (!pdbi1->fUpdateTmpct(szModule(), szObj, ptm)) {
                    return FALSE;
                }
            }

            if (!packType(psym)) {
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL Mod1::fAddModTypeRefSym()
{
    if (!ppdb1->m_fNoTypeMerge) {
        return TRUE;
    }

    MODTYPEREF mtr = {0};

    mtr.rectyp = S_MOD_TYPEREF;
    mtr.reclen = sizeof(MODTYPEREF) - sizeof(unsigned short);

    if (!ptm) {
        mtr.fNone = 1;
    }
    else if (ptm->IsTMR()) {
        assert(ptm->Imod() == imod);

        mtr.fOwnTMR = 1;

        if (ptm->IsTmpctOwner()) {
            mtr.fOwnTMPCT = 1;
        }
        else if (((TMR *) ptm)->Ptmpct()) {
            mtr.fRefTMPCT = 1;
            mtr.word1 = ((TMR *) ptm)->Ptmpct()->Imod();
        }
    }
    else if (ptm->Imod() == imod) {
        mtr.fOwnTM = 1;
    }
    else {
        mtr.fRefTM = 1;
        mtr.word0 = ptm->Imod();
    }

    return AddJustSymbols((PB) &mtr, sizeof(MODTYPEREF), false);
}

BOOL Mod1::fAddSymRefToGSI(PB pb, DWORD cb, DWORD isectCoff)
{
    assert(pdbi1->fWrite);
    assert(IS_SZ_FORMAT_PDB(ppdb1));
    dassert(pb);

    DWORD sig = *(DWORD *) pb;

    if (sig == CV_SIGNATURE_C7 ||
        sig == CV_SIGNATURE_C11 ||
        sig == CV_SIGNATURE_C13) {
        pb += sizeof(DWORD);
        cb -= sizeof(DWORD);
    }

    int iLevel = 0;
    PSYM psymMac = (PSYM) (pb + cb);

    for (PSYM psym = (PSYM) pb; psym < psymMac; psym = (PSYM) pbEndSym(psym)) {
        if (((PB) psym) + cbForSym(psym) > (PB) psymMac) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        bool fLocal = false;

        switch (psym->rectyp) {
            case S_GPROC16:
            case S_GPROCMIPS:
            case S_GPROCIA64:
            case S_LPROC16:
#if defined(CC_DP_CXX)
            case S_LPROC32_DPC:
#endif // CC_DP_CXX
            case S_LPROCMIPS:
            case S_LPROCIA64:
            case S_GPROCMIPS_ID:
            case S_GPROCIA64_ID:
#if defined(CC_DP_CXX)
            case S_LPROC32_DPC_ID:
#endif // CC_DP_CXX
            case S_LPROCMIPS_ID:
            case S_LPROCIA64_ID:
            case S_BLOCK16:
            case S_BLOCK32:
            case S_WITH16:
            case S_WITH32:
            case S_THUNK16:
            case S_THUNK32:
            case S_SEPCODE:
            case S_GMANPROC:
            case S_LMANPROC:
            case S_INLINESITE2:
            case S_INLINESITE:
                assert(iLevel >= 0);
                iLevel++;
                break;

            case S_END:
            case S_INLINESITE_END:
            case S_PROC_ID_END:
                iLevel--;
                break;

            /*--
            typedef struct REFMINIPDB {
                unsigned short  reclen;             // Record length
                unsigned short  rectyp;             // S_REF_MINIPDB
                union {
                    unsigned long  isectCoff;       // coff section
                    CV_typ_t       typind;          // type index
                };
                unsigned short  imod;               // mod index
                unsigned short  fLocal   :  1;      // reference to local (vs. global) func or data
                unsigned short  fData    :  1;      // reference to data (vs. func)
                unsigned short  fUDT     :  1;      // reference to UDT
                unsigned short  fLabel   :  1;      // reference to label
                unsigned short  fConst   :  1;      // reference to const
                unsigned short  reserved : 11;      // reserved, must be zero
                unsigned char   name[1];            // zero terminated name string
            } REFMINIPDB;
            --*/

            case S_UDT:
                {
                    UDTSYM *psymUdt = (UDTSYM *) psym;
                    bool fPrimitive = CV_IS_PRIMITIVE(psymUdt->typind);

                    assert((imod >> CodeViewInfo::ComboID::ImodBitWidth) == 0);
                    assert((psymUdt->typind >> CodeViewInfo::ComboID::IndexBitWidth) == 0);

                    if (!fPrimitive) {
                        assert(ptm != NULL);

                        if (ptm->fTiMapped(psymUdt->typind)) {
                            break;
                        }
                    }

                    if (!pdbi1->packRefToGSForMiniPDB((char *) psymUdt->name,
                                                      imod + 1,
                                                      fPrimitive ? isectCoff : psymUdt->typind,
                                                      (iLevel != 0) ? true : false,
                                                      fPrimitive ? true : false,
                                                      fPrimitive ? false : true,
                                                      false,
                                                      false)) {
                        return FALSE;
                    }
                }
                break;

            case S_CONSTANT:
                {
                    if (iLevel != 0) {
                        break;
                    }

                    ST st;

                    if (!fGetSymName(psym, &st)) {
                        assert(false);
                        return FALSE;
                    }

                    if (!pdbi1->packRefToGSForMiniPDB(st,
                                                      imod + 1,
                                                      isectCoff,
                                                      false,
                                                      false,
                                                      false,
                                                      false,
                                                      true)) {
                        return FALSE;
                    }
                }
                break;

            case S_LPROC32:
            case S_LPROC32_ID:
                fLocal = true;

                // fall through

            case S_GPROC32:
            case S_GPROC32_ID:
                if (iLevel == 0) {
                    if (!pdbi1->packRefToGSForMiniPDB((char *) ((PROCSYM32 *) psym)->name,
                                                      imod + 1,
                                                      isectCoff,
                                                      fLocal,
                                                      false,
                                                      false,
                                                      false,
                                                      false)) {
                        return FALSE;
                    }
                }

                assert(iLevel >= 0);
                iLevel++;
                break;

            case S_LDATA32:
                fLocal = true;

                // fall through

            case S_GDATA32:
                if (iLevel != 0) {
                    break;
                }

                if (!pdbi1->packRefToGSForMiniPDB((char *) ((DATASYM32 *) psym)->name,
                                                  imod + 1,
                                                  isectCoff,
                                                  fLocal,
                                                  true,
                                                  false,
                                                  false,
                                                  false)) {
                    return FALSE;
                }
                break;

            case S_LTHREAD32:
                fLocal = true;

                // fall through

            case S_GTHREAD32:
                if (iLevel != 0) {
                    break;
                }

                if (!pdbi1->packRefToGSForMiniPDB((char *) ((THREADSYM32 *) psym)->name,
                                                  imod + 1,
                                                  isectCoff,
                                                  fLocal,
                                                  true,
                                                  false,
                                                  false,
                                                  false)) {
                    return FALSE;
                }
                break;

#if 0
            case S_LABEL32:
                if (iLevel != 0) {
                    break;
                }

                if (!pdbi1->packRefToGSForMiniPDB((char *) ((LABELSYM32 *) psym)->name,
                                                  imod + 1,
                                                  isectCoff,
                                                  fLocal,
                                                  false,
                                                  false,
                                                  true,
                                                  false)) {
                    return FALSE;
                }
                break;
#endif

            default:
                break;
        }
    }

    assert(iLevel == 0);
    return TRUE;
}

BOOL Mod1::AddSymbols(PB pbSym, CB cb)
{
    return fAddSymbols(pbSym, cb, 0);
}

BOOL Mod1::AddSymbols2(PB pbSym, DWORD cb, DWORD isectCoff)
{
    return fAddSymbols(pbSym, cb, isectCoff);
}

BOOL Mod1::fAddSymbols(PB pbSym, DWORD cb, DWORD isectCoff)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    ULONG   sigSymsT = sigSyms_S;

    if (!fSymsAdded_S) {
         sigSymsT = *(ULONG*)pbSym;
    } 

    if (sigSymsT == CV_SIGNATURE_C13) {
        if (!fSymsAdded_S) {
            if (!AddJustSymbols(pbSym, sizeof(ULONG), false)) {
                return FALSE;
            }

            if (!fAddModTypeRefSym()) {
                return FALSE;
            }
        }

        assert(fSymsAdded_S);

        if (*(ULONG*) pbSym != CV_SIGNATURE_C13) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        COMRefPtr<IDebugSSectionReader> pReader;
        if (!CheckFCreateReader(pbSym, cb, &pReader, CV_SIGNATURE_C13)) {
            return FALSE;
        }
        COMRefPtr<IDebugSSubSectionEnum> pEnum;
        if (!pReader->GetSectionEnum(&pEnum)) {
            setDSRError(ppdb1, pReader->GetLastError());
            return FALSE;
        }
        while (pEnum->Next()) {
            COMRefPtr<IDebugSSubSection> pSubSection;
            pEnum->Get(&pSubSection);
            if (pSubSection == NULL) {
                setDSRError(ppdb1, pReader->GetLastError());
                return FALSE;
            }
            PB pbSec = NULL;
            DWORD cbSec = 0;
            switch (pSubSection->Type()) {
            case DEBUG_S_SYMBOLS: // add to symbols substream
                cbSec = static_cast<DWORD>(pSubSection->GetData(&pbSec));
                if (ppdb1->m_fMinimalDbgInfo) {
                    if (!fAddSymRefToGSI(pbSec, cbSec, isectCoff)) {
                        return FALSE;
                    }
                    break;
                }
                if (!AddJustSymbols(pbSec, cbSec, ppdb1->m_fNoTypeMerge)) {
                    return FALSE;
                }
                break;

            case DEBUG_S_FRAMEDATA:
                if (!AppendSubSectionToBuffer(bufC13Fpo, pSubSection)) {
                    ppdb1->setOOMError(); 
                    return FALSE;
                }
                break;
            case DEBUG_S_STRINGTABLE:
                if (!AppendSubSectionToBuffer(bufC13Strings, pSubSection)) {
                    ppdb1->setOOMError(); 
                    return FALSE;
                }
                break;
            case DEBUG_S_CROSSSCOPEIMPORTS:
            case DEBUG_S_CROSSSCOPEEXPORTS:
            case DEBUG_S_INLINEELINES:
                if (ppdb1->m_fMinimalDbgInfo) {
                    break;
                }
            case DEBUG_S_LINES:
            case DEBUG_S_COFF_SYMBOL_RVA:
            case DEBUG_S_FUNC_MDTOKEN_MAP:
            case DEBUG_S_FILECHKSMS:
            case DEBUG_S_IL_LINES:
            case DEBUG_S_TYPE_MDTOKEN_MAP:
            case DEBUG_S_MERGED_ASSEMBLYINPUT:
                // ### TODO: process file names here?
                if (!AppendSubSectionToBuffer(*rbufC13Lines, pSubSection)) {
                    ppdb1->setOOMError(); 
                    return FALSE;
                }
                break;

            default:
                break;  // just ignore subsections that we don't understand
            }
        }
        return TRUE;
    }
    else
    {    // !CV_SIGNATURE_C13
        return AddJustSymbols(pbSym, cb, ppdb1->m_fNoTypeMerge);
    }
}

BOOL Mod1::RemoveGlobalRefs()
{
    if (!pdbi1->fWrite || !ppdb1->m_fMinimalDbgInfo) {
        ppdb1->setUsageError();
        return FALSE;
    }

    return pdbi1->removeGlobalRefsForMod(imod + 1);
}

static bool ConvertSymRecFmMBCSToUTF8(PSYM psymIn, Buffer& buf)
{
    buf.Truncate(0);
    if (!buf.Reserve(0x10000)) {
        return false;
    }
    return ConvertSymRecFmMBCSToUTF8(psymIn, reinterpret_cast<PSYM>(buf.Start()), buf.Size()) != -1;
}


// For each symbol in the group of symbols in the buffer,
// ensure any TIs within the symbol properly refer to type records
// in the project PDB.
//
// Note: the symbol buffer is modified in place, as TIs are updated.
//
BOOL Mod1::AddJustSymbols(PB pbSym, CB cb, bool fFixupLevel)
{
    assert(pdbi1->fWrite);
    assert(IS_SZ_FORMAT_PDB(ppdb1));
    dassert(pbSym);

    PSYM    psymMac = (PSYM)(pbSym + cb);
    
    ULONG   sigSymsT = *(ULONG*)pbSym;

    if (sigSymsT == CV_SIGNATURE_C7 ||
        sigSymsT == CV_SIGNATURE_C11 ||
        sigSymsT == CV_SIGNATURE_C13)
    {
        ULONG   sigSymsWrite;

        sigSyms_S = sigSymsT;
        sigSymsWrite = CV_SIGNATURE_C13;

        if (!fSymsAdded_S && !bufSyms.Append(PB(&sigSymsWrite), sizeof(ULONG))) {
            ppdb1->setOOMError();
            return FALSE;
        }

        pbSym += sizeof(ULONG);
        fSymsAdded_S = TRUE;

        if (sigSymsT != CV_SIGNATURE_C13 && !fAddModTypeRefSym()) {
            return FALSE;
        }
    }
    else if (!fSymsAdded_S) {
        ppdb1->setCorruptError();
        return FALSE;
    }
    fSymsAdded_S = TRUE;

    if (sigSyms_S != CV_SIGNATURE_C11 && sigSyms_S != CV_SIGNATURE_C13 && !pwti) {
        // need to widen records
        if (!WidenTi::fCreate(pwti, 1, wtiSymsNB10)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    // make pass thru incoming records and perform alignment 
    // if necessary and copy to local syms buffer

    DWORD offParent = 0;
    int iLevel = 0;

    Buffer bufSym;                  // Used to convert ST records to SZ records

    for (PSYM psym = (PSYM)pbSym; psym < psymMac; psym = (PSYM)pbEndSym(psym)) {
        if (((PB) psym) + cbForSym(psym) > (PB) psymMac) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        PSYM psymT = psym;

        if (pwti) {
            psymT = PSYM(pwti->pSymWidenTi(PB(psymT)));
            if (!psymT) {
                ppdb1->setOOMError();
                return FALSE;
            }
        }

        cassert(sizeof(psym->reclen) == 2);
    
        if (sigSyms_S != CV_SIGNATURE_C13) {
            if (!ConvertSymRecFmMBCSToUTF8(psymT, bufSym)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            psymT = (PSYM)bufSym.Start();
            assert(dcbAlign(cbForSym(psymT)) == 0);
        }

        PSYM pbLastWrite;
        if (!bufSyms.Append((PB) psymT, cbForSym(psymT), (PB*) &pbLastWrite)) {
            ppdb1->setOOMError();
            return FALSE;
        }

        expect(fAlign(pbLastWrite));

        if (!fAlign(cbForSym(psymT))) {
            // need alignment - adjust reclen in the local sym buffer and append the
            // adjustment
            pbLastWrite->reclen += (USHORT) dcbAlign(cbForSym(psymT));
            if (!bufSyms.AppendFmt("f", dcbAlign(cbForSym(psymT)))) {
                ppdb1->setOOMError();
                return FALSE;
            }
        }

        if (!ppdb1->m_fMinimalDbgInfo &&
             (   psymT->rectyp == S_DEFRANGE 
              || psymT->rectyp == S_DEFRANGE_SUBFIELD
              || psymT->rectyp == S_FILESTATIC)) {
            // These symbols have string table offsets, and would need to be fixed up
            fSymsNeedStringTableFixup = true;
        }

        // Update the TMPCT's module name with what is written in S_OBJNAME record.

        if (ppdb1->m_fMinimalDbgInfo || ppdb1->m_fNoTypeMerge) {
            if (psymT->rectyp == S_OBJNAME && ptm && ptm->IsTmpctOwner() &&
                !pdbi1->fUpdateTmpct(szModule(), szCopy((char*)((OBJNAMESYM *)psym)->name), ptm)) {
                return FALSE;
            }
        }

        if (fFixupLevel) {
            switch (psymT->rectyp) {
                case S_GPROC16:
                case S_GPROC32:
                case S_GPROCMIPS:
                case S_GPROCIA64:
                case S_LPROC16:
                case S_LPROC32:
#if defined(CC_DP_CXX)
                case S_LPROC32_DPC:
#endif // CC_DP_CXX
                case S_LPROCMIPS:
                case S_LPROCIA64:
                case S_GPROC32_ID:
                case S_GPROCMIPS_ID:
                case S_GPROCIA64_ID:
#if defined(CC_DP_CXX)
                case S_LPROC32_DPC_ID:
#endif // CC_DP_CXX
                case S_LPROC32_ID:
                case S_LPROCMIPS_ID:
                case S_LPROCIA64_ID:
                case S_BLOCK16:
                case S_BLOCK32:
                case S_WITH16:
                case S_WITH32:
                case S_THUNK16:
                case S_THUNK32:
                case S_SEPCODE:
                case S_GMANPROC:
                case S_LMANPROC:
                case S_INLINESITE2:
                case S_INLINESITE:
                    reinterpret_cast<BLOCKSYM *>(pbLastWrite)->pParent = offParent;
                    offParent = ULONG((PB) pbLastWrite - bufSyms.Start());   // REVIEW:WIN64 CAST
                    iLevel++;
                    break;

                case S_END:
                case S_INLINESITE_END:
                case S_PROC_ID_END:
                    reinterpret_cast<BLOCKSYM *>(bufSyms.Start() + offParent)->pEnd = ULONG((PB) pbLastWrite - bufSyms.Start());
                    offParent = reinterpret_cast<BLOCKSYM *>(bufSyms.Start() + offParent)->pParent;
                    assert(iLevel != 0);
                    iLevel--;
                    break;

                default:
                    break;
            }
        }
    }

    if (iLevel) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    if (pwti) {
        pwti->release();
        pwti = 0;
    }

    return TRUE;
}

BOOL Mod1::AddPublic(SZ_CONST szPublic, ISECT isect, OFF off)
{
    return AddPublic2(szPublic, isect, off, cvpsfNone);
}


BOOL Mod1::AddPublic2(SZ_CONST szPublic, ISECT isect, OFF off, CV_pubsymflag_t cvpsf)
{
    // This should be in ASCII, same as UTF8,
    // don't bother to change
    return pdbi1->AddPublic2(szPublic, isect, off, cvpsf);
}


BOOL Mod1::ReplaceLines(BYTE* pbLines, long cb)
{
    if (!pdbi1->fWrite || fAddLines || bufLines.Size()) {
        ppdb1->setUsageError();
        return FALSE;
    }

    if (!bufLines.Append(pbLines, cb)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    return TRUE;
}


BOOL Mod1::InsertLines(BYTE* pbLines, long cb)
{
    Buffer lines;
    if (!ConvertFileNamesInLineInfoFmMBCSToUnicode(pbLines, cb, lines)) {
        return FALSE;
    }

    return ReplaceLines(lines.Start(), lines.Size());
}


BOOL Mod1::fUpdateLines()
{
    assert(pdbi1->fWrite);

    if (m_pLinesWriter1st != NULL) {
        // We had COFF lines, DebugSFileChksm of which is 
        // in m_pLinesWriter1st. Emit it to bufC13Lines

        if (!m_pLinesWriter1st->EndSection()) {
            ppdb1->setOOMError();
            return FALSE;
        }

        if (!fInitRefBuffer(&rbufC13Lines)) {
            return FALSE;
        }

        PB pbRaw = NULL;
        CB cbRaw = static_cast<CB>(m_pLinesWriter1st->GetSectionBytes(&pbRaw));
        if (cbRaw && !rbufC13Lines->Append(pbRaw+sizeof(DWORD), cbRaw-sizeof(DWORD))) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    return TRUE;
}

BOOL Mod1::QuerySecContrib(OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb, OUT DWORD* pdwCharacteristics)
{
    MTS_PROTECT(pdbi1->m_csForMods);
    MODI* pmodi = pdbi1->pmodiForImod(imod);

    if (!pmodi) {
        ppdb1->setUsageError();
        return FALSE;
    }

    if (pisect) *pisect = pmodi->sc.isect;
    if (poff) *poff = pmodi->sc.off;
    if (pcb) *pcb = pmodi->sc.cb;
    if (pdwCharacteristics) *pdwCharacteristics = pmodi->sc.dwCharacteristics;
    return TRUE;
}

BOOL Mod1::AddSecContribEx(USHORT isect, long off, long cb, ULONG dwCharacteristics, DWORD dwDataCrc, DWORD dwRelocCrc)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForMods);

    pdbi1->UnsetSCVersion2();

    if (fUpdateSecContrib()) {

        sc.isect = isect;
        sc.off = off;
        sc.cb = cb;
        sc.dwCharacteristics = dwCharacteristics;
        sc.imod = imod;
        sc.dwDataCrc = dwDataCrc;
        sc.dwRelocCrc = dwRelocCrc;

        return TRUE;
    }

    return FALSE;
}

BOOL Mod1::AddSecContrib(ISECT isect, OFF off, CB cb, DWORD dwCharacteristics)
{
    return AddSecContribEx(isect, off, cb, dwCharacteristics, 0, 0);
}

BOOL Mod1::AddSecContrib2Ex(USHORT isect, DWORD off, DWORD isectCoff, DWORD cb, ULONG dwCharacteristics, DWORD dwDataCrc, DWORD dwRelocCrc)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForMods);

    pdbi1->SetSCVersion2();

    if (fUpdateSecContrib()) {

        sc.isect = isect;
        sc.off = off;
        sc.isectCoff = isectCoff;
        sc.cb = cb;
        sc.dwCharacteristics = dwCharacteristics;
        sc.imod = imod;
        sc.dwDataCrc = dwDataCrc;
        sc.dwRelocCrc = dwRelocCrc;

        return TRUE;
    }

    return FALSE;
}

BOOL Mod1::AddSecContrib2(USHORT isect, DWORD off, DWORD isectCoff, DWORD cb, DWORD dwCharacteristics)
{
    return AddSecContrib2Ex(isect, off, isectCoff, cb, dwCharacteristics, 0, 0);
}

// used by Mod1::Close() and Mod1::AddSecContribEx()
BOOL Mod1::fUpdateSecContrib()
{
    assert(pdbi1->fWrite);
    MTS_ASSERT(pdbi1->m_csForMods);

    if (sc.isect == isectNil)
        return TRUE;

    if (!pdbi1->addSecContrib(sc))
        return FALSE;

    MODI* pmodi = pdbi1->pmodiForImod(imod);
    if (pmodi->sc.isect == isectNil &&
        (sc.dwCharacteristics & IMAGE_SCN_CNT_CODE))
    {
        //fill in first code sect contribution
        memcpy(&pmodi->sc, &sc, sizeof(SC));
    }

    return TRUE;

}

BOOL Mod1::QuerySymbols(PB pbSym, CB* pcb)
{
    MTS_PROTECT(pdbi1->m_csForMods);
    BOOL fResult;

    if (!IS_SZ_FORMAT_PDB(ppdb1)) {
        fResult = fReadAndConvertStSyms(pbSym, pcb);
    }
    else
    {
        if (cvtsyms.fConverting()) {
            assert(pwti);
            fResult = fReadAndConvertSyms(pbSym, pcb);
        }
        else {
            fResult = fReadPbCb(pbSym, pcb, 0, cbSyms());
        }

    }

    if (pbSym && fResult && !fSymRecordsVerified) {
        fResult = VerifySymbols(pbSym, *pcb);
    }

    return fResult;
}

BOOL Mod1::QueryTypes(PB pb, DWORD *pcb)
{
    if (!ppdb1->m_fNoTypeMerge) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForMods);

    if (fNoType || fRefTM) {
        *pcb = 0;
        return TRUE;
    }

    assert(fOwnTM || fOwnTMR);
    return fReadFromStream(snType, pb, pcb);

}

BOOL Mod1::QueryIDs(PB pb, DWORD *pcb)
{
    if (!ppdb1->m_fNoTypeMerge) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForMods);

    if (fNoType || fRefTM) {
        *pcb = 0;
        return TRUE;
    }

    if (fOwnTMR && !fOwnTMPCT) {
        *pcb = 0;
        return TRUE;
    }

    if (fOwnTM && snId == snNil) {
        *pcb = 0;
        return TRUE;
    }

    return fReadFromStream(snId, pb, pcb);
}

BOOL Mod1::IsTypeServed(DWORD index, BOOL fID)
{
    if (!ppdb1->m_fNoTypeMerge && !ppdb1->m_fMinimalDbgInfo) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForMods);

    assert(ptm != NULL);

    return ptm->fVerifyIndex(index, fID);
}

BOOL Mod1::fQueryCVRecordForTi(DWORD index, BOOL fID, PTYPE *pptype, DWORD *pcb)
{
    MTS_ASSERT(pdbi1->m_csForMods);

    if (!ppdb1->m_fNoTypeMerge && !ppdb1->m_fMinimalDbgInfo) {
        ppdb1->setUsageError();
        return FALSE;
    }

    assert(pptype != NULL);
    assert(ptm != NULL);

    PTYPE ptype = ptm->ptypeForTi(index, fID);

    bool fUDT = ptype->leaf == LF_CLASS || ptype->leaf == LF_STRUCTURE ||
                ptype->leaf == LF_ENUM || ptype->leaf == LF_UNION ||
                ptype->leaf == LF_INTERFACE;

    if (fUDT) {
        lfClass *pclass = (lfClass *) &ptype->leaf;

        if (pclass->property.fwdref) {
            // UNDONE -- Go fetch the non fwdref type record
        }
    }

    *pptype = ptype;

    if (pcb != NULL) {
        *pcb = REC::cbForPb((PB) ptype);
    }

    return TRUE;
}

BOOL Mod1::QueryCVRecordForTi(DWORD index, BOOL fID, PB pb, DWORD *pcb)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    PTYPE ptype;
    DWORD cb;

    if (!fQueryCVRecordForTi(index, fID, &ptype, &cb)) {
        return FALSE;
    }

    if (pb != NULL) {
        memcpy(pb, ptype, min(cb, *pcb));
    }

    *pcb = cb;
    return TRUE;
}

BOOL Mod1::QueryPbCVRecordForTi(DWORD index, BOOL fID, PB *ppb)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    assert(ppb != NULL);

    PTYPE ptype;
    DWORD cb;

    if (!fQueryCVRecordForTi(index, fID, &ptype, &cb)) {
        return FALSE;
    }

    *ppb = (PB) ptype;
    return TRUE;
}

BOOL Mod1::QuerySrcLineForUDT(TI ti, char **pszSrc, DWORD *pLine)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    return ptm->QuerySrcLineForUDT(ti, pszSrc, pLine);
}

BOOL Mod1::QueryTiForUDT(const char *sz, BOOL fCase, TI *pti)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    assert(sz != NULL);
    assert(pti != NULL);
    assert(ptm != NULL);

    return ptm->QueryTiForUDT(sz, fCase, pti);
}

BOOL Mod1::QueryCoffSymRVAs(PB pb, DWORD *pcb)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    assert(pcb != NULL);

    if (!ppdb1->m_fMinimalDbgInfo) {
        *pcb = 0;
        return TRUE;
    }

    if (!fQueryC13LinesBuf(0, NULL, pcb, DEBUG_S_COFF_SYMBOL_RVA)) {
        return FALSE;
    }

    if (pb == NULL) {
        return TRUE;
    }

    DWORD cb = *pcb;

    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_COFF_SYMBOL_RVA);
}

BOOL Mod1::QueryLines(PB pbLines, CB* pcb)
{
    if (cbC13Lines()) {
        // We have C13 lines, can't have C11 lines.
        expect(cbLines() == 0);
        if (bufLines.Size() || GetC11LinesFromC13(bufLines)) {
            if (pbLines && *pcb) {
                CB cbCopy = min(bufLines.Size(), *pcb);
                memcpy(pbLines, bufLines.Start(), cbCopy);
                if (*pcb >= cbCopy) {
                    *pcb = bufLines.Size();
                    bufLines.Free();
                }
            }
            else {
                *pcb = bufLines.Size();
            }
            return TRUE;
        }
        return FALSE;
    }
    else {

        CB cbMax = *pcb;
        BOOL fResult = fReadPbCb(pbLines, pcb, cbSyms(), cbLines());

        if (fResult && !IS_SZ_FORMAT_PDB(ppdb1) && *pcb) {
            PB                          pb = NULL;
            Buffer                      buffer;
            SafeStackAllocator<0x1000>  allocator;

            if (pbLines == NULL || *pcb < cbLines()) {
                assume(cbLines() >= 0);
                pb = allocator.Alloc<BYTE>(cbLines());
                *pcb = cbLines();
                if (NULL == pb) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                if (!fReadPbCb(pb, pcb, cbSyms(), cbLines())) {
                    return FALSE;
                }
            }
            else {
                pb = pbLines;
            }

            // We have all the filenames in MBCS format
            // change them to Unicode before returning
            fResult = ConvertFileNamesInLineInfoFmMBCSToUnicode(pb, *pcb, buffer);

            if (fResult && cbMax >= buffer.Size() && pbLines != NULL) {
                memcpy(pbLines, buffer.Start(), buffer.Size());
            }
            else {
                fResult = (pbLines == NULL);        // Not enough space to copy result
            }
                
            *pcb = buffer.Size();
        }
        return fResult;
    }
}

BOOL Mod1::QueryLines2(long cbLines, BYTE* pbLines, long* pcbLines)
{
    if (!findC13Lines()) {
        return FALSE;
    }

    if (cbLines && pbLines) {
        *pcbLines = min(cbLines, rbufC13Lines->Size());
        memcpy(pbLines, rbufC13Lines->Start(), *pcbLines);
        return *pcbLines <= cbLines;
    }
    else {
        *pcbLines = rbufC13Lines->Size();
        return true;
    }
}

BOOL Mod1::VerifySymbols(PB pbSym, CB cb)
{
    // Verify the record lengths for those records that we've read in are reasonable.
    // Reasonable means that if we walk through the symbols, we hit exactly the end
    // of the symbol buffer that we've read into, rather than going beyond it's extent.

    dassert(!fSymRecordsVerified);

    ULONG sigSymsT = *(ULONG*)pbSym;

    if (sigSymsT != CV_SIGNATURE_C7
        && sigSymsT != CV_SIGNATURE_C11
        && sigSymsT != CV_SIGNATURE_C13) {
            ppdb1->setCorruptError();
            return FALSE;
    }

    PB pbSymCur = pbSym + sizeof(ULONG);
    PB pbSymEnd = pbSym + cb;

    if (cb < 0 || pbSymEnd < pbSymCur) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    while (pbSymCur < pbSymEnd) {
        PSYM psym = (PSYM) pbSymCur;
        pbSymCur += psym->reclen + sizeof(psym->reclen);
    }

    if (pbSymCur > pbSymEnd) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    fSymRecordsVerified = true;
    return TRUE;
}


CB Mod1::cbGlobalRefs()
{
    MTS_ASSERT(pdbi1->m_csForMods);
    MODI* pmodi = pdbi1->pmodiForImod(imod);
    dassert(pmodi);
    if (pmodi->sn == snNil) {
        return 0;
    }

    CB cbRet;
    CB cb = sizeof (cbRet);

    if (fReadPbCb((PB) &cbRet, &cb, pmodi->cbSyms + pmodi->cbLines + pmodi->cbC13Lines, sizeof(OFF)) &&
         cb == sizeof (OFF))
        return cbRet;

    // Use "return -1" to indicate something wrong occurred in fReadPbCb(),
    // to differentiate with the "return 0" case above.
    return -1;
}

BOOL Mod1::fQueryGlobalRefs(PB pb, CB cb)
{
    dassert(pb);
    dassert(cb);
    CB cbRead = cb;
    return
        fReadPbCb(pb, &cbRead, cbSyms() + cbLines() + cbC13Lines() + sizeof(CB), cb) &&
        cbRead == cb;
}


BOOL Mod1::fReadPbCb(PB pb, CB* pcb, OFF off, CB cb)
{
    // empty if no stream

    SN sn = pdbi1->Sn(imod);
    if (sn == snNil) {
        dassert(cb == 0);
        *pcb = cb;
        return TRUE;
    }

    if (pb) {
        CB cbT = cb = *pcb = min(*pcb, cb);
        if (!(ppdb1->pmsf->ReadStream(sn, off, pb, &cb) && cb == cbT)){
            ppdb1->setReadError();
            return FALSE;
            }
        return TRUE;
    }
    else {
        // if !pb, we were called to set *pcb to the stream size
        *pcb = cb;
        return TRUE;
    }
}

BOOL Mod1::fReadFromStream(SN sn, PB pb, DWORD *pcb)
{
    assert(sn != snNil);

    DWORD cbStream = (DWORD) ppdb1->pmsf->GetCbStream(sn);

    if (pb) {
        *pcb = min(*pcb, cbStream);

        CB cbT = *pcb;
        CB cb = *pcb;

        if (!(ppdb1->pmsf->ReadStream(sn, 0, pb, &cb) && cb == cbT)) {
            ppdb1->setReadError();
            return FALSE;
        }
    }
    else {
        *pcb = cbStream;
    }

    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////
// This will do the work of buffering the converted syms so that we only
// have to convert once.  NB: It has knowlege regarding the layout of the MOD
// stream which fReadPbCb does not have.
//
BOOL Mod1::fReadAndConvertSyms(PB pbDst, CB* pcb)
{
    MTS_ASSERT(pdbi1->m_csForMods);

    // empty if no stream
    if (pdbi1->Sn(imod) == snNil) {
        dassert(cbSyms() == 0);
        *pcb = 0;
        return TRUE;
    }

    assert(cvtsyms.fConverting());

    if (!cvtsyms.bufSyms().Start()) {
        // we have not already converted the symbols...do so now.
        // someone just asking for the count of bytes will pay the
        // price of an actual conversion happening.
        SymConvertInfo  sci = { 0, 0, 0, 0 };
        Buffer          buf;

        if (!buf.Reserve(cbSyms())) {
            ppdb1->setOOMError();
            return FALSE;
        }
        
        PB  pbSymBase = buf.Start();;
        CB  cbSymT = buf.Size();
        CB  cbDummy = cbSymT;

        if (!fReadPbCb(buf.Start(), &cbDummy, 0, cbSymT)) {
            return FALSE;
        }

        // Remember the signature!
        if (pwti->fQuerySymConvertInfo(sci, pbSymBase, cbSymT, sizeof ULONG)) {
            assert(sci.cbSyms >= ULONG(cbSymT) - sizeof ULONG);
            assert(sci.cSyms < ULONG(cbSymT) / 4); // something reasonable
            // grab all the memory we will need up front.
            if (!cvtsyms.rgOffMap().setSize(sci.cSyms) ||
                !cvtsyms.bufSyms().Reserve(sci.cbSyms)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            sci.pbSyms = cvtsyms.bufSyms().Start();
            sci.rgOffMap = &cvtsyms.rgOffMap()[0];
            if (!pwti->fConvertSymbolBlock(sci, pbSymBase, cbSymT, sizeof ULONG)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            // set the signature
            *(ULONG*)(cvtsyms.bufSyms().Start()) = CV_SIGNATURE_C11;

            // tell dbi to fix up the refsyms that reference this module
            pdbi1->fixupRefSymsForImod(ximodForImod(imod), cvtsyms);
        }
    }

    assert(cvtsyms.bufSyms().Start());

    if (pbDst) {
        CB cbT = *pcb = min(*pcb, cvtsyms.bufSyms().Size());
        memcpy(pbDst, cvtsyms.bufSyms().Start(), cbT);
        cvtsyms.bufSyms().Free();
    }
    else {
        *pcb = cvtsyms.bufSyms().Size();
    }
    return TRUE;
}


// used by Mod1::fReadAndConvertStSyms();
BOOL Mod1::fConvertSymRecsStToSz(PB pbSrc, CB cbSrc)
{
    MTS_ASSERT(pdbi1->m_csForMods);

    assert(pbSrc != NULL || cbSrc == 0);

    if (cbSrc == 0)
        return TRUE;

    // Get the size of converted symbols
    CB cbNew;
    if (!fConvertSymRecStToSzWithSig(pbSrc, cbSrc, NULL, &cbNew))
        return FALSE;

    if (!bufSZSyms.Reserve(cbNew)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    PB pbDst = bufSZSyms.Start();

    // Copy the signature
    assert((*(ULONG*)pbSrc) == CV_SIGNATURE_C11);
    *(ULONG *)pbDst = CV_SIGNATURE_C13;

    // Convert the rest, leave a map in rgOffMap
    cbNew -= sizeof ULONG;
    BOOL fRet = fConvertSymRecStToSz(
        pbSrc + sizeof ULONG, 
        cbSrc - sizeof ULONG, 
        pbDst + sizeof ULONG, 
        &cbNew,  
        cvtstsyms.rgOffMap());

    if (fRet)
        cvtstsyms.SetValid();

    return fRet;
}

// used by Mod1::QuerySymbol
BOOL Mod1::fReadAndConvertStSyms(PB pbDst, CB* pcb)
{
    MTS_ASSERT(pdbi1->m_csForMods);

    // empty if no stream
    MODI* pmodi = pdbi1->pmodiForImod(imod);
    dassert(pmodi);
    if (pmodi->sn == snNil) {
        dassert(cbSyms() == 0);
        *pcb = 0;
        return TRUE;
    }

    if (!bufSZSyms.Start()) {

        Buffer buf;

        CB  cbDummy = 0;

        if (!fForcedReadSymbolInfo(NULL, cbDummy)) {
            return FALSE;
        }

        if (!buf.Reserve(cbDummy)) {
            ppdb1->setOOMError();
            return FALSE;
        }
        
        PB  pbSymBase = buf.Start();
        CB  cbSymT = buf.Size();

        if (!fForcedReadSymbolInfo(buf.Start(), cbDummy)) {
            return FALSE;
        }

        // Convert all the symbols
        if (!fConvertSymRecsStToSz(buf.Start(), cbDummy))
            return FALSE;

        // Do not fixup refsyms for this conversion
        // The refsyms will be fixed up when they are
        // asked for through GSI::HashSym or GSI::NextSym

        // fix all the parents and blocks
        if (!fRemapParents(bufSZSyms))
            return FALSE;
    }

    // Commented for the need to accomodate mods
    // with no symbols, cbSyms = 0 pbSyms = NULL.
    // assert(bufSZSyms.Start());

    if (pbDst) {
        CB cbT = *pcb = min(*pcb, bufSZSyms.Size());
        memcpy(pbDst, bufSZSyms.Start(), cbT);
        // Don't keep redundent copies
        bufSZSyms.Free();
    }
    else {
        *pcb = bufSZSyms.Size();
    }

    return TRUE;
}

BOOL Mod1::fRemapParents(Buffer &bufSyms)
{
    DWORD offParent = 0;
    int iLevel = 0;

    for (PSYM psym = (PSYM)(bufSyms.Start() + sizeof(ULONG));
        (PB) psym < bufSyms.End();
        psym = (PSYM)pbEndSym(psym)) {

        expect(fAlign(psym));
        
        switch(psym->rectyp) {
            case S_THUNK16:
            case S_GPROC16:
            case S_GPROC32:
            case S_THUNK32:
            case S_GPROCMIPS:
            case S_GPROCIA64:
            case S_LPROC16:
            case S_LPROC32:
#if defined(CC_DP_CXX)
            case S_LPROC32_DPC:
#endif // CC_DP_CXX
            case S_LPROCMIPS:
            case S_LPROCIA64:

            case S_GMANPROC:
            case S_LMANPROC:

            case S_BLOCK16:
            case S_WITH16:
            case S_BLOCK32:
            case S_WITH32:
                
                // Code duplicated from EnterLevel
                ((BLOCKSYM *)psym)->pParent = offParent;
                offParent = ULONG(PB(psym) - bufSyms.Start());   // REVIEW:WIN64 CAST
                iLevel++;
                break;

            case S_END:

                // Same as ExitLevel
                // fill in the end record to the parent
                ((BLOCKSYM *)(bufSyms.Start() + offParent))->pEnd = (ULONG)((PB)psym - bufSyms.Start());

                // reclaim his parent as the parent
                offParent = ((BLOCKSYM *)(bufSyms.Start() + offParent))->pParent;
                iLevel--;
                break;

            default:
                break;
        }
    }

    if (iLevel) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    return TRUE;    //iLevel better be zero or we had bad scoping
}

// used by DBI1::fReadSymRec()
OFF Mod1::offSymNewForOffSymOld(OFF offOld, bool fVC40Offset)
{
    MTS_PROTECT(pdbi1->m_csForMods);

    if (!cvtstsyms.fValid()) 
    {
        CB cbSyms = 0;
        verify(QuerySymbols(NULL, &cbSyms));

        if (cvtsyms.fConverting() && fVC40Offset) {
            // Converting from 4.0 PDBs as well
            // so first change offOld to that 6.0
            offOld = cvtsyms.offSymNewForOffSymOld(offOld);
        }
    }

    return cvtstsyms.offSymNewForOffSymOld(offOld);
}

BOOL Mod1::Close()
{
    MTS_PROTECT(pdbi1->m_csForMods);
    // no need to MT protect other stuff because write to DBI is exclusive

    BOOL fOK = true;

    if (pdbi1->fWrite) {
        if (ppdb1->m_fMinimalDbgInfo) {
            if (strcmp(szModule(), "* Linker *") == 0) {
                // Write out input PDB mapping entries.

                PDBMAP pm = {0, S_PDBMAP};

                for (DWORD i = 0; i < ppdb1->m_rgPDBMapping.size(); i++) {
                    wchar_t *szFrom = ppdb1->m_rgPDBMapping[i].szFrom;
                    size_t cbFrom = sizeof(wchar_t) * (wcslen(szFrom) + 1);

                    wchar_t *szTo = ppdb1->m_rgPDBMapping[i].szTo;
                    size_t cbTo = sizeof(wchar_t) * (wcslen(szTo) + 1);

                    pm.reclen = (unsigned short) (sizeof(PDBMAP) - sizeof(unsigned short) + cbFrom + cbTo);

                    unsigned short cbPad = (unsigned short) dcbAlign(static_cast<size_t>(pm.reclen + sizeof(unsigned short)));

                    pm.reclen += cbPad;

                    if (!bufSyms.Append((PB) (&pm), (int) sizeof(PDBMAP)) ||
                        !bufSyms.Append((PB) szFrom, (int) cbFrom) ||
                        !bufSyms.Append((PB) szTo, (int) cbTo) ||
                        !bufSyms.AppendFmt("f", (int) cbPad)) {
                        return FALSE;
                    }
                }
            }
        }

        fOK = (fUpdateLines() &&
               fUpdateSyms() &&
               fUpdateSecContrib() &&
               fCommit());
    }

    pdbi1->NoteModCloseForImod(imod);

    delete this;
    return fOK;
}

BOOL Mod1::fCommit()
{
    dassert(pdbi1->fWrite);
    MTS_ASSERT(pdbi1->m_csForMods);

    MODI* pmodi = pdbi1->pmodiForImod(imod);

    pmodi->cbSyms = ppdb1->m_fNoTypeMerge ? bufSyms.Size() : bufSymsOut.Size();
    pmodi->cbLines = bufLines.Size();
    assert(rbufC13Lines != NULL);
    pmodi->cbC13Lines = rbufC13Lines->Size();
#ifdef PDB_TYPESERVER
    pmodi->iTSM = itsm;
#else
    pmodi->iTSM = 0;
#endif
    pmodi->fECEnabled = fECSymSeen;
    CB cbGlobalRefs = bufGlobalRefs.Size();
    expect(fAlign(pmodi->cbSyms));
    expect(fAlign(pmodi->cbLines));
    expect(fAlign(pmodi->cbC13Lines));

    if (pmodi->cbSyms + pmodi->cbLines + pmodi->cbC13Lines + cbGlobalRefs == 0) {
        return fEnsureNoSn(&pmodi->sn);
    }

    if (ppdb1->m_fNoTypeMerge && !fCopyTM()) {
        return FALSE;
    }

    if (!fEnsureSn(&pmodi->sn)) {
        return FALSE;
    }
    
    MSF *pmsf = ppdb1->pmsf;
    SN   sn = pmodi->sn;

    PB pbSyms = ppdb1->m_fNoTypeMerge ? bufSyms.Start() : bufSymsOut.Start();

    if (!pmsf->ReplaceStream(sn, pbSyms,  pmodi->cbSyms)  ||
        !pmsf->AppendStream (sn, bufLines.Start(), pmodi->cbLines) ||
        !pmsf->AppendStream (sn, rbufC13Lines->Start(), pmodi->cbC13Lines) ||
        !pmsf->AppendStream (sn, &cbGlobalRefs, sizeof(CB)) ||
        !pmsf->AppendStream (sn, bufGlobalRefs.Start(), cbGlobalRefs)) {
        ppdb1->setWriteError();
        return FALSE;
    }

    return TRUE;
}


BOOL Mod1::fCopyTpiStream(MSF *pmsfFrom, SN snFrom, SN *psnTo)
{
    SN sn = snNil;

    if (!fEnsureSn(&sn)) {
        return FALSE;
    }

    CB cb = pmsfFrom->GetCbStream(snFrom);
    PB pb = new BYTE[cb];

    if (pb == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!pmsfFrom->ReadStream(snFrom, pb, cb)) {
        ppdb1->setReadError();
        delete [] pb;
        return FALSE;
    }

    if (!ppdb1->pmsf->ReplaceStream(sn, pb + sizeof(HDR), cb - sizeof(HDR))) {
        ppdb1->setWriteError();
        delete [] pb;
        return FALSE;
    }

    delete [] pb;

    if (psnTo) {
        *psnTo = sn;
    }

    return TRUE;
}


BOOL Mod1::fCopyTMR(TMR *ptmr, SN *psnTo)
{
    PB pb;
    CB cb;

    ptmr->GetRawTypes(&pb, &cb);

    if (pb && cb) {
        SN sn = snNil;

        if (!fEnsureSn(&sn)) {
            return FALSE;
        }

        if (!ppdb1->pmsf->ReplaceStream(sn, pb, cb)) {
            ppdb1->setWriteError();
            return FALSE;
        }

        if (psnTo) {
            *psnTo = sn;
        }
    }

    return TRUE;
}


BOOL Mod1::fCopyTM()
{
    dassert(pdbi1->fWrite);
    MTS_ASSERT(pdbi1->m_csForMods);

    if (!ptm) {
        return TRUE;
    }

    if (ptm->Imod() != imod) {
        return TRUE;
    }

    MODTYPEREF *pmtr = (MODTYPEREF *) (bufSyms.Start() + sizeof(DWORD));

    assert(pmtr->rectyp == S_MOD_TYPEREF);

    if (ptm->IsTMR()) {
        assert(ptm->Imod() == imod);

        TMR *ptmr = (TMR *) ptm;

        if (!fCopyTMR(ptmr, &snType)) {
            return FALSE;
        }

        pmtr->word0 = snType;

#if 0
        if (ptm->IsTmpctOwner()) {
            if (!fCopyTMR(ptmr->Ptmpct(), &snZ7Pch)) {
                return FALSE;
            }

            pmtr->word1 = snZ7Pch;
        }
#endif
    }
    else if (ptm->Imod() == imod) {
        if (ptm->IsTMEQTS()) {
            pmtr->word0 = snTpi;
            pmtr->word1 = snIpi;

            return TRUE;
        }

        TMTS *ptmts = (TMTS *) ptm;
        PDB1 *ppdbFrom = (PDB1 *) ptmts->PPdbFrom();

        if (!fCopyTpiStream(ppdbFrom->pmsf, snTpi, &snType)) {
            return FALSE;
        }

        pmtr->word0 = snType;

        if (ppdbFrom->fIsPdb110()) {
            if (!fCopyTpiStream(ppdbFrom->pmsf, snIpi, &snId)) {
                return FALSE;
            }

            pmtr->word1 = snId;
        }
        else {
            pmtr->word1 = snNil;
        }
    }

    return TRUE;
}


// MOD1::fUpdateSyms
// final process of a modules local syms. at this point we will make a pass thru the
// local syms kept in bufSyms.  we will
//      resolve any S_UDT that point to a forward refs to point to the defining type
//      record if possible
//      link up matching scope records for PROC/WITH/BEGIN with their matching end records
//      add and delete the appropriate entries to the Globals and Statics symbol tables.
//      copy the resultant locals to the appropriate MSF in the PDB

// used in Mod1::Close();
BOOL Mod1::fUpdateSyms()
{
    MTS_ASSERT(pdbi1->m_csForMods);
    assert(pdbi1->fWrite);

    if (ptm == NULL && fHasTypeRef) {
        // We failed to set up a TM but this mod does have local sym that references
        // type.  The linker must have already emitted a warning saying linking as if
        // there was no debug info.  So here we just ignore any private sym.
        return TRUE;
    }

    return (fProcessSyms() && fProcessGlobalRefs());
}

// used by Mod1::fProcessSyms();
BOOL Mod1::fCopySymOut(PSYM psym)
{
    assert(pdbi1->fWrite);
    if (!bufSymsOut.Append((PB) psym, cbForSym(psym), 0)) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

// used by Mod1::fProcessSyms();
BOOL Mod1::fCopySymOut(PSYM psym, PSYM *ppsymOut)
{
    assert(pdbi1->fWrite);
    if (!bufSymsOut.Append((PB) psym, cbForSym(psym), (PB *)ppsymOut)) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

// used by Mod1::fProcessSyms();
BOOL Mod1::fCopyGlobalRef(OFF off)
{
    assert(pdbi1->fWrite);
    if (!bufGlobalRefs.Append((PB) &off, sizeof (OFF))) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

// used by Mod1::fProcessSyms()
bool Mod1::fUdtIsESU(TI ti) const
{
    assert(pdbi1->fWrite);

    if (CV_IS_PRIMITIVE(ti)) 
        return false;

    PTYPE ptype;
    if (ptm)
        ptype = ptm->ptypeForTi(ti, false);
    else {
        TPI * ptpi = pdbi1->GetTpi();
        assert(ptpi);
        if (!ptpi->QueryPbCVRecordForTi(ti, (PB*)&ptype))
            return false; // scalar types are considered definitions
    }

    dassert(ptype);

    switch (ptype->leaf) {
    case LF_CLASS:
    case LF_STRUCTURE:
    case LF_UNION:
    case LF_ENUM:
        {
            // already eliminated forward refs in Mod1::fUdtIsDefn
            return true;
        }

    default:
        return false;
    }
}

// used by Mod1::fProcessSyms()
bool Mod1::fUdtIsDefn(TI tiUdt) const
{
    assert(pdbi1->fWrite);
    if (CV_IS_PRIMITIVE(tiUdt)) {
        return true;
    }

    PTYPE ptype;
    if (ptm) {
        ptype = ptm->ptypeForTi(tiUdt, false);
    }
    else {
        TPI * ptpi = pdbi1->GetTpi();
        assert(ptpi);
        if (!ptpi->QueryPbCVRecordForTi(tiUdt, (PB*)&ptype)) {
            return true; // scalar types are considered definitions
        }
    }

    dassert(ptype);

    switch (ptype->leaf) {
        case LF_CLASS:
        case LF_STRUCTURE:
            {
            lfClass* pClass = (lfClass*) &(ptype->leaf);
            return !(pClass->property.fwdref);
            }

        case LF_UNION:
            {
            lfUnion* pUnion = (lfUnion*) &(ptype->leaf);
            return !(pUnion->property.fwdref);
            }

        case LF_ENUM:
            {
            lfEnum* pEnum = (lfEnum*) &(ptype->leaf);
            return !(pEnum->property.fwdref);
            }

        default:
            return true;
    }
}

// used by Mod1::fProcessSyms()
bool Mod1::fUdtNameMatch(TI tiUdt, const UDTSYM * pudt) const
{

    assert(pdbi1->fWrite);
    if (CV_IS_PRIMITIVE(tiUdt)) {
        return false;
    }

    PTYPE ptype;
    if (ptm) {
        ptype = ptm->ptypeForTi(tiUdt, false);
    }
    else {
        TPI * ptpi = pdbi1->GetTpi();
        assert(ptpi);
        if (!ptpi->QueryPbCVRecordForTi(tiUdt, (PB*)&ptype)) {
            return TRUE; // scalar types are considered definitions
        }
    }

    assert(ptype);

    SZ  szUdt;
    SZ  szType;

    if (!fGetSymName(reinterpret_cast<PSYM>(const_cast<UDTSYM*>(pudt)), &szUdt) ||
        !(szType = szUDTName(reinterpret_cast<PB>(ptype))))
    {
        return false;
    }
    return strcmp(szUdt, szType) == 0;
}

// used by Mod1::fProcessSyms()
BOOL Mod1::fConvertIdToType(PSYM psym)
{
    assert(pdbi1->fWrite);

    if (ptm == NULL) {
        return TRUE;
    }

    for (SymTiIter tii(psym); tii.next();) {
        TI ti = tii.rti();
    
        assert(!CodeViewInfo::CrossScopeId::IsCrossScopeId(ti));

        if (CV_IS_PRIMITIVE(ti)) {
            continue;
        }

        PTYPE pType = ptm->ptypeForTi(ti, true);

        if (pType == NULL) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        else if (pType->leaf == LF_FUNC_ID) {
            lfFuncId *plfFuncId = (lfFuncId *)&pType->leaf;
            tii.rti() = plfFuncId->type;
        }
        else if (pType->leaf == LF_MFUNC_ID) {
            lfMFuncId *plfMFuncId = (lfMFuncId *)&pType->leaf;
            tii.rti() = plfMFuncId->type;
        }
        else {
            assert(0);
            return FALSE;
        }

        if (!ptm->fIdMapped(ti) && !packType(psym)) {
            return FALSE;
        }
    }

    return TRUE;
}

// used by Mod1::fProcessSyms()
BOOL Mod1::packType(PSYM psym)
{
    assert(pdbi1->fWrite);

    if (ptm) {
        instrumentation(pdbi1->info.cSymbols++);

        for (SymTiIter tii(psym); tii.next();) {
            if (CodeViewInfo::CrossScopeId::IsCrossScopeId(tii.rti())) {
                // Delay resolving for those proxy id until DIA resolve it.
                continue;
            }

            if (ppdb1->m_fPdbTypeMismatch && !tii.fId()) {
                pdbi1->m_tmdTiObj = tii.rti();
            }

            if (!ptm->fMapRti(imod, tii.rti(), tii.fId())) {
                return FALSE;
            }
        }
    }

#ifdef PDB_TYPESERVER
    else if (itsm > 0) {  // if symbols must be modified for a typeserver
        instrumentation(pdbi1->info.cSymbols++);
        for (SymTiIter tii(psym); tii.next();)
            tii.rti() = pdbi1->TiForTiItsm(tii.rti(), itsm);
    }
#endif

    return TRUE;
}

BOOL Mod1::QueryIpi(OUT TPI** ppipi)
{
    if (pdbi1) {
        *ppipi = pdbi1->GetIpi();
        return TRUE;
    }
    return FALSE;
}

BOOL Mod1::QueryTpi(OUT TPI** pptpi)
{
    if (pdbi1) {
#ifdef PDB_TYPESERVER
        MODI *pmodi = pdbi1->pmodiForImod(imod);
        if (pmodi) {
            return pdbi1->QueryTypeServer((UCHAR)pmodi->iTSM, pptpi);
        }
#else
        *pptpi = pdbi1->GetTpi();
        return TRUE;
#endif
    }
    return FALSE;
}

const char* szFindSrc(COMPILESYM* psym)
{
    CompEnviron env(psym);
    return env.szValueForTag(ENC_SRC);
}

const char* szFindPdb(COMPILESYM* psym)
{
    CompEnviron env(psym);
    return env.szValueForTag(ENC_PDB);
}

const char* szFindSrc(ENVBLOCKSYM* psym)
{
    EnvBlockEnviron env(psym);
    return env.szValueForTag(ENVBLOCK_SRC);
}

const char* szFindPdb(ENVBLOCKSYM* psym)
{
    EnvBlockEnviron env(psym);
    return env.szValueForTag(ENVBLOCK_PDB);
}

BOOL Mod1::fProcessSyms()
{
    assert(pdbi1->fWrite);
    MTS_ASSERT(pdbi1->m_csForMods);     

    if (!bufSyms.Start() || bufSyms.Start() == bufSyms.End() || ppdb1->m_fNoTypeMerge) {
        return processC13(true);   // no syms were added for this module, just process the lines
    }

    dassert(bufSyms.End());

    DWORD offParent = 0;
    int iLevel = 0;

    bool    fCTypes = ppdb1->m_fPdbCTypes;
    bool    fTypeMismatch = ppdb1->m_fPdbTypeMismatch;

    if (fTypeMismatch) {
        pdbi1->m_tmdImod = imod;
        pdbi1->m_tmdPtm  = ptm;
    }

    // copy the ever-lovin' signature
    if (*(ULONG*)bufSyms.Start() != CV_SIGNATURE_C13) {
        return FALSE;
    }
    if (!bufSymsOut.Append(bufSyms.Start(), sizeof(ULONG))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    PSYM psymNext;
    for (PSYM psym = (PSYM)(bufSyms.Start() + sizeof(ULONG));
        (PB) psym < bufSyms.End();
        psym = psymNext) {

        // VSW:582748 - We need to do some validation on the symbol records to avoid crashing.
        psymNext = (PSYM)pbEndSym(psym);
        if (psymNext > (PSYM)bufSyms.End()) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        OFF offSym;
        PSYM psymOut;

        bool  fConverted = false;

        expect(fAlign(psym));
        assert(!fNeedsSzConversion(psym));

        switch(psym->rectyp) {
            case S_GPROC16:
            case S_GPROC32:
            case S_GPROCMIPS:
            case S_GPROCIA64:
            case S_LPROC16:
            case S_LPROC32:
#if defined(CC_DP_CXX)
            case S_LPROC32_DPC:
#endif // CC_DP_CXX
            case S_LPROCMIPS:
            case S_LPROCIA64:

                if (!packType(psym) ||
                    !pdbi1->packRefToGS(psym, imod, OFF(bufSymsOut.Size()), &offSym) ||
                    // copy full sym to output syms
                    !fCopySymOut(psym, &psymOut) ||
                    // copy offset of procref to tables of globals ref'd
                    !fCopyGlobalRef(offSym))
                {
                    return FALSE;
                }

                EnterLevel(psymOut, offParent, iLevel);
                break;

            // We replace the S_*PROC*_ID symbols with the traditional
            // S_*PROC* symbols which reference type instead of ID.

            case S_GPROC32_ID:
                psym->rectyp = S_GPROC32;
                fConverted = true;

            case S_GPROCMIPS_ID:
                if (!fConverted) {
                    psym->rectyp = S_GPROCMIPS;
                    fConverted = true;
                }

            case S_GPROCIA64_ID:
                if (!fConverted) {
                    psym->rectyp = S_GPROCIA64;
                    fConverted = true;
                }

#if defined(CC_DP_CXX)
            case S_LPROC32_DPC_ID:
                if (!fConverted) {
                    psym->rectyp = S_LPROC32_DPC;
                    fConverted = true;
                }

#endif // CC_DP_CXX
            case S_LPROC32_ID:
                if (!fConverted) {
                    psym->rectyp = S_LPROC32;
                    fConverted = true;
                }

            case S_LPROCMIPS_ID:
                if (!fConverted) {
                    psym->rectyp = S_LPROCMIPS;
                    fConverted = true;
                }

            case S_LPROCIA64_ID:
                if (!fConverted) {
                    psym->rectyp = S_LPROCIA64;
                }

                if (!fConvertIdToType(psym) ||
                    !pdbi1->packRefToGS(psym, imod, OFF(bufSymsOut.Size()), &offSym) ||
                    // copy full sym to output syms
                    !fCopySymOut(psym, &psymOut) ||
                    // copy offset of procref to tables of globals ref'd
                    !fCopyGlobalRef(offSym))
                {
                    return FALSE;
                }

                EnterLevel(psymOut, offParent, iLevel);
                break;
                
            case S_BLOCK16:
            case S_BLOCK32:
            case S_WITH16:
            case S_WITH32:
            case S_THUNK16:
            case S_THUNK32:
            case S_SEPCODE:
                // No types in thunk/block/with records; skip the packType call
                if (!fCopySymOut(psym, &psymOut)) {
                    return FALSE;
                }
                EnterLevel(psymOut, offParent, iLevel);
                break;
                
            case S_GMANPROC:
            case S_LMANPROC:
                {
                    OFF offSymOut = OFF(bufSymsOut.Size());
                    if (!packType(psym) ||
                        !pdbi1->packRefToGS(psym, imod, offSymOut, &offSym) ||
                        // copy offset of procref to tables of globals ref'd
                        !fCopyGlobalRef(offSym) ||
                        // pack a tokenref
                        !pdbi1->packTokenRefToGS(psym, imod, offSymOut, &offSym) ||
                        // copy this new offset to tables of globals ref'd
                        !fCopyGlobalRef(offSym) ||
                        // copy full sym to output syms
                        !fCopySymOut(psym, &psymOut)
                       )
                    {
                        return FALSE;
                    }
                    
                    EnterLevel(psymOut, offParent, iLevel);
                    break;
                }

            case S_CONSTANT: {
                if (!packType(psym)) {
                    return FALSE;
                }

                if (iLevel) {
                    if (!fCopySymOut(psym)) {
                        return FALSE;
                    }
                    break;
                }
                // global scope...
                SPD spd;
                if (!pdbi1->packSymSPD(psym, &offSym, spd)) {
                    return FALSE;
                }
                
                // success, now decide what to do with the constsym
                switch (spd) {
                case spdMismatch:
                    // need to copy the symbol to the mod.
                    if (!fCopySymOut(psym)) {
                        return FALSE;
                    }
                    break;
                
                case spdAdded:
                case spdIdentical:
                    if (!fCopyGlobalRef(offSym)) {
                        return FALSE;
                    }
                    break;
                }
                break;
            }

            case S_UDT: {
                // S_UDTs are a special problem.  What we need to do is this,
                //  in order, with early outs:
                //
                // 1.  If the S_UDT refers to a forward ref of an ESU, we pack
                //      the type and skip copying the S_UDT.  It is of no use.
                // 2.  If the S_UDT is at a higher than 0 scope, it is a local
                //      type and we copy it and get out.
                // 3.  If fCTypes is turned on, everything below is kept in order
                //      to support conflicting types in different modules.
                // Otherwise, we continue:
                // 4.  If the S_UDT refers to a primitive, we infer that it is
                //      a typedef and we want to keep it.
                // 5.  If the S_UDT refers to an ESU, we check to see if the
                //      name is the same as the type it is referring to.
                //      If they are the same, we skip it.
                // 6.  If the S_UDT refers to a non ESU type and is not a
                //      primitive, we copy it.  Typically, this is
                //      an LF_POINTER.
                //
                UDTSYM *    pudtsym = reinterpret_cast<UDTSYM*>(psym);
                TI          tiUdtFrom = pudtsym->typind;

#if defined(_DEBUG)
                if (tiUdtFrom == T_NOTYPE) {
                    USES_STACKBUFFER(0x400);

                    // Build up a module name and type name into szBuff
                    SZ_CONST    szMod = GetSZMBCSFromSZUTF8(szModule());
                    SZ_CONST    szFile = GetSZMBCSFromSZUTF8(szObjFile());
                    SZ_CONST    szType = GetSZMBCSFromSZUTF8(UTFSZ(pudtsym->name));

                    if (!szMod) {
                        szMod = "<module name failed ANSI conversion>";
                    }

                    if (!szFile) {
                        szFile = "<file name failed ANSI conversion>";
                    }

                    if (!szType) {
                        szType = (*pudtsym->name) ?
                            "<type name failed ANSI conversion>" :
                            "<no type name present>" ;
                    }

                    size_t      cbBuff = strlen(szMod) + strlen(szFile) + strlen(szType) + 256;
                    SZ          szBuff = ___allocator.Alloc<char>(cbBuff);

                    if (szBuff) {
                        _snprintf_s(
                            szBuff,
                            cbBuff,
                            _TRUNCATE,
                            "S_UDTs should have a real type. Type = '%s', File = '%s(%s)'",
                            szType,
                            szFile,
                            szMod
                           );
                        expectFailed(szBuff);
                    }
                    else {
                        expect(("S_UDTs should have a real type!", tiUdtFrom != T_NOTYPE));
                    }
                }
#endif
                if (tiUdtFrom == T_NOTYPE) {
                    break;
                }

                // we need to pack the type regardless.
                if (!packType(psym)) {
                    return FALSE;
                }

                if (ptm && !ptm->fVerifyIndex(tiUdtFrom, false)) {
                    break;
                }

                // if we have a udt decl (forward ref only) simply throw it out
                // doesn't help us
                //
                if (!fUdtIsDefn(tiUdtFrom)) {
                    break;
                }

                if (iLevel > 0) {
                    // simply copy the udt to the mod symbols and get out
                    //
                    if (!fCopySymOut(psym)) {
                        return FALSE;
                    }
                    break;
                }

                // Decide if we are going to do type-mismatch detection.
                // If the user has determined to promote types to handle type conflicts,
                // we disregard C files.
                // In all cases, we do C++ files.
                //
                if (fTypeMismatch && ((fIsCFile && !fCTypes) || fIsCPPFile)) {
                    assert(fIsCFile || fIsCPPFile);
                }

                bool    fESU;

                if (fCTypes ||
                    // When fCTypes is on, we keep everything
                    CV_IS_PRIMITIVE(tiUdtFrom) ||
                    // The only way to keep this name in the debugging
                    // info is to put it out as an S_UDT.
                    //
                    !(fESU = fUdtIsESU(tiUdtFrom)) ||
                    // Not an ESU, probably an LF_POINTER or the like
                    //
                    (fESU && !fUdtNameMatch(tiUdtFrom, pudtsym)))
                    // The name is a typedef of an existing ESU; we
                    // need to keep that name in an S_UDT too
                    //
                {
                    // At this point, we have a udt that is at global scope
                    // and we may be promoting the name to global scope.
                    //
                    // Check the promote of the S_UDT to global scope so that the
                    // name is available globally.  If there is a global name that
                    // matches but the record does not, we add it to the mod
                    // syms instead.
                    //
                    SPD spd;
                    if (pdbi1->packSymSPD(psym, &offSym, spd)) {
                        switch (spd) {
                        case spdMismatch:
                            // need to copy the sym to module scope
                            //
                            if (!fCopySymOut(psym)) {
                                return FALSE;
                            }
                            break;

                        case spdAdded:
                        case spdIdentical:
                            // It's now in global scope, so do the ref thing.
                            //
                            if (!fCopyGlobalRef(offSym)) {
                                return FALSE;
                            }
                            break;
                        }
                    }
                    else {
                        return FALSE;
                    }
                }
                break;
            }

            case S_LDATA16:
            case S_LDATA32:
            case S_LTHREAD32:
            case S_LMANDATA:
            case S_LDATA_HLSL:
            case S_LDATA_HLSL32:
            case S_LDATA_HLSL32_EX:
                if (!packType(psym) || !fCopySymOut(psym)) {
                    return FALSE;
                }
                if (iLevel == 0) {
                    // We never want to promote an S_L* to global scope
                    // if there is no name.  It is useless and wasteful.
                    //
                    ST      st;
                    bool    fHasName = fGetSymName(psym, &st) && *st != 0;
                    if (fHasName &&
                        (!pdbi1->packSymToGS(psym, &offSym) ||
                        !fCopyGlobalRef(offSym)))
                    {
                        return FALSE;
                    }
                }
                break;

            case S_GDATA16:
            case S_GDATA32:
            case S_GTHREAD32:
            case S_GMANDATA:
            case S_GDATA_HLSL:
            case S_GDATA_HLSL32:
            case S_GDATA_HLSL32_EX:
                if (!packType(psym) ||
                    !pdbi1->packSymToGS(psym, &offSym) ||
                    !fCopyGlobalRef(offSym))
                {
                    return FALSE;
                }

                break;

            case S_INLINESITE2:
            case S_INLINESITE:
                if (!packType(psym) ||
                    !fCopySymOut(psym, &psymOut))
                {
                    return FALSE;
                }

                EnterLevel(psymOut, offParent, iLevel);
                break;

            case S_PROC_ID_END:
                psym->rectyp = S_END;

            case S_END:
            case S_INLINESITE_END:
                if (!fCopySymOut(psym, &psymOut)) {
                    return FALSE;
                }

                ExitLevel(psymOut, offParent, iLevel);
                break;

            case S_OBJNAME:
                if (ptm && ptm->IsTmpctOwner() &&
                    !pdbi1->fUpdateTmpct(
                        szModule(),
                        szCopy((char*)((OBJNAMESYM *)psym)->name),
                        ptm))
                {
                    return FALSE;
                }
                if (!fCopySymOut(psym))
                    return FALSE;
                break;

            case S_COMPILE:
                fIsCFile = reinterpret_cast<CFLAGSYM*>(psym)->flags.language == CV_CFL_C;
                fIsCPPFile = reinterpret_cast<CFLAGSYM*>(psym)->flags.language == CV_CFL_CXX;
                if (!fCopySymOut(psym)) {
                    return FALSE;
                }
                break;

            case S_COMPILE2:    // signals an EnC ready object file
                fECSymSeen = reinterpret_cast<COMPILESYM*>(psym)->flags.fEC != 0;
                fIsCFile = reinterpret_cast<COMPILESYM*>(psym)->flags.iLanguage == CV_CFL_C;
                fIsCPPFile = reinterpret_cast<COMPILESYM*>(psym)->flags.iLanguage == CV_CFL_CXX;
                if (const char* szSrc = szFindSrc(reinterpret_cast<COMPILESYM*>(psym))) {
                    if (!pdbi1->setSrcForImod(imod, szSrc)) {
                        return FALSE;
                    }
                }
                if (const char* szPdb = szFindPdb(reinterpret_cast<COMPILESYM*>(psym))) {
                    if (!pdbi1->setPdbForImod(imod, szPdb)) {
                        return FALSE;
                    }
                }
                if (!fCopySymOut(psym))
                    return FALSE;
                break;

            case S_COMPILE3:
                fECSymSeen = reinterpret_cast<COMPILESYM3*>(psym)->flags.fEC != 0;
                fIsCFile = reinterpret_cast<COMPILESYM3*>(psym)->flags.iLanguage == CV_CFL_C;
                fIsCPPFile = reinterpret_cast<COMPILESYM3*>(psym)->flags.iLanguage == CV_CFL_CXX;
                if (!fCopySymOut(psym))
                    return FALSE;
                break;

            case S_ENVBLOCK:
                fECSymSeen = reinterpret_cast<ENVBLOCKSYM*>(psym)->flags.fEC != 0;
                if (const char* szSrc = szFindSrc(reinterpret_cast<ENVBLOCKSYM*>(psym))) {
                    if (!pdbi1->setSrcForImod(imod, szSrc)) {
                        return FALSE;
                    }
                }
                if (const char* szPdb = szFindPdb(reinterpret_cast<ENVBLOCKSYM*>(psym))) {
                    if (!pdbi1->setPdbForImod(imod, szPdb)) {
                        return FALSE;
                    }
                }
                if (!fCopySymOut(psym))
                    return FALSE;
                break;

            case S_ANNOTATION:
                if (!pdbi1->packRefToGS(psym, imod, OFF(bufSymsOut.Size()), &offSym) ||
                    // copy full sym to output syms
                    !fCopySymOut(psym, &psymOut) ||
                    // copy offset of annotationref to tables of globals ref'd
                    !fCopyGlobalRef(offSym))
                {
                    return FALSE;
                }
                break;

            default:
                if (!packType(psym) ||
                    !fCopySymOut(psym))
                    return FALSE;
                break;
        }
    }

    // check to see here that we have run out of type indices during the pack of
    // this module
    if (ptm) {
        if (!ptm->fNotOutOfTIs()) {
            ppdb1->setLastError(EC_OUT_OF_TI);
            return FALSE;
        }

        if (ptm->fCorruptedFromTypePool()) {
            ppdb1->setCorruptTypePoolError(ptm->PPdbFrom());
            return FALSE;
        }
    }

    // iLevel better be zero or we had bad scoping

    if (iLevel) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    return processC13(true);
}


class remapper : public Map<DWORD, NI, HcNi>, public MapStrs
{
public:
    bool mapStrId(DWORD oldId, DWORD* newId)
    {
        return map(oldId, newId) != 0;
    }
};

bool Mod1::processC13Strings(bool fWrite, IDebugSSectionReader* pStringReader, remapper& mapStrings, IDebugSStringTable** ppTable)
{
    COMRefPtr<IDebugSSubSectionEnum> pEnum;
    if (!pStringReader->GetSectionEnum(&pEnum)) {
        setDSRError(ppdb1, pStringReader->GetLastError());
        return false;
    }

    size_t  cSrcMapEntries = pdbi1->cSrcMapEntries();

    // if we have any mappings, we have a bit more work to do

    while (pEnum->Next()) {
        COMRefPtr<IDebugSSubSection> pSubSection;
        pEnum->Get(&pSubSection);
        if (pSubSection == NULL) {
            setDSRError(ppdb1, pStringReader->GetLastError());
            return false;
        }
        switch (pSubSection->Type()) {
        case DEBUG_S_STRINGTABLE: {
            Buffer      bufName;
            Buffer      bufMappedName;
            NameMap *   pNameMap = NULL;

            if (!pdbi1->GetNameMap(true, &pNameMap)) {
                ppdb1->setOOMError();
                return false;
            }
            if (FAILED(pSubSection->QueryInterface(IID_IDebugSStringTable, (void**)ppTable))) {
                ppdb1->setOOMError();
                return false;
            }
            COMRefPtr< IDebugSStringTable > pStringTable = *ppTable;
            COMRefPtr< IDebugSStringEnum > pStrEnum;
            if (!pStringTable->GetStringEnum(&pStrEnum)) {
                ppdb1->setOOMError();
                return false;
            }
            while (pStrEnum->Next()) {
                DWORD       off;
                NI          ni;
                DWORD       cch = 0;
                wchar_t *   pwch;

                pStrEnum->Get(NULL, &cch, &off);
                if (!bufName.ResetAndReserve(cch * sizeof(wchar_t), reinterpret_cast<PB*>(&pwch))) {
                    ppdb1->setOOMError();
                    return false;
                }
                pStrEnum->Get(pwch, &cch, &off);
                if (cSrcMapEntries > 0 &&
                    pdbi1->fSrcMap(pwch, bufMappedName))
                {
                    // Use the new name mapped from the old one
                    // NB: On failure from fSrcMap, we simply skip this

                    pwch = reinterpret_cast<wchar_t*>(bufMappedName.Start());
                }

                if (!pNameMap->getNiW(pwch, &ni)) {
                    return false;
                }
                if (!mapStrings.add(off, ni)) {
                    ppdb1->setOOMError();
                    return false;
                }
            }
        }
        default:
            break;
        }
    }
    return true;
}

BOOL Mod1::fUpdateTiOrIdReferencedByNonSyms(remapper& mapStrings)
{
    assert(pdbi1->fWrite);
    MTS_ASSERT(pdbi1->m_csForMods);

    if (ppdb1->m_fNoTypeMerge || ppdb1->m_fMinimalDbgInfo) {
        return TRUE;
    }
    
    if (rbufC13Lines->Size() == 0) {
        return TRUE;
    }

    COMRefPtr<IDebugSSectionReader> pReader;
    if (!InitC13Reader(*rbufC13Lines, &pReader)) {
        return FALSE;
    }

    COMRefPtr<IDebugSSubSectionEnum> pEnum;
    if (!pReader->GetSectionEnum(&pEnum)) {
        setDSRError(ppdb1, pReader->GetLastError());
        return FALSE;
    }

    while (pEnum->Next()) {
        COMRefPtr<IDebugSSubSection> pSubSection;
        pEnum->Get(&pSubSection);
        if (pSubSection == NULL) {
            setDSRError(ppdb1, pReader->GetLastError());
            return FALSE;
        }

        switch (pSubSection->Type()) {
            case DEBUG_S_CROSSSCOPEEXPORTS:
            {
                if (ptm == NULL) {
                    break;
                }

                PB pb = NULL;
                DWORD cb = static_cast<DWORD>(pSubSection->GetData(&pb));

                while (cb >= sizeof(CodeViewInfo::LocalIdAndGlobalIdPair)) {
                    CodeViewInfo::LocalIdAndGlobalIdPair *p =
                        (CodeViewInfo::LocalIdAndGlobalIdPair *) pb;
        
                    CodeViewInfo::DecoratedItemId decoratedItemId =
                        CodeViewInfo::DecoratedItemId(p->localId);
            
                    p->globalId = decoratedItemId.GetItemId();
                    
                    TI& rti = p->globalId;
                    
                    if (!ptm->fMapRti(imod, rti, decoratedItemId.IsFuncId())) {
                        return FALSE;
                    }
            
                    pb += sizeof(CodeViewInfo::LocalIdAndGlobalIdPair);
                    cb -= static_cast<DWORD>(sizeof(CodeViewInfo::LocalIdAndGlobalIdPair));
                }

                assert(cb == 0);
                break;
            }

            case DEBUG_S_CROSSSCOPEIMPORTS:
            {
                PB pb = NULL;
                DWORD cb = static_cast<DWORD>(pSubSection->GetData(&pb));

                while (cb >= sizeof(CodeViewInfo::CrossScopeReferences)) {
                    CodeViewInfo::CrossScopeReferences *p =
                        (CodeViewInfo::CrossScopeReferences *) pb;
            
                    CodeViewInfo::PdbIdScope pdbIdScope = p->externalScope;
        
                    DWORD stringidNew;
            
                    if (mapStrings.mapStrId(pdbIdScope.offObjectFilePath, &stringidNew)) {
                        p->externalScope.offObjectFilePath = stringidNew;
                    }
                    else {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }
            
                    DWORD recordSize = static_cast<DWORD>(
                        sizeof(CodeViewInfo::CrossScopeReferences) + 
                        sizeof(CV_ItemId) * p->countOfCrossReferences);
            
                    cb -= recordSize;
                    pb += recordSize;
                }

                assert(cb == 0);
                break;
            }

            case DEBUG_S_INLINEELINES:
            {
                if (ptm == NULL) {
                    break;
                }

                PB pb = NULL;
                DWORD cb = static_cast<DWORD>(pSubSection->GetData(&pb));

                bool fExtended = (*(DWORD *)pb == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX);

                pb += sizeof(DWORD);
                cb -= static_cast<DWORD>(sizeof(DWORD));

                while (cb >= (fExtended ? sizeof(CodeViewInfo::InlineeSourceLineEx)
                                        : sizeof(CodeViewInfo::InlineeSourceLine))) {
                    CodeViewInfo::InlineeSourceLine *p =
                        (CodeViewInfo::InlineeSourceLine *) pb;

                    if (!CodeViewInfo::CrossScopeId::IsCrossScopeId(p->inlinee)) {
                        CV_ItemId& id = p->inlinee;

                        if (!ptm->fMapRti(imod, id, true)) {
                            return FALSE;
                        }
                    }

                    size_t cbRec = sizeof(CodeViewInfo::InlineeSourceLine);

                    if (fExtended) {
                        CodeViewInfo::InlineeSourceLineEx *pEx =
                            (CodeViewInfo::InlineeSourceLineEx *) pb;

                        cbRec = sizeof(CodeViewInfo::InlineeSourceLineEx) +
                                pEx->countOfExtraFiles * sizeof(CV_off32_t);
                    }

                    cb -= static_cast<DWORD>(cbRec);
                    pb += cbRec;
                }

                assert(cb == 0);
                break;
            }

            case DEBUG_S_TYPE_MDTOKEN_MAP:
            {
                if (ptm == NULL) {
                    break;
                }

                PB pb = NULL;
                DWORD cb = static_cast<DWORD>(pSubSection->GetData(&pb));

                PB pbMax = pb + cb;
                DWORD cEntries = *(DWORD *) pb;

                pb += sizeof(DWORD);

                for (DWORD i = 0; i < cEntries; i++) {
                    TI& rti = *(DWORD *) pb;

                    if (!ptm->fMapRti(imod, rti, false)) {
                        return FALSE;
                    }

                    pb += sizeof(DWORD) * 2;
                }

                assert(pb <= pbMax);
                break;
            }

            default:
                break;
        }
    }
            
    return TRUE;
}


bool Mod1::processC13(bool fWrite)
{
    debug(if (!fWrite) { MTS_ASSERT(m_csForC13); });

    //
    // Now process the new C13 linenumber stuff
    //

    // Process any string tables

    remapper mapStrings;
    COMRefPtr< IDebugSStringTable > pStringTable;

    if (bufC13Strings.Size() > 0) {
        COMRefPtr<IDebugSSectionReader> pStringReader;

        if (!CheckFCreateReader(bufC13Strings.Start(), bufC13Strings.Size(), &pStringReader, CV_SIGNATURE_C13)) {
            return false;
        }

        if (!processC13Strings(fWrite, pStringReader, mapStrings, &pStringTable)) {
            return false;
        }
    }

    if (fWrite && !fUpdateTiOrIdReferencedByNonSyms(mapStrings)) {
        return false;
    }

    if (fSymsNeedStringTableFixup) {
        assert(fWrite);
        assert(!ppdb1->m_fMinimalDbgInfo);

        Buffer *pbuf = ppdb1->m_fNoTypeMerge ? &bufSyms : &bufSymsOut;

        assert(pbuf->Size());

        for (PSYM psym = reinterpret_cast<PSYM>(pbuf->Start() + sizeof(ULONG)); 
            psym < reinterpret_cast<PSYM>(pbuf->End()); 
            psym = reinterpret_cast<PSYM>(pbEndSym(psym))) {

            DWORD stringidNew;
            if (psym->rectyp == S_DEFRANGE || psym->rectyp == S_DEFRANGE_SUBFIELD) {
                DEFRANGESYM *pDefRangeSym = reinterpret_cast<DEFRANGESYM *>(psym);

                if (mapStrings.mapStrId(pDefRangeSym->program, &stringidNew)) {
                    pDefRangeSym->program = stringidNew;
                }
                else {
                    ppdb1->setCorruptError();
                    return false;
                }
            }
            else if (psym->rectyp == S_FILESTATIC) {
                FILESTATICSYM *pFileStatSym = reinterpret_cast<FILESTATICSYM *>(psym);
                
                if (mapStrings.mapStrId(pFileStatSym->modOffset, &stringidNew)) {
                    pFileStatSym->modOffset = stringidNew;
                }
                else {
                    ppdb1->setCorruptError();
                    return false;
                }
            }
        }
    }

    if (rbufC13Lines != NULL && rbufC13Lines->Size()) {
        // Now process the new C13 linenumber stuff

        COMRefPtr<IDebugSSectionReader> pReader;
        if (!InitC13Reader(*rbufC13Lines, &pReader))
            return false;

        // could be stringtables in the lines stuff too, so process these first

        COMRefPtr<IDebugSStringTable> pStringTable2;
        remapper mapStrings2;
        if (!processC13Strings(fWrite, pReader, mapStrings2, &pStringTable2)) {
            return FALSE;
        }

        COMRefPtr<IDebugSSubSectionEnum> pEnum;
        if (!pReader->GetSectionEnum(&pEnum)) {
            setDSRError(ppdb1, pReader->GetLastError());
            return false;
        }

        // now remap any string references held in any of these sections

        while (pEnum->Next()) {
            COMRefPtr<IDebugSSubSection> pSubSection;
            pEnum->Get(&pSubSection);
            if (pSubSection == NULL) {
                setDSRError(ppdb1, pReader->GetLastError());
                return false;
            }
            if (fWrite && pSubSection->Type() == DEBUG_S_FILECHKSMS) {
                // Add filenames to FileInfo blob in DBI
                COMRefPtr<IDebugSFileInfo> pFileInfo;
                if (SUCCEEDED(pSubSection->QueryInterface(IID_IDebugSFileInfo, reinterpret_cast<void**>(&pFileInfo)))) {
                    COMRefPtr<IDebugSFileEnum> pFileEnum;
                    if (!pFileInfo->GetFileEnum(&pFileEnum)) {
                        setDSRError(ppdb1, pReader->GetLastError());
                        return false;
                    }

                    unsigned cFiles = 0;
                    for(; pFileEnum->Next(); cFiles++)
                        /* Do Nothing */;

                    if (!initFileInfo(cFiles)) {
                        return false;
                    }

                    pFileEnum->Reset();
                    NameMap *   pNameMap = NULL;
                    if (!pdbi1->GetNameMap(true, &pNameMap)) {
                        ppdb1->setOOMError();
                        return false;
                    }
                    for(unsigned iFile = 0; pFileEnum->Next(); iFile++) {

                        DWORD idFile, stoffFileName;
                        const char *utfszFileName = NULL;
                        NI ni;

                        pFileEnum->Get(&idFile, &stoffFileName, NULL, NULL, NULL);
                        
                        if (pStringTable2 && 
                            ((mapStrings2.mapStrId(stoffFileName, &ni) && pNameMap->getName(ni, &utfszFileName)) ||
                             pStringTable2->GetStringByOff(stoffFileName, (char **)&utfszFileName))) 
                        {
                            if (!addFileInfo(iFile, utfszFileName)) {
                                return false;
                            }
                        }
                        else if (pStringTable && 
                                 ((mapStrings.mapStrId(stoffFileName, &ni) && pNameMap->getName(ni, &utfszFileName)) ||
                                  pStringTable->GetStringByOff(stoffFileName, (char **)&utfszFileName))) 
                        {
                            if (!addFileInfo(iFile, utfszFileName)) {
                                return false;
                            }
                        }
                        else {
                            ppdb1->setCorruptError();
                            return false;
                        }
                    }
                }
            }
            remapper& mapFileNameStrings = (pStringTable2 != NULL) ? mapStrings2 : mapStrings;
            if (!pSubSection->remap(&mapFileNameStrings)) {
                ppdb1->setCorruptError();
                return false;
            }
        }
    }

    //
    // process the new FPO information
    //
    if (bufC13Fpo.Size() > 0) {
        COMRefPtr<IDebugSSectionReader> pReader;
        if (!CheckFCreateReader(bufC13Fpo.Start(), bufC13Fpo.Size(), &pReader, CV_SIGNATURE_C13)) {
            return false;
        }
        COMRefPtr<IDebugSSubSectionEnum> pEnum;
        if (!pReader->GetSectionEnum(&pEnum)) {
            setDSRError(ppdb1, pReader->GetLastError());
            return false;
        }
        while (pEnum->Next()) {
            COMRefPtr<IDebugSSubSection> pSubSection;
            pEnum->Get(&pSubSection);
            PB pbSec = NULL;
            size_t  cbSec = 0;
            switch (pSubSection->Type()) {
            case DEBUG_S_FRAMEDATA:
                {
                    if (!pSubSection->remap(&mapStrings)) {
                        ppdb1->setCorruptError();
                        return false;
                    }
                    COMRefPtr<IDebugSFrameData> pFrame;
                    if (FAILED(pSubSection->QueryInterface(IID_IDebugSFrameData, (void**)&pFrame))) {
                        ppdb1->setOOMError();
                        return false;
                    }
                    pFrame->ApplyReloc();
                    cbSec = pSubSection->GetData(&pbSec);
                    if (cbSec > sizeof(DWORD)) {
                        if (!pdbi1->AddFrameData(
                                reinterpret_cast<FRAMEDATA*>(pbSec+sizeof(DWORD)),
                                static_cast<ULONG>((cbSec-sizeof(DWORD))/sizeof(FRAMEDATA)))
                           ) {
                            ppdb1->setOOMError();
                            return false;
                        }
                    }
                }
                break;
            default:
                assert(false);
                ppdb1->setCorruptError();
                return false;
            }
        }
    }

    return true;    
}


BOOL Mod1::fProcessGlobalRefs()
{
    assert(pdbi1->fWrite);

    BOOL    fRet = TRUE;
    CB      cb = cbGlobalRefs();

    if (ppdb1->m_fMinimalDbgInfo || ppdb1->m_fNoTypeMerge) {
        assert(cb == 0);
        return TRUE;
    }

    if (cb < 0) {
        // Something wrong occurred.  Should return FALSE.
        fRet = FALSE;
    }
    else if (cb > 0) {
        PB  pb = new (ppdb1) BYTE[cb];
        PB  pbBase = pb;
        if (!pb || !fQueryGlobalRefs(pb, cb)) {
            ppdb1->setCorruptError();
            fRet = FALSE;
        }
        else {
            for (PB pbEnd = pb + cb; pb < pbEnd; pb += sizeof(OFF)) {
                if (!pdbi1->decRefCntGS(*(OFF *)pb))  {
                    ppdb1->setCorruptError();
                    fRet = FALSE;
                    break;
                }
            }
        }
        if (pbBase) {
            delete [] pbBase;
        }
    }
    return fRet;
}

// EnterLevel/ExitLevel - fill in the scope link fields and bump the level indicator

void Mod1::EnterLevel(PSYM psym, DWORD & offParent, int & iLevel)
{
    assert(pdbi1->fWrite);
    // note that this works because all of these symbols
    // have a common format for the first fields.  The
    // address variants follow the link fields.

    // put in the parent
    reinterpret_cast<BLOCKSYM *>(psym)->pParent = offParent;
    offParent = ULONG(PB(psym) - bufSymsOut.Start());   // REVIEW:WIN64 CAST
    iLevel++;
}

void Mod1::ExitLevel(PSYM psym, DWORD & offParent, int & iLevel)
{
    assert(pdbi1->fWrite);
    // fill in the end record to the parent
    reinterpret_cast<BLOCKSYM *>(bufSymsOut.Start() + offParent)->pEnd =
        ULONG(PB(psym) - bufSymsOut.Start());

    // reclaim his parent as the parent
    offParent = reinterpret_cast<BLOCKSYM *>(bufSymsOut.Start() + offParent)->pParent;
    iLevel--;
}

/////////////////////////////////////////////////////////////////////////////
// C13 Lines support

class EnumC13Lines: public EnumLines
{
public:
    EnumC13Lines()
    {
        m_fILLines = false;
    }

    void SetILLines(bool f) {
        m_fILLines = f;
    }

    bool Init(const EnumC13Lines& clone) {
        assert(clone.m_pEnumSections);
        if (!clone.m_pEnumSections->clone(&m_pEnumSections)) {
            return false;
        }
        if (clone.m_pBlockEnum && !clone.m_pBlockEnum->clone(&m_pBlockEnum)) {
            return false;
        }
        m_rbuf = clone.m_rbuf;
        if (!m_mpIFileToIDFile.setSize(clone.m_mpIFileToIDFile.size())) {
            return false;
        }
        for (size_t i = 0; i < clone.m_mpIFileToIDFile.size(); i++) {
            m_mpIFileToIDFile[i] = clone.m_mpIFileToIDFile[i];
        }
        return true;
    }
    bool Init(RefBuf& rbuf, Array<DWORD>& mpIFileToIDFile, IDebugSSectionReader *pC13Reader) {
        if (!pC13Reader->GetSectionEnum(&m_pEnumSections)) {
            return false;
        }
        m_rbuf = rbuf;
        if (!m_mpIFileToIDFile.setSize(mpIFileToIDFile.size())) {
            return false;
        }
        for (size_t i = 0; i < mpIFileToIDFile.size(); i++) {
            m_mpIFileToIDFile[i] = mpIFileToIDFile[i];
        }
        return true;
    }

    bool getLines(   
            OUT DWORD*      fileId,     // id for the filename
            OUT DWORD*      poffset,    // offset part of address
            OUT WORD*       pseg,       // segment part of address
            OUT DWORD*      pcb,        // count of bytes of code described by this block
            IN OUT DWORD*   pcLines,    // number of lines (in/out)
            OUT CV_Line_t*  pLines      // pointer to buffer for line info
           );
    bool getLinesColumns(    
            OUT DWORD*      fileId,     // id for the filename      
            OUT DWORD*      poffset,    // offset part of address
            OUT WORD*       pseg,       // segment part of address
            OUT DWORD*      pcb,        // count of bytes of code described by this block
            IN OUT DWORD*   pcLines,    // number of lines (in/out)
            OUT CV_Line_t*  pLines,     // pointer to buffer for line info
            OUT CV_Column_t*pColumns    // pointer to buffer for column info
           );
    virtual void release() {
        m_rbuf->Free();                 // Get rid of the data to save space!
        delete this;
    }
    virtual void reset() {
        m_pEnumSections->Reset();
        m_pBlockEnum = NULL;
        m_pSection = NULL;
    }
    virtual BOOL next() {
        if (m_pBlockEnum != NULL && m_pBlockEnum->Next())
            return true;
        while (m_pEnumSections != 0 && m_pEnumSections->Next()) {
            COMRefPtr<IDebugSSubSection> pSection;
            m_pEnumSections->Get(&pSection);
            if (pSection == NULL) {
                return false;
            }
            if (m_fILLines) {
                if (pSection->Type() != DEBUG_S_IL_LINES) {
                    continue;
                }
            } else {
                if (pSection->Type() != DEBUG_S_LINES) {
                    continue;
                }
            }
            pSection->QueryInterface(IID_IDebugSLines, (void**)&m_pSection);
            if (!m_pSection->GetBlockEnum(&m_pBlockEnum))
                continue;
            if (m_pBlockEnum->Next())
                return true;
        }
        return false;
    }
    virtual bool clone(EnumLines** ppEnum) {
        EnumC13Lines *pLines = new EnumC13Lines;
        return pLines && pLines->Init(*this) && (*ppEnum = pLines);
    }
private:
    COMRefPtr<IDebugSSubSectionEnum> m_pEnumSections;
    COMRefPtr<IDebugSLines> m_pSection;
    COMRefPtr<IDebugSLineBlockEnum> m_pBlockEnum;
    RefBuf m_rbuf;
    Array<DWORD> m_mpIFileToIDFile;
    bool m_fILLines;
};

bool EnumC13Lines::getLines(   
        OUT DWORD*      pfileId,    // id for the filename
        OUT DWORD*      poffset,    // offset part of address
        OUT WORD*       pseg,       // segment part of address
        OUT DWORD*      pcb,        // count of bytes of code described by this block
        IN OUT DWORD*   pcLines,    // number of lines (in/out)
        OUT CV_Line_t*  pLines      // pointer to buffer for line info
       )
{
    return getLinesColumns(pfileId, poffset, pseg, pcb, pcLines, pLines, NULL);
}

bool EnumC13Lines::getLinesColumns(    
        OUT DWORD*      pfileId,     // id for the filename     
        OUT DWORD*      poffset,    // offset part of address
        OUT WORD*       pseg,       // segment part of address
        OUT DWORD*      pcb,        // count of bytes of code described by this block
        IN OUT DWORD*   pcLines,    // number of lines (in/out)
        OUT CV_Line_t*  pLines,     // pointer to buffer for line info
        OUT CV_Column_t*pColumns    // pointer to buffer for column info
       )
{
    if (pfileId) {
        m_pBlockEnum->GetFileId(pfileId);
        bool fIFileFound = false;
        for(size_t i = 0; i < m_mpIFileToIDFile.size(); i++) {
            if (m_mpIFileToIDFile[i] == *pfileId) {
                fIFileFound = true;
                *pfileId = static_cast<DWORD>(i);
                break;
            }
        }
        if (fIFileFound == false) {
            assert(false);
            return false;
        }
    }

    if (poffset && pseg) {
        m_pBlockEnum->GetSegOffset(pseg, poffset, pcb);
    }

    DWORD cLines;
    CV_Line_t* _pLines;
    CV_Column_t* _pColumns;

    m_pBlockEnum->Get(&cLines, &_pLines, &_pColumns);

    if ((pLines || pColumns) && pcLines) {
        if (pLines) {
            if (_pLines == NULL) {
                return false;
            }
            memcpy(pLines, _pLines, min(cLines, *pcLines)*sizeof(CV_Line_t));

            // Check for a block adjustment

            DWORD offSec, offBlk;

            m_pSection->GetSegOffset(NULL, &offSec, NULL);
            m_pBlockEnum->GetSegOffset(NULL, &offBlk, NULL);

            if (offBlk - offSec > 0) {
                DWORD delta = offBlk - offSec;
                for (DWORD i = 0; i < cLines; ++i) {
                    pLines[i].offset -= delta;
                }
            }
        }

        if (pColumns) {
            if (_pColumns == NULL) {
                return false;
            }
            memcpy(pColumns, _pColumns, min(cLines, *pcLines)*sizeof(CV_Column_t));
        }
    } 
    if (pcLines) {
        *pcLines = cLines;
    }
    return true;
}


bool Mod1::fInitRefBuffer(RefBuf *prbuf)
{
    if (*prbuf == NULL) {
        if ((*prbuf = new RefCountedBuffer) == NULL) {
            ppdb1->setOOMError();
            return false;
        }
    }
    return true;
}

bool Mod1::findC13Lines()
{
    MTS_PROTECT(m_csForC13);

    if (rbufC13Lines != NULL && rbufC13Lines->Size() > 0) {
        return true;
    }

    if (!fInitRefBuffer(&rbufC13Lines))
        return false;

    if (cbC13Lines() > 0) {
        CB cb = cbC13Lines();

        if (!rbufC13Lines->Reserve(cb)) {
            ppdb1->setOOMError();
            return false;
        }

        if (!fReadPbCb(rbufC13Lines->Start(), &cb, cbSyms() + cbLines(), cb) || cb != cbC13Lines())
            return false;
    }
    else {
        if (fCoffToC13Failed) {
            return false;
        }

        // attempt to convert old cvinfo lines to C13 lines

        fCoffToC13Failed = true;   // haven't yet converted

        COMRefPtr<IDebugSSectionWriter> pWriter1st;
        if (!CheckFCreateWriter(false, &pWriter1st, CV_SIGNATURE_C13, true)) {
            return false;
        }

        COMRefPtr<IDebugSSectionWriter> pWriter;
        if (!CheckFCreateWriter(false, &pWriter, CV_SIGNATURE_C13, false)) {
            return false;
        }

        CB cb = 0;
        PB pb = NULL;

        if (!QueryLines(NULL, &cb)) {
            return false;
        }

        if (cb == 0) {
            fCoffToC13Failed = false;
            return true;
        }

        pb = new BYTE[cb];
        if (pb == NULL) {
            return false;
        }

        if (!QueryLines(pb, &cb)) {
            delete [] pb;
            return false;
        }

        LineBuffer lbuf(true);
        lbuf.fInit(pb, cb);
        
        PB pbRaw = NULL;
        CB cbRaw = 0;

        pWriter1st->StartSectionSymId(0);

        for (DWORD iFile = 0; iFile < lbuf.cFile(); ++iFile) {
            DWORD len = 0;
            LineBuffer::SPB *spb = lbuf.file(iFile);
            wchar_t szFile[ MAX_PATH ];
            const char* sz = lbuf.fileName(iFile, &len);

            UTF8ToUnicodeCch(sz, len+1, szFile, MAX_PATH);
            
            szFile[ MAX_PATH-1 ] = 0;
            
            DWORD idFile = pWriter1st->AddSourceFile(szFile, NULL, 0, CHKSUM_TYPE_NONE);
            if (idFile == IDebugSSectionWriter::BAD_INDEX) {
                return false;
            }

            WORD seg = 0xffff;  // bad seg number

            for (DWORD iSeg = 0; iSeg < spb->cSeg; ++iSeg) {
                LineBuffer::SPO* spo = lbuf.fileSeg(spb, iSeg);
                LineBuffer::SE* se = &lbuf.segStartEnd(spb)[iSeg];

                if (!pWriter->StartSection(se->start, spo->Seg, se->end-se->start+1)) {
                    // convert offset to count of bytes
                    return false;
                }

                for(DWORD iLine = 0; iLine < spo->cPair; ++iLine) {
                    if (!pWriter->AddLine(idFile, spo->offset[iLine] - se->start,
                                          lbuf.linenumbers(spo)[iLine],
                                          lbuf.linenumbers(spo)[iLine])) {
                        return false;
                    }
                }

                if (!pWriter->EndSection()) {
                    return false;
                }

                cbRaw = static_cast<CB>(pWriter->GetSectionBytes(&pbRaw));
                if (!rbufC13Lines->Append(pbRaw+sizeof(DWORD), cbRaw-sizeof(DWORD))) {
                    ppdb1->setOOMError();
                    return false;
                }

                DWORD pad = dcbAlign(cbRaw-sizeof(DWORD));
                if (pad > 0 && !rbufC13Lines->Reserve(pad)) {
                    ppdb1->setOOMError();
                    return false;
                }
            }
        }

        if (!pWriter1st->EndSection()) {
            return false;
        }

        cbRaw = static_cast<CB>(pWriter1st->GetSectionBytes(&pbRaw));
        if (!rbufC13Lines->Append(pbRaw+sizeof(DWORD), cbRaw-sizeof(DWORD))) {  // skip the signature
            ppdb1->setOOMError();
            return false;
        }

        DWORD pad = dcbAlign(cbRaw-sizeof(DWORD));
        if (pad > 0 && !rbufC13Lines->Reserve(pad)) {
            ppdb1->setOOMError();
            return false;
        }

        if (!processC13(false)) {
            return false;
        }

        fCoffToC13Failed = false;
    }

    // Cache the fileinfo in a seperate buffer, so we can get rid of this one
    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(*rbufC13Lines, &pC13Reader)) {
        return false;
    }

    COMRefPtr<IDebugSSubSectionEnum> pEnum;
    if (pC13Reader->GetSectionEnum(&pEnum) && pEnum != NULL) {
        while (pEnum->Next()) {
            COMRefPtr<IDebugSSubSection> pSection;
            pEnum->Get(&pSection);
            if (pSection == NULL) {
                setDSRError(ppdb1, pC13Reader->GetLastError());
                return false;
            }
            if (pSection->Type() == DEBUG_S_FILECHKSMS) {

                // Cache this subsection
                PB pbFileInfo = NULL;
                size_t cbFileInfo = pSection->GetRawBytes(&pbFileInfo);
                if (!bufFileInfo.Append(pbFileInfo, static_cast<CB>(cbFileInfo))) {
                    ppdb1->setOOMError();
                    rbufC13Lines->Free();
                    return false;
                }

                // Cache a mapping from File index (as in FileInfo blob) to FileID (as in the bufFileInfo subsection)
                COMRefPtr<IDebugSFileInfo> pC13Files;
                if (FAILED(pSection->QueryInterface(IID_IDebugSFileInfo, reinterpret_cast<void**>(&pC13Files)))) {
                    return false;
                }
                COMRefPtr<IDebugSFileEnum> pFileEnum;
                if (!pC13Files->GetFileEnum(&pFileEnum)) {
                    setDSRError(ppdb1, pC13Reader->GetLastError());
                    return false;
                }
                for(DWORD iFile = 0; pFileEnum->Next(); iFile++) {
                    DWORD idFile;
                    pFileEnum->Get(&idFile, NULL, NULL, NULL, NULL);
                    if (!mpIFileToIDFile.append(idFile)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

BOOL Mod1::TranslateFileId(DWORD id, DWORD *pid)
{
    if (mpIDFileToIFile.count() == 0) {
        for (DWORD i = 0; i < mpIFileToIDFile.size(); i++) {
            if (!mpIDFileToIFile.add(mpIFileToIDFile[i], i)) {
                ppdb1->setOOMError();
                return FALSE;
            }
        }
    }

    DWORD iFileId;

    if (!mpIDFileToIFile.map(id, &iFileId)) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    if (pid != NULL) {
        *pid = iFileId;
    }

    return TRUE;
}

bool Mod1::fQueryC13LinesBuf(DWORD cb, PB pb, DWORD *pcb, enum DEBUG_S_SUBSECTION_TYPE e)
{
    MTS_PROTECT(m_csForC13);

    assert(pcb != NULL);

    if (!findC13Lines()) {
        return false;
    }

    assert(rbufC13Lines != NULL);

    if (rbufC13Lines->Size() == 0) {
        *pcb = 0;
        return true;
    }

    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(*rbufC13Lines, &pC13Reader)) {
        return false;
    }

    COMRefPtr<IDebugSSubSectionEnum> pEnum;
    if (!pC13Reader->GetSectionEnum(&pEnum) || pEnum == NULL) {
        return false;
    }

    PB pbData = NULL;
    DWORD cbData = 0;

    while (pEnum->Next()) {
        COMRefPtr<IDebugSSubSection> pSection;

        pEnum->Get(&pSection);
        if (pSection == NULL) {
            setDSRError(ppdb1, pC13Reader->GetLastError());
            return FALSE;
        }

        if (pSection->Type() == e) {
            cbData = static_cast<DWORD>(pSection->GetData(&pbData));
            break;
        }
    }

    if (cbData == 0) {
        *pcb = 0;
        return true;
    }

    assert(pbData != NULL);

    if (cb != 0 && pb != NULL) {
        *pcb = min(cb, cbData);
        memcpy(pb, pbData, *pcb);

        if (e == DEBUG_S_INLINEELINES) {
            PB    pbRaw = pb;
            DWORD cbRaw = cb;

            bool fExtended = (*(DWORD *)pbRaw == CV_INLINEE_SOURCE_LINE_SIGNATURE_EX);

            pbRaw += sizeof(DWORD);
            cbRaw -= static_cast<DWORD>(sizeof(DWORD));

            while (cbRaw >= (fExtended ? sizeof(CodeViewInfo::InlineeSourceLineEx)
                                       : sizeof(CodeViewInfo::InlineeSourceLine))) {
                CodeViewInfo::InlineeSourceLine *p =
                    (CodeViewInfo::InlineeSourceLine *) pbRaw;

                if (!TranslateFileId(p->fileId, (DWORD *) &p->fileId)) {
                    return false;
                }

                size_t cbRec = sizeof(CodeViewInfo::InlineeSourceLine);

                if (fExtended) {
                    CodeViewInfo::InlineeSourceLineEx *pEx =
                        (CodeViewInfo::InlineeSourceLineEx *) pbRaw;

                    for (DWORD i = 0; i < pEx->countOfExtraFiles; i++) {
                        if (!TranslateFileId(pEx->extraFileId[i], (DWORD *) &pEx->extraFileId[i])) {
                            return false;
                        }
                    }

                    cbRec = sizeof(CodeViewInfo::InlineeSourceLineEx) +
                            pEx->countOfExtraFiles * sizeof(CV_off32_t);
                }
               
                cbRaw -= static_cast<DWORD>(cbRec);
                pbRaw += cbRec;
            }
        }

        return *pcb <= cb;
    }
    else {
        *pcb = cbData;
        return true;
    }
}

BOOL Mod1::QueryFileChecksums(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_FILECHKSMS);
}

BOOL Mod1::QueryLines3(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_LINES);
}

BOOL Mod1::QueryILLines(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_IL_LINES);
}

BOOL Mod1::QueryInlineeLines(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_INLINEELINES);
}

BOOL Mod1::QueryCrossScopeExports(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_CROSSSCOPEEXPORTS);
}

BOOL Mod1::QueryCrossScopeImports(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_CROSSSCOPEIMPORTS);
}

BOOL Mod1::QueryFuncMDTokenMap(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_FUNC_MDTOKEN_MAP);
}

BOOL Mod1::QueryTypeMDTokenMap(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_TYPE_MDTOKEN_MAP);
}

BOOL Mod1::QueryMergedAssemblyInput(DWORD cb, PB pb, DWORD *pcb)
{
    return fQueryC13LinesBuf(cb, pb, pcb, DEBUG_S_MERGED_ASSEMBLYINPUT);
}

bool Mod1::InitC13Reader(Buffer& bufSSubSection, IDebugSSectionReader **ppC13Reader)
{
    if (!CheckFCreateReader(bufSSubSection.Start(), bufSSubSection.Size(), ppC13Reader, CV_SIGNATURE_C13) 
        || *ppC13Reader == NULL) 
        return false;
    return true;
}

bool Mod1::GetEnumLines(EnumLines** ppenum)
{
    if (ppenum == NULL)
        return false;

    if (!findC13Lines())
        return false;

    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(*rbufC13Lines, &pC13Reader))
            return false;

    EnumC13Lines *pLines = new EnumC13Lines;
    if (!pLines) {
        ppdb1->setOOMError();
        return false;
    }

    if (!pLines->Init(rbufC13Lines, mpIFileToIDFile, pC13Reader)) {
        delete pLines;
        return false;
    }

    *ppenum = pLines;
    return true;
}

bool Mod1::GetEnumILLines(EnumLines** ppenum)
{
    if (!GetEnumLines(ppenum)) {
        return false;
    }

    ((EnumC13Lines *)(*ppenum))->SetILLines(true);
    return true;
}

bool Mod1::QueryLineFlagsHelper(OUT DWORD* pdwFlags, bool fIL)
{
    if (pdwFlags == NULL)
        return false;

    if (!findC13Lines())
        return false;

    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(*rbufC13Lines, &pC13Reader))
        return false;

    COMRefPtr<IDebugSSubSectionEnum> pEnum = NULL;
    if (!pC13Reader->GetSectionEnum(&pEnum)) {
        setDSRError(ppdb1, pC13Reader->GetLastError());
        return false;
    }

    while (pEnum->Next()) {
        COMRefPtr<IDebugSSubSection> pSection;
        pEnum->Get(&pSection);
        if (pSection == NULL) {
            setDSRError(ppdb1, pC13Reader->GetLastError());
            return false;
        }

        if (fIL) {
            if (pSection->Type() != DEBUG_S_IL_LINES) {
                continue;
            }
        } else {
            if (pSection->Type() != DEBUG_S_LINES) {
                continue;
            }
        }

        COMRefPtr<IDebugSLines> pLines;
        if (SUCCEEDED(pSection->QueryInterface(IID_IDebugSLines, (void**)&pLines))) {
            *pdwFlags = pLines->GetFlags();
            return true;
        }
    }
    return false;
}

bool Mod1::QueryLineFlags(OUT DWORD* pdwFlags)
{
    return QueryLineFlagsHelper(pdwFlags, false);
}

bool Mod1::QueryILLineFlags(OUT DWORD* pdwFlags)
{
    return QueryLineFlagsHelper(pdwFlags, true);
}

bool Mod1::QueryFileNameInfo(
    IN DWORD        fileId,                 // source file identifier
    __out_ecount_full_opt(*pccFilename) wchar_t*    szFilename,    // file name string buffer
    IN OUT DWORD*   pccFilename,            // length of filename in chars
    OUT DWORD*      pChksumType,            // type of chksum
    OUT BYTE*       pbChksum,               // pointer to buffer for chksum data
    IN OUT DWORD*   pcbChksum               // number of bytes of chksum (in/out)
    ) 
{
    if (szFilename && !pccFilename)
        return false;

    if (pbChksum && !pcbChksum)
        return false;

    {
        MTS_PROTECT(m_csForC13);
        if (bufFileInfo.Size() == 0 && !findC13Lines()) {
            return false;
        }
    }

    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(bufFileInfo, &pC13Reader)) {
        return false;
    }

    COMRefPtr<IDebugSFileInfo> pC13Files;
    if (!pC13Reader->GetFileInfo(&pC13Files)) {
        return false;
    }

    return QueryFileNameInfoInternal(pC13Files, fileId, szFilename, pccFilename, pChksumType, pbChksum, pcbChksum);
}

bool Mod1::QueryFileNameInfoInternal(
                    IDebugSFileInfo* pC13Files,
                    IN DWORD         fileId,                 // source file identifier
                    __out_ecount_full_opt(*pccFilename) wchar_t*    szFilename,             // file name string buffer
                    IN OUT DWORD*    pccFilename,            // length of filename in chars
                    OUT DWORD*       pChksumType,            // type of chksum
                    OUT BYTE*        pbChksum,               // pointer to buffer for chksum data
                    IN OUT DWORD*    pcbChksum,              // number of bytes of chksum (in/out)
                    NameMap*         pNameMapIn
                   )               
{
    
    assert(bufFileInfo.Size() != 0);
    assert(!(szFilename && !pccFilename));
    assert(!(pbChksum && !pcbChksum));

    if (!(fileId < mpIFileToIDFile.size())) {
        // invalid file id
        return false;
    }

    // Get the fileId from IFile
    fileId = mpIFileToIDFile[ fileId ];

    DWORD poString = 0;
    BYTE* _pbChksum = NULL;
    DWORD _cbChksum = 0;
    if (!pC13Files->GetFileById(fileId, &poString, pChksumType, &_pbChksum, &_cbChksum))
        return false;

    if (szFilename) {
        NameMap* pNameMap = pNameMapIn;
        if (pNameMap == NULL && !NameMap::open(ppdb1, false, &pNameMap)) {
            return false;
        }

        size_t len = *pccFilename;
   
        if (!pNameMap->isValidNi(poString) ||
             !pNameMap->getNameW(poString, szFilename, &len)) {
            // corrupted
            if (pNameMapIn == NULL) {
                pNameMap->close();
            }
            return false;
        }

        *pccFilename = static_cast<DWORD>(len);
        if (pNameMapIn == NULL) {
            pNameMap->close();
        }
    }
    
    if (pbChksum) {
        memcpy(pbChksum, _pbChksum, min(*pcbChksum, _cbChksum));
    }

    if (pcbChksum) {
        *pcbChksum = _cbChksum;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
// Long filenames support


BOOL Mod1::ConvertFileNamesInLineInfoFmMBCSToUnicode(PB pbLines, CB& cbLines, Buffer& buffer)
{
    // Converts all the filenames in lineinfo from
    // MBCS to Unicode and returns the new length
    // of lineinfo in cb, cbMax is max length of buffer
    // pointed by pbLines

    MLI mli;

    if (!mli.Construct(pbLines) 
        || !mli.ConvertSrcFileNames()
        || !mli.EmitWithSZName(buffer))
        return FALSE;

    return TRUE;
}


BOOL Mod1::fForcedReadSymbolInfo(PB pbSym, CB& cbSym)
{
    MTS_ASSERT(pdbi1->m_csForMods);

    // Read the symbol information, widen it
    // if it's necessary
    BOOL fResult;

    if (cvtsyms.fConverting()) {
        assert(pwti);
        fResult = fReadAndConvertSyms(pbSym, &cbSym);
    }
    else
        fResult = fReadPbCb(pbSym, &cbSym, 0, cbSyms());

    return fResult;
}

BOOL Mod1::AddPublicW(const wchar_t* wszPublic, USHORT isect, long off, CV_pubsymflag_t cvpsf)
{
    USES_STACKBUFFER(0x400);
    assert(IS_SZ_FORMAT_PDB(ppdb1));

    UTFSZ utfszPublic = GetSZUTF8FromSZUnicode(wszPublic);
    if (!utfszPublic)
        return FALSE;

    return AddPublic2(utfszPublic, isect, off, cvpsf);
}

BOOL Mod1::AddLinesW(const wchar_t* szSrc, USHORT isectCon, OFF offCon, CB cbCon, OFF doff,
                      LINE32 lineStart, PB pbCoff, CB cbCoff)
{
    if (!pdbi1->fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    // cbCon is really not count of bytes that are described
    // by this line number, it really is last valid offset. Adjust
    // it to make it count of bytes, becasue that's what we
    // need to use in C13 lines.
    cbCon = cbCon + 1;

    // Create DebugSFileChkSMS and DebugSLines for these lines
    if (!m_pLinesWriter1st) {
        if (!CheckFCreateWriter(false, &m_pLinesWriter1st, CV_SIGNATURE_C13, true)) {
            return FALSE;
        }
        m_pLinesWriter1st->StartSectionSymId(0);
    }

    COMRefPtr<IDebugSSectionWriter> pLinesWriter;      // Saves lines for this con
    if (!CheckFCreateWriter(false, &pLinesWriter, CV_SIGNATURE_C13, false)) {
        return FALSE;
    }

    assert(m_pLinesWriter1st != NULL);
    assert(pLinesWriter != NULL);

    DWORD idFile = m_pLinesWriter1st->AddSourceFile(szSrc, NULL, 0, CHKSUM_TYPE_NONE);
    if (idFile == IDebugSSectionWriter::BAD_INDEX) {
        ppdb1->setOOMError();
        return FALSE;
    }

    assert(cbCoff % sizeof(IMAGE_LINENUMBER) == 0);
    int cLineNumbers = cbCoff / sizeof(IMAGE_LINENUMBER);

    assert(cLineNumbers > 0);

    IMAGE_LINENUMBER *rgImageLineNumbers = reinterpret_cast<IMAGE_LINENUMBER *>(pbCoff);

    // If this CON does not start a function (linenumber != 0), then 
    // the first line number (equivalant of .lf) has address of start
    // of CON, as opposed to the symbol table index of the CON.
    if (rgImageLineNumbers[0].Linenumber != 0) {
        // Does not represent a function start
        offCon = rgImageLineNumbers[0].Type.VirtualAddress;
    }

    if (!pLinesWriter->StartSection(offCon, isectCon, cbCon)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!pLinesWriter->AddLine(idFile, 0, lineStart, 0, true)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    for(int i = 1; i < cLineNumbers; i++) {
        IMAGE_LINENUMBER *pln = rgImageLineNumbers + i;

        // COFF doesn't have any way to indicate lineStart again - 0x7fff is interpreted
        // to solve that. A line offset of 0x7fff is interpreted to mean line offset of
        // 0 without beginning a new block of lines.
        DWORD dwLine = (pln->Linenumber == 0x7fff) ? lineStart : pln->Linenumber + lineStart;
        if (!pLinesWriter->AddLine(idFile, pln->Type.VirtualAddress + doff - offCon, dwLine, 0, true)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    if (!pLinesWriter->EndSection()) {
        ppdb1->setOOMError();
        return FALSE;
    }

    PB pbRaw = NULL;
    CB cbRaw = static_cast<CB>(pLinesWriter->GetSectionBytes(&pbRaw));
    if (cbRaw && !rbufC13Lines->Append(pbRaw+sizeof(DWORD), cbRaw-sizeof(DWORD))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    return TRUE;
}

BOOL Mod1::QueryNameW(OUT wchar_t szName[PDB_MAX_PATH], OUT CB * pcch)
{
    SafeStackAllocator<0x400> ___allocator;

    UTFSZ utfszModule = szModule();
    if (!utfszModule || !pcch)
        return FALSE;

    assert(strlen(utfszModule) < PDB_MAX_PATH);

    // The SZ must be in UTF8, convert it to Unicode
    wchar_t *wcs = szName;
    if (wcs == NULL) {
        *pcch = PDB_MAX_PATH;
        wcs = ___allocator.Alloc<wchar_t>(*pcch);
    }

    if (_GetSZUnicodeFromSZUTF8(utfszModule, wcs, *pcch) == NULL)
        return FALSE;

    *pcch = static_cast<CB>(wcslen(wcs) + 1);
    return TRUE;
}

BOOL Mod1::QueryFileW(OUT wchar_t szFile[PDB_MAX_PATH], OUT CB* pcch)
{
    SafeStackAllocator<0x400> ___allocator;

    UTFSZ utfszObjFile = szObjFile();
    if (!utfszObjFile || !pcch)
        return FALSE;

    // The SZ must be in UTF8, convert it to MBCS
    wchar_t *wcs = szFile;
    if (wcs == NULL) {
        *pcch = PDB_MAX_PATH;
        wcs = ___allocator.Alloc<wchar_t>(*pcch);
    }
    if (_GetSZUnicodeFromSZUTF8(utfszObjFile, wcs, *pcch) == NULL)
        return FALSE;

    *pcch = static_cast<CB>(wcslen(wcs) + 1);
    return TRUE;
}

BOOL Mod1::QuerySrcFileW(OUT wchar_t wcsFile[PDB_MAX_PATH], OUT CB* pcch)
{
    char szFile[PDB_MAX_PATH];
    CB cbFile = sizeof(szFile);

    if (pdbi1->srcForImod(imod, szFile, &cbFile) && pcch) {

        SafeStackAllocator<0x400> ___allocator;

        assert(strlen(szFile) < PDB_MAX_PATH);
        wchar_t *wcs = wcsFile;
        if (wcs == NULL) {
            *pcch = PDB_MAX_PATH;
            wcs   = ___allocator.Alloc<wchar_t>(*pcch);
        }

        if (IS_SZ_FORMAT_PDB(ppdb1)) {
            // The SZ must be in UTF8, convert it to Unicode
            if (_GetSZUnicodeFromSZUTF8(szFile, wcs, *pcch) == NULL)
                return FALSE;
        }
        else {
            // UNDONE: doesn't work with client/server with different codepage
            // The SZ must be in MBCS, convert it to Unicode
            if (_GetSZUnicodeFromSZMBCS(szFile, wcs, *pcch) == NULL)
                return FALSE;
        }

        *pcch = static_cast<CB>(wcslen(wcs) + 1);
        return TRUE;
    }
    else
        return FALSE;
}

BOOL Mod1::QueryPdbFileW(OUT wchar_t wcsFile[PDB_MAX_PATH], OUT CB* pcch)
{
    char szFile[PDB_MAX_PATH];
    CB cbFile = sizeof(szFile);

    if (pdbi1->pdbForImod(imod, szFile, &cbFile) && pcch) {

        SafeStackAllocator<0x400> ___allocator;

        assert(strlen(szFile) < PDB_MAX_PATH);
        wchar_t *wcs = wcsFile;
        if (wcs == NULL) {
            *pcch = PDB_MAX_PATH;
            wcs = ___allocator.Alloc<wchar_t>(*pcch);
        }

        if (IS_SZ_FORMAT_PDB(ppdb1)) {
            // The SZ must be in UTF8, convert it to Unicode
            if (_GetSZUnicodeFromSZUTF8(szFile, wcs, *pcch) == NULL)
                return FALSE;
        }
        else {
            // UNDONE: This does not work on the client/server with different codepage
            // The SZ must be in MBCS, convert it to Unicode
            if (_GetSZUnicodeFromSZMBCS(szFile, wcs, *pcch) == NULL)
                return FALSE;
        }

        *pcch = static_cast<CB>(wcslen(wcs) + 1);
        return TRUE;
    }
    else
        return FALSE;
}


//////////////////////////////////////////////////////////////////////////
// Convert C13 lines to C11

namespace C11Lines {
    struct FSB  { unsigned short cFile; unsigned short cSeg; long baseSrcFile[]; };
    struct SE   { long start; long end; };
    struct SPB  { short cSeg; short pad; long baseSrcLn[]; };
    struct SPO  { short Seg; short cPair; long offset[]; };

    struct C13FileLineInfo {
  
        C13FileLineInfo() : cSections(0) {};

        Array<SE> rgOffsets;
        Array<DWORD> rgCLines;
        Array<WORD> rgSecIds;
        Buffer bufFileLines;
        DWORD cSections;
    };
};

using namespace C11Lines;

BOOL Mod1::GetC11LinesFromC13(Buffer& bufC11Lines)
{
    AutoReleasePtr<EnumLines> pLinesEnum;
    if (!GetEnumLines(&pLinesEnum)) {
        return FALSE;
    }

    assert(bufFileInfo.Size() != 0);

    COMRefPtr<IDebugSSectionReader> pC13Reader;
    if (!InitC13Reader(bufFileInfo, &pC13Reader)) {
        return FALSE;
    }

    COMRefPtr<IDebugSFileInfo> pC13Files;
    if (!pC13Reader->GetFileInfo(&pC13Files)) {
        setDSRError(ppdb1, pC13Reader->GetLastError());
        return FALSE;
    }

    COMRefPtr<IDebugSFileEnum> pFileEnum;
    if (!pC13Files->GetFileEnum(&pFileEnum)) {
        setDSRError(ppdb1, pC13Reader->GetLastError());
        return FALSE;
    }

    DWORD cFile = 0;
    DWORD cSeg = 0;

    // TRUNCATE: discard file > USHRT_MAX
    for(;pFileEnum->Next() && cFile < USHRT_MAX; cFile++) {
        /* do nothing */;
    }

    Array<C13FileLineInfo> rgC13FileLineInfo(cFile);
    Array<SE>         rgse;
    Array<WORD>       rgseg;

    // TRUNCATE: discard seg > USHRT_MAX
    for(;pLinesEnum->next() && cSeg < USHRT_MAX; cSeg++) {
        DWORD fileId, offsetCon, cbCon, cLines;
        WORD  isectCon;

        pLinesEnum->getLines(&fileId, &offsetCon, &isectCon, &cbCon, &cLines, NULL);

        SE se = { offsetCon, offsetCon+cbCon-1 };
        if (!rgseg.append(isectCon) || !rgse.append(se)) {
            ppdb1->setOOMError();
            return FALSE;
        }

        assert(fileId < cFile);
        if (fileId >= cFile) {
            return FALSE;
        }

        rgC13FileLineInfo[fileId].cSections++;
        if (!rgC13FileLineInfo[fileId].rgOffsets.append(se) || 
            !rgC13FileLineInfo[fileId].rgCLines.append(cLines) || 
            !rgC13FileLineInfo[fileId].rgSecIds.append(isectCon)) 
        {
            ppdb1->setOOMError();
            return FALSE;
        }

        CV_Line_t *pLines = NULL;
        if (!rgC13FileLineInfo[fileId].bufFileLines.Reserve(cLines*sizeof(CV_Line_t), reinterpret_cast<PB*>(&pLines))) {
            ppdb1->setOOMError();
            return FALSE;
        }
        pLinesEnum->getLines(&fileId, &offsetCon, &isectCon, &cbCon, &cLines, pLines);
    }

    FSB fsb = { static_cast<unsigned short>(cFile), static_cast<unsigned short>(cSeg) };

    if (!bufC11Lines.AppendFmt("s", fsb.cFile) ||
        !bufC11Lines.AppendFmt("s", fsb.cSeg)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    DWORD offbaseSrcFile = bufC11Lines.Size();
    if (!bufC11Lines.Reserve(cFile*sizeof(long))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (fsb.cSeg && !bufC11Lines.Append(reinterpret_cast<PB>(&(rgse[0])), rgse.size()*sizeof(SE))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (fsb.cSeg && !bufC11Lines.Append(reinterpret_cast<PB>(&(rgseg[0])), rgseg.size()*sizeof(WORD))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (rgseg.size() & 1) {
        bufC11Lines.AppendFmt("s", 0); /* alignment pad */
    }

    pFileEnum->Reset();
    AutoClosePtr<NameMap> pNameMap;
    if (!NameMap::open(ppdb1, false, &pNameMap)) {
        return FALSE;
    }

    DWORD iFile = 0;
    for(iFile = 0; pFileEnum->Next() && iFile < USHRT_MAX; iFile++) {

        DWORD cbFileName = 0;
        char *utfszFileName = NULL;
        wchar_t wszFileName[PDB_MAX_PATH];
        DWORD cchFileName = PDB_MAX_PATH; 
        DWORD chksumtype;
        if (!QueryFileNameInfoInternal(pC13Files, iFile, wszFileName, &cchFileName, &chksumtype, NULL, NULL, pNameMap)) {
            return FALSE;
        }

        USES_STACKBUFFER(0x400);
        if (utfszFileName = GetSZUTF8FromSZUnicode(wszFileName)) {
            cbFileName = static_cast<DWORD>(strlen(utfszFileName));
        }
        else {
            ppdb1->setOOMError();
            return FALSE;
        }

        // patch in the start for this file
        DWORD *pdwOffbaseSrcFile = reinterpret_cast<DWORD*>(bufC11Lines.Start()+offbaseSrcFile);
        pdwOffbaseSrcFile[iFile] = bufC11Lines.Size();

        // this must be true because cSeg <= USHRT_MAX and cSections <= cSeg
        assert(rgC13FileLineInfo[iFile].cSections <= USHRT_MAX);

        WORD cSections = static_cast<WORD>(rgC13FileLineInfo[iFile].cSections);
        Array<SE>& rgOffsets = rgC13FileLineInfo[iFile].rgOffsets;
        Array<DWORD>& rgCLines = rgC13FileLineInfo[iFile].rgCLines;
        Array<WORD>& rgSecIds = rgC13FileLineInfo[iFile].rgSecIds;
        Buffer& bufFileLines = rgC13FileLineInfo[iFile].bufFileLines;

        // append info for this file ../OMFS
        if (!bufC11Lines.AppendFmt("ss", cSections, 0 /*pad*/)) {
            ppdb1->setOOMError();
            return FALSE;
        }
        DWORD offSegs = bufC11Lines.Size() + cSections*3*sizeof(ULONG);
        offSegs += static_cast<DWORD>(cbAlign(cbFileName + 1));

        for(unsigned iCon = 0; iCon < rgCLines.size(); iCon++) {
            DWORD cbSection = (DWORD)cbAlign(rgCLines[iCon] * (sizeof(DWORD)+sizeof(WORD))) + (2*sizeof(WORD));
            if (!bufC11Lines.AppendFmt("l", offSegs)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            offSegs += cbSection;
        }
        if (cSections && !bufC11Lines.Append(reinterpret_cast<PB>(&(rgOffsets[0])), (int)rgOffsets.size()*sizeof(SE))) {
            ppdb1->setOOMError();
            return FALSE;
        }
        if (!bufC11Lines.AppendFmt("zbf", utfszFileName, '\0', static_cast<int>(dcbAlign(cbFileName+1)))) {
            ppdb1->setOOMError();
            return FALSE;
        }
        CV_Line_t *pLines = reinterpret_cast<CV_Line_t*>(bufFileLines.Start());
        debug(DWORD cLines = static_cast<DWORD>(bufFileLines.Size()/sizeof(CV_Line_t)));

        for(unsigned iCon = 0; iCon < rgCLines.size(); iCon++) {
            DWORD offConStart = rgOffsets[iCon].start;
            DWORD cPairs = min(rgCLines[iCon], USHRT_MAX);      // TRUNCATE: cPairs > USHRT_MAX is discard
            if (!bufC11Lines.AppendFmt("ss", rgSecIds[iCon], cPairs)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            for(DWORD iPair1 = 0; iPair1 < cPairs; iPair1++) {
                if (!bufC11Lines.AppendFmt("l", pLines[iPair1].offset + offConStart)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
            }
            for(DWORD iPair2 = 0; iPair2 < cPairs; iPair2++) {
                WORD linenum;

                // The compiler will generate special line numbers like 0xfeefee (not to step onto)
                // and 0xf00f00 (not to step into).  When converting to 16-bit line numbers, we'll
                // represent these as USHRT_MAX and all others >= USHRT_MAX with (linenum & 0xfffe).
                if ((pLines[iPair2].linenumStart == 0xfeefee) ||
                    (pLines[iPair2].linenumStart == 0xf00f00)) {
                    linenum = USHRT_MAX;
                } else {
                    if (pLines[iPair2].linenumStart >= USHRT_MAX) {
                        // not 0xffff - leave that as a distinguishable line number to represent
                        // special line numbers: 0xfeefee and 0xf00f00
                        linenum = static_cast<WORD>(pLines[iPair2].linenumStart & 0xfffe);
                    } else {
                        linenum = static_cast<WORD>(pLines[iPair2].linenumStart);
                    }
                }

                if (!bufC11Lines.AppendFmt("s", linenum)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
            }
            if (cPairs & 1) {
                if (!bufC11Lines.AppendFmt("s", 0 /* pad */)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
            }
            pLines+= cPairs;
        }

        rgOffsets.clear();
        rgCLines.clear();
        rgSecIds.clear();
        bufFileLines.Clear();
    }
    assert(iFile == cFile);
    return TRUE;
}
