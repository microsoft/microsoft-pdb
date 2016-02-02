//////////////////////////////////////////////////////////////////////////////
// DBI: Debug Information API Implementation

#include "pdbimpl.h"
#include "dbiimpl.h"
#include "cvexefmt.h"
#include "cvhelper.h"
#include "critsec.h"
#include "output.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <malloc.h>
#include "verstamp.h"
#include "util_misc.h"

static inline SIG70 SIG70FromTSRecord(PTYPE pts)
{
    dassert(pts->leaf == LF_TYPESERVER_ST || pts->leaf == LF_TYPESERVER || pts->leaf == LF_TYPESERVER2);

    if (pts->leaf == LF_TYPESERVER2) { 
        return ((lfTypeServer2 *)&pts->leaf)->sig70;
    }
    else
    {
        return Sig70FromSig(((lfTypeServer *)&pts->leaf)->signature);
    }
}

static inline AGE AgeFromTSRecord(PTYPE ptype)
{
    if (ptype->leaf == LF_TYPESERVER2) {
        lfTypeServer2 *plf = (lfTypeServer2 *)&ptype->leaf;
        return plf->age;
    }
    else if (ptype->leaf == LF_TYPESERVER || ptype->leaf == LF_TYPESERVER_ST)
    {
        lfTypeServer *plf = (lfTypeServer *)&ptype->leaf;
        return plf->age;
    }
    else {
        assert(FALSE);
        return -1;
    }
}


// make sure our version numbers fit in the two ushorts we have
//
// major version is 7 bits (0-127), minor is 8 bits (0-255),
// build is 16 bits (0-65535) and rbld is 16 bits (0-65535).
//
cassert(rmj < 128);
cassert(rmm < 256);
cassert(rup < 65536);
cassert(rbld < 65536);

const char DBI1::c_szLinkInfoStr[] = "/LinkInfo";

#ifdef INSTRUMENTED
void DBI1::DumpSymbolPages()
{
    char szBuf[256];
    unsigned int iPg;
    int ich;
    int cPgLoaded=0;

    sprintf(szBuf, "\n\n**** Symbol page dump for %s ****\r\n", ppdb1->szPDBName);
    OutputDebugString(szBuf);

    if (cSymRecPgs == 0)
    {
        sprintf(szBuf, "(No pages)");
        OutputDebugString(szBuf);
        return;
    }

    for (iPg=0, ich=0; iPg < cSymRecPgs; iPg++)
    {
        // Is page loaded?
        if (pbvSymRecPgs->fTestBit(iPg))
        {
            szBuf[ich++] = 'X';
            cPgLoaded++;
        }
        else
        {
            szBuf[ich++] = '-';
        }

        // Output 50 pages per line (200K per line)
        if (ich == 49)
        {
            szBuf[ich++] = '\r';
            szBuf[ich++] = '\n';
            szBuf[ich++] = '\0';
            OutputDebugString(szBuf);
            ich = 0;
        }
    }

    if (ich != 0)
    {
        szBuf[ich++] = '\r';
        szBuf[ich++] = '\n';
        szBuf[ich++] = '\0';
        OutputDebugString(szBuf);
    }

    sprintf(szBuf, "%d out of %d pages are loaded (%d%%)\r\n", cPgLoaded, cSymRecPgs, (cPgLoaded*100)/cSymRecPgs);
    OutputDebugString(szBuf);
}
#endif

#pragma warning(4:4355)   // 'this' used in member init list...

DBI1::DBI1(PDB1* ppdb1_, BOOL fWrite_, bool fCTypes_, bool fTypeMismatch_)
:   bufGpmodi(fixBufGpmodi, (void*)this, !!fWrite_),  // don't alloc extra padding for read-only
    bufRgpmodi(fixBufBase, (void*)&rgpmodi),
    bufSymRecs(!fWrite_, fixSymRecs, this),
    bufSC(0, 0, !!fWrite_),     // don't alloc extra padding for read-only
    bufSecMap(0, 0, !!fWrite_), // don't alloc extra padding for read-only
    fFrameDataLoaded(FALSE),
    pdbgFrameData(NULL),
#ifdef _DEBUG
    fFrameDataAdded(false),
#endif
#ifdef PDB_DEFUNCT_V8
    fGlobalsConverted(FALSE),
#endif
#ifdef PDB_TYPESERVER
    bufTSMap(FALSE),             // don't allow adding type servers
    m_pfnFindDIF(0),
#endif
    ptpi(0),
	pipi(0),
    m_fCTypes(fCTypes_),
    m_fTypeMismatch(fTypeMismatch_)
{
    ppdb1 = ppdb1_;
    fWrite = fWrite_;
    pgsiGS = 0;
    pgsiPS = 0;
    potmTSHead = 0;
    potmPCTHead = 0;
    for(size_t i = 0; i < _countof(rgOpenedDbg); i++) {
        rgOpenedDbg[i] = NULL;
    }
    for(size_t i = 0; i < _countof(fDbgDataVerified); i++) {
        fDbgDataVerified[i] = FALSE;
    }
#ifndef PDB_MT
    imodLastFMN = 0;
    imodLastQNM = imodNil;
#endif

#ifdef INSTRUMENTED
    log = 0;
#endif
    imodMac = 0;
    fSCCleared = FALSE;
    rgpmodi = (MODI**)bufRgpmodi.Start();
    expect(fAlign(rgpmodi));
    pbvSymRecPgs = 0;
#ifdef PDB_DEFUNCT_V8
    m_cbSymRecs = 0;
#endif
    pwti = 0;
#ifdef PDB_TYPESERVER
    popdbHead = 0;
#endif
    pbscEnd = NULL;
    fSCv2 = false;

    m_dbicbc.pvContext = NULL;
    m_dbicbc.pfnQueryCallback = NULL;
    m_dbicbc.pfnNotePdbUsed = NULL;
    m_dbicbc.pfnNoteTypeMismatch = NULL;
    m_dbicbc.pfnTmdTypeFilter = NULL;

    m_rgpTmdBuckets = NULL;
}
#pragma warning(default:4355)

BOOL DBI1::QueryPdb(OUT PDB** pppdb)
{
    assert(pppdb);
    *pppdb = ppdb1;
    return true;
}

void DBI1::fixBufGpmodi(void* pDbi1, void* pOld, void* pNew)
{
    if (pOld && pNew) {
        DBI1* pdbi1 = (DBI1*)pDbi1;

        MTS_ASSERT(pdbi1->m_csForMods);

        ptrdiff_t dcb = PB(pNew) - PB(pOld);

        for (IMOD imod = 0; imod < pdbi1->imodMac; imod++) {
            pdbi1->rgpmodi[imod] = (MODI*)((PB)pdbi1->rgpmodi[imod] + dcb);
        }
    }
}

void DBI1::fixBufBase(void* pv, void* pvOld, void* pvNew)
{
    *(void**)pv = pvNew;
}

inline BOOL convertMODIFromSTtoSZ(Buffer &bufIn)
{
    // Actually all the MODI are already in SZ format,
    // just that the strings are in MBCS, must convert
    // them to UTF8.

    Buffer bufTemp;

    if (!bufTemp.Append(bufIn.Start(), bufIn.Size()))
        return FALSE;

    bufIn.Reset();

    for (PB pb = bufTemp.Start();
            pb < bufTemp.End();
            pb = ((MODI*)pb)->pbEnd())
    {
        expect(fAlignNative(pb));
        MODI* pmodi = (MODI*)pb;

        // Copy all non string data
        PB pbMODIStart;
        if (!bufIn.Append(pb, sizeof MODI, &pbMODIStart)) 
            return FALSE;

        expect(fAlignNative(pbMODIStart));

        // Copy translated strings.
        size_t len = strlen(pmodi->rgch) + 1;
        len += strlen(pmodi->rgch + len) + 1;

        assert(len < 2 * PDB_MAX_PATH);  // Assumption - We can't handle 
                                         // filenames having > 256 chars anyway

        // Skip trailing NULLs
        char szBuf[2048];
        size_t cbBuf = MBCSToUTF8(pmodi->rgch, len, szBuf, 2048);

        if (!bufIn.Append((PB)szBuf, static_cast<CB>(cbBuf))) 
            return FALSE;

        if (!bufIn.AppendFmt("f", dcbAlignNative(bufIn.Size()))) 
            return FALSE;
    }

    return TRUE;
}

