// Name table interface/implementation

#ifndef __NMTNI_INCLUDED__
#define __NMTNI_INCLUDED__

#ifndef __ARRAY_INCLUDED__
#include "array.h"
#endif
#ifndef __MAP_INCLUDED__
#include "map.h"
#endif
#ifndef __BUFFER_INCLUDED__
#include "buffer.h"
#endif
#ifndef __MISC_INCLUDED__
#include "misc.h"
#endif

#include "util_misc.h"

#ifdef PDB_MT
#include "mts.h"
#endif

class NMTNI {               // name table with user-defined NIs
public:

    struct SZO {            // string offset: relative to start of buffer 
        OFF off;            

        SZ sz(const Buffer *pbuf) const {
            assert(isValid(pbuf));
            return getsz(pbuf);
        }
        BOOL Equals(const SZO &szo, const Buffer *pbuf) const {
            return off == szo.off || strcmp(sz(pbuf), szo.sz(pbuf)) == 0;
        }
        BOOL operator ==(const SZO& szo) const {
            return off == szo.off;
        }
        HASH Hash(const Buffer *pbuf) const {
            return hashSz(sz(pbuf));
        }
        BOOL isValid(const Buffer *pbuf) const {
            if (off < 0) {
                // PDB must be corrupted.
                return FALSE;
            }
            return off < pbuf->Size() && fValidateSz(getsz(pbuf), (SZ)pbuf->End());
        }
    private:
        SZ getsz(const Buffer *pbuf) const {
            return (SZ)pbuf->Start() + off;
        }
    };

    typedef HashClassWithContext<SZO,Buffer> HcSzo;

public:
    // create a name table with default name index generation (0, 1, 2, ...)
    NMTNI() 
    {
        pfnNi = niNext;
        pfnNiArg = this;
        niMac = niNil + 1;
        mapSzoNi.setContext(&buf);
    }
    // create a name table with client defined name index generation
    NMTNI(BOOL (*pfnNi_)(void*, OUT NI*), void* pfnNiArg_ = 0)
    {
        pfnNi = pfnNi_;
        pfnNiArg = pfnNiArg_;
        niMac = 0; // will not be used
        mapSzoNi.setContext(&buf);
    }
    // append a serialization of this NMTNI to the buffer
    BOOL save(Buffer* pbuf) {
        MTS_PROTECT(m_csReadWrite);

        // optimization: since mapNiSzo is just the reverse map of mapSzoNi,
        // we store only the latter
        if (!buf.save(pbuf))
            return FALSE;
        traceOnly(CB cb0 = pbuf->Size());

        if (!mapSzoNi.save(pbuf))
            return FALSE;
        traceOnly(CB cbMap = pbuf->Size() - cb0);

        if (!pbuf->Append((PB)&niMac, sizeof niMac))
            return FALSE;

        trace((trSave, "NMTNI::save() cbBuf=%d cbMap=%d\n", buf.Size(), cbMap));
        return TRUE;
    }
#if 0
    // reload a serialization of this empty NMTNI from the buffer; leave
    // *ppb pointing just past the NMTNI representation
    BOOL reload(PB* ppb) {
        MTS_PROTECT(m_csReadWrite);

        if (!buf.reload(ppb))
            return FALSE;

        if (!mapSzoNi.reload(ppb))
            return FALSE;

        niMac = *(NI UNALIGNED *)*ppb;
        *ppb += sizeof(niMac);

        // recover mapNiSzo from mapSzoNi
        mapNiSzo.reset();
        EnumMap<SZO,NI,HcSzo,Buffer> enumMap(mapSzoNi);
        while (enumMap.next()) {
            SZO szo;
            NI ni;
            enumMap.get(&szo, &ni);
            if (!mapNiSzo.add(ni, szo))
                return FALSE;
        }

        return TRUE;
    }
#endif
    // reload a serialization of this empty NMTNI from the buffer; leave
    // *ppb pointing just past the NMTNI representation
    BOOL reload(PB* ppb, CB cbReloadBuf) {
        MTS_PROTECT(m_csReadWrite);

        PB  pbEnd = *ppb + cbReloadBuf;

        if (!buf.reload(ppb, cbReloadBuf)) {
            return FALSE;
        }
        cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

        if (!mapSzoNi.reload(ppb, cbReloadBuf)) {
            return FALSE;
        }
        cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

        if (cbReloadBuf < sizeof(niMac)) {
            return FALSE;
        }

        niMac = *(NI UNALIGNED *)*ppb;
        *ppb += sizeof(niMac);

        // recover mapNiSzo from mapSzoNi
        mapNiSzo.reset();
        EnumMap<SZO,NI,HcSzo,Buffer> enumMap(mapSzoNi);
        while (enumMap.next()) {
            SZO szo;
            NI ni;
            enumMap.get(&szo, &ni);

            // make sure that the SZO is valid
            if (!szo.isValid(&buf)) {
                return FALSE;
            }

            if (!mapNiSzo.add(ni, szo)) {
                return FALSE;
            }
        }

        return TRUE;
    }
    // return a name index for this name
    BOOL addNiForSz(SZ_CONST sz, OUT NI *pni) {
        MTS_PROTECT(m_csReadWrite);

        precondition(pni);

        // speculatively add the argument name to the name buffer
        SZO szo;
        if (!addSzo(sz, &szo))
            return FALSE;
        else if (mapSzoNi.map(szo, pni)) {
            // name already in table, remove name we just added to buffer
            verify(retractSzo(szo));
            return TRUE;
        }
        else if (pfnNi(pfnNiArg, pni) &&
                 mapSzoNi.add(szo, *pni) &&
                 mapNiSzo.add(*pni, szo))
        {
            // successfully added the name and its new name index
            return TRUE;
        }
        else {
            // failed hard; we'd better not commit these changes!
            verify(retractSzo(szo));
            return FALSE;
        }
    }

