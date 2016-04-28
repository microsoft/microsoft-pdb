// Multistream File (MSF) Implementation
//
//  Revision History
//  When    Who         What
//  4/92    jangr       created in support of the minimal build proposal
//  7/93    v-danwh     added MSFCreateCopy
//  8/93    jangr       added MSFAppendStream and MSFReadStream2
//                      eliminated requirement that streams be a multiple of
//                       cbPg in size
//                      open using appropriate share modes for safe
//                       concurrency of read/read and no concurrency of
//                       read/write or write/write
//  2/94    jangr       redesigned stream table structure to eliminate
//                       limits and improve efficiency
//                      eliminated MSFCreateCopy
//  6/99    kherold     expanded capacity; added support for full 65K stream
//                      numbers and 32-bit page numbers (PN32)
//  12/99   vinitd      Mapped file I/O support under flag MAPPED_IO
//  
// REVIEW: TO DO
//  * implement memory mapped file primitives

// Behaviour: implements a multistream file, where each stream is assigned
// a stream number.  All operations are transacted.  Logical change occurs
// atomically at Commit time only.  Operations include Open, Replace, Append,
// Read, and Delete stream, and Commit and Close.  Can query for the size of
// a stream or for an unused stream no.
//
// A MSF is implemented as a sequence of pages.  A page can contain
//  HDR     --  header structure, including stream table stream info
//  FPM     --  free page map: maps a page number (PN) to a boolean
//              where TRUE => page free
//  DATA    --  a stream data page
//
// The first few pages of a MSF are special:
//  PN  Type/Name   Description
//  0   HDR hdr     page 0: master index
//  1   FPM fpm0    first free page map
//  2   FPM fpm1    second free page map
//
// According to hdr.pnFpm, the first or the second free page map is valid.
//
// There is one special stream, snST, the "stream table" stream.  The stream
// table maps a stream number (SN) into a stream info (SI).  A stream info
// stores the stream size and an index to the subarray of the page numbers
// (PNs) that each stream uses.
//
// MSF capacity has been increased by making page numbers 32-bits and making
// 64K streams available. In the interest of the on-disk size of the file,
// the FPM has been split across the file at regular intervals, with new
// intervals introduced as needed. Also, a layer of indirection has been
// added to the stream table serialization. Where before the page list for
// the stream table was stored in the header page for the reconstruction of
// the stream table, now a page list of the pages is written instead. This
// way the page list for the stream table won't exceed a single page.
//
// This organization enables efficient two-phase commit.  At commit time,
// after one or more streams have been written (to new pages), a new
// StrmTbl stream is written and the new FPM is written.  Then, a single
// write to hdr swaps the roles of the two FPM sets and atomically
// updates the MSF to reflect the new location of the StrmTbl stream.

#include "prefast.h"

#define UNICODE
#define _UNICODE
#include <tchar.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <share.h>
#include <memory.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>


#define STRICT
//#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include <objidl.h>

#include "ref.h"                // for COMRefPtr

#include "assert_.h"
#include "mapfile.h"
#include "istream.h"

#include "pdbtypdefs.h"


#define precondition(x) assert(x)
#define postcondition(x) assert(x)
#define notReached() assert(0)


#include "../include/array.h"
#include "mts.h"

#ifdef PDB_MT
#include "trace.h"
#include "pdb.h"
#include "../include/buffer.h"
#include "../include/map.h"
#endif

using namespace pdb_internal;

// undefine the min/max
#ifdef min 
#undef min
#endif

#ifdef max
#undef max
#endif

#define MSF_IMP     // for declspec()
#include "msf.h"

#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

typedef unsigned short      ushort;
typedef unsigned __int16    PN16;       // page number
typedef unsigned __int16    SPN16;      // stream page number
typedef unsigned __int32    PN32;
typedef unsigned __int32    SPN32;

typedef PN16    PN;
typedef SPN16   SPN;

typedef PN32    UPN;    // universal page no.
typedef SPN32   USPN;   // universal stream page no.

typedef unsigned char BYTE;
typedef BYTE* PB;
typedef void* PV;

const CB    cbPgMax     = 0x1000;
#ifdef SMALLPAGES
const CB    cbPgMin     = 0x200;
#else
const CB    cbPgMin     = 0x400;
#endif
const CB    cbDbMax     = 128 * 0x10000;    // 128meg
const CB    cpnDbMax    = 0x10000;
const CB    cbitsFpmMax = cpnDbMax;
const CB    cbFpmMax    = cbitsFpmMax/CHAR_BIT;
const CB    cpnDbMaxBigMsf = 0x100000;      // 2^20 pages
const CB    cbitsFpmMaxBigMsf = cpnDbMaxBigMsf;
const CB    cbFpmMaxBigMsf = cbitsFpmMaxBigMsf/CHAR_BIT;

struct MSFParms {
    CB          cbPg;
    unsigned    lgCbPg;
    unsigned    maskCbPgMod;
    CB          cbitsFpm;
    CB          cpgFileGrowth;
    UPN         pnMax;
    UPN         cpnFpm;
    UPN         pnFpm0;
    UPN         pnFpm1;
    UPN         pnDataMin;
};

const UPN   pnNil       = UPN(~0);      // Use a UPN for pnNil for safety.
const PN    pnMaxMax    = 0xffff;       // max no of pgs in any msf
const PN    pnHdr       = 0;

const SPN   spnNil      = (SPN)-1;
const SN    snSt        = 0;            // stream info stream
const SN    snUserMin   = 1;            // first valid user sn
const SN    snMax       = 0x1000;       // max no of streams in msf
const SPN   spnMaxMax   = pnMaxMax;     // max no of pgs in a stream

const UPN   upnNil      = UPN(~0);
const USPN  uspnNil     = USPN(~0);
const UNSN  unsnMax     = 0x10000;             // 64K streams
const PN32  upnMaxMax   = cpnDbMaxBigMsf;      // 2^20 pages
const SPN32 uspnMaxMax  = upnMaxMax;

#define MSF_PARMS(cbPg, lgCbPg, pnMax, cpgGrowth, cpnFpm) \
    { cbPg, lgCbPg, cbPg-1, pnMax+1, cpgGrowth, pnMax, cpnFpm, 1, 1+cpnFpm, 1+cpnFpm+cpnFpm }

const MSFParms  rgmsfparms[] = {
#ifdef SMALLPAGES
    MSF_PARMS(1024, 10, pnMaxMax, 8, 8),    // gives 64meg
#else
    MSF_PARMS(cbPgMin, 10, pnMaxMax, 8, 8), // gives 64meg (??)
#endif
    MSF_PARMS(2048,    11, pnMaxMax, 4, 4), // gives 128meg
    MSF_PARMS(cbPgMax, 12, 0x7fff,   2, 1)  // gives 128meg
};

#define MSFHC_PARMS(cbPg, lgCbPg, pnMax, cpgGrowth, cpnFpm) \
    { cbPg, lgCbPg, cbPg-1, pnMax, cpgGrowth, pnMax, cpnFpm, 1, 1+cpnFpm, 1+cpnFpm+cpnFpm }

const MSFParms  rgmsfparms_hc[] = { // high capacity MSF
#ifdef SMALLPAGES
    MSFHC_PARMS(cbPgMin,  9, upnMaxMax, 4, 1),  // gives .5GB, note this is used
                                                // primarily for small pdbs, so
                                                // keep growth rate low (2K).
    MSFHC_PARMS(1024,    10, upnMaxMax, 8, 1),  // gives 1.0GB
#else
    MSFHC_PARMS(cbPgMin, 10, upnMaxMax, 8, 1),  // gives 1.0GB
#endif  
    MSFHC_PARMS(2048,    11, upnMaxMax, 4, 1),  // gives 2.0GB
    MSFHC_PARMS(cbPgMax, 12, upnMaxMax, 2, 1)   // gives 4.0GB
};

const int cpnBigMsfReserved = 3;    // first three pages in a big MSF are the header and two FPMs

inline unsigned cpnForCbCbPg(unsigned cb, unsigned cbpg) {
#ifdef SMALLPAGES
    // valid pages sizes are only 512, 1k, 2k, 4k.
    assert(cbpg == 512 || cbpg == 1024 || cbpg == 2048 || cbpg == 4096);
#else
    // valid pages sizes are only 1k, 2k, 4k.
    assert(cbpg == 1024 || cbpg == 2048 || cbpg == 4096);
#endif
    return (cb + cbpg - 1) / cbpg;
}
inline unsigned cpnForCbLgCbPg(unsigned cb, unsigned lgcbpg) {
#ifdef SMALLPAGES
    // valid pages sizes are only 512, 1k, 2k, 4k.
    assert(lgcbpg >= 9 && lgcbpg <= 12);
#else
    // valid pages sizes are only 1k, 2k, 4k.
    assert(lgcbpg >= 10 && lgcbpg <= 12);
#endif
    return (cb + (1 << lgcbpg) - 1) >> lgcbpg;
}

#define cpnMaxForCb(cb) (((cb) + ::cbPgMin - 1) / ::cbPgMin)

struct SI { // stream info
    CB  cb; // length of stream, cbNil if stream does not exist
    UPN*    mpspnpn;

    SI() : cb(cbNil), mpspnpn(0) { }
    BOOL isValid() const {
        return cb != cbNil;
    }
    BOOL allocForCb(CB cb_, unsigned lgcbPage) {
        cb = cb_;
        USPN spnMacT = spnMac(lgcbPage);

        // Allocate the SPN->PN map based on the calculated
        // required number of pages.

        if (!!(mpspnpn = new UPN[spnMacT])) {
            for (USPN spn = 0; spn < spnMacT; spn++)
                mpspnpn[spn] = upnNil;
            return TRUE;
        } else
            return FALSE;
    }
    void dealloc() { // idempotent
        if (mpspnpn) {
            delete [] mpspnpn;
        }
        *this = SI();
    }       
    USPN spnMac(unsigned lgcbPg) {
        return USPN(cpnForCbLgCbPg(cb, lgcbPg));
    }
};

static const SI siNil;

struct SI_PERSIST {
    CB      cb;
    __int32 mpspnpn;

    // Note that we need spnMac() to be defined for use just as it is in
    // an SI.  Make sure that everything is kosher by checking size and
    // offsets of cb in both structures.
    USPN spnMac(unsigned lgcbPg) {
        return ((SI *) this)->spnMac(lgcbPg);
    }
};


cassert(offsetof(SI_PERSIST,cb) == offsetof(SI,cb));
cassert(sizeof(((SI_PERSIST*)0)->cb) == sizeof(((SI*)0)->cb));

class MSF_HB;

struct FPM { // free page map (in-memory version)
#if defined(_M_IA64) || defined(_M_AMD64)
    // REVIEW: ensure that using 64-bit ints on win64 doesn't impact
    // reading/writing pdbs written/read from win32.
    //
    typedef unsigned __int64    native_word;
    enum {
        lgBPW   = 6,
    };
#else
    typedef unsigned long       native_word;
    enum {
        lgBPW   = 5,
    };
#endif

