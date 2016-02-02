//////////////////////////////////////////////////////////////////////////////
// PDB Debug Information API GSI Implementation

#include "pdbimpl.h"
#include "dbiimpl.h"
#include "cvinfo.h"
#ifdef SMALLBUCKETS
#include "cbitvect.h"
#endif

#ifndef PDB_LIBRARY
#include <locale.h>
#endif

typedef unsigned long UOFF;

GSI1::GSI1 (PDB1* ppdb1_, DBI1* pdbi1_)
    : ppdb1(ppdb1_), pdbi1(pdbi1_), rgphrBuckets(0)
#ifdef PDB_MT
    , refcount(1)
#endif
{
    if (ppdb1->m_fMinimalDbgInfo) {
        iphrHash = 0x3FFFF;
    } else {
        iphrHash = 4096;
    }
}

BOOL GSI1::Close()
{
    assert(pdbi1);
#ifdef PDB_MT
    MTS_PROTECT(pdbi1->m_csForPGSI);
    if (--refcount != 0) {
        assert(!pdbi1->fWrite);
        return TRUE;
    }
#endif
    pdbi1->pgsiGS = NULL;
    delete this;
    return TRUE;
}

GSI1::~GSI1()
{
}

BOOL GSI1::fInit(SN sn)
{
    rgphrBuckets = (HR **) (new (poolSymHash) BYTE[(iphrHash + 1) * sizeof(HR *)]);

    if (rgphrBuckets == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    memset(rgphrBuckets, 0, (iphrHash + 1) * sizeof(HR *));

    if (!pdbi1->fReadSymRecs())
        return FALSE;
    return readStream(sn);
}

// used by GSI1::fInit and PSGSI1::fInit
BOOL GSI1::readHash(SN sn, OFF offPoolInStream, CB cb)
{
    PB      pbHR = NULL;
    MSF *   pmsf = ppdb1->pmsf;
    int     cEntries = 0;
    CB      cbphr = 0;

#ifdef SMALLBUCKETS
    GSIHashHdr gsiHdr;
    cbphr = sizeof(gsiHdr);

    if (cb < cbphr) {
        ppdb1->setCorruptError();
        return false;
    }

    if (!pmsf->ReadStream(sn, offPoolInStream, &gsiHdr, &cbphr)) {
        ppdb1->setReadError();
        return false;
    }

    if (gsiHdr.verSignature == GSIHashHdr::hdrSignature &&
        gsiHdr.verHdr == GSIHashHdr::hdrVersion ) {
        // must be the new compressed buckets
        assert(cb == CB(sizeof(gsiHdr) + gsiHdr.cbHr + gsiHdr.cbBuckets));

        // must allocate the buffer for the records before we read in the buckets or
        // the fix up routines will generate garbage
        expect(fAlign(gsiHdr.cbHr));

        cEntries = gsiHdr.cbHr / sizeof(HRFile);
        CB  cbHR = cEntries * sizeof(HR);

        // allocate one record of slop at the beginning so we can step backwards thru
        // the begining of the hashrecs in fixHashIn without a memory violation
        pbHR = PB(new (poolSymHash) BYTE[cbHR + sizeof(HR)]);
        if (!pbHR) {
            ppdb1->setOOMError();
            return FALSE;
        }
        pbHR += sizeof(HR);

        CB cbLeft = cb - sizeof(gsiHdr);
        if (cbLeft < gsiHdr.cbHr || cbLeft - gsiHdr.cbHr < gsiHdr.cbBuckets) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        // funny deal - read in the pool of HR's then the buckets
        if (!pmsf->ReadStream(sn, offPoolInStream + sizeof(gsiHdr), pbHR, &gsiHdr.cbHr)) {
            ppdb1->setReadError();
            return FALSE;
        }
         
        if (!ExpandBuckets(sn, offPoolInStream + sizeof(gsiHdr) + gsiHdr.cbHr, gsiHdr.cbBuckets, (OFF*)rgphrBuckets)) {
            return FALSE;
        }
    }
    else 
#endif
    {
        // we persist the phr's as OFFs, so we need to do the right size here
        cbphr = sizeof(OFF) * (iphrHash + 1);

        if (cb < cbphr) {
            ppdb1->setCorruptError();
            return FALSE;
        }

        // must allocate the buffer for the records before we read in the buckets or
        // the fix up routines will generate garbage
        cb -= cbphr;
        expect(fAlign(cb));
        cEntries = cb / sizeof(HRFile);
        CB  cbHR = cEntries * sizeof(HR);

        // allocate one record of slop at the beginning so we can step backwards thru
        // the begining of the hashrecs in fixHashIn without a memory violation
        pbHR = PB(new (poolSymHash) BYTE[cbHR + sizeof(HR)]);
        if (!pbHR) {
            ppdb1->setOOMError();
            return FALSE;
        }
        pbHR += sizeof(HR);

        // funny deal - read in the pool of HR's then the buckets
        if (!pmsf->ReadStream(sn, offPoolInStream, pbHR, &cb) ||
            !pmsf->ReadStream(sn, offPoolInStream + cb, rgphrBuckets, &cbphr))
        {
            ppdb1->setReadError();
            return FALSE;
        }
    }

    // check to see if we need to fixup and/or expand the offsets
    //
    PREFAST_SUPPRESS(6285, "For 64-bit machines");
    if (sizeof(OFF) != sizeof(PHR) || sizeof(HR) != sizeof(HROffsetCalc)) {
        POFF        rgoffBuckets = POFF(rgphrBuckets);
        for (DWORD iphr = iphrHash; int(iphr) >= 0; iphr--) {
            // need sign extension!!!
            long_ptr_t l = rgoffBuckets[iphr];
            if (l != -1) {

                assert((ulong_ptr_t(l)/sizeof(HROffsetCalc))*sizeof(HROffsetCalc) == ulong_ptr_t(l));

                l /= sizeof(HROffsetCalc);
                l *= sizeof(HR);
            }
            rgphrBuckets[iphr] = PHR(l);
        }
    }


    if (!fixHashIn(pbHR, cEntries, pdbi1->bufSymRecs.Size())) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    MTS_PROTECT(pdbi1->m_csForSymRec);
    // we don't check whether the psym we read is correct.
    // all corruption should be detected and reported when we call psymForPhr
    // so we can use the GSI even if there entries that are corrupted.
    fixSymRecs((void *)1, pdbi1->bufSymRecs.Start());

    if (pdbi1->cvtsyms.fConverting()) {
        updateConvertedSymRecs();
    }

#ifdef HASH_REPORT
    // Print the statistical info about the pCHN
    DWORD nBuckets[256] = {0};

    for(DWORD h = 0; h <= iphrHash; h++) {
        HR* phr = rgphrBuckets[h];
        DWORD nChn;

        for(nChn = 0; phr != NULL; nChn++) {
            phr = phr->pnext;
        }

        nBuckets[nChn > 255 ? 255 : nChn]++;
    }

    printf("\nGSI:#Items\t#Buckets\n");

    DWORD nItems = 0;
    for(DWORD i1 = 0; i1 < 256; i1++) {
        if (nBuckets[i1] != 0) {
            printf("%8u\t%8u\n", i1, nBuckets[i1]);
        }

        nItems += nBuckets[i1] * i1;
    }

    printf("items/bucket = %u\n", nItems / (iphrHash + 1));
#endif

    return TRUE;
}

//
// we need to fix up the pointers from the hash to point to the new location
// of the symbols in the symrec buffer after they have been widened.  the
// offset map is in the cvtsyms object in the DBI1 object.
void GSI1::updateConvertedSymRecs()
{
    assert(pdbi1);
    MTS_ASSERT(pdbi1->m_csForSymRec);

    CvtSyms &   cvtsyms = pdbi1->cvtsyms;
    PB          pbBase = pdbi1->bufSymRecs.Start();

    assert(cvtsyms.fConverting());
    assert(pbBase);

    for (unsigned i = 0; i <= iphrHash; i++) {
        PHR phr;
        for (phr = rgphrBuckets[i]; phr; phr = phr->pnext) {
            PB  pbSym;
            if (pbSym = PB(phr->psym)) {
                ULONG   offOld = ULONG(pbSym - pbBase); // REVIEW:WIN64 CAST
                ULONG   offNew = cvtsyms.offSymNewForOffSymOld(offOld);
                phr->psym = PSYM(pbBase + offNew);
            }
        }
    }

}

BOOL GSI1::readStream(SN sn)
{
    if (sn == snNil)
        return TRUE;        // nothing to read

    // read in the hash bucket table from the dbi stream
    CB cb = ppdb1->pmsf->GetCbStream(sn);

    if (cb == cbNil)
        return TRUE;        // nothing to read

    return readHash(sn, 0, cb);
}

BOOL GSI1::fSave(SN* psn)
{
    assert(pdbi1->fWrite);
    return writeStream(psn);
}

// used by GSI1::fSave
BOOL GSI1::writeStream(SN* psn)
{
    assert(pdbi1->fWrite);
    if (!fEnsureSn(psn)) {
        ppdb1->setLastError(EC_LIMIT);
        return FALSE;
    }

    // ptrs in the stream are offsets biased by one to distinguish null ptrs/offsets
    MTS_ASSERT(pdbi1->m_csForSymRec);
    fixSymRecs(pdbi1->bufSymRecs.Start(), (void*)1);

    expect(fAlign(sizeof(rgphrBuckets)));
    // need to do a dummy replace here because fWriteHash just appends
    if (!ppdb1->pmsf->ReplaceStream(*psn, NULL, 0) ||
        !fWriteHash(*psn, NULL)){
        ppdb1->setWriteError();
        return FALSE;
    }

    return TRUE;
}

INTV GSI1::QueryInterfaceVersion()
{
    return intv;
}

IMPV GSI1::QueryImplementationVersion(){
    return impv;
}

PSYM GSI1::psymForPhr (HR *phr) 
{
    if (pdbi1->fReadSymRec(phr->psym))      // Will always return SZ symbol
        return phr->psym;
    else
        return NULL;
}

BOOL GSI1::getEnumSyms(EnumSyms ** ppEnum, PB pbSym) 
{
    assert(!pdbi1->fWrite);
    *ppEnum = NULL;
    if (pdbi1->fWrite) {
        return FALSE;
    }
    PSYM psym = (PSYM)pbSym;
    if (psym == NULL) {
        *ppEnum = new EnumGSISyms(this);
    } else {
        ST st;
        if (!fGetSymName(psym, &st)) {
            dassert(FALSE);
            return FALSE;
        }
        assert(!fNeedsSzConversion(psym));
        int iphr = hashSz(st);
        for (HR * phr = rgphrBuckets[iphr]; phr; phr = phr->pnext) {
            if (phr->psym == psym) {
                *ppEnum = new EnumGSISyms(this, phr, iphr);
                break;
            }
        }
    }
    return (*ppEnum != NULL);
}

PB GSI1::NextSym(PB pbSym)
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) {
        return NULL;
    }

    PSYM psym = (PSYM)pbSym;
    PHR phr = 0;
    DWORD iphr = DWORD(-1);

