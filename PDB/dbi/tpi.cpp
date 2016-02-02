#include "pdbimpl.h"
#include "dbiimpl.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>

#define TPI_IS_FUNCTION(l)     ((l) == LF_MFUNCTION     || \
                                (l) == LF_METHOD        || \
                                (l) == LF_ONEMETHOD     || \
                                (l) == LF_FRIENDFCN     || \
                                (l) == LF_METHODLIST)



BOOL fUDTAnon(PTYPE ptype);

// compile time assert
#if !defined(cassert)
#define cassert(x) extern char dummyAssert[ (x) ]
#endif

const HDR   hdrNew = {
    TPI1::impv80,
    sizeof(HDR),
    tiMin,
    tiMin,
    0,
    {
        snNil, snNil,       // sn, snPad
        sizeof(LHASH),      // cbHashKey
        TPI1::cchnV8,       // cHashBuckets
        {0, -1},            // offcbHashVals
        {0, -1},            // offcbTiOff
        {0, -1}             // offcbHashAdj
    }
};

#if defined(_DEBUG)
void hexDump(const char * szTitle, PB pb, CB cb)
{
    printf ( "\t%s\n", szTitle );
    for ( int ib = 0; ib < cb; ib += 16 ) {
        printf ( "\t%p:  ", pb + ib );
        for ( int ib2 = 0; ib2 < 16 && ib + ib2 < cb; ib2++ ) {
            printf ( "%02x ", *(pb + ib + ib2) );
        }
        printf ( "\n" );
    }
}
#endif


// we rely upon the fact that these two structures, while
// different, actually have the same size.
cassert(sizeof TI_OFF_16t == sizeof TI_OFF);
cassert(offsetof(TI_OFF_16t,ti) == offsetof(TI_OFF,ti));
cassert(offsetof(TI_OFF_16t,off) == offsetof(TI_OFF,off));

HDR &
HDR::operator=(const HDR_16t & hdr16) {
    vers = hdr16.vers;
    cbHdr = sizeof(HDR_16t);
    tiMin = hdr16.tiMin;
    tiMac = hdr16.tiMac;
    cbGprec = hdr16.cbGprec;
    tpihash.sn = hdr16.snHash;
    tpihash.snPad = snNil;
    tpihash.cbHashKey = sizeof(HASH);
    tpihash.cHashBuckets = TPI1::cchnV7;
    tpihash.offcbHashVals.off = 0;
    tpihash.offcbHashVals.cb = (tiMac - tiMin) * sizeof(HASH);
    tpihash.offcbTiOff.off = tpihash.offcbHashVals.cb;
    tpihash.offcbTiOff.cb = -1; // don't know the value of this yet!
    tpihash.offcbHashAdj = hdrNew.tpihash.offcbHashAdj; // not present
    return *this;
    }

HDR &
HDR::operator=(const HDR_VC50Interim & hdrvc50) {
    vers = hdrvc50.vers;
    cbHdr = sizeof(HDR_VC50Interim);
    tiMin = hdrvc50.tiMin;
    tiMac = hdrvc50.tiMac;
    cbGprec = hdrvc50.cbGprec;
    tpihash.sn = hdrvc50.snHash;
    tpihash.snPad = snNil;
    tpihash.cbHashKey = sizeof(HASH);
    tpihash.cHashBuckets = TPI1::cchnV7;
    tpihash.offcbHashVals.off = 0;
    tpihash.offcbHashVals.cb = (tiMac - tiMin) * sizeof(HASH);
    tpihash.offcbTiOff.off = tpihash.offcbHashVals.cb;
    tpihash.offcbTiOff.cb = -1; // don't know the value of this yet!
    tpihash.offcbHashAdj = hdrNew.tpihash.offcbHashAdj; // not present
    return *this;
    }

TPI1::TPI1(MSF* pmsf_, PDB1* ppdb1_, SN sn_) {
    PDBLOG_FUNC();
    assert(ppdb1_);
    assert((sn_ == snTpi) || (sn_ == snIpi));

    pmsf = pmsf_;
    ppdb1 = ppdb1_;
    mphashpchn = 0;
    pblkPoolCommit = 0;
    fWrite = FALSE;
    fGetTi = FALSE;
    fGetCVRecords = FALSE;
    fAppend = FALSE;
    fInitd = FALSE;
    fInitResult = FALSE;
    cbMapHashCommit = 0;
    fReplaceHashStream = FALSE;
    cbHdr = sizeof HDR;
    f16bitPool = FALSE;
    pwti = 0;
    pnamemap = NULL;
    m_pfnHashTpiRec = NULL;
    m_sn = sn_;
#ifdef PDB_TYPESERVER
    itsm = 0;
    pdbi = 0;
#endif // PDB_TYPESERVER
#ifdef HASH_REPORT
    cRecComparisons = 0;
    cRecUDTComparisons = 0;
#endif
}

INTV TPI1::QueryInterfaceVersion()
{
    PDBLOG_FUNC();
    return intv;
}

IMPV TPI1::QueryImplementationVersion()
{
    PDBLOG_FUNC();
    return curImpv;
}

void* REC::operator new(size_t size, TPI1* ptpi1, PB pb) {
    // because size of empty struct is one, and REC is empty, we should ignore size
    MTS_ASSERT(ptpi1->m_cs);
    return new (ptpi1->poolRec) BYTE[cbForPb(pb)];
}

#ifdef PDB_DEFUNCT_V8
void* REC::operator new(size_t size, TPI1* ptpi1, PC8REC pc8rec) {
    return new (ptpi1->poolC8Rec) BYTE[cbForPb(pc8rec->buf)];
}
#endif

// validate and return an ACceSs Level regarding the current header
TPI1::ACSL TPI1::acslValidateHdr() const {
    PDBLOG_FUNC();

    switch (hdr.vers) {
    case curImpv:
        if (hdr.tpihash.cbHashKey == sizeof(SIG) &&
            hdr.tpihash.cHashBuckets >= cchnV7 &&
            hdr.tpihash.cHashBuckets < cchnMax)
        {
            m_pfnHashTpiRec = &TPI1::hashBufv8;
            return acslReadWrite;
        }
        else {
            return acslNone;
        }

    case impv70:
    case impv50:
        if (hdr.tpihash.cbHashKey == sizeof(HASH) &&
            hdr.tpihash.cHashBuckets == cchnV7)
        {
            m_pfnHashTpiRec = &TPI1::hashBuf;
            return acslReadWrite;
        }
        else {
            return acslNone;
        }

    case impv50Interim:
        m_pfnHashTpiRec = &TPI1::hashBuf;
        return acslReadWriteConvert;

    case impv41:
    case impv40:
    case intvVC2:
        m_pfnHashTpiRec = &TPI1::hashBuf;
        return acslRead;

    default:
        return acslNone;
    }
}

BOOL TPI1::fOpen(SZ_CONST szMode) {
    PDBLOG_FUNC();
    PDBLOG_FUNCARG(szMode);

    if (this == ppdb1->ptpi1) {
        MTS_ASSERT(ppdb1->m_csForTPI);
    }
    else {
        assert(this == ppdb1->pipi);
        MTS_ASSERT(ppdb1->m_csForIPI);
    }

    MTS_PROTECT(m_cs);
    assert(pmsf);

    fGetCVRecords = fGetTi = fAppend = FALSE;
    fEnableQueryTiForUdt = TRUE;
    for (; *szMode; szMode++) {
        switch (*szMode) {
            case 'i':   fGetTi = TRUE;                  break;
            case 'c':   fGetCVRecords = TRUE;           break;
            case 'w':   fWrite = TRUE;                  break;
            case 'a':   fAppend = TRUE;                 break;
            default:    fGetTi = fGetCVRecords = TRUE;  break;
        }
    }

    // Load the database (if it exists).  If the database does not exist,
    // try to create one iff we're in write mode.

    if (pmsf->GetCbStream(m_sn) != 0) {
        if (!ppdb1->m_fMinimalDbgInfo || !fWrite) {
            return fLoad();
        }

        // Read the header

        CB cbHdrT = sizeof hdr;

        if (!pmsf->ReadStream(m_sn, 0, &hdr, &cbHdrT) || (cbHdrT < cbHdrMin)) {
            ppdb1->setReadError();
            return FALSE;
        }

        if (acslValidateHdr() != acslReadWrite) {
            // only care about the latest version

            ppdb1->setLastError(EC_FORMAT);
            return FALSE;
        }

        // Clear TPI streams for mini PDB

        if (!pmsf->ReplaceStream(m_sn, 0, 0) ||
            ((hdr.tpihash.sn != snNil) && !pmsf->DeleteStream(hdr.tpihash.sn))) {
            ppdb1->setWriteError();
            return FALSE;
        }
    }

    return fWrite && fCreate();
}


