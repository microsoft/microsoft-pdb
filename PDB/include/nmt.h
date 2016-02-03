// Name table interface/implementation
#pragma once

#include "SysPage.h"
#include "array.h"
#include "buffer.h"
#include "misc.h"
#include "iset.h"

#include <ctype.h>

#if 0
A name table is a two-way mapping from string to name index and back.
Name indices (NIs) are intended to be small positive integers.

This implementation uses a pool of names, and NIs are actually the offsets
of each name in the pool.

Strings are mapped into name indices using a closed hash table of NIs.
To find a string, we hash it and probe into the table, and compare the
string against each successive ni's name until we hit or find an empty
hash table entry.

Acknowledgements: RicoM and RichardS made great suggestions.
#endif
class NMT {                 // name table
private:
    Buffer              buf;        // the strings when we are in write mode

    mutable
    VirtualBuffer       virtbuf;    // the strings when we are in r/o mode

    mutable
    ISet                bvPages;    // bitvector of pages loaded or not for virt mode

    Array<NI>           mphashni;   // closed hash table from string hash to NI
    unsigned            cni;        // no. of names
    CB                  cbReloaded; // size of names when we loaded from a stream
    bool                fConvert;   // if we have to convert from short hash to long
    bool                fVirtMode;  // keep track of whether we are in virtual mode

    mutable
    bool                fAllVirtPagesLoaded;   // if we have loaded all of the pages, we can quit
                                            // checking for having to load the pages
    bool                fWrite;     // keep track of whether we are in write mode or not
    Stream *            pstream;    // Keep track of the stream for r/o virtual i/o.
    long                offStrings; // keep track of where the strings start in our stream
    CB                  cbStrings;  // and the size of them

    Map<NI, NI, HcNi>   mpoldnew;   // Holds a map of Old NIs to new
                                    // if we had to convert a 6.0 namemap
                                    // which was in MBCS, for 7.0 reading
                                    // which returns UTF8 strings

    // the hash function
    LHASH (*pfnHashSz)(PB, size_t, ULONG);

    struct VHdr {                   // the version header
        ULONG   ulHdr;
        ULONG   ulVer;
    };

    VHdr        vhdr;
public:
    enum {
        verLongHash = 1,
        verLongHashV2 = 2,
    };

private:
    enum {
        niUserMin = 1,
        verHdr = 0xeffeeffe,
        verCur = verLongHash,
        verMax = verLongHashV2,
        offCbBuf = sizeof(VHdr),
    };

#ifdef PDB_MT
    mutable CriticalSection m_cs;
#endif

public:

    NMT(ULONG ulVer = verLongHash) : mphashni(1), virtbuf(true)
    {
        assert(ulVer != 0 && ulVer <= verMax);
        setHashFunc(ulVer);
        reset();
    }
    BOOL isValidNi(NI ni) const {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);
        ni = getNewNIForOld(ni);
        return ni < NI(fVirtMode ? virtbuf.Size() : buf.Size());
    }

    // Lookup the name corresponding to 'ni' or 0 if ni == niNil.
    SZ szForNi(NI ni) const {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);
        return szForNiInternal(ni);

    }

    // Return the ni for this name if the association already exists;
    // niNil otherwise.
    NI niForSz(SZ_CONST sz) const {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);

        precondition(sz);

        NI ni;
        return find(sz, &ni, 0) ? ni : niNil;
    }
    // Return TRUE, with a name index for this name, whether or not a name->ni
    // association preexists; return FALSE only on internal failure.
    BOOL addNiForSz(SZ_CONST sz, OUT NI *pni) {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);

        precondition(sz);
        precondition(pni);

        unsigned ini;
        if (find(sz, pni, &ini))
            return TRUE;
        if (addSz(sz, pni)) {
            if (ini >= mphashni.size()) {
                // PDB must be corrupted.
                return FALSE;
            }
            mphashni[ini] = *pni;
            return grow();
        } else {
            *pni = niNil;
            return FALSE;
        }
    }

    BOOL clear() {
        // Now actually get rid of all memory too
        MTS_PROTECT(m_cs);

        buf.Free();
        buf.SetInitAlloc(1);    // for the Append in reset() that adds a single byte.
        bvPages.clear();
        mphashni.clear();
        mpoldnew.clear();

        // fix up all the normal data to be correct too
        return reset();
    }