#ifndef PDB_MT
    if (psym && last.phr && last.phr->psym == psym) {
        // cache of position of last answer valid
        iphr = last.iphr;
        phr = last.phr;
    }
    else 
#endif
    if (psym) {
        ST st;
        if (!fGetSymName(psym, &st)) {
            dassert(FALSE);
            return NULL;
        }
        assert(!fNeedsSzConversion(psym));
        iphr = hashSz(st);
        for (phr = rgphrBuckets[iphr]; phr; phr = phr->pnext)
            if (phr->psym == psym)
                break;
        if (!phr) {
            dassert(FALSE);
            return NULL;
        }
    }
    // at this point, phr and iphr address the symbol that was last returned
    // advance to the next phr, if any
    if (phr)
        phr = phr->pnext;
    if (!phr)
        while (++iphr < iphrHash && !(phr = rgphrBuckets[iphr]))
            ;

    if (phr) {
        // success: save this last answer position for the next call
#ifndef PDB_MT
        last.iphr = iphr;
        last.phr  = phr;
#endif
        return (PB)psymForPhr(phr);
    }

    // no more entries; return no symbol
    return NULL;
}

inline static int caseInsensitiveComparePchPchCchCch(PCCH pch1, PCCH pch2, size_t cb1, size_t cb2, bool fIsAsciiString2)
{
#ifndef PDB_LIBRARY
    // Explicitly use "C" locale. When loaded in linker process, linker set's locale that 
    // results in _memicmp_l doing a bunch of other work before using "C" locale
    static auto locale = _create_locale(LC_ALL, "C");
#endif

    if (cb1 < cb2)
        return -1;
    else if (cb1 > cb2)
        return 1;
    else if (!fIsAsciiString2 || !IsASCIIString(pch1, cb1))
        return memcmp(pch1, pch2, cb1);
    else
#ifdef  PDB_LIBRARY
        return _memicmp(pch1, pch2, cb1);
#else
        return _memicmp_l(pch1, pch2, cb1, locale);
#endif
}

PB GSI1::HashSym(SZ_CONST szName, PB pbSym)
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) {
        return NULL;
    }

    PSYM psym = (PSYM)pbSym;
    PHR phr;
    int iphr;

    if (psym) {
        // Find the next sym after this one...
#ifndef PDB_MT
        if (last.phr && last.phr->psym == psym) {
            // cache of position of last answer valid
            phr = last.phr;
            iphr = last.iphr;
        }
        else 
#endif
        {
            // cache miss, find the sym on its bucket
            iphr = hashSz(szName);
            for (phr = rgphrBuckets[iphr]; phr; phr = phr->pnext)
                if (phr->psym == psym)
                    break;
            if (!phr) {
                // incoming sym not in this symbol table - start from scratch
                goto nosym;
            }
        }
        // we have reestablished phr; now advance it to next potential sym
        dassert(phr);
        phr = phr->pnext;
    }
    else {
nosym:
        iphr = hashSz(szName);
        phr = rgphrBuckets[iphr];
    }


    // At this point, phr may be 0, may address the next sym with the same name,
    // or may address some sym on the hash bucket before the HR we're looking for.
    // Search the HRs for the next sym with matching name, and return it, or 0
    // if not found.
    //
    // Note that since HR entries are sorted by memcmp of their syms' names, we
    // can exit early if the current HR is >= the name we're looking for.

    // Cache information for szName so it is not calculated every time
    size_t cbName = strlen(szName);
    bool fNameIsASCII = IsASCIIString(szName, cbName);

    for ( ; phr; phr = phr->pnext) {
        PSYM psymPhr = psymForPhr(phr);

        if (!psymPhr) {
            return 0;
        }

        ST st;

        if (!fGetSymName(psymPhr, &st)) {
            dassert(FALSE);
            return 0;
        }

        int icmp;

        assert(!fNeedsSzConversion(psymPhr));
        icmp = caseInsensitiveComparePchPchCchCch(st, szName, strlen(st), cbName, fNameIsASCII);

        if (icmp == 0) {
            // success: save this last answer position for the next call
#ifndef PDB_MT
            last.phr = phr;
            last.iphr = iphr;
#endif
            return (PB)psymForPhr(phr);
        }
        else if (icmp > 0)
            return 0;
    }
    return 0;
}

PB GSI1::HashSymW(const wchar_t *wszName, PB pbSym)
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) {
        return NULL;
    }
    USES_STACKBUFFER(0x400);

    UTFSZ utfszName = GetSZUTF8FromSZUnicode(wszName);
    if (!utfszName)
        return NULL;

    return HashSym(utfszName, pbSym);
}

unsigned long GSI1::OffForSym( BYTE* pbSym )
{
    assert(!pdbi1->fWrite);
    return DWORD( offForSym( PSYM( pbSym ) ) );
}

BYTE* GSI1::SymForOff( unsigned long off )
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) {
        return NULL;
    }
    PSYM psym;
    BOOL flag = fpsymFromOff( long( off ), &psym );
    assert( flag );
    return PB( psym );
}

// we have read in a list of dbi symrecs offsets and a rphrbuckets of offsets into
// this first table  -  we need to walk thru this abbreviated hash structure backwards
// and reconstruct a linked list hash table

// used by GSI1::readHash during fInit
bool GSI1::fixHashIn(PB pb, DWORD cEntries, CB cbSyms)
{
    cEntries--;

    for (DWORD iphr = iphrHash; int(iphr) >= 0; iphr--) {
        if (int_ptr_t(rgphrBuckets[iphr]) != -1) {
            rgphrBuckets[iphr] = PHR(pb + int_ptr_t(rgphrBuckets[iphr]));

            PHR     phrMin = rgphrBuckets[iphr];
            PHR     phr = PHR(pb) + cEntries;
            PHR     phrTail = NULL;
            PHRFile phrf = PHRFile(pb) + cEntries;

            if ((PB)phrMin < pb || phrMin > phr) {
                // corrupted
                return false;
            }

            for (; phr >= phrMin; phr--, phrf--, cEntries--) {

                // make sure the symbol offsets are within range when we load
                if (phrf->off > cbSyms) {
                    // corrupted
                    return false;
                }

                // See comment in HR::operator=(const HRFile &)!
                //
                *phr = *phrf;
                phr->pnext = phrTail;
                phrTail = phr;
            }
        }
        else {
            rgphrBuckets[iphr] = NULL;
        }
    }
    assert(cEntries == -1);
    return true;
}

BOOL GSI1::fWriteHash(SN sn, CB* pcb)
{
    assert(pdbi1->fWrite);

    // allocate size of buffer based on size of poolSymHash
    Buffer  buffer;

    if (poolSymHash.cbTotal > 0 ) {
        CB cbBuffer = CB((poolSymHash.cbTotal / sizeof(HR)) * sizeof(HRFile));
        if (!buffer.SetInitAlloc(cbBuffer)) {
            ppdb1->setOOMError();
            return FALSE;
        }
    }

    OFF     off = 0;
    CB      cb = 0;

    SafeStackAllocator<4096>  allocator;
    OFF * rgoffBuckets = reinterpret_cast<OFF *>(allocator.Alloc<OFF>(iphrHash + 1));

    if (rgoffBuckets == NULL) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    memset(rgoffBuckets, 0, sizeof(rgoffBuckets));

    // write out all the buckets except for the free list - lose them
    //
    for (unsigned iphr = 0; iphr < iphrHash; iphr++) {
        if (rgphrBuckets[iphr]) {
            PHR phr = rgphrBuckets[iphr];
            rgoffBuckets[iphr] = off;
            for (; phr; phr = phr->pnext) {
                // make sure that the psym field has already been
                // normalized to an offset.
                assert(phr->psym == PSYM(long_ptr_t(OFF(long_ptr_t(phr->psym)))));

                // If PDB is already corrupted, return FALSE.
                if (PSYM(long_ptr_t(phr->psym) & 0xffffffff) != phr->psym) {
                    ppdb1->setCorruptError();
                    return FALSE;
                }

                HRFile  hrf(*phr);

                if (!buffer.Append(PB(&hrf), sizeof(HRFile))) {
                    ppdb1->setOOMError();
                    return FALSE;
                }

                // NB: we need to persist offsets based on HROffsetCalc here
                // since we want to have the same data in the stream
                // regardless of Win32 or Win64.
                //
                off += sizeof(HROffsetCalc);
                cb += sizeof(HRFile);
            }
        }
        else {
            rgoffBuckets[iphr] = -1;    // neg one for null
        }
    }

    rgoffBuckets[iphrHash] = -1;        // lose the free bucket

    MSF *   pmsf = ppdb1->pmsf;

#ifdef SMALLBUCKETS
    Buffer bufBuckets;

    if (!CompressBuckets(&bufBuckets, rgoffBuckets)) {
        return FALSE;
    }

    GSIHashHdr gsiHdr;
    gsiHdr.verSignature = GSIHashHdr::hdrSignature;
    gsiHdr.verHdr = GSIHashHdr::hdrVersion;
    gsiHdr.cbHr = buffer.Size();
    gsiHdr.cbBuckets = bufBuckets.Size();

    if (!pmsf->AppendStream(sn, &gsiHdr, sizeof( gsiHdr ) ) ||
        !pmsf->AppendStream(sn, buffer.Start(), buffer.Size()) ||
        !pmsf->AppendStream(sn, bufBuckets.Start(), bufBuckets.Size())) {
        ppdb1->setWriteError();
        return FALSE;
    }

    expect(fAlign(sizeof(gsiHdr) + gsiHdr.cbHr + gsiHdr.cbBuckets));
    if (pcb) {
        *pcb = sizeof(gsiHdr) + gsiHdr.cbHr + gsiHdr.cbBuckets;
    }
#else
    if (!pmsf->AppendStream(sn, buffer.Start(), buffer.Size()) ||
        !pmsf->AppendStream(sn, rgoffBuckets, sizeof(rgoffBuckets))) {
        ppdb1->setWriteError();
        return FALSE;
    }

     expect(fAlign(cb + sizeof(rgoffBuckets)));
     if (pcb) {
         *pcb = cb + sizeof(rgoffBuckets);
     }
#endif

    return TRUE;
}