// only calls from TPI1::fOpen()
BOOL TPI1::fLoad() {
    PDBLOG_FUNC();

    if (this == ppdb1->ptpi1) {
        MTS_ASSERT(ppdb1->m_csForTPI);
    }
    else {
        assert(this == ppdb1->pipi);
        MTS_ASSERT(ppdb1->m_csForIPI);
    }

    MTS_ASSERT(m_cs);
    dassert(pmsf);

    // try to read in our current header size; may fail to read in that much...
    CB cbHdrT = sizeof hdr;
    if (!(pmsf->ReadStream(m_sn, 0, &hdr, &cbHdrT) && cbHdrT >= cbHdrMin) ) {
        ppdb1->setReadError();
        return FALSE;
    }

    // check to see what we can do with the version we loaded.
    ACSL acsl = acslValidateHdr();

    if ( acsl == acslNone || (fWrite && acsl == acslRead) ) {
        ppdb1->setLastError(EC_FORMAT);
        return FALSE;
    }

    if (acsl == acslReadWriteConvert) {
        if (hdr.vers == impv50Interim) {
            // we need to load the entire REC into memory and mark it as dirty
            // before we go ahead and fix up the header and replace the m_sn
            // with just the header.  This sets up for a potential full read
            // of the records in fInitReally()
            HDR_VC50Interim hdrT = *(HDR_VC50Interim*)&hdr;
            hdr = hdrT;
            // fix up the one field that the hdr copy ctor cannot handle
            CB  cbHashStream;
            if (snHash() == snNil) {
                assert(cbHashValues() == 0);
                cbHashStream = 0;
            }
            else {
                if (cbNil == (cbHashStream = pmsf->GetCbStream(snHash()))) {
                    cbHashStream = 0;
                }
            }
            hdr.tpihash.offcbTiOff.cb = cbHashStream - cbHashValues();
            cbHdr = sizeof HDR_VC50Interim;
        }
        else {
            ppdb1->setLastError(EC_FORMAT);
            return FALSE;
        }
    }
    else if (hdr.vers <= impv41) {
        // have to convert the records from 16 to 32 bits and normalize
        // the header
        f16bitPool = TRUE;
        cbHdr = sizeof HDR_16t;
        HDR_16t hdrT = *(HDR_16t*)&hdr;
        hdr = hdrT;
        // fix up the one field that the hdr copy ctor cannot handle
        CB  cbHashStream;
        if (snHash() == snNil) {
            assert(cbHashValues() == 0);
            cbHashStream = 0;
        }
        else {
            if (cbNil == (cbHashStream = pmsf->GetCbStream(snHash()))) {
                cbHashStream = 0;
            }
        }
        hdr.tpihash.offcbTiOff.cb = cbHashStream - cbHashValues();

        // create our WidenTi object, initial size is set to be size
        // of record stream / 16.
        if (!WidenTi::fCreate(pwti, cbGpRec() >> 4)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }
    else {
        f16bitPool = FALSE;
    }

    // for read/write files, we'll fInit immediately;
    // for read-only files, we won't fInit until we have to
    if (fWrite
        // !!! HACK HACK HACK !!!
        // See comments in PDB1::OpenIpi() in dbi\pdb.cpp.
        || (ppdb1->pipi == this && !ppdb1->fIsPdb110()))
    {
        BOOL    fRet = fInit();

        // code relies upon fInitReally() to call fInitTiToPrecMap().
        if (fRet && acsl == acslReadWriteConvert) {
            // at this juncture, we need to fixup our read-in records
            // to look as if they are new instead of already committed.
            poolRec.xferPool(poolRecClean);

            assert(poolRecClean.cbTotal == 0);

            // now we need to fix our header so that we are a new version
            // and look like we need to write out all the records.
            hdr.cbGprec = 0;
            hdr.cbHdr = sizeof hdr;
            cbHdr = sizeof hdr;
            cbClean = 0;
            if (!(fRet = pmsf->ReplaceStream(m_sn, &hdr, sizeof hdr))   )
                ppdb1->setWriteError();
        }
        return fRet;
    }
    else
        return TRUE;
}

// only calls from TPI1::fOpen()
BOOL TPI1::fCreate() {
    PDBLOG_FUNC();

    if (this == ppdb1->ptpi1) {
        MTS_ASSERT(ppdb1->m_csForTPI);
    }
    else {
        assert(this == ppdb1->pipi);
        MTS_ASSERT(ppdb1->m_csForIPI);
    }

    MTS_ASSERT(m_cs);
    dassert(pmsf);
    hdr = hdrNew;
    assert(cbGpRec() == 0);
    if (!pmsf->ReplaceStream(m_sn, &hdr, sizeof hdr)) {
        ppdb1->setWriteError();
        return FALSE;
    }

    return fInit();
}

inline BOOL TPI1::fInit() { 
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);
    if (fInitd) {
        return fInitResult;
    } else {
        fInitd = TRUE;
        return fInitResult = fInitReally();
    }
}

BOOL TPI1::fInitReally() {
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    dassert(pmsf);

    // allocate TI=>PREC and TI->OFF data structures
    unsigned    cprec = tiMac() - ::tiMin + (fWrite ? cprecInit : 0);

    // init original length of TI stream, to handle multiple commits
    cbClean = cbGpRec();

    fEnableQueryTiForUdt &= (hdr.vers >= impv41) || fWrite;
    fGetTi |= fEnableQueryTiForUdt;

    // even if fGetCVRecords = 0, we need to have mptiprec
    // since CHNs no longer have prec.
    if (!mptiprec.growMaxSize(cprec) ||
        !mptiprec.setSize(tiMac() - ::tiMin)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    mptiprec.fill(PRECEX());

    // new PDBs have <TI, OFF> tuples
    if (hdr.vers >= impv40 && !fLoadTiOff()) {
        return FALSE;
    }

    // done if no writes to be done
    expect ( hdr.vers < impv40 || ppdb1->m_fFullBuild || fWrite || fGetTi );

    // init the prec mapping for vc2 and vc4.0 type pdbs only, or client wants us to prefetch.
    if (hdr.vers < impv70 || (acslValidateHdr() == acslReadWriteConvert && fWrite) || ppdb1->m_fFullBuild || ppdb1->m_fPrefetching) {
        if (!fInitTiToPrecMap())
            return FALSE;
    }

    // init hash to pchn mapping
    if (!fInitHashToPchnMap())
        return FALSE;

#ifdef HASH_REPORT
    // Print the statistical info about the pCHN
    wchar_t szPdbName[_MAX_PATH];
    wprintf(
        L"\nTPI Hash Report for %s.\n",
        ppdb1->QueryPDBNameExW(szPdbName, _countof(szPdbName))
        );

    int rgcitemBuckets[1024];
    int cTypeOverflow = 0;
    int nChnMax = 0;
    int iBucket = 0;
    for(int i1=0; i1<_countof(rgcitemBuckets); i1++)
        rgcitemBuckets[i1] = 0;

    for(unsigned h = 0; h < hdr.tpihash.cHashBuckets; h++) {
        PCHN pchn = mphashpchn[h];
        int nChn;

        for(nChn = 0; pchn != NULL; nChn++)
            pchn = pchn->pNext;

        if (nChn > _countof(rgcitemBuckets) - 1) {
            rgcitemBuckets[_countof(rgcitemBuckets) - 1]++;
            cTypeOverflow += nChn;
        }
        else {
            rgcitemBuckets[nChn]++;
        }
        if (nChn > nChnMax) {
            nChnMax = nChn;
            iBucket = h;
        }
    }

    printf("\nTPI:cchnMax = %d, ti/bucket = %.2f, max items in bucket = %d, bucket #%d\n",
        hdr.tpihash.cHashBuckets, double(tiMac() - tiMin())/ hdr.tpihash.cHashBuckets,
        nChnMax, iBucket );
    printf("#Types\t\t#Buckets\t#Total types\tCumulative types\n");

    int cCumulative = 0;
    int i;
    for(i=0; i<_countof(rgcitemBuckets) - 1; i++) {
        if (rgcitemBuckets[i] != 0) {
            cCumulative += i * rgcitemBuckets[i];
            printf("%8d\t%8d\t%8d\t%8d\n", i, rgcitemBuckets[i], i * rgcitemBuckets[i], cCumulative);
        }
    }
    if (rgcitemBuckets[_countof(rgcitemBuckets) - 1] != 0) {
        printf("%8d\t%8d\t%8d\t%8d\n", i, rgcitemBuckets[i], cTypeOverflow, cCumulative + cTypeOverflow);
    }

    printf("Largest bucket (%d (0x%08x)) dump by TI:\n", iBucket, iBucket);

    for (PCHN pchnT = mphashpchn[iBucket]; pchnT != NULL; pchnT = pchnT->pNext) {
        printf("\t0x%08x\n", pchnT->ti);
    }

    fflush(stdout);

#endif

    return TRUE;
}

BOOL TPI1::fInitHashToPchnMap () {
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);
    if (!fGetTi)
        return TRUE;

    // allocate hash(PREC)=>PREC data structures
    if (!(mphashpchn = new (zeroed) PCHN[hdr.tpihash.cHashBuckets])) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if ((hdr.vers == impv40) && fRehashV40ToPchnMap())
        return TRUE;

    if (snHash() != snNil) {
        int cti = tiMac() - tiMin();

        // read in the previous hash value stream
        CB      cbhash = cbHashValues();
         if (cbhash < 0) {
            ppdb1->setCorruptError();
            return FALSE;
        }
        if (cbhash > 0)
        {
            Buffer  bufHash(0, 0, false);
            union {
                PB      pbPrevHash;
                PV      pvPrevHash;
                HASH *  mpPrevHash;
                LHASH * mpPrevLHash;
            };

            if (!bufHash.ReserveNoZeroed(cbhash, &pbPrevHash)) {
                ppdb1->setOOMError();
                return FALSE;
            }

            if (!(pmsf->ReadStream(snHash(), offHashValues(), pvPrevHash, &cbhash) &&
                cbhash == cbHashValues()))
            {
                ppdb1->setReadError();
                return FALSE;
            }

            // build the hash to chn map
            TI tiMacT = tiMac();
            if (hdr.tpihash.cbHashKey == 2) {
                for (TI ti = tiMin(); ti < tiMacT; ti++) {
                    LHASH hash =
                        (hdr.vers < impv70) ? 
                        hashPrec(precForTi(ti).prec) : 
                        mpPrevHash[ti - tiMin()];
                
                    if (hash < 0 || hash >= hdr.tpihash.cHashBuckets) {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }

                    PCHN* ppchnHead = &mphashpchn[hash];
                    PCHN pchn = new (poolChn) CHN(*ppchnHead, ti);
                    *ppchnHead = pchn;
                }
            }
            else {
                assert(hdr.tpihash.cbHashKey == 4);

                for (TI ti = tiMin(); ti < tiMacT; ti++) {
                    LHASH hash =
                        (hdr.vers < impv70) ? 
                        hashPrec(precForTi(ti).prec) : 
                        mpPrevLHash[ti - tiMin()];
                
                    if (hash < 0 || hash >= hdr.tpihash.cHashBuckets) {
                        ppdb1->setCorruptError();
                        return FALSE;
                    }

                    PCHN* ppchnHead = &mphashpchn[hash];
                    PCHN pchn = new (poolChn) CHN(*ppchnHead, ti);
                    *ppchnHead = pchn;
                }
            }

            bufHash.Free();
        }

        // read in any adjustment values that tell us which records need
        // to be at the head of a hash chain.
        CB  cbhashadj = cbHashAdj();

        if (cbhashadj > 0) {
            Buffer  buf(0, 0, false);
            if (!buf.ReserveNoZeroed(cbhashadj)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            if (!(pmsf->ReadStream(snHash(), offHashAdj(), buf.Start(), &cbhashadj) &&
                cbhashadj == cbHashAdj()) ) {

                ppdb1->setReadError();
                return FALSE;
            }

            if (!getNameMap()) {
                ppdb1->setOOMError();
                return FALSE;
            }

            PB  pb = buf.Start();
            mpnitiHead.reload(&pb, cbhashadj);

            if (pb != buf.End()) {
                ppdb1->setCorruptError();
                return FALSE;
            }

            EnumMap<NI,TI,HcNi> e(mpnitiHead);
            while (e.next()) {
                NI      ni;
                TI      tiHead;
                PCHN    pchn;
                PCHN    pchnPrev = NULL;

                e.get(&ni, &tiHead);

                // get the hash value from the name
                SZ_CONST    sz;
                pnamemap->getName(ni, &sz);
                if (!sz || (*sz == '\0')) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }

                LHASH       hash = hashSz(sz);

                // find the ti we want to be at the head of the chain
                // and put it there.
                for (pchn = mphashpchn[hash]; pchn; pchnPrev = pchn, pchn = pchn->pNext) {
                    if (pchn->ti == tiHead) {
                        if (!pchnPrev) {
                            // the ti is already at the head of the chain.
                            break;
                        }
                        pchnPrev->pNext = pchn->pNext;
                        pchn->pNext = mphashpchn[hash];
                        mphashpchn[hash] = pchn;
                        break;
                    }
                }

                if (!pchn) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }

                assert(mphashpchn[hash]->ti == tiHead);
            }
            // get rid of the name map if we are not writing
            if (!fWrite) {
                pnamemap->close();
                pnamemap = 0;
            }
        }
    }

    // Update the version to impv70 if less than that.
    if (hdr.vers < impv70) {
        hdr.vers = impv70;
    }

    return TRUE;
}