    enum {
        BPW     = sizeof(native_word) * CHAR_BIT,
        iwMax   = ::cbFpmMaxBigMsf/sizeof(native_word),
        BPL     = BPW,
    };

    unsigned    iwMac;      // set to max size of the FPM bit array, based on small or big msfs
    unsigned    iwRover;    // heuristic approach for finding a free page quickly
    CB          cbPg;
    bool        fBigMsf;

    Array<native_word>
                rgw;
    native_word wFill;          // keep track of the last fill type, as any
                                // appending we do to the array needs to be this type

    FPM() {
        iwMac = iwMax;
        iwRover = 0;
        wFill = 0;
        fBigMsf = false;
    }

    void init(CB cbitsFpm, CB cbPg_, bool fBigMsf_) {
        iwMac = cbitsFpm/(CHAR_BIT*sizeof(native_word));
        iwRover = 0;
        wFill = 0;
        cbPg = cbPg_;
        fBigMsf = fBigMsf_;
    }

    PV pvBase() const {
        return PV(rgw.pBase());
    }

    PV pvEnd() const {
        return PV(rgw.pBase() + rgw.size());
    }

    unsigned mppnil(unsigned pn) {
        return pn >> lgBPW;
    }

    unsigned mppniw(unsigned pn) {
        return pn >> lgBPW;
    }
    
    native_word mppnmask(unsigned pn) {
        return native_word(1) << (pn & (BPW-1));
    }
    
    bool isFreePn(UPN upn) {
        unsigned    iw = mppniw(upn);
        if (iw >= rgw.size() && iw < iwMac) {
            // catch all of the virtual pages that we would have if
            // we were still using a static array
            //
            return wFill != 0;
        }
        else if (iw < rgw.size()) {
            // check an actual entry
            //
            return !!(rgw[mppniw(upn)] & mppnmask(upn));
        }
        else {
            // outta here.  no more room.
            //
            return false;
        }
    }
    
    void allocPn(UPN upn) {
        assert(upn != upnNil && isFreePn(upn));
        assert(mppniw(upn) < rgw.size());
        rgw[mppniw(upn)] &= ~mppnmask(upn);
            
    }
    
    void freePn(UPN upn) {
        assert(mppniw(upn) < rgw.size());
        if (upn != upnNil) {
            rgw[mppniw(upn)] |= mppnmask(upn);
        }
        iwRover = 0;
    }
    
    void setAll() {
        rgw.fill(wFill = ~native_word(0));
        iwRover = 0;
    }
    
    void clearAll() {
        rgw.fill(wFill = 0);
        iwRover = 0;
    }
    
    bool add(const FPM& fpm) {
        iwMac = __max(iwMac, fpm.iwMac);
        unsigned    iwMacThis = rgw.size();
        unsigned    iwMacThat = fpm.rgw.size();
        unsigned    iwMacMin = __min(iwMacThis, iwMacThat);
        unsigned    iwMacMax = __max(iwMacThis, iwMacThat);

        if (!rgw.setSize(iwMacMax)) {
            return false;
        }
        // handle all of the common entries
        //
        for (unsigned iw = 0; iw < iwMacMin; iw++) {
            rgw[iw] |= fpm.rgw[iw];
        }
        // handle all of the entries in fpm that are not in this one, (i.e.
        // use our "virtual" fill
        //
        for (unsigned iw = iwMacThis; iw < iwMacThat; iw++) {
            rgw[iw] = wFill | fpm.rgw[iw];
        }
        // handle all of the entries in this one that are not actually in fpm
        // (i.e. use their "virtual" setting in fpm.wFill).
        //
        if (fpm.wFill) {
            // Only need to do this step when fpm.wFill is non-zero.
            //
            for (unsigned iw = iwMacThat; iw < iwMacThis; iw++) {
                rgw[iw] |= fpm.wFill;
            }
        }
        // now do all of the virtual entries from both us and them.
        //
        wFill |= fpm.wFill;
        iwRover = 0;
        return true;
    }
    
    UPN nextPn() {
        unsigned    iwMacT = rgw.size();
        unsigned    iw;
        for (iw = iwRover; iw < iwMacT && rgw[iw] == 0; iw++)
            ;
        iwRover = iw;
        if (iw == iwMacT && iw < iwMac) {
            if (!rgw.append(~native_word(0))) {
                return pnNil;
            }
        }
        native_word w = rgw[iw];
        unsigned    u;
        for (u = 0; u < BPW && !(w & mppnmask(u)); u++)
            ;
        assert(u < BPW);

        UPN upn = UPN(iw * BPW + u);

        if (fBigMsf && fpmPn(upn)) {
            assert(isFreePn(upn));
            assert(isFreePn(upn + 1));

            // We've hit the pages needed for the next page map.
            // So reserve them, and search again.
            unsigned    iwPnFpm = mppniw(upn+1);
            if (rgw.size() < iwPnFpm && !rgw.append(~native_word(0))) {
                return pnNil;
            }
            allocPn(upn);
            allocPn(upn + 1);

            return nextPn();
        }

        allocPn(upn);
        return upn;
    }

    bool fpmPn(UPN pn) {
        // For a bigMsf, the page map pages are scattered
        // at regular intervals throughout the file.

        assert(fBigMsf);

        // Allow the first three special pages to be allocated

        if (pn < cpnBigMsfReserved)
            return false;

        // Every other pair of page map pages starts at offsets
        // 1 and 2 from the start of their page range.
        // (The following calculation works because cbPg is always
        // a power of 2.)

        // REVIEW: Bug.  we should be reserving pages at cbPg * CHAR_BIT
        //  page numbers, thus we are reserving 8 times as many pages as
        //  necessary.
        //
        PN32 pnT = pn & (cbPg - 1);  // pn % cbPg

        if (pnT == 1 || pnT == 2)
            return true;

        return false;
    }

    FPM & operator=(const FPM & fpm) {
        if (rgw.setSize(fpm.rgw.size())) {
            memcpy(rgw.pBase(), fpm.rgw.pBase(), rgw.size() * sizeof(native_word));
            iwMac = fpm.iwMac;
            iwRover = 0;
            cbPg = fpm.cbPg;
            fBigMsf = fpm.fBigMsf;
            wFill = fpm.wFill;
        }

        return *this;
    }

    bool fEnsureRoom(UPN upnMac) {
        unsigned    iwMac = mppniw(upnMac + BPW - 1);
        bool        fRet = true;

        if (iwMac > rgw.size()) {
            unsigned    iwMacCur = rgw.size();
            fRet = !!rgw.setSize(iwMac);
            if (fRet) {
                for (unsigned iwT = iwMacCur; iwT < iwMac; iwT++) {
                    rgw[iwT] = wFill;
                }
            }
        }
        return fRet;
    }

private:
    FPM(const FPM&);

};

struct StrmTbl { // (in memory) stream table
    enum serOp { ser, deser, size };

    // The stream table is serialized as follows:  Number of streams, followed by the stream table's SN->SI map (an array
    // of SI's), followed by each SI's SPN->PN map. So in this calculation, (u)pnMaxMax*sizeof((U)PN) takes into account
    // that the total number of SPN's for all SI's can be at most (u)pnMaxMax.

    enum {
        cbMaxSerialization = snMax*sizeof(SI_PERSIST) + sizeof(SN) + sizeof(ushort) + pnMaxMax*sizeof(PN),
        cbBigMSFMaxSer = unsnMax*sizeof(SI_PERSIST) + sizeof(UNSN) + upnMaxMax*sizeof(UPN)
    };


    unsigned    lgcbPage;
    UNSN        m_unsnMax;

    bool
        (StrmTbl::* m_pmfnSerialize)(serOp, PB, CB*, MSF_HB *, MSF_EC *);

    mutable
    Array<SI>   mpsnsi;
    
    void init(unsigned lgcbPage_, bool fBigMsf=false) {
        lgcbPage = lgcbPage_;

        if (fBigMsf) {
            m_unsnMax = ::unsnMax;
            m_pmfnSerialize = &StrmTbl::internalSerializeBigMsf2;
        }
        else {
            m_unsnMax = ::snMax;
            m_pmfnSerialize = &StrmTbl::internalSerializeSmallMsf;
        }
    }

    StrmTbl() : lgcbPage(0), m_unsnMax(0) {
        mpsnsi.setSize(256);    // start with a reasonable allocation
        mpsnsi.setSize(5);      // scale it back to the actual size needed for bootstrapping
    }

    ~StrmTbl() {
        dealloc();
    }
    void dealloc() { // idempotent because SI::dealloc() is
        UNSN    unsnMac = snMac();
        for (UNSN sn = 0; sn < unsnMac; sn++)
            mpsnsi[sn].dealloc();
    }
    UNSN snMinFree() const {
        UNSN    unsnMac = snMac();
        for (UNSN sn = snUserMin; sn < unsnMac; sn++) {
            if (!mpsnsi[sn].isValid()) {
                return sn;
            }
        }
        if (unsnMac < unsnNil && mpsnsi.size() < m_unsnMax) {
            unsigned    isi;
            if (mpsnsi.append(siNil, isi)) {
                return isi;
            }
        }
        return unsnNil;
    }
    UNSN snMac() const {
        // Find snMac, the largest sn such that mpsnsi[snMac-1].isValid(),
        // or 0 if there does not exist any mpsnsi[sn].isValid().
        UNSN    sn;
        for (sn = mpsnsi.size(); sn > 0 && !mpsnsi[sn-1].isValid(); sn--)
            ;
        return sn;
    }

    CB cbCopyRgsiDeser(SI * psiDst, void * pvSrc, UNSN isiMac, bool fBigMsf, PB pbMac) {
        // used to go from disk to memory

        if (fBigMsf) {
            // De-serialize the CBs into SIs.
            CB *    pcbSrc = (CB *)pvSrc;
            CB *    pcbSrcMac = pcbSrc + isiMac;

            if (pcbSrcMac <= (CB *)pbMac) {
                for (; pcbSrc < pcbSrcMac; pcbSrc++, psiDst++) {
                    psiDst->cb = *pcbSrc;
                    psiDst->mpspnpn = 0;
                }
            }
            return isiMac * sizeof(CB);
        }

        assert(!fBigMsf);

        SI_PERSIST *    psipSrc = (SI_PERSIST *)pvSrc;
        SI_PERSIST *    psipSrcMac = psipSrc + isiMac;
        if (psipSrcMac <= (SI_PERSIST *)pbMac) {
            if (sizeof(SI_PERSIST) != sizeof(SI)) {
                for (; psipSrc < psipSrcMac; psipSrc++, psiDst++) {
                        psiDst->cb = psipSrc->cb;
                        psiDst->mpspnpn = 0;
                }
            }
            else {
                memcpy(psiDst, pvSrc, isiMac * sizeof(SI_PERSIST));
            }
        }
        return isiMac * sizeof(SI_PERSIST);
    }