#ifdef SMALLBUCKETS

class CBitVect32GSISmartPointer
{
public:
    operator CBitVect32GSI*() const
    {
        return m_p;
    }

    CBitVect32GSISmartPointer(bool f, DWORD dw)
    {
        m_p = new CBitVect32GSI(f, dw);
    }

    ~CBitVect32GSISmartPointer()
    {
        delete m_p;
    }

private:
    CBitVect32GSI *m_p;
};

bool GSI1::CompressBuckets(Buffer* pbufBuckets, OFF *pbuckets)
{
    assert(pbufBuckets->Size() == 0);

    CBitVect32GSISmartPointer ps(false, iphrHash + 1);
    CBitVect32GSI *pbitvec = ps;

    if (pbitvec->CbRaw() == 0) {
        ppdb1->setOOMError();
        return false;
    }

    unsigned cNonEmpty = 0;

    for (unsigned iphr = 0; iphr <= iphrHash; iphr++) {
        if (pbuckets[iphr] != -1) {
            pbitvec->SetIBit(iphr, true);
            ++cNonEmpty;
        }
    }

    if (cNonEmpty == 0) {
        // nothing to write, so skip it.
        return true;
    }

    OFF* pOutBuckets = NULL;
    unsigned iOutBucket = 0;

    if (!pbufBuckets->Append(pbitvec->PbRaw(), pbitvec->CbRaw()) ||
        !pbufBuckets->Reserve(cNonEmpty * sizeof(pbuckets[0]), (BYTE**) &pOutBuckets)) {
        ppdb1->setOOMError();
        return false;
    }

    for (unsigned iphr2 = 0; iphr2 <= iphrHash; iphr2++) {
        if (pbuckets[iphr2] != -1) {
            pOutBuckets[iOutBucket++] = pbuckets[iphr2];
        }
    }

    assert(cNonEmpty == iOutBucket);
    return true;
}

bool GSI1::ExpandBuckets(SN sn, DWORD cbStart, DWORD cbBuckets, OFF* pbuckets)
{
    CBitVect32GSISmartPointer ps(false, iphrHash + 1);
    CBitVect32GSI *pbitvec = ps;

    if (pbitvec->CbRaw() == 0) {
        ppdb1->setOOMError();
        return false;
    }

    unsigned cNonEmpty = 0;
    Buffer bufOffs;
    MSF * pmsf = ppdb1->pmsf;
    OFF* pInBuckets = NULL;

    if (cbBuckets > 0) {
        DWORD cbphr = pbitvec->CbRaw();

        if (cbBuckets < cbphr) {
            ppdb1->setCorruptError();
            return false;
        }

        if (!pmsf->ReadStream(sn, cbStart, pbitvec->PbRaw(), (CB *) &cbphr)) {
            ppdb1->setReadError();
            return false;
        }

        cNonEmpty = pbitvec->CBitsSet();

        DWORD cbIn = cNonEmpty * sizeof(pbuckets[0]);

        if (cbBuckets - cbphr < cbIn) {
            ppdb1->setCorruptError();
            return false;
        }

        assert(CB(pbitvec->CbRaw() + cbIn) == cbBuckets);
        if (!bufOffs.Reserve(cbIn, (BYTE**)&pInBuckets)) {
            ppdb1->setOOMError();
            return false;
        }
        
        if (!pmsf->ReadStream(sn, cbStart + pbitvec->CbRaw(), pInBuckets, (CB *) &cbIn)) {
            ppdb1->setReadError();
            return false;
        }
    }

    unsigned iInBucket = 0;
    for (unsigned iphr = 0; iphr <= iphrHash; iphr++) {
        if ((*pbitvec)[iphr]) {
            pbuckets[iphr] = pInBuckets[iInBucket++];
        } else {
            pbuckets[iphr] = -1;
        }
    }

    assert(iInBucket == cNonEmpty);
    return true;
}

#endif

void GSI1::fixSymRecs(void* pOld, void* pNew)
{
    // either pdbi1->fWrite or pdbi1->m_csForSymRecs is held
    ptrdiff_t   cbDelta = PB(pNew) - PB(pOld);

    for (unsigned iphr = 0; iphr <= iphrHash; iphr++) {
        PHR phr;
        for (phr = rgphrBuckets[iphr]; phr; phr = phr->pnext) {
            if (phr->psym) {
                phr->psym  = PSYM(PB(phr->psym) + cbDelta);
            }
        }
    }
}

HASH GSI1::hashSt(ST st)
{
    return HashPbCb((PB)st + 1, cbForSt(st) - 1, iphrHash);
}

HASH GSI1::hashSz(SZ_CONST sz)
{
    return HashPbCb(PB(sz), strlen(sz), iphrHash);
}

BOOL GSI1::fGetFreeHR(PPHR pphr) {
    PPHR pphrFree = &rgphrBuckets[iphrHash];

    if (*pphrFree) {
        *pphr = *pphrFree;
        *pphrFree = (*pphrFree)->pnext; // unlink from free list
        return TRUE;
    }

    return FALSE;
}

void* HR::operator new(size_t size, GSI1* pgsi1) {
    assert(size == sizeof(HR));
    PHR phr;
    if (pgsi1->fGetFreeHR(&phr))
        return phr;
    else
        return new (pgsi1->poolSymHash) BYTE[sizeof(HR)];
}

BOOL GSI1::fInsertNewSym(PPHR pphr, PSYM psym, OFF *poff)
{
    assert(pdbi1->fWrite);
    dassert(pphr);
    dassert(psym);

    PHR phr = new (this) HR(*pphr, 0);

    if (!phr) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!pdbi1->fAddSym(psym, &(phr->psym)) ||
        !addToAddrMap(phr->psym))
    {
        return FALSE;
    }

    assert(offForSym(phr->psym) != OFF(-1));

    phr->pnext = *pphr;
    *pphr = phr;
    if (poff) {
        *poff = offForSym(phr->psym);
    }
    return TRUE;
}


// unlink the HR from its hash table chain and add it to the free list
inline BOOL GSI1::fUnlinkHR(PPHR pphr)
{
    assert(pdbi1->fWrite);

    PHR phr = *pphr;
    *pphr = (*pphr)->pnext;
    phr->pnext = rgphrBuckets[iphrHash];
    rgphrBuckets[iphrHash] = phr;
    return delFromAddrMap(phr->psym);
}

void GSI1::incRefCnt(PPHR pphr)
{
    assert(pdbi1->fWrite);

    assert(pphr);
    assert(*pphr);

    (*pphr)->cRef++;

}


BOOL GSI1::decRefCnt(OFF off)
{
    assert(pdbi1->fWrite);

    // off is an offset into the pdb's bufSymRec - we need get the symrec
    // from the pool and do a findsym to get the hr of the symbol so
    // we can dec the refcnt in the hr

    PSYM psym = 0;
    ST st = 0;
    PPHR pphr = 0;

    MTS_PROTECT(pdbi1->m_csForSymRec);

    if (!(fpsymFromOff(off, &psym)) || !psym ||
        !fGetSymName(psym, &st) || !st)
        return FALSE;

    assert(!fNeedsSzConversion(psym));

    while (fFindRec(st, &pphr)) {
        PSYM psymPhr = psymForPhr(*pphr);
        if (!psymPhr)
            return FALSE;
        if (!memcmp(psym, psymPhr, psym->reclen + sizeof(psym->reclen))) {
            // we found a match decrement the use count and return
            if (--((*pphr)->cRef) <= 0) {
                // refcnt is zero - put on the free list
                return fUnlinkHR(pphr);
            }
            return TRUE;
        }
    }

    return FALSE;
}

// a couple of template functions to do the right thing for
// the alignment values for refsym*'s
//
template <class S>
size_t cbPad(S *, size_t cb) {
    return cbAlign(offsetof(S, name) + cb);
}

template <class S>
size_t dcbPad(S *, size_t cb) {
    return dcbAlign(offsetof(S, name) + cb);
}

// a nameless REFSYM2 or a S_TOKENREF with a fixed size name
// are the only things that this is used for.
// 
struct REFSYM2_T : public REFSYM2 {
    char    rgchTokenNameExtra[8];
    char    rgchPad[1];
};


BOOL GSI1::removeGlobalRefsForMod(IMOD imod)
{
    MTS_PROTECT(pdbi1->m_csForPGSI);

    assert(pdbi1->fWrite);
    assert(ppdb1->m_fMinimalDbgInfo);

    Array<PPHR> rgpphr;

    for (unsigned iphr = 0; iphr < iphrHash; iphr++) {
        PPHR pphr = &rgphrBuckets[iphr];

        if (*pphr == NULL) {
            continue;
        }

        for (; *pphr != NULL; pphr = &((*pphr)->pnext)) {
            PSYM psym = psymForPhr(*pphr);

            assert(psym->rectyp == S_REF_MINIPDB);

            if (((REFMINIPDB *) psym)->imod == imod) {
                if (!rgpphr.append(pphr)) {
                    ppdb1->setOOMError();
                    return FALSE;
                }
            }
        }
    }

    for (DWORD i = 0; i < rgpphr.size(); i++) {
        PPHR pphr = rgpphr[i];

        if (*pphr == NULL) {
            continue;
        }

        fUnlinkHR(pphr);
    }

    return TRUE;
}