BOOL DBI1::fInit(BOOL fCreate)
{
#if defined(INSTRUMENTED)
    if (fWrite)
        log = LogOpen();
    if (log)
        LogNoteEvent(log, "DBI", 0, letypeBegin, 0);
#endif
    MTS_PROTECT(ppdb1->m_csPdbStream);
    MTS_PROTECT(m_csForMods);
    MTS_PROTECT(m_csSecContribs);

    MSF* pmsf = ppdb1->pmsf;
    CB cbDbiStream = pmsf->GetCbStream(snDbi);

    if (cbDbiStream > 0) {
        // read in signature and version
        CB      cbread = sizeof(ULONG);
        ULONG   verSig;
        ULONG   verHdrOnDisk = NewDBIHdr::hdrVersion;

        if (cbDbiStream < cbread) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        if (!pmsf->ReadStream(snDbi, 0, &verSig, &cbread)) {
            ppdb1->setReadError();
            return FALSE;
        }
        if (verSig != NewDBIHdr::hdrSignature) {
            // create a WidenTi object...
            if (!WidenTi::fCreate(pwti, 1, wtiSymsNB10)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            // old version, load up and convert
            dassert (ppdb1->pdbStream.impv < PDB1::impv);
            cvtsyms.fConverting() = TRUE;

            // set up and read an old header
            DBIHdr  dbiHdrOld;
            cbread = sizeof(DBIHdr);
            if (cbDbiStream < cbread) {
                ppdb1->setCorruptError();
                return FALSE;
            }
            if (!pmsf->ReadStream(snDbi, 0, &dbiHdrOld, &cbread)) {
                ppdb1->setReadError();
                return FALSE;
            }
            verHdrOnDisk = 0;
            dbihdr = dbiHdrOld;
        }
        else {
            // read in a new header!
            cbread = sizeof(NewDBIHdr);
            if (cbDbiStream < cbread) {
                ppdb1->setCorruptError();
                return FALSE;
            }
            if (!pmsf->ReadStream(snDbi, 0, &dbihdr, &cbread)) {
                ppdb1->setReadError();
                return FALSE;
            }
            dassert (dbihdr.verSignature == NewDBIHdr::hdrSignature);
            if (dbihdr.verHdr < DBIImpvV50 || ppdb1->pdbStream.impv < PDB1::impvVC50) {
                // we never use new header before V5
                ppdb1->setCorruptError();
                return FALSE;
            }
            cvtsyms.fConverting() = FALSE;
            verHdrOnDisk = dbihdr.verHdr;
            if (dbihdr.verHdr < DBIImpvV60) {
                dbihdr.cbECInfo = 0;    // make sure this count is 0
            }
        }

        // Set the fCTypes flag correctly on read/write

        if (fWrite) {
            dbihdr.flags.fCTypes = m_fCTypes;
        }
        else {
            m_fCTypes = dbihdr.flags.fCTypes;
        }

        OFF off = cbread;
        CB const cbdbihdr = cbread;

        dassert(cbDbiStream == cbdbihdr + dbihdr.cbGpModi + dbihdr.cbSC +
            dbihdr.cbSecMap + dbihdr.cbFileInfo + dbihdr.cbTSMap + 
            dbihdr.cbDbgHdr + dbihdr.cbECInfo); 
        if (cbDbiStream != cbdbihdr + dbihdr.cbGpModi + dbihdr.cbSC +
            dbihdr.cbSecMap + dbihdr.cbFileInfo + dbihdr.cbTSMap + 
            dbihdr.cbDbgHdr + dbihdr.cbECInfo)
        {
            ppdb1->setCorruptError();
            return FALSE;
        }

        if (fWrite && (ppdb1->pdbStream.impv == PDB1::impvVC4)) {
            // can't incremental link a vc4 format pdb, we have introduced
            // new scheme to track refcounts on promoted global syms - return
            // a format error here so that we force a full link and rewrite of
            // all of the mods streams
            // sps 8/14/95
            if (fCreate)
                // ok we are rewritting this as a vc 4.1 dbi
                ppdb1->pdbStream.impv = PDB1::impv;
            else {
                ppdb1->setLastError(EC_FORMAT);
                return FALSE;
            }
        }

        // read in the gpmodi substream
        if (dbihdr.cbGpModi > 0) {
            expect(fAlign(dbihdr.cbGpModi));

            if (cbDbiStream - off < dbihdr.cbGpModi) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            // load gpmodi
            PB pb;
            bool fSzModNameCheck = false;
            if (verHdrOnDisk < DBIImpvV60) {
                // read in v5 modi into temp table and do initial alloc of
                // bufGpmodi which will hold the converted v6 modi
                Buffer bufGpV5modi;
                PB pbV5;

                if (!bufGpmodi.SetInitAlloc(dbihdr.cbGpModi) ||
                    !bufGpV5modi.ReserveNoZeroed(dbihdr.cbGpModi, &pbV5)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                else if (!pmsf->ReadStream(snDbi, off, pbV5, &(dbihdr.cbGpModi))) {
                    ppdb1->setReadError();
                    return FALSE;
                }

                // pass thru v5 modi table and copy/convert into v6 modi table
                PB const pbEnd = bufGpV5modi.End();
                while (pbV5 < pbEnd)
                {
                    if (!fValidateSz(((MODI50*)pbV5)->szModule(), (SZ)pbEnd) ||
                        !fValidateSz(((MODI50*)pbV5)->szObjFile(), (SZ)pbEnd)) 
                    {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }
                          
                    (void) new (bufGpmodi, 
                        ((MODI50*)pbV5)->szModule(), 
                        ((MODI50*)pbV5)->szObjFile()) MODI(*(MODI50*)pbV5);

                    pbV5 = ((MODI50*)pbV5)->pbEnd();
                }

                pb = bufGpmodi.Start();
                off += dbihdr.cbGpModi;
                dbihdr.cbGpModi = bufGpmodi.Size();
            }
            else if (sizeof(MODI) != sizeof(MODI_60_Persist)) {
                // handle the Win64 case where we have to have pointers fixed
                // up appropriately.

                // Calculate the upper bound on the memory required.
                CB          cbInMemMax = dbihdr.cbGpModi;
                unsigned    cmodi = (cbInMemMax + sizeof(MODI_60_Persist) - 1) / sizeof(MODI_60_Persist);

                cbInMemMax += cmodi * dcbModi60;

                Buffer  bufGpmodi60;
                PB      pbV6;

                if (!bufGpmodi.SetInitAlloc(cbInMemMax) ||
                    !bufGpmodi60.Reserve(dbihdr.cbGpModi, &pbV6)
                   )
                {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                CB  cbOnDisk = dbihdr.cbGpModi;
                if (!pmsf->ReadStream(snDbi, off, pbV6, &cbOnDisk)) {
                    ppdb1->setReadError();
                    return FALSE;
                }
                
                // pass through win32 modi 6.0 structures, load the bufGpmodi up
                // with the win64 version of the structure.
                PB const pbEnd = bufGpmodi60.End();
                while (pbV6 < pbEnd)
                {
                    PMODI60 pmodi = PMODI60(pbV6);
                    if (!fValidateSz(pmodi->szModule(), (SZ)pbEnd) ||
                        !fValidateSz(pmodi->szObjFile(), (SZ)pbEnd)) 
                    {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }
                    MODI * pmodiT = new (bufGpmodi, pmodi->szModule(), pmodi->szObjFile()) MODI(*pmodi);
                    expect(fAlignNative(pmodiT));

                    pbV6 = pmodi->pbEnd();
                }

                pb = bufGpmodi.Start();
                off += dbihdr.cbGpModi;
                dbihdr.cbGpModi = bufGpmodi.Size();
                expect(dbihdr.cbGpModi <= cbInMemMax);
            }
            else {
                if (!bufGpmodi.ReserveNoZeroed(dbihdr.cbGpModi, &pb)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                else if (!pmsf->ReadStream(snDbi, off, pb, &(dbihdr.cbGpModi))) {
                    ppdb1->setReadError();
                    return FALSE;
                }
                off += dbihdr.cbGpModi;
                fSzModNameCheck = true;
            }


            // Is this an ST pdb? if yes then convert the
            // module names from MBCS to UTF8
            if (!IS_SZ_FORMAT_PDB(ppdb1)) {
                if (!convertMODIFromSTtoSZ(bufGpmodi)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }

                pb = bufGpmodi.Start();
            }

            // build rgpmodi
            for (PB const pbEnd = bufGpmodi.End(); pb < pbEnd;
                 pb = ((MODI*)pb)->pbEnd(), imodMac++)
            {
                expect(fAlignNative(pb));
                MODI* pmodi = (MODI*)pb;
                if (fSzModNameCheck) {
                    if (!fValidateSz(pmodi->szModule(), (SZ)pbEnd) ||
                        !fValidateSz(pmodi->szObjFile(), (SZ)pbEnd))
                    {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }
                }
                pmodi->pmod = 0;
                pmodi->fWritten = FALSE;
                pmodi->ifileMac = 0;
                pmodi->mpifileichFile = 0;
                if (!bufRgpmodi.Append((PB)&pmodi, sizeof pmodi)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                expect(fAlignNative(&(rgpmodi[imodMac])));
                assert(rgpmodi[imodMac] == pmodi);
            }
        }

        dassert(off <= cbDbiStream);

        // read in the Section Contribution substream
        if (dbihdr.cbSC > 0) {
            expect(fAlign(dbihdr.cbSC));

            if (cbDbiStream - off < dbihdr.cbSC) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            offSC = off;
            off += dbihdr.cbSC;

            // Load sec contribs if we are going to write 
            // this PDB or else wait till someone asks for it.
            if (fWrite) {
                // DDB 124047 -- It is possible that the section contribution stream
                // contains only a version number but no other info.  So we need tell
                // between PDB corruption and empty sec contrib info.
                //
                // DDB 173223 -- The case where the stream had the version number but
                // nothing else was not clearing the error code, so if there was a 
                // previous error the 124047 fix was mis-diagnosing that as a corrupt
                // pdb, the function signature was changed to return TRUE on success
                // with the SecContribs pointer as an optional out parm. Here we only
                // care about whether there was an error.
                if (!getSecContribs(NULL))
                {
                    return FALSE;
                }
            }
        }

        dassert(off <= cbDbiStream);

        // read in the Section Map substream only if we are not writing
        if (dbihdr.cbSecMap) {
            if (cbDbiStream - off < dbihdr.cbSecMap) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            if (!fWrite) {
                expect(fAlign(dbihdr.cbSecMap));
                if (!bufSecMap.ReserveNoZeroed(dbihdr.cbSecMap)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
                else if (!pmsf->ReadStream(snDbi, off, bufSecMap.Start(), &(dbihdr.cbSecMap))) {
                    ppdb1->setReadError();
                    return FALSE;
                }
            }

            off += dbihdr.cbSecMap;
        }

        dassert(off <= cbDbiStream);

        if (dbihdr.cbFileInfo > 0) {
            expect(fAlign(dbihdr.cbFileInfo));

            if (cbDbiStream - off < dbihdr.cbFileInfo) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            Buffer bufFileInfo(0, 0, FALSE);        // no allocation padding
            if (!bufFileInfo.ReserveNoZeroed(dbihdr.cbFileInfo)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            else if (!pmsf->ReadStream(snDbi, off, bufFileInfo.Start(), &dbihdr.cbFileInfo)) {
                ppdb1->setReadError();
                return FALSE;
            }
            off += dbihdr.cbFileInfo;

            if (!reloadFileInfo(bufFileInfo.Start(), dbihdr.cbFileInfo)) {
                return FALSE;
            }
            if (!fWrite) {
                // If we are not writing (i.e. incremental linking) we can kill the memory
                // consumed by our NMT for the filenames.
                nmtFileInfo.clear();
            }
        }

        dassert(off <= cbDbiStream);

        // read in the TSM substream
        if (dbihdr.cbTSMap > 0) {
            expect(fAlign(dbihdr.cbTSMap));
            if (cbDbiStream - off < dbihdr.cbTSMap) {
                ppdb1->setCorruptError();
                return FALSE;
            }
            off += dbihdr.cbTSMap;
#ifdef PDB_TYPESERVER
            PB start;
            if (!bufTSMap.Reserve(dbihdr.cbTSMap, &start)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            else if (!pmsf->ReadStream(snDbi, off, start, &dbihdr.cbTSMap)) {
                ppdb1->setReadError();
                return FALSE;
            }
            bufTSMap.scan();
            if (!bufTSMServer.Reserve(bufTSMap.Length() * sizeof(TPI*))) {
                ppdb1->setOOMError();
                return FALSE;
            }
            for (UINT i = 0; i < bufTSMap.Length(); i++) {
                bufTSMServer[i] = NULL;   // ensure the server pointers are null
            }
            assert(bufTSMServer.length() == bufTSMap.Length());
            // if we are reading then open and init the tpi for querying 
            if (!fWrite) {
                assert(QueryLazyTypes());
                TPI* ptpi;
                if (!ppdb1->OpenTpi(fWrite ? pdbWrite : pdbRead, &ptpi))
                    return FALSE;
                assert(ptpi == ppdb1->ptpi1);
                ppdb1->ptpi1->UseTsm(0, this);
            }
#endif
        }

        dassert(off <= cbDbiStream);

        if (dbihdr.cbECInfo > 0) {    // read in the EC substream
            if (cbDbiStream - off < dbihdr.cbECInfo) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            PB start;
            Buffer buf(0, 0, FALSE);    // no allocation padding
            if (!buf.ReserveNoZeroed(dbihdr.cbECInfo, &start)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            else if (!pmsf->ReadStream(snDbi, off, start, &dbihdr.cbECInfo)) {
                ppdb1->setReadError();
                return FALSE;
            }
            off += dbihdr.cbECInfo;
            nmtEC.reload(&start, dbihdr.cbECInfo);
        }

        dassert(off <= cbDbiStream);

        if (dbihdr.cbDbgHdr > 0) {
            if (cbDbiStream - off < dbihdr.cbDbgHdr) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            // Read in only the amount we can handle
            CB  cbDbgHdrRead = min(dbihdr.cbDbgHdr, CB(sizeof(dbghdr)));

            if (!pmsf->ReadStream(snDbi, off, &dbghdr, &cbDbgHdrRead)) {
                ppdb1->setReadError();
                return FALSE;
            }
            // And move past the full amount there.
            off += dbihdr.cbDbgHdr;
        }
    }

    // for now we will completely delete old Mod info from previous builds on
    // a dbi creation.  later we may want to perform this instead of trying to
    // perform any garbage collection.

#pragma message ("todo - temporary clear dbi on create")
    if (fCreate && !clearDBI())
        return FALSE;

    if (fWrite) {
        // open the global and public symbol tables
        GSI* pgsigs_;
        GSI* pgsips_;

        if (!OpenGlobals(&pgsigs_) || !OpenPublics(&pgsips_))
            return FALSE;
         
        pgsiGS = (GSI1*) pgsigs_;
        pgsiPS = (PSGSI1*) pgsips_;

        // also open the TPI
        if (GetTpi() == NULL) {
            return FALSE;
        }

        // also open the IPI
        GetIpi();

        // just allocate the seg descriptor counters
        OMFSegMap omfsegmap = {0, 0};
        expect(fAlign(sizeof(omfsegmap)));
        if (!bufSecMap.Append((PB)&omfsegmap, sizeof(omfsegmap))) {
            ppdb1->setOOMError();
            return FALSE;
        }
        dbihdr.flags.fIncLink = false;  // always false until thunks are written

        if (ppdb1->m_fPdbTypeMismatch) {
            TPI * ptpi = GetTpi();
            TPI1 * ptpi1 = (TPI1 *) ptpi;

            assert(ptpi1 != NULL);

            m_rgpTmdBuckets = new (poolTmd) PTMD[ptpi1->cchnV8];
            if (m_rgpTmdBuckets == NULL) {
                ppdb1->setOOMError();
                return FALSE;
            }
            memset(m_rgpTmdBuckets, 0, sizeof(PTMD) * ptpi1->cchnV8);
        }
    }

    return TRUE;
}

inline BOOL nullifyStream(MSF* pmsf, SN* psn) {
    dassert(psn);
    if (*psn == snNil)
        return TRUE;

    dassert(pmsf);
    if (!pmsf->DeleteStream(*psn))
        return FALSE;

    *psn = snNil;
    return TRUE;
}


BOOL DBI1::clearDBI()
{
    // no need to MTS protect, only called form fInit

    MSF *pmsf = ppdb1->pmsf;

    // delete all mod streams

    for (IMOD imod = 0; imod < imodMac; imod++) {
        MODTYPEREF mtr = {0};

        if (!QueryModTypeRef(imod, &mtr)) {
            return FALSE;
        }

        MODI* pmodi = rgpmodi[imod];

        if (!nullifyStream(pmsf, &pmodi->sn)) {
            ppdb1->setWriteError();
            return FALSE;
        }

        pmodi->~MODI();

        if (mtr.rectyp != S_MOD_TYPEREF || mtr.fNone || mtr.fRefTM) {
            continue;
        }

        if (mtr.fOwnTMR || mtr.fOwnTM) {
            SN sn = (SN) mtr.word0;

            if (sn != snTpi && !nullifyStream(pmsf, &sn)) {
                ppdb1->setWriteError();
                return FALSE;
            }
        }

        if (mtr.fOwnTM) {
            SN sn = (SN) mtr.word1;

            if (sn != snNil && sn != snIpi && !nullifyStream(pmsf, &sn)) {
                ppdb1->setWriteError();
                return FALSE;
            }
        }
    }

    // delete sym records stream

    if (!nullifyStream(pmsf, &dbihdr.snSymRecs) ||
        !nullifyStream(pmsf, &dbihdr.snGSSyms)  ||
        !nullifyStream(pmsf, &dbihdr.snPSSyms))
    {
        ppdb1->setWriteError();
        return FALSE;
    }

    // delete DbgData streams
    for (ULONG i=0; i < dbgtypeMax; i++) {
        if (!nullifyStream(pmsf, &dbghdr.rgSnDbg[i])) {
            ppdb1->setWriteError();
            return FALSE;
        }
    }

    // commit these changes to recover their free pages
    // (Necessary to avoid doubling the pdb size.)
    if ((pmsf->GetCbStream(snDbi) != cbNil && !pmsf->DeleteStream(snDbi)) ||
        !pmsf->Commit())
    {
        ppdb1->setWriteError();
        return FALSE;
    }

    // we need to reclaim the snDbi stream to make sure no one sneaks in and
    // grabs it.
    if (pmsf->GetCbStream(snDbi) == cbNil && !pmsf->ReplaceStream(snDbi,0, 0))
    {
        ppdb1->setWriteError();
        return FALSE;
    }

    // clear out any LinkInfo data
    //
    Stream *    pstr;
    if (ppdb1->OpenStream(c_szLinkInfoStr, &pstr)) {
        // This should be pstr->Delete() but Strm::Delete is NYI
        //
        pstr->Replace(0, 0);
        pstr->Release();
    }

    // clear out all buffers and tables
    imodMac = 0;
    bufGpmodi.Clear();
    bufRgpmodi.Clear();
    bufSC.Clear();
#ifdef PDB_TYPESERVER
    bufTSMap.Clear();
#endif
    pbscEnd = bufSC.End();
    dbihdr.cbDbgHdr = 0;
    fSCCleared = TRUE;

    return TRUE;
}

inline BOOL DBI1::writeSymRecs() {
    MTS_ASSERT(m_csForSymRec);

    expect(fAlign(bufSymRecs.Size()));
    if (dbihdr.snSymRecs == snNil) {
        dbihdr.snSymRecs = ppdb1->pmsf->GetFreeSn();

        if (dbihdr.snSymRecs == snNil){
            ppdb1->setLastError(EC_LIMIT);
            return FALSE;
            }


        if (!ppdb1->pmsf->ReplaceStream(dbihdr.snSymRecs, bufSymRecs.Start(),
            bufSymRecs.Size())) {
            ppdb1->setWriteError();
            return FALSE;
            }
    }
    else {
        // just append whatever is new
        MSF* pmsf = ppdb1->pmsf;
        CB cb = pmsf->GetCbStream(dbihdr.snSymRecs);
        dassert(bufSymRecs.Size() >= cb);
        if (!pmsf->AppendStream(dbihdr.snSymRecs, bufSymRecs.Start() + cb,
            bufSymRecs.Size() - cb)) {
            ppdb1->setWriteError();
            return FALSE;
            }
    }

    return TRUE;
}

template<typename T>
int __cdecl SCCmp(const void* elem1, const void* elem2)
{
    T *        psc1 = (T*) elem1;
    T *        psc2 = (T*) elem2;
    unsigned   isect1 = psc1->isect;
    unsigned   isect2 = psc2->isect;


    if (isect1 != isect2)
        return (isect1 > isect2) ? 1 : -1;

    unsigned    off1 = psc1->off;
    unsigned    off2 = psc2->off;

    if (off1 != off2)
        return (off1 > off2) ? 1 : -1;

    return 0;
}

BOOL DBI1::fEnCSymbolsPresent()
{
    MTS_ASSERT(m_csForPGSI);

    GSI* pgsips_;

    if (!OpenPublics(&pgsips_)) {
        return FALSE;
    }

    if (pgsips_->HashSymW(L"__enc$textbss$begin", NULL) == NULL ||
        pgsips_->HashSymW(L"__enc$textbss$end", NULL) == NULL) {
        return FALSE;
    }

    return TRUE;
}

BOOL DBI1::IsLinkedIncrementally()
{
    MTS_PROTECT(m_csForHdr);
    MTS_ASSERT(m_csForPGSI);

    if (dbihdr.hdrVersion >= DBIImpvV70)
        return dbihdr.flags.fIncLink;

    if (pgsiPS) {
        return pgsiPS->cbSizeOfThunkMap() > 0;
    }    

    GSI* pgsips_;

    if (OpenPublics(&pgsips_)) {
        BOOL fThunkMap = pgsiPS->cbSizeOfThunkMap() > 0;
        pgsiPS->Close();
        pgsiPS = 0;
        return fThunkMap;
    }

    return FALSE;
}


BOOL DBI1::QuerySupportsEC()
{
    
    MTS_PROTECT(m_csForMods);
    MTS_PROTECT(m_csForPGSI);

    // We don't support EnC on ST PDB's anymore
    if (!IS_SZ_FORMAT_PDB(ppdb1)) {
        return FALSE;
    }

    if (!fEnCSymbolsPresent()) {
        return FALSE;
    }

    for (IMOD imod = 0; imod < imodMac; ++imod) {
        if (rgpmodi[imod]->fECEnabled) {
            return IsLinkedIncrementally();
        }
    }

    return FALSE;
}

BOOL DBI1::FStripped()
{
    MTS_PROTECT(m_csForHdr);
    return dbihdr.flags.fStripped;
}

BOOL DBI1::FCTypes()
{
    MTS_PROTECT(m_csForHdr);
    return dbihdr.flags.fCTypes;
}

BOOL DBI1::srcForImod(unsigned imod, char szSrcFile[ _MAX_PATH ], long* pcb)
{
    MTS_PROTECT(m_csForMods);
    MODI* pmodi;
    if ((pmodi = pmodiForImod((IMOD) imod)) && pmodi->fECEnabled) {
        NI ni = pmodi->niECSrcFile();
        SZ sz = nmtEC.szForNi(ni);
        if (sz) {
            strncpy_s(szSrcFile, _MAX_PATH, sz, _TRUNCATE);
            if (pcb)
                *pcb = CB(strlen(szSrcFile) + 1);
            return TRUE;
        }
    }
    return FALSE;
}


// used in Mod1::fProcessSym
BOOL DBI1::setSrcForImod(unsigned imod, SZ_CONST szSrcFile)
{
    MTS_PROTECT(m_csForMods);
    MODI* pmodi;
    if ((pmodi = pmodiForImod((IMOD) imod)) && szSrcFile) {
        NI ni;
        SZ sz = 0;
        if (pmodi->niECSrcFile() != niNil) {   // see what name is already there
            ni = pmodi->niECSrcFile();
            sz = nmtEC.szForNi(ni);
        }
        if (sz && strcmp(szSrcFile, sz) == 0) {
            return true; // already there, just return 
        }
        // add the src file name
        if (nmtEC.addNiForSz(szSrcFile, &ni)) {
            pmodi->ecInfo.niSrcFile = ni;
            return TRUE;
        }
    }
    return false;
}

BOOL DBI1::pdbForImod(unsigned imod, char szPdbFile[ _MAX_PATH ], long* pcb)
{
    MTS_PROTECT(m_csForMods);
    MODI* pmodi;
    if ((pmodi = pmodiForImod((IMOD) imod)) && pmodi->fECEnabled) {
        NI ni = pmodi->niECPdbFile();
        SZ sz = nmtEC.szForNi(ni);
        if (sz) {
            strncpy_s(szPdbFile, _MAX_PATH, sz, _TRUNCATE);
            if (pcb)
                *pcb = CB(strlen(szPdbFile) + 1);
            return TRUE;
        }
    }
    return FALSE;
}

// used in Mod1::fProcessSym()
BOOL DBI1::setPdbForImod(unsigned imod, SZ_CONST szPdbFile)
{
    MTS_PROTECT(m_csForMods);
    MODI* pmodi;
    if ((pmodi = pmodiForImod((IMOD) imod)) && szPdbFile) {
        NI ni;
        SZ sz = 0;
        if (pmodi->niECPdbFile() != niNil) {   // see what name is already there
            ni = pmodi->niECPdbFile();
            sz = nmtEC.szForNi(ni);
        }
        if (sz && strcmp(szPdbFile, sz) == 0) {
            return true; // already there, just return 
        }
        // add the pdb file name
        if (nmtEC.addNiForSz(szPdbFile, &ni)) {
            pmodi->ecInfo.niPdbFile = ni;
            return TRUE;
        }
    }
    return false;
}

BOOL DBI1::fSave()
{
    MTS_PROTECT(m_csForPGSI);
    MTS_PROTECT(m_csSecContribs);
    MTS_PROTECT(m_csForMods);
    MTS_PROTECT(m_csForSymRec);
    MTS_PROTECT(m_csForHdr);
    MTS_PROTECT(m_csForDbg);
    MTS_PROTECT(m_csForFrameData);
    assert(fWrite);
    assert(pgsiGS);
    assert(pgsiPS);

    // output the any deferred udt defns from foreign type servers
    if (
        !pgsiGS->fSave(&(dbihdr.snGSSyms)) ||
        !pgsiPS->fSave(&(dbihdr.snPSSyms)) ||
        !writeSymRecs())
    {
        return FALSE;
    }

    if (pbscEnd) {
        // sort entries in the SC

        size_t cchSC = SizeOfSCEntry();
        qsort(bufSC.Start(), size_t(pbscEnd - bufSC.Start()) / cchSC, cchSC, (fSCv2 ? SCCmp<SC2> : SCCmp<SC>));  // REVIEW:WIN64 CAST
    }


    // record lengths of the gpmodi and sc substreams in the header and then
    // emit the dbi stream
    dbihdr.cbGpModi = bufGpmodi.Size();
    dbihdr.cbSC = CB(pbscEnd - bufSC.Start() + sizeof(DWORD)); // REVIEW:WIN64 CAST
    dbihdr.cbSecMap = bufSecMap.Size();
    expect(fAlign(dbihdr.cbGpModi));
    expect(fAlign(dbihdr.cbSC));
    expect(fAlign(dbihdr.cbSecMap));

    //
    // record EC info
    //
    Buffer bufEC;
    nmtEC.save(&bufEC);
    dbihdr.cbECInfo = bufEC.Size();

    // Convert the file info in the gpmodi into sstFileIndex format
    // and save that!
    Buffer bufFileInfo(0, 0, FALSE);
    if (!QueryFileInfo(0, &dbihdr.cbFileInfo)) {
        return FALSE;
    }
    if (!bufFileInfo.ReserveNoZeroed(dbihdr.cbFileInfo)) {
        ppdb1->setOOMError();
        return FALSE;
    }
    if (!QueryFileInfo(bufFileInfo.Start(), &dbihdr.cbFileInfo)) {
        return FALSE;
    }

    expect(fAlign(sizeof(dbihdr)));
    expect(fAlign(dbihdr.cbFileInfo));

#ifdef PDB_TYPESERVER
    dbihdr.cbTSMap = bufTSMap.RawSize();
#else
    dbihdr.cbTSMap = 0;
#endif

    expect(fAlign(dbihdr.cbTSMap));

    MSF *   pmsf = ppdb1->pmsf;
    
    dbihdr.cbDbgHdr = dbghdr.fInUse() ? CB(sizeof(DbgDataHdr)) : 0;

    // Starting with VC60, every time the DBI stream is updated, 
    // its age becomes equal to the pdb age
    dbihdr.age = ppdb1->QueryAge();
    DWORD versionSC = fSCv2 ? DBISCImpv2 : DBISCImpv;    // current SC version
    dbihdr.verHdr = DBIImpv;        // current dbi type
    dbihdr.SetPdbVersion(rmj, rmm, rup, rbld);
    SetStripped(false);

    if (!pmsf->ReplaceStream(snDbi, &dbihdr, sizeof (dbihdr)) ||
        !fWriteModi(bufGpmodi) || 
        !pmsf->AppendStream(snDbi, &versionSC, sizeof(DWORD)) ||
        !pmsf->AppendStream(snDbi, bufSC.Start(), dbihdr.cbSC-sizeof(DWORD)) ||
        !pmsf->AppendStream(snDbi, bufSecMap.Start(), dbihdr.cbSecMap) ||
        !pmsf->AppendStream(snDbi, bufFileInfo.Start(), dbihdr.cbFileInfo) ||
#ifdef PDB_TYPESERVER
        !pmsf->AppendStream(snDbi, bufTSMap.RawStart(), dbihdr.cbTSMap) ||
#endif
        !pmsf->AppendStream(snDbi, bufEC.Start(), dbihdr.cbECInfo)
       ){
        ppdb1->setWriteError();
        return FALSE;
    }

    if (dbihdr.cbDbgHdr) {
        if (!pmsf->AppendStream(snDbi, &dbghdr, dbihdr.cbDbgHdr)) {
            ppdb1->setWriteError();
            return FALSE;
        }
    }

    if (pdbgFrameData) {
        pdbgFrameData->Close();
    }

    return TRUE;
}

INTV DBI1::QueryInterfaceVersion()
{
    return intv;
}

IMPV DBI1::QueryImplementationVersion()
{
    return impv;
}

void DBI1::NoteModCloseForImod(IMOD imod)
{
    MTS_ASSERT(m_csForMods);
    assert(0 <= imod && imod < imodMac);

    MODI *pmodi = rgpmodi[imod];
    pmodi->pmod = NULL;
}

IMOD DBI1::imodForModNameW(const wchar_t* szModule, const wchar_t* szObjFile)
{
    MTS_ASSERT(m_csForMods);

    if (imodMac == 0)
        return imodNil;

    if (imodMac > rgModNames.size()) {
        for (IMOD imod = rgModNames.size(); imod < imodMac; ++imod) {
            ModNames modNames;
            MODI* pmodi = rgpmodi[imod];
            const char* szMod = pmodi->szModule();
            PB  pb;
            size_t cchMod = UnicodeLengthOfUTF8(szMod);
            if (!bufNames.Reserve(static_cast<CB>(cchMod * sizeof(wchar_t)), &pb)) {
                ppdb1->setOOMError();
                return imodNil;
            }
            modNames.offwszName = pb - bufNames.Start();
            _GetSZUnicodeFromSZUTF8(szMod, reinterpret_cast<wchar_t*>(pb), cchMod);
            const char* szObj = pmodi->szObjFile();
            size_t cchObj = UnicodeLengthOfUTF8(szObj);
            if (!bufNames.Reserve(static_cast<CB>(cchObj * sizeof(wchar_t)), &pb)) {
                ppdb1->setOOMError();
                return imodNil;
            }
            modNames.offwszFile = pb - bufNames.Start();
            _GetSZUnicodeFromSZUTF8(szObj, reinterpret_cast<wchar_t*>(pb), cchObj);
            if (!rgModNames.append(modNames)) {
                return imodNil;
            }
        }
    }

#ifdef PDB_MT
    IMOD imodLastFMN = 0;
#endif
    // performance heuristic: search for module starting from last search
    // index, rather than from 0.
    if (imodLastFMN >= imodMac)
        imodLastFMN = 0;
    IMOD imod = imodLastFMN;
    do {
        assert(0 <= imod && imod < imodMac);
        ModNames* pmodNames = &rgModNames[ imod ];
        PB  pbBase = bufNames.Start();

        if (_wcsicmp(reinterpret_cast<wchar_t*>(pbBase + pmodNames->offwszName), szModule) == 0 &&
            (!szObjFile || _wcsicmp(reinterpret_cast<wchar_t*>(pbBase + pmodNames->offwszFile), szObjFile) == 0))
            return imodLastFMN = imod;
        imod = (imod + 1) % imodMac;
    } while (imod != imodLastFMN);
    return imodNil;
}

BOOL DBI1::openModByImod(IMOD imod, OUT Mod** ppmod)
{
    MTS_PROTECT(m_csForMods);

    if (imod == imodNil || imod >= imodMac)
        return FALSE;
    MODI* pmodi = pmodiForImod(imod);
    if (!pmodi->pmod) {
        // module is not yet "open"...open it
        Mod1* pmod_;
        if (!(pmod_ = new (ppdb1) Mod1(ppdb1, this, imod)))
            return FALSE;

        if (!(pmod_->fInit())) {
            delete pmod_;
            return FALSE;
        }

        pmodi->pmod = (Mod*) pmod_;
    }
    else {
#ifndef PDB_MT
        // must not reopen a module if writing
        if (fWrite) {
            return FALSE;
        }
#endif
    }

    *ppmod = pmodi->pmod;
    return TRUE;
}

BOOL DBI1::DeleteMod(SZ_CONST szModule)
{
    dassert(szModule);
#pragma message("TODO return FALSE when implemented")
    return TRUE;
}

#if 0 // NYI
BOOL DBI1::QueryCountMod(long *pcMod)
{
    assert(pcMod);
    *pcMod = imodMac;
    return TRUE;
}
#endif

BOOL DBI1::QueryNextMod(Mod* pmod, Mod** ppmodNext)
{
    MODI* pmodi;

    IMOD imod = imodNil;

    MTS_PROTECT(m_csForMods);

    // establish imod to be the imod of pmod
    if (pmod) {
#ifndef PDB_MT
        if (imodLastQNM != imodNil &&
            !!(pmodi = pmodiForImod(imodLastQNM)) && pmodi->pmod == pmod) {
            // cache hit
            imod = imodLastQNM;
        }
        else 
#endif
        {
            // cache miss, search MODI table for it
            for (imod = 0; imod < imodMac; imod++)
                if (!!(pmodi = pmodiForImod(imod)) && pmodi->pmod == pmod)
                    break;
            if (imod >= imodMac) {
                ppdb1->setUsageError();
                return FALSE;
            }
        }
    }
    // at this point, imod address the previous modi, or -1.

    // advance to the next modi, if any
    if (++imod < imodMac) {
        if (!openModByImod(imod, ppmodNext))
            return FALSE;
#ifndef PDB_MT
        imodLastQNM = imod; // update cache
#endif
    }
    else {
        // no more modules; return success but no symbol
        *ppmodNext = 0;
    }
    return TRUE;
}

BOOL DBI1::QueryMods(Mod **ppmods, long cMods)
{
    IMOD    imod;
    for (imod = 0; imod < imodMac && imod < cMods; imod++) {
        if (!openModByImod(imod, &(ppmods[imod]))){
            break;
        }
    }

    return imod >= imodMac || imod > cMods;
}

BOOL DBI1::OpenGlobals(OUT GSI** ppgsi)
{
    dassert (ppgsi);

    MTS_PROTECT(m_csForPGSI);

    if (pgsiGS) {
#ifdef PDB_MT
        if (fWrite) { 
            // can't open twice for write
            return FALSE;
        }
        // already opened - just return
        pgsiGS->refcount++;
#endif
        *ppgsi = pgsiGS;
        return TRUE;
    }

    
    GSI1* pgsi = new GSI1(ppdb1, this);

    if (!pgsi) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (pgsi->fInit(dbihdr.snGSSyms)) {
        *ppgsi = pgsiGS = pgsi;
        return TRUE;
    }
    pgsi->Close();
    return FALSE;
}

BOOL DBI1::OpenPublics(OUT GSI** ppgsi)
{
    dassert (ppgsi);

    MTS_PROTECT(m_csForPGSI);

    if (pgsiPS) {
#ifdef PDB_MT
        if (fWrite) { 
            // can't open twice for write
            return FALSE;
        }
        // already opened - just return
        pgsiPS->refcount++;
#endif
        *ppgsi = pgsiPS;
        return TRUE;
    }


    PSGSI1* ppsgsi = new PSGSI1(ppdb1, this, fWrite);

    if (!ppsgsi) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (ppsgsi->fInit(dbihdr.snPSSyms)) {
        *ppgsi = pgsiPS = ppsgsi;
        return TRUE;
    }
    else {
        ppsgsi->Close();
        return FALSE;
    }
}

BOOL DBI1::AddSec(ISECT isect, USHORT flags, OFF off, CB cb)
{
    // MTS NOTE: fWrite and ppdb1 are not change since the constructor
    if (!fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    OMFSegMapDesc* pOMFSegMapDesc;
    if (!bufSecMap.Reserve(sizeof(OMFSegMapDesc), (PB*) &pOMFSegMapDesc)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    pOMFSegMapDesc->flags.fAll = flags;
    pOMFSegMapDesc->ovl = 0;
    pOMFSegMapDesc->group = 0;
    pOMFSegMapDesc->frame = isect;
    pOMFSegMapDesc->iSegName = 0xffff;
    pOMFSegMapDesc->iClassName = 0xffff;
    pOMFSegMapDesc->offset = off;
    pOMFSegMapDesc->cbSeg = cb;

    OMFSegMap* pOMFSegMap = (OMFSegMap*)bufSecMap.Start();
    dassert(pOMFSegMap);
    pOMFSegMap->cSeg++;
    pOMFSegMap->cSegLog++;

    return TRUE;
}

BOOL DBI1::AddPublic(SZ_CONST szPublic, ISECT isect, OFF off)
{
    return AddPublic2(szPublic, isect, off, cvpsfNone);
}

BOOL DBI1::AddPublic2(SZ_CONST szPublic, ISECT isect, OFF off, CV_pubsymflag_t cvpsf)
{
    if (!fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }
    // Should be ASCII szPublic, same as UTF8
    MP  mp(szPublic, isect, off, cvpsf);
    return packSymToPS(PSYM(&mp));
}

BOOL DBI1::RemovePublic(SZ_CONST szPublic)
{
    if (!fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }
    assert(pgsiPS);
    return pgsiPS->removeSym(szPublic);
}

USHORT DBI1::QueryMachineType() const
{
    MTS_PROTECT(m_csForHdr);
    return dbihdr.wMachine;
}

void DBI1::SetMachineType(USHORT wMachine)
{
    MTS_PROTECT(m_csForHdr);
    dbihdr.wMachine = wMachine;
}


BOOL DBI1::QuerySecMap(OUT PB pb, CB* pcb)
{
    // if in write mode, it's always exclusive
    // if in read mod, bufSecMap doesn't change

    dassert(pcb);

    if (bufSecMap.Size() == 0) {
        *pcb = 0;
        return TRUE;
    }

    if (pb) {
        if (*pcb < bufSecMap.Size()) {
            return FALSE;
        }

        memcpy (pb, bufSecMap.Start(), bufSecMap.Size());
    }

    *pcb = bufSecMap.Size();

    return TRUE;
}

BOOL DBI1::getEnumContrib(Enum** ppenum)
{
    MTS_PROTECT(m_csSecContribs);

    *ppenum = NULL;

    PB pb;
    BOOL noerror = getSecContribs(&pb);

    if (fSCv2) {
        return FALSE;
    }

    noerror = noerror && (pb != NULL) && ((*ppenum = (Enum *)new EnumSC<SC>(bufSC)) != NULL);
    return noerror;
}

BOOL DBI1::getEnumContrib2(Enum** ppenum)
{
    MTS_PROTECT(m_csSecContribs);

    *ppenum = NULL;

    PB pb;
    BOOL noerror = getSecContribs(&pb);

    if (!fSCv2) {
        return FALSE;
    }

    noerror = noerror && (pb != NULL) && ((*ppenum = (Enum *)new EnumSC<SC2>(bufSC)) != NULL);
    return noerror;
}


// DDB 173223: A zero will only stand for so much. Function now
// returns TRUE on success. On an error the setXXXError is called,
// FALSE is returned, and ppbStart is set to NULL.
// A stream that is totally empty or contains just a version field
// is not considered an error so TRUE is returned, but ppbStart will
// be set to NULL.
BOOL DBI1::getSecContribs(PB *ppbStart)
{
    if (ppbStart) {
        *ppbStart = NULL;
    }

    MTS_ASSERT(m_csSecContribs);

    if (!bufSC.Start() && dbihdr.cbSC > 0) {
        // read in the Section Contribution substream
        expect(fAlign(dbihdr.cbSC));

        MSF* pmsf = ppdb1->pmsf;
        OFF off = offSC;
        DWORD SCversion;
        CB cb = sizeof(SCversion);

        if (!pmsf->ReadStream(snDbi, off, &SCversion, &cb)) {
            ppdb1->setReadError();
            return FALSE;
        }

        if (SCversion != DBISCImpv && SCversion != DBISCImpv2) {
            // Convert VC++ 4.0 SC entries to current format

            fSCv2 = false;

            unsigned csc = dbihdr.cbSC / sizeof(SC40);

            cb = csc * sizeof(SC);

            if (!bufSC.ReserveNoZeroed(cb)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            SC *psc = (SC *) (bufSC.Start());
            cb = sizeof(SC40);

            while (csc--) {
                SC40 sc40;

                if (!pmsf->ReadStream(snDbi, off, &sc40, &cb)) {
                    ppdb1->setReadError();
                    return FALSE;
                }

                *psc = sc40;

                off += sizeof(sc40);
                psc++;
            }

            pbscEnd = (PB) psc;
        }
        else {
            fSCv2 = SCversion == DBISCImpv2;

            off += cb;  // skip over the version field
            cb = dbihdr.cbSC - cb;

            if (!bufSC.ReserveNoZeroed(cb)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            if (!pmsf->ReadStream(snDbi, off, bufSC.Start(), &cb)) {
                ppdb1->setReadError();
                return FALSE;
            }

            pbscEnd = bufSC.End();
        }
    }

    if (ppbStart) {
        *ppbStart = bufSC.Start();
    }

    return TRUE;
}

BOOL DBI1::QueryModFromAddr(ISECT isect, OFF off, OUT Mod** ppmod,
    OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb)
{
    return QueryModFromAddr2(isect, off, ppmod, pisect, poff, pcb, NULL);
}

BOOL DBI1::QueryModFromAddr2(
    ISECT isect,
    OFF off, 
    OUT Mod** ppmod,
    OUT ISECT* pisect,
    OUT OFF* poff,
    OUT CB* pcb,
    OUT ULONG * pdwCharacteristics)
{
    MTS_PROTECT(m_csSecContribs);

    IMOD imod;
    if (QueryImodFromAddr(isect, off, &imod, pisect, poff, pcb, pdwCharacteristics)) {
        return OpenModFromImod(imod, ppmod);
    }
    return FALSE;
}

BOOL DBI1::QueryModFromAddrEx(
    ISECT isect,
    DWORD off, 
    OUT Mod** ppmod,
    OUT ISECT* pisect,
    OUT ULONG* pisectCoff,
    OUT ULONG* poff,
    OUT ULONG* pcb,
    OUT ULONG * pdwCharacteristics)
{
    MTS_PROTECT(m_csSecContribs);

    IMOD imod;
    if (QueryImodFromAddrEx(isect, off, &imod, pisect, pisectCoff, poff, pcb, pdwCharacteristics)) {
        return OpenModFromImod(imod, ppmod);
    }
    return FALSE;
}

template<typename T>
BOOL DBI1::QueryImodFromAddrHelper(
    ISECT isect,
    ULONG off, 
    OUT IMOD*  pimod,
    OUT ISECT* pisect,
    OUT ULONG* pisectCoff,
    OUT ULONG* poff,
    OUT ULONG* pcb,
    OUT ULONG* pdwCharacteristics)
{
    assert(sizeof(T) == sizeof(SC) || sizeof(T) == sizeof(SC2));
    assert(pimod != NULL);

    *pimod = -1;

    MTS_PROTECT(m_csSecContribs);

    PB  pbscLo;
    getSecContribs(&pbscLo);

    T* psc = NULL;

    // If no SCs or if there was an error reading them, skip around and return FALSE.

    if (pbscLo != NULL) {
        if (pbscEnd - pbscLo > 0) {
            T      key;

            key.isect = isect;
            key.off   = off;
            psc = reinterpret_cast<T *>(bsearch(&key, pbscLo, (pbscEnd - pbscLo) / sizeof(T), sizeof(T),
                                        sizeof(T) == sizeof(SC) ? SC::compareSC : SC2::compareSC));
        }

        if (psc != NULL) {
            // we found it

            *pimod = ximodForImod(psc->imod);

            if (pisect) {
                *pisect = psc->isect;
            }
            if (pisectCoff) {
                assert(sizeof(T) == sizeof(SC2));
                *pisectCoff = (reinterpret_cast<SC2 *>(psc))->isectCoff;
            }
            if (poff) {
                *poff = psc->off;
            }
            if (pcb) {
                *pcb = psc->cb;
            }
            if (pdwCharacteristics) {
                *pdwCharacteristics = psc->dwCharacteristics;
            }
        }
    }

    return psc != NULL;
}

BOOL DBI1::QueryImodFromAddr(
    ISECT isect,
    OFF off, 
    OUT IMOD *pimod,
    OUT ISECT* pisect,
    OUT OFF* poff,
    OUT CB* pcb,
    OUT ULONG * pdwCharacteristics)
{
    return QueryImodFromAddrHelper<SC>(isect, off, pimod, pisect, NULL, (ULONG *) poff, (ULONG *) pcb, pdwCharacteristics);
}

BOOL DBI1::QueryImodFromAddrEx(
    USHORT isect,
    ULONG off,
    OUT IMOD* pimod,
    OUT USHORT* pisect,
    OUT ULONG* pisectCoff,
    OUT ULONG* poff,
    OUT ULONG* pcb,
    OUT ULONG * pdwCharacteristics)
{
    return QueryImodFromAddrHelper<SC2>(isect, off, pimod, pisect, pisectCoff, poff, pcb, pdwCharacteristics);
}

BOOL DBI1::fCreateSecToSCMap(Map<IModSec, DWORD, hcSC> & map, IMOD imod, bool fZeroChar)
{
    IMOD imodInt = imodForXimod(imod);
    EnumMap<IModSec, DWORD, hcSC> enm(map);

    if (enm.next()) {
        IModSec secPrev;
        DWORD dwPrev;

        enm.get(&secPrev, &dwPrev);

        if (imodInt == secPrev.imod) {
            return TRUE;
        }
        map.reset();
    }

    // Construct the map for faster search

    PB pbscStart; 
    if ((getSecContribs(&pbscStart) == FALSE) || (pbscStart == NULL)) {
        return FALSE;
    }

    for (PB pbsc = pbscStart; pbsc != pbscEnd; pbsc += SizeOfSCEntry()) {
        SC *psc = reinterpret_cast<SC *>(pbsc);

        if (psc->imod == imodInt) {
            IModSec sec = { psc->imod, 
                            psc->dwDataCrc, 
                            psc->dwRelocCrc, 
                            psc->cb,
                            fZeroChar ? 0 : psc->dwCharacteristics};

            map.add(sec, DWORD(pbsc - pbscStart) / SizeOfSCEntry());
        }
    }

    return TRUE;
}

BOOL DBI1::QueryAddrForSecEx(OUT USHORT* pisect, OUT long* poff, IMOD imod,
            long cb, DWORD dwDataCrc, DWORD dwRelocCrc, DWORD dwCharacteristics)
{
    assert(!fWrite);
    if (fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(m_csSecContribs);

    if (!fCreateSecToSCMap(mpSecToSC, imod, false)) {
        return FALSE;
    }

    IModSec sec = { imodForXimod(imod), dwDataCrc, dwRelocCrc, cb, dwCharacteristics };
    DWORD iSC;

    if (mpSecToSC.map(sec, &iSC)) {
        PB  pbsc;

        if ((getSecContribs(&pbsc) == FALSE) || (pbsc == NULL)) {
            return FALSE;
        }

        SC *psc = reinterpret_cast<SC *>(pbsc + iSC * SizeOfSCEntry());
        assert(psc->Match(imodForXimod(imod), cb, dwDataCrc, dwRelocCrc, dwCharacteristics));

        if (pisect) {
            *pisect = psc->isect;
        }
        if (poff) {
            *poff = psc->off;
        }

        return TRUE;
    }

    return FALSE;
}

BOOL DBI1::QueryAddrForSec(OUT USHORT* pisect, OUT long* poff, 
            IMOD imod, long cb, DWORD dwDataCrc, DWORD dwRelocCrc)
{
    assert(!fWrite);
    if (fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }

    MTS_PROTECT(m_csSecContribs);

    if (!fCreateSecToSCMap(mpSecToSC_ZeroChar, imod, true)) {
        return FALSE;
    }

    IModSec sec = { imodForXimod(imod), dwDataCrc, dwRelocCrc, cb, 0 };
    DWORD iSC;

    if (mpSecToSC.map(sec, &iSC)) {
        PB pbsc;

        if ((getSecContribs(&pbsc) == FALSE) || (pbsc == NULL)) {
            return FALSE;
        }

        SC *psc = reinterpret_cast<SC *>(pbsc + iSC * SizeOfSCEntry());
        assert(psc->Match(imodForXimod(imod), cb, dwDataCrc, dwRelocCrc));

        if (pisect) {
            *pisect = psc->isect;
        }
        if (poff) {
            *poff = psc->off;
        }

        return TRUE;
    }

    return FALSE;
}

BOOL DBI1::Close()
{
    if (fWrite) {
        // We also need to close any PDB files which have been opened for read.
        // The info of these PDB files are saved in potmTSHead and potmPCTHead,
        // the destructions of which will happen in DBI1::internal_Close().
        // So we disregard fSave()'s return value, which could be FALSE sometime
        // due to PDB corruption.
        fSave();
    }

    if (ppdb1->internal_CloseDBI()) {
        ReportTypeMismatches();
        return internal_Close();
    }

    assert(!fWrite);        // exclusive write!
    return TRUE;
}

BOOL DBI1::internal_Close() {
    {
        MTS_PROTECT(m_csForPGSI);

        if (pgsiGS)
            pgsiGS->Close();
        if (pgsiPS)
            pgsiPS->Close();
    }
    delete this;
    return TRUE;
}

DBI1::~DBI1()
{
#if defined(INSTRUMENTED)
    if (log) {
        LogNoteEvent(log, "DBI", 0, letypeEvent,
                     "cModules:%d cSymbols:%d cTypesMapped:%d",
                     info.cModules, info.cSymbols, info.cTypesMapped);
        LogNoteEvent(log, "DBI", 0, letypeEvent,
                     "cTypesMappedRec.:%d cTypesQueried:%d cTypesAdded:%d",
                     info.cTypesMappedRecursively, info.cTypesQueried, info.cTypesAdded);
        LogNoteEvent(log, "DBI", 0, letypeEvent, "cTMTS:%d cTMR:%d cTMPCT:%d",
                     info.cTMTS, info.cTMR, info.cTMPCT);
        LogNoteEvent(log, "DBI", 0, letypeEnd, 0);
        LogClose(log);
    }
#endif

#ifdef INSTRUMENTED
    DumpSymbolPages();
#endif

    if (potmTSHead)
        delete potmTSHead;
    if (potmPCTHead)
        delete potmPCTHead;

    {
        MTS_PROTECT(m_csForMods);           // just for pmodiForImod
        // dtor pmodi's
        for (IMOD imod = 0; imod < imodMac; imod++) {
            MODI* pmodi = pmodiForImod(imod);
            if (pmodi) {
                pmodi->~MODI();
            }
        }
    }


    if (pbvSymRecPgs) {
        delete pbvSymRecPgs;
    }

    if (pwti) {
        pwti->release();
        pwti = NULL;
    }

#ifdef PDB_MT
    if (ptpi) {
        ptpi->Close();
    }

    if (pipi) {
        pipi->Close();
    }
#endif

    unsigned isrcmapMax = m_rgsrcmap.size();
    for (unsigned isrcmap = 0; isrcmap < isrcmapMax; isrcmap++) {
        free(m_rgsrcmap[isrcmap].szFrom);
        free(m_rgsrcmap[isrcmap].szTo);
    }

#ifdef PDB_TYPESERVER
    ClosePdbs();
#endif

#if CC_LAZYSYMS
    ObjectFile::Flush();
#endif
}

wchar_t * DBI1::szGetTsInfo(PTYPE pts, wchar_t *szNameBuf, wchar_t *szFullMapped, SIG70 *pSig70, AGE *pAge)
{
    dassert(pts != NULL && szNameBuf != NULL && szFullMapped != NULL && pSig70 != NULL && pAge != NULL);
    dassert(pts->leaf == LF_TYPESERVER_ST || pts->leaf == LF_TYPESERVER || pts->leaf == LF_TYPESERVER2);

    *pSig70 = SIG70FromTSRecord(pts);
    *pAge = AgeFromTSRecord(pts);
    wchar_t *szTSName = SZNameFromTSRecord(pts, szNameBuf, pts->len);

    if (szTSName == NULL) {
        ppdb1->setOOMError();
        return NULL;
    }

    wchar_t *szMappedName = ppdb1->QueryPDBMapping(szTSName);

    if (szMappedName != NULL) {
        if (_wfullpath(szFullMapped, szMappedName, _MAX_PATH)) {
            szTSName = szFullMapped;
        }
        else {
            ppdb1->setLastError(EC_FILE_SYSTEM, szMappedName);
            return NULL;
        }
    }

    return szTSName;
}

// Get a TMTS for the TypeServer PDB referenced by the lfTypeServer record.
// Return this TMTS in *ptm, except (*ptm == 0) when the referenced PDB
// corresponds to the project (output) PDB.  Return TRUE if successful.
//
// Note that subsequent calls upon fGetTmts, for the same PDB, from subsequent
// modules in this DBI, will return the same TMTS.
//

// Called from Mod1->AddTypes (mod.cpp), UTF8 string is provided
BOOL DBI1::fGetTmts(PTYPE pts, UTFSZ_CONST szObjFile, TM** pptm, BOOL fQueryOnly)
{
    MTS_PROTECT(m_csForOTM);

    dassert(pts && pptm);

    // Get info on compiler generated PDB.

    SafeStackAllocator<0x210> allocator;

    wchar_t *szT = allocator.Alloc<wchar_t>(pts->len);
    wchar_t *szFullMapped = allocator.Alloc<wchar_t>(_MAX_PATH);

    if (NULL == szT || NULL == szFullMapped) {
        ppdb1->setOOMError();
        return FALSE;
    }

    SIG70  sig70;
    AGE    age;
    const wchar_t * szTSName = szGetTsInfo(pts, szT, szFullMapped, &sig70, &age);
    
    if (szTSName == NULL) {
        return FALSE;
    }

    // Consult the open TMTS list to determine if an existing TMTS matches the
    // referenced PDB.

    if (fFindTm(potmTSHead, szTSName, sig70, age, tiNil, pptm)) {
        return TRUE;
    }

    if (fQueryOnly) {
        return FALSE;
    }

    // open a TMTS
    AGE agePdb;

    if (!fOpenTmts(szTSName, sig70, age, szObjFile, pptm, agePdb)) {
        return FALSE;
    }

    // add this TMTS the open TMTS list
    wchar_t *szName = wszCopy(szTSName);

    if ((szName == NULL) ||
        !(potmTSHead = new (ppdb1) OTM(potmTSHead, szName, sig70, agePdb, tiNil, *pptm)))
    {
        ppdb1->setOOMError();
        delete pptm;
        *pptm = NULL;
        return FALSE;
    }

    return TRUE;
}

static bool fIsFullPath(const wchar_t * szName) {
    if (szName &&
        ((szName[0] >= L'A' && szName[0] <= L'Z') || (szName[0] >= L'a' && szName[0] <= L'z')) &&
        szName[1] == L':' &&
        (szName[2] == L'\\' || szName[2] == L'/'))
    {
        return true;
    }

    if (szName &&
        szName[0] == L'\\' &&
        szName[1] == L'\\')
    {
        return true;
    }

    return false;
}

// Open the PDB referenced by *pts.
// Set *ppdb and return TRUE if successful, FALSE otherwise.
BOOL DBI1::fOpenPdb(const SIG70& sig70, AGE age, const wchar_t *szName, UTFSZ_CONST utfszObjFile,
       PDB **pppdb, BOOL fQuery)
{
    USES_STACKBUFFER(0x400);

    *pppdb = 0; // 0 means use 'to' PDB

    assert(fIsFullPath(szName));

    wchar_t szPDBTo[_MAX_PATH];
    ppdb1->QueryPDBNameExW(szPDBTo, _MAX_PATH);

    if (_wcsnicmp(szPDBTo, szName, _MAX_PATH) == 0) {
        // PDB filenames match, reference to the 'to' PDB
        if (!IsEqualSig(sig70, ppdb1))  {
            ppdb1->setLastError(EC_INVALID_SIG, szName);
            return FALSE;
        }

        if (age > ppdb1->QueryAge()) {
            ppdb1->setLastError(EC_INVALID_AGE, szName);
            return FALSE;
        }

        return TRUE;
    }

    wchar_t szPDBFrom[_MAX_PATH];
    wcsncpy_s(szPDBFrom, _countof(szPDBFrom), szName, _TRUNCATE);

    if (IsEqualSig(sig70, ppdb1) && age <= ppdb1->QueryAge()) {
        // PDB signature and age match; this 'from' PDB must contain equivalent
        // information (even if it is a copy on some other path).  However, we
        // may have the highly unlikely case of distinct PDBs with equal
        // signatures; to feel better about this case, we won't conclude
        // equivalence unless the PDB base names also match.  In practice this
        // should be exactly conservative enough to avoid false positives and
        // yet prevent accidental reopening of the 'to' PDB.

        wchar_t* pchBaseFrom = wcsrchr(szPDBFrom, '\\');
        wchar_t* pchBaseTo = wcsrchr(szPDBTo, '\\');

        if (_wcsicmp(pchBaseFrom, pchBaseTo) == 0) {
            // even the base names match; reference to the 'to' PDB
            return TRUE;
        }
    }

    wchar_t *szObjFile = GetSZUnicodeFromSZUTF8(utfszObjFile);

    if (szObjFile == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    assert(fIsFullPath(szObjFile));

    // Alas, probably a reference to a different type server.  Open it.
    EC ec;
    wchar_t szError[cbErrMax];
    wchar_t szObjDrive[_MAX_DRIVE+_MAX_PATH];
    wchar_t szObjDir[_MAX_PATH];

    // Get the drive and directory of the referencing OBJ or LIB
    szObjDrive[0] = szObjDir[0] = L'\0';
    _wsplitpath_s(szObjFile, szObjDrive, _countof(szObjDrive), szObjDir, _countof(szObjDir), NULL, 0, NULL, 0);

    assert((szObjDrive[0] != 0 && szObjDrive[1] != 0) || (szObjDir[0] == '\\' && szObjDir[1] == '\\'));
    assert(szObjDir[0] != 0);

    wcscat_s(szObjDrive, _countof(szObjDrive), szObjDir);

    wchar_t *szObjOrLibPath = szObjDrive;

    if (PDB1::OpenTSPdb(
            szPDBFrom,
            szObjOrLibPath,
            ppdb1->m_fFullBuild ? (pdbRead pdbGetRecordsOnly pdbFullBuild) : (pdbRead pdbGetRecordsOnly),
            sig70,
            age,
            &ec,
            szError,
            cbErrMax,
            pppdb))
    {
        // do nothing; fall through
    }
    else {
        // Try looking in the same directory as the PDB we are writing to

        wchar_t szPdbFromFile[_MAX_FNAME];
        wchar_t szPdbFromExt[_MAX_EXT];
        wchar_t szPdbToDir[_MAX_DIR];
        wchar_t szPdbToDrive[_MAX_DRIVE];
        wchar_t szPdbFromWhereSzPdbToIs[PDB_MAX_FILENAME];

        _wsplitpath_s(szPDBTo, szPdbToDrive, _countof(szPdbToDrive), szPdbToDir, _countof(szPdbToDir), NULL, 0, NULL, 0);
        _wsplitpath_s(szPDBFrom, NULL, 0, NULL, 0, szPdbFromFile, _countof(szPdbFromFile), szPdbFromExt, _countof(szPdbFromExt));
        _wmakepath_s(szPdbFromWhereSzPdbToIs, _countof(szPdbFromWhereSzPdbToIs), szPdbToDrive, szPdbToDir, szPdbFromFile, szPdbFromExt);

        if (PDB1::OpenTSPdb(
                szPdbFromWhereSzPdbToIs,
                NULL,
                ppdb1->m_fFullBuild ? (pdbRead pdbGetRecordsOnly pdbFullBuild) : (pdbRead pdbGetRecordsOnly),
                sig70,
                age,
                &ec,
                szError,
                cbErrMax,
                pppdb))
        {
            // do nothing; fall through
        }
        else {
            ppdb1->setLastError(ec, szError);
            return FALSE;
        }
    }

    // Check again that the PDB we found along the lib path is the same as the target PDB.
    (*pppdb)->QueryPDBNameExW(szPDBFrom, _MAX_PATH);

    if (_wcsicmp(szPDBTo, szPDBFrom) == 0) {
        // PDB filenames match, reference to the 'to' PDB
        (*pppdb)->Close();
        *pppdb = 0;     // indicate use of 'to' pdb
    }

    return TRUE;
}


// Open the TypeServer referenced by *pts and initialize a TMTS from it.
// Set *ptm and return TRUE if successful, FALSE otherwise.
//
BOOL DBI1::fOpenTmts(const wchar_t* wszName, const SIG70& sig70, AGE age, UTFSZ_CONST szObjFile, TM** pptm, AGE &agePdb)
{
    MTS_ASSERT(m_csForOTM);

    BOOL    ftmeqts = FALSE;
    PDB*    ppdb = 0;
    BOOL    fRet = FALSE;

    *pptm = 0; // 0 means use 'to' PDB

    TPI * ptpi = GetTpi();
    if (ptpi == NULL) {
        goto errorOut;
    }

    TPI * pipi = GetIpi();
    if (pipi == NULL) {
        // Do nothing.  Old versioned PDB doesn't have ID stream.
    }

    if (fOpenPdb(sig70, age, wszName, szObjFile, &ppdb)) {
        if (ppdb == 0) {
            // need an equivalent TM to handle the UDT refs.
            
            TMEQTS *    ptmeqts = new TMEQTS(ppdb1, this, ptpi, pipi);

            ftmeqts = TRUE;
            if (!ptmeqts) {
                ppdb1->setOOMError();
                goto errorOut;
            }
            if (!ptmeqts->fInit()) {
                goto errorOut;
            }
            agePdb = ppdb1->QueryAge();
            *pptm = ptmeqts;
        }
        else {
            // Create and initialize the TMTS.
            TMTS* ptmts = new TMTS(ppdb1, this, ptpi, pipi);
            if (!ptmts) {
                ppdb1->setOOMError();
                goto errorOut;
            }
            if (!ptmts->fInit(ppdb)) {
                goto errorOut;
            }
            agePdb = ppdb->QueryAge();
            *pptm = ptmts;
        }
        fRet = TRUE;
    }

errorOut:
    if (m_dbicbc.pfnNotePdbUsed != NULL) {
        wchar_t         szPdbFilename[512];
        const wchar_t * sz = wszName;

        if (ppdb != NULL) {
            sz = szPdbFilename;
            ppdb->QueryPDBNameExW(szPdbFilename, _countof(szPdbFilename));
        }
        (*m_dbicbc.pfnNotePdbUsed)(m_dbicbc.pvContext, sz, TRUE, ftmeqts);
    }

    if (!fRet && ppdb != NULL) {
        ppdb->Close();
    }

    return fRet;
}


BOOL DBI1::fGetTmpct(PTYPE ppc, TMPCT** pptmpct)
{
    MTS_PROTECT(m_csForOTM);

    dassert(fWrite || ppdb1->FLazy() || ppdb1->FMinimal());
    dassert(ppc && pptmpct);
    dassert(ppc->leaf == LF_PRECOMP || ppc->leaf == LF_PRECOMP_ST);

    SafeStackAllocator<0x180>   allocator;

    // Consult the open TMPCT list to determine which existing TMPCT corresponds
    // to the given module name and signature.
    lfPreComp * plf = (lfPreComp*)&(ppc->leaf);
    wchar_t *   szT = allocator.Alloc<wchar_t>(ppc->len);

    if (NULL != szT) {
        const wchar_t * szTSName = SZNameFromTSRecord(ppc, szT, ppc->len);
        TI              tiPreComp = plf->start + plf->count;

        if (szTSName != NULL) {
            return fFindTm(potmPCTHead, szTSName, Sig70FromSig(plf->signature), AGE(0), tiPreComp, (TM**)pptmpct);
        }
    }
    ppdb1->setOOMError();
    return FALSE;
}

// used by TMR::fInit, from Mod1::AddTypes
BOOL DBI1::fAddTmpct(lfEndPreComp* pepc, TI tiPreComp, UTFSZ_CONST utfszModule, TMPCT* ptmpct)
{
    MTS_PROTECT(m_csForOTM);

    dassert(fWrite || ppdb1->FLazy() || ppdb1->FMinimal());

    // we use the module name as a place holder; when processing the symbols
    // for this PCT, we will update it with the actual name from the S_OBJNAME
    // symbol record

    wchar_t *szLocal = new wchar_t[_MAX_PATH];

    if (szLocal != NULL) {
        _GetSZUnicodeFromSZUTF8(utfszModule, szLocal, _MAX_PATH);
    }

    if (szLocal == NULL ||
        !(potmPCTHead = new OTM(potmPCTHead, szLocal, Sig70FromSig(pepc->signature), AGE(0), tiPreComp, (TM*)ptmpct)))
    {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

// used by MOD1::fProcessSym to update the name to the internal name
BOOL DBI1::fUpdateTmpct(UTFSZ_CONST utfszModule, UTFSZ_CONST utfszInternalName, TM* ptm)
{
    MTS_PROTECT(m_csForOTM);

    dassert(fWrite || ppdb1->FLazy() || ppdb1->FMinimal());

    if (strcmp(utfszModule, utfszInternalName) == 0) {
        return TRUE;
    }

    USES_STACKBUFFER(0x400);

    wchar_t *wszModule = GetSZUnicodeFromSZUTF8(utfszModule);
    wchar_t *wszIntName = GetSZUnicodeFromSZUTF8(utfszInternalName);

    if (wszModule == NULL ||
        wszIntName == NULL ||
        utfszInternalName == NULL)
    {
        ppdb1->setOOMError();
        return FALSE;
    }

    // find the potm that matches with the external filename, signature and tiPreComp
    OTM *   potm = potmPCTHead;
    TMR *   ptmr = reinterpret_cast<TMR*>(ptm);
    SIG70   sig70Match = Sig70FromSig(ptmr->Sig());
    
    assert(ptmr->IsTmpctOwner());
    assert(ptmr->Ptmpct() != NULL);
    assert(ptmr->Ptmpct()->IsTmpct());

    TI  tiPreComp = ptmr->Ptmpct()->TiMac();

    for (; potm; potm = potm->pNext) {
        if (potm->tiPreComp == tiPreComp &&
            IsEqualGUID(potm->sig70, sig70Match) &&
            _wcsicmp(potm->wszName, wszModule) == 0)
        {
            break;
        }
    }

    // add a potm (alias) with the internal name
    if (potm) {
        wchar_t *wsz = wszCopy(wszIntName);

        if (wsz == NULL) {
            ppdb1->setOOMError();
            return FALSE;
        }

        if (!potm->fUpdateName(wsz)) {
            return FALSE;
        }
    }
    
    return TRUE;
}

// Search this OTM and the rest of the OTM list it heads for one which
// matches the name and signature.  If found, set *pptm to the corresponding
// TM and return TRUE, else FALSE.
//
BOOL DBI1::fFindTm(OTM* potm, const wchar_t *szName, const SIG70& sig70, AGE age, TI tiPreComp, TM** pptm)
{
    MTS_ASSERT(m_csForOTM);

    for (; potm; potm = potm->pNext) {
        //
        // Check for equal name and guid and a reasonable age.  The AGE
        // parameter is the age given from the type record and needs to be
        // <= the OTM's cached age of the source type pool.
        // If all is well, we have our type pool.
        //
        if (potm->tiPreComp == tiPreComp &&
            potm->age >= age &&
            IsEqualGUID(potm->sig70, sig70) &&
            _wcsicmp(potm->wszName, szName) == 0)
        {
            if (pptm != NULL) {
                *pptm = potm->ptm;
            }
            return TRUE;
        }
    }
    return FALSE;
}

OTM::OTM(OTM* pNext_, const wchar_t *wszName_, const SIG70& sig70_, AGE age_, TI tiPreComp_, TM* ptm_)
    : pNext(pNext_), wszName(wszName_), ptm(ptm_), sig70(sig70_), age(age_), tiPreComp(tiPreComp_)
{
}

OTM::~OTM()
{
    if (wszName) {
        freeWsz(wszName);
    }

    if (ptm) {
        ptm->endDBI();
    }

    if (pNext) {
        delete pNext;
    }
}

bool OTM::fUpdateName(const wchar_t * wszNewName)
{
    expect(wszName != NULL);
    assert(wszNewName != NULL);

    freeWsz(wszName);
    wszName = wszNewName;

    return true;
}

void DBI1::fixSymRecs (void* pdbi, void* pOld, void* pNew)
{
    DBI1* pdbi1 = (DBI1*)pdbi;
    assert(pdbi1->fWrite);
//    MTS_ASSERT(pdbi1->m_csForSymRec);         // don't need because of fWrite
//    MTS_PROTECT(pdbi1->m_csForPGSI);
    if (pdbi1->pgsiGS)
        pdbi1->pgsiGS->fixSymRecs(pOld, pNew);
    if (pdbi1->pgsiPS)
        pdbi1->pgsiPS->fixSymRecs(pOld, pNew);
}

// only called from DBI1::fReadSymRec
BOOL DBI1::fReadSymRecPage (size_t iPg) {

    MTS_ASSERT(m_csForSymRec);

    if (iPg >= cSymRecPgs) {
        return fValidPsym(PSYM(bufSymRecs.Start() + iPg * cbSysPage));
    }

    assert(iPg < cSymRecPgs);

    // page already read in
    if (pbvSymRecPgs->fTestBit(iPg))
        return TRUE;

    // Calculate the offset for the start of page.
    // We must commit the virtual memory for this page if bufSymRecs is
    // using virtual memory (if not this is a noop).
    OFF off = OFF(iPg * cbSysPage);
    if (!bufSymRecs.Commit(bufSymRecs.Start() + off, cbSysPage)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    // compute size to read in & read in the chunk of sym recs
    CB cb;
    MSF* pmsf = ppdb1->pmsf;

    if (iPg == cSymRecPgs - 1) { // last page?
        cb = pmsf->GetCbStream(dbihdr.snSymRecs) % cbSysPage;
        cb = cb ? cb : cbSysPage;
    }
    else {
        cb = cbSysPage;
    }

    CB cbRead = cb;
    if (!(pmsf->ReadStream(dbihdr.snSymRecs, off, bufSymRecs.Start() + off,
        &cbRead)) || cbRead != cb) {
        ppdb1->setReadError();
        return FALSE;
        }

    // mark page as read in
    pbvSymRecPgs->fSetBit(iPg);

    return TRUE;
}

BOOL DBI1::fpsymFromOff(OFF off, PSYM *ppsym)
{
    MTS_ASSERT(m_csForSymRec);
    *ppsym = reinterpret_cast<PSYM>(bufSymRecs.Start() + off);
    return fReadSymRec(*ppsym);
}

BOOL DBI1::fReadSymRec (PSYM psym) {

    MTS_PROTECT(m_csForSymRec);
    
    // Check to see if we are lazy loading our symbol buffer or not.
    // the bit vector presence is our flag.
    //
    if (pbvSymRecPgs) {
        // if psym is not part of lazy load area return
        if (!fValidPsym(psym)) {
            // REVIEW: Why were we ever returning a TRUE for a psym that is
            // out of range of our symbol record store?
            ppdb1->setCorruptError();
            return FALSE;
        }

        // read in first page in which sym rec starts
        size_t  iSymRecPgFirst = (PB(psym) - bufSymRecs.Start()) / cbSysPage;
        assert(iSymRecPgFirst < cSymRecPgs || fWrite && fValidPsym(psym));

        // If this page is already loaded this is almost a noop
        if (!fReadSymRecPage(iSymRecPgFirst)) {
            return FALSE;
        }

        // sanity check before we can reference psym->reclen:
        //  1) reclen should be the first field in the SYM struct,
        //  2) reclen should be two bytes
        //  3) both bytes of the reclen field should be in the page we just
        //      loaded (is it possible to have odd record sizes??)
        //  4) rectyp should have some valid value
        assert((PB)psym == (PB)&psym->reclen);
        assert(sizeof(psym->reclen) == 2);
        assert(UINT((PB(psym) + 1 - bufSymRecs.Start()) / cbSysPage) == iSymRecPgFirst);
        if (static_cast<UINT>((reinterpret_cast<PB>(psym) + 1 - bufSymRecs.Start()) / cbSysPage) != iSymRecPgFirst) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        assert(psym->rectyp < S_RECTYPE_MAX);

        // make sure we read in all pages that this sym rec spans
        size_t  iSymRecPgLast = (PB(NextSym(psym)) - 1 - bufSymRecs.Start()) / cbSysPage;

        assert(iSymRecPgLast < cSymRecPgs || fWrite && fValidPsym(PSYM(PB(NextSym(psym)) - 1)));
            
        if (!fValidPsym(PSYM(PB(NextSym(psym)) - 1))) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        size_t  iPg = iSymRecPgFirst + 1;

        for (; iPg <= iSymRecPgLast; iPg++) {
            if (!fReadSymRecPage(iPg)) {
                return FALSE;
            }
        }

        // check for special sym recs S_DATAREF, S_PROCREF & S_LPROCREF
        // - for these we may have to read in more stuff
        if (fIsRefSym(psym) && fNeedsSzConversion(psym))
        {
            // NextSym doesn't take into account the hidden name after the non-SZ
            // REFSYM symbols, but cbForSym does.
            //
            iSymRecPgFirst = (PB(NextSym(psym)) - bufSymRecs.Start()) / cbSysPage;

            if (!fReadSymRecPage(iSymRecPgFirst)) {
                return FALSE;
            }

            assert(PB(NextSym(psym)) < PB(psym) + cbForSym(psym));      // can't assert before reading first page!

            iSymRecPgLast = (PB(psym) + cbForSym(psym) - 1 - bufSymRecs.Start()) / cbSysPage;

            iPg = iSymRecPgFirst + 1;
            for (; iPg <= iSymRecPgLast; iPg++) {
                if (!fReadSymRecPage(iPg)) {
                    return FALSE;
                }
            }

        }

    }

    if (bufSymRecs.Contains(psym)) {
        if (fNeedsSzConversion(psym)) {
            fConvertSymRecStToSz(psym);

            if (fIsRefSym(psym)) {
                Mod1 *pmod; 
                REFSYM2 *pRefSym = (REFSYM2 *)psym;
                bool fVC40Offset = !isetModConverted.contains(pRefSym->imod);
                if (!openModByImod(imodForXimod(pRefSym->imod), (Mod **)&pmod)) {
                    return FALSE;
                }
                pRefSym->ibSym = pmod->offSymNewForOffSymOld(pRefSym->ibSym, fVC40Offset);
            }
        }
        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Convert the input symbol records into 32-bit versions
//
BOOL DBI1::fConvertSymRecs(CB cbSymRecs)
{
    MTS_ASSERT(m_csForSymRec);

    assert(pwti);
    assert(!fWrite);
    assert(cvtsyms.fConverting());
    
    MSF *           pmsf = ppdb1->pmsf;
    SymConvertInfo  sci;

    if (!cvtsyms.bufSyms().ReserveNoZeroed(cbSymRecs)) {
        ppdb1->setOOMError();
        return FALSE;
    }
    CB cbRead = cbSymRecs;
    if (!pmsf->ReadStream(dbihdr.snSymRecs, 0, cvtsyms.bufSyms().Start(), &cbRead) ||
        cbRead != cbSymRecs) {
        ppdb1->setReadError();
        return FALSE;
    }
    if (pwti->fQuerySymConvertInfo(sci, cvtsyms.bufSyms().Start(), cbSymRecs)) {
        assert(sci.cbSyms >= ULONG(cbSymRecs));
        assert(sci.cSyms < ULONG(cbSymRecs) / 4); // something reasonable
        // grab all the memory we will need up front.
        if (!cvtsyms.rgOffMap().setSize(sci.cSyms) ||
            !bufSymRecs.Reserve(sci.cbSyms) ||
            !bufSymRecs.Commit(bufSymRecs.Start(), sci.cbSyms)) {
            ppdb1->setOOMError();
            return FALSE;
        }
        sci.pbSyms = bufSymRecs.Start();
        sci.rgOffMap = &cvtsyms.rgOffMap()[0];
        if (!pwti->fConvertSymbolBlock(sci, cvtsyms.bufSyms().Start(), cbSymRecs)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    isetModConverted.reset();
    //
    // if you need to see the old symbols for diagnostic purposes,
    // comment out the following statement: cvtsyms.bufSyms().Free();
    //
    cvtsyms.bufSyms().Free();
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// This is called by the Mod1 objects when they open up and convert
// the symbols.  The REFSYM.ibSym fields in the global pool need to
// be adjusted to the new offsets in the Mod sym pool.
//
void DBI1::fixupRefSymsForImod(unsigned imod, CvtSyms & cvtsymsMod)
{
    MTS_PROTECT(m_csForSymRec);

    assert(cvtsyms.fConverting());
    assert(cvtsymsMod.fConverting());

    if (!isetModConverted.contains(imod) && fReadSymRecs()) {
        PSYM    psym = PSYM(bufSymRecs.Start());
        PSYM    psymMax = PSYM(PB(psym) + bufSymRecs.Size());

        for (; psym < psymMax; psym = PSYM(PB(psym) + cbForSym(psym))) {
            REFSYM2* pRefSym = (REFSYM2*)psym;
            if (fIsRefSym(PSYM(psym)) && pRefSym->imod == imod) {
                // assert((CB)cbAlign(psym->reclen) == cbForSym(psym));
                pRefSym->ibSym = cvtsymsMod.offSymNewForOffSymOld(pRefSym->ibSym);
            }
        }

        isetModConverted.add(imod);
    }
}

BOOL DBI1::fReadSymRecs()
{
    MTS_PROTECT(m_csForSymRec);

    // check and see if we have to read in the Symrecs Stream for this DBI
    if (!(bufSymRecs.Start()) && (dbihdr.snSymRecs != snNil)) {
        MSF* pmsf = ppdb1->pmsf;
        CB cbSymRecs = pmsf->GetCbStream(dbihdr.snSymRecs);
        if (cbSymRecs > 0) {
            expect(fAlign(cbSymRecs));

            if (cvtsyms.fConverting()) {
                return fConvertSymRecs(cbSymRecs);
            }

            if (!bufSymRecs.Reserve(cbSymRecs)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            // for full link case or upon request, simply read in all syms
            if (ppdb1->m_fFullBuild || ppdb1->m_fPrefetching) {
                if (!fWrite) {
                    if (!bufSymRecs.Commit(bufSymRecs.Start(), cbSymRecs)) {
                        ppdb1->setOOMError();
                        return FALSE;
                    }
                }
                CB cbRead = cbSymRecs;
                if (!(pmsf->ReadStream(dbihdr.snSymRecs, 0, bufSymRecs.Start(),
                    &cbRead)) || cbRead != cbSymRecs) {
                    ppdb1->setReadError();
                    return FALSE;
                }
                return TRUE;
            }

            
            // alloc a bitvec to keep track of pages loaded
            cSymRecPgs = (((unsigned)cbSymRecs) + cbSysPage - 1) / cbSysPage;
            pbvSymRecPgs = new BITVEC;
            if (!pbvSymRecPgs) {
                ppdb1->setOOMError();
                return FALSE;
            }

            if (!pbvSymRecPgs->fAlloc(cSymRecPgs)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            // we will lazy load sym recs
            return TRUE;
        }
    }

    return TRUE;
}



BOOL DBI1::fCheckReadWriteMode(BOOL fWrite_)
{
    if (fWrite_ != fWrite) {
        ppdb1->setUsageError();
        return FALSE;
    }
    else
        return TRUE;
}

// used in Mod1::fUpdateSecContrib()
BOOL DBI1::addSecContrib(SC2& scIn)
{
    assert(fWrite);
    
    MTS_PROTECT(m_csSecContribs);

    if (PB(pbscEnd) == bufSC.End() &&
        !bufSC.ReserveNoZeroed(SizeOfSCEntry(), &pbscEnd))
    {
        ppdb1->setOOMError();
        return FALSE;
    }

    expect(fAlign(pbscEnd));
    memcpy(pbscEnd, &scIn, SizeOfSCEntry());

    pbscEnd += SizeOfSCEntry();
    dassert(pbscEnd <= bufSC.End());

    return TRUE;
}

// used in Mod1::fInit()
BOOL DBI1::invalidateSCforMod(IMOD imod) {
    assert(fWrite);
    
    if (fSCCleared) {
        // the SC was cleared when the DBI was open - do nothing
        return TRUE;
    }

    MTS_PROTECT(m_csSecContribs);

    // scan the SC looking for matching imods and invalidate the entry
    PB  pbsc;

    // This is used only in fInit so the pdb shouldn't ever be corrupted here;
    // if it was corrupt it should have failed to open.
    getSecContribs(&pbsc);

    if (pbsc == NULL) {
        // There are no seccontribs - so nothing needs to be invalidated.
        return TRUE;
    }

    while (pbsc < pbscEnd) {
        expect(fAlign(pbsc));

        SC* psc = reinterpret_cast<SC *>(pbsc);

        if (psc->imod == imod) {
            // move bottom of the table into this spot
            dassert (pbscEnd > bufSC.Start());

            pbscEnd -= SizeOfSCEntry();
            memcpy(pbsc, pbscEnd, SizeOfSCEntry());
        }
        else {
            pbsc += SizeOfSCEntry();
        }
    }

    return TRUE;
}


bool DBI1::InitFrameDataStream()
{
    MTS_ASSERT(m_csForFrameData);

    if (pdbgFrameData == NULL && !OpenDbg(dbgtypeNewFPO, &pdbgFrameData)) 
        return false;
    return true;
}

bool DBI1::LoadFrameData()
{
    MTS_ASSERT(m_csForFrameData);
    assert(fWrite);
    if (pdbgFrameData == NULL && !InitFrameDataStream())
        return false;
    if (!fFrameDataLoaded) {    // load the frame data
        fFrameDataLoaded = true;

        assert(m_rgFrameData.size() == 0);
        
        DWORD cFrameData = pdbgFrameData->QuerySize();
        m_rgFrameData.setSize(cFrameData);

        if (!pdbgFrameData->QueryNext(cFrameData, m_rgFrameData.pBase())) {
            m_rgFrameData.clear();
            return false;
        }
        pdbgFrameData->Clear();
    }
    return true;
}

static int __cdecl blkCompare(MemBlock* pa, MemBlock* pb)
{
    return pa->rva - pb->rva;
}

static int __cdecl frameCompare(FRAMEDATA* pa, FRAMEDATA* pb)
{
    return pa->ulRvaStart - pb->ulRvaStart;
}

bool DBI1::SaveFrameData()
{
    MTS_ASSERT(m_csForFrameData);
    //
    // load the old data
    //
    if (!LoadFrameData())
        return false;
    if (m_rgFrameData.size() > 0) {
        fFrameDataLoaded = false;
        size_t iFrames = 0;
        size_t cFrames = m_rgFrameData.size();
        if (m_rgrvaRemovals.size() > 0) {
            qsort(m_rgrvaRemovals.pBase(), m_rgrvaRemovals.size(), sizeof MemBlock, (int (*)(const void*, const void*)) blkCompare);
            size_t cRemovals = m_rgrvaRemovals.size();
            size_t iRemovals = 0;

            while (iRemovals < cRemovals && iFrames < cFrames) {
                if (m_rgrvaRemovals[iRemovals].in(m_rgFrameData[iFrames].ulRvaStart)) {
                    m_rgFrameData[iFrames].ulRvaStart = 0;   // mark as deleted
                    ++iFrames;
                }
                else if (m_rgFrameData[iFrames].ulRvaStart < m_rgrvaRemovals[iRemovals].rva) {
                    ++iFrames;
                }
                else {
                    ++iRemovals;
                }
            }

            qsort(m_rgFrameData.pBase(), m_rgFrameData.size(), sizeof FRAMEDATA, (int (*)(const void*, const void*)) frameCompare);
            for (iFrames = 0; iFrames < cFrames && m_rgFrameData[iFrames].ulRvaStart == 0; ++iFrames)
                ;
        }
        assert(pdbgFrameData != NULL);
        if (iFrames < cFrames) {
            if (!pdbgFrameData->Append(static_cast<ULONG>(cFrames-iFrames), &m_rgFrameData[iFrames])) {
                ppdb1->setWriteError();
                return false;
            }
        }
        m_rgFrameData.setSize(0);
        m_rgrvaRemovals.setSize(0);
    }
    assert(m_rgFrameData.size() == 0);
    return true;
}

bool DBI1::AddFrameData(FRAMEDATA* pframe, DWORD cFrameData)
{
    MTS_PROTECT(m_csForFrameData);
    //
    // All calls to RemoveDataForRva must precede all calls to AddFrameData
    //
    if (!InitFrameDataStream()) {
        return false;
    }
#ifdef _DEBUG
    fFrameDataAdded = true;
#endif
    assert(fWrite);
    if (!fWrite) {
        return false;
    }
    if (pframe == NULL || cFrameData == 0) {
        return false;
    }
    if (m_rgrvaRemovals.size() > 0 && !SaveFrameData()) {
        return false;
    }

    assert(pdbgFrameData && m_rgFrameData.size() == 0);    // all framedata are in the dbg stream
    return pdbgFrameData->Append(cFrameData, pframe) != 0;
}

void DBI1::RemoveDataForRva(ULONG rva, ULONG cb)
{
    //
    // This call can only be made before the first call to AddFrameData
    //
#ifdef _DEBUG
    assert(!fFrameDataAdded);
#endif
    assert(fWrite);
    if (!fWrite)
        return;

    MTS_PROTECT(m_csForFrameData);

    size_t cRva = m_rgrvaRemovals.size();
    for (size_t i = cRva; i > 0; --i) {
        if (m_rgrvaRemovals[i-1].rva + m_rgrvaRemovals[i-1].cb == rva) {
            m_rgrvaRemovals[i-1].cb += cb;
            return;
        }
        else if (m_rgrvaRemovals[i-1].in(rva)) {
            if (rva + cb > m_rgrvaRemovals[i-1].rva + m_rgrvaRemovals[i-1].cb) {
                m_rgrvaRemovals[i-1].cb = rva + cb - m_rgrvaRemovals[i-1].rva;
            }
        }
    }
    MemBlock b(rva, cb);
    m_rgrvaRemovals.append(b);
}


BOOL DBI1::initFileInfo(IMOD imod, IFILE ifileMac)
{
    MTS_ASSERT(m_csForMods);

    MODI* pmodi = pmodiForImod(imod);
    if (!pmodi)
        return FALSE;
    if (ifileMac > pmodi->ifileMac) {
        if (pmodi->mpifileichFile != NULL) {
            delete [] pmodi->mpifileichFile;
        }
        // need more space than we currently have
        if (!(pmodi->mpifileichFile = new ICH[ifileMac])) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }
    pmodi->ifileMac = ifileMac;
    memset(pmodi->mpifileichFile, 0, ifileMac * sizeof(ICH));
    return TRUE;
}

BOOL DBI1::addFileInfo(IMOD imod, IFILE ifile, SZ_CONST strFile)
{
    MTS_ASSERT(m_csForMods);

    MODI* pmodi = pmodiForImod(imod);
    if (!pmodi)
        return FALSE;
    ICH ich;
    if (!addFilename(strFile, &ich))
        return FALSE;
    assert(ifile < pmodi->ifileMac);
    pmodi->mpifileichFile[ifile] = ich;
    return TRUE;
}

BOOL DBI1::addFilename(SZ_CONST szFile, ICH *pich)
{
    MTS_ASSERT(m_csForMods);

    // search bufFilenames, the catenation of filenames, for szFile
    NI  niFile = nmtFileInfo.niForSz(szFile);

    // NB: this depends upon the implementation of NMT...namely
    // that the NI returned is the offset of the string in the NMT
    // buffer.  Coupled with the fact that the NMT stores a \0 as the
    // automatic first string means that the ni(szFile) - 1 == ich.

    if (niFile == niNil) {
        // we have to add it...
        if (!nmtFileInfo.addNiForSz(szFile, &niFile)) {
            ppdb1->setOOMError();
            return FALSE;
        }

        assert(bufFilenames.Size() == CB(niFile - 1));

        *pich = bufFilenames.Size();
        if (!bufFilenames.Append(reinterpret_cast<PB>(const_cast<SZ>(szFile)), static_cast<CB>(strlen(szFile) + 1))) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }
    else {
        *pich = niFile - 1;
        assert(strcmp(nmtFileInfo.szForNi(niFile), szFile) == 0);
    }

    return TRUE;
}

inline bool  __fastcall fRangeCheckPtr(PV pvBase, PV pv, PV pvEnd)
{
    return pvBase <= pv && pv < pvEnd;
}


inline bool __fastcall fValidateSt(const BYTE * pbSt, const BYTE * pbEnd)
{
    return pbSt < pbEnd && pbSt + *pbSt < pbEnd;
}

BOOL DBI1::reloadFileInfo(PB pb, CB cbReloadBuf)
{
    PB  pbOrig = pb;
    PB  pbEnd  = pb + cbReloadBuf;

    if (cbReloadBuf < sizeof(IMOD) + sizeof(USHORT)) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    MTS_ASSERT(m_csForMods);      

    if (*((IMOD*&)pb)++ != imodMac) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    DWORD cRefs = *((USHORT*&)pb)++;                    // overridden later
    
    if (imodMac == 0) {
        if (cRefs != 0) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        return TRUE;
    }

    USHORT* mpimodiref  = (USHORT*)pb;                  // Not very useful if cRefs >= 0x10000, see below
    USHORT* mpimodcref  = (USHORT*)((PB)mpimodiref    + sizeof(USHORT)*imodMac);
    ICH*  mpirefichFile = (ICH*)   ((PB)mpimodcref    + sizeof(USHORT)*imodMac);

    // check the relative ordering of the pointers and whether they are
    // in the proper range.
    if (!fRangeCheckPtr(pb, mpimodiref, mpimodcref) ||
        !fRangeCheckPtr(mpimodiref, mpimodcref, mpirefichFile)) 
    {
        ppdb1->setCorruptError();
        return FALSE;
    }

    // cRefs read above could be wrong - if it was >= 0x10000 when
    // writing. Just recompute it with mpimodcref. This is the same
    // reason why mpimodiref may not be valid as well.
    cRefs = 0;
    for (IMOD imod = 0; imod < imodMac; imod++) {
        cRefs += mpimodcref[imod];
    }

    PCH rgchNames = (PCH) ((PB)mpirefichFile + sizeof(ICH)*cRefs);
    if (!fRangeCheckPtr(mpimodcref, mpirefichFile, rgchNames + 1)) {
        ppdb1->setCorruptError();
        return FALSE;
    }
    
    dassert(size_t(cbReloadBuf) >= sizeof(IMOD) +           // imodMac
                           sizeof(USHORT) +                 // cRefs
                           sizeof(USHORT) * imodMac * 2 +   // mpimodiref and mpimodcref tables
                           sizeof(ICH) * cRefs);            // mpirefichFile table
                        
    // Preallocate the memory for the real filename info
    CB cbAlloc = (CB)(cbReloadBuf - (PB(rgchNames) - pbOrig));
    if ((cbAlloc != 0) && !bufFilenames.SetInitAlloc(cbAlloc)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    UINT cRefsDone = 0;
    for (IMOD imod = 0; imod < imodMac; imod++) {
        if (!initFileInfo(imod, mpimodcref[imod]))
            return FALSE;
        for (IFILE ifile = 0; ifile < mpimodcref[imod]; ifile++)    {
            UINT iref = cRefsDone + ifile;
            ICH ich = mpirefichFile[iref];
            if (!fRangeCheckPtr(rgchNames, &rgchNames[ich], pbEnd)) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            if (!IS_SZ_FORMAT_PDB(ppdb1)) {
                char szName[1024];
                if (!fValidateSt(PB(&rgchNames[ich]), pbEnd)) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }
                verify(STMBCSToSZUTF8(&rgchNames[ich], szName, 1024));
                if (!addFileInfo(imod, ifile, szName)) {
                    return FALSE;
                }
            }
            else
            {
                if (!fValidateSz(PB(&rgchNames[ich]), pbEnd)) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }
                assert(fRangeCheckPtr(&rgchNames[ich], &rgchNames[ich] + strlen(&rgchNames[ich]), pbEnd));
                if (!addFileInfo(imod, ifile, &rgchNames[ich])) {
                    return FALSE;
                }
            }
        }
        cRefsDone += mpimodcref[imod];
    }
    return TRUE;
}

BOOL DBI1::QueryFileInfo(OUT PB pb, CB* pcb)
{
    MTS_PROTECT(m_csForMods);

    debug(PB pbSave = pb);

    // Return blob in following format
    // { ushort cMods, cRefs }
    // { ushort iRefModStart[ cMods ] }
    // { ushort cRefsForMod[ cMods ]  }
    // { ICH   rgICH[ cMods ] [ cRefsForMod(iMod) ] }
    // { SZ    rgFileNames[] }

    // count refs
    int     cRefs = 0;
    IMOD    imod;
    for (imod = 0; imod < imodMac; imod++) {
        MODI* pmodi = pmodiForImod(imod);
        if (!pmodi)
            return FALSE;
        cRefs += pmodi->ifileMac;
    }

    size_t cbSize = cbAlign(2*sizeof(USHORT) + 2*sizeof(USHORT)*imodMac + sizeof(ULONG)*cRefs + bufFilenames.Size());
    if (cbSize > MAXLONG) {
        return FALSE;
    }
    CB cb = CB(cbSize);
    if (!pb) {
        *pcb = cb;
        return TRUE;
    }
    else if (pb && *pcb != cb)
        return FALSE;

    // form sstFileIndex record
    *((USHORT*&)pb)++ = imodMac;
    
    //rangeCheck(cRefs, 0, USHRT_MAX+1);
    *((USHORT*&)pb)++ = USHORT(cRefs & 0xffff);
    
    DWORD   irefStart = 0;
    for (imod = 0; imod < imodMac; imod++) {
        *((USHORT*&)pb)++ = USHORT(irefStart & 0xffff);
        irefStart += pmodiForImod(imod)->ifileMac;
    }
    for (imod = 0; imod < imodMac; imod++) {
        *((USHORT*&)pb)++ = pmodiForImod(imod)->ifileMac;
    }
    for (imod = 0; imod < imodMac; imod++) {
        MODI* pmodi = pmodiForImod(imod);
        for (IFILE ifile = 0; ifile < pmodi->ifileMac; ifile++)
            *((ICH*&)pb)++ = pmodi->mpifileichFile[ifile];
    }
    if (bufFilenames.Size() != 0) {
        memcpy(pb, bufFilenames.Start(), bufFilenames.Size());  // Should be SZ now
        pb += bufFilenames.Size();
    }
    if (pb < pbAlign(pb))
        memset(pb, 0, size_t(pbAlign(pb) - pb));

    debug(assert(pbSave + cb == pbAlign(pb)));
    return TRUE;
}

BOOL DBI1::QueryFileInfo2(OUT PB pb, CB* pcb)
{
    MTS_PROTECT(m_csForMods);

    debug(PB pbSave = pb);

    // Return blob in following format
    // { ulong cMods, cRefs }
    // { ulong iRefModStart[ cMods ] }
    // { ulong cRefsForMod[ cMods ]  }
    // { ICH   rgICH[ cMods ] [ cRefsForMod(iMod) ] }
    // { SZ    rgFileNames[] }

    // count refs
    ULONG   cRefs = 0;
    IMOD    imod;
    for (imod = 0; imod < imodMac; imod++) {
        MODI* pmodi = pmodiForImod(imod);
        if (!pmodi)
            return FALSE;
        cRefs += pmodi->ifileMac;
    }

    size_t cbSize = cbAlign(2*sizeof(ULONG) + 2*sizeof(ULONG)*imodMac + sizeof(ICH)*cRefs + bufFilenames.Size());
    if (cbSize > MAXLONG) {
        return FALSE;
    }
    CB cb = CB(cbSize);
    if (!pb) {
        *pcb = cb;
        return TRUE;
    }
    else if (pb && *pcb != cb)
        return FALSE;

    // form sstFileIndex record
    *((ULONG*&)pb)++ = imodMac;
    *((ULONG*&)pb)++ = cRefs;
    
    ULONG irefStart = 0;
    for (imod = 0; imod < imodMac; imod++) {
        *((ULONG*&)pb)++ = irefStart;
        irefStart += pmodiForImod(imod)->ifileMac;
    }
    for (imod = 0; imod < imodMac; imod++) {
        *((ULONG*&)pb)++ = pmodiForImod(imod)->ifileMac;
    }
    for (imod = 0; imod < imodMac; imod++) {
        MODI* pmodi = pmodiForImod(imod);
        for (IFILE ifile = 0; ifile < pmodi->ifileMac; ifile++)
            *((ICH*&)pb)++ = pmodi->mpifileichFile[ifile];
    }
    if (bufFilenames.Size() != 0) {
        memcpy(pb, bufFilenames.Start(), bufFilenames.Size());  // Should be SZ now
        pb += bufFilenames.Size();
    }
    if (pb < pbAlign(pb))
        memset(pb, 0, size_t(pbAlign(pb) - pb));

    debug(assert(pbSave + cb == pbAlign(pb)));
    return TRUE;
}

void DBI1::DumpMods()
{
#if !defined(PDB_LIBRARY)

    USES_STACKBUFFER(0x400);

    MTS_PROTECT(m_csForMods);

    OutputInit();

    StdOutPrintf(L"imod   %-20.20s %-30.30s  sn cbSyms cbLines cbC13Lines source\n", L"module", L"file");

    for (IMOD imod = 0; imod < imodMac; imod++) {
        MODI *pmodi = pmodiForImod(imod);

        char szSrcFile[_MAX_PATH];
        szSrcFile[0] = '\0';
        srcForImod(imod, szSrcFile, 0);

        // The filenames are UTF-8.  Convert to UTF-16.

        wchar_t *wszModule = GetSZUnicodeFromSZUTF8(pmodi->szModule());
        wchar_t *wszObjFile = GetSZUnicodeFromSZUTF8(pmodi->szObjFile());
        wchar_t *wszSrcFile = GetSZUnicodeFromSZUTF8(szSrcFile);

        StdOutPrintf(L"0x%04X %-20.20s %-30.30s %3d %6d %7d %5d %s\n",
                     imod,
                     wszModule,
                     wszObjFile,
                     (short) pmodi->sn,
                     pmodi->cbSyms,
                     pmodi->cbLines,
                     pmodi->cbC13Lines,
                     wszSrcFile);
    }

    StdOutFlush();

#endif
}

void DBI1::DumpSecContribs()
{
#if !defined(PDB_LIBRARY)
    MTS_PROTECT(m_csSecContribs);

    OutputInit();

    PB pbsc;
    if (!getSecContribs(&pbsc)) {
        StdOutPrintf(L"*** Error reading Section Contributions ***\n");
    }

    if (fSCv2) {
        StdOutPrintf(L"Section Contributions\n"
                     L"isect  off        cb         deltaOff cbExcess dwChar     imod   dwDataCrc  dwRelocCrc isectCoff\n");
    } else {
        StdOutPrintf(L"Section Contributions\n"
                     L"isect  off        cb         deltaOff cbExcess dwChar     imod   dwDataCrc  dwRelocCrc\n");
    }

    unsigned isectPrev = 0;
    unsigned offPrev = 0;
    unsigned cbPrev = 0;

    for (; pbsc != NULL && pbsc < pbscEnd; pbsc += SizeOfSCEntry()) {
        SC2 *psc = reinterpret_cast<SC2 *>(pbsc);

        if (psc->isect != isectPrev) {
            offPrev = 0;
            isectPrev = psc->isect;
            cbPrev = 0;
        }

        unsigned cbDelta = psc->off - offPrev;
        unsigned cbExcess = cbDelta - cbPrev;

        if (fSCv2) {
            StdOutPrintf(L"0x%4.4x 0x%8.8x 0x%8.8x 0x%06x 0x%06x 0x%08x 0x%4.4x 0x%8.8x 0x%8.8x 0x%8.8x\n",
                         psc->isect,
                         psc->off,
                         psc->cb,
                         cbDelta,
                         cbExcess,
                         psc->dwCharacteristics,
                         psc->imod,
                         psc->dwDataCrc,
                         psc->dwRelocCrc,
                         psc->isectCoff);
        } else {
            StdOutPrintf(L"0x%4.4x 0x%8.8x 0x%8.8x 0x%06x 0x%06x 0x%08x 0x%4.4x 0x%8.8x 0x%8.8x\n",
                         psc->isect,
                         psc->off,
                         psc->cb,
                         cbDelta,
                         cbExcess,
                         psc->dwCharacteristics,
                         psc->imod,
                         psc->dwDataCrc,
                         psc->dwRelocCrc);
        }

        offPrev = psc->off;
        cbPrev = psc->cb;
    }

    StdOutFlush();
#endif
}

void DBI1::DumpSecMap()
{
#if !defined(PDB_LIBRARY)

    if (!bufSecMap.Start())
        return;

    OutputInit();

    OMFSegMap* phdr = (OMFSegMap*) bufSecMap.Start();

    StdOutPrintf(L"Section Map cSeg = 0x%4.4x, cSegLog = 0x%4.4x\n", phdr->cSeg, phdr->cSegLog);
    StdOutPrintf(L"flags\tovl\tgroup\tframe\tsegname\tclass\toffset\t\tcbseg\n");

    for (OMFSegMapDesc* pDesc =(OMFSegMapDesc*)(bufSecMap.Start() + sizeof (OMFSegMap));
        (PB) pDesc < bufSecMap.End();
        pDesc++) {
        StdOutPrintf(L"0x%4.4x\t0x%4.4x\t0x%4.4x\t0x%4.4x\t0x%4.4x\t0x%4.4x\t0x%8.8x\t0x%8.8x\n",
                     int(pDesc->flags.fAll),
                     int(pDesc->ovl),
                     pDesc->group,
                     pDesc->frame,
                     pDesc->iSegName,
                     pDesc->iClassName,
                     pDesc->offset,
                     pDesc->cbSeg);
    }

	StdOutFlush();
#endif
}

void DBI1::DumpTypeServers()
{
#if !defined(PDB_LIBRARY)
#ifdef PDB_TYPESERVER

    USES_STACKBUFFER(0x400);

    UINT i;
    TSM *ptsm;

    OutputInit();

    if (QueryLazyTypes())
        StdOutPrintf(L"supports lazy types\n");
    else
        StdOutPrintf(L"does not support lazy types\n");

    for (i = 0; i < bufTSMap.Length(); i++) { // search existing list
        ptsm = bufTSMap[i];

        // The filenames are UTF-8.  Convert to UTF-16.

        wchar_t *wszName = GetSZUnicodeFromSZUTF8(ptsm->szName());
        wchar_t *wszPath = GetSZUnicodeFromSZUTF8(ptsm->szPath());

        if (i > 0 && dbihdr.iMFC == i) {  // MFC library
            StdOutPrintf(L"%5.5d %s (%s) MFC symbols\n", i, wszName, wszPath);
        }
        else {
            StdOutPrintf(L"%5.5d %s (%s)\n", i, wszName, wszPath);
        }
    }

    StdOutFlush();
#endif
#endif
}

BOOL DBI1::AddThunkMap(OFF* poffThunkMap, UINT nThunks, CB cbSizeOfThunk,
    SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable)
{
    dassert(pgsiPS);
    if (nThunks > 0)
        SetIncLink();
    return pgsiPS->addThunkMap(poffThunkMap, nThunks, cbSizeOfThunk, psoSectMap, nSects, isectThunkTable, offThunkTable);
}


#ifdef PDB_TYPESERVER
BOOL DBI1::FindValidate(const char *szPDB, const char *szObjFile)
{
    dassert(szPDB);

    // first try finding the pdb where the objfile says
    if (szObjFile) {
        char szPDBSansPath[_MAX_FNAME];
        char szPDBExt[_MAX_EXT];
        char szPDBLocal[_MAX_PATH];

        strncpy_s(szPDBLocal, _countof(szPDBLocal), szPDB, _TRUNCATE);
        _splitpath_s(szPDBLocal, NULL, 0, NULL, 0, szPDBSansPath, _countof(szPDBSansPath), szPDBExt, _countof(szPDBExt));

        char szPathBuf[_MAX_PATH+_MAX_DRIVE];
        char szFullPath[_MAX_PATH+_MAX_DRIVE];

        _fullpath(szFullPath, szObjFile, _MAX_PATH+_MAX_DRIVE);
        _splitpath_s(szFullPath, szPathBuf, _MAX_DRIVE, szPathBuf + _MAX_DRIVE, _countof(szPathBuf) - _MAX_DRIVE, NULL, 0, NULL, 0);
        SZ szPath;
        if (szPathBuf[0] == 0) {
            // no drive spec - set up path without it
            szPath = szPathBuf + _MAX_DRIVE;
        }
        else {
            // concatenate drive and dir to form full path
            szPathBuf[2] = szPathBuf[1];
            szPathBuf[1] = szPathBuf[0];
            szPath = szPathBuf + 1;
        }
        _stprintf(szPDBLocal, "%s\\%s%s", szPath, szPDBSansPath, szPDBExt);
        if (_access_s(szPDBLocal, 0) == 0) {
            return TRUE;
        }
    } 

    // try finding pdb as originally referenced
    return _access_s(szPDB, 0) == 0;
}

ITSM DBI1::itsmMfcFromSzPdbName(SZ_CONST szPdb, UINT iTsmCur)
{
    char    sz[ _MAX_FNAME ];
    static const char *
            rgszMfcPrefix[] = { "mfcs", "nafx", "uafx" };

    _splitpath_s(szPdb, NULL, 0, NULL, 0, sz, _countof(sz), NULL, 0);
    for (unsigned isz = 0; isz < _countof(rgszMfcPrefix); isz++) {
        assert(strlen(rgszMfcPrefix[isz]) == 4);
        if (_strnicmp(sz, rgszMfcPrefix[isz], 4) == 0) {
            rangeCheck(iTsmCur, 0, UCHAR_MAX + 1);
            return ITSM(iTsmCur);       // just a guess, but we have to do it!
        }
    }
    return 0;
}

#endif  // PDB_TYPESERVER

// Compare and see of the names point to same typeserver
static BOOL SameFileInfo(SZ_CONST szName1, SZ_CONST szPath1, SZ_CONST szName2, SZ_CONST szPath2)
{
    return _tcsicmp(szName1, szName2) == 0;   // full paths are provided in LF_TYPESERVER
}

#if 0   // Don't allow adding type servers

BOOL DBI1::AddTypeServer(SIG sig, AGE age, SZ szName, SZ szPath,  OUT ITSM *piTsm)
{
    UINT i;
    TSM *ptsm;
    *piTsm = 0;
    for (i = 0; i < bufTSMap.Length(); i++) { // search existing list
        ptsm = bufTSMap[i];
        if (SameFileInfo(ptsm->szName(), ptsm->szPath(), szName, szPath)) {  // found it in list
            rangeCheck(i, 0, UCHAR_MAX + 1);
            *piTsm = ITSM(i);
            return TRUE;
        }
    }

    rangeCheck(i, 0, CHAR_MAX+1);
    TI tiMin = TiForTiItsm(0, ITSM(i));   // use top byte to hold index
    ptsm = new(bufTSMap.RawBuffer(), szName, szPath) TSM(sig, age, szName, szPath, tiMin);

    if (ptsm == 0 || !bufTSMap.append(ptsm)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (dbihdr.iMFC == 0) {   // no MFC library seen yet
        dbihdr.iMFC = itsmMfcFromSzPdbName(szName, i);
    }

    rangeCheck(i, 0, UCHAR_MAX + 1);
    *piTsm = ITSM(i);
    assert(i == bufTSMap.Length()-1);

    //
    // Copy Src object from the typeserver's pdb
    //
    PDB* ppdb;
    if (fOpenPdb(Sig70FromSig(sig), age, szName, szPath, &ppdb) && ppdb != 0) {
        ppdb1->CopySrc(ppdb);
        ppdb->Close();
    }

    return TRUE;
}

#endif

void DBI1::FlushTypeServers()
{
#ifdef PDB_TYPESERVER
    if (QueryLazyTypes()) {
        ClosePdbs();    // close down all the type pools
        for (UINT i = 0; i < bufTSMServer.length(); i++) { // clear old pointers to pools
            if (bufTSMServer[i] != (TPI*)(-1)) {    // remember if we failed to find a server
                bufTSMServer[i] = 0;
            }
        }
    }
#else
    ppdb1->setLastError(EC_NOT_IMPLEMENTED /*, perhaps the name of typeserver will be good */);
#endif
}

BOOL DBI1::QueryTypeServerByPdbUTF8(UTFSZ_CONST szPdb, OUT ITSM* piTsm)
{
#ifdef PDB_TYPESERVER
    if (QueryLazyTypes()) {
        TSM *ptsm;
        for (UINT i = 0; i < bufTSMap.Length(); i++) { // search existing list
            ptsm = bufTSMap[i];
            if (SameFileInfo(ptsm->szName(), ptsm->szPath(), szPdb, 0)) {  // found it in list
                rangeCheck(i, 0, UCHAR_MAX + 1);
                *piTsm = ITSM(i);
                return TRUE;
            }
        }
    }
#else
    ppdb1->setLastError(EC_NOT_IMPLEMENTED /*, perhaps the name of typeserver will be good */);
#endif

    return FALSE;
}

BOOL DBI1::QueryTypeServer(ITSM iTsm, OUT TPI** pptpi)
{
#ifdef PDB_TYPESERVER
    USES_STACKBUFFER(0x400);

    if (iTsm < bufTSMap.Length()) {
        TSM *ptsm = bufTSMap[iTsm];
        if (bufTSMServer[iTsm] == 0) { // open the server
            bufTSMServer[iTsm] = (TPI*)(-1); // indicate that we have already tried to open this once
            PDB* ppdb = 0;
            wchar_t *wszTSMName = GetSZUnicodeFromSZUTF8(ptsm->szName());
            if (wszTSMName == NULL) {
                return FALSE;   // could not open server
            }
            if (iTsm != 0 && 
                 !fOpenPdb(Sig70FromSig(ptsm->signature), ptsm->age, wszTSMName, ptsm->szPath(), &ppdb, TRUE)) {
                return FALSE;   // could not open server
            }
            if (ppdb == 0) { // use this pdb's type server
                assert(ppdb1->ptpi1 != 0); // we opened the tpi in fInit
                bufTSMServer[iTsm] = ppdb1->ptpi1;
            } 
            else {  // get the type server from the new pdb
                TPI* pSrv = 0;
                if (!ppdb->OpenTpi(pdbRead pdbGetRecordsOnly, &pSrv) ||
                     !fInsertPdb(ppdb, pSrv)) {
                    if (pSrv) {
                        pSrv->Close();
                    }
                    ppdb->Close();
                    return FALSE;   // error opening TPI
                }
#ifdef _CPPRTTI
                TPI1* ptpi1 = dynamic_cast<TPI1*>(pSrv); 
                if (ptpi1) {
                    ptpi1->UseTsm(iTsm, this);
                }
                else {
                    pSrv->Close();
                    ppdb->Close();
                    return FALSE;   // error some other kind of type server got here?
                }
#else
                ((TPI1*)(pSrv))->UseTsm(iTsm, this); //### gack! downcast of TPI pointer
#endif
                assert(pSrv != 0);
                bufTSMServer[iTsm] = pSrv;
            }
        }
        else if (bufTSMServer[iTsm] == (TPI*)(-1)) { // don't try again
            return FALSE;
        }
        *pptpi = bufTSMServer[iTsm];
        return *pptpi != 0;
    }
#endif
    ppdb1->setLastError(EC_NOT_IMPLEMENTED /*, perhaps the name of typeserver will be good */);
    return FALSE;
}

BOOL DBI1::FindTypeServers(OUT EC* pec, OUT char szError[cbErrMax])
{
#ifdef PDB_TYPESERVER
    UINT i;
    TSM *ptsm;
    for (i = 0; i < bufTSMap.Length(); i++) { // access all pdb's in list
        ptsm = bufTSMap[i];
        if (!FindValidate(ptsm->szName(), ptsm->szPath())) {
            *pec = EC_NOT_FOUND;
            strncpy_s(szError, cbErrMax, ptsm->szName(), _TRUNCATE);
            return FALSE;
        }
    }   
    return TRUE;
#else
    ppdb1->setLastError(EC_NOT_IMPLEMENTED /*, perhaps the name of typeserver will be good */);
    return FALSE;
#endif
}

BOOL DBI1::QueryItsmForTi(TI ti, OUT ITSM* piTsm)
{
#ifdef PDB_TYPESERVER
    if (bufTSMap.Length() > 0 && !CV_IS_PRIMITIVE(ti)) {
        UINT i = ItsmFromTi(ti);                        // top byte is the TSM index
        if (i >= bufTSMap.Length()) i = 0;    // 0 is default for bad TI's
        rangeCheck(i, 0, CHAR_MAX+1);
        *piTsm = ITSM(i);
        return TRUE;
    }
#endif
    return FALSE;
}

BOOL DBI1::QueryNextItsm(ITSM itpi, OUT ITSM *inext)
{
#ifdef PDB_TYPESERVER
    if (dbihdr.iMFC < bufTSMap.Length()) {
        *inext = (ITSM)dbihdr.iMFC;
        return TRUE;
    }
#endif
    return FALSE;
}

BOOL DBI1::QueryLazyTypes()
{
#ifdef PDB_TYPESERVER
    return bufTSMap.Length() > 0;   // type map is not empty, so lazy types
#else
    return FALSE;
#endif
}

BOOL DBI1::SetLazyTypes(BOOL fLazy)
{
#ifdef PDB_TYPESERVER
    if (!fLazy) {// turn off lazy types
        if (bufTSMap.Length() == 1) { // only the entry for this pdb's TPI
            bufTSMap.Clear();       // empty the buffer, we will not be adding any new TPI's
        } 
        return bufTSMap.Length() == 0;
    }
    else {  // turn on lazy types
        return QueryLazyTypes();    // cannot turn lazt types on if they have already been turned off
    }
#else
    // Turning off lazy types should return true, turning it on returns false.

    return !fLazy;
#endif
}

Dbg1Data* DBI1::fGetDbgData(int dbgtype, ULONG cbElement, PFN_DBGCMP pfn)
{
    MTS_ASSERT(m_csForDbg);
    assert(dbgtype >= 0 && dbgtype < dbgtypeMax);

    Dbg1Data *pDbgData = rgOpenedDbg[dbgtype];

    if (pDbgData != NULL) {
        if (fWrite) {
            return NULL;
        }
    } 
    else {
        MSF* pmsf = getpmsf();

        if (!pmsf) {
            // C8 pdb
            ppdb1->setLastError(EC_FORMAT);
            return NULL;
        }

        if (!fValidateSnDbg(&dbghdr.rgSnDbg[dbgtype])) {
            ppdb1->setLastError(EC_FORMAT);
            return NULL;
        }

        pDbgData = new Dbg1Data(this, dbgtype, cbElement, pfn);
        if (pDbgData == NULL) {
            return pDbgData;
        }

        SN snDbg = dbghdr.rgSnDbg[dbgtype];
        CB cbRead = pmsf->GetCbStream(snDbg);
        Buffer &bufDbg = pDbgData->GetBuffer();

        OFF off = 0;
        if (cbRead >0) {
            if (!bufDbg.ReserveNoZeroed(cbRead)) {
                ppdb1->setOOMError();
                delete pDbgData;
                return NULL;
            }
            if (!pmsf->ReadStream(snDbg, off, bufDbg.Start(), &cbRead)) {
                ppdb1->setReadError();
                delete pDbgData;
                return NULL;
            }
        }

        // Verify that the debug data read from stream is not corrupted.
        if (!VerifyDbgData(&bufDbg, dbgtype)) {
            ppdb1->setCorruptError();
            delete pDbgData;
            return NULL;
        }

        rgOpenedDbg[dbgtype] = pDbgData;
    }

    return pDbgData;
}

BOOL DBI1::fSaveDbgData(int dbgtype, Buffer &buf)
{
    MTS_ASSERT(m_csForDbg);

    MSF *pmsf = getpmsf();
    SN snDbg = dbghdr.rgSnDbg[dbgtype];
    if (buf.Size() == 0) {
        if (!nullifyStream(pmsf, &dbghdr.rgSnDbg[dbgtype])) {
            return FALSE;
        }
    }
    else {
        if (!pmsf->ReplaceStream(snDbg, buf.Start(), buf.Size())) {
            return FALSE;
        }
    }
    return TRUE;
}

//
// Opens interface for handling Dbg data in the pdb
//
BOOL DBI1::OpenDbg(DBGTYPE dbgtype, Dbg **ppdbg) 
{
    MTS_PROTECT(m_csForDbg);


    Dbg1 *pdbg1;

    *ppdbg = pdbg1 = NULL;

    switch (dbgtype) {
    case dbgtypeFPO:
        pdbg1 = new DbgFpo1(this);
        break;

    case dbgtypeException:
        pdbg1 = new DbgFunc1(this);
        break;

    case dbgtypeFixup:
        pdbg1 = new DbgXFixup1(this);
        break;

    case dbgtypeOmapToSrc:
        pdbg1 = new DbgOmap1(this, TRUE);
        break;

    case dbgtypeOmapFromSrc:
        pdbg1 = new DbgOmap1(this, FALSE);
        break;

    case dbgtypeSectionHdr:
        pdbg1 = new DbgSect1(this);
        break;

    case dbgtypeTokenRidMap:
        pdbg1 = new DbgTokenRidMap(this);
        break;

    case dbgtypeXdata:
        pdbg1 = new DbgXdata(this);
        break;

    case dbgtypePdata:
        pdbg1 = new DbgPdata(this);
        break;

    case dbgtypeNewFPO:
        pdbg1 = new DbgFrameData1(this);
        break;

    case dbgtypeSectionHdrOrig:
        pdbg1 = new DbgSectOrig(this);
        break;

    default:
        return FALSE;
    }

    if (pdbg1 != NULL && pdbg1->fInit()) {
        *ppdbg = pdbg1;
        return TRUE;
    }
    else {
        delete pdbg1;
        return FALSE;
    }
}

//
// Return array of enums describing contents of pdb
//
BOOL DBI1::QueryDbgTypes(OUT DBGTYPE *pdbgtype, OUT long* pcDbgtype)
{   
    MTS_PROTECT(m_csForDbg);
    assert(pcDbgtype);
    long cDbgType = 0;

    for (ULONG dbgtype = 0; dbgtype < dbgtypeMax; dbgtype++) {
        if (dbghdr.rgSnDbg[dbgtype] != snNil) {
            if (pdbgtype) {
                if (cDbgType >= *pcDbgtype) {
                    return FALSE;
                }
                assume(cDbgType >= 0);
                pdbgtype[cDbgType] = (DBGTYPE)dbgtype;
            }
            cDbgType++;
        }
    }
    *pcDbgtype = cDbgType;
    return TRUE;
}


BOOL DBI1::fValidateSnDbg(SN *psnDbg)
{
    MTS_ASSERT(m_csForDbg);

    if (*psnDbg == snNil) {
        if (!fWrite) {
            ppdb1->setLastError(EC_NOT_FOUND);
            return FALSE;
        }

        MSF *   pmsf = ppdb1->pmsf;
        assert(pmsf);
        *psnDbg = pmsf->GetFreeSn();
        if (*psnDbg == snNil ||
            !pmsf->ReplaceStream(*psnDbg, 0, 0)) {
            return FALSE;
        }
    }

    return TRUE;
}

//
// add and overwrite any link info in the pdb.
//

PLinkInfo DBI1::GetUTF8LinkInfo(PLinkInfo pli)
{
    USES_STACKBUFFER(0x400);

    // Convert the link info structure from MBCS to UTF8
    CB cb = 2 * pli->Cb();
    PB pb = new BYTE[cb];

    if (!pb) {
        return NULL;
    }

    PLinkInfo pliNew = (PLinkInfo)pb;
    
    pliNew->ver      = pli->ver;
    pliNew->offszCwd = sizeof LinkInfo;

    SZ szCwd = GetSZUTF8FromSZMBCS(pli->SzCwd());
    if (szCwd != NULL) {

        strcpy_s(pliNew->SzCwd(), cb - pliNew->offszCwd, szCwd);
        pliNew->offszCommand = pliNew->offszCwd + static_cast<ULONG>(strlen(szCwd)) + 1;

        SZ szCommand = GetSZUTF8FromSZMBCS(pli->SzCommand());
        if (szCommand != NULL)  {

            strcpy_s(pliNew->SzCommand(), cb - pliNew->offszCommand, szCommand);
            pliNew->offszLibs = pliNew->offszCommand + static_cast<ULONG>(strlen(szCommand)) + 1;

            size_t  cb = MBCSToUTF8(pli->SzLibs(), pli->cb - pli->offszLibs,
                            pliNew->SzLibs(), pli->Cb() * 2 - pliNew->offszLibs);

            if (cb)  {
                pliNew->cb = static_cast<ULONG>((pliNew->SzLibs() - (char *)pliNew) + cb);
                PB pbOutFile = PB(strstr(pliNew->SzCommand(), "out") + 5);
                pliNew->ichOutfile =  static_cast<ULONG>(pbOutFile - (PB)pliNew->SzCommand());

                return pliNew;
            }
        }
    }

    if (pb != NULL) {
        delete []pb;
    }

    return NULL;
}

PLinkInfo DBI1::GetUTF8LinkInfo(PLinkInfoW pli)
{
    USES_STACKBUFFER(0x400);

    // Convert the link info structure from Unicode to UTF8
    size_t cbAlloc = 2 * pli->Cb();     // Cb() returns size in bytes, 2 * cb == 4 * cch
    PB pb = new BYTE[cbAlloc];          
                                    
    if (!pb) {
        return NULL;
    }

    PLinkInfo pliNew = reinterpret_cast<PLinkInfo>(pb);
    
    pliNew->ver      = pli->ver;
    pliNew->offszCwd = sizeof(LinkInfo);
    cbAlloc          = cbAlloc - sizeof(LinkInfo);

    SZ szCwd = _GetSZUTF8FromSZUnicode(pli->SzCwdW(), pliNew->SzCwd(), cbAlloc);
    if (szCwd != NULL) {

        size_t cbCwd = strlen(pliNew->SzCwd()) + 1;
        pliNew->offszCommand = static_cast<ULONG>(pliNew->offszCwd + cbCwd);

        SZ szCommand = _GetSZUTF8FromSZUnicode(pli->SzCommandW(), pliNew->SzCommand(), cbAlloc - cbCwd);
        if (szCommand != NULL)  {

            size_t cbCommand = strlen(szCommand) + 1;
            pliNew->offszLibs = static_cast<ULONG>(pliNew->offszCommand + cbCommand);

            size_t  cbLibs = UnicodeToUTF8(
                pli->SzLibsW(), 
                pliNew->SzLibs(), 
                (cbAlloc - (cbCwd + cbCommand)));

            if (cbLibs)  {
                pliNew->cb = static_cast<ULONG>((pliNew->SzLibs() - reinterpret_cast<char *>(pliNew)) + cbLibs);
                PB pbOutFile = PB(strstr(pliNew->SzCommand(), "out") + 5);
                pliNew->ichOutfile =  static_cast<ULONG>(pbOutFile - reinterpret_cast<PB>(pliNew->SzCommand()));

                return pliNew;
            }
        }
    }

    if (pb != NULL) {
        delete []pb;
    }

    return NULL;
}

BOOL DBI1::AddLinkInfoW(PLinkInfoW pli)
{
    precondition(ppdb1);

    // Convert the string from Unicode to UTF8
    PLinkInfo pliNew = GetUTF8LinkInfo(pli);

    Stream *    pstr;
    BOOL        fRet = FALSE;
    if (ppdb1->OpenStreamEx(c_szLinkInfoStr, pdbExclusive, &pstr)) {
        fRet = pstr->Replace(pliNew, pli->Cb());
        pstr->Release();
    }

    delete [] PB(pliNew);
    return fRet;
}


BOOL DBI1::AddLinkInfo(PLinkInfo pli)
{
    precondition(ppdb1);

    // Convert the string from MBCS to UTF8
    if ((pli = GetUTF8LinkInfo(pli)) == NULL)
        return FALSE;

    Stream *    pstr;
    BOOL        fRet = FALSE;
    if (ppdb1->OpenStreamEx(c_szLinkInfoStr, pdbExclusive, &pstr)) {
        fRet = pstr->Replace(pli, pli->Cb());
        pstr->Release();
    }

    delete [] PB(pli);

    return fRet;
}

//
// Query the link info
//
BOOL DBI1::QueryLinkInfo(PLinkInfo pli, long * pcb)
{
    precondition(ppdb1);

    Stream *    pstr;
    BOOL        fRet = FALSE;
    if (ppdb1->OpenStreamEx(c_szLinkInfoStr, pdbExclusive, &pstr)) {
        CB  cbLinkInfo = pstr->QueryCb();
        PLinkInfo pliOld = NULL, pliNew = NULL;
        if (!IS_SZ_FORMAT_PDB(ppdb1))
        {
            pliOld = (PLinkInfo)new BYTE[cbLinkInfo];

            if (pliOld == NULL) {
                return FALSE;
            }

            if (!pstr->Read2(0, pliOld, cbLinkInfo)) {
                delete [] pliOld;
                return FALSE;
            }

            pliNew = GetUTF8LinkInfo(pliOld);

            if (pliNew == NULL) {
                delete [] PB(pliOld);
                return FALSE;
            }

            cbLinkInfo = pliNew->Cb();
        }

        if (!pli) {
            *pcb = cbLinkInfo;
            fRet = TRUE;
        }
        else if (*pcb < cbLinkInfo) {
            *pcb = cbLinkInfo;
        }
        else {
            // fill in the buffer
            //
            if (!IS_SZ_FORMAT_PDB(ppdb1)) {
                assert(pliNew != NULL);
                memcpy(pli, pliNew, cbLinkInfo);
                fRet = TRUE;
            }
            else
                fRet = pstr->Read2(0, pli, cbLinkInfo);
        }

        if (pliOld) {
            delete [] PB(pliOld);
        }
        if (pliNew) {
            delete [] PB(pliNew);
        }
        pstr->Release();
    }

    return fRet;
}

//
// Write out the MODI info.  Takes care of the case where Win64 has to write
// out Win32 compatible info.  We can change this when we move to v7 and
// write out only the pertinent bits (i.e. non-pointer types) to the MODI strm.
//
BOOL DBI1::fWriteModi (Buffer & buf)
{
    MTS_ASSERT(m_csForMods);

    if (sizeof(MODI) != sizeof(MODI_60_Persist)) {
        // take the hit of conversion
        MSF *   pmsf = ppdb1->pmsf;
        DWORD   rgdw[256];          // suitably aligned buffer for the MODI_60_Persist's
        PMODI60 pmodi60 = PMODI60(rgdw);
        PMODI   pmodi = PMODI(buf.Start());
        PMODI   pmodiEnd = PMODI(PB(pmodi) + dbihdr.cbGpModi);
        CB      cbModi = 0;

        for (; pmodi < pmodiEnd; pmodi = PMODI(pmodi->pbEnd()))
        {
            *pmodi60 = *pmodi;
            assert(pmodi60->pbEnd() - PB(pmodi60) <= sizeof(rgdw));
            CB  cb = CB(pmodi60->pbEnd() - PB(pmodi60));
            cbModi += cb;
            if (!pmsf->AppendStream(snDbi, PB(pmodi60), cb)) {
                // error will be handled appropriately in fSave.
                return FALSE;
            }
        }
        // fix up the header with the proper size
        dbihdr.cbGpModi = cbModi;
        return pmsf->WriteStream(snDbi, 0, &dbihdr, sizeof(dbihdr));
    }
    else {
        return ppdb1->pmsf->AppendStream(snDbi, buf.Start(), dbihdr.cbGpModi);
    }
}

BOOL DBI1::OpenModW(const wchar_t* wszModule, const wchar_t* wszObjFile, OUT Mod** ppmod)
{
    USES_STACKBUFFER(0x400);
    MTS_PROTECT(m_csForMods);

    IMOD imod = imodForModNameW(wszModule, wszObjFile);

    if (imod == imodNil) {
        UTFSZ utfszModule  = GetSZUTF8FromSZUnicode(wszModule);
        UTFSZ utfszObjFile = wszObjFile != NULL ? GetSZUTF8FromSZUnicode(wszObjFile) : NULL;
        if (!utfszModule || (wszObjFile && !utfszObjFile)) 
            return FALSE;

        if (!fCheckReadWriteMode(TRUE))
            return FALSE;

        MODI* pmodi = new (bufGpmodi, utfszModule, utfszObjFile) MODI(utfszModule, utfszObjFile);
        if (!pmodi || !bufRgpmodi.Append((PB)&pmodi, sizeof pmodi)) {
            ppdb1->setOOMError();
            return FALSE;
        }
        imod = imodMac++;
        expect(fAlign(&(rgpmodi[imod])));
        assert(pmodiForImod(imod) == pmodi);
    }

    return openModByImod(imod, ppmod);
}

BOOL DBI1::DeleteModW(const wchar_t* wcsModule)
{
    // We don't have any implememtation
    // for this as yet, just return TRUE
    // See MBCS implementation
    return TRUE;
}

BOOL DBI1::AddPublicW(const wchar_t* wszPublic, USHORT isect, long off, CV_pubsymflag_t cvpsf)
{
    USES_STACKBUFFER(0x400);

    UTFSZ utfszPublic = GetSZUTF8FromSZUnicode(wszPublic);
    if (utfszPublic == NULL)
        return FALSE;
    return AddPublic2(utfszPublic, isect, off, cvpsf);
}

BOOL DBI1::QueryTypeServerByPdbW(const wchar_t* wszPdb, OUT ITSM* pitsm)
{
    USES_STACKBUFFER(0x400);

    UTFSZ utfszPdb = GetSZUTF8FromSZUnicode(wszPdb);
    if (utfszPdb == NULL)
        return FALSE;
    return QueryTypeServerByPdbUTF8(utfszPdb, pitsm);
}

BOOL DBI1::QueryModTypeRef(IMOD imod, MODTYPEREF *pmtr)
{
    MSF *pmsf = ppdb1->pmsf;
    MODI* pmodi = rgpmodi[imod];

    if (pmodi->sn == snNil) {
        return TRUE;
    }

    DWORD cb = pmsf->GetCbStream(pmodi->sn);

    if (cb < sizeof(DWORD) + sizeof(MODTYPEREF)) {
        return TRUE;
    }

    cb = sizeof(MODTYPEREF);

    if (!pmsf->ReadStream(pmodi->sn, sizeof(DWORD), (PB) pmtr, (CB *) &cb) || cb != sizeof(MODTYPEREF)) {
        ppdb1->setReadError();
        return FALSE;
    }

    return TRUE;
}

BOOL Dbg1Data::Close()
{
    MTS_PROTECT(pdbi1->m_csForDbg);

    BOOL fResult = TRUE;

    if (pdbi1->fWrite) {
        assert(refcount == 1);
        fResult = fSave();
    }

    if (--refcount == 0) {
        pdbi1->rgOpenedDbg[iDbg] = NULL;
        delete this;
    }

    return TRUE;
}

long Dbg1Data::QuerySize() {
    return bufDbg.Size() / cbElement;
}

BOOL Dbg1Data::fSave()
{
    MTS_ASSERT(pdbi1->m_csForDbg);

    if (bufDbg.Size()) {
        Sort();
    }
    return pdbi1->fSaveDbgData(iDbg, bufDbg);
}

BOOL Dbg1Data::QueryRange(ULONG iCur, ULONG celt, void *rgelt)
{
    assert(iCur <= (ULONG)QuerySize());
    assert(iCur + celt <= (ULONG)QuerySize());

    PB pbStart = bufDbg.Start() + iCur * cbElement;
    memcpy (rgelt, pbStart, celt * cbElement);
    return TRUE;
}


// 
// Sorts data 
// 
// The sort order is defined by the cmpDbg* functions; these
// support an ascending ordering of Fpo/Func/Omap/XFixup elements
// based on their RVA / offset field
//
BOOL Dbg1Data::Sort()
{
    assert(pdbi1->fWrite);
    assert(refcount == 1);
    if (pfnCmp) {
        qsort(bufDbg.Start(), QuerySize(), cbElement, pfnCmp);
    }
    return TRUE;
}

// 
// Looks up an element based on a key (i.e., RVA or offset)
// If the element is found, *pelt is updated and TRUE is returned
//
BOOL Dbg1Data::Find(void *pelt)
{
    assert(refcount == 1 || !pdbi1->fWrite);

    void *pFound = NULL;
    unsigned int nElements = QuerySize();
    
    if (!pfnCmp) {
        pdbi1->ppdb1->setUsageError();
        return FALSE;
    }

    if (nElements > 0)
        pFound = ::bsearch(pelt, bufDbg.Start(), 
                nElements, cbElement, pfnCmp);

    if (!pFound) {
        return FALSE;
    }

    memcpy(pelt, pFound, cbElement);

    return TRUE;
}

//
// Clears all data in the current Dbg interface
//
BOOL Dbg1Data::Clear()
{
    MTS_PROTECT(m_cs);
    if (!pdbi1->fWrite)
        return FALSE;

    assert(refcount == 1);

    bufDbg.Clear();
    return TRUE;
}

// 
// Appends celt elements to the current data stream
// Does not affect the enumeration index
// Arguments:
//      celt    number of DbgdataType elements to be appended
//      rgelt   array containing the elements to be appended
//              
//
BOOL Dbg1Data::Append(
    ULONG celt, 
    const void *rgelt
    )
{
    MTS_PROTECT(m_cs);
    if (!pdbi1->fWrite)
        return FALSE;

    assert(refcount == 1);

    CB cb = celt * cbElement;
    if (!bufDbg.Append(PB(rgelt), cb, 0)) {
        return FALSE;
    }

    return TRUE;
}


//
// Replaces next celt elements with those contained in rgelt
// Does not affect the enumeration index
// Arguments:
//      celt    number of DbgdataType elements to be replaced
//      rgelt   array containing the new elements
//
BOOL Dbg1Data::Replace(
    ULONG iCur,
    ULONG celt, 
    const void *rgelt
    )
{
    MTS_PROTECT(m_cs);
    assert(iCur <= (ULONG)QuerySize());
    assert(iCur + celt <= (ULONG)QuerySize());
    if (!pdbi1->fWrite)
        return FALSE;
    PB pbStart = bufDbg.Start() + iCur * cbElement;
    memcpy (pbStart, rgelt, celt * cbElement);

    return TRUE;
}

BOOL
DBI1::FAddSourceMappingItem(
        const wchar_t * szMapTo,
        const wchar_t * szMapFrom,
        ULONG           grFlags
        )
{
    assert(grFlags == 0);

    assert(szMapTo != NULL);
    assert(*szMapTo != L'\0');

    assert(szMapFrom != NULL);
    assert(*szMapFrom != L'\0');

    SrcMap  srcmap = {
        _wcsdup(szMapFrom),
        wcslen(szMapFrom),
        _wcsdup(szMapTo),
        wcslen(szMapTo),
        grFlags
    };

    if (srcmap.szFrom != NULL && srcmap.szTo != NULL) {
        if (m_rgsrcmap.append(srcmap)) {
            return TRUE;
        }
    }

    // don't leak memory
    free(srcmap.szFrom);
    free(srcmap.szTo);

    ppdb1->setOOMError();
    return FALSE;
}

bool
DBI1::fSrcMap(const wchar_t * szFile, Buffer & bufOut) {

    bool        fRet = false;   // no mapping done
    unsigned    isrcmapMax = m_rgsrcmap.size();

    for (unsigned isrcmap = 0; isrcmap < isrcmapMax; isrcmap++) {
        const SrcMap &  srcmap = m_rgsrcmap[isrcmap];
        if (_wcsnicmp(szFile, srcmap.szFrom, srcmap.cchszFrom) == 0) {
            wchar_t *   pwch;
            size_t cch = wcslen(szFile) - srcmap.cchszFrom + srcmap.cchSzTo + 1;
            if (bufOut.ResetAndReserve(
                    CB(sizeof(wchar_t) * cch),
                    reinterpret_cast<PB*>(&pwch))
               )
            {
                wcscpy_s(pwch, cch, srcmap.szTo);
                wcscpy_s(pwch + srcmap.cchSzTo, cch - srcmap.cchSzTo, szFile + srcmap.cchszFrom);
                fRet = true;    // we are done, signal that we did something
            }
            else {
                ppdb1->setOOMError();
            }
            
            // Matched one, so we are done
            break;
        }
    }

    return fRet;
}

BOOL
DBI1::FSetPfnNotePdbUsed(void * pvContext, DBI::PFNNOTEPDBUSED pfn) {
    m_dbicbc.pvContext = pvContext;
    m_dbicbc.pfnNotePdbUsed = pfn;
    return TRUE;
}

BOOL
DBI1::FSetPfnNoteTypeMismatch(void * pvContext, DBI::PFNNOTETYPEMISMATCH pfn) {
    m_dbicbc.pvContext = pvContext;
    m_dbicbc.pfnNoteTypeMismatch = pfn;
    return TRUE;
}

BOOL
DBI1::FSetPfnTmdTypeFilter(void * pvContext, DBI::PFNTMDTYPEFILTER pfn) {
    m_dbicbc.pvContext = pvContext;
    m_dbicbc.pfnTmdTypeFilter = pfn;
    return TRUE;
}

BOOL
DBI1::FSetPfnQueryCallback(void *pvContext, PFNDBIQUERYCALLBACK pfn) {
    if (m_dbicbc.pvContext != NULL && m_dbicbc.pvContext != pvContext) {
        // You can't call FSetPfnNotePdbUsed with a different context
        return FALSE;
    }

    m_dbicbc.pvContext = pvContext;
    m_dbicbc.pfnQueryCallback = pfn;

    m_dbicbc.pfnNotePdbUsed = (PFNNOTEPDBUSED)(*m_dbicbc.pfnQueryCallback)(pvContext, dovcNotePdbUsed);
    m_dbicbc.pfnNoteTypeMismatch = (PFNNOTETYPEMISMATCH)(*m_dbicbc.pfnQueryCallback)(pvContext, dovcNoteTypeMismatch);
    m_dbicbc.pfnTmdTypeFilter = (PFNTMDTYPEFILTER)(*m_dbicbc.pfnQueryCallback)(pvContext, dovcTmdTypeFilter);
    
    return TRUE;
}

#define GET_UDT_SRC_LINE(ptm, ti, wszSrc, line)  {  \
    wszSrc = NULL;  \
    char *sz = NULL;  \
    wchar_t *wsz = NULL;  \
    if (ptm->QuerySrcLineForUDT(ti, &sz, &line)) {  \
        wsz = GetSZUnicodeFromSZUTF8(sz);  \
        size_t cch = wcslen(wsz) + 1;  \
        wszSrc = new (poolTmd) wchar_t[cch];  \
        if (wszSrc == NULL) {  \
            ppdb1->setOOMError();  \
            return FALSE;  \
        }  \
        wcscpy_s(wszSrc, cch, wsz);  \
        delete [] sz;  \
    }  \
}

BOOL
DBI1::FTypeMismatchDetection(SZ szUdt, TI tiUdt, LHASH hash)
{
    MTS_PROTECT(m_csForTmd);

    USES_STACKBUFFER(0x100);

    if (m_dbicbc.pfnTmdTypeFilter != NULL) {
        // Check whether to skip this UDT.

        wchar_t *wszUdt = GetSZUnicodeFromSZUTF8(szUdt);

        if (m_dbicbc.pfnTmdTypeFilter(m_dbicbc.pvContext, wszUdt)) {
            return TRUE;
        }
    }

    TPI *ptpi = GetTpi();
    assert(ptpi != NULL);

    // Locate the right element (with same name) in the bucket.

    for (PTMD ptmd = *(m_rgpTmdBuckets + hash); ptmd != NULL; ptmd = ptmd->next) {
        PB pb = NULL;
        if (!ptpi->QueryPbCVRecordForTi(ptmd->pInfo->ti, &pb)) {
            return FALSE;
        }

        SZ sz = szUDTName(pb);
        assert(sz != NULL);

        if (strcmp(szUdt, sz) != 0) {
            continue;
        }
    
        // Found a mismatch.  Chain this type info into the bucket,
        // so we won't report same mismatch again.

        PTMD_INFO p = new (poolTmd) TMD_INFO;

        if (p == NULL) {
            ppdb1->setOOMError();
            return FALSE;
        }

        GET_UDT_SRC_LINE(m_tmdPtm, m_tmdTiObj, p->wszSrc, p->line);

        p->ti = tiUdt;
        p->imod = m_tmdImod;
        p->next = ptmd->pInfo;
        ptmd->pInfo = p;
    
        return TRUE;
    }

    // The bucket is empty or doesn't contain same named type.
    // Chain this type info into the bucket.

    PTMD         p = new (poolTmd) TMD;
    PTMD_INFO    pinfo = new (poolTmd) TMD_INFO;

    if (p == NULL || pinfo == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    GET_UDT_SRC_LINE(m_tmdPtm, m_tmdTiObj, pinfo->wszSrc, pinfo->line);

    pinfo->ti = tiUdt;
    pinfo->imod = m_tmdImod;
    pinfo->next = NULL;

    p->pInfo = pinfo;
    p->next = *(m_rgpTmdBuckets + hash);
    *(m_rgpTmdBuckets + hash) = p;

    return TRUE;
}

#define GET_FULL_MOD_NAME(imod, wsz)  {  \
    SZ sz1 = szModule(imod);  \
    SZ sz2 = szObjFile(imod);  \
    if (strcmp(sz1, sz2) == 0) {  \
        wsz = GetSZUnicodeFromSZUTF8(sz1);  \
    }  \
    else {  \
        size_t cch = strlen(sz1) + strlen(sz2) + 4;  \
        SZ sz = new char[cch];  \
        if (sz == NULL) {  \
            ppdb1->setOOMError();  \
            return FALSE;  \
        }  \
        strcpy_s(sz, cch, sz1);  \
        strcat_s(sz, cch, " (");  \
        strcat_s(sz, cch, sz2);  \
        strcat_s(sz, cch, ")");  \
        wsz = GetSZUnicodeFromSZUTF8(sz);  \
        delete [] sz;  \
    }  \
}

BOOL DBI1::ReportTypeMismatches()
{
    if (!fWrite) {
        return TRUE;
    }
    
    if (!ppdb1->FCheckTypeMismatch()) {
        return TRUE;
    }

    if (m_dbicbc.pfnNoteTypeMismatch == NULL) {
        return TRUE;
    }

    TPI  * ptpi = GetTpi();
    TPI1 * ptpi1 = (TPI1 *) ptpi;
    Buffer buf;

    assert(ptpi != NULL);

    for (DWORD hash = 0; hash < ptpi1->cchnV8; hash++) {
        for (PTMD ptmd = *(m_rgpTmdBuckets + hash); ptmd != NULL; ptmd = ptmd->next) {
            if (ptmd->pInfo->next == NULL) {
                continue;
            }

            PB pb = NULL;
            if (!ptpi->QueryPbCVRecordForTi(ptmd->pInfo->ti, &pb)) {
                return FALSE;
            }

            SZ sz = szUDTName(pb);
            assert(sz != NULL);

            USES_STACKBUFFER(0x400);

            wchar_t *wszUdt = GetSZUnicodeFromSZUTF8(sz);

            buf.Clear();

            for (PTMD_INFO pinfo = ptmd->pInfo; pinfo != NULL; pinfo = pinfo->next) {
                wchar_t *wszMod = NULL;
                GET_FULL_MOD_NAME(pinfo->imod, wszMod);

                if (!buf.Append((PB)(L"\n  '"), 8) ||
                    !buf.Append((PB) wszMod, static_cast<CB>(sizeof(wchar_t) * wcslen(wszMod))) ||
                    !buf.Append((PB)(L"'"), 2)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }

                if (pinfo->wszSrc != NULL) {
                    wchar_t wszTmp[20];
                    swprintf_s(wszTmp, _countof(wszTmp), L"' line %d)", pinfo->line);

                    if (!buf.Append((PB)(L" ('"), 6) ||
                        !buf.Append((PB) pinfo->wszSrc, static_cast<CB>(sizeof(wchar_t) * wcslen(pinfo->wszSrc))) ||
                        !buf.Append((PB) wszTmp, static_cast<CB>(sizeof(wchar_t) * wcslen(wszTmp)))) {
                        ppdb1->setOOMError();
                        return FALSE;
                    }
                }
            }

            if (!buf.Append((PB)(L"\n"), 4)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            (*m_dbicbc.pfnNoteTypeMismatch)(m_dbicbc.pvContext, wszUdt, (wchar_t *) buf.Start());
        }
    }

    return TRUE;
}