// keep this api we probably want it later

BOOL TPI1::fRehashV40ToPchnMap()
{
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    assert(hdr.vers == impv40);
    assert(m_pfnHashTpiRec != NULL);

    if (!fWrite)
        return FALSE;

    if (!bufMapHash.SetInitAlloc((tiMac() - tiMin()) * sizeof (HASH))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    // build the hash to chn map
    for (TI ti = tiMin(); ti < tiMac(); ti++) {
        PRECEX    prec = precForTi(ti);
        if (!prec.prec)
            return FALSE;
        HASH   hash = static_cast<HASH>(hashPrec(prec.prec));
        assert(hash >= 0 && hash < hdr.tpihash.cHashBuckets);
        PCHN* ppchnHead = &mphashpchn[hash];
        PCHN pchn = new (poolChn) CHN(*ppchnHead, ti);
        *ppchnHead = pchn;
        // store new hash value - we will use this to update the hash stream later
        verify (bufMapHash.Append((PB) &hash, sizeof (HASH)));
    }

    fReplaceHashStream = TRUE;
    hdr.vers = impv70;
    return TRUE;
}

BOOL TPI1::fInitTiToPrecMap () {
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    // allocate and read all the records at once
    PREC prec = (PREC) new (poolRecClean) BYTE[cbGpRec()];
    if (!prec) {
        ppdb1->setOOMError();
        return FALSE;
    }

    CB cb = cbGpRec();
    if (!(pmsf->ReadStream(m_sn, cbHdr, prec, &cb) && cb == cbGpRec())) {
        ppdb1->setReadError();
        return FALSE;
    }

    // Try to catch a corrupt pdb file.  See if the ti chain takes us further than
    //   cbGpRec() bytes.
    size_t  cbProcessed = 0;

    // build the ti to prec map
    for (TI ti = tiMin(); ti < tiMac(); ti++) {
        dassert(prec);
        mptiprec[ti - ::tiMin] = prec;

        size_t  cbRec = prec->cbForPb(PB(prec));

        cbProcessed += cbRec;

        assert(cbProcessed <= size_t(cb));

        if (cbProcessed > size_t(cb)) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        // Convert the type record
        if (impv50 <= hdr.vers && hdr.vers < impv70 && !fConvertTypeRecStToSz((PTYPE)prec)) {
            ppdb1->setLastError(EC_FORMAT);
            return FALSE;
        }

        prec = reinterpret_cast<PREC>(PB(prec) + cbRec);
    }

    return TRUE;
}

// only called from TPI1::fInitReally()
BOOL TPI1::fLoadTiOff () {
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    if (snHash() == snNil) {
        return TRUE;
    }

    // alloc space & read in <ti, off> tuples
    OFF off = offTiOff();
    CB  cb = cbTiOff();

    tiCleanMac = tiMac();
    cTiOff = cb / sizeof TI_OFF;

    assert((CB)(cTiOff * sizeof TI_OFF) == cb);
    if ((CB)(cTiOff * sizeof TI_OFF) != cb) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    // no type records to begin with
    if (!cTiOff) {
        return TRUE;
    }

    if (!bufTiOff.Reserve(cb)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!(pmsf->ReadStream(snHash(), off, bufTiOff.Start(), &cb) &&
        cb == cbTiOff())) {
        ppdb1->setReadError();
        return FALSE;
    }

    if (f16bitPool) {
        // SLEAZE ALERT:zero out the high order words of the ti_off pairs if
        // we are loading a 16-bit pool...
        TI_OFF *    ptioff = (TI_OFF*)bufTiOff.Start();
        TI_OFF *    ptioffMax = ptioff + cTiOff;
        while ( ptioff < ptioffMax ) {
            ptioff->ti &= 0xffff;
            ptioff++;
        }
    }

    // set last pair of <TI, OFF> values
    TI_OFF *    ptioffLast = ((TI_OFF*) bufTiOff.Start()) + cTiOff - 1;
    tioffLast.ti  = ptioffLast->ti;
    tioffLast.off = ptioffLast->off;
    return TRUE;
}


BOOL TPI1::fIsModifiedClass(PTYPE ptA, PTYPE ptB)
{
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);
    for(TypeTiIter tiiA(ptA), tiiB(ptB); 
                    tiiA.next() && tiiB.next(); )  {

        if (!fIsMatchingType(tiiA.rti(), tiiB.rti()))
            return FALSE;
    }

    return TRUE;
}