private:
    void setHashFunc(ULONG ulVer) {
        switch(ulVer) {
            case verLongHash:
                pfnHashSz = &LHashPbCb;
                break;
            case verLongHashV2:
                pfnHashSz = &LHashPbCbV2;
                break;
        };
        vhdr.ulVer = ulVer;
    }

    BOOL reset() {
        PDBLOG_FUNC();
        // MTS NOTE: Requires external syncronization

        buf.Clear();
        virtbuf.Free();
        bvPages.reset();
        fVirtMode = false;
        fWrite = true;
        fAllVirtPagesLoaded = false;
        offStrings = cbStrings = 0;

        fConvert = FALSE;
        vhdr.ulHdr = ULONG(verHdr);

        BYTE nul = 0;
        if (!buf.Append(&nul, sizeof nul))
            return FALSE;
        if (!mphashni.setSize(1))
            return FALSE;
        mphashni.fill(niNil);
        cni = 0;
        cbReloaded = 0;
        return TRUE;
    }
    // Append a serialization of this NMT to the buffer
    BOOL save(Buffer* pbuf) const {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);

        if (!pbuf->Append(PB(&vhdr), sizeof(vhdr)))
            return FALSE;
        if (!buf.save(pbuf))
            return FALSE;
        traceOnly(CB cbPreHash = pbuf->Size());
        if (!mphashni.save(pbuf))
            return FALSE;
        else if (!pbuf->Append((PB)&cni, sizeof cni))
            return FALSE;

        trace((trSave, _TEXT("NMT::save() cbBuf=%d cbHash=%d\n"), buf.Size(), pbuf->Size() - cbPreHash));
        return TRUE;
    }
#if 0
    // Reload a serialization of this empty NMT from the buffer; leave
    // *ppb pointing just past the NMT representation
    BOOL reload(PB* ppb) {
        PDBLOG_FUNC();
        // MTS NOTE: Requires external syncronization

        buf.Reset();
        virtbuf.Free();
        offStrings = cbStrings = 0;
        pstream = NULL;
        fVirtMode = false;
        fWrite = true;
        bvPages.reset();

        fConvert = FALSE;
        VHdr    vhT = *((VHdr UNALIGNED *&)*ppb);
        if (vhT.ulHdr == verHdr) {
            if (vhT.ulVer > verCur)
                return FALSE;
            *ppb += sizeof(VHdr);
        }
        else {
            fConvert = TRUE;
        }
        if (!buf.reload(ppb))
            return FALSE;
        else if (!mphashni.reload(ppb))
            return FALSE;
        else {
            cni = *((NI UNALIGNED *&)*ppb)++;
            if (fConvert)
                return rehash(mphashni.size());

            return TRUE;
        }
    }
#endif
    // Reload a serialization of this empty NMT from the buffer; leave
    // *ppb pointing just past the NMT representation
    BOOL reload(PB* ppb, CB cbReloadBuf) {
        PDBLOG_FUNC();
        // MTS NOTE: Requires external syncronization

        buf.Reset();
        virtbuf.Free();
        offStrings = cbStrings = 0;
        pstream = NULL;
        fVirtMode = false;
        fWrite = true;
        bvPages.reset();

        if (cbReloadBuf < sizeof(VHdr)) {
            return FALSE;
        }

        assume(cbReloadBuf >= 0);

        fConvert = FALSE;
        VHdr    vhT = *((VHdr UNALIGNED *&)*ppb);
        if (vhT.ulHdr == verHdr && vhT.ulVer != 0) {
            if (vhT.ulVer > verMax) {
                return FALSE;
            }
            if (vhT.ulVer < vhdr.ulVer) {
                // upgrade if the version on disk is older then the one the user asked for
                fConvert = TRUE;
            } else {
                // just use the version on disk
                setHashFunc(vhT.ulVer);
            }

            *ppb += sizeof(VHdr);
            cbReloadBuf -= sizeof(VHdr);
        }
        else {
            fConvert = TRUE;
        }

        PB  pbEnd = *ppb + cbReloadBuf;

        if (!buf.reload(ppb, cbReloadBuf)) {
            return FALSE;
        }
        cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

        if (!mphashni.reload(ppb, cbReloadBuf)) {
            return FALSE;
        }
        cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

        if (cbReloadBuf < sizeof(NI)) {
            return FALSE;
        }
        cni = *((NI UNALIGNED *&)*ppb)++;

        if (fConvert) {
            return rehash(mphashni.size());
        }

        return TRUE;
    }

    bool fSwitchToWriteMode() {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);
        // Accomplish a switch from virtual mode to buffer mode
        // Note that we cannot really get rid of the virtual buffer
        // since someone may still string references into it.
        //
        if (fVirtMode) {
            assert(offStrings);
            assert(cbStrings);
            assert(pstream);

            PB pb;
            if (!buf.Reserve(cbStrings, &pb))
                return false;

            // now read in the text of the strings if non-virtual
            CB cb = cbStrings;
            if (!pstream->Read(offStrings, pb, &cb) || cb != cbStrings)
                return false;

            fVirtMode = false;
            fAllVirtPagesLoaded = false;
            offStrings = cbStrings = 0;
            pstream = NULL;
            bvPages.reset();
        }
        return true;
    }