    CB cbCopyRgsiSer(void * pvDst, SI * psiSrc, UNSN isiMac, bool fBigMsf) {
        // used to go from memory to disk

        if (fBigMsf) {
            // All we've stored is byte counts, so no pointer adjustment is necessary.
            // Copy the count of bytes.

            CB *    pcbDst = (CB *)pvDst;
            CB *    pcbDstMac = pcbDst + isiMac;

            for (; pcbDst < pcbDstMac; pcbDst++, psiSrc++) {
                *pcbDst = psiSrc->cb;
            }

            return isiMac * sizeof(CB);
        }

        assert(!fBigMsf);

        if (sizeof(SI_PERSIST) != sizeof(SI)) {
            SI_PERSIST *    psipDst = (SI_PERSIST *) pvDst;
            SI_PERSIST *    psipDstMac = psipDst + isiMac;

            for (; psipDst < psipDstMac; psipDst++, psiSrc++) {
                psipDst->cb = psiSrc->cb;
                psipDst->mpspnpn = 0;
            }
        }
        else {
            memcpy(pvDst, psiSrc, isiMac * sizeof(SI_PERSIST));
        }
        return isiMac * sizeof(SI_PERSIST);
    }
    
    bool serialize(serOp op, PB pb, CB* pcb, MSF_HB *pmsf, MSF_EC *pec) {
        PREFAST_SUPPRESS(6287, "");
        assert(
            m_pmfnSerialize == &StrmTbl::internalSerializeBigMsf2 ||
            m_pmfnSerialize == &StrmTbl::internalSerializeSmallMsf
            );

        return (this->*m_pmfnSerialize)(op, pb, pcb, pmsf, pec);
    }

private:

    friend class MSF_HB;

    bool internalSerializeBigMsf(serOp op, PB pb, CB* pcb, MSF_HB *pmsf, MSF_EC *pec) {

        UNSN snMacT = (op == deser) ? 0 : snMac();

        PB  pbMac = NULL;
        CB  cbReloadBuf = *pcb;
        PB  pbEnd = pb;

        switch (op) {
        case ser:
            assert(snMacT < m_unsnMax);
            *((SPN32*&)pbEnd)++ = snMacT;
            pbEnd += cbCopyRgsiSer((SI_PERSIST*)pbEnd, mpsnsi.pBase(), snMacT, true);
            break;
        case deser:
            pbMac = pb + cbReloadBuf;
            assert(snMacT < m_unsnMax);
            if (cbReloadBuf < sizeof(SPN32)) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }

            snMacT = *((SPN32*&)pbEnd)++;
            if (snMacT >= unsnMax) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }

            if (snMacT > mpsnsi.size() && !mpsnsi.setSize(snMacT)) {
                if (pec) {
                    *pec = MSF_EC_OUT_OF_MEMORY;
                }
                return false;
            }
            pbEnd += cbCopyRgsiDeser(mpsnsi.pBase(), pbEnd, snMacT, true, pbMac);
            if (pbEnd > pbMac) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }
            break;
        case size:
            // NB: size is *always* the serialized size!
            pbEnd += sizeof(SPN32) + snMacT*sizeof(CB);
            break;
        }

        // [de]serialize each valid SI
        UNSN    sn;
        for (sn = 0; sn < snMacT; sn++) {
            SI si = mpsnsi[sn];
            if (si.isValid()) {
                switch (op) {
                case ser:
                    memcpy(pbEnd, si.mpspnpn, si.spnMac(lgcbPage)*sizeof(PN32));
                    break;
                case deser:
                    if (pbEnd + si.spnMac(lgcbPage) * sizeof(PN32) > pbMac) {
                        if (pec) {
                            *pec = MSF_EC_CORRUPT;
                        }
                        return false;
                    }
                    if (!si.allocForCb(si.cb, lgcbPage)) {
                        if (pec) {
                            *pec = MSF_EC_OUT_OF_MEMORY;
                        }
                        return false;
                    }
                    memcpy(si.mpspnpn, pbEnd, si.spnMac(lgcbPage)*sizeof(PN32));
                    mpsnsi[sn] = si;
                    break;
                }
                (PN32*&)pbEnd += si.spnMac(lgcbPage);
            }
        }

        if (op == deser) {
            UNSN    unsnMac = mpsnsi.size();
            for ( ; sn < unsnMac; sn++)
                mpsnsi[sn] = siNil;

            assert(pbEnd <= pbMac);
            if (pbEnd != pbMac) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }
        }

        *pcb = static_cast<CB>(pbEnd - pb);
        return true;
    }


    bool internalSerializeSmallMsf(serOp op, PB pb, CB* pcb, MSF_HB *pmsf, MSF_EC *pec) {

        UNSN snMacT = (op == deser) ? 0 : snMac();

        PB  pbMac = NULL;
        CB  cbReloadBuf = *pcb;
        PB  pbEnd = pb;

        switch (op) {
        case ser:
            assert(snMacT < m_unsnMax);
            *((SN*&)pbEnd)++ = SN(snMacT);
            *((ushort*&)pbEnd)++ = 0;
            pbEnd  += cbCopyRgsiSer((SI_PERSIST*)pbEnd, mpsnsi.pBase(), snMacT, false);
            break;
        case deser:
            assert(snMacT < m_unsnMax);
            if (cbReloadBuf < sizeof(SN) * 2) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }
            assume(cbReloadBuf >= 0);

            pbMac = pb + cbReloadBuf;

            snMacT = *((SN*&)pbEnd)++;
            ((ushort*&)pbEnd)++;
            if (snMacT >= snMax) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }

            if (snMacT > mpsnsi.size() && !mpsnsi.setSize(snMacT)) {
                if (pec) {
                    *pec = MSF_EC_OUT_OF_MEMORY;
                }
                return false;
            }
            pbEnd += cbCopyRgsiDeser(mpsnsi.pBase(), (SI_PERSIST*)pbEnd, snMacT, false, pbMac);
            if (pbEnd > pbMac) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }
            break;
        case size:
            // NB: size is *always* the serialized size!
            pbEnd += sizeof(SN) + sizeof(ushort) + snMacT*sizeof(SI_PERSIST);
            break;
        }

        UNSN    sn;
        for (sn = 0; sn < snMacT; sn++) {
            SI si = mpsnsi[sn];
            if (si.isValid()) {
                switch (op) {
                case ser: {
                    for (SPN spn = 0; spn < si.spnMac(lgcbPage); spn++) {
                        *(PN *)pbEnd = PN(si.mpspnpn[spn]);
                        pbEnd += sizeof(PN);
                        }
                    }
                    break;
                case deser: {
                    SPN32 spnMac = si.spnMac(lgcbPage);
                    if (pbEnd + spnMac * sizeof(PN) > pbMac) {
                        if (pec) {
                            *pec = MSF_EC_CORRUPT;
                        }
                        return false;
                    }
                    if (!si.allocForCb(si.cb, lgcbPage)) {
                        if (pec) {
                            *pec = MSF_EC_OUT_OF_MEMORY;
                        }
                        return false;
                    }

                    for (SPN spn = 0; spn < spnMac; spn++) {
                        si.mpspnpn[spn] = *(PN *) pbEnd;
                        pbEnd += sizeof(PN);
                    }

                    mpsnsi[sn] = si;
                    }
                    break;

                case size:
                    pbEnd += si.spnMac(lgcbPage) * sizeof(PN);
                    break;
                }
            }
        }

        if (op == deser) {
            UNSN    unsnMac = mpsnsi.size();
            for ( ; sn < unsnMac; sn++)
                mpsnsi[sn] = siNil;

            assert(pbEnd <= pbMac);
            if (pbEnd != pbMac) {
                if (pec) {
                    *pec = MSF_EC_CORRUPT;
                }
                return false;
            }
        }

        *pcb = CB(pbEnd - pb);  // REVIEW:WIN64 CAST
        return true;
    }

    bool internalSerializeBigMsf2(serOp op, PB pb, CB* pcb, MSF_HB *pmsf, MSF_EC *pec);
};



struct PG {
    char rgb[cbPgMax];
};

union MSF_HDR { // page 0
    struct {
        char szMagic[0x2C];
        CB  cbPg;       // page size
        PN  pnFpm;      // page no. of valid FPM
        PN  pnMac;      // current no. of pages
        SI_PERSIST
            siSt;       // stream table stream info
        PN  mpspnpn[cpnMaxForCb(StrmTbl::cbMaxSerialization)];
    };
    PG pg;
};

union BIGMSF_HDR {  // page 0 (and more if necessary)
    struct {
        char    szMagic[0x1e];  // version string
        CB  cbPg;               // page size
        UPN pnFpm;              // page no. of valid FPM
        UPN pnMac;              // current no. of pages
        SI_PERSIST              
            siSt;               // stream table stream info
        PN32 mpspnpnSt[cpnMaxForCb(cpnMaxForCb(StrmTbl::cbBigMSFMaxSer) * sizeof(PN32))];
    };
    PG  pg;
};

cassert(sizeof(PG) == sizeof(BIGMSF_HDR));

static const char szHdrMagic[0x2c] =    "Microsoft C/C++ program database 2.00\r\n\x1a\x4a\x47";
static const char szBigHdrMagic[0x1e] = "Microsoft C/C++ MSF 7.00\r\n\x1a\x44\x53";

class MSF_HB :public MSF { // multistream file, home brewed version
public:

    friend MSF;

    enum { impv = 19960101 };
    enum serOp { ser, deser };
    MSF_HB() 
#ifdef PDB_MT
        : m_fOpenedFromStream(true)  
#endif
    { 
        PDBLOG_FUNC();
    }
    ~MSF_HB() {
        PDBLOG_FUNC();
        siPnList.dealloc();
    }

    long    QueryImplementationVersion() { return impv; }
    long    QueryInterfaceVersion() { return intvBase; }

    CB  GetCbPage();
    CB  GetCbStream(SN sn);
    SN  GetFreeSn();
    BOOL ReadStream(SN sn, PV pvBuf, CB cbBuf);
    BOOL ReadStream(SN sn, OFF off, PV pvBuf, CB* pcbBuf);
    BOOL WriteStream(SN sn, OFF off, PV pvBuf, CB cbBuf);
    BOOL ReplaceStream(SN sn, PV pvBuf, CB cbBuf);
    BOOL AppendStream(SN sn, PV pvBuf, CB cbBuf);
    BOOL TruncateStream(SN sn, CB cb);
    BOOL DeleteStream(SN sn);
    BOOL Commit(MSF_EC *pec = NULL);
    BOOL Close();
    BOOL GetRawBytes(PFNfReadMSFRawBytes fSNarfRawBytes);
    UNSN SnMax() const { return fBigMsf ? unsnMax : snMax; }
#ifdef PDB_MT
    BOOL FSupportsSharing() const { return TRUE; }
#else
    BOOL FSupportsSharing() const { return FALSE; }
#endif
    BOOL CloseStream(UNSN) { return TRUE; }
    bool FIsBigMsf() const { return fBigMsf; }
    bool FSetCompression(CompressionType ct, bool fCompress) const;
    bool FGetCompression(CompressionType ct, bool & fPrevCompress) const;
    BOOL NonTransactionalEraseStream(SN sn);

private:

