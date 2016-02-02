#include "pdbimpl.h"
#include "dbiimpl.h"

#ifndef PDB_MT
BOOL PDB1::OpenStreamEx(SZ_CONST sz, SZ_CONST szMode, OUT Stream** ppstream) {
    // MTS NOTE: NMTNI is MTS, no need to protect
	NI ni;
  	return nmt.addNiForSz(sz, &ni) && !!(*ppstream = new Strm(this, (SN)ni));
}

#else


BOOL PDB1::OpenStreamEx(SZ_CONST sz, SZ_CONST szMode, OUT Stream** ppstream) {
    PDBLOG_FUNC();
    PDBLOG_FUNCARG(sz);
    PDBLOG_FUNCARG(szMode);

    // MTS NOTE: NMTNI is MTS, no need to protect
    BOOL fExclusive = strchr(szMode, 'x') != 0;
    NI ni;
    if (!nmt.addNiForSz(sz, &ni)) {
        return FALSE;
    }
    
    MTS_PROTECT_T(Mutex, m_mutexStreamOpenClose);

    Strm * pstrm;
    if (m_mpOpenedStream.map(ni, &pstrm)) {
        while (pstrm->GetRefcount() != 0) {
            if (!fExclusive && !pstrm->fExclusive()) {
                break;
            }
            if (!pstrm->ReleaseAndAcquire( m_mutexStreamOpenClose )) {
                setLastError(EC_ACCESS_DENIED);
                return FALSE;
            }
        }
        assert(pstrm->GetRefcount() == 0 || (!pstrm->fExclusive() && !fExclusive));
    } else {
        pstrm = new (this) Strm(this, (SN)ni, m_mutexStreamOpenClose);
        if (pstrm == NULL || !m_mpOpenedStream.add(ni, pstrm)) {
            setOOMError();
            return FALSE;
        }
    }

    *ppstream = pstrm;
        
    pstrm->IncRefcount();
    pstrm->fExclusive() = fExclusive;

    return TRUE;
}
#endif

BOOL PDB1::GetEnumStreamNameMap(OUT Enum** ppenum) {
    // MTS NOTE: NMTNI is MTS, no need to protect
    // MTS NOTE: EnumNMTNI can't be share with multiple thread!
	return !!(*ppenum = (Enum *)new EnumNMTNI(nmt));
}

// Return the next free SN as an NI.  Called by PDB1::nmt.niForSz() when first
// establishing a name index for a new stream name.
BOOL PDB1::niForNextFreeSn(void* pv, OUT NI* pni) {
    PDB1* ppdb1 = (PDB1*)pv;
	SN sn = ppdb1->pmsf->GetFreeSn();
	if (sn != snNil) {
		*pni = (NI)sn;
#ifdef PDB_MT
        // MTS NOTE: the MSF is MTS and it automatically replace the stream
        return TRUE;
#else
        // Alas, necessary to create an empty stream to "claim it", otherwise
		// the next call to MSFGetFreeSn() will return same sn again.
		return ppdb1->pmsf->ReplaceStream(sn, 0, 0);
#endif
	}
    return FALSE;
}

extern const char szStreamNameMap[] = "/names";