BOOL TPI1::fIsMatchingType(TI tiA, TI tiB)
{
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);
    // Compare the filed lists of UDTs member by member,
    // ignore member functions. 
    // Return 
    //  FALSE -> A and B are not equal
    //  TRUE  -> A and B are equal
    // Note - Virtual Function changes are ignored
    //        here, the debugger must take care of them.

    if (tiA == tiB)
        return TRUE;

    PTYPE ptA = NULL;
    if (!QueryPbCVRecordForTi(tiA, (PB*)&ptA))
        return FALSE;

    if (ptA->leaf != LF_FIELDLIST)
        return FALSE;

    PTYPE ptB = NULL;
    if (!QueryPbCVRecordForTi(tiB, (PB *)&ptB))
        return FALSE;

    assert(ptB->leaf == LF_FIELDLIST);
    BOOL fResult = TRUE;

    TypeTiIter tiiA(ptA);
    TypeTiIter tiiB(ptB);

    while(TRUE) {

        lfEasy *pLeafA = NULL, *pLeafB = NULL;

        while(tiiA.nextField()) {
            lfEasy *pLeaf = (lfEasy *) tiiA.pbCurField();
            if (!TPI_IS_FUNCTION(pLeaf->leaf)) {
                pLeafA = pLeaf;
                break;
            }
        }

        while(tiiB.nextField()) {
            lfEasy *pLeaf = (lfEasy *) tiiB.pbCurField();
            if (!TPI_IS_FUNCTION(pLeaf->leaf)) {
                pLeafB = pLeaf;
                break;
            }
        }

        if (pLeafA == NULL || pLeafB == NULL) {
            // We have exhausted atleast one list.
            // If we exhausted both of them, then
            // edit is allowed, or else, the other
            // list did have some extra data members
            // and hence it is not a legal edit.
            return pLeafA == pLeafB;
        }

        if (!(TPI_IS_FUNCTION(pLeafA->leaf) && TPI_IS_FUNCTION(pLeafB->leaf))) {
            PB pbStartA = (PB) pLeafA;
            PB pbEndA = tiiA.pbEndCurFieldSansPad();
            if (memcmp(pLeafA, pLeafB, pbEndA - pbStartA) != 0) {
                return FALSE;
            }
        }
    }

    // We should never be here
    assert(FALSE);
    return FALSE;
}

// only called from TPI1::QueryTiForCVRecord
__forceinline PB TPI1::GetAlignedCVRecord(const PB pb)
{
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    if (fNeedsSzConversion((PTYPE)pb) || !fAlign(REC::cbForPb(pb))) {
        if (!bufAlign.Size()) {
            if (!bufAlign.Reserve(cbRecMax)){
                ppdb1->setOOMError();
                return NULL;
            }
        }

        CB  cbRec = REC::cbForPb(pb);
        memcpy(bufAlign.Start(), pb, cbRec);
        if (!fAlign(cbRec)) {
            // must adjust reclen and add padding
            PTYPE   ptype = reinterpret_cast<PTYPE>(bufAlign.Start());
            debug(
                CB  dcb = dcbAlign(cbRec);
            )

            ptype->len += cbInsertAlign(PB(ptype) + cbRec, cbRec);

            assert(dcb + cbRec == REC::cbForPb(PB(ptype)));
        }

        if (!fConvertTypeRecStToSz((PTYPE)bufAlign.Start())) {
            ppdb1->setLastError(EC_FORMAT);
            return NULL;
        }

        assert(fAlign(REC::cbForPb(bufAlign.Start())));
        return bufAlign.Start();
    }

    return pb;
}

// only called by TPI1::QueryTiForCVRecord()
__forceinline BOOL TPI1::LookupNonUDTCVRecord(const PB pb, TI *pti)
{
    PDBLOG_FUNC();
    assert(fInitd && fInitResult);
    MTS_ASSERT(m_cs);

    LHASH recHash = hashPrecFull((PREC)pb);
    LHASH hash = m_pfnHashTpiRec(pb, hdr.tpihash.cHashBuckets);

    PCHN pchnHead = mphashpchn[hash];

    // look up an existing association
    for (PCHN pchn = pchnHead; pchn; pchn = pchn->pNext) {
        TI      ti = pchn->ti;
        PRECEX    prec = precForTi(ti);
        if (!prec.prec)
            return FALSE;

#ifdef HASH_REPORT
        cRecComparisons++;
#endif
        if (prec.recHash == recHash && prec.prec->fSame(pb)) {
            *pti = ti;
            return TRUE;
        }
    }

    return FALSE;
}