BOOL GSI1::packRefForMiniPDB(const char *szName, WORD imod, DWORD isectCoff,
                             bool fLocal, bool fData, bool fUDT, bool fLabel,
                             bool fConst)
{
    assert(pdbi1->fWrite);
    assert(ppdb1->m_fMinimalDbgInfo);

    // Compose the record

    SafeStackAllocator<1024>  allocator;
    size_t                    cbName = strlen(szName);
    size_t                    cbLen = cbPad((REFMINIPDB *) NULL, cbName + 1);
    REFMINIPDB *              pRef = reinterpret_cast<REFMINIPDB *>(allocator.Alloc<BYTE>(cbLen));

    if (pRef == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    pRef->reclen = static_cast<unsigned short>(cbLen - sizeof(pRef->reclen));
    pRef->rectyp = S_REF_MINIPDB;
    pRef->imod = imod;
    pRef->isectCoff = isectCoff;
    pRef->fLocal = fLocal;
    pRef->fData = fData;
    pRef->fUDT = fUDT;
    pRef->fLabel = fLabel;
    pRef->fConst = fConst;
    pRef->reserved = 0;

    strcpy_s((char *) pRef->name, cbName + 1, szName);
    memset(PB(pRef->name) + cbName + 1, 0, dcbPad(pRef, cbName + 1));

    // Try to add the record into global stream

    PPHR pphr = 0;

    while (fFindRec((ST) szName, &pphr)) {
        PSYM psymPhr = psymForPhr(*pphr);

        if (psymPhr == NULL) {
            return FALSE;
        }

        if (memcmp(pRef, psymPhr, pRef->reclen + sizeof(pRef->reclen)) == 0) {
            // Found a match, increment ref count and return

            incRefCnt(pphr);
            return TRUE;
        }

        if (pRef->fUDT || pRef->fConst) {
            // Just keep the first seen UDT in case of ODR violation

            ST st;

            if (!fGetSymName(psymPhr, &st)) {
                assert(false);
                return FALSE;
            }

            if (strcmp(st, szName) == 0) {
                incRefCnt(pphr);
                return TRUE;
            }
        }

        // we found a match name but not a matching record - if the record sought
        // is of global scope insert it before any of its matching local records

        if (!pRef->fLocal && !pRef->fUDT && !pRef->fLabel && !pRef->fConst) {
            return fInsertNewSym(pphr, (PSYM) pRef, NULL);
        }
    }

    if (pphr) {
        return fInsertNewSym(pphr, (PSYM) pRef, NULL);
    }

    // VSW:543853 - If the PDB is corrupt, fFindRec() can fail and leave pphr==NULL.
    // This happens on the path where fFindRec() calls psymForPhr() calls fReadSymRec(),
    // and fReadSymRec() detects corruption and calls setCorruptError(). In this case, we'll
    // just return FALSE and let the code calling us sort things out based on setCorruptError()
    // having been called.

    return FALSE;
}


BOOL GSI1::packRefSym(PSYM psym, IMOD imod, OFF off, OFF *poff)
{
    assert(pdbi1->fWrite);
    assert(!fNeedsSzConversion(psym));

    SZ szName;

    if (!fGetSymName(psym, reinterpret_cast<ST*>(&szName))) {
        // only S_ANNOTATION syms get through here w/o a name.
        assert(psym->rectyp == S_ANNOTATION);
        szName = "";
    }
    
    SafeStackAllocator<1024>    allocator;
    size_t                      cbName = strlen(szName);
    size_t                      cbLen = cbAlign(sizeof(REFSYM2) + cbName + 1);
    REFSYM2 *                   pRefSym = reinterpret_cast<REFSYM2 *>(allocator.Alloc<BYTE>(cbLen));

    if (NULL == pRefSym) {
        ppdb1->setOOMError();
        return FALSE;
    }

    // form the refsym2 record
    pRefSym->reclen =
        static_cast<unsigned short>(cbPad(pRefSym, cbName + 1) - sizeof(pRefSym->reclen));
    if (psym->rectyp == S_ANNOTATION) {
        pRefSym->rectyp = S_ANNOTATIONREF;
    }
    else {
        pRefSym->rectyp = fSymIsData(psym) ? S_DATAREF : (fSymIsGlobal(psym) ? S_PROCREF : S_LPROCREF);
    }
    pRefSym->sumName = 0;
    pRefSym->ibSym = off;
    pRefSym->imod = ximodForImod(imod);
    
    strcpy_s(SZ(pRefSym->name), cbName + 1, szName);

    memset(
        PB(pRefSym->name) + cbName + 1,
        0,
        dcbPad(pRefSym, cbName + 1)
        ); //align pad with zeros

    return packSym(reinterpret_cast<PSYM>(pRefSym), poff);

}

BOOL GSI1::packTokenRefSym(PSYM psym, IMOD imod, OFF off, OFF * poff)
{
    assert(pdbi1->fWrite);

    // token names are always 9 bytes long, including the trailing NULL
    //
    const size_t    cbName = 9;
    
    REFSYM2_T       refsym2;

    memset(&refsym2, 0, sizeof(refsym2));
    refsym2.reclen = static_cast<ushort>(cbPad(&refsym2, cbName) - sizeof(refsym2.reclen));
    refsym2.rectyp = S_TOKENREF;
    refsym2.ibSym  = off;
    refsym2.imod   = ximodForImod(imod);

    CV_tkn_t    token = 0;

    switch (psym->rectyp) {

    case S_GMANPROC:
    case S_LMANPROC:
        token = reinterpret_cast<MANPROCSYM*>(psym)->token;
        break;

    default:
        assert(false);
#if !defined(_DEBUG)
        __assume(false);
#endif
    }
    
    assert(token != 0);
    PREFAST_SUPPRESS(6202, "REFSYM2_T::rgchTokenNameExtra allocates the remaining bytes, so we don't actually overflow here");
    sprintf_s(reinterpret_cast<SZ>(refsym2.name), cbName, "%08x", token);

    memset(
        refsym2.name + cbName,
        0,
        dcbPad(&refsym2, cbName)
        ); //align pad with zeros

    return packSym(reinterpret_cast<PSYM>(&refsym2), poff);
}


BOOL GSI1::packSym(PSYM psym, OFF *poff)
{
    assert(pdbi1->fWrite);
    assert(!fNeedsSzConversion(psym));

    ST st;
    
    if (!fGetSymName(psym, &st)) {
        return FALSE;
    }

    if (!st) {
        return FALSE;
    }

    PPHR pphr = 0;

    while (fFindRec(st, &pphr)) {
        PSYM psymPhr = psymForPhr(*pphr);
        if (!psymPhr)
            return FALSE;
        if (!memcmp(psym, psymPhr, psym->reclen + sizeof(psym->reclen))) {
            // we found a match increment/decrement the use count and return
            incRefCnt(pphr);
            *poff = offForSym(psymPhr);
            assert(*poff != OFF(-1));
            return TRUE;
        }
        // we found a match name but not a matching record - if the record sought
        // is of global scope insert it before any of its matching local records
        // NB:  If the incoming symbol is global, make sure it goes in ahead of
        // *any* other symbols, including other globals with the same name but
        // different addresses.  This happens when ILINK moves data items.
        // DevStudio96:19016 [drs, 12/10/96]
        //
        if (fSymIsGlobal(psym)) {
            return fInsertNewSym(pphr, psym, poff);
        }
    }

    if (pphr) {
        // Brand new public
        return fInsertNewSym(pphr, psym, poff);
    } else {
        // VSW:543853 - If the PDB is corrupt, fFindRec() can fail and leave pphr==NULL.
        // This happens on the path where fFindRec() calls psymForPhr() calls fReadSymRec(),
        // and fReadSymRec() detects corruption and calls setCorruptError(). In this case, we'll
        // just return FALSE and let the code calling us sort things out based on setCorruptError()
        // having been called.
        return FALSE;
    }
}

// special packing for const and udt symbols.  we don't want to do the normal packing
// into global scope if it is already there and doesn't match.  The first one
// with a particular name into DBI wins.  Those that don't match get copied
// into the MOD symbols.  Those that match are promoted to global scope.
//
// spd return value is only valid to look at when TRUE is returned.
//
BOOL GSI1::packSymSPD(PSYM psym, OFF * poff, SPD & spd)
{
    assert(pdbi1->fWrite);

    // Only have tested S_CONSTANT and S_UDT records
    //
    assert(psym->rectyp == S_CONSTANT || psym->rectyp == S_UDT);

    spd = spdInvalid;

    ST      st;
    size_t  cbSt;

    if (!fGetSymName(psym, &st) || !st) {
        return FALSE;
    }

    cbSt = strlen(st);

    HR **   pphr = NULL;
    while (fFindRec(st, &pphr)) {

        PSYM    psymPhr;
        if (!(psymPhr = psymForPhr(*pphr))) {
            return FALSE;
        }

        ST  stPhr;
        if (!fGetSymName(psymPhr, &stPhr)) {
            return FALSE;
        }

        size_t  cbStPhr;

        cbStPhr = strlen(stPhr);

        if (cbSt == cbStPhr && memcmp(st, stPhr, cbSt) == 0) {
            // Names were identical.  Our SPD is now mismatch,
            // as we won't be emitting to global space due to
            // a name match/record mismatch
            // However, we need to keep looking for other records
            // in the hash chain for an exact match.
            spd = spdMismatch;

            if (memcmp(psym, psymPhr, psym->reclen + sizeof(psym->reclen)) == 0) {
                // we found a match increment/decrement the use count and return
                incRefCnt(pphr);
                *poff = offForSym(psymPhr);
                assert(*poff != OFF(-1));
                spd = spdIdentical;
                return TRUE;
            }
        }
    }
    if (pphr) {
        if (spd != spdMismatch) {
            assert(spd == spdInvalid);
            // need to insert the symbol
            if (fInsertNewSym(pphr, psym, poff)) {
                spd = spdAdded;
            }
            else {
                return FALSE;
            }
        }
        return TRUE;    
    } else {
        // VSW:543853 - If the PDB is corrupt, fFindRec() can fail and leave pphrFirst==NULL.
        // This happens on the path where fFindRec() calls psymForPhr() calls fReadSymRec(),
        // and fReadSymRec() detects corruption and calls setCorruptError(). In this case, we'll
        // just return FALSE and let the code calling us sort things out based on setCorruptError()
        // having been called.
        return FALSE;
    }
}

//
// Find the record in the hash table which has a name equal to sz when comparing in a case-insensitive
// manner.
//
// Returns TRUE if a record with a matching name (when comparing in a case-insensitive manner) is
// found, and sets *ppphr to point to that record.
//
// Returns FALSE if a record is not found, and in this case *ppphr can be:
//   1. Pointing to the record after which we would want to insert a new record (i.e. the record
//      before the insertion point).
//   2. NULL, in which case there was an error indicating a corrupt PDB record (not an ideal means
//      of communicating this error, but this is what happens in this code today).
//
// TODO: This code (among much more) should change to using a more descriptive return type that 
// would indicate failures such as corrupt PDBs in a more clear fashion.
//
BOOL GSI1::fFindRec(SZ sz, OUT PPHR* ppphr)
{
    PPHR pphr = *ppphr;
    BOOL retval = FALSE;

    if (!pphr) {
        pphr = &rgphrBuckets[hashSz(sz)];
    }
    else {
        pphr = &((*pphr)->pnext);
    }

    // Force *ppphr to NULL so that if we return early, we can indicate failure to read a record.
    *ppphr = NULL; 

    // Cache information for sz so it is not calculated every time
    size_t cbSz = strlen(sz);
    size_t fSzIsASCII = IsASCIIString(sz, cbSz);

    while (*pphr) {
        ST stTab;
        PSYM psymPhr = psymForPhr(*pphr);
        if (!psymPhr)
            return FALSE;
        if (fGetSymName(psymPhr, &stTab)) {
            dassert(stTab);
            int icmp;

            assert(!fNeedsSzConversion(psymPhr));

            icmp = caseInsensitiveComparePchPchCchCch(stTab, sz, strlen(stTab), cbSz, fSzIsASCII);

            if (icmp == 0) {
                retval = TRUE;
                break;
            }
            else if (icmp > 0)
                break;
        }
        pphr = &((*pphr)->pnext);
    }

    *ppphr = pphr;
    return retval;
}

BOOL GSI1::delFromAddrMap(PSYM psym)
{
    assert(pdbi1->fWrite);
    return TRUE;        // no AddrMap here
}

BOOL GSI1::addToAddrMap(PSYM psym)
{
    assert(pdbi1->fWrite);
    return TRUE;        // no AddrMap here
}

// PUBLIC GSI specific methods

inline int cmpAddrMapByAddr(ISECT isect1, UOFF uoff1, ISECT isect2, UOFF uoff2)
{
    dassert(sizeof(UOFF) == sizeof(long));
    dassert(sizeof(ISECT) == sizeof(short));

    return (isect1 == isect2) ? (long)uoff1 - (long)uoff2 : (short)isect1 - (short)isect2;
}

inline ISECT isectForPub(PSYM psym)
{
    dassert(psym->rectyp == S_PUB32 || psym->rectyp == S_PUB32_ST);
    return (ISECT) ((PUBSYM32*)psym)->seg;
}

inline UOFF uoffForPub(PSYM psym)
{
    dassert(psym->rectyp == S_PUB32 || psym->rectyp == S_PUB32_ST);
    return (UOFF) ((PUBSYM32*)psym)->off;
}

inline int cmpAddrMapByAddr(ISECT isect, UOFF uoff, PSYM psym)
{
    return cmpAddrMapByAddr(isect, uoff, isectForPub(psym), uoffForPub(psym));
}

inline int __cdecl cmpAddrMapByAddr(const void* pelem1, const void* pelem2)
{
    PSYM psym1 = *(PSYM*)pelem1;
    PSYM psym2 = *(PSYM*)pelem2;
    return cmpAddrMapByAddr(isectForPub(psym1), uoffForPub(psym1), psym2);
}

inline int __cdecl cmpAddrMapByPos(const void* pelem1, const void* pelem2)
{
    PB p1 = *(PB*)pelem1;
    PB p2 = *(PB*)pelem2;

    if (p1 < p2) {
        return -1;
    }
    if (p1 > p2) {
        return 1;
    }
    return 0;
}

inline int __cdecl cmpAddrMapByAddrAndName(const void* pelem1, const void* pelem2)
{
    PUBSYM32 *p1 = *(PUBSYM32**)pelem1;
    PUBSYM32 *p2 = *(PUBSYM32**)pelem2;

    dassert(p1->rectyp == S_PUB32 && p2->rectyp == S_PUB32);

    int ret = cmpAddrMapByAddr(pelem1, pelem2);

    if (ret != 0) {
        return ret;
    }

    return strcmp((ST)p1->name, (ST)p2->name);
}

PB PSGSI1::NearestSym (ISECT isect, OFF off, OUT POFF pdisp)
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) { 
        return NULL;
    }

    if (bufCurAddrMap.Size() == 0) {
        return NULL;
    }

    PB pb;
    if ((pb = pbInThunkTable(isect, off, pdisp)) != NULL)
        return pb;

    PSYM* ppsymLo = (PSYM*)bufCurAddrMap.Start();
    PSYM* ppsymHi = (PSYM*)bufCurAddrMap.End() - 1;

    while (ppsymLo < ppsymHi) {
        PSYM* ppsym = ppsymLo + ((ppsymHi - ppsymLo + 1) >> 1);

        if ( !pdbi1->fReadSymRec(*ppsym) && !fIsDummyThunkSym(*ppsym)) {
            return NULL;
        }

        int cmp = cmpAddrMapByAddr(isect, (UOFF)off, *ppsym);

        if (cmp < 0)
            ppsymHi = ppsym - 1;
        else if (cmp > 0)
            ppsymLo = ppsym;
        else
            ppsymLo = ppsymHi = ppsym;
    }

    // Need load symbol to get (section, offset).
    if (!pdbi1->fReadSymRec(*ppsymLo) && !fIsDummyThunkSym(*ppsymLo)) {
        return NULL;
    }

    if (isectForPub(*ppsymLo) == isect) {
        // Found a symbol, not in boundary condition as in the else part.
        // Check for same addr symbol due to ICF.  If exists, return the
        // first one from the list.

        PSYM *ppsym = ppsymLo - 1;

        while (ppsym >= (PSYM*)bufCurAddrMap.Start() &&
               (fIsDummyThunkSym(*ppsym) || pdbi1->fReadSymRec(*ppsym)) &&
               cmpAddrMapByAddr(ppsym, ppsymLo) == 0) {
            ppsymLo--;
            ppsym--;
        }
    }
    else {
        // Boundary conditions.
        // Example: given publics at (a=1:10, b=1:20, c=2:10, d=2:20, e=4:0),
        // search for (1: 9) returns (a,-1)
        //        for (1:11) returns (a,1)
        //        for (1:21) returns (b,1)
        //        for (2: 9) returns (c,-1)
        //        for (2:11) returns (c,1)
        //        for (2:21) returns (d,1)
        //        for (3:xx) returns NULL
        // so, for cases (2:9), we must advance ppsymLo from (1:21) to (2:9)
        //
        // Need to use a loop instead of justing returning the next symbol,
        // since due to ICF, ppsymLo may not point to the last among symbols
        // having same address.

        while (isectForPub(*ppsymLo) < isect) {
            ++ppsymLo;

            if (ppsymLo == (PSYM*)bufCurAddrMap.End()) {
                return NULL;
            }

            if (!pdbi1->fReadSymRec(*ppsymLo) && !fIsDummyThunkSym(*ppsymLo)) {
                return NULL;
            }

            if (isectForPub(*ppsymLo) > isect) {
                return NULL;
            }
        }
    }

    *pdisp = static_cast<OFF>(UOFF(off) - uoffForPub(*ppsymLo));
    return reinterpret_cast<PB>(*ppsymLo);
}