    // return the name corresponding to ni, valid until next NMTNI call
    
#ifndef PDB_MT
    // MTS note: this cannot be MTS since the result can be invalidate by another thread
    // no one uses it anyway, so just disable it
    BOOL szForNi(NI ni, _Out_opt_ OUT SZ *psz) {
        MTS_PROTECT(m_csReadWrite);

        precondition(ni != niNil);
        precondition(psz);

        SZO szo;
        if (mapNiSzo.map(ni, &szo)) {
            *psz = szo.sz(&buf);
            return TRUE;
        }
        else
            return FALSE;
    }
#endif
    void reset() {
        MTS_PROTECT(m_csReadWrite);

        niMac = niNil + 1;
        mapSzoNi.reset();
        mapNiSzo.reset();
        buf.Clear();
    }

    BOOL deleteNiSzo(NI ni) {
        MTS_PROTECT(m_csReadWrite);

        precondition(ni != 0);

        SZO szo;

        if (!mapNiSzo.map(ni, &szo)) {
            return FALSE;
        }

        if (!mapSzoNi.remove(szo)) {
            return FALSE;
        }

        if (!mapNiSzo.remove(ni)) {
            return FALSE;
        }

        return TRUE;
    }

private:
    Map<SZO,NI,HcSzo,Buffer>   mapSzoNi;   // map from szo to ni
    Map<NI,SZO,HcNi> mapNiSzo;   // map from ni to szo
    Buffer buf;             // store the names
    // REVIEW: this buffer should be a pool!
    BOOL (*pfnNi)(void*, OUT NI*);
    void* pfnNiArg;
    NI niMac;               // last NI allocated by niNext()

#ifdef PDB_MT
    mutable CriticalSection m_csReadWrite;
#endif

    // append the name to the names buffer
    BOOL addSzo(SZ_CONST sz, OUT SZO* pszo) {
        // only used by NMTNI::addNiForSz()
        MTS_ASSERT(m_csReadWrite);
        precondition(sz && strlen(sz) > 0);
        precondition(pszo);

        CB cb = CB(strlen(sz)) + 1;
        PB pb;
        if (buf.Append((PB)sz, cb, &pb)) {
            pszo->off = OFF(pb - buf.Start());  // REVIEW:WIN64 CAST
            return TRUE;
        }
        else
            return FALSE;
    }
    // remove the name recently appended to the names buffer
    BOOL retractSzo(const SZO& szo) {
        // only used by NMTNI::addNiForSz()
        MTS_ASSERT(m_csReadWrite);
        return buf.Truncate(szo.off);
    }
    // default NI generator: returns the sequence (1, 2, ...)
    static BOOL niNext(void* pv, OUT NI* pni) {
        NMTNI* pnmt = (NMTNI*)pv;
        precondition(pnmt);

        MTS_ASSERT(pnmt->m_csReadWrite);
        *pni = pnmt->niMac++;

        return TRUE;
    }

    friend class EnumNMTNI;
};

class EnumNMTNI : public EnumNameMap {
public:
    EnumNMTNI(const NMTNI& nmt)
        : enumMap(nmt.mapSzoNi), pnmt(&nmt)
    {
    }
    void release() {
        delete this;
    }
    void reset() {
        MTS_PROTECT(pnmt->m_csReadWrite);
        enumMap.reset();
    }
    BOOL next() {
        MTS_PROTECT(pnmt->m_csReadWrite);
        return enumMap.next();
    }
    void get(OUT SZ_CONST* psz, OUT NI* pni) {
        MTS_PROTECT(pnmt->m_csReadWrite);

        NMTNI::SZO szo;
        enumMap.get(&szo, pni);
#ifdef PDB_MT
        SZ_CONST sz = szo.sz(&pnmt->buf);
        buf.Reset();
        buf.Append((PB)sz, (CB)strlen(sz) + 1, (PB *)psz);
#else
        *psz = szo.sz(&pnmt->buf);
#endif
    }
private:
    EnumMap<NMTNI::SZO,NI,NMTNI::HcSzo,Buffer> enumMap;
    const NMTNI* pnmt;
#ifdef PDB_MT
    Buffer buf;
#endif
};

#endif // !__NMTNI_INCLUDED__
