#pragma once

#include "map.h"
#include "szcanon.h"
#include <malloc.h>
#include "srccommon.h"

struct SHO : public SrcHeaderOut {
    SHO() {
        memset(this, 0, sizeof(SHO));
    }
 
    SHO(NameMap * pnamemap, const SrcHeaderW & srchdr) {
        CreateWithUnicodeNames( pnamemap, srchdr );
    }

private:
    void CreateWithUnicodeNames(NameMap *pnamemap, const SrcHeaderW& srchdr)
    {
        memset(this, 0, sizeof(SHO));
        
        cb = sizeof SrcHeaderOut;
        ver = srcverOne;
        sig = srchdr.sig;
        cbSource = srchdr.cbSource;
        srccompress = srchdr.srccompress;
        grFlags = srchdr.grFlags;

        const wchar_t *wsz = srchdr.szNames;
        pnamemap->getNiW(wsz, &niFile);
        wsz += wcslen(wsz) + 1;
        pnamemap->getNiW(wsz, &niObj);
        wsz += wcslen(wsz) + 1;

        wchar_t  wszT[_MAX_PATH * 2];
        assert(wcslen(wsz) < _countof(wszT));
        wcsncpy_s (wszT, _countof(wszT), wsz, _TRUNCATE);

        CCanonFile::SzCanonFilename(wszT);
        pnamemap->getNiW(wszT, &niVirt);

        assert(ptrdiff_t(srchdr.cb) >= (PB(wsz) + wcslen(wsz)*sizeof(*wsz) + 1) - PB(&srchdr));
    }

public:
    bool
    operator==(const SHO & sho) const {
        return memcmp(this, &sho, sizeof(SHO)) == 0;
    }


};

#ifdef PDB_MT
typedef Map<NI, SHO, HcNi, void, CriticalSection>      MapNiSrcHdr;
typedef EnumMap<NI, SHO, HcNi, void, CriticalSection>  EnumMapNiSrcHdr;
#else
typedef Map<NI, SHO, HcNi>      MapNiSrcHdr;
typedef EnumMap<NI, SHO, HcNi>  EnumMapNiSrcHdr;
#endif

class SrcImpl : public SrcCommon {

    friend class PDB1;

public:
    // implement the public Src interface

    virtual bool
    Close();

    virtual bool
    Remove(IN SZ_CONST szFile);

    virtual bool
    GetData(IN PCSrcHeaderOut pcsrcheader, OUT void * pvData) const;

    virtual bool
    GetEnum(OUT EnumSrc ** ppenum) const;

    virtual bool
    GetHeaderBlock(SrcHeaderBlock &shb) const;

    virtual bool RemoveW(__in_z wchar_t *wcsFile);
    virtual bool QueryByNameW(__in_z wchar_t *wcsFile, OUT PSrcHeaderOut psrcheaderOut) const;
    virtual bool AddW(PCSrcHeaderW psrcheader, const void * pvData);

    // private members/types
private:
    static const wchar_t    c_szSrcHeader[];
    static const wchar_t    c_szSrcFilesPrefix[12];

    enum {
        // NB: depdendence on values of c_szSrcFilesPrefix and the name
        // the compiler uses to tag the injected code!
        cchInjPrefix = sizeof("*:12345"),
        cchStreamNameMax = _MAX_PATH + _countof(c_szSrcFilesPrefix) + cchInjPrefix,
    };

    typedef SrcHeaderBlock  Hdr;

    // private methods
private:
#ifdef PDB_MT
    SrcImpl(PDB * ppdb, BOOL (*pfnClose)(PDB *));
#else
    SrcImpl(PDB * ppdb);
#endif
    ~SrcImpl();
    
    bool
    internalInit(bool fWrite);

    bool
    internalClose();

    static bool
    fBuildStreamName(WSZ_CONST szFile, _Out_opt_z_cap_(cchStreamNameMax) wchar_t szStrmName[cchStreamNameMax]);

    // private members
private:
    bool                    m_fInit;
    bool                    m_fWritten;
    PDB *                   m_ppdb;
    Stream *                m_pstrmHdr;     // also used as a flag for fWrite.
    NameMap *               m_pnamemap;
    MapNiSrcHdr             m_mpnisrchdr;
    Hdr                     m_hdr;
#ifdef PDB_MT
    mutable CriticalSection m_cs;
    BOOL                    (*m_pfnClose)(PDB *);
#endif
};


class EnumSrcImpl : public EnumSrc {

    friend SrcImpl;

public:
    virtual void release();
    virtual void reset();
    virtual BOOL next();
    virtual void get(OUT PCSrcHeaderOut * ppcsrcheader);

private:
    EnumSrcImpl(const MapNiSrcHdr &);

    EnumMapNiSrcHdr m_enum;
#ifdef PDB_MT
    // store one private copy for MT
    SHO m_sho;
#endif
};