    friend struct StrmTbl;

    union {
        MSF_HDR     hdr;
        BIGMSF_HDR  bighdr;
    };

    FPM         fpm;
    FPM         fpmFreed;
    FPM         fpmCommitted;
    StrmTbl     st;
    MSFParms    msfparms;
    SI          siPnList;

    // MTS NOTE: access to fBigMsf is not protected since it only changes in
    // internalOpen/internalStreamOpen and stay constant afterwards. These function
    // are called before a new MSF pointer is given to the user in MSF::Open family of functions
    // so it must be MTS.
    bool        fBigMsf;

#ifdef PDB_MT
    FILE_ID                 m_fileid;
    bool                    m_fOpenedFromStream;
    mutable CriticalSection m_cs;           // UNDONE MTS: should think about better syncronizations
#endif

    COMRefPtr<IStreamInternal> pIStream;

    void init();
    BOOL load(MSF_EC *pec);
    BOOL create(MSF_EC* pec, CB cbPage);
    BOOL internalOpen(const wchar_t *wszFilename, BOOL fWrite, MSF_EC* pec, CB cbPage =cbPgDef);
    BOOL internalStreamOpen(IStream* pIStream_, BOOL fWrite, MSF_EC *pec, CB cbPage_);
    BOOL afterOpen(MSF_EC* pec );
    BOOL afterCreate( MSF_EC* pec, CB cbPage );
    BOOL switchToCRTStream();
    BOOL setStreamSize( DWORD dwSize );
    BOOL internalReplaceStream(UNSN sn, PV pvBuf, CB cbBuf);
    BOOL internalDeleteStream(UNSN sn, bool fErase = false);
    BOOL readWriteStream(SI si, OFF off, PV pvBuf, CB* pcbBuf,
                         BOOL (MSF_HB::*pRW)(UPN*, OFF, CB, PV),
                         BOOL (MSF_HB::*pRWPn)(UPN*, PV));

    BOOL internalReadStream(SI si, OFF off, PV pvBuf, CB* pcbBuf);

    BOOL validCbPg(CB cbPage) {
        const MSFParms *rgmsfparms_;
        int cparms;
        
        if (fBigMsf) {
            rgmsfparms_ = rgmsfparms_hc;
            cparms = _countof(rgmsfparms_hc);
        }
        else {
            rgmsfparms_ = rgmsfparms;
            cparms = _countof(rgmsfparms);
        }

        for (int i = 0; i < cparms; i++) {
            if (cbPage == rgmsfparms_[i].cbPg) {
                msfparms = rgmsfparms_[i];
                return TRUE;
            }
        }
        return FALSE;
    }

    CB  cbPg() const {
        return msfparms.cbPg;
    }
    unsigned lgCbPg() const {
        return msfparms.lgCbPg;
    }
    unsigned maskCbPgMod() const {
        return msfparms.maskCbPgMod;
    }
    UPN pnMax() const {
        return msfparms.pnMax;
    }
    BOOL validSn(UNSN sn) {
        return 0 <= sn && sn < unsnMax;
    }
    BOOL validUserSn(UNSN sn) {
        return validSn(sn) && sn != snSt;
    }
    BOOL extantSn(UNSN sn) {
        return validSn(sn) && sn < st.mpsnsi.size() && st.mpsnsi[sn].cb != cbNil;
    }
    BOOL validPn(UPN pn) {
        return 0 <= pn && pn < pnMax();
    } 
    BOOL extantPn(UPN pn) {
        return validPn(pn) && pn < pnMac();
    }
    UPN allocPn() {
        UPN pn = fpm.nextPn();
        if (pn != upnNil) {
            assert(pn <= pnMac() || (fBigMsf && fpm.fpmPn(pnMac() + 1) && pn <= pnMac() + 2));
            if (pn < pnMac())
                return pn;
            else {
                // cap the size by msfparms.pnMax - 1.  With 16-bit PNs, pnNil == PN(-1) so we cannot
                // actually allocate a PN == 0xffff.
                //
                
                // Must have the max of returned pn and current mac, since we now can alloc more
                // than one page in fpm.nextPn() due to fpm page reservation.
                //
                UPN pnMacMax = __max(pnMac(), pn);
                UPN upnMacNew = __min(pnMacMax + msfparms.cpgFileGrowth, UPN(msfparms.pnMax - 1));
                assert(pn < upnMacNew);

                // chsize the MSF only if we are going to be growing it (ie, not at the
                //  limit already)
                //
                if (upnMacNew > pnMac() && setStreamSize(upnMacNew * cbPg()) ) {
                    pnMac(upnMacNew);
                    return pn;
                }
                else {
                    fpm.freePn(pn); // back out
                    return upnNil;
                }
            }
        }
        return upnNil;
    }
    void freePn(UPN pn) {
        if (fpmCommitted.isFreePn(pn)) {
            // if the page was not used in the previous committed set of pages,
            // it is available again immediately in this transaction.
            //
            fpm.freePn(pn);
        }
        else {
            // otherwise, add to the standard free list.
            //
            fpmFreed.fEnsureRoom(pn + 1);
            fpmFreed.freePn(pn);
        }
    }
    BOOL readPn(UPN pn, PV buf) {
        return readPnOffCb(pn, 0, cbPg(), buf);
    }
    BOOL zeroPn(UPN pn) {
        PG pg;
        if (!readPn(pn, &pg)) {
            return FALSE;
        }
        memset(pg.rgb, 0, cbPg());
        return writePnOffCb(pn, 0, cbPg(), &pg);
    }
    BOOL readPpn(UPN* ppn, PV buf) {
        return readPn(*ppn, buf);
    }
    BOOL readPnOffCb(UPN pn, OFF off, CB cb, PV buf) {
        assert(extantPn(pn));
        if (!extantPn(pn)) {
            return FALSE;
        }
        assert(!cb || extantPn(pn + cpnForCbLgCbPg(cb, lgCbPg()) - 1));
        if (cb && !extantPn(pn + cpnForCbLgCbPg(cb, lgCbPg()) - 1)) {
            return FALSE;
        }
        ULONG cbRead = 0;
        return seekPnOff(pn, off) && SUCCEEDED( pIStream->Read(buf, cb, &cbRead) ) && cbRead == ULONG(cb);
    }
    BOOL readPpnOffCb(UPN* ppn, OFF off, CB cb, PV buf) {
        return readPnOffCb(*ppn, off, cb, buf);
    }
    BOOL writePn(UPN pn, PV buf) {
        return writePnCb(pn, cbPg(), buf);
    }
    BOOL writePnCb(UPN pn, CB cb, PV buf) {
        return writePnOffCb(pn, 0, cb, buf);
    }
    BOOL writePnOffCb(UPN pn, OFF off, CB cb, void *buf) {
        assert(extantPn(pn));
        assert(!cb || extantPn(pn + cpnForCbLgCbPg(cb, lgCbPg()) - 1));
        ULONG cbWrite = 0;
        return seekPnOff(pn, off) && SUCCEEDED( pIStream->Write(buf, cb, &cbWrite) ) && cbWrite == ULONG(cb);
    }
    BOOL writeNewDataPgs(SI* psi, USPN spn, PV pvBuf, CB cbBuf) {
        // allocate pages up front to see if we can cluster write them
        CB      cbWrite = cbBuf;
        USPN    spnT = spn;
        UPN     pnNew;
        while (cbWrite > 0) {
            if ((pnNew = allocPn()) == pnNil)
                return FALSE;
            psi->mpspnpn[spnT++] = pnNew;
            cbWrite -= cbPg();
        }

        while (cbBuf > 0) {
            UPN pnStart;
            UPN pnLim;
            CB  cbT;
            pnStart = pnLim = psi->mpspnpn[spn];
            cbWrite = 0;
            do {
                spn++;
                pnLim++;
                cbT = __min(cbPg(), cbBuf);
                cbWrite += cbT;
                cbBuf -= cbT;
            } while (cbBuf > 0 && psi->mpspnpn[spn] == pnLim);

            if (!writePnOffCb(pnStart, 0, cbWrite, pvBuf))
                return FALSE;
            pvBuf = PV(PB(pvBuf) + cbWrite);
        }
        assert(cbBuf == 0);
        return (cbBuf == 0);
    }
    BOOL writeNewPn(UPN *ppn, PV buf) {
        return writeNewPnCb(ppn, cbPg(), buf);
    }
    BOOL writeNewPnCb(UPN *ppn, CB cb, PV buf) {
        assert(cb > 0);
        UPN pn = allocPn();

        if (pn != pnNil && writePnCb(pn, cb, buf)) {
            freePn(*ppn);
            *ppn = pn;
            return TRUE;
        }
        return FALSE;
    }
    BOOL replacePnOffCb(UPN *ppn, OFF off, CB cb, PV buf) {
        assert(off >= 0 && cb > 0 && off + cb <= cbPg());
        PG pg;
        if (!readPn(*ppn, &pg))
            return FALSE;
        memcpy(pg.rgb + off, buf, cb);
        return writeNewPn(ppn, &pg);
    }
    BOOL seekPn(UPN pn) {
        return seekPnOff(pn, 0);
    }
    BOOL seekPnOff(UPN pn, OFF off) {
        assert(extantPn(pn) || pn <= pnMac() + 1);
        assert(off <= cbPg());
        off += pn*cbPg();
        ULARGE_INTEGER pos;
        LARGE_INTEGER lOff;
        lOff.QuadPart = (unsigned __int64) ULONG(off);
        return (pn < pnMax()) && SUCCEEDED(pIStream->Seek(lOff, STREAM_SEEK_SET, &pos)) 
            && pos.QuadPart == ULONG(off);
    }

    MSF_HDR & MsfHdr() {
        return hdr;
    }

    BIGMSF_HDR & BigMsfHdr() {
        return bighdr;
    }

    BOOL fValidHdr() {
        if (memcmp(MsfHdr().szMagic, szHdrMagic, sizeof szHdrMagic) == 0) {
            fBigMsf = false;
            return validCbPg(MsfHdr().cbPg);
        }
        else if (memcmp(BigMsfHdr().szMagic, szBigHdrMagic, sizeof szBigHdrMagic) == 0) {
            fBigMsf = true;
            return validCbPg(BigMsfHdr().cbPg);
        }

        return false;
    }

    UPN pnFpm() {
        return fBigMsf ? BigMsfHdr().pnFpm : MsfHdr().pnFpm;
    }

    UPN pnMac() {
        return fBigMsf ? BigMsfHdr().pnMac : MsfHdr().pnMac;
    }