BOOL PSGSI1::fInit(SN sn_)
{
    rgphrBuckets = (HR **) (new (poolSymHash) BYTE[(iphrHash + 1) * sizeof(HR *)]);

    if (rgphrBuckets == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }

    memset(rgphrBuckets, 0, (iphrHash + 1) * sizeof(HR *));

    if (!pdbi1->fReadSymRecs())
        return FALSE;
    sn = sn_;   // need to remember stream for incremental merge

    if (!readStream()) {
        return FALSE;
    }
#ifdef PDB_MT
    if ((rgFakePubDef = new (thunkpool) PB[psgsihdr.nThunks]) == NULL) {
        ppdb1->setOOMError();
        return FALSE;
    }
    for(unsigned i = 0; i < psgsihdr.nThunks; i++) {
        rgFakePubDef[i] = NULL;
    }
#endif
    return TRUE;
}

// used by PSGSI1::fInit
BOOL PSGSI1::readStream()
{
    if (sn == snNil) {
        fCreate = TRUE;
        return TRUE;        // nothing to read
    }

    MSF *   pmsf = ppdb1->pmsf;

    // read in the hash bucket table from the dbi stream
    CB cb = pmsf->GetCbStream(sn);

    if (cb == cbNil)
        return TRUE;        // nothing to read

    // read in the header
    CB cbHdr = sizeof(PSGSIHDR);
    if (cb < cbHdr) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    if (!pmsf->ReadStream(sn, 0, &psgsihdr, &cbHdr))    {
        ppdb1->setReadError();
        return FALSE;
    }

    if (cb - cbHdr < psgsihdr.cbSymHash) {
        ppdb1->setCorruptError();
        return FALSE;
    }

    if (!readHash(sn, sizeof(PSGSIHDR), psgsihdr.cbSymHash))
        return FALSE;

    // if we are updating a pdb don't bother to read in the AddrMap until we are
    // ready to save the Publics
    return (fWrite || readAddrMap());

}