BOOL TPI1::AddNewTypeRecord(const PB pb, TI *pti)
{
    PDBLOG_FUNC();

    assert(fInitd && fInitResult);
    MTS_ASSERT(m_cs);

    // not found: add a new association
    PREC prec = new (this, pb) REC(pb);
    if (!prec) {
        ppdb1->setOOMError();
        return FALSE;
    }

    LHASH hash = hashPrec(prec);
    PCHN *ppchnHead = &mphashpchn[hash];
    PCHN pchn = new (poolChn) CHN(*ppchnHead, tiNext());

    if (!pchn) {
        ppdb1->setOOMError();
        return FALSE;
    }

    *pti = pchn->ti;

    if (pchn->ti == T_VOID)     // must have run out of type indices
        return FALSE;

    *ppchnHead = pchn;

    if (!mptiprec.append(PRECEX(prec))) {
        ppdb1->setOOMError();
        return FALSE;
    }

    debug(unsigned iprec = pchn->ti - ::tiMin);
    assert(mptiprec.size() - 1 == iprec);

    // store new hash value - we will use this to update the hash stream later
    if (hdr.vers < impv80) {
        PREFAST_SUPPRESS(22006, "We trancate this on purpose");
        HASH    hashOld = static_cast<HASH>(hash);

        if (!bufMapHash.Append((PB) &hashOld, sizeof (HASH))) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }
    else {
        if (!bufMapHash.Append((PB) &hash, sizeof (LHASH))) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    // record <TI, OFF> tuple
    if (!RecordTiOff(pchn->ti, OFF(cbClean + poolRec.cb() - REC::cbForPb(pb))))
        return FALSE;

    return TRUE;
}

BOOL TPI1::fLegalTypeEdit(PB pb, TI ti)
{
    PDBLOG_FUNC();

    // check if this type edit is allowed by E&C
    PB pbOld = NULL;
    return QueryPbCVRecordForTi(ti, &pbOld)
            && fIsModifiedClass((PTYPE)pb, (PTYPE)pbOld);
}

BOOL TPI1::PromoteTIForUDT(SZ szName, PCHN *ppchnHead, PCHN pchnPrev, BOOL fAdjust, TI tiUdt)
{
    PDBLOG_FUNC();

    assert(fInitd && fInitResult);
    MTS_ASSERT(m_cs);

    assert(pchnPrev != NULL && pchnPrev->pNext != NULL);
    assert(szName && ppchnHead);

    // If PDB is not being written to, can't do this
    if (!fAppend && !fWrite) {
        ppdb1->setLastError(EC_ILLEGAL_TYPE_EDIT);
        return FALSE;
    }

    PCHN pchn  = pchnPrev->pNext;
    NI   niUdt = niNil;

    assert(pnamemap);
    if (!pnamemap->getNiUTF8(szName, &niUdt)) {
        return FALSE;
    }

    // Promote the pchn to the head of the list
    pchnPrev->pNext = pchn->pNext;
    pchn->pNext = *ppchnHead;
    *ppchnHead = pchn;

    // we may need to add, replace, or delete the hash adjuster,
    // since all the hash chains are in strictly descending order
    // except for at most one UDT at the head of the list, which
    // must have come from our mphashtiHead mapping.
    if (fAdjust) {
        // add or replace
        mpnitiHead.add(niUdt, tiUdt);
    }
    else {
        // remove any existing association
        mpnitiHead.remove(niUdt);
    }

    return TRUE;
}

BOOL TPI1::QueryTiForCVRecord(PB pbIn, OUT TI* pti) {
    PDBLOG_FUNC();

    MTS_PROTECT(m_cs);  

    if (!fInit())
        return FALSE;
    
    assert(fGetTi && REC::cbForPb(pbIn) < cbRecMax);
    PB   pb = GetAlignedCVRecord(pbIn);
    BOOL fUnmatchedNameFound = FALSE;
    NI   niUdt = niNil;
    TI   tiLast = tiNil;

    if (pb == NULL) {
        // OOM already set in GetAlignedCVRecord
        return FALSE;
    }

    bool  fGlobalDefnUdt = fEnableQueryTiForUdt && REC::fIsGlobalDefnUdt(pb);
    bool  fLocalDefnUdt  = fEnableQueryTiForUdt && REC::fIsLocalDefnUdtWithUniqueName(pb);
    bool  fUdtSrcLine    = fEnableQueryTiForUdt && REC::fIsUdtSrcLine(pb);
    LHASH hash = 0;
    SZ    szName = NULL;

    if (fGlobalDefnUdt || fLocalDefnUdt) {
        unsigned cSameUDT = 0;
        TI       tiUdtMax = tiNil, tiUdt = tiNil;

        // hash on udt name only - to support QueryTiForUDT
        // preflight the NI to see if we will need to remove one of the
        // mappings...need to have an NI w/o adding it to the namemap.
        if (!getNameMap()) {
            ppdb1->setOOMError();
            return FALSE;
        }

        szName = szUDTName(pb);

        if (fLocalDefnUdt) {
            // For local UDT we hash on unique name.
            szName = szName + strlen(szName) + 1;
        }

        pnamemap->containsUTF8(szName, &niUdt);

        // look up an existing association
        hash = hashUdtName(szName);
        LHASH recHash = hashPrecFull((PREC)pb);

        PCHN  *ppchnHead = &mphashpchn[hash];
        PCHN pchn, pchnPrev = NULL;
        for (pchn = *ppchnHead; pchn; pchnPrev = pchn, pchn = pchn->pNext) {
            // look for udt with matching name
            tiUdt     = pchn->ti;
            PRECEX prec = precForTi(tiUdt);
            if (!prec.prec)
                return FALSE;

#ifdef HASH_REPORT
            cRecUDTComparisons++;
#endif
            if (prec.prec->fSameUDT(szName, TRUE))
            {
                // found the same named UDT, check to see if it is
                // exactly the right one and keep a count of how many with
                // that name exist in the chain.
                cSameUDT++;

                // keep the maximum ti we have seen to determine if we need
                // to emit a hash adjuster in the case of moving an existing
                // type record to the head of the list.
                if (tiUdt > tiUdtMax) {
                    tiUdtMax = tiUdt;
                }

                if (cSameUDT == 1) {
                    tiLast = tiUdt;     // Last def'n for /ZX type comparison
                }

#ifdef HASH_REPORT
                cRecComparisons++;
#endif
                if (prec.recHash == recHash && prec.prec->fSame(pb)) {
                    // found the correct UDT already in the pool.
                    *pti = tiUdt;
                    if ( fAppend && !fWrite ) {    // in E&C mode we only care that we found it
                        return TRUE;
                    }

                    if (cSameUDT > 1) {            
                        // Is it a /ZX build? Is this type change allowed?
                        if (fAppend && fWrite && !fLegalTypeEdit(pb, tiLast)) {
                            ppdb1->setLastError(EC_ILLEGAL_TYPE_EDIT);
                            return FALSE;
                        }

                        // must ensure this PREC is first in hash chain
                        if (!PromoteTIForUDT(szName, ppchnHead, pchnPrev, tiUdt < tiUdtMax, tiUdt))
                            return FALSE;
                    }
                    return TRUE;
                }
                else {
                    // don't stop looking - we found the matching UDT name but the
                    // types don't match.  we may need to add a new one or check to see if
                    // there is already one in the pool
                    
                    // UNDONE : /ZX compile also can reuse types which are already 
                    //          defined in this type pool. Lets go through all of them
                    /*  
                    if ( fAppend && fWrite ) {
                        // /ZX compile - We are probably adding a
                        // modified type, do a sanity check bellow
                        break;
                    }
                    */
                }
            }
        }
    }
    else if (fUdtSrcLine) {
        PTYPE          ptype = (PTYPE) pb;
        lfUdtSrcLine  *plf = (lfUdtSrcLine *) &ptype->leaf;
        LHASH recHash = hashPrecFull((PREC)pb);

        for (PCHN pchn = mphashpchn[hashTypeIndex(plf->type)]; pchn; pchn = pchn->pNext) {
            PRECEX prec = precForTi(pchn->ti);

            if (!prec.prec) {
                return FALSE;
            }

            if (prec.recHash == recHash && prec.prec->fSame(pb)) {
                return TRUE;
            }
        }
    }
    else if (LookupNonUDTCVRecord(pb, pti)) {
        return TRUE;
    }

    if ((!fAppend && !fWrite))       // Just reading, but could not find the type
        return FALSE;

    // Now that we are writing to PDB, check whether PDB is already corrupted.
    // And if so, don't bother to add new type record but just return FALSE.
    if (ppdb1->QueryLastErrorExW(NULL, 0) == EC_CORRUPT) {
        return FALSE;
    }

    if (fAppend && tiLast != tiNil && !fLegalTypeEdit(pb, tiLast)) {
        // E&C or /ZX build and this kind of change is not allowed
        ppdb1->setLastError( EC_ILLEGAL_TYPE_EDIT );
        return FALSE;
    }

    if (niNil != niUdt) {
        // We have a hash adjuster record for this type.
        // Get rid of it since we are going to add a new
        // record for this type, which will be added in
        // front of the hash chain anyway.
        mpnitiHead.remove(niUdt);
    }

    bool fRet = AddNewTypeRecord(pb, pti);

    if (fRet && fGlobalDefnUdt && ppdb1->m_fPdbTypeMismatch) {
        fRet = ppdb1->pdbi1->FTypeMismatchDetection(szName, *pti, hash);
    }

    return fRet;
}

LHASH TPI1::hashPrec(PREC prec) const
{
    PDBLOG_FUNC();

    assert(!fNeedsSzConversion((PTYPE)prec->buf));
    assert(m_pfnHashTpiRec != NULL);

    if (REC::fIsGlobalDefnUdt(prec->buf)) {
        // hash on udt name only - to support QueryTiForUDT
        return hashUdtName(szUDTName(prec->buf));
    }

    if (REC::fIsLocalDefnUdtWithUniqueName(prec->buf)) {
        // hash on udt's unique name only - to support QueryTiForUDT
        SZ szName = szUDTName(prec->buf);
        return hashUdtName(szName + strlen(szName) + 1);
    }

    if (REC::fIsUdtSrcLine(prec->buf)) {
        PTYPE          ptype = (PTYPE) prec->buf;
        lfUdtSrcLine  *plf = (lfUdtSrcLine *) &ptype->leaf;
        return hashTypeIndex(plf->type);
    }

    return m_pfnHashTpiRec(prec->buf, hdr.tpihash.cHashBuckets);
}

LHASH TPI1::hashPrecFull(PREC prec)
{
    return hashBufv8(prec->buf, 0xFFFFFFFF);
}

#define         _8K             (8L*1024)

// only called by TPI1::AddNewTypeRecord()
BOOL TPI1::RecordTiOff (TI ti, OFF off) {
    PDBLOG_FUNC();
    assert(fInitd && fInitResult);
    MTS_ASSERT(m_cs);

    if (!tioffLast.ti || (tioffLast.off / _8K) < (off / _8K)) {

        tioffLast = TI_OFF(ti, off);
        if (!bufTiOff.Append((PB)&tioffLast, sizeof TI_OFF)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }
    return TRUE;
}

// only called by TPI1::precForTi()
BOOL TPI1::fLoadRecBlk (TI ti) {
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);

    TI_OFF *rgTiOff = (TI_OFF *)bufTiOff.Start();

    // find blk that has the ti of interest
    int i;
    for (i = 0; i < cTiOff - 1; i++ ) {
        if (ti >= rgTiOff[i].ti &&
            ti < rgTiOff[i+1].ti )
            break;
    } // end for

    assert(i < cTiOff);
    if (i >= cTiOff) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    // compute interesting values
    TI tiBlkMin = rgTiOff[i].ti;
    TI tiBlkMac = i == cTiOff - 1 ? tiCleanMac : rgTiOff[i+1].ti;

    OFF off = rgTiOff[i].off + cbHdr;
    CB cb = ((i == cTiOff - 1) ? (cbClean + cbHdr - off) : (rgTiOff[i+1].off - rgTiOff[i].off));

    // Alloc space & read in blk of records
    PREC prec = reinterpret_cast<PREC>(new (poolRecClean) BYTE[cb]);
    if (!prec) {
        ppdb1->setOOMError();
        return FALSE;
    }

    CB cbRead = cb;
    if (!(pmsf->ReadStream(m_sn, off, prec, &cbRead) && cbRead == cb)) {
        ppdb1->setReadError();
        return FALSE;
    }

    // build partial ti to prec mapping for the blk read in

    assert(mptiprec.size() >= ULONG(tiBlkMac - ::tiMin));

    size_t  cbProcessed = 0;
    for (TI tiCur = tiBlkMin; tiCur < tiBlkMac; tiCur++)
    {
        if (tiCur < ::tiMin) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        // Make sure we won't overflow

        size_t  cbRec = prec->cbForPb(PB(prec));
        cbProcessed += cbRec;
        
        assert(cbProcessed <= size_t(cbRead));

        if (cbProcessed > size_t(cbRead)) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        mptiprec[tiCur - ::tiMin] = prec;
        prec = reinterpret_cast<PREC>(PB(prec) + cbRec);
    }
    return TRUE;
}

TPI1::PRECEX TPI1::precForTi(TI ti) {
    PDBLOG_FUNC();

    MTS_ASSERT(m_cs);

    // precForTi() is going to be called even if !fGetCVRecords
    // since a CHN no longer has a prec
    // assert(fGetCVRecords);
    if (!fHasTi(ti))
        return 0;

    dassert(mptiprec.size() > ULONG(ti - ::tiMin));

    // check if chunk of records loaded
    if (!mptiprec[ti - ::tiMin].prec && !fLoadRecBlk(ti))
        return 0;

    PRECEX prec = mptiprec[ti - ::tiMin];
    dassert(prec.prec);

    // fix up any 16-bit records we find
    PREC    precT = prec.prec;
    if (f16bitPool && fNeeds16bitConversion(PTYPE(precT->buf))) {
        assert (pwti != 0);
        precT = PREC(pwti->pTypeWidenTi(ti, precT->buf));
        if (precT) {
            if (fNeedsSzConversion((PTYPE)precT->buf))
                verify(fConvertTypeRecStToSz((PTYPE)precT->buf));

            prec = PRECEX(precT);
            mptiprec[ti - ::tiMin] = precT;
        }
    }

    return prec;
}

#ifdef PDB_TYPESERVER
void TPI1::ConvertTypeRange( TYPTYPE* ptype, ITSM itsm )
{
    PDBLOG_FUNC();
    if ( pdbi != 0 ) {
        for ( TypeTiIter tii(ptype); tii.next();) {
            tii.rti() = pdbi->ConvertItem( tii.rti(), itsm );
        }
    }
}

BOOL TPI1::AlternateTS( TI ti, TPI** pptpi )
{
    PDBLOG_FUNC(); 
    if ( pdbi != 0 && pdbi->ItsmFromTi( ti ) != itsm ) {
        return pdbi->QueryTypeServer( pdbi->ItsmFromTi( ti ), pptpi );
    }
    return FALSE;
}

BOOL TPI1::AreTypesEqual( TI ti1, TI ti2 )
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_cs); 

    if ( pdbi != 0 ) {
        TPI* altTpi;
        if ( AlternateTS( ti1, &altTpi ) ) // is another server available?
            return altTpi->AreTypesEqual( ti1, ti2 );
        if ( fHasExTi( ti1 ) ) {    // do I serve this type?
            ITSM itsm2;
            if ( pdbi->QueryItsmForTi( ti2, &itsm2 ) ) {
                if ( itsm != itsm2 ) {  // not the same type server
                    if ( pdbi->QueryTypeServer( itsm2, &altTpi ) ) {
                        TI titi2;
                        if ( FindTiFromAltTpi( ti2, altTpi, &titi2 ) ) {
                            return titi2 == InternalTi( ti1 );
                        }
                    }
                }
            }
        }
    }
    return ti1 == ti2;
}