    void pnMac(UPN pn) {
        if (fBigMsf) {
            BigMsfHdr().pnMac = pn;
        }
        else {
            MsfHdr().pnMac = PN(pn);
        }
    }

    CB cbSt() {
        return fBigMsf ? BigMsfHdr().siSt.cb : MsfHdr().siSt.cb;
    }

    BOOL siStCreateSmallMsf(SI *psiSt, MSF_EC *pec) {

        assert(!fBigMsf);

        unsigned    lgcbPg = lgCbPg();

        if (!psiSt->allocForCb(cbSt(), lgcbPg)) {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }

        USPN        spnMac = psiSt->spnMac(lgcbPg);

        // Have to copy PNs into an array of UPNs

        for (USPN spn = 0; spn < spnMac; spn++) {
            assert(MsfHdr().mpspnpn[spn] != 0);
            psiSt->mpspnpn[spn] = MsfHdr().mpspnpn[spn];
        }

        return TRUE;
    }

    BOOL siStCreateBigMsf(SI *psiSt, CB cb, MSF_EC *pec) {

        // Build siSt from the layers of indirection between here and there.

        unsigned lgcbPg = lgCbPg();
        UPN cpn = cpnForCbLgCbPg(cb, lgcbPg);
        CB cbpn = cpn * sizeof(UPN);

        if (!siPnList.allocForCb(cbpn, lgcbPg)) {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }

        if (cpnForCbLgCbPg(cbpn, lgcbPg) > cpnMaxForCb(cpnMaxForCb(StrmTbl::cbBigMSFMaxSer) * sizeof(PN32))) {
            if (pec) {
                *pec = MSF_EC_CORRUPT;
            }
            return FALSE;
        }
        memcpy(siPnList.mpspnpn, BigMsfHdr().mpspnpnSt, cpnForCbLgCbPg(cbpn, lgcbPg) * sizeof(PN32));

        // siPnList now contains the list of pages that contain the mpspnpn for the stream table

        if (!psiSt->allocForCb(cb, lgcbPg)) {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }

        CB cbmp = cbpn;

        if (!internalReadStream(siPnList, 0, psiSt->mpspnpn, &cbpn)) {
            if (pec) {
                *pec = MSF_EC_FILE_SYSTEM;
            }
            return FALSE;
        }
        if (cbpn != cbmp) {
            if (pec) {
                *pec = MSF_EC_CORRUPT;
            }
            return FALSE;
        }

        return TRUE;
    }

    BOOL serializeFpm(serOp, MSF_EC *pec);
    bool FSerializeMsf(PB pb, CB* pcb, StrmTbl *pst, MSF_EC *pec);

#if defined(_DEBUG) 
    void checkInvariants() {
        // check that every page is either free, freed, or in use in exactly one stream
        FPM fpmInUse;
        fpmInUse.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
        // this is the only place where we call FPM::freePn() w/o making sure we have
        // the bit vector allocated (i.e. every place else is freeing one that exists).
        //
        fpmInUse.fEnsureRoom(pnMac());
        fpmInUse.clearAll();
        UNSN    snMac = st.snMac();
        for (UNSN sn = 0; sn < snMac; sn++) {
            SI si = st.mpsnsi[sn];
            if (!si.isValid())
                continue;
            for (USPN spn = 0; spn < si.spnMac(lgCbPg()); spn++) {
                UPN pn = si.mpspnpn[spn];
                assert(validPn(pn));
                assert(!fpm.isFreePn(pn));
                assert(!fpmFreed.isFreePn(pn));
                assert(!fpmInUse.isFreePn(pn));
                fpmInUse.freePn(pn);
            }
        }

        // Also check the the page numbers reserved in siPnList : they're not in a stream

        if (fBigMsf) {

            UPN cpn = cpnForCbLgCbPg(siPnList.cb, lgCbPg());

            for (USPN spn = 0; spn < cpn; spn++) {
                UPN pn = siPnList.mpspnpn[spn];
                assert(!fpm.isFreePn(pn));
                assert(!fpmFreed.isFreePn(pn));
                assert(!fpmInUse.isFreePn(pn));
                fpmInUse.freePn(pn);
            }
        }

        for (UPN pn = msfparms.pnDataMin; pn < pnMax(); pn++) {
            if (!fBigMsf || !fpm.fpmPn(pn)) {
                assert(fpm.isFreePn(pn) + fpmFreed.isFreePn(pn) + fpmInUse.isFreePn(pn) == 1);
            }
        }
    }
#endif
};


static BOOL OsAwareDeleteFile( const wchar_t *wszFilename )
{
    return DeleteFileW( wszFilename );
}


BOOL MSF_HB::internalStreamOpen(IStream* pIStream_, BOOL fWrite, MSF_EC* pec, CB cbPage_) {
    assert( pIStream == NULL );
    if ((pIStream = IStreamReal::Create(pIStream_)) == NULL ) {
        if ( pec ) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
        return FALSE;
    }
    STATSTG statstg;
    if ( FAILED( pIStream->Stat( &statstg, STATFLAG_NONAME ) ) ) {
        if ( pec ) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        return FALSE;
    }
    if ( statstg.cbSize.QuadPart == 0 ) {
        fBigMsf = true;
        if (!validCbPg(cbPage_)) {
            if ( pec ) {
                *pec = MSF_EC_FORMAT;
            }
            return FALSE;
        }
        return afterCreate( pec, cbPage_ );  // check for writer mode?
    } else {
        return afterOpen( pec );
    }
}

BOOL MSF_HB::internalOpen(const wchar_t *wszFilename, BOOL fWrite, MSF_EC* pec, CB cbPage_) {

    int  fd = -1;
    BOOL fCreated = FALSE;

    if (pec) {
        *pec = MSF_EC_OK;
    }

#if defined(MAPPED_IO)
    HANDLE  h;
    if (fWrite) {
        pIStream = IStreamMemMappedFile::Create(wszFilename, fWrite, &h, &fCreated);
    }
    else
#endif
    {
        pIStream = IStreamFile::Create(wszFilename, fWrite, fWrite, &fCreated );
    }

    if (pIStream == NULL) {
        if (pec) {
            *pec = fWrite ? MSF_EC_FILE_SYSTEM : MSF_EC_NOT_FOUND;
        }
        return FALSE;
    }

    BOOL ret;
    if ( fCreated ) {
        // Just created a new file, initialize the
        // MSF data structures for it.
        if ( !(ret = create(pec, cbPage_)) ) {
            // Failed to initialize the file, delete it!
            pIStream = NULL;
            OsAwareDeleteFile( wszFilename );
        }
    } else {
        ret = afterOpen(pec);
    }

#ifdef PDB_MT
    if ( ret ) {
        verify(GetFileID(wszFilename, &m_fileid));
        m_fOpenedFromStream = false;
    }
#endif

    return ret;
}

BOOL MSF_HB::afterOpen( MSF_EC* pec ) {
                                // VSWhidbey:600553
    fBigMsf = true;             // This is arbitrary, and will be overwritten in fValidHdr().
                                // We do this to avoid uninitialized reads of this variable in pnMac().
    pnMac(1);                   // extantPn(pnHdr) must be TRUE for first readPn()!
    msfparms = rgmsfparms[0];   // need min page size set here for initial read.

    if (!readPn(pnHdr, &hdr)) {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        pIStream = NULL;
        return FALSE;
    }
    
    if (fValidHdr())
    {
        if ( !load( pec ) ) {
            pIStream = NULL;
            return FALSE;
        }
        return TRUE;
    }
    else {
        if (pec) {
            *pec = MSF_EC_FORMAT;
        }
        pIStream = NULL;
        return FALSE;
    }
}

BOOL MSF_HB::load( MSF_EC *pec ) {
    // initialize the dynamic page size vars
    fpm.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
    fpmFreed.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
    fpmCommitted.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);

    unsigned lgcbPg = lgCbPg();

    st.init(lgcbPg, fBigMsf);

    if (!serializeFpm(deser, pec)) {
        return FALSE;
    }

    // Build the stream table stream info from the header, then
    // load the stream table stream and deserialize it
    CB cb = cbSt();
    if (cb < 0) {
        if (pec) {
            *pec = MSF_EC_CORRUPT;
        }
        return FALSE;
    }
    SI siSt;

    if (fBigMsf) {
        if (!siStCreateBigMsf(&siSt, cb, pec))
            return FALSE;
    }
    else {
        if (!siStCreateSmallMsf(&siSt, pec))
            return FALSE;
    }

    PB pbSt(new BYTE[cb]);
    if (!pbSt) {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
        return FALSE;
    }

    if (!internalReadStream(siSt, 0, pbSt, &cb))
    {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        delete [] pbSt;
        return FALSE;
    }

    if (cb != siSt.cb) {
        if (pec) {
            *pec = MSF_EC_CORRUPT;
        }
        delete [] pbSt;
        return FALSE;
    }

    if (!st.serialize(StrmTbl::deser, pbSt, &cb, this, pec))
    {
        delete [] pbSt;
        return FALSE;
    }

    delete [] pbSt;

    // The st.mpsnsi[snSt] just loaded is bogus: it is the StrmTbl stream in effect
    // prior to the previous Commit.  Replace it with the good copy saved
    // in the MSF hdr.
    if (st.mpsnsi[snSt].isValid())
        st.mpsnsi[snSt].dealloc();
    st.mpsnsi[snSt] = siSt;

    init();
#if defined(_DEBUG)
    checkInvariants();
#endif
    if (pec) {
        *pec = MSF_EC_OK;
    }
    return TRUE;
}

void MSF_HB::init() {

    if (fBigMsf)
        BigMsfHdr().pnFpm = (BigMsfHdr().pnFpm == msfparms.pnFpm0) ? msfparms.pnFpm1 : msfparms.pnFpm0;
    else
        MsfHdr().pnFpm = (MsfHdr().pnFpm == PN(msfparms.pnFpm0)) ? PN(msfparms.pnFpm1) : PN(msfparms.pnFpm0);

    fpmFreed.clearAll();        // no pages recently freed

    fpmCommitted = fpm;         // cache current committed pages
}

// Create MSF: create file, hand craft initial hdr,, fpm0, and commit.
BOOL MSF_HB::create(MSF_EC* pec, CB cbPage) {
    int         fd = -1;

    fBigMsf = true;
    if (!validCbPg(cbPage)) {
        if (pec) {
            *pec = MSF_EC_FORMAT;
        }
        return FALSE;
    }

    return afterCreate(pec, cbPage);
}