BOOL PSGSI1::readAddrMap(bool fConvert)
{
    if (sn == snNil)
        return FALSE;

    if (!psgsihdr.cbAddrMap)
        return TRUE;

    expect(fAlign(psgsihdr.cbAddrMap));

    // fixup the sizes we need to allocate
    //
    CB  cbAddrMap = psgsihdr.cbAddrMap;
    CB  cbAddrMapAlloc = psgsihdr.cbAddrMap;
    if (sizeof(OFF) != sizeof(uint_ptr_t)) {
        cbAddrMapAlloc /= sizeof(OFF);
        cbAddrMapAlloc *= sizeof(uint_ptr_t);
    }

    if (!bufCurAddrMap.Reserve(cbAddrMapAlloc)) {
        ppdb1->setOOMError();
        return FALSE;
    }

    if (!ppdb1->pmsf->ReadStream(
            sn,
            sizeof(PSGSIHDR) + psgsihdr.cbSymHash,
            bufCurAddrMap.Start(),
            &cbAddrMap)
        ||
        cbAddrMap != psgsihdr.cbAddrMap)
    {
        ppdb1->setReadError();
        return FALSE;
    }

    // now, fix up the 32-bit offsets to be 64-bit offset/pointers.
    //
    if (sizeof(OFF) != sizeof(uint_ptr_t)) {
        POFF            poffMin = POFF(bufCurAddrMap.Start());
        POFF            poff = POFF(bufCurAddrMap.Start() + psgsihdr.cbAddrMap) - 1;
        plong_ptr_t     pw = plong_ptr_t(bufCurAddrMap.End()) - 1;

        while (poff >= poffMin) {
            *pw-- = *poff--;
        }
    }

    // Convert buffer element from offset to pointer.
    //
    if (fConvert) {
        assert(pdbi1);
        MTS_PROTECT(pdbi1->m_csForSymRec);

        if (pdbi1->cvtsyms.fConverting()) {
            fixupAddrMapForConvertedSyms(bufCurAddrMap,
                    long_ptr_t(pdbi1->bufSymRecs.Start()));
        }
        else {
            fixupAddrMap(bufCurAddrMap, long_ptr_t(pdbi1->bufSymRecs.Start()));
        }
    }

    return TRUE;
}

BOOL PSGSI1::readThunkMap()
{
    assert(!pdbi1->fWrite);
    MTS_PROTECT(m_csForLoad);
        
    if (bufThunkMap.Start())
        return TRUE;        // already read it - return

    if (sn == snNil)
        return FALSE;

    if ( !psgsihdr.nThunks ) {
        return TRUE;       // No thunks, nothing to load
    }

    CB cbThunkMap;
    CB cbSectMap;
    if (!bufThunkMap.Reserve(cbThunkMap = cbSizeOfThunkMap()) ||
        !bufSectMap.Reserve(cbSectMap = cbSizeOfSectMap())) {
        ppdb1->setOOMError();
        return FALSE;
    }

    expect(fAlign(cbThunkMap));
    expect(fAlign(cbSectMap));

    MSF *   pmsf = ppdb1->pmsf;

    BOOL fResult = pmsf->ReadStream(
        sn, 
        sizeof(PSGSIHDR) + psgsihdr.cbSymHash + psgsihdr.cbAddrMap,
        bufThunkMap.Start(), 
        &cbThunkMap);

    fResult = fResult && pmsf->ReadStream(
        sn, 
        sizeof(PSGSIHDR) + psgsihdr.cbSymHash + psgsihdr.cbAddrMap + cbThunkMap,
        bufSectMap.Start(),
        &cbSectMap);
    
    if ( !fResult ) {
        ppdb1->setReadError();
        return FALSE;
    }

    // Add an entry in addrmap so NearestSym() will
    // identify these addresses as well.
    PUBSYM32 *pPubSym = reinterpret_cast< PUBSYM32* >( rgbThunkTableSym );
    pPubSym->rectyp = S_PUB32;
    pPubSym->pubsymflags.grfFlags = 0;
    pPubSym->pubsymflags.fCode = true;

    pPubSym->off = psgsihdr.offThunkTable;
    pPubSym->seg = psgsihdr.isectThunkTable;
    strncpy_s( SZ(pPubSym->name), sizeof(rgbThunkTableSym) - offsetof( PUBSYM32, name ), ".Base", _TRUNCATE);

    pPubSym->reclen = sizeof(PUBSYM32) + 6;

    if ( !bufCurAddrMap.Append( PB(&pPubSym), sizeof(pPubSym) ) ) {
        ppdb1->setOOMError();
        return false;
    }

    typedef PSYM *PPSYM;
    PPSYM ppsym = reinterpret_cast<PPSYM>(bufCurAddrMap.Start());
    PPSYM ppsymEnd = reinterpret_cast<PPSYM>(bufCurAddrMap.End());

    ppsymEnd--;

    for( ; ppsym < ppsymEnd; ppsym++ ) {
        if ( !pdbi1->fReadSymRec( *ppsym ) ) {
            return false;
        }
        if ( cmpAddrMapByAddr( ppsym, &pPubSym ) >= 0 ) {
            memmove( ppsym + 1, 
                ppsym, 
                ( ppsymEnd - ppsym ) * sizeof( PPSYM ) );
            *ppsym = reinterpret_cast<PSYM>(pPubSym);
            break;
        }
    }

    return TRUE;
}

BOOL PSGSI1::fSave(SN* psn)
{
    assert(pdbi1->fWrite);

    // If no previous public symbols, just write out all the
    // records we have collected.

    if (fCreate) {
        sortBuf(bufNewAddrMap);
        return writeStream(psn, bufNewAddrMap);
    }

    // Incremental update public.

    if (mergeAddrMap()) {
        fixupAddrMap(bufCurAddrMap, long_ptr_t(pdbi1->bufSymRecs.Start()));
        fixupAddrMap(bufNewAddrMap, long_ptr_t(pdbi1->bufSymRecs.Start()));

        // ensure that all sym records are loaded
        if (!readSymsInAddrMap(bufCurAddrMap) ||
            !readSymsInAddrMap(bufNewAddrMap)) {
            return FALSE;
        }
        
        sortBuf(bufResultAddrMap);
        return writeStream(psn, bufResultAddrMap);
    }

    return FALSE;
}

BOOL PSGSI1::Close()
{
    assert(pdbi1);
#ifdef PDB_MT
    MTS_PROTECT(pdbi1->m_csForPGSI);
    if (--refcount != 0) {
        assert(!pdbi1->fWrite);
        return TRUE;
    }
#endif
    pdbi1->pgsiPS = NULL;
    delete this;
    return TRUE;
}

PSGSI1::~PSGSI1()
{
}

// mergeAddrMap -- A three way merge for the incremental update of the AddrMap.
//
// 1. bufCurAddrMap is read here - it actually represents a sorted list of the
//    previous AddrMap.
//
// 2. sort bufCurAddrMap and bufDelAddrMap, based on symbol's offset in symbol
//    buffer, instead of on symbol's address.  Since each element is an offset,
//    there won't be duplicate.
//
// 3. do the three way merge, considering the case where a newly added element
//    could be invalidated (deleted) later (DevDiv #94763).

BOOL PSGSI1::mergeAddrMap()
{
    assert(pdbi1->fWrite);

    // read in the previous addr map - it is sorted by sym addr
    if (!readAddrMap(false)) {
        return FALSE;
    }

    // sort the previous and deleted addr maps by sym's offset in sym buffer
    // (the new addr map is sorted already)
    sortBuf2(bufCurAddrMap);
    sortBuf2(bufDelAddrMap);

    PSYM* ppsymNew = (PSYM*) bufNewAddrMap.Start();
    PSYM* ppsymDel = (PSYM*) bufDelAddrMap.Start();
    PSYM* ppsymCur = (PSYM*) bufCurAddrMap.Start();

    bool newValid = (PB) ppsymNew < bufNewAddrMap.End();
    bool delValid = (PB) ppsymDel < bufDelAddrMap.End();
    bool curValid = (PB) ppsymCur < bufCurAddrMap.End();

    while (curValid || newValid) {
        expect(fAlign(ppsymNew));
        expect(fAlign(ppsymDel));
        expect(fAlign(ppsymCur));

        bool fNew = false;

        if (curValid && newValid && (*ppsymNew <= *ppsymCur)) {
            fNew = true;
        }
        else if (!curValid && newValid) {
            fNew = true;
        }

        PSYM psym;

        if (fNew) {
            psym = *ppsymNew;
            newValid = (PB) (++ppsymNew) < bufNewAddrMap.End();
        }
        else {
            psym = *ppsymCur;
            curValid = (PB) (++ppsymCur) < bufCurAddrMap.End();
        }

        bool fKeep = true;

        while (delValid) {
            if (psym == *ppsymDel) {
                fKeep = false;
                delValid = (PB) (++ppsymDel) < bufDelAddrMap.End();
                break;
            }

            if (psym < *ppsymDel) {
                break;
            }

            delValid = (PB) (++ppsymDel) < bufDelAddrMap.End();
        }

        if (fKeep) {
            if (!bufResultAddrMap.Append((PB) &psym, sizeof(PSYM))) {
                ppdb1->setOOMError();
                return FALSE;
            }
        }
    }

    return TRUE;
}

// Fix up buffer elements to point to symbol records, and
// then sort the buffer by symbol address and name.
void PSGSI1::sortBuf(Buffer& buf)
{
    assert(pdbi1->fWrite);

    MTS_ASSERT(pdbi1->m_csForSymRec);
    if (buf.Size()) {
        fixupAddrMap(buf, long_ptr_t(pdbi1->bufSymRecs.Start()));
        qsort(buf.Start(), buf.Size()/sizeof(PSYM), sizeof(PSYM), cmpAddrMapByAddrAndName);
    }
}

// Sort buffer elements as if they are integers.
void PSGSI1::sortBuf2(Buffer& buf)
{
    assert(pdbi1->fWrite);

    MTS_ASSERT(pdbi1->m_csForSymRec);
    if (buf.Size()) {
        qsort(buf.Start(), buf.Size()/sizeof(PSYM), sizeof(PSYM), cmpAddrMapByPos);
    }
}