BOOL TPI1::IsTypeServed( TI ti )
{
    PDBLOG_FUNC();
    if ( pdbi != 0 ) {
        TPI* tpi;
        if ( AlternateTS( ti, &tpi ) ) return tpi->IsTypeServed( ti );
    }
    return fHasExTi( ti );
}

BOOL TPI1::FindTiFromAltTpi( TI rti, TPI* ptpiFrom, OUT TI* pti )
{
    PDBLOG_FUNC();
    if ( CV_IS_PRIMITIVE( rti ) ) { // we have a base type here, just copy it out
        *pti = rti;
        return TRUE;
    }

    if (!fInit()) {
        return FALSE;
    }

    SafeStackAllocator<256> buf;
    // read type record from the 'from' TypeServer
    PTYPE ptype = reinterpret_cast<PTYPE>(buf.Alloc<BYTE>(256));
    CB cb = 256;
    if (!ptpiFrom->QueryCVRecordForTi(rti, reinterpret_cast<PB>(ptype), &cb))
        return FALSE;
    if (cb > 256) {
        assume(cb >= 0);
        // alloc a new buffer and try again
        if ( !(ptype = reinterpret_cast<PTYPE>(buf.Alloc<BYTE>(cb))) ) {
            ppdb1->setLastError(EC_OUT_OF_MEMORY);
            return FALSE;
        }
        if (!ptpiFrom->QueryCVRecordForTi(rti, reinterpret_cast<PB>(ptype), &cb))
            return FALSE;
    }
    // recursively find each subtype index and replace it with
    // the internal index of the matching type from this typeserver
    for (TypeTiIter tii(ptype); tii.next(); ) {
        if ( !FindTiFromAltTpi( tii.rti(), ptpiFrom, &tii.rti() ) ) {
            return FALSE;
        }
    }

    // lookup the modified type record
    if (fEnableQueryTiForUdt && REC::fIsGlobalDefnUdt((PB)ptype)) {

#pragma message("UNDONE: add code to handle local UDTs when fIsLocalDefnUdtWithUniqueName() is true")

        // hash on udt name only
        SZ sz = szUDTName((PB)ptype);
        LHASH hash = hashUdtName(sz);
        LHASH recHash = hashPrecFull((PREC)ptype)
        PCHN* ppchnHead = &mphashpchn[hash];
        PCHN        pchnPrev = NULL;

        // look up an existing association
        for (PCHN pchn = *ppchnHead; pchn; pchnPrev = pchn, pchn = pchn->pNext) {
            // look for udt with matching name
            PRECEX prec = precForTi(pchn->ti);
            if (!prec.prec)
                return FALSE;
            ConvertTypeRange( (PTYPE)prec.prec->buf, 0 );    // convert back to internal form
#ifdef HASH_REPORT
            cRecUDTComparisons++;
#endif
            if (prec.prec->fSameUDT(sz, TRUE)) {
#ifdef HASH_REPORT
                cRecComparisons++;
#endif
                if (prec.recHash == recHash && prec.prec->fSame((PB)ptype)) {
                    *pti = pchn->ti;
                    return TRUE;
                }
                // don't stop looking - we found the matching UDT name but the
                // types don't match.
            }
        }
    }
    else {
        LHASH hash = m_pfnHashTpiRec((PB)ptype, hdr.tpihash.cHashBuckets);
        LHASH recHash = hashPrecFull((PREC)ptype);
        PCHN* ppchnHead = &mphashpchn[hash];

        // look up an existing association
        for (PCHN pchn = *ppchnHead; pchn; pchn = pchn->pNext) {
            PRECEX prec=precForTi(pchn->ti);
            if (!prec.prec)
                return FALSE;
            ConvertTypeRange( (PTYPE)(prec.prec)->buf, 0 );    // convert back to internal form
#ifdef HASH_REPORT
            cRecComparisons++;
#endif
            if (prec.recHash == recHash && prec->fSame((PB)ptype)) {
                *pti = pchn->ti;
                return TRUE;
            }
        }
    }
    return FALSE;
}

#else // !PDB_TYPESERVER

// Dummy implementation of typeserver functions
BOOL TPI1::AreTypesEqual( TI ti1, TI ti2 )
{ 
    PDBLOG_FUNC();
    return ti1 == ti2;
}

BOOL TPI1::IsTypeServed( TI ti )
{
    PDBLOG_FUNC();

    MTS_PROTECT(m_cs); 
    return fHasTi(ti);
}

#endif // !PDB_TYPESERVER

BOOL TPI1::QueryCVRecordForTi(TI ti, PB pb, CB *pcb)
{
    PDBLOG_FUNC();

    PB pbInternal;

    if (!QueryPbCVRecordForTi(ti, &pbInternal))
        return FALSE;

    memcpy(pb, pbInternal, __min(*pcb, REC::cbForPb(pbInternal)));
    *pcb = REC::cbForPb(pbInternal);

    return TRUE;
}

BOOL TPI1::QueryPbCVRecordForTi(TI ti, OUT PB* ppb)
{
    PDBLOG_FUNC();

    MTS_PROTECT(m_cs); 

    if (!fInit())
        return FALSE;

    assert(hdr.vers == impv70 || hdr.vers == curImpv);

#ifdef PDB_TYPESERVER
    TPI* ptpiAlt;
    if ( AlternateTS( ti, &ptpiAlt ) )
        return ptpiAlt->QueryPbCVRecordForTi( ti, ppb );
    ti = fHasExTi( ti ) ? InternalTi( ti ) : ti;;
#endif

    PRECEX prec = precForTi(ti);
    if (!prec.prec)
        return FALSE;

    *ppb = &(prec.prec->buf[0]);

#ifdef PDB_TYPESERVER
    ConvertTypeRange( (TYPTYPE*)*ppb, itsm );
#endif

    assert(!fNeedsSzConversion((PTYPE)*ppb));

    return TRUE;
}

BOOL TPI1::fCommit()
{
    PDBLOG_FUNC();
    MTS_ASSERT(m_cs);  

    assert(fWrite);

    // write out all the dirty blocks
    dassert(pmsf);
    for (BLK* pblk = pblkPoolCommit ? pblkPoolCommit->pNext : poolRec.pHead;
         pblk;
         pblk = pblk->pNext) {
        dassert(pblk);

        if (!pmsf->AppendStream(m_sn, pblk->buf, pblk->cb())){
            ppdb1->setWriteError();
            return FALSE;
        } else {
            cbGpRec() += pblk->cb();
        }
    } // end for

    // commit the type stream
    if (poolRec.pTail) {
        poolRec.blkFlush();
    }
    pblkPoolCommit = poolRec.pTail;

    // write out hash stream
    if ((snHash() == snNil) || fReplaceHashStream) {
        assert(cbMapHashCommit == 0);

        // allocate and replace entire stream
        if (!fGetSnHash() ||
            !pmsf->ReplaceStream(snHash(), bufMapHash.Start(), bufMapHash.Size())) {
            ppdb1->setWriteError();
            return FALSE;
            }
    }
    else {

        // truncate stream if we are going to add new hash values
        if (!pmsf->TruncateStream(
                snHash(),
                offHashValues() + cbHashValues()
                )
            ) {
            ppdb1->setWriteError();
            return FALSE;
        }

        // just append new, uncommitted hash values to whats there
        if (!pmsf->AppendStream(snHash(), bufMapHash.Start() + cbMapHashCommit,
                             bufMapHash.Size() - cbMapHashCommit)) {
            ppdb1->setWriteError();
            return FALSE;
            }
    }

    // append <TI, OFF> values to hash stream
    if (!pmsf->AppendStream(snHash(), bufTiOff.Start(), bufTiOff.Size())) {
        ppdb1->setWriteError();
        return FALSE;
    }

    if (mpnitiHead.count()) {
        Buffer  buf;
        if (!mpnitiHead.save(&buf)) {
            ppdb1->setOOMError();
            return FALSE;
        }
        if (!pmsf->AppendStream(snHash(), buf.Start(), buf.Size())) {
            ppdb1->setWriteError();
            return FALSE;
        }
    }

    // update the header, sections layed out contiguously:
    //      offset 0 = start of hash values
    //      offset 0 + sizeof hash values = offset of TiOff's
    //      offset TiOff + sizeof TiOffs = offset of Hash Adjusters
    if (hdr.vers < impv70) {
        hdr.vers = impv70;
    }

    TpiHash &   tpihash = hdr.tpihash;

    tpihash.offcbHashVals.off = 0;
    tpihash.offcbHashVals.cb  = (tiMac() - tiMin()) * tpihash.cbHashKey;

    tpihash.offcbTiOff.off = tpihash.offcbHashVals.cb;
    tpihash.offcbTiOff.cb  = bufTiOff.Size();

    tpihash.offcbHashAdj.off = tpihash.offcbTiOff.off + tpihash.offcbTiOff.cb;
    tpihash.offcbHashAdj.cb  = mpnitiHead.count() ? mpnitiHead.cbSave() : 0;


    // write new header
    if (!pmsf->WriteStream(m_sn, 0, &hdr, sizeof hdr)){
        ppdb1->setWriteError();
        return FALSE;
        }

    // mark everything committed on the hash stream
    cbMapHashCommit = bufMapHash.Size();

    return TRUE;
}