BOOL MSF_HB::afterCreate(MSF_EC* pec, CB cbPage) {
    // init hdr; when creating a new MSF, always create the BigMsf variant.
    memset(&bighdr, 0, sizeof bighdr);
    memcpy(&bighdr.szMagic, szBigHdrMagic, sizeof szBigHdrMagic);
    bighdr.cbPg = cbPage;
    bighdr.pnFpm  = msfparms.pnFpm0;
    bighdr.pnMac  = msfparms.pnDataMin;
    fpm.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
    fpmFreed.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
    fpmCommitted.init(msfparms.cbitsFpm, msfparms.cbPg, fBigMsf);
    st.init(lgCbPg(), fBigMsf);

    // (each SI in st.mpsnsi is already siNil)

    // init fpm0: mark all non-special pages free
    fpm.setAll();
    fpmCommitted = fpm;

    for (UPN pn = 0; pn < msfparms.pnDataMin; pn++) {
        if (fpm.nextPn() != pn)
            assert(FALSE);
    }

    assert(msfparms.pnFpm0 == 1 && msfparms.pnFpm1 == 2);

    fpmFreed.clearAll(); // no pages freed yet

    // store it!
    if (Commit(pec))
        return TRUE;
    else {
        pIStream = NULL;
        return FALSE;
    }
}

BOOL MSF_HB::switchToCRTStream() {
    if ( pIStream->GetType() == IStreamInternal::StreamTypeMemMappedFile ) {
        // We can downgrade from mem mapped I/O to CRT I/O
        // and save some, now precious, address space ...
        IStreamMemMappedFile *pMemMappedFile = (IStreamMemMappedFile *)(IStreamInternal *)pIStream;
        HANDLE hFile;
        if (pMemMappedFile->Detach(&hFile)) {
            if (hFile != INVALID_HANDLE_VALUE) {
                // Successful detach from the MemMappedFile, attach
                // the file to a CRTFile ...
                pIStream = IStreamFile::Create( hFile );
                if (pIStream == NULL) {
                    // Already detached from the old
                    // IStream, can't attach to it again :-(
                    return FALSE;
                }
            }
            else {
                // Can't detach from MemMappedFile, 
                // just continue using it.
            }
        }
        else {
            // Catastrophic failure, our old MemMappedFile 
            // is not usable anymore. 
            return FALSE;
        }
    }

    return TRUE;
}

BOOL MSF_HB::setStreamSize(DWORD cbNewSize) {
    ULARGE_INTEGER ullSize;
    ullSize.QuadPart = cbNewSize;
    if (SUCCEEDED(pIStream->SetSize(ullSize))) {
        return TRUE;
    }

    // OK, maybe we are using memory mapped files
    // and we hit the address space limit. Let's
    // try to switch to CRT files, and retry the
    // operation.
    if (switchToCRTStream()) {
        return SUCCEEDED(pIStream->SetSize(ullSize));
    }

    return FALSE;
}


BOOL MSF_HB::Commit(MSF_EC *pec) {
    MTS_PROTECT(m_cs);
#if defined(_DEBUG)
    checkInvariants();
#endif

    // write the new stream table to disk as a special stream
    CB cbSt;
    PB pbSt;
    if (!st.serialize(StrmTbl::size, 0, &cbSt, this, pec)) {
        return FALSE;
    }
    
    if (!(pbSt = new BYTE[cbSt])) {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
        return FALSE;
    }

    if (!st.serialize(StrmTbl::ser, pbSt, &cbSt, this, pec)) {
        delete [] pbSt;
        return FALSE;
    }
    
    // The St replacement for the big MSF occurs w/in serialize (above)
    if (!fBigMsf && !internalReplaceStream(snSt, pbSt, cbSt)) {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;  // most likely file I/O error
        }
        delete [] pbSt;
        return FALSE;
    }

    delete [] pbSt;

    // copy the stream table stream info into the header
    UPN pnFpm;

    unsigned lgcbPg = lgCbPg();

    if (fBigMsf) {
        // don't copy the stream table's page numbers, copy the list of pages of these numbers

        bighdr.siSt.cb = st.mpsnsi[snSt].cb;
        bighdr.siSt.mpspnpn = 0;
        assert(cpnForCbLgCbPg(bighdr.siSt.spnMac(lgcbPg)*sizeof(PN32), lgcbPg) * sizeof(PN32) <= sizeof bighdr.mpspnpnSt);

        memcpy(bighdr.mpspnpnSt, siPnList.mpspnpn, cpnForCbLgCbPg(siPnList.cb, lgcbPg) * sizeof(PN32));

        pnFpm = bighdr.pnFpm;
    }
    else {
        hdr.siSt.cb = st.mpsnsi[snSt].cb;
        hdr.siSt.mpspnpn = 0;
        assert(hdr.siSt.spnMac(lgcbPg)*sizeof(PN) <= sizeof hdr.mpspnpn);

        // The in-memory array is wider than the on-disk page list - so copy
        // them by hand.
        for (USPN ispn = 0; ispn < hdr.siSt.spnMac(lgCbPg()); ispn++)
            hdr.mpspnpn[ispn] = PN(st.mpsnsi[snSt].mpspnpn[ispn]);

        pnFpm = hdr.pnFpm;
    }

    // mark pages that have been freed to the next FPM as free.
    if (!fpm.add(fpmFreed)) {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
        return FALSE;
    }
    if (!serializeFpm(ser, pec)) {
        return FALSE;
    }

    // at this point, all pages but hdr safely reside on disk
    if (!writePn(pnHdr, &hdr)) {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        return FALSE;
    }

    if ( FAILED(pIStream->Commit(STGC_DEFAULT)) ) {
        return FALSE;
    }

    init();
    return TRUE;
}

BOOL MSF_HB::Close() {
    PDBLOG_FUNC();
#ifdef PDB_MT
    MTS_PROTECT(s_csForMSFOpen);

    if (!m_fOpenedFromStream) {
        MSF_REC * prec = NULL;

        if (!s_mpOpenedMSF.map(m_fileid, &prec)) {
            return FALSE;
        }

        assert(prec->pmsf == this);

        if (--prec->refcount != 0) {
            return TRUE;
        }

        s_mpOpenedMSF.remove(m_fileid);
    }
#endif

    delete this;
    return TRUE;
}

BOOL MSF_HB::GetRawBytes(PFNfReadMSFRawBytes pfnfSnarfRawBytes) {

    MTS_PROTECT(m_cs);

    CB cb = cbPg();
    PB pBuf = new BYTE[cb];
    if (!pBuf) {
        return FALSE;
    }
    BOOL fOK = TRUE;
    UPN pnmac = pnMac();

    // Dump bytes into the callback a page at a time
    for (UPN pn = 0; pn < pnmac; pn++) {
        if (!readPn(pn, pBuf) ||
            !pfnfSnarfRawBytes(pBuf, cb))
        {
            fOK = FALSE;
            break;
        }
    }
    // Tell callback we've finished the dump, even if error already seen
    if (!pfnfSnarfRawBytes(NULL, 0)) {
        fOK = FALSE;
    }
    delete [] pBuf;
    return fOK;
}

CB MSF_HB::GetCbPage() {
    // UNDONE MTS: Probably don't need to protect this
    MTS_PROTECT(m_cs);

    return cbPg();
}

CB MSF_HB::GetCbStream(SN sn) {
    MTS_PROTECT(m_cs);
    return validUserSn(SN(sn)) && extantSn(SN(sn)) ? st.mpsnsi[sn].cb : cbNil;
}

SN MSF_HB::GetFreeSn() {
#ifdef PDB_MT
    MTS_PROTECT(m_cs);
    SN sn = SN(st.snMinFree());

    assert(sn == snNil || validUserSn(sn));

    if (sn == snNil || !internalReplaceStream(sn, 0, 0)) {
        return snNil;
    }
    return sn;
#else
    return SN(st.snMinFree());
#endif
}

BOOL MSF_HB::ReadStream(SN sn, PV pvBuf, CB cbBuf)
{
    CB cbT = cbBuf;
    return ReadStream(sn, 0, pvBuf, &cbT) && cbT == cbBuf;
}

BOOL MSF_HB::ReadStream(SN sn, OFF off, PV pvBuf, CB* pcbBuf) {
    MTS_PROTECT(m_cs);
    return validUserSn(SN(sn)) && extantSn(SN(sn)) &&
           internalReadStream(st.mpsnsi[sn], off, pvBuf, pcbBuf);
}

// Overwrite a piece of a stream.  Will not grow the stream, will fail instead.
BOOL MSF_HB::WriteStream(SN sn, OFF off, PV pvBuf, CB cbBuf) {
    MTS_PROTECT(m_cs);
    return validUserSn(SN(sn)) && extantSn(SN(sn)) &&
           off + cbBuf <= GetCbStream(sn) &&
           readWriteStream(st.mpsnsi[sn], off, pvBuf, &cbBuf,
                           &MSF_HB::replacePnOffCb, &MSF_HB::writeNewPn);
}

// Read a stream, cluster reads contiguous pages
BOOL MSF_HB::internalReadStream(SI si, OFF off, PV pvBuf, CB* pcbBuf) {
    // ensure off and *pcbBuf remain within the stream
    if (off < 0 || off > si.cb || *pcbBuf < 0)
        return FALSE;
    if (*pcbBuf > si.cb - off)
        *pcbBuf = si.cb - off;
    if (*pcbBuf == 0)
        return TRUE;

    CB  cb    = *pcbBuf;
    USPN spn   = USPN(off >> lgCbPg()); //(SPN)(off / cbPg());
    OFF offPg = off & maskCbPgMod();    //off % cbPg();

    // first partial page, if any
    if (offPg != 0) {
        CB cbFirst = __min(cbPg() - offPg, cb);
        if (!readPnOffCb(si.mpspnpn[spn], offPg, cbFirst, pvBuf))
            return FALSE;
        // PREfast defect 22019: detect integer underflow
        if ((cb - cbFirst) > cb)
            return FALSE;
        cb -= cbFirst;
        spn++;
        pvBuf = (PB)pvBuf + cbFirst;
    }

    // intermediate full pages, if any
    while (cb > 0 ) {
        // accumulate contiguous pages into one big read
        UPN pnStart;
        UPN pnLim;
        CB  cbT;
        CB  cbRead = 0;
        pnStart = pnLim = si.mpspnpn[spn];
        do {
            spn++;
            pnLim++;
            cbT = __min(cbPg(), cb);
            cbRead += cbT;
            cb -= cbT;
        } while (cb > 0 && si.mpspnpn[spn] == pnLim);

        if (!readPnOffCb(pnStart, 0, cbRead, pvBuf))
            return FALSE;
        pvBuf = PV(PB(pvBuf) + cbRead);
    }
    assert(cb == 0);
    return TRUE;
}
// Read or write a piece of a stream.
BOOL MSF_HB::readWriteStream(SI si, OFF off, PV pvBuf, CB* pcbBuf,
                          BOOL (MSF_HB::*pRW)(UPN*, OFF, CB, PV),
                          BOOL (MSF_HB::*pRWPn)(UPN*, PV))
{
    // ensure off and *pcbBuf remain within the stream
    if (off < 0 || off > si.cb || *pcbBuf < 0)
        return FALSE;
    if (off + *pcbBuf > si.cb)
        *pcbBuf = si.cb - off;
    if (*pcbBuf == 0)
        return TRUE;

    CB  cb    = *pcbBuf;
    USPN spn  = USPN(off >> lgCbPg());  //(SPN)(off / cbPg());
    OFF offPg = off & maskCbPgMod();    //off % cbPg();

    // first partial page, if any
    if (offPg != 0) {
        CB cbFirst = __min(cbPg() - offPg, cb);
        if (!(this->*pRW)(&si.mpspnpn[spn], offPg, cbFirst, pvBuf))
            return FALSE;
        // PREfast defect 22019: detect integer underflow
        if ((cb - cbFirst) > cb)
            return FALSE;
        cb -= cbFirst;
        spn++;
        pvBuf = (PB)pvBuf + cbFirst;
    }

    // intermediate full pages, if any
    for ( ; cb >= cbPg(); cb -= cbPg(), spn++, pvBuf = (PB)pvBuf + cbPg()) {
        if (!(this->*pRWPn)(&si.mpspnpn[spn], (PB)pvBuf))
            return FALSE;
    }
    // last partial page, if any
    if (cb > 0 && !(this->*pRW)(&si.mpspnpn[spn], 0, cb, pvBuf))
        return FALSE;

    return TRUE;
}