// used by PSGSI1::fSave
BOOL PSGSI1::writeStream(SN* psn, Buffer& bufAddrMap)
{
    assert(pdbi1->fWrite);
    if (!fEnsureSn(psn)) {
        ppdb1->setLastError(EC_LIMIT);
        return FALSE;
    }

    MTS_ASSERT(pdbi1->m_csForSymRec);
    fixupAddrMap(bufAddrMap, -long_ptr_t(pdbi1->bufSymRecs.Start()));

    // ptrs in the stream are offsets biased by one to distinguish null ptrs/offsets
    fixSymRecs(pdbi1->bufSymRecs.Start(), (void*)1);
    psgsihdr.cbAddrMap = bufAddrMap.Size();

    // if we are on win64, we need to adjust the size of the address map and fixup
    // a new buffer of the actual offsets
    Buffer  bufT;
    Buffer *pbuf = &bufAddrMap;

    if (sizeof(OFF) != sizeof(long_ptr_t)) {
        assert((psgsihdr.cbAddrMap % sizeof(long_ptr_t)) == 0);
        psgsihdr.cbAddrMap = (psgsihdr.cbAddrMap / sizeof(long_ptr_t)) * sizeof(OFF);
        if (psgsihdr.cbAddrMap) {
            if (!bufT.SetInitAlloc(psgsihdr.cbAddrMap)) {
                ppdb1->setOOMError();
                return FALSE;
            }
            pbuf = &bufT;
            plong_ptr_t p = plong_ptr_t(bufAddrMap.Start());
            plong_ptr_t pMax = plong_ptr_t(bufAddrMap.End());

            for (; p < pMax; p++) {
                OFF off = OFF(*p);
                verify(bufT.Append(PB(&off), sizeof(off)));
            }
        }
        else {
            // Any extra checking??? 
        }
    }

    expect(fAlign(sizeof(psgsihdr)));
    expect(fAlign(sizeof(rgphrBuckets)));
    expect(fAlign(psgsihdr.cbSymHash));
    expect(fAlign(psgsihdr.cbAddrMap));

    MSF *   pmsf = ppdb1->pmsf;

    if (!pmsf->ReplaceStream(*psn, &psgsihdr, sizeof(psgsihdr)) ||
        !fWriteHash(*psn, &psgsihdr.cbSymHash) ||
        !pmsf->AppendStream(*psn, pbuf->Start(), pbuf->Size()) ||
        !pmsf->AppendStream(*psn, bufThunkMap.Start(), bufThunkMap.Size()) ||
        !pmsf->AppendStream(*psn, bufSectMap.Start(), bufSectMap.Size()) ||
        !pmsf->WriteStream(*psn, 0, &psgsihdr, sizeof(psgsihdr))) {
        ppdb1->setWriteError();
        return FALSE;
    }

    return TRUE;
}