#ifdef HASH_REPORT
    struct NI_LIST
    {
        NI ni;
        NI_LIST* next;
    };
    struct BUCKET_STATS
    {
        int bucket_num;
        int hit_count;
        NI_LIST* ni_first;
    };
    static int __cdecl compare_bucket(const void * p1, const void * p2)
    {
        BUCKET_STATS* e1 = (BUCKET_STATS*)p1;
        BUCKET_STATS* e2 = (BUCKET_STATS*)p2;
        if (e1->hit_count > e2->hit_count)
            return -1;
        else if (e1->hit_count == e2->hit_count)
            return 0;
        else
            return 1;
    }
#endif

    BOOL reload(Stream* pstm, bool fWrite_ = true) {
        PDBLOG_FUNC();
        // MTS NOTE: Requires external syncronization

        buf.Reset();
        virtbuf.Free();
        bvPages.reset();

        OFF off = 0;
        CB  cb;

        // check to see if we have a new version...
        VHdr    vhT;
        cb = sizeof(vhT);
        if (!pstm->Read(off, &vhT, &cb) || cb != sizeof(vhT)) {
            return FALSE;
        }

        fConvert = FALSE;
        if (vhT.ulHdr == vhdr.ulHdr && vhT.ulVer != 0) {
            if (vhT.ulVer > verMax) {
                return FALSE;
            }
            if (vhT.ulVer < vhdr.ulVer) {
                // upgrade if the version on disk is older then the one the user asked for
                fConvert = TRUE;
            } else {
                // just use the version on disk
                setHashFunc(vhT.ulVer);
            }
            off += sizeof(VHdr);
        }
        else {
            fConvert = TRUE;
        }

        // read the number of bytes from the stream...
        CB cbBuf;
        cb = sizeof(cbBuf);
        if (!pstm->Read(off, &cbBuf, &cb) || cb != sizeof(cbBuf) || cbBuf < 0) {
            return FALSE;
        }
        off += sizeof(cb);

        // set up the virtual mode data, i.e. we have >= 64K of strings
        // and we are not writing
        //
        fWrite = fWrite_;
        fAllVirtPagesLoaded = false;
        fVirtMode = !fWrite_ && cbBuf >= 0x8000 && !fConvert;

        PB pb;
        if (fVirtMode) {
            pstream = pstm;
            offStrings = off;
            cbStrings = cbBuf;
            // make sure we have room for the bit vector all at once.
            //
            unsigned    iLastPage = ((cbStrings + cbSysPage - 1) / cbSysPage) - 1;
            if (!bvPages.add(iLastPage)) {
                return FALSE;
            }
            bvPages.remove(iLastPage);
            if (!virtbuf.Reserve(cbBuf, &pb)) {
                return FALSE;
            }
        }
        else {
            pstream = NULL;
            offStrings = cbStrings = 0;
            if (!buf.Reserve(cbBuf, &pb)) {
                return FALSE;
            }
            // now read in the text of the strings if non-virtual
            cb = cbBuf;
            if (!pstm->Read(off, pb, &cb) || cb != cbBuf) {
                return FALSE;
            }
        }

        off += cbBuf;

        // read in the number of elements in the hash table
        unsigned long cit;
        cb = sizeof(cit);
        if (!pstm->Read(off, &cit, &cb) || cb != sizeof(cit)) {
            return FALSE;
        }
        off += sizeof(cit);

        // now make sure the array is at least that big
        if (!mphashni.setSize(cit)) {
            return FALSE;
        }

        // read in the bytes of the array
        cb = cit * sizeof(NI);
        if (!pstm->Read(off, &mphashni[0], &cb) || unsigned(cb) != cit * sizeof(NI)) {
            return FALSE;
        }
        off += cit * sizeof(NI);

        // lastly, read the number of names in the table...
        cb = sizeof(cni);
        if (!pstm->Read(off, &cni, &cb)  || cb != sizeof(cni)) {
            return FALSE;
        }

        // handle the conversion of v0 (short hash) to v1 (long hash)
        if (fConvert && !rehash(mphashni.size())) {
            return FALSE;
        }

        // remember how many names we had at first so we can quickly update...
        cbReloaded = cbBuf;

#ifdef HASH_REPORT
        // Print a report about hashing
        BUCKET_STATS* pStats;

        printf("\nNMT:#items = %d\n", mphashni.size());

        pStats = new BUCKET_STATS[mphashni.size()];
        for(unsigned i = 0; i < mphashni.size(); i++) {
            pStats[i].bucket_num = i;
            pStats[i].hit_count = 0;
            pStats[i].ni_first = NULL;
        }

        int hits = 0, misses = 0, distance = 0, current_distance = 0;
        int longest_miss = 0, longest_miss_index = 0;
        for(unsigned i = 0; i < mphashni.size(); i++) {
            NI ni = mphashni[i];

            if (ni == niNil)
                continue;

            unsigned iHash = hashSz(szForNi(ni)) % mphashni.size();

            pStats[iHash].hit_count += 1;
            NI_LIST* pNew = new NI_LIST;
            pNew->ni = ni;
            pNew->next = pStats[iHash].ni_first;
            pStats[iHash].ni_first = pNew;

            if (iHash == i)
                hits++;
            else {
                misses++;
                current_distance = 0;
                for(unsigned j = iHash; mphashni[j] != ni; j = (j+1 >= mphashni.size()) ? 0 : j+1) {
                    distance += 1;
                    current_distance += 1;
                }
                if (current_distance > longest_miss) {
                    longest_miss = current_distance;
                    longest_miss_index = i;
                }
                // distance += (mphashni.size() + ni - iHash) % mphashni.size();
            }
        }

        printf("Hits = %d\nMisses = %d\nTotal = %d\nLoad = %f\nMax penalty = %d @%d\nAvg. Penalty = %d\n",
            hits,
            misses,
            hits + misses,
            (hits + misses) / (1.0f * mphashni.size()),
            longest_miss,
            longest_miss_index,
            misses ? distance/misses : 0);

        qsort(pStats, mphashni.size(), sizeof(BUCKET_STATS), compare_bucket);

        int full = 0;
        for(unsigned i = 0; i < mphashni.size(); i++) {
            if (pStats[i].hit_count > 0)
                full++;
        }
        printf("Full = %d\nEmpty = %d\n", full, mphashni.size() - full);
        printf("Top bucket sizes: ");
        for(unsigned i = 0; i < 20 && i < mphashni.size(); i++) {
            printf("%d ", pStats[i].hit_count);
        }
        printf("\n");

# ifdef HASH_REPORT_VERBOSE

        // Print out all the bucket contents. This is sometimes useful to
        // determine a pattern in the buckets.
        for(unsigned i = 0; i < mphashni.size(); i++) {
            printf("%6d: %5d:: ", pStats[i].bucket_num, pStats[i].hit_count);
            size_t linelen = 0;
            for (NI_LIST* pList = pStats[i].ni_first; pList != NULL; ) {
                SZ sz = szForNi(pList->ni);
                printf("%s ", sz);
                linelen += strlen(sz) + 1;
                NI_LIST* pTmp = pList;
                pList = pList->next;
                delete pTmp;
                if (linelen > 120 && pList != NULL) {
                    printf("\n                ");
                    linelen = 0;
                }
            }
            printf("\n");
        }

# endif // HASH_REPORT_VERBOSE

        // Clean up
        for(unsigned i = 0; i < mphashni.size(); i++) {
            for (NI_LIST* pList = pStats[i].ni_first; pList != NULL; ) {
                NI_LIST* pTmp = pList;
                pList = pList->next;
                delete pTmp;
            }
        }
        delete[] pStats;

#endif // HASH_REPORT

        return TRUE;
    }

    BOOL save(Stream *pstm) {
        PDBLOG_FUNC();
        MTS_PROTECT(m_cs);

        CB cbBuf = buf.Size();

        // if we are going to convert, we have to rewrite everything, cause
        // we have inserted the VHdr at the beginning of stream.
        if (fConvert)
            cbReloaded = 0;

        // fast exit if no names have been added
        if (cbBuf == cbReloaded)
            return TRUE;

        if (cbReloaded) {
            // replace the size of the buffer, truncate to the old buffer size, then append
            // the new part of the buffer
            if (!pstm->Write(offCbBuf, &cbBuf, static_cast<CB>(sizeof(cbBuf))) ||
                !pstm->Truncate(offCbBuf + cbReloaded + static_cast<CB>(sizeof(cbBuf))) ||
                !pstm->Append(buf.Start() + cbReloaded, cbBuf - cbReloaded))
                    return FALSE;
        }
        else {
            // a completely new write ignoring the old stream contents
            // truncate to zero, append the size of the name buffer and its contents
            if (!pstm->Truncate(0) ||
                !pstm->Append(PB(&vhdr), static_cast<CB>(sizeof(VHdr))) ||
                !pstm->Append(&cbBuf, static_cast<CB>(sizeof(cbBuf))) ||
                !pstm->Append(buf.Start(), cbBuf))
                    return FALSE;
        }

        // finally, append the hash table size and count of names
        unsigned long cit = mphashni.size();

        if (!pstm->Append(&cit, static_cast<CB>(sizeof(cit))) ||
            !pstm->Append(&mphashni[0], cit * static_cast<CB>(sizeof(NI))) ||
            !pstm->Append(&cni, static_cast<CB>(sizeof(cni))))
                return FALSE;

        // no need to recommit the newly saved names
        cbReloaded = cbBuf;
        // no longer need to convert
        fConvert = FALSE;
        return TRUE;
    }

    // All the internal strings should be in UTF8
    // Call this function to convert MBCS namemaps to UTF8
    BOOL convert() {
        PDBLOG_FUNC();
        // MTS NOTE: Requires external syncronization

        // Do we have any strings at all?
        if (cni == 0)
            return TRUE;

        // We are going to have to switch to non-virtual mode
        // if we are in it, since we look at all of the strings
        //
        if (!fSwitchToWriteMode()) {
            return FALSE;
        }

        // Go through all the strings and see if we
        // have any stings in MBCS, which we need to convert
        size_t bufLen = buf.Size();
        for(size_t i = 1; i < bufLen; )  {

            SZ_CONST szMBCS = (SZ)(buf.Start() + i);
            NI niOld = (NI)i;
            NI niNew = niOld;

            // Update J, addNiForSzMBCS could
            // change the buffer locations and
            // szMBCS could point to garbage
            // after that
            i += strlen(szMBCS) + 1;

            if (!IsASCIIString(szMBCS)) {
                // This is not an ASCII string, add a
                // UTF8 string for this one
                if (!addNiForSzMBCS(szMBCS, &niNew))
                    return FALSE;

                mpoldnew.add(niOld, niNew);
            }
        }

        return TRUE;
    }


    // If sz is already in the NMT, return TRUE with *pni set to its ni.
    // Otherwise return FALSE with *pi at a suitable insertion point for
    // an ni for the sz.
    BOOL find(SZ_CONST sz, OUT NI* pni, OUT unsigned* pi) const {
        PDBLOG_FUNC();
        MTS_ASSERT(m_cs);
        precondition(sz);
        precondition(0 <= cni && cni < mphashni.size());

        // search closed hash table for the string
        NI ni = niNil;
        unsigned n = mphashni.size();
        assert(n != 0);
        if (n == 0) {
            // PDB must be corrupted.
            return FALSE;
        }
        unsigned i = hashSz(sz) % n;
        while (1) {
            ni = mphashni[i];
            if (ni == niNil) {
                break;
            }
            if (!szForNiInternal(ni)) {
                // PDB must be corrupted.
                return FALSE;
            }
            if (!strcmp(sz, szForNiInternal(ni))) {
                break;
            }
            i = (i+1 < n) ? (i+1) : 0;
        }

        if (pni) {
            *pni = ni;
            postcondition(ni != niNil || mphashni[i] == niNil);
        }
        if (pi) {
            *pi = i;
            postcondition(0 <= *pi && *pi < mphashni.size());
        }
        return ni != niNil;
    }
    // Rehash the data
    BOOL rehash(unsigned cniNew) {
        PDBLOG_FUNC();

        Array<NI> mpNew(cniNew);

        // Check to see if the array was allocated properly
        if (cniNew > 0 && !mpNew.isValidSubscript(cniNew - 1)) {
            return FALSE;
        }

        mpNew.fill(niNil);

        // Rehash each nonnil hash table entry into mphashniNew.
        for (unsigned i = 0; i < mphashni.size(); i++) {
            NI ni = mphashni[i];
            if (ni != niNil) {
                unsigned j;
                if (!szForNiInternal(ni)) {
                    // PDB must be corrupted.
                    return FALSE;
                }
                for (j = hashSz(szForNiInternal(ni)) % cniNew;
                     mpNew[j] != niNil;
                     j = (j+1 < cniNew) ? j+1 : 0)
                    ;
                mpNew[j] = ni;
            }
        }
        mphashni.swap(mpNew);
        return TRUE;
    }
    // Ensure the hash table has not become too full.  Grow and rehash
    // if necessary.  Return TRUE if all is well.
    BOOL grow() {
        PDBLOG_FUNC();
        ++cni;

        // this is protected from Array<T>'s limit 
        if (mphashni.size() * 3/4  < cni) {
            return rehash(mphashni.size() * 3/2 + 1);
        }
        return TRUE;
    }
    // Append the name to the names buffer
    BOOL addSz(SZ_CONST sz, OUT NI* pni) {
        PDBLOG_FUNC();
        precondition(sz && pni);
        precondition(!fVirtMode);

        CB cb = CB(strlen(sz)) + 1;
        PB pb;
        if (!fVirtMode && buf.Append((PB)sz, cb, &pb)) {
            *pni = NI(pb - buf.Start());    // REVIEW:WIN64 CAST
            return TRUE;
        }
        else {
            *pni = niNil;
            return FALSE;
        }
    }

    bool fLoadString(NI ni) const {
        PDBLOG_FUNC();
        MTS_ASSERT(m_cs);
        if (fVirtMode && !fAllVirtPagesLoaded) {
            precondition(pstream);
            precondition(offStrings);
            precondition(cbStrings);

            PB          pbBase = virtbuf.Start();
            PB          pb = pbBase + ni;
            unsigned    iPage = ni / cbSysPage;
            unsigned    iPageLast = ((cbStrings + cbSysPage - 1) / cbSysPage) - 1;
            PB          pbMaxMax = pbBase + cbStrings;
            bool        fLoading;

            for (fLoading = true; fLoading && iPage <= iPageLast; iPage++) {
                if (!bvPages.contains(iPage)) {
                    // attempt to load said page
                    //
                    PB  pbPage = pbBase + iPage * cbSysPage;

                    assert(unsigned(cbStrings) > iPage * cbSysPage);
                    CB  cbRead = min(cbSysPage, cbStrings - iPage * cbSysPage);

                    if (!virtbuf.Commit(pbPage, cbSysPage) ||
                        !pstream->Read2(offStrings + iPage * cbSysPage, pbPage, cbRead)) {
                        return false;
                    }
                    bvPages.add(iPage);
                }

                // assemble all the contiguous loaded pages into one block
                //
                unsigned    iPageT = iPage + 1;
                PB  pbMax = pbBase + iPageT * cbSysPage;
                while (iPageT <= iPageLast && bvPages.contains(iPageT)) {
                    pbMax += cbSysPage;
                    iPageT++;
                }
                iPage = iPageT - 1;

                // if we reach the end, we are done, as all of pages are
                // loaded and we need not check any more bytes.
                //
                if (pbMax >= pbMaxMax) {
                    break;
                }

                // scan for the end of the string in this block
                //
                while (pb < pbMax) {
                    if (0 == *pb++) {
                        // got the whole string loaded, get out
                        fLoading = false;
                        break;
                    }
                }
            }
            fAllVirtPagesLoaded = (bvPages.cardinality() == (cbStrings + cbSysPage - 1) / cbSysPage);
        }
        return true;
    }

    SZ szForNiInternal(NI ni) const {
        MTS_ASSERT(m_cs);

        precondition(isValidNi(ni));
        if (!isValidNi(ni)) {
            return NULL;
        }

        ni = getNewNIForOld(ni);

        if (!fLoadString(ni)) {
            return NULL;
        }
        return (ni != niNil) ? SZ((fVirtMode ? virtbuf.Start() : buf.Start()) + ni) : 0;
    }

    // long hash, now the default
    LHASH hashSz(SZ_CONST sz) const {
        return pfnHashSz(PB(sz), strlen(sz), ULONG(-1));
    }
    // short hash, use for converting to new hash only.
    static LHASH shashSz(SZ_CONST sz) {
        PDBLOG_FUNC();
        return HashPbCb(PB(sz), CB(strlen(sz)), ULONG(-1));
    }

    BOOL addNiForSzMBCS(SZ_CONST szMBCS, OUT NI* pni) {
        PDBLOG_FUNC();
        USES_STACKBUFFER(0x400);
        UTFSZ szUTF8 = GetSZUTF8FromSZMBCS(szMBCS);
        return ( szUTF8 != NULL ) ? addNiForSz(szUTF8, pni) : FALSE;
    }

    NI getNewNIForOld (NI niOld) const {
        PDBLOG_FUNC();
        MTS_ASSERT(m_cs);
        NI niNew;
        if (mpoldnew.map(niOld, &niNew))
            return niNew;
        else
            return niOld;
    }

    friend class EnumNMT;

    // these classes are using reload/save responsiblily
    friend class NMP;
    friend struct DBI1;
};