BOOL TPI1::Commit()
{
    PDBLOG_FUNC();
    MTS_PROTECT(m_cs);    

    BOOL fOK = !fWrite || fCommit();

    // We need to commit namemap as well when committing TPI (DDB #153671).
    // However we cannot do this when PDB is opened for read only, because
    // although writing name map to disk is guarded by 'NMP::m_fWrite' in
    // NMP::commit(), we cannot rely on this flag unfortunately, since the
    // API NameMap::open() allows a client to open a name map in write mode
    // even if the corresponding PDB file is opened for read only.  The BVT
    // test failure on native EnC suites in DDB #161497 is due to this.
    if (fOK && fWrite && pnamemap)
        fOK = pnamemap->commit();

    if (!fOK)
        ppdb1->setWriteError();
    return fOK;
}

BOOL TPI1::Close()
{
    PDBLOG_FUNC();
    BOOL fOK = Commit();
    assert(ppdb1);
#ifdef HASH_REPORT
    wchar_t szPdbName[_MAX_PATH];
    wprintf(
        L"\nTPI Hash comparisons in %s:\nfull = %u, UDT name = %u\n",
        ppdb1->QueryPDBNameExW(szPdbName, _countof(szPdbName)),
        cRecComparisons, cRecUDTComparisons
        );
    fflush(stdout);
#endif
    if (ppdb1->internal_CloseTPI(this)) {
        if (pwti)
            pwti->release();
        if (pnamemap) {
            pnamemap->close();
            pnamemap = 0;
        }
        delete this;
    }
    return fOK;
}


BOOL REC::fSameUDT(const char *sz, BOOL fCase)
{
    if (!REC::fIsGlobalDefnUdt(buf) && !REC::fIsLocalDefnUdtWithUniqueName(buf)) {
        return FALSE;
    }

    SZ szBufName = szUDTName(buf);

    if (REC::fIsLocalDefnUdtWithUniqueName(buf)) {
        szBufName = szBufName + strlen(szBufName) + 1;
    }

    if (fCase) {
        return (strcmp(sz, szBufName) == 0);
    }

    return (_stricmp(sz, szBufName) == 0);
}


BOOL REC::fSameUdtSrcLine(TI ti, unsigned short leaf)
{
    if (!REC::fIsUdtSrcLine(buf)) {
        return FALSE;
    }

    lfUdtSrcLine *plf = (lfUdtSrcLine *) &((PTYPE) buf)->leaf;

    return (plf->type == ti) && (plf->leaf == leaf);
}


BOOL REC::fIsDefnUdt(PB pb)
{
    PTYPE     ptype = PTYPE(pb);
    unsigned  leaf = ptype->leaf;

    assert(!fNeedsSzConversion((PTYPE)pb));

    if (leaf != LF_CLASS && leaf != LF_STRUCTURE &&
        leaf != LF_UNION && leaf != LF_ENUM &&
        leaf != LF_INTERFACE) {
        return FALSE;
    }

    lfClass *plf = (lfClass *) &ptype->leaf;

    return !plf->property.fwdref;
}


BOOL REC::fIsGlobalDefnUdt(PB pb)
{
    PTYPE     ptype = PTYPE(pb);
    unsigned  leaf = ptype->leaf;

    assert(!fNeedsSzConversion((PTYPE)pb));

    if (leaf == LF_ALIAS) {
        return TRUE;
    }

    if (leaf != LF_CLASS && leaf != LF_STRUCTURE &&
        leaf != LF_UNION && leaf != LF_ENUM &&
        leaf != LF_INTERFACE) {
        return FALSE;
    }

    lfClass *plf = (lfClass *) &ptype->leaf;

    return !plf->property.fwdref && !plf->property.scoped && !fUDTAnon(PTYPE(pb));
}


BOOL REC::fIsLocalDefnUdtWithUniqueName(PB pb)
{
    PTYPE     ptype = PTYPE(pb);
    unsigned  leaf = ptype->leaf;

    assert(!fNeedsSzConversion((PTYPE)pb));

    if (leaf != LF_CLASS && leaf != LF_STRUCTURE &&
        leaf != LF_UNION && leaf != LF_ENUM &&
        leaf != LF_INTERFACE) {
        return FALSE;
    }

    lfClass *plf = (lfClass *) &ptype->leaf;

    return !plf->property.fwdref && plf->property.scoped &&
           plf->property.hasuniquename && !fUDTAnon(PTYPE(pb));
}


BOOL REC::fIsUdtSrcLine(PB pb)
{
    PTYPE     ptype = PTYPE(pb);
    unsigned  leaf = ptype->leaf;

    return leaf == LF_UDT_SRC_LINE || leaf == LF_UDT_MOD_SRC_LINE;
}


BOOL TPI1::QueryTiForUDTW(const wchar_t *wcs, BOOL fCase, OUT TI* pti)
{
    PDBLOG_FUNC();
    USES_STACKBUFFER(0x400);

    UTFSZ utfsz = GetSZUTF8FromSZUnicode(wcs);
    if (utfsz == NULL)
        return FALSE;

    return QueryTiForUDT(utfsz, fCase, pti);
}


BOOL TPI1::QueryTiForUDT(const char *sz, BOOL fCase, OUT TI* pti)
{
    PDBLOG_FUNC(); 

    MTS_PROTECT(m_cs);

    // check version of tpi - if new hash on udt not implemented - this
    // functionality will not be supported
    if (!fInit() || !fEnableQueryTiForUdt )
        return FALSE;

    // hash on udt name only - to support QueryTiForUDTName
    LHASH hash = hashUdtName(sz);

    dassert(mphashpchn);
    PCHN* ppchnHead = &mphashpchn[hash];

    // look up an existing association
    for (PCHN pchn = *ppchnHead; pchn; pchn = pchn->pNext) {
        // look for udt with matching name
        PRECEX    prec = precForTi(pchn->ti);
        if (!prec.prec) {
            return FALSE;
        }

#ifdef HASH_REPORT
        cRecUDTComparisons++;
#endif
        if (prec.prec->fSameUDT(sz, fCase)) {
#ifdef PDB_TYPESERVER
            *pti = ExternalTi( pchn->ti );
#else // !PDB_TYPESERVER
            *pti = pchn->ti;
#endif // !PDB_TYPESERVER
            return TRUE;
        }
    }

#ifdef PDB_TYPESERVER
    if ( pdbi ) {
        ITSM i;
        TPI *ptpi;
        if ( pdbi->QueryNextItsm( itsm, &i ) && itsm != i ) {   // try another type server
            if ( pdbi->QueryTypeServer( i, &ptpi ) ) {
                return ptpi->QueryTiForUDT( sz, fCase, pti );
            }
        }
    }
#endif
    return FALSE;
}


BOOL TPI1::getSrcInfoRecordIdForUDT(const TI tiUdt, unsigned short leaf, TI& id)
{
    PDBLOG_FUNC(); 

    MTS_ASSERT(m_cs);

    assert((leaf == LF_UDT_SRC_LINE) || (leaf == LF_UDT_MOD_SRC_LINE));

    // check version of tpi - if new hash on udt not implemented - this
    // functionality will not be supported
    
    if (!fInit() || !fEnableQueryTiForUdt) {
        return FALSE;
    }

    LHASH hash = hashTypeIndex(tiUdt);

    dassert(mphashpchn);

    for (PCHN pchn = mphashpchn[hash]; pchn; pchn = pchn->pNext) {
        PRECEX  prec = precForTi(pchn->ti);

        if (!prec.prec) {
            return FALSE;
        }

        if (prec.prec->fSameUdtSrcLine(tiUdt, leaf)) {
            id = pchn->ti;
            return TRUE;
        }
    }

    return FALSE;
}


// Given an UDT's type index, look for corresponding LF_UDT_SRC_LINE record,
// and grab the source file index and line number.

BOOL TPI1::QuerySrcIdLineForUDT(const TI tiUdt, TI& srcId, DWORD& line)
{
    PDBLOG_FUNC(); 

    MTS_PROTECT(m_cs);

    TI id = 0;

    if (!getSrcInfoRecordIdForUDT(tiUdt, LF_UDT_SRC_LINE, id)) {
        return FALSE;
    }

    PB pb = NULL;

    if (!QueryPbCVRecordForTi(id, &pb)) {
        return FALSE;
    }

    PTYPE ptype = (PTYPE) pb;

    assert(ptype->leaf == LF_UDT_SRC_LINE);

    lfUdtSrcLine *plf = (lfUdtSrcLine *) &ptype->leaf;

    srcId = plf->src;
    line = plf->line;

    return TRUE;
}