BOOL PSGSI1::addToAddrMap(PSYM psym)
{
    assert(pdbi1->fWrite);
    long_ptr_t  off = PB(psym) - pdbi1->bufSymRecs.Start();
    if (!bufNewAddrMap.Append(PB(&off), sizeof(off))) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

BOOL PSGSI1::delFromAddrMap(PSYM psym)
{
    assert(pdbi1->fWrite);
    if (fCreate)
        return  TRUE;       // don't bother

    long_ptr_t  off = PB(psym) - pdbi1->bufSymRecs.Start();
    if (!bufDelAddrMap.Append(PB(&off), sizeof(off))) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

void PSGSI1::fixupAddrMap(Buffer& buf, long_ptr_t doff)
{
    plong_ptr_t poffMax = plong_ptr_t(buf.End());

    for (plong_ptr_t poff = plong_ptr_t(buf.Start()); poff < poffMax; poff++) {
        *poff += doff;
    }
}

void PSGSI1::fixupAddrMapForConvertedSyms(Buffer & buf, long_ptr_t off)
{
    assert(pdbi1);
    assert(pdbi1->cvtsyms.fConverting());
    assert(!pdbi1->fWrite);
    assert(off > 0);    // further assurance we are fixing up after a read

    CvtSyms &   cvtsyms = pdbi1->cvtsyms;
    plong_ptr_t poffMax = plong_ptr_t(buf.End());

    for (plong_ptr_t poff = plong_ptr_t(buf.Start()); poff < poffMax; poff++) {
        assert(OFF(*poff) == *poff);
        *poff = cvtsyms.offSymNewForOffSymOld(ULONG(*poff)) + off;
    }
}

BOOL PSGSI1::readSymsInAddrMap (Buffer& buf)
{
    for (PSYM* ppsym = (PSYM*)buf.Start(); ppsym < (PSYM*)buf.End(); ppsym++)
        if (!pdbi1->fReadSymRec(*ppsym))
            return FALSE;
    return TRUE;
}

BOOL PSGSI1::removeSym (SZ_CONST szName)
{
    assert(pdbi1->fWrite);

    PPHR pphr = 0;

    if (fFindRec((SZ)szName, &pphr)) {
        do {
            PUBSYM32* ppubHR = (PUBSYM32*)(psymForPhr(*pphr));
            dassert(ppubHR->rectyp == S_PUB32);

            if (strcmp((ST)szName, (ST)ppubHR->name) == 0) {
                // found the public with same name (case sensitive)

                return fUnlinkHR(pphr);
            }
        } while (fFindRec((SZ)szName, &pphr));
    }

    return TRUE;
}

// Pack new public symbol into the publics table.
//
// (as of 11/93:)
// Unlike GSI1::packSym, we are called only with new public definitions.
// We are not given an opportunity to delete obsolete publics.
// Therefore, we must use this algorithm:
// (Treating public names as case sensitive:)
// If the public exists and is unchanged, do nothing.
// If the public exists and is different, delete the existing public
// and insert one for the new symbol.
// If the public does not yet exist, insert one for the new symbol.
//
// One complication: we are obliged to return symbols from HashSym using a
// case insensitive search.  This obliges them to be stored using a case
// insensitive ordering scheme.  This obliges all code which operates upon
// them to use a case insensitive iteration mechanism.  This complicates
// our search code which must treat public names case sensitively.
//
BOOL PSGSI1::packSym(PSYM psym)
{
    assert(pdbi1->fWrite);

    PUBSYM32* ppub = (PUBSYM32*)psym;
    dassert(ppub->rectyp == S_PUB32);

    PPHR pphrFirst = 0;
    if (fFindRec((SZ)ppub->name, &pphrFirst)) {
        // Loop on every public with same name (case insensitive),
        // searching for one with same name (case sensitive).
        PPHR pphr = pphrFirst;
        do {
            PUBSYM32* ppubHR = (PUBSYM32*)(psymForPhr(*pphr));
            dassert(ppubHR->rectyp == S_PUB32);

            if (strcmp((ST)ppub->name, (ST)ppubHR->name) == 0) {
                // found a public with same name (case sensitive)
                dassert(ppub->reclen == ppubHR->reclen);
                if (memcmp(ppub, ppubHR, ppub->reclen + sizeof(ppub->reclen)) == 0) {
                    // record contents match: the existing public stands as is
                    return TRUE;
                }
                else {
                    // record contents differ: the new public must *replace*
                    // the existing public
                    return fUnlinkHR(pphr) && fInsertNewSym(pphr, psym);
                }
            }
        } while (fFindRec((SZ)ppub->name, &pphr));

        // Not found: there were some publics with the same name (case insensitive)
        // but none with the same name (case sensitive).  Fall through...
    }

    if (pphrFirst) {
        // Brand new public
        return fInsertNewSym(pphrFirst, psym);
    } else {
        // VSW:543853 - If the PDB is corrupt, fFindRec() can fail and leave pphrFirst==NULL.
        // This happens on the path where fFindRec() calls psymForPhr() calls fReadSymRec(),
        // and fReadSymRec() detects corruption and calls setCorruptError(). In this case, we'll
        // just return FALSE and let the code calling us sort things out based on setCorruptError()
        // having been called.
        return FALSE;
    }
}

BOOL PSGSI1::addThunkMap(POFF poffThunkMap, UINT nThunks, CB cbSizeOfThunk,
    SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable)
{
    assert(pdbi1->fWrite);

    psgsihdr.nThunks = nThunks;
    psgsihdr.cbSizeOfThunk = cbSizeOfThunk,
    psgsihdr.isectThunkTable = isectThunkTable;
    psgsihdr.offThunkTable = offThunkTable;
    psgsihdr.nSects = nSects;

    if (!bufThunkMap.Append(PB(poffThunkMap), cbSizeOfThunkMap()) ||
        !bufSectMap.Append(PB(psoSectMap), cbSizeOfSectMap()))  {
        ppdb1->setOOMError();
        return FALSE;
    }

    return TRUE;
}

// prepare the thunk sym template

PB PSGSI1::pbInThunkTable (ISECT isect, OFF off, OUT POFF pdisp)
{
    assert(!pdbi1->fWrite);

    if (!fInThunkTable(isect, off) ||
        !readThunkMap())
        return NULL;

    OFF offTarget = offThunkMap(off);
    ISECT isectTarget;

    mapOff(offTarget, &isectTarget, &offTarget);

    if (fInThunkTable(isectTarget, offTarget))
        return NULL;    // stop any recursion here

    PB pb = NearestSym(isectTarget, offTarget, pdisp);
    OFF disp = *pdisp;
    *pdisp = 0;

    return pbFakePubdef(pb, isect, off, disp);
}

BOOL PSGSI1::fInThunkTable(ISECT isect, OFF off)
{
    if ((off >= psgsihdr.offThunkTable) &&
        (off < psgsihdr.offThunkTable + cbSizeOfThunkTable()) &&
        (isect == psgsihdr.isectThunkTable))
        return TRUE;

    return FALSE;
}

UINT PSGSI1::iThunk(OFF off) {
    return (off - psgsihdr.offThunkTable) / psgsihdr.cbSizeOfThunk;
}

OFF PSGSI1::offThunkMap(OFF off)
{
    assert(!pdbi1->fWrite);
    UINT ui = iThunk(off);
    dassert(psgsihdr.nThunks > ui);
    dassert(bufThunkMap.Start());
    return *(POFF(bufThunkMap.Start()) + ui);
}

void PSGSI1::mapOff(OFF off, OUT ISECT * pisect, OUT POFF poff)
{
    assert(!pdbi1->fWrite);
    unsigned int i;
    SO* pso = (SO*) bufSectMap.Start();

    for (i = 0; i < (psgsihdr.nSects - 1); i++, pso++) {
        if ((off >= pso->off) && (off < (pso+1)->off)) {
            *pisect = pso->isect;
            *poff = off - pso->off;
            return;
        }
    }

    *pisect = pso->isect;
    *poff = off  - pso->off;
}

BOOL PSGSI1::getEnumThunk( ISECT isect, OFF off, EnumThunk** ppenum )
{
    assert(!pdbi1->fWrite);
    if (pdbi1->fWrite) {
        return FALSE;
    }
    if ( !readThunkMap() ) return false;

    // get the off as it would appear in the thunk map
    unsigned int i;
    OFF offThunk = 0;
    SO* pso = (SO*) bufSectMap.Start();

    for (i = 0; i < psgsihdr.nSects; i++, pso++) {
        if (pso->isect == isect) {
            offThunk = pso->off + off;
            break;
        }
    }
    if ( offThunk == 0 ) {
        return false;   // failed to find seg
    }

    if ( ppenum ) {
        *ppenum = new PSEnumThunk( offThunk, POFF(bufThunkMap.Start()), psgsihdr );
    }
    return true;
}

PSEnumThunk::PSEnumThunk( OFF offThunk, POFF poffStart, PSGSIHDR psgsihdr )
: m_offThunk( offThunk ), m_poffStart( poffStart ), m_poffEnd( poffStart + psgsihdr.nThunks ),
m_psgsihdr( psgsihdr )
{
    m_poffThunk = 0;
}

BOOL PSEnumThunk::next()
{
    // look up reference to the thunk offset in the thunk map
    if ( m_poffThunk == 0 )
        m_poffThunk = m_poffStart;  // new search
    else
        ++m_poffThunk;              // move forward one slot
    for(; m_poffThunk != m_poffEnd; ++m_poffThunk) {
        if ( *m_poffThunk == m_offThunk ) {  // found the reference
            return true;
        }
    }
    return false;
}

void PSEnumThunk::reset()
{
    m_poffThunk = 0;
}

void PSEnumThunk::release()
{
    delete this;
}

void PSEnumThunk::get( OUT USHORT* pisect, OUT long* poff, OUT long* pcb )
{
    assert( m_poffThunk >= m_poffStart && m_poffThunk < m_poffEnd );
    if ( poff ) {
        *poff = OFF(
            (m_poffThunk - m_poffStart) *
            m_psgsihdr.cbSizeOfThunk +
            m_psgsihdr.offThunkTable
            );  // REVIEW:WIN64 CAST
    }
    if ( pisect ) {
        *pisect = m_psgsihdr.isectThunkTable;
    }
    if ( pcb ) {
        *pcb = m_psgsihdr.cbSizeOfThunk;
    }
}


// Locate followed by next will always give you the enclosing
// symbol (public at or preceeding this address), except when
// you asked for something before the first symbol.
//
// Return values -
//  TRUE indicates succeess.
//  FALSE indicates catastrophic error.
BOOL PSGSI1::EnumPubsByAddr::locate( long isect, long off )
{
    // First, set the m_i to point correctly
    if ( m_bufAddrMap.Size() != 0 ) {

        PPSYM ppsymLo = (PPSYM)m_bufAddrMap.Start();
        PPSYM ppsymHi = (PPSYM)m_bufAddrMap.End() - 1;
        PPSYM ppsymStart = ppsymLo;

        while (ppsymLo < ppsymHi) {
            PPSYM ppsym = ppsymLo + ((ppsymHi - ppsymLo + 1) >> 1);

            if ( !m_psgsi1->fReadSymRec(*ppsym) && !m_psgsi1->fIsDummyThunkSym(*ppsym) ) {
                return false;
            }

            int cmp = cmpAddrMapByAddr(ISECT(isect), (UOFF)off, *ppsym);

            if (cmp < 0)
                ppsymHi = ppsym - 1;
            else if (cmp > 0)
                ppsymLo = ppsym;
            else
                ppsymLo = ppsymHi = ppsym;
        }

        if ( !m_psgsi1->fReadSymRec(*ppsymLo) && !m_psgsi1->fIsDummyThunkSym(*ppsymLo) ) {
            return false;
        }

        int icmp = cmpAddrMapByAddr(ISECT(isect), (UOFF)off, *ppsymLo);

        if (icmp < 0) {
            m_iPubs = ppsymLo - ppsymStart - 2;
        }
        else {
            m_iPubs = ppsymLo - ppsymStart - 1;
        }

        if (m_iPubs < 0) {
            m_iPubs = -1;
        }

        PUBSYM32 *pPubSym = reinterpret_cast<PUBSYM32 *>(*ppsymLo);

        ISECT seg = pPubSym->seg;
        UOFF offset = pPubSym->off;

        while (m_iPubs != -1) {
            if (m_psgsi1->fIsDummyThunkSym(*(ppsymLo - 1))) {
                break;
            }
            if (!m_psgsi1->fReadSymRec(*(ppsymLo - 1))) {
                return false;
            }
            if (cmpAddrMapByAddr(seg, offset, *(ppsymLo - 1)) != 0) {
                break;
            }
            m_iPubs--;
            ppsymLo--;
        }
    }

    // Now, see if we need a thunk enum
    if ( m_psgsi1->fInThunkTable( ISECT(isect), off ) ) {
        OFF offDisp;
        if (m_psgsi1->pbInThunkTable(ISECT(isect), off, &offDisp) == NULL) {
            // Could not allocate memory for the fake thunk symbol
            return false;
        }
        m_iThunks = ( ( off - m_psgsi1->psgsihdr.offThunkTable ) / m_psgsi1->psgsihdr.cbSizeOfThunk ) - 1;
        assert( (m_iThunks + 1) * sizeof( OFF ) < (size_t)m_bufThunkMap.Size() );
    }

    return true;
}

BOOL PSGSI1::EnumPubsByAddr::next() 
{
    if ( traversingThunks() ) {
        if ( nextThunk() ) {
            return TRUE;
        }
        else {
            resetThunk();
        }
    }

    if (++m_iPubs * sizeof(PPSYM) < size_t(m_bufAddrMap.Size())) { 
        PSYM psym = reinterpret_cast<PSYM>( readSymbol() );
        if ( m_psgsi1->fIsDummyThunkSym( psym ) ) {
            m_iThunks = 0;
        }
        return psym != NULL;
    }

    return FALSE;
}

BOOL PSGSI1::EnumPubsByAddr::prev()
{
    if ( traversingThunks() ) {
        if ( prevThunk() ) {
            return TRUE;
        }
        resetThunk();
    }
    if ( m_iPubs != -1 ) {
        m_iPubs--;
        return readSymbol() != NULL;
    }

    return FALSE;
}

void PSGSI1::EnumPubsByAddr::get( BYTE** ppbsym )
{
    if ( m_iThunks != -2 ) {
        ISECT isect = m_psgsi1->psgsihdr.isectThunkTable;
        OFF   off   = OFF( m_psgsi1->psgsihdr.offThunkTable + 
                        m_iThunks * m_psgsi1->psgsihdr.cbSizeOfThunk );
        OFF   disp;
        *ppbsym = m_psgsi1->pbInThunkTable( isect, off, &disp );
    }
    else {
        *ppbsym = readSymbol();
    }

    assert( *ppbsym != NULL );
}

#ifdef PDB_MT
PB PSGSI1::pbFakePubdef(PB pb, ISECT isectThunk, OFF offThunk, OFF disp)
{
    assert(!pdbi1->fWrite);
    //
    // if we are not given a symbol or not at the beginning of the thunk,
    // return NULL to indicate no symbol
    //
    if (!pb)
        return NULL;

    
    UINT i = iThunk(offThunk);
    assert(i < psgsihdr.nThunks);

    MTS_PROTECT(m_csForFakePubDef);
    assert(rgFakePubDef);
    if (rgFakePubDef[i] == NULL) {
        PUBSYM32* psymTarget = (PUBSYM32*) pb;
        CB fakelen = 7 + 11;            // "@ILT+%d()"
        if (disp) {
            fakelen += 1 + 11;          //  "+%d"
        } 

        CB reclen = psymTarget->reclen + fakelen;

        PB rgbThunkSym = new (thunkpool) BYTE[reclen];
        PUBSYM32* psymFake = (PUBSYM32*)rgbThunkSym;
        size_t cbName = reclen - offsetof(PUBSYM32, name);

        assert (!fNeedsSzConversion((PSYM)psymTarget));

        memcpy(rgbThunkSym, pb, sizeof(PUBSYM32));
        psymFake->off = offThunk;
        psymFake->seg = isectThunk;

        sprintf_s((char*)psymFake->name, cbName, "@ILT+%d(", offThunk - psgsihdr.offThunkTable);
        strcat_s((char *)psymFake->name, cbName, (char *)psymTarget->name);
        CB cb = CB(strlen((char*) psymFake->name));

        if (disp) {
            sprintf_s((char*)&psymFake->name[cb], cbName - cb, "+%d)", disp);
            cb += CB(strlen((char*)&psymFake->name[cb]));
        }
        else {
            psymFake->name[cb++] = ')';
            psymFake->name[cb++] = '\0';
        }

        psymFake->reclen += static_cast<unsigned short>(strlen((char*)psymFake->name) - strlen((char*)psymTarget->name));
        assert(psymFake->reclen < reclen);

        rgFakePubDef[i] = rgbThunkSym;
    }
    return rgFakePubDef[i] ;
}

#else
PB PSGSI1::pbFakePubdef(PB pb, ISECT isectThunk, OFF offThunk, OFF disp)
{

    //
    // if we are not given a symbol or not at the beginning of the thunk,
    // return NULL to indicate no symbol
    //
    if (!pb)
        return NULL;

    // you can't have a record bigger than this
    if (!bufThunkSym.Start() && !bufThunkSym.Reserve(0x10000)) {
        ppdb1->setOOMError();
        return NULL;
    }

    PB rgbThunkSym = bufThunkSym.Start();
    PUBSYM32* psymTarget = (PUBSYM32*) pb;
    PUBSYM32* psymFake = (PUBSYM32*) rgbThunkSym;
    size_t cbName = 0x10000 - offsetof(PUBSYM32, name);

    assert (!fNeedsSzConversion((PSYM)psymTarget));

    memcpy(rgbThunkSym, pb, sizeof(PUBSYM32));
    psymFake->off = offThunk;
    psymFake->seg = isectThunk;

    sprintf_s((char*)psymFake->name, cbName, "@ILT+%d(", offThunk - psgsihdr.offThunkTable);
    strcat_s((char *)psymFake->name, cbName, (char *)psymTarget->name);
    CB cb = CB(strlen((char*) psymFake->name));

    if (disp) {
        sprintf_s((char*)&psymFake->name[cb], cbName - cb, "+%d)", disp);
        cb += CB(strlen((char*)&psymFake->name[cb]));
    }
    else {
        assert(cbName - cb >= 2);
        psymFake->name[cb++] = ')';
        psymFake->name[cb++] = '\0';
    }

    psymFake->reclen += static_cast<unsigned short>(strlen((char*)psymFake->name) - strlen((char*)psymTarget->name));
    return rgbThunkSym;
}
#endif