#ifdef PDB_MT
// Kinda MTS accessing NMT, but bad if NMT is reloaded
class EnumNMT : public EnumNameMap {
public:
    EnumNMT(const NMT& nmt) {
        PDBLOG_FUNC();
        pnmt = &nmt;
        reset();
    }
    void release() {
        PDBLOG_FUNC();
        delete this;
    }
    void reset() {
        PDBLOG_FUNC();
        i = (unsigned)-1;
        rgNi.reset();

        MTS_PROTECT(pnmt->m_cs);
        if (!rgNi.setSize(pnmt->cni)) {
            return;
        }
        rgNi.reset();
        for (unsigned j = 0; j < pnmt->mphashni.size(); j++) {
            if (pnmt->mphashni[j] != niNil) {
                if (!rgNi.append(pnmt->mphashni[j])) {
                    rgNi.clear();
                    return;
                }
            }
        }
    }
    BOOL next() {
        PDBLOG_FUNC();
        return (++i < rgNi.size());
    }
    void get(OUT SZ_CONST* psz, OUT NI* pni) {
        PDBLOG_FUNC();
        precondition(0 <= i && i < rgNi.size());
        precondition(rgNi[i] != niNil);

        *pni = rgNi[i];
        *psz = pnmt->szForNi(*pni);

        postcondition(*pni != niNil && *psz != 0);
    }
private:
    const NMT * pnmt;
    unsigned i;
    Array<NI> rgNi;
};
#else

class EnumNMT : public EnumNameMap {
public:
    EnumNMT(const NMT& nmt) {
        pnmt = &nmt;
        reset();
    }
    void release() {
        delete this;
    }
    void reset() {
        i = (unsigned)-1;
    }
    BOOL next() {
        while (++i < pnmt->mphashni.size())
            if (pnmt->mphashni[i] != niNil)
                return TRUE;
        return FALSE;
    }
    void get(OUT SZ_CONST* psz, OUT NI* pni) {
        precondition(0 <= i && i < pnmt->mphashni.size());
        precondition(pnmt->mphashni[i] != niNil);

        *pni = pnmt->mphashni[i];
        *psz = pnmt->szForNi(*pni);

        postcondition(*pni != niNil && *psz != 0);
    }
private:
    const NMT* pnmt;
    unsigned i;
};
#endif