// Given an UDT's type index, look for corresponding LF_UDT_MOD_SRC_LINE record,
// and grab the module index, source file index and line number.

BOOL TPI1::QueryModSrcLineForUDTDefn(const TI tiUdt, unsigned short *pimod, TI* pSrcId, DWORD* pLine)
{
    PDBLOG_FUNC(); 

    MTS_PROTECT(m_cs);

    TI id = 0;

    if (!getSrcInfoRecordIdForUDT(tiUdt, LF_UDT_MOD_SRC_LINE, id)) {
        return FALSE;
    }

    PB pb = NULL;

    if (!QueryPbCVRecordForTi(id, &pb)) {
        return FALSE;
    }

    PTYPE ptype = (PTYPE) pb;

    assert(ptype->leaf == LF_UDT_MOD_SRC_LINE);

    lfUdtModSrcLine *plf = (lfUdtModSrcLine *) &ptype->leaf;

    if (pimod != NULL) {
        *pimod = plf->imod;
    }

    if (pSrcId != NULL) {
        *pSrcId = plf->src;
    }

    if (pLine != NULL) {
        *pLine = plf->line;
    }

    return TRUE;
}


// Contract with caller of this function:
//
// If pimod is not NULL, look for LF_UDT_MOD_SRC_LINE.
// Otherwise, look for LF_UDT_SRC_LINE.

BOOL TPI1::QueryModSrcLineForUDT(const TI tiUdt, unsigned short *pimod, char **psz, DWORD *pLine)
{
    PDBLOG_FUNC(); 

    MTS_PROTECT(m_cs);

    TI id = 0;
    bool fQueryMod = (pimod != NULL);

    if (!getSrcInfoRecordIdForUDT(tiUdt, fQueryMod ? LF_UDT_MOD_SRC_LINE : LF_UDT_SRC_LINE, id)) {
        return FALSE;
    }

    PB pb = NULL;

    if (!QueryPbCVRecordForTi(id, &pb)) {
        return FALSE;
    }

    PTYPE ptype = (PTYPE) pb;

    if (fQueryMod) {
        assert(ptype->leaf == LF_UDT_MOD_SRC_LINE);
    } else {
        assert(ptype->leaf == LF_UDT_SRC_LINE);
    }

    lfUdtSrcLine     *plf = (lfUdtSrcLine *) &ptype->leaf;
    lfUdtModSrcLine  *plf2 = (lfUdtModSrcLine *) &ptype->leaf;

    if (pLine != NULL) {
        *pLine = plf->line;
    }

    if (fQueryMod) {
        *pimod = plf2->imod;
    }

    if (fQueryMod) {
        if (!getNameMap()) {
            ppdb1->setOOMError();
            return FALSE;
        }

        SZ_CONST    sz;

        pnamemap->getName(plf2->src, &sz);

        if (!sz || (*sz == '\0')) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        // For parity with the "else" part, allocate a buffer to hold the
        // string, so that caller of this function can free returned string.

        size_t cch = strlen(sz) + 1;
        char *szNew = new char[cch];

        if (szNew == NULL) {
            ppdb1->setOOMError();
            return FALSE;
        }

        strcpy_s(szNew, cch, sz);

        if (psz != NULL) {
            *psz = szNew;
        }
    }
    else {
        size_t cch = 0;

        if (!GetStringForId(plf->src, NULL, &cch, NULL)) {
            return FALSE;
        }

        char *sz = new char[cch + 1];

        if (sz == NULL) {
            ppdb1->setOOMError();
            return FALSE;
        }

        if (!GetStringForId(plf->src, sz, NULL, NULL)) {
            return FALSE;
        }

        sz[cch] = '\0';

        if (psz != NULL) {
            *psz = sz;
        }
    }

    return TRUE;
}


// When sz is NULL, if there are sub strings, this function calculates
// how big a string buffer is needed, not counting the terminating NULL,
// and the result is added into pcb.  If there is no sub string, this
// function fills pszRaw with a pointer to the string in raw record.
//
// When sz is not NULL, pcb and pszRaw are not used and this function
// just writes into the buffer psz (excluding the terminating NULL),
// and ADVANCES sz.  It is caller's responsibility to ensure that
// provided string buffer is big enough.

BOOL TPI1::GetStringForId(TI id, char *sz, size_t *pcb, char **pszRaw,
                          TM *ptm, PTYPE (*pfn)(TM *, TI))
{
    assert((sz != NULL) || (pcb != NULL));

    PTYPE pRaw = NULL;

    if (pfn != NULL) {
        pRaw = pfn(ptm, id);

        if (pRaw == NULL) {
            return FALSE;
        }
    }
    else {
        if (!QueryPbCVRecordForTi(id, (PB *) &pRaw)) {
            return FALSE;
        }
    }

    if (pRaw->leaf != LF_STRING_ID) {
        return FALSE;
    }

    lfStringId *plfStringId = (lfStringId *) &(pRaw->leaf);

    if (plfStringId->id != 0) {
        // Process sub strings

        if (pfn != NULL) {
            pRaw = pfn(ptm, plfStringId->id);

            if (pRaw == NULL) {
                return FALSE;
            }
        }
        else {
            if (!QueryPbCVRecordForTi(plfStringId->id, (PB *) &pRaw)) {
                return FALSE;
            }
        }

        if (pRaw->leaf != LF_SUBSTR_LIST) {
            return FALSE;
        }

        lfArgList *plfArgList = (lfArgList *) &(pRaw->leaf);

        for (unsigned i = 0; i < plfArgList->count; i++) {
            if (!GetStringForId(plfArgList->arg[i], sz, pcb, NULL, ptm, pfn)) {
                return FALSE;
            }
        }
    }
    else if (pszRaw != NULL) {
        // No sub string.  Just return the buffer that is in the raw debug record.

        *pszRaw = reinterpret_cast<char *>(&plfStringId->name[0]);
        return TRUE;
    }

    unsigned char *str = plfStringId->name;

    while (*str != '\0') {
        if (sz != NULL) {
            *sz = *str;
            sz++;
        } else {
            (*pcb)++;
        }

        str++;
    }

    return TRUE;
}


#if defined(NEVER)
CB TPI1::cbForRecsInBlk(TI tiMin, TI tiMac)
{
    PDBLOG_FUNC();
    // used to calculate the count of bytes between the first
    // record of a block (tiMin) and the given record (tiMac)

    // normalize to 0 based
    tiMin -= ::tiMin;
    tiMac -= ::tiMin;

    CB  cbRet = 0;
    TI  ti = tiMin;

    while (ti < tiMac) {
        assert(mptiprec[ti]);
        cbRet += mptiprec[ti++].prec->cb();
    }
    return cbRet;
}

OFF TPI1::offRecFromTiPrec(TI ti, PREC prec)
{
    PDBLOG_FUNC();
    // this is used so that we can find the offset in the stream of the record
    // being queried.  We rely on the fact that we do have a record already loaded.

    assert(ti >= ::tiMin);
    assert(ti < tiMac());
    assert(prec);
    assert(bufTiOff.Size());
    assert(bufTiOff.Start());

    TI_OFF * ptioff = (TI_OFF*)bufTiOff.Start();
    TI_OFF * ptioffMax = (TI_OFF*)(PB(ptioff) + bufTiOff.Size());

    // find blk that has the ti of interest
    for (; ptioff < ptioffMax - 1; ptioff++) {
        if (ti >= ptioff->ti && ti < (ptioff+1)->ti )
            break;
    }

    assert(ptioff < ptioffMax);

    // we get the base of the block containing the record we want.
    TI  tiBlkMin = ptioff->ti;

    assert(prec == mptiprec[ti - ::tiMin]);
    assert(mptiprec[tiBlkMin - ::tiMin]);

    // note that you cannot use PB(prec) - PB(mptiprec[tiBlkMin - ::tiMin])
    // in place of cbForRecsInBlk, since the two records may be in different
    // allocation pools.
    return ptioff->off + cbHdr + cbForRecsInBlk(tiBlkMin, ti);
}

BOOL TPI1::fReplaceRecord(TI tiUdt, PREC precDst, PB pbSrc, CB cbRec)
{
    PDBLOG_FUNC();
    OFF off = offRecFromTiPrec(tiUdt, precDst);
    CB  cbT = cbRec;

    assert(off >= cbHdr);

#if defined(_DEBUG)
    if (off < pmsf->GetCbStream(m_sn)) {
        DWORD   dw;
        CB      cb = sizeof(dw);
        // read in the first four bytes...they should be invariant between type-whacks
        if (!pmsf->ReadStream(m_sn, off, &dw, &cb) || cb != sizeof(dw)) {
            assert(FALSE);
        }
        if (*(DWORD UNALIGNED *)pbSrc != dw) {
            assert(FALSE);
        }
    }
#endif

    if (off >= pmsf->GetCbStream(m_sn) || pmsf->WriteStream(m_sn, off, pbSrc, cbRec)) {
        memcpy(precDst, pbSrc, cbRec);
        return TRUE;
    }
    return FALSE;

}
#endif