BOOL MSF_HB::ReplaceStream(SN sn, PV pvBuf, CB cbBuf) {

    MTS_PROTECT(m_cs);

    return validUserSn(sn) && internalReplaceStream(sn, pvBuf, cbBuf);
}

BOOL MSF_HB::internalReplaceStream(UNSN sn, PV pvBuf, CB cbBuf) {
    if (!validSn(sn) || cbBuf < 0)
        return FALSE;

    if (extantSn(sn))
        internalDeleteStream(sn);

    SI si;
    if (!si.allocForCb(cbBuf, lgCbPg()))
        return FALSE;
    else if (!writeNewDataPgs(&si, 0, pvBuf, cbBuf)) {
        // VSW:556849 - Explicitly deallocate memory allocated in allocForCb() if we fail to write.
        // This is done rather than defining a dtor for SI which frees this memory, because SI's don't
        // appear to really 'own' their memory - there is no copy-ctor, e.g., so when you assign between
        // them, which is done throughout this code, it just copies the raw pointer to the new SI.
        si.dealloc();
        return FALSE;
    }

    if (sn > st.mpsnsi.size()) {
        // PDB must be corrupted.
        si.dealloc();
        return FALSE;
    }

    st.mpsnsi[sn] = si;
    return TRUE;
}

BOOL MSF_HB::AppendStream(SN sn, PV pvBuf, CB cbBuf) {
    
    MTS_PROTECT(m_cs);

    if (!validUserSn(sn) || !extantSn(sn) || cbBuf < 0)
        return FALSE;
    if (cbBuf == 0)
        return TRUE;

    bool    fDeallocSi = false;
    SI      si = st.mpsnsi[sn];

    if (si.spnMac(lgCbPg()) < cpnForCbLgCbPg(si.cb + cbBuf, lgCbPg())) {
        // allocate a new SI, copied from the old one
        SI siNew;
        if (!siNew.allocForCb(si.cb + cbBuf, lgCbPg()))
            return FALSE;

        // copy the old stuff over
        UPN   spnForSiMac = si.spnMac(lgCbPg());
        memcpy(siNew.mpspnpn, si.mpspnpn, spnForSiMac * sizeof(UPN));

        // initialize the new
        UPN   spnForSiNewMac = siNew.spnMac(lgCbPg());
        for (UPN spn = spnForSiMac; spn < spnForSiNewMac; spn++)
            siNew.mpspnpn[spn] = pnNil;

        siNew.cb = si.cb;   // so far, nothing has been appended
        si = siNew;
        fDeallocSi = true;
    }

    OFF offLast = si.cb & maskCbPgMod();    //si.cb % cbPg();
    if (offLast) {
        // Fill any space on the last page of the stream.  Writes
        // to the current (likely nontransacted) page which is safe on
        // most "extend-stream" type Append scenarios: if the transaction
        // aborts, the stream info is not updated and no preexisting
        // data is overwritten.
        //
        // This is a dangerous optimization which (to guard) we now incur
        // overhead elsewhere; see comment on Truncate()/Append() interaction
        // in TruncateStream().
        UPN pnLast = si.mpspnpn[si.spnMac(lgCbPg()) - 1];
        CB cbFirst = __min(cbPg() - offLast, cbBuf);
        if (!writePnOffCb(pnLast, offLast, cbFirst, pvBuf)) {
            if (fDeallocSi) {
                assert(si.mpspnpn != st.mpsnsi[sn].mpspnpn);
                si.dealloc();
            }
            return FALSE;
        }
        si.cb += cbFirst;
        cbBuf -= cbFirst;
        pvBuf = PB(pvBuf) + cbFirst;
    }

    if (cbBuf > 0) {
        // append additional data and update the stream map
        if (!writeNewDataPgs(&si, USPN(si.spnMac(lgCbPg())), pvBuf, cbBuf)) {
            if (fDeallocSi) {
                assert(si.mpspnpn != st.mpsnsi[sn].mpspnpn);
                si.dealloc();
            }
            return FALSE;
        }
        si.cb += cbBuf;
    }

    if (fDeallocSi) {
        assert(si.mpspnpn != st.mpsnsi[sn].mpspnpn);
        st.mpsnsi[sn].dealloc();// free original SI
    }
    else {
        assert(si.mpspnpn == st.mpsnsi[sn].mpspnpn);
    }
    st.mpsnsi[sn] = si;         // store back the new one
    return TRUE;
}

BOOL MSF_HB::TruncateStream(SN sn, CB cb) {
    
    MTS_PROTECT(m_cs);

    if (!validUserSn(SN(sn)) || !extantSn(SN(sn)))
        return FALSE;

    SI si = st.mpsnsi[sn];
    if (cb > si.cb || cb < 0)
        return FALSE;

    USPN spnNewMac = USPN(cpnForCbLgCbPg(cb, lgCbPg()));
    bool fNewSi = false;

    if (spnNewMac < si.spnMac(lgCbPg())) {
        // The new stream length requires fewer pages...

        fNewSi = true;

        // Allocate a new SI, copied from the old one.
        SI siNew;
        if (!siNew.allocForCb(cb, lgCbPg()))
            return FALSE;
        memcpy(siNew.mpspnpn, si.mpspnpn, spnNewMac*sizeof(UPN));

        // Free subsequent, unneeded pages.
        for (USPN spn = spnNewMac; spn < si.spnMac(lgCbPg()); spn++)
            freePn(si.mpspnpn[spn]);

        si.dealloc();
        st.mpsnsi[sn] = siNil;
        si = siNew;
    }
    si.cb = cb;

    // In case of Truncate(sn, cb) where cb > 0, and in case the Truncate()
    // is followed by an Append(), we must copy the new last partial page
    // of the stream to a transacted page, because the subsequent Append()
    // is optimized to write new stuff to the last (e.g. current,
    // nontransacted) page of the stream.  So, the scenario Truncate()
    // then Append() may need this code or else on transacation abort
    // we could damage former contents of the stream.
    OFF offLast = si.cb & maskCbPgMod();    //si.cb % cbPg();
    if (offLast != 0) {
        PG pg;
        assert(si.spnMac(lgCbPg()) > 0);
        if (!readPn(si.mpspnpn[si.spnMac(lgCbPg())-1], &pg) ||
            !writeNewPn(&si.mpspnpn[si.spnMac(lgCbPg())-1], &pg)) {
            if (fNewSi) {
                si.dealloc();
            }
            return FALSE;
        }
    }

    st.mpsnsi[sn] = si;
    return TRUE;
}

// called only from PDB1::CopyToW2().
BOOL MSF_HB::NonTransactionalEraseStream(SN sn) {

    MTS_PROTECT(m_cs);

    return validUserSn(sn) && internalDeleteStream(sn, true);
}

BOOL MSF_HB::DeleteStream(SN sn) {

    MTS_PROTECT(m_cs);

    return validUserSn(sn) && internalDeleteStream(sn);
}

BOOL MSF_HB::internalDeleteStream(UNSN sn, bool fErase) {
    if (!extantSn(sn))
        return FALSE;

    SI si = st.mpsnsi[sn];
    for (USPN spn = 0; spn < si.spnMac(lgCbPg()); spn++) {
        if (fErase && !zeroPn(si.mpspnpn[spn])) {
            return FALSE;
        }
        freePn(si.mpspnpn[spn]);
    }

    si.dealloc();
    st.mpsnsi[sn] = siNil;
    return TRUE;
}


MSF *
MSF::Open(IStream* pIStream, BOOL fWrite, MSF_EC *pec, CB cbPage) {
    PDBLOG_FUNC();
    assert( pIStream );

    MSF_HB* pmsf = new MSF_HB;

    if (pmsf) {
        if (pmsf->internalStreamOpen(pIStream, fWrite, pec, cbPage)) {
            return pmsf;
        }

        delete pmsf;
    }
    else {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
    }

    return NULL;
}

#ifdef PDB_MT
Map<MSF::FILE_ID, MSF::MSF_REC, HashClassCRC<MSF::FILE_ID> > MSF::s_mpOpenedMSF;
CriticalSection MSF::s_csForMSFOpen;

BOOL
MSF::GetFileID(const wchar_t * wszFilename, MSF::FILE_ID * id) {
    PDBLOG_FUNC();
    assert(wszFilename != NULL);
    assert(id != NULL);
    
    HANDLE hfile = ::CreateFileW(wszFilename, 0, 
                FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                OPEN_EXISTING, 0, NULL);

    if (hfile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    BOOL ret = GetFileID(hfile, id);

    CloseHandle(hfile);

    return ret;
}

BOOL
MSF::GetFileID(HANDLE hfile, MSF::FILE_ID * id) {
    PDBLOG_FUNC();

    if (GetFileInformationByHandleEx(hfile, (FILE_INFO_BY_HANDLE_CLASS) 0x12 /* FileIdInfo */, id, sizeof(MSF::FILE_ID)) == 0) {
        DWORD ec = GetLastError();

        if (ec != ERROR_INVALID_PARAMETER && ec != ERROR_INVALID_LEVEL) {
            return FALSE;
        }

        BY_HANDLE_FILE_INFORMATION info;

        if (GetFileInformationByHandle(hfile, &info) == 0) {
            return FALSE;
        }

        QWORD qwId = (((QWORD) info.nFileIndexHigh) << 32) | ((QWORD) info.nFileIndexLow);

        id->qwVolumeSerialNumber = info.dwVolumeSerialNumber;
        memcpy(id->rgb, &qwId, sizeof(QWORD));
        memset(id->rgb + sizeof(QWORD), 0, sizeof(QWORD));
    }

    return TRUE;
}
#endif

MSF *
MSF::Open(const wchar_t *wszFilename, BOOL fWrite, MSF_EC *pec, CB cbPage, MSF_FLAG msff) {
    PDBLOG_FUNC();
    PDBLOG_FUNCARG(wszFilename);
    PDBLOG_FUNCARG(fWrite);

#ifdef PDB_MT
    MTS_PROTECT(s_csForMSFOpen);

    FILE_ID file_id;
    if (GetFileID(wszFilename, &file_id)) {
        MSF_REC * prec;

        if (s_mpOpenedMSF.map(file_id, &prec)) {
            if (fWrite && !prec->fWrite) {
                if (pec) {
                    *pec = MSF_EC_ACCESS_DENIED;
                }
                // return the MSF if access is quested, but don't ref count it
                return (msff & msffAccess? prec->pmsf : NULL);
            }
            prec->refcount++;
            PDBLOG_FUNCRET(prec->pmsf);
            return prec->pmsf;
        }
    }
#endif

    MSF_HB* pmsf = new MSF_HB;

    if (pmsf) {
        if (pmsf->internalOpen(wszFilename, fWrite, pec, cbPage)) {
#ifdef PDB_MT
            MSF_REC rec = { pmsf, fWrite, 1 };
            if (!s_mpOpenedMSF.add(pmsf->m_fileid, rec)) {
                delete pmsf;

                if (pec) {
                    *pec = MSF_EC_OUT_OF_MEMORY;
                }

                PDBLOG_FUNCRET(NULL);
                return NULL;
            }
#endif
            PDBLOG_FUNCRET(pmsf);
            return pmsf;
        }

        delete pmsf;
    }
    else {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
    }

    PDBLOG_FUNCRET(NULL);
    return NULL;
}


bool MSF_HB::FSerializeMsf(PB pb, CB* pcb, StrmTbl *pst, MSF_EC *pec) {

    CB cbSer;

    if (!pst->internalSerializeBigMsf(StrmTbl::size, 0, &cbSer, this, pec)) {
        return false;
    }

    if (cbSer != *pcb) {
        if (pec) {
            *pec = MSF_EC_CORRUPT;
        }
        return false;
    }

    // Serialize ST to the buffer

    if (!pst->internalSerializeBigMsf(StrmTbl::ser, pb, &cbSer, this, pec)) {
        return false;
    }

    // Write the ST to disk

    if (!internalReplaceStream(snSt, pb, cbSer)) {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        return false;
    }

    // Calculate how many pages will be needed to serialize this mess.

    // Reserve the pages we'll store this list in, and store the page numbers
    // in siPnList (use a tmp first)

    // Write the list of page numbers in ST's SI to the list of pages

    unsigned lgcbPage = lgCbPg();
    PN32 cpnSerSt = cpnForCbLgCbPg(cbSer, lgcbPage);
    CB cbPn = cpnSerSt * sizeof(PN32);

    SI siT;

    if (!siT.allocForCb(cbPn, lgcbPage)) {
        if (pec) {
            *pec = MSF_EC_OUT_OF_MEMORY;
        }
        return false;
    }
    else if (!writeNewDataPgs(&siT, 0, st.mpsnsi[snSt].mpspnpn, cbPn)) {
        if (pec) {
            *pec = MSF_EC_FILE_SYSTEM;
        }
        // VSW:556849 - Explicitly deallocate memory allocated in allocForCb() if we fail to write.
        // This is done rather than defining a dtor for SI which frees this memory, because SI's don't
        // appear to really 'own' their memory - there is no copy-ctor, e.g., so when you assign between
        // them, which is done throughout this code, it just copies the raw pointer to the new SI.
        siT.dealloc();
        return false;
    }

    // Free the pages we're no longer using in the siPnList

    PN32 cpnOld = cpnForCbLgCbPg(siPnList.cb, lgcbPage);

    if (cpnOld) {
        for (PN32 ipn = 0; ipn < cpnOld; ipn++) {
            assert(validPn(siPnList.mpspnpn[ipn]));
            freePn(siPnList.mpspnpn[ipn]);
        }
    }

    siPnList.dealloc();
    siPnList = siT;

    return true;
}

BOOL MSF_HB::serializeFpm(serOp op, MSF_EC *pec) {

    if (!fBigMsf) {
        if (fpm.fEnsureRoom(msfparms.cbitsFpm)) {
            // we know the free page map is contiguous
            assert(ptrdiff_t(msfparms.cpnFpm * cbPg()) <= PB(fpm.pvEnd()) - PB(fpm.pvBase()));

            if (op == deser) {
                if (!readPnOffCb(pnFpm(), 0, msfparms.cpnFpm * cbPg(), fpm.pvBase())) {
                    if (pec) {
                        *pec = MSF_EC_FILE_SYSTEM;
                    }
                    return FALSE;
                }
                return TRUE;
            }
            else {
                if (!writePnOffCb(pnFpm(), 0, msfparms.cpnFpm * cbPg(), fpm.pvBase())) {
                    if (pec) {
                        *pec = MSF_EC_FILE_SYSTEM;
                    }
                    return FALSE;
                }
                return TRUE;
            }
        }
        else {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }
    }

    UPN pnFirst = BigMsfHdr().pnFpm;

    assert(pnFirst == 1 || pnFirst == 2);
    if ((pnFirst != 1) && (pnFirst != 2)) {
        if (pec) {
            *pec = MSF_EC_FORMAT;
        }
        return FALSE;
    }

    // Based on the size of the file and the size of a page,
    // determine how many pieces the page map was broken into.

    CB cbpg = cbPg();

    // Calc number of bytes needed to represent FPM, aligned to FPM::native_word sizes
    //
    CB cbFpm = (pnMac() + CHAR_BIT - 1) / CHAR_BIT;
    cbFpm = sizeof(FPM::native_word) * ((cbFpm + sizeof(FPM::native_word) - 1) / sizeof(FPM::native_word));

    // Calc number of pages needed to represent that number of bytes
    UPN cpnFpm = (cbFpm + cbpg - 1) / cbpg;

    PN32 pnNextWrite = pnFirst;

    if (op == ser) {

        // align it to paged size of ease of use below
        //
        assert(cpnFpm * cbpg >= unsigned(cbFpm));
        if (!fpm.fEnsureRoom(cpnFpm * cbpg * CHAR_BIT)) {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }

        PB pbFpm = reinterpret_cast<PB>(fpm.pvBase());

        while (cpnFpm) {

            assert(pbFpm >= reinterpret_cast<PB>(fpm.pvBase()));
            assert(pbFpm < reinterpret_cast<PB>(fpm.pvEnd()));

            if (!writePnOffCb(pnNextWrite, 0, cbpg, pbFpm)) {
                if (pec) {
                    *pec = MSF_EC_FILE_SYSTEM;
                }
                return FALSE;
            }

            cpnFpm--;

            // REVIEW: Bug.  we should be writing pages at cbpg * CHAR_BIT page numbers
            // see FPM::fpmPn
            //
            pnNextWrite += cbpg;
            pbFpm += cbpg;
        }

        return TRUE;
    }
    else {
        assert(op == deser);
        
        // Deserialize the non-contiguous free page maps
        
        assert(cpnFpm * cbpg >= unsigned(cbFpm));
        if (!fpm.fEnsureRoom(cpnFpm * cbpg * CHAR_BIT)) {
            if (pec) {
                *pec = MSF_EC_OUT_OF_MEMORY;
            }
            return FALSE;
        }
        
        fpm.setAll();
        
        PB      pbFpm = reinterpret_cast<PB>(fpm.pvBase());
        PN32    pnNextRead = pnFirst;
        
        while (cpnFpm) {
            if (!readPnOffCb(pnNextRead, 0, cbpg, pbFpm)) {
                if (pec) {
                    *pec = MSF_EC_FILE_SYSTEM;
                }
                return false;
            }
            
            cpnFpm--;
            // REVIEW: Bug.  we should be reading pages at cbpg * CHAR_BIT page numbers
            // see FPM::fpmPn
            //
            pnNextRead += cbpg;
            pbFpm += cbpg;
        }
        return TRUE;
    }
    return FALSE;
}

bool MSF_HB::FSetCompression(CompressionType ct, bool fCompress) const {

    MTS_PROTECT(m_cs);

    if (ct == ctFileSystem) {

        HANDLE hFile = pIStream->GetFileHandle();

        if (hFile == INVALID_HANDLE_VALUE) {
            // If we don't have a file handle, we cannot handle file system
            // compression.
            //
            return false;
        }

        if (!fCompress) {
            // We don't uncompress a file here.
            //
            return false;
        }

        bool    fPrevCompress;
        if (!FGetCompression(ctFileSystem, fPrevCompress)) {
            return false;
        }
        if (!fPrevCompress) {
            // Not already compressed, do it.
            //
            USHORT  uscf = COMPRESSION_FORMAT_DEFAULT;
            DWORD   cbDummy;
            if (!DeviceIoControl(hFile, FSCTL_SET_COMPRESSION, &uscf, sizeof(uscf), NULL, 0, &cbDummy, NULL)) {
                return false;
            }
        }
        return true;
    }
    else {
        return false;
    }
}

bool MSF_HB::FGetCompression(MSF::CompressionType ct, bool & fPrevCompress) const {

    MTS_PROTECT(m_cs);

    if (ct == ctFileSystem) {

        HANDLE hFile = pIStream->GetFileHandle();

        if (hFile == INVALID_HANDLE_VALUE) {
            // If we don't have a file handle, we cannot handle file system
            // compression.
            //
            return false;
        }

        USHORT  uscfPrev;
        DWORD   cbDummy;
        if (!DeviceIoControl(hFile, FSCTL_GET_COMPRESSION, NULL, 0, &uscfPrev, sizeof(uscfPrev), &cbDummy, NULL)) {
            return false;
        }
        fPrevCompress = uscfPrev != COMPRESSION_FORMAT_NONE;
        return true;
    }
    else {
        return false;
    }
}

bool StrmTbl::internalSerializeBigMsf2(serOp op, PB pb, CB* pcb, MSF_HB *pmsf, MSF_EC *pec) {

    if (op == size || op == deser) {

        assert ((op == size) ? pb == 0 : pb != 0);
       
        if (!internalSerializeBigMsf(op, pb, pcb, pmsf, pec))
            return false;
        
        return true;
    }

    assert(op == ser);
    
    return pmsf->FSerializeMsf(pb, pcb, this, pec);
}


extern "C" {

// open MSF; return MSF* or NULL if error
MSF* __cdecl MSFOpenW(const wchar_t *wszFilename, BOOL fWrite, MSF_EC *pec) {
    return MSF::Open(wszFilename, fWrite, pec, ::cbPgDef);
}

// open MSF; return MSF* or NULL if error
MSF* __cdecl MSFOpenExW(const wchar_t *wszFilename, BOOL fWrite, MSF_EC *pec, CB cbPage) {
    return MSF::Open(wszFilename, fWrite, pec, cbPage);
}

} // extern "C"
