/***********************************************************************
* Microsoft (R) PDB Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

#define UNICODE
#define _UNICODE

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#define STRICT
#include "windows.h"

#include "pdb.h"
#include "cvr.h"
#include "mdalign.h"
#include "msf.h"
#include "pdbtypdefs.h"
#include "safestk.h"
#include "szst.h"

typedef USHORT IMOD;

struct XFIXUP
{
   WORD wType;
   WORD wExtra;
   DWORD rva;
   DWORD rvaTarget;
};

struct OMAP
{
    DWORD       rva;
    DWORD       rvaTo;
};

const wchar_t szUsage[] =
   L"%s: usage: %s <pdb> item\n"
   L"where:\n"
   L"\titem: dbi <exe> <dbi-items> | hdr | msf | src | streams [stream-list]\n"
   L"\t\t| strnames | tpihdr | ipihdr | [ids | types] [type-list | last] | namemap\n"
   L"\n"
   L"\tdbi-items: check | files | fixup | fpo | hdr | headers\n"
   L"\t\t| globals [sym-list] | lines [mod-list] | mods | omapt | omapf\n"
   L"\t\t| pdata | publics [sym-list] | seccontribs | secmap\n"
   L"\t\t| syms [mod-list] | symtypes | typeservers | linkinfo\n";

#define abstract

abstract class Idx;
    abstract class IdxVal;
        class IdxSingle;
        class IdxRange;
            class IdxAll;
        class IdxSeq;

typedef unsigned long   ulong;
typedef unsigned short  ushort;

MSF *pmsf;
PDB *ppdb;
TPI *ptpi;
TPI *pipi;
DBI *pdbi;

wchar_t *szPDBDump;

void    usage();
void    fatal(const wchar_t *, ...);
void    warning(const wchar_t *, ...);
void    dumpHeader(TPI *ptpi);
void    dumpPdbHdr(PDB *ppdb, const wchar_t *szPdb);
void    dumpDbiHdr(DBI *pdbi, const wchar_t *szPdb);
void    dumpMSF(MSF *pmsf, const wchar_t *szMSF);
void    dumpMSFByName(MSF *pmsf, PDB *ppdb, const wchar_t *szMSF);
void    dumpStreamsSz(MSF *pmsf, _Inout_ wchar_t *sz);
bool    dumpFileInfo(DBI *pdbi);
bool    dumpGlobals(DBI *pdbi);
void    dumpSymsSz(DBI *pdbi, const wchar_t *sz);
void    dumpLinesSz(DBI *pdbi, const wchar_t *sz);
bool    dumpPublics(DBI *pdbi);
bool    checkAddrsToPublics(DBI *pdbi);
bool    dumpSymbols(Mod *pmod);
void    dumpSymbol(SYMTYPE *);
void    dumpTpiHdr(MSF *pmsf, TPI *ptpi, const wchar_t *sz, bool fId);
void    dumpTpiSz(TPI *ptpi, _Inout_ wchar_t *sz, bool fId);
void    dumpTpiLastSz(TPI *ptpi, bool fId);
void    dumpTpiTi(void *, int, bool);

void    dumpSymTypesSz(DBI *pdbi, const wchar_t *sz);
bool    dumpSymbolTypes(Mod *pmod);
bool    dumpFpo(DBI *pdbi);
bool    dumpPdata(DBI *pdbi);
bool    dumpXFixup(DBI *pdbi);
bool    dumpOmap(DBI *pdbi, bool fFromSrc);
bool    dumpSectionHdr(DBI *pdbi);
bool    dumpSrcHeader(PDB *ppdb);
bool    dumpLinkInfo(DBI *pdbi);
void    dumpText(size_t, BYTE *);
bool    dumpNamemap(PDB *ppdb, bool fUseDecimal = false);

//bool    dumpSrcFiles(Src *psrc, SZ szFile);

size_t CbExtractNumeric(BYTE *, ulong *);

enum Bind { bindPtr, bindArray, bindProc, bindNone };

class SST {
    char *st;
public:
    SST(_In_z_ char *st_) : st(st_) { }
    operator char *() { return st; }
};


class Str {
private:
    wchar_t *sz;
    size_t cch;

    Str(_In_z_ wchar_t *szT, size_t cchT) : sz(szT), cch(cchT) { }

    void copy(const wchar_t *szT, size_t cchT) {
        cch = cchT;
        sz = new wchar_t[cch + 1];

        memcpy(sz, szT, (cch + 1) * sizeof(wchar_t));
    }

    void copy(const char *szT, size_t cchT) {
        cch = cchT;
        sz = new wchar_t[cch + 1];

        MultiByteToWideChar(CP_UTF8, 0, szT, int(cchT + 1), sz, int(cch + 1));
    }

public:
    Str(SST st) { copy(st + 1, *(BYTE *) (char *) st); }

    Str(const wchar_t *szT = L"") { copy(szT, wcslen(szT)); }

    Str(const char *szT) { copy(szT, strlen(szT)); }

    Str(const Str& s) { copy(s.sz, s.cch); }

    enum Fmt { dec, hex };

    Str(int i, Fmt fmt = dec) {
        wchar_t buf[20];

        swprintf_s(buf, _countof(buf), (fmt == dec) ? L"%d" : L"%X", i);

        copy(buf, wcslen(buf));
    }

    Str(unsigned u, Fmt fmt = dec) {
        wchar_t buf[20];

        swprintf_s(buf, _countof(buf), (fmt == dec) ? L"%u" : L"%X", u);

        copy(buf, wcslen(buf));
    }

    Str(ulong ul, Fmt fmt = dec) {
        wchar_t buf[20];

        swprintf_s(buf, _countof(buf), (fmt == dec) ? L"%u" : L"%X", (int)ul);

        copy(buf, wcslen(buf));
    }

    Str& operator=(const Str& s) {
        if (this != &s)
             delete [] sz, copy(s.sz, s.cch);

        return *this;
    }

    ~Str() { delete [] sz; }

    Str& operator+=(const Str& s) {
        size_t cchNew = cch + s.cch;
        wchar_t *szNew = new wchar_t[cchNew + 1];

        memcpy(szNew, sz, cch * sizeof(wchar_t));
        memcpy(szNew + cch, s.sz, s.cch * sizeof(wchar_t));

        szNew[cchNew] = L'\0';

        delete [] sz;
        sz = szNew;

        cch = cchNew;

        return *this;
    }

    friend Str operator+(const Str& s1, const Str& s2) {
        size_t cchNew = s1.cch + s2.cch;
        wchar_t *szNew = new wchar_t[cchNew + 1];

        memcpy(szNew, s1.sz, s1.cch * sizeof(wchar_t));
        memcpy(szNew + s1.cch, s2.sz, s2.cch * sizeof(wchar_t));

        szNew[cchNew] = L'\0';

        return Str(szNew, cchNew);
    }

    operator const wchar_t *() { return sz; }

    bool isEmpty() { return cch == 0; }

    Str sansExtraSpaces() {
        Str t = *this;
        wchar_t *pD = t.sz;
        const wchar_t *pS = t.sz;

        while ((*pD = *pS++) != 0) {
            pD += !((*pD == L' ') && (*pS == L' '));
        }

        t.cch = size_t(pD - t.sz);

        return t;
    }

    static Str fmt(const wchar_t *fmt, ...) {
        wchar_t buf[512];

        va_list ap;
        va_start(ap, fmt);

        _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);

        va_end(ap);

        return Str(buf);
    }
};

typedef wchar_t *WSZ;


abstract class Idx
{
public:
    virtual Idx *eachDo(void (*)(void *, int, bool)) = 0;
    virtual Str asStr() = 0;
};


abstract class IdxValue : public Idx
{
protected:
    void *p;
    Str::Fmt fmt;
public:
    IdxValue(void *p_, Str::Fmt fmt_) : p(p_), fmt(fmt_) { }
};


class IdxSingle : public IdxValue
{
    int idx;
public:
    IdxSingle(void *p, Str::Fmt fmt, int idx_) : IdxValue(p, fmt), idx(idx_) { }

    virtual Idx *eachDo(void (*pfn)(void *, int, bool)) {
        (*pfn)(p, idx, false);
        return this;
    }

    virtual Str asStr() { return Str(idx, fmt); }
};


class IdxRange : public IdxValue
{
    int idxMin;
    int idxLim;

public:
    IdxRange(void *p, Str::Fmt fmt, int idxMin_, int idxLim_)
        : IdxValue(p, fmt), idxMin(idxMin_), idxLim(idxLim_) { }

    virtual Idx *eachDo(void (*pfn)(void *, int, bool)) {
        for (int idx = idxMin; idx <= idxLim; idx++)
            (*pfn)(p, idx, false);
        return this;
    }

    virtual Str asStr() {
        return Str(idxMin, fmt) + L"-" + Str(idxLim, fmt);
    }
};


class IdxAll : public IdxRange
{
public:
    IdxAll(void *p, Str::Fmt fmt, int idxMin, int idxMac)
        : IdxRange(p, fmt, idxMin, idxMac - 1) { }

    virtual Str asStr() { return L"all"; }
};


class IdxSeq : public Idx
{
    Idx *pFirst;
    Idx *pRest;
public:
    IdxSeq(Idx *pFirstT, Idx *pRestT) : pFirst(pFirstT), pRest(pRestT) { }

    ~IdxSeq() {
        delete pFirst;
        delete pRest;
    }

    virtual Idx *eachDo(void (*pfn)(void *, int, bool)) {
        pFirst->eachDo(pfn);
        pRest->eachDo(pfn);
        return this;
    }

    virtual Str asStr() {
        return pFirst->asStr() + L"," + pRest->asStr();
    }
};


int idxForSz(const wchar_t *sz, Str::Fmt fmt)
{
    int i;

    swscanf_s(sz, (fmt == Str::dec) ? L"%d" : L"%X", &i);

    return i;
}


Idx *pIdxForSz(void *p, _Inout_ wchar_t *sz, Str::Fmt fmt, int idxMin, int idxMax)
{
    wchar_t *pch;

    if (!!(pch = wcschr(sz, L','))) {
        *pch++ = 0;

        return new IdxSeq(pIdxForSz(p, sz, fmt, idxMin, idxMax),
                          pIdxForSz(p, pch, fmt, idxMin, idxMax));
    }

    if (!!(pch = wcschr(sz, L'-'))) {
        *pch++ = 0;

        return new IdxRange(p, fmt, idxForSz(sz, fmt), idxForSz(pch, fmt));
    }

    if (wcscmp(sz, L"all") == 0) {
        return new IdxAll(p, fmt, idxMin, idxMax);
    }

    return new IdxSingle(p, fmt, idxForSz(sz, fmt));
}


enum {
    asRecords,
    asC
} Style;


int __cdecl wmain(int iargMax, _In_ wchar_t *rgarg[])
{
#ifdef  _M_IX86
    // Enable access to the System32 directory without remapping to SysWow64

    HMODULE hmodKernel32 = GetModuleHandle(L"KERNEL32.DLL");

    BOOLEAN (WINAPI *pfnWow64EnableWow64FsRedirection)(BOOLEAN);

    *(FARPROC *) &pfnWow64EnableWow64FsRedirection = GetProcAddress(hmodKernel32, "Wow64EnableWow64FsRedirection");

    if (pfnWow64EnableWow64FsRedirection != 0) {
       (*pfnWow64EnableWow64FsRedirection)(FALSE);
    }
#endif  // _M_IX86

    szPDBDump = rgarg[0];

    if (iargMax < 3) {
        usage();
    }

    wchar_t *szPDBName = rgarg[1];
    EC ec;
    int iarg;

    wchar_t szError[cbErrMax];

    bool fCanHandleDbgData = !PDB::ExportValidateInterface(PDBIntv50a);

    wchar_t szPdbPath[PDB_MAX_PATH];

    if (!PDB::Open2W(szPDBName, pdbRead, &ec, szError, cbErrMax, &ppdb)) {
        if (!PDB::OpenValidate5(szPDBName, NULL, NULL, 0, &ec, szError, cbErrMax, &ppdb)) {
            fatal(L"cannot open %s\n", szPDBName);
        }

        szPDBName = ppdb->QueryPDBNameExW(szPdbPath, PDB_MAX_PATH);
    }

    ppdb->OpenTpi(pdbRead, &ptpi);
    ppdb->OpenIpi(pdbRead, &pipi);

    MSF_EC msfEc;

    if (!(pmsf = MSFOpenW(szPDBName, FALSE, &msfEc))) {
        if (msfEc == MSF_EC_FORMAT) {
            // can only dump types

            warning(L"Previous version of PDB: can only dump types.\n");

            for (int iarg = 2; iarg < iargMax; ++iarg) {
                if (wcscmp(rgarg[iarg], L"types") == 0) {
                    if (iarg == iargMax - 1) {
                        wchar_t dumpConfig[] = L"all";
                        dumpTpiSz(ptpi, dumpConfig, false);
                    }

                    else {
                        while (++iarg < iargMax) {
                            dumpTpiSz(ptpi, rgarg[iarg], false);
                        }
                    }
                }

                else {
                    fatal(L"Previous version of PDB: can only dump types.\n");
                }
            }

            ptpi->Close();
            ppdb->Close();

            return 0;
        }

        else {
            fatal(L"cannot open MSF %s (%d)\n", szPDBName, msfEc);
        }
    }

    for (iarg = 2; iarg < iargMax; ++iarg) {
        bool fNotSupported = false;

        if (wcscmp(rgarg[iarg], L"msf") == 0) {
            dumpMSF(pmsf, szPDBName);
        } else if (wcscmp(rgarg[iarg], L"hdr") == 0) {
            if (pdbi) {
                dumpDbiHdr(pdbi, szPDBName);
            }
            else {
                dumpPdbHdr(ppdb, szPDBName);
            }
        } else if (wcscmp(rgarg[iarg], L"strnames") == 0) {
            dumpMSFByName(pmsf, ppdb, szPDBName);
        } else if (wcscmp(rgarg[iarg], L"streams") == 0) {
            if (iarg == iargMax - 1) {
                wchar_t dumpConfig[] = L"all";
                dumpStreamsSz(pmsf, dumpConfig);
            }

            else {
                while (++iarg < iargMax) {
                    dumpStreamsSz(pmsf, rgarg[iarg]);
                }
            }
        } else if (wcscmp(rgarg[iarg], L"namemap") == 0) {
                dumpNamemap(ppdb);
        } else if (wcscmp(rgarg[iarg], L"namemapd") == 0) {
                dumpNamemap(ppdb, true);
        } else if (wcscmp(rgarg[iarg], L"namemapx") == 0) {
                dumpNamemap(ppdb);
        } else if (wcscmp(rgarg[iarg], L"tpihdr") == 0) {
                dumpTpiHdr(pmsf, ptpi, szPDBName, false);
        } else if (wcscmp(rgarg[iarg], L"ipihdr") == 0) {
                dumpTpiHdr(pmsf, pipi, szPDBName, true);
        } else if ((wcscmp(rgarg[iarg], L"types") == 0) ||
                   (wcscmp(rgarg[iarg], L"ids") == 0)) {
            bool fId = wcscmp(rgarg[iarg], L"ids") == 0;
            TPI* p = fId ? pipi : ptpi;

            if (p == NULL) {
                continue;
            }

            if (iarg == iargMax - 1) {
                wchar_t dumpConfig[] = L"all";
                dumpTpiSz(p, dumpConfig, fId);
            }

            else if (wcscmp(rgarg[iarg +1], L"last") == 0) {
                iarg++;
                dumpTpiLastSz(p, fId);
            }

            else {
                while (++iarg < iargMax) {
                    dumpTpiSz(p, rgarg[iarg], fId);
                }
            }
        } else if (wcscmp(rgarg[iarg], L"src") == 0) {
            dumpSrcHeader(ppdb);
        } else if (wcscmp(rgarg[iarg], L"dbi") == 0 && iarg + 1 < iargMax) {
            // We skip the next arg since OpenDBI doesn't use it

            ++iarg;

            if (!ppdb->OpenDBI(NULL, pdbRead, &pdbi)) {
                fatal(L"cannot OpenDbi\n");
            }
        } else if (wcscmp(rgarg[iarg], L"records") == 0) {
            Style = asRecords;
        } else if (wcscmp(rgarg[iarg], L"C") == 0) {
            Style = asC;
        } else if (wcscmp(rgarg[iarg], L"globals") == 0 && pdbi) {
            dumpGlobals(pdbi);
        } else if (wcscmp(rgarg[iarg], L"mods") == 0 && pdbi) {
            pdbi->DumpMods();
        } else if (wcscmp(rgarg[iarg], L"seccontribs") == 0 && pdbi) {
            pdbi->DumpSecContribs();
        } else if (wcscmp(rgarg[iarg], L"typeservers") == 0 && pdbi) {
            pdbi->DumpTypeServers();
        } else if (wcscmp(rgarg[iarg], L"check") == 0 && pdbi) {
            checkAddrsToPublics(pdbi);
        } else if (wcscmp(rgarg[iarg], L"publics") == 0 && pdbi) {
            dumpPublics(pdbi);
        } else if (wcscmp(rgarg[iarg], L"secmap") == 0 && pdbi) {
            pdbi->DumpSecMap();
        } else if (wcscmp(rgarg[iarg], L"files") == 0 && pdbi) {
            dumpFileInfo(pdbi);
        } else if (wcscmp(rgarg[iarg], L"fpo") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpFpo(pdbi);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"pdata") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpPdata(pdbi);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"fixup") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpXFixup(pdbi);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"omapt") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpOmap(pdbi, false);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"omapf") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpOmap(pdbi, true);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"headers") == 0 && pdbi) {
            if (fCanHandleDbgData)
                dumpSectionHdr(pdbi);
            else
                fNotSupported = true;
        } else if (wcscmp(rgarg[iarg], L"syms") == 0 && pdbi) {
            if (iarg == iargMax - 1)
                dumpSymsSz(pdbi, L"all");
            else while (++iarg < iargMax)
                dumpSymsSz(pdbi, rgarg[iarg]);
        } else if (wcscmp(rgarg[iarg], L"lines") == 0 && pdbi) {
            if (iarg == iargMax - 1)
                dumpLinesSz(pdbi, L"all");
            else while (++iarg < iargMax)
                dumpLinesSz(pdbi, rgarg[iarg]);
        } else if (wcscmp(rgarg[iarg], L"symtypes") == 0 && pdbi) {
            if (iarg == iargMax - 1)
                dumpSymTypesSz(pdbi, L"all");
            else while (++iarg < iargMax)
                dumpSymTypesSz(pdbi, rgarg[iarg]);
        } else if (wcscmp(rgarg[iarg], L"linkinfo") == 0) {
            dumpLinkInfo(pdbi);
        } else
            usage();

        if (fNotSupported)
            warning(L"%s is not supported by the mspdb dll: ignored\n", rgarg[iarg]);
    }

    if (pdbi) {
        pdbi->Close();
    }

    if (ptpi) {
        ptpi->Close();
    }

    ppdb->Close();
    pmsf->Close();

    return 0;
}


void usage()
{
    fwprintf(stderr, szUsage, szPDBDump, szPDBDump, szPDBDump);
    exit(1);
}


void fatal(const wchar_t *szFmt, ...)
{
    fwprintf(stderr, L"%s: fatal error: ", szPDBDump);

    va_list ap;
    va_start(ap, szFmt);

    vfwprintf(stderr, szFmt, ap);

    va_end(ap);

    exit(1);
}


void warning(const wchar_t *szFmt, ...)
{
    fwprintf(stderr, L"%s: warning: ", szPDBDump);

    va_list ap;
    va_start(ap, szFmt);

    vfwprintf(stderr, szFmt, ap);

    va_end(ap);
}


void dumpMSF(MSF *pmsf, const wchar_t *szMSF)
{
    CB cbTotal = 0;
    CB cbAlloc = 0;
    int csn = 0;
    CB cbPg = pmsf->GetCbPage();

    for (SN sn = 0; sn != snNil; sn++) {
        CB cb = pmsf->GetCbStream(sn);

        if (cb != cbNil) {
            wprintf(L"%4d:%6ld%s", (int)sn, cb, (++csn > 1 && csn % 4 == 0) ? L"\n" : L"    ");

            cbTotal += cb;
            cb += cbPg - 1; cb -= cb % cbPg;
            cbAlloc += cb;
        }
    }

    struct _stat statbuf;
    _wstat(szMSF, &statbuf);

    wprintf(L"\ntotal: %d streams, %ld bytes used, %ld committed, %ld total\n", csn, cbTotal, cbAlloc, statbuf.st_size);
}


const wchar_t *GetStreamName(SN sn)
{
    // Get name of stream for stream number

    static Str *rgNames = NULL;

    if (rgNames == NULL) {
        // Initialize map of stream number to stream name on first call
        // determine max stream number

        SN snMax;

        for (snMax = snNil - 1; snMax > 0; snMax--) {
            if (pmsf->GetCbStream(snMax) != cbNil) {
                break;
            }
        }

        // Map stream numbers to names

        rgNames = new Str[snMax+1];
        EnumNameMap *penum;

        if (!ppdb->GetEnumStreamNameMap((Enum **) &penum)) {
            fatal(L"can't GetEnumStreamNameMap\n");
        }

        while (penum->next()) {
            const char *sz;
            NI ni;

            penum->get(&sz, &ni);

            if (sz != 0 && ni <= snMax) {
                rgNames[ni] = sz;
            }
        }

        penum->release();
    }

    const wchar_t *sz;

    if (rgNames[sn].isEmpty()) {
        switch (sn) {
            case 1:  sz = L"<Pdb>"; break;
            case 2:  sz = L"<Tpi>"; break;
            case 3:  sz = L"<Dbi>"; break;
            case 4:  sz = pipi ? L"<Ipi>" : L"<unnamed>"; break;
            default: sz = L"<unnamed>"; break;
        }
    } else {
        sz = rgNames[sn];
    }

    return sz;
}


void dumpPdbHdr(PDB *ppdb, const wchar_t *szPdb)
{
    // dump out some of the interesting info about the pdb (age, sig, impv, intv)

    wprintf(L"Header for MSPDB.dll = {\n");
    wprintf(L"\tintv  =   %ld\n", ppdb->QueryInterfaceVersion());
    wprintf(L"\timpv  =   %ld\n", ppdb->QueryImplementationVersion());
    wprintf(L"\t}\n");
    wprintf(L"Header for '%s' = {\n", szPdb);
    wprintf(L"\timpv  =   %ld\n", ppdb->QueryPdbImplementationVersion());
    wprintf(L"\tage   = 0x%lx (%ld)\n", ppdb->QueryAge(), ppdb->QueryAge());
    wprintf(L"\tsig   = 0x%lx\n", ppdb->QuerySignature());

    SIG70 sig70;
    if (ppdb->QuerySignature2(&sig70)) {
        wchar_t szSig70[64];

        StringFromGUID2(sig70, szSig70, 64);

        wprintf(L"\tsig70 = %s\n",  szSig70);
    }

    wprintf(L"\t}\n");
}


static const wchar_t *SzMachine(unsigned idMachine)
{
    const wchar_t *sz = L"Unknown";

    switch (idMachine) {
        case IMAGE_FILE_MACHINE_ARM       : sz = L"ARM (CE)"; break;
        case IMAGE_FILE_MACHINE_ARMNT     : sz = L"ARM"; break;
        case IMAGE_FILE_MACHINE_CEF       : sz = L"CEF"; break;
        case IMAGE_FILE_MACHINE_I386      : sz = L"i386"; break;
        case IMAGE_FILE_MACHINE_IA64      : sz = L"IA64"; break;
        case IMAGE_FILE_MACHINE_MIPS16    : sz = L"MIPS16"; break;
        case IMAGE_FILE_MACHINE_MIPSFPU   : sz = L"MIPS (FPU)"; break;
        case IMAGE_FILE_MACHINE_MIPSFPU16 : sz = L"MIPS16 (FPU)"; break;
        case IMAGE_FILE_MACHINE_POWERPC   : sz = L"PPC"; break;
        case IMAGE_FILE_MACHINE_R3000     : sz = L"R3000"; break;
        case IMAGE_FILE_MACHINE_R4000     : sz = L"R4000"; break;
        case IMAGE_FILE_MACHINE_R10000    : sz = L"R10000"; break;
        case IMAGE_FILE_MACHINE_SH3       : sz = L"SH3"; break;
        case IMAGE_FILE_MACHINE_SH4       : sz = L"SH4"; break;
        case IMAGE_FILE_MACHINE_SH5       : sz = L"SH5"; break;
        case IMAGE_FILE_MACHINE_THUMB     : sz = L"THUMB"; break;
        case IMAGE_FILE_MACHINE_TRICORE   : sz = L"TRICORE"; break;
        case IMAGE_FILE_MACHINE_AMD64     : sz = L"x64"; break;
        case IMAGE_FILE_MACHINE_AM33      : sz = L"AM33"; break;
        case IMAGE_FILE_MACHINE_CEE       : sz = L"COM+ EE"; break;
    }

    return sz;
}


const SN    snPDB       = 1;
const SN    snTpi       = 2;
const SN    snDbi       = 3;
const SN    snIpi       = 4;

struct NewDBIHdr {
    enum {
        hdrSignature = -1,
    };

    ULONG       verSignature;
    ULONG       verHdr;
    AGE         age;

    SN          snGSSyms;

    union {
        struct {
            USHORT      usVerPdbDllMin : 8; // minor version and
            USHORT      usVerPdbDllMaj : 7; // major version and
            USHORT      fNewVerFmt     : 1; // flag telling us we have rbld stored elsewhere (high bit of original major version)
        } vernew;                           // that built this pdb last.
        // the following structure was for a screwed up ordering of the bits we actually
        // wanted above.
        struct {
            USHORT      fNewVerFmt     : 1; // flag telling us we have rbld stored elsewhere
            USHORT      usVerPdbDllMaj : 7; // major version and
            USHORT      usVerPdbDllMin : 8; // minor version of the pdb dll
        } verreallyscrewedup;               // that built this pdb last.
        struct {
            USHORT      usVerPdbDllRBld: 4;
            USHORT      usVerPdbDllMin : 7;
            USHORT      usVerPdbDllMaj : 5;
        } verold;
        USHORT          usVerAll;
    };

    SN          snPSSyms;
    USHORT      usVerPdbDllBuild;   // build version of the pdb dll
                                    // that built this pdb last.
    SN          snSymRecs;
    USHORT      usVerPdbDllRBld;    // rbld version of the pdb dll
                                    // that built this pdb last.
    CB          cbGpModi;   // size of rgmodi substream
    CB          cbSC;       // size of Section Contribution substream
    CB          cbSecMap;
    CB          cbFileInfo;

    CB          cbTSMap;    // size of the Type Server Map substream
    ULONG       iMFC;       // index of MFC type server
    CB          cbDbgHdr;   // size of optional DbgHdr info appended to the end of the stream
    CB          cbECInfo;   // number of bytes in EC substream, or 0 if EC no EC enabled Mods
    struct _flags {
        USHORT  fIncLink:1;     // true if linked incrmentally (really just if ilink thunks are present)
        USHORT  fStripped:1;    // true if PDB::CopyTo stripped the private data out
        USHORT  fCTypes:1;      // true if linked with /debug:ctypes
        USHORT  unused:13;      // reserved, must be 0.
    } flags;
    USHORT      wMachine;   // machine type
    ULONG       rgulReserved[ 1 ];      // pad out to 64 bytes for future growth.
};


// Make sure our version bitfields and unions don't consume more than a USHORT.
cassert(offsetof(NewDBIHdr,snPSSyms) - offsetof(NewDBIHdr,usVerAll) == sizeof(USHORT));


void dumpDbiHdr(DBI *pdbi, const wchar_t *szPdbName)
{
    // dump out the dbi header
    wprintf(L"Dbi information = {\n");
    wprintf(L"\tintv = %ld\n", pdbi->QueryInterfaceVersion());
    wprintf(L"\timpv = %ld\n", pdbi->QueryImplementationVersion());
    wprintf(L"\t}\n");

    NewDBIHdr *phdr = (NewDBIHdr*) pdbi->QueryHeader();
    wprintf(L"Dbi Header for \"%s\" = {\n", szPdbName);
    wprintf(L"\tsig = 0x%X\n", phdr->verSignature);
    wprintf(L"\tver = %ld\n", phdr->verHdr);
    wprintf(L"\tage = 0x%X (%d)\n", phdr->age, phdr->age);
    wprintf(L"\tsnGSSyms = %ld\n", phdr->snGSSyms);
    wprintf(L"\tsnPSSyms = %ld\n", phdr->snPSSyms);
    wprintf(L"\tsnSymRecs = %ld\n", phdr->snSymRecs);
    wprintf(L"\tcbGpModi = %ld\n", phdr->cbGpModi);
    wprintf(L"\tcbSC = %ld\n", phdr->cbSC);
    wprintf(L"\tcbSecMap   = %ld\n", phdr->cbSecMap);
    wprintf(L"\tcbFileInfo = %ld\n", phdr->cbFileInfo);
    wprintf(L"\tcbTSMap = %ld\n", phdr->cbTSMap);
    wprintf(L"\tiMFC = %ld\n", phdr->iMFC);
    wprintf(L"\tcbDbgHdr = %ld\n", phdr->cbDbgHdr);
    wprintf(L"\tcbECInfo = %ld\n", phdr->cbECInfo);
    wprintf(L"\tflags.fIncLink = %s\n", phdr->flags.fIncLink ? L"true" : L"false");
    wprintf(L"\tflags.fStripped = %s\n", phdr->flags.fStripped ? L"true" : L"false");
    wprintf(L"\tflags.fCTypes = %s\n", phdr->flags.fCTypes ? L"true" : L"false");
    wprintf(L"\twMachine = 0x%04X ('%s')\n", phdr->wMachine, SzMachine(phdr->wMachine));

    if (phdr->verreallyscrewedup.fNewVerFmt &&
        phdr->verreallyscrewedup.usVerPdbDllMaj == 8 &&
        phdr->verreallyscrewedup.usVerPdbDllMin == 0)
    {
        // new screwed up style
        wprintf(L"\tBuilt with PDB (screwed) code version: %d.%02d.%d.%02d\n",
            phdr->verreallyscrewedup.usVerPdbDllMaj,
            phdr->verreallyscrewedup.usVerPdbDllMin,
            phdr->usVerPdbDllBuild,
            phdr->usVerPdbDllRBld);
    } else if (phdr->vernew.fNewVerFmt) {
        // new style
        wprintf(L"\tBuilt with PDB code version: %d.%02d.%d.%02d\n",
            phdr->vernew.usVerPdbDllMaj,
            phdr->vernew.usVerPdbDllMin,
            phdr->usVerPdbDllBuild,
            phdr->usVerPdbDllRBld);
    }
    else {
        // old style
        wprintf(L"\tBuilt with PDB (old) code version: %d.%02d.%d.%02d\n",
            phdr->verold.usVerPdbDllMaj,
            phdr->verold.usVerPdbDllMin,
            phdr->usVerPdbDllBuild,
            phdr->verold.usVerPdbDllRBld);
    }

    wprintf(L"\tRes[0] = 0x%08X\n", phdr->rgulReserved[0]);
    wprintf(L"\t}\n");

    if(phdr->cbDbgHdr) {
        unsigned short  rgsnDbgStreams[dbgtypeMax + 10];    // fudge factor
        memset(rgsnDbgStreams, -1, sizeof rgsnDbgStreams);

        unsigned off =
            sizeof NewDBIHdr +
            phdr->cbGpModi +
            phdr->cbSC +
            phdr->cbSecMap +
            phdr->cbFileInfo +
            phdr->cbTSMap +
            phdr->cbECInfo;

        const static wchar_t * const mpdbgtypesz[] = {
            L"dbgtypeFPO",
            L"dbgtypeException (deprecated)",
            L"dbgtypeFixup",
            L"dbgtypeOmapToSrc",
            L"dbgtypeOmapFromSrc",
            L"dbgtypeSectionHdr",
            L"dbgtypeTokenRidMap",
            L"dbgtypeXdata",
            L"dbgtypePdata",
            L"dbgtypeNewFPO",
            L"dbgtypeSectionHdrOrig",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
            L"<unknown dbgtype>",
        };


        CB  cbT = min(sizeof rgsnDbgStreams, phdr->cbDbgHdr);
        if (pmsf->ReadStream(snDbi, off, &rgsnDbgStreams, &cbT )) {
            CB cb = 0;
            unsigned isn = 0;

            wprintf(L"DbgStream = {\n");

            while (cb < phdr->cbDbgHdr) {
                wchar_t szT[16] = L"<unused>";
                SN sn = rgsnDbgStreams[isn];

                if (sn != 65535) {
                    swprintf_s(szT, _countof(szT), L"%u", sn);
                }

                wprintf(L"\tSN of %s (%d) = %s;\n", mpdbgtypesz[isn], isn, szT);
                cb += sizeof SN;
                isn++;
            }

            wprintf(L"\t};\n");
        }
    }
}

struct HDR_16t { // type database header, 16-bit types:
    IMPV    vers;           // version which created this TypeServer
    TI16    tiMin;          // lowest TI
    TI16    tiMac;          // highest TI + 1
    CB      cbGprec;        // count of bytes used by the gprec which follows.
    SN      snHash;         // stream to hold hash values
    // rest of file is "REC gprec[];"
};

const size_t    cbHdrMin = sizeof(HDR_16t);

struct OffCb {  // offset, cb pair
    OFF off;
    CB  cb;
};

struct TpiHash {
    SN      sn;             // main hash stream
    SN      snPad;          // auxilliary hash data if necessary
    CB      cbHashKey;      // size of hash key
    unsigned __int32
            cHashBuckets;   // how many buckets we have
    OffCb   offcbHashVals;  // offcb of hashvals
    OffCb   offcbTiOff;     // offcb of (TI,OFF) pairs
    OffCb   offcbHashAdj;   // offcb of hash head list, maps (hashval,ti),
                            //  where ti is the head of the hashval chain.
};

struct HDR_VC50Interim {    // used for converting the interim v5 version
    IMPV    vers;           // to the current version.
    TI      tiMin;
    TI      tiMac;
    CB      cbGprec;
    SN      snHash;
};

struct HDR { // type database header:
    IMPV    vers;           // version which created this TypeServer
    CB      cbHdr;          // size of the header, allows easier upgrading and
                            //  backwards compatibility
    TI      tiMin;          // lowest TI
    TI      tiMac;          // highest TI + 1
    CB      cbGprec;        // count of bytes used by the gprec which follows.
    TpiHash tpihash;        // hash stream schema

    // rest of file is "REC gprec[];"
};

enum TPIIMPV {
    impv40 = 19950410,
    impv41 = 19951122,
    impv50Interim = 19960307,
    impv50 = 19961031,
    impv70 = 19990903,
    impv80 = 20040203,
    curImpv = impv80,
};


void dumpTpiHdr(MSF *pmsf, TPI *ptpi, const wchar_t *szPdbName, bool fId)
{
    // dump out the tpi header
    wprintf(L"%s information = {\n", fId ? L"Ipi" : L"Tpi");
    wprintf(L"    intv = %ld\n", ptpi->QueryInterfaceVersion());
    wprintf(L"    impv = %ld\n", ptpi->QueryImplementationVersion());
    wprintf(L"}\n");

    // get the header

    HDR hdrTpi = { 0 };
    HDR_16t *phdr16 = reinterpret_cast<HDR_16t*>(&hdrTpi);
    HDR_VC50Interim *phdrvc50Int = reinterpret_cast<HDR_VC50Interim*>(&hdrTpi);

    CB  cbHdrT = sizeof hdrTpi;

    if (!pmsf->ReadStream(fId ? snIpi : snTpi, 0, &hdrTpi, &cbHdrT) && cbHdrT >= cbHdrMin) {
        wprintf(L"FAILED: ReadStream\n");
        return;
    }

    wprintf(L"%s Header = {\n", fId ? L"Ipi" : L"Tpi");
    wprintf(L"    vers      = %d;\n", hdrTpi.vers);

    if (hdrTpi.vers < impv50Interim) {
        wprintf(L"    tiMin(16) = %d (0x%04X);\n", phdr16->tiMin, phdr16->tiMin);
        wprintf(L"    tiMac(16) = %d (0x%04X);\n", phdr16->tiMac, phdr16->tiMac);
        wprintf(L"    cbGprec   = %d (0x%08X);\n", phdr16->cbGprec, phdr16->cbGprec);
        wprintf(L"    snHash    = %d;\n", phdr16->snHash);
    }
    else if (hdrTpi.vers == impv50Interim) {
        wprintf(L"    tiMin     = %d (0x%08X);\n", phdrvc50Int->tiMin, phdrvc50Int->tiMin);
        wprintf(L"    tiMac     = %d (0x%08X);\n", phdrvc50Int->tiMac, phdrvc50Int->tiMac);
        wprintf(L"    cbGprec   = %d (0x%08X);\n", phdrvc50Int->cbGprec, phdrvc50Int->cbGprec);
        wprintf(L"    snHash    = %d;\n", phdrvc50Int->snHash);
    }
    else {
        wprintf(L"    cbHdr     = %d;\n", hdrTpi.cbHdr);
        wprintf(L"    tiMin     = %d (0x%08X);\n", hdrTpi.tiMin, hdrTpi.tiMin);
        wprintf(L"    tiMac     = %d (0x%08X);\n", hdrTpi.tiMac, hdrTpi.tiMac);
        wprintf(L"    cbGprec   = %d (0x%08X);\n", hdrTpi.cbGprec, hdrTpi.cbGprec);
        wprintf(L"    Tpihash   = {\n");
        wprintf(L"        sn            = %d;\n", hdrTpi.tpihash.sn    );
        wprintf(L"        snPad         = %d;\n", hdrTpi.tpihash.snPad);
        wprintf(L"        cbHashKey     = %d;\n", hdrTpi.tpihash.cbHashKey);
        wprintf(L"        cHashBuckets  = %d (0x%08X);\n", hdrTpi.tpihash.cHashBuckets, hdrTpi.tpihash.cHashBuckets);
        wprintf(L"        offcbHashVals = {\n");
        wprintf(L"            off = 0x%08X;\n", hdrTpi.tpihash.offcbHashVals.off);
        wprintf(L"            cb  = 0x%08X;\n", hdrTpi.tpihash.offcbHashVals.cb);
        wprintf(L"        }\n");
        wprintf(L"        offcbTiOff    = {\n");
        wprintf(L"            off = 0x%08X;\n", hdrTpi.tpihash.offcbTiOff.off);
        wprintf(L"            cb  = 0x%08X;\n", hdrTpi.tpihash.offcbTiOff.cb);
        wprintf(L"        }\n");
        wprintf(L"        offcbHashAdj  = {\n");
        wprintf(L"            off = 0x%08X;\n", hdrTpi.tpihash.offcbHashAdj.off);
        wprintf(L"            cb  = 0x%08X;\n", hdrTpi.tpihash.offcbHashAdj.cb);
        wprintf(L"        }\n");
        wprintf(L"    }\n");
        wprintf(L"}\n");
    }
}


void dumpMSFByName(MSF *pmsf, PDB *ppdb, const wchar_t *szMSF)
{
    // dump streams, names in stream number order
    SN sn;
    CB cbTotal = 0;
    CB cbAlloc = 0;
    int csn = 0;
    CB cbPg = pmsf->GetCbPage();

    for (sn = 0; sn != snNil; sn++) {
        CB cb = pmsf->GetCbStream(sn);

        if (cb != cbNil) {
            ++csn;
            static const wchar_t szFmt[] =
                L"%4d: %7ld  %s\n"
                ;

            wprintf(szFmt, (int)sn, cb, GetStreamName(sn));

            cbTotal += cb;
            cb += cbPg - 1; cb -= cb % cbPg;
            cbAlloc += cb;
        }
    }

    struct _stat statbuf;
    _wstat(szMSF, &statbuf);

    wprintf(L"\ntotal: %d streams, %ld bytes used, %ld committed, %ld total\n", csn, cbTotal, cbAlloc, statbuf.st_size);
}


void dumpStream(void *p, int i, bool);


void dumpStreamsSz(MSF *pmsf, wchar_t *sz)
{
    Idx *pIdx = pIdxForSz(pmsf, sz, Str::dec, 0, snNil);

    wprintf(L"dump streams %ws:\n", (const wchar_t *) pIdx->asStr());

    pIdx->eachDo(dumpStream);

    delete pIdx;
}


void dumpStream(void *p, int i, bool)
{
    MSF *pmsf = (MSF *) p;
    SN sn = (SN) i;
    CB cb = pmsf->GetCbStream(sn);

    if (cb == cbNil) {
        return;
    }

    static const wchar_t szFmt[] =
        L"stream %3d: cb( %7ld ), nm( %s )\n"
        ;

    wprintf(szFmt, sn, cb, GetStreamName(sn));

    if (cb > 0) {
        PB pb = new BYTE[cb];

        if (!pb) {
            fatal(L"out of memory\n");
        }

        if (!pmsf->ReadStream(sn, pb, cb)) {
            fatal(L"can't MSF::ReadStream\n");
        }

        for (int ib = 0; ib < cb; ib += 16) {
            wprintf(L"%04X: ", ib);

            int dib;

            for (dib = 0; dib < 16 && ib + dib < cb; dib++) {
                wprintf(L"%s%02X", (dib == 8) ? L"  " : L" ", pb[ib + dib]);
            }

            for (; dib < 16; dib++) {
                wprintf((dib == 8) ? L"    " : L"   ");
            }

            wprintf(L"  ");

            for (dib = 0; dib < 16 && ib + dib < cb; dib++) {
                BYTE b = pb[ib + dib];
                putchar(isprint(b) ? b : '.');
            }

            putwchar(L'\n');
        }

        delete [] pb;
    }

    putwchar(L'\n');
}


bool dumpFileInfo(DBI *pdbi)
{
    USES_STACKBUFFER(0x400);

    wprintf(L"file info:\n");
    CB cb;
    PB pb;

    if (!pdbi->QueryFileInfo2(0, &cb) ||
        !(pb = new BYTE[cb]) ||
        !pdbi->QueryFileInfo2(pb, &cb))
        return false;

    PB pbT = pb;
    DWORD imodMac = *((DWORD *&) pbT)++;
    DWORD cRefs = *((DWORD *&) pbT)++;

    wprintf(L"imodMac:%d cRefs:%d\n", imodMac, cRefs);

    typedef char *PCH;
    typedef long ICH;
    DWORD* mpimodiref    = (DWORD*) pbT;
    DWORD* mpimodcref    = (DWORD*) ((PB)mpimodiref    + sizeof(DWORD) * imodMac);
    ICH*   mpirefichFile = (ICH*)   ((PB)mpimodcref    + sizeof(DWORD) * imodMac);
    PCH    rgchNames     = (PCH)    ((PB)mpirefichFile + sizeof(ICH) * cRefs);

    wprintf(L" imod    irefS    ifile     iref      ich szFile\n");

    for (DWORD imod = 0; imod < imodMac; imod++) {
        for (UINT diref = 0; diref < mpimodcref[imod]; diref++) {
            UINT iref = mpimodiref[imod] + diref;
            ICH ich = mpirefichFile[iref];

            wchar_t *wcs = GetSZUnicodeFromSZUTF8(&rgchNames[ich]);

            wprintf(L"%5d %8d %8d %8d %8d %s\n",
                imod, mpimodiref[imod], diref, iref, ich, wcs);
        }
    }

    putwchar(L'\n');

    if (pb) {
        delete [] pb;
    }

    return true;
}


bool dumpGlobals(DBI *pdbi) {
    wprintf(L"globals:\n");

    PB pbSym = 0;
    GSI* pgsi;

    if (!pdbi->OpenGlobals(&pgsi)) {
        return false;
    }

    while (pbSym = pgsi->NextSym(pbSym)) {
        dumpSymbol((SYMTYPE *) pbSym);
    }

    putwchar(L'\n');

    return (pgsi->Close() != FALSE);
}


bool dumpPublics(DBI *pdbi) {
    wprintf(L"publics:\n");

    PB pbSym = 0;
    GSI* pgsi;

    if (!pdbi->OpenPublics(&pgsi)) {
        return false;
    }

    while (pbSym = pgsi->NextSym(pbSym)) {
        dumpSymbol((SYMTYPE *) pbSym);
    }

    putwchar(L'\n');

    return (pgsi->Close() != FALSE);
}


bool dumpLinkInfo(DBI *pdbi)
{
    PB pb = NULL;
    CB cb;

    if (!pdbi->QueryLinkInfo((PLinkInfo) pb, &cb)) {
        return false;
    }

    if (!cb) {
        wprintf(L"No link info.\n");

        return true;
    }

    wprintf(L"linkinfo:\n");
    pb = new BYTE[cb];

    if (!pdbi->QueryLinkInfo((PLinkInfo) pb, &cb)) {
        return false;
    }

    PLinkInfo pli = (PLinkInfo) pb;

    wprintf(L"cb=%d\n"
            L"ver=%ld\n"
            L"offSzCwd=%ld\n"
            L"offSzCommand=%ld\n"
            L"ichOutFile=%ld\n"
            L"offSzLibs=%ld\n",
            pli->cb,
            pli->ver,
            pli->offszCwd,
            pli->offszCommand,
            pli->ichOutfile,
            pli->offszLibs);

    wprintf(L"SzCwd=%hs\n", pli->SzCwd());
    wprintf(L"SzCommand=%hs\n", pli->SzCommand());
    wprintf(L"SzOutFile=%hs\n", pli->SzOutFile());

    for(size_t i = pli->offszLibs, j = 0 ; i < pli->cb; j++) {
        wprintf(L"lib[%d]=%hs\n", j, pb + i);

        i += strlen((const char *) pb + i) + 1;
    }

    return true;
}


void dumpSymsSz(DBI *pdbi, const wchar_t *sz)
{
    Mod *pmod = 0;

    if (wcscmp(sz, L"all") == 0) {
        while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
            dumpSymbols(pmod);
        }

        return;
    }

    if (!pdbi->OpenModW(sz, 0, &pmod)) {
        return;
    }

    dumpSymbols(pmod);

    pmod->Close();
}


void dumpSymTypesSz(DBI *pdbi, const wchar_t *sz)
{
    Mod *pmod = 0;

    if (wcscmp(sz, L"all") == 0) {
        while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
            dumpSymbolTypes(pmod);
        }

        return;
    }

    if (!pdbi->OpenModW(sz, 0, &pmod))
        return;

    dumpSymbolTypes(pmod);

    pmod->Close();
}


bool dumpSymbolTypes(Mod *pmod)
{
    CB cb;

    if (!pmod->QuerySymbols(0, &cb)) {
        return false;
    }

    PB pb = new BYTE[cb];

    if (!pb) {
        return false;
    }

    if (!pmod->QuerySymbols(pb, &cb)) {
        return false;
    }

    const SYMTYPE *psymMac = (SYMTYPE *) (pb + cb);

    for (SYMTYPE *psym = (SYMTYPE *) (pb + sizeof(ULONG)); psym < psymMac; psym = (SYMTYPE *) pbEndSym(psym)) {
        for (SymTiIter tii(psym); tii.next();) {
            dumpTpiTi(0, tii.rti(), tii.fId());
        }
    }

    delete [] pb;

    putwchar(L'\n');

    return true;
}


bool dumpSymbols(Mod *pmod)
{
    CB cb;

    if (!pmod->QuerySymbols(0, &cb)) {
        return false;
    }

    PB pb = new BYTE[cb];

    if (!pb) {
        return false;
    }

    if (!pmod->QuerySymbols(pb, &cb)) {
        return false;
    }

    const SYMTYPE *psymMac = (SYMTYPE *) (pb + cb);

    for (SYMTYPE *psym = (SYMTYPE *) (pb + sizeof(ULONG)); psym < psymMac; psym = (SYMTYPE *) pbEndSym(psym)) {
        dumpSymbol(psym);
    }

    delete [] pb;

    putwchar(L'\n');

    return true;
}


Str strForTypeTi(TI ti, Str str = L"", Bind bind = bindNone);
Str strForFuncAttr(CV_funcattr_t funcattr);

Str strForREFSYM(const SYMTYPE *);
Str strForDATASYM32(const SYMTYPE *);
Str strForPROCSYM32(const SYMTYPE *);
Str strForSymNYI(const SYMTYPE *);
Str strForREGREL32(const SYMTYPE *);
Str strForBPREL32(const SYMTYPE *);
Str strForMANPROCSYM(const SYMTYPE *);
Str strForFRAMERELSYM(const SYMTYPE *);
Str strForATTRREGSYM(const SYMTYPE *);
Str strForATTRSLOTSYM(const SYMTYPE *);
Str strForATTRMANYREGSYM(const SYMTYPE *);
Str strForATTRREGREL(const SYMTYPE *);
Str strForATTRMANYREGSYM2(const SYMTYPE *);

void dumpSymbol(SYMTYPE *psym)
{
    Str str;

    switch (SZSYMIDX(psym->rectyp)) {
        case S_COMPILE:     str = strForSymNYI(psym); break;
        case S_REGISTER:    str = strForSymNYI(psym); break;
        case S_CONSTANT:    str = strForSymNYI(psym); break;
        case S_UDT:         str = strForSymNYI(psym); break;
        case S_SSEARCH:     str = strForSymNYI(psym); break;
        case S_END:         str = strForSymNYI(psym); break;
        case S_SKIP:        str = strForSymNYI(psym); break;
        case S_CVRESERVE:   str = strForSymNYI(psym); break;
        case S_OBJNAME:     str = strForSymNYI(psym); break;
        case S_ENDARG:      str = strForSymNYI(psym); break;
        case S_COBOLUDT:    str = strForSymNYI(psym); break;
        case S_MANYREG:     str = strForSymNYI(psym); break;
        case S_RETURN:      str = strForSymNYI(psym); break;
        case S_ENTRYTHIS:   str = strForSymNYI(psym); break;
        case S_BPREL16:     str = strForSymNYI(psym); break;
        case S_LDATA16:     str = strForSymNYI(psym); break;
        case S_GDATA16:     str = strForSymNYI(psym); break;
        case S_PUB16:       str = strForSymNYI(psym); break;
        case S_LPROC16:     str = strForSymNYI(psym); break;
        case S_GPROC16:     str = strForSymNYI(psym); break;
        case S_THUNK16:     str = strForSymNYI(psym); break;
        case S_BLOCK16:     str = strForSymNYI(psym); break;
        case S_WITH16:      str = strForSymNYI(psym); break;
        case S_LABEL16:     str = strForSymNYI(psym); break;
        case S_CEXMODEL16:  str = strForSymNYI(psym); break;
        case S_VFTABLE16:   str = strForSymNYI(psym); break;
        case S_REGREL16:    str = strForSymNYI(psym); break;
        case S_BPREL32:     str = strForBPREL32(psym); break;
        case S_LDATA32:     str = strForDATASYM32(psym); break;
        case S_GDATA32:     str = strForDATASYM32(psym); break;
        case S_PUB32:       str = strForDATASYM32(psym); break;
        case S_LPROC32:     str = strForPROCSYM32(psym); break;
#if defined(CC_DP_CXX)
        case S_LPROC32_DPC: str = strForPROCSYM32(psym); break;
#endif // CC_DP_CXX
        case S_GPROC32:     str = strForPROCSYM32(psym); break;
        case S_THUNK32:     str = strForSymNYI(psym); break;
        case S_BLOCK32:     str = strForSymNYI(psym); break;
        case S_WITH32:      str = strForSymNYI(psym); break;
        case S_LABEL32:     str = strForSymNYI(psym); break;
        case S_CEXMODEL32:  str = strForSymNYI(psym); break;
        case S_VFTABLE32:   str = strForSymNYI(psym); break;
        case S_REGREL32:    str = strForREGREL32(psym); break;
        case S_LTHREAD32:   str = strForDATASYM32(psym); break;
        case S_GTHREAD32:   str = strForDATASYM32(psym); break;
        case S_LPROCMIPS:   str = strForSymNYI(psym); break;
        case S_GPROCMIPS:   str = strForSymNYI(psym); break;
        case S_PROCREF:     str = strForREFSYM(psym); break;
        case S_DATAREF:     str = strForREFSYM(psym); break;
        case S_ALIGN:       str = strForSymNYI(psym); break;
        case S_LPROCREF:    str = strForREFSYM(psym); break;
        case S_GMANPROC:
        case S_LMANPROC:    str = strForMANPROCSYM(psym); break;
        case S_MANFRAMEREL: str = strForFRAMERELSYM(psym); break;
        case S_MANREGISTER: str = strForATTRREGSYM(psym); break;
        case S_MANSLOT:     str = strForATTRSLOTSYM(psym); break;
        case S_MANMANYREG:  str = strForATTRMANYREGSYM(psym); break;
        case S_MANREGREL:   str = strForATTRREGREL(psym); break;
        case S_MANMANYREG2: str = strForATTRMANYREGSYM2(psym); break;
        case S_MANTYPREF:   str = strForSymNYI(psym); break;
        case S_UNAMESPACE:  str = strForSymNYI(psym); break;
    }

    SZ szRecTyp = "???";
    fGetSymRecTypName(psym, &szRecTyp);

    wchar_t szName[2048];
    unsigned long len = _countof(szName);

    if (!fNameFromSym(psym, szName, &len)) {
        wcscpy_s(szName, _countof(szName), L"???");
    }

    wprintf(L"%hs[%s] ", szRecTyp, (const wchar_t *) str);

    SymTiIter tii(psym);
    if (tii.next()) {
        wprintf(L"%s;\n", (const wchar_t *) strForTypeTi(tii.rti(), Str(szName).sansExtraSpaces()));
    }

    else {
        wprintf(L"%s\n", szName);
    }
}

Str strForREGREL32(const SYMTYPE *psym)
{
    const REGREL32 *p = (REGREL32 *) psym;

    return Str::fmt(L"off:%08lx reg:%02x typ:%08X",
                    p->off, p->reg, p->typind);
}

Str strForBPREL32(const SYMTYPE *psym)
{
    const BPRELSYM32 *p = (BPRELSYM32 *) psym;

    return Str::fmt(L"off:%08lx typ:%08X",
                    p->off, p->typind);
}

Str strForDATASYM32(const SYMTYPE *psym)
{
    const DATASYM32 *p = (DATASYM32 *) psym;

    return Str::fmt(L"off:%08lx seg:%02x typ:%08X",
                    p->off, p->seg, p->typind);
}


Str strForREFSYM(const SYMTYPE *psym)
{
    const REFSYM *p = (REFSYM *) psym;

    return Str::fmt(L"sum:%08lx ib:%08lx imod:%04X pad:%04X",
                    p->sumName, p->ibSym, p->imod, p->usFill);
}


Str strForPROCSYM32(const SYMTYPE *psym)
{
    const PROCSYM32 *p = (PROCSYM32 *) psym;

    return Str::fmt(L"pPa:%lx pEn:%lx pNe:%lx len:%lx DbgS:%lx DbgE:%lx off:%08lx seg:%02x typ:%08X fla:%X",
                    p->pParent, p->pEnd, p->pNext, p->len, p->DbgStart, p->DbgEnd, p->off, p->seg, p->typind, p->flags);
}


Str strForMANPROCSYM(const SYMTYPE *psym)
{
    const MANPROCSYM *p = (MANPROCSYM *) psym;

    return Str::fmt(L"pPa:%lx pEn:%lx pNe:%lx DbgS:%lx DbgE:%lx off:%08lx seg:%02x token:%08X fla:%X",
                    p->pParent, p->pEnd, p->pNext, p->DbgStart, p->DbgEnd, p->off, p->seg, p->token, p->flags);
}


Str strForFRAMERELSYM(const SYMTYPE *psym)
{
    const FRAMERELSYM *p = (FRAMERELSYM *) psym;

    return Str::fmt(L"off:%X %s:%08X",
        p->off, p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForATTRREGSYM(const SYMTYPE *psym)
{
    const ATTRREGSYM *p = (ATTRREGSYM *) psym;

    return Str::fmt(L"reg:%X %s:%08X",
        p->reg, p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForATTRSLOTSYM(const SYMTYPE *psym)
{
    const ATTRSLOTSYM *p = (ATTRSLOTSYM *) psym;

    return Str::fmt(L"slot:%d %s:%08X",
        p->iSlot, p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForATTRMANYREGSYM(const SYMTYPE *psym)
{
    const ATTRMANYREGSYM *p = (ATTRMANYREGSYM *) psym;

    return Str::fmt(L"%s:%08X",
        p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForATTRREGREL(const SYMTYPE *psym)
{
    const ATTRREGREL *p = (ATTRREGREL *) psym;

    return Str::fmt(L"reg:%X off:%X %s:%08X",
        p->reg, p->off, p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForATTRMANYREGSYM2(const SYMTYPE *psym)
{
    const ATTRMANYREGSYM2 *p = (ATTRMANYREGSYM2 *) psym;

    return Str::fmt(L"%s:%08X",
        p->attr.flags.fIsParam ? L"param" : L"token" , p->typind);
}


Str strForSymNYI(const SYMTYPE *psym)
{
    return L"...";
}


void dumpTpiTi(void *p, int i, bool fId);
Str strForTI(TI ti);
Str strForPrimitiveTi(TI ti);
Str strForNYI(void *pleaf, Str strBase, Bind bind);
Str strForModifier(lfModifier* pm, Str str);
Str strForPtr(lfPointer* pp, Str strBase, Bind bind);
Str strForArray(lfArray* pa, Str strBase, Bind bind);
Str strForClassStruct(lfStructure* ps, Str strBase);
Str strForUnion(lfUnion* pu, Str strBase);
Str strForEnum(lfEnum* pe, Str strBase);
Str strForProp(CV_prop_t prop);
Str strForProc(lfProc* pp, Str str, Bind bind);
Str strForMFunc(lfMFunc* pf, Str strBase, Bind bind);
Str strForArgList(lfArgList* pa);
Str strForFieldList(lfFieldList* pf, size_t cb);
Str strForMember(lfMember*pdata, size_t *pcb);
Str strForBClass(lfBClass* pb, size_t *pcb);
Str strForVBClass(lfVBClass* pb, size_t *pcb);
Str strForTagTi(TI ti);
Str strForAttr(struct CV_fldattr_t a);
Str strForMember(lfMember* pm, size_t *pcb);
Str strForEnumerate(lfEnumerate* pe, size_t *pcb);
Str strSep(bool& fFirst, const wchar_t *szFirst = L"", const wchar_t *szRest = L",");


void dumpTpiSz(TPI *ptpi, wchar_t *sz, bool fId)
{
    Idx *pIdx = pIdxForSz(ptpi, sz, Str::hex, ptpi->QueryTiMin(), ptpi->QueryTiMac());

    wprintf(L"dump %s %s:\n", fId ? L"IDs" : L"types", (const wchar_t *) pIdx->asStr());


    pIdx->eachDo(dumpTpiTi);

    delete pIdx;
}


void dumpTpiLastSz(TPI *ptpi, bool fId)
{
    TI ti = ptpi->QueryTiMac() - 1;

    wprintf(L"last %s index = %d(0x%04X)\n", fId ? L"ID" : L"type", ti, ti);
}


Str strForTI(TI ti)
{
    return Str((int) ti, Str::hex);
}


void dumpTpiTi(void *p, int i, bool fId)
{
    TI ti = i;

    wprintf(L"%s: %s;\n", (const wchar_t *) strForTI(ti),
           (const wchar_t *) strForTypeTi(ti, fId ? L"I" : L"T" + strForTI(ti)).sansExtraSpaces());
}


Str strNoRecForTi(TI ti)
{
    static int count = 0;

    if (count++ > 20) {
        exit(1);
    }

    return L"#Error#: No Rec for TI " + strForTI(ti);
}

Str strForFuncAttr(CV_funcattr_t funcattr)
{
    return
        funcattr.cxxreturnudt ? L"return UDT (C++ style)" : L"" +
        funcattr.ctor ? L"instance constructor" : L"" +
        funcattr.ctorvbase ? L"instance constructor of a class with virtual base" : L"";
}


Str strForTypeTi(TI ti, Str str, Bind bind)
{
    if (ti < ptpi->QueryTiMin()) {
        Str strPrim = strForPrimitiveTi(ti);

        return str.isEmpty() ? strPrim : strPrim + L" " + str;
    }

    PB pb;

    if (!ptpi->QueryPbCVRecordForTi(ti, &pb)) {
        return strNoRecForTi(ti);
    }

    TYPTYPE *ptype = (TYPTYPE *) pb;
    void *pleaf = &ptype->leaf;

    switch (SZLEAFIDX(ptype->leaf)) {
        case LF_MODIFIER:       return strForModifier((lfModifier *) pleaf, str);
        case LF_POINTER:        return strForPtr((lfPointer *) pleaf, str, bind);
        case LF_ARRAY:          return strForArray((lfArray *) pleaf, str, bind);
        case LF_CLASS:          return strForClassStruct((lfStructure *) pleaf, str);
        case LF_STRUCTURE:      return strForClassStruct((lfStructure *) pleaf, str);
        case LF_UNION:          return strForUnion((lfUnion *) pleaf, str);
        case LF_ENUM:           return strForEnum((lfEnum *) pleaf, str);
        case LF_PROCEDURE:      return strForProc((lfProc *) pleaf, str, bind);
        case LF_MFUNCTION:      return strForMFunc((lfMFunc *) pleaf, str, bind);
        case LF_VTSHAPE:        return strForNYI(pleaf, str, bind);
        case LF_VFTABLE:        return strForNYI(pleaf, str, bind);
        case LF_COBOL0:         return strForNYI(pleaf, str, bind);
        case LF_COBOL1:         return strForNYI(pleaf, str, bind);
        case LF_BARRAY:         return strForNYI(pleaf, str, bind);
        case LF_LABEL:          return strForNYI(pleaf, str, bind);
        case LF_NULL:           return strForNYI(pleaf, str, bind);
        case LF_NOTTRAN:        return strForNYI(pleaf, str, bind);
        case LF_DIMARRAY:       return strForNYI(pleaf, str, bind);
        case LF_VFTPATH:        return strForNYI(pleaf, str, bind);
        case LF_PRECOMP:        return strForNYI(pleaf, str, bind);
        case LF_ENDPRECOMP:     return strForNYI(pleaf, str, bind);
        case LF_TYPESERVER:     return strForNYI(pleaf, str, bind);
        case LF_SKIP:           return strForNYI(pleaf, str, bind);
        case LF_ARGLIST:        return strForArgList((lfArgList *) pleaf);
        case LF_DEFARG:         return strForNYI(pleaf, str, bind);
        case LF_LIST:           return strForNYI(pleaf, str, bind);
        case LF_FIELDLIST:      return strForFieldList((lfFieldList *) pleaf, ptype->len);
        case LF_DERIVED:        return strForNYI(pleaf, str, bind);
        case LF_BITFIELD:       return strForNYI(pleaf, str, bind);
        case LF_METHODLIST:     return strForNYI(pleaf, str, bind);
        case LF_DIMCONU:        return strForNYI(pleaf, str, bind);
        case LF_DIMCONLU:       return strForNYI(pleaf, str, bind);
        case LF_DIMVARU:        return strForNYI(pleaf, str, bind);
        case LF_DIMVARLU:       return strForNYI(pleaf, str, bind);
        case LF_REFSYM:         return strForNYI(pleaf, str, bind);
        case LF_BCLASS:         return strForNYI(pleaf, str, bind);
        case LF_VBCLASS:        return strForNYI(pleaf, str, bind);
        case LF_IVBCLASS:       return strForNYI(pleaf, str, bind);
        case LF_ENUMERATE:      return strForNYI(pleaf, str, bind);
        case LF_FRIENDFCN:      return strForNYI(pleaf, str, bind);
        case LF_INDEX:          return strForNYI(pleaf, str, bind);
        case LF_MEMBER:         return strForNYI(pleaf, str, bind);
        case LF_STMEMBER:       return strForNYI(pleaf, str, bind);
        case LF_METHOD:         return strForNYI(pleaf, str, bind);
        case LF_ONEMETHOD:      return strForNYI(pleaf, str, bind);
        case LF_NESTTYPE:       return strForNYI(pleaf, str, bind);
        case LF_VFUNCTAB:       return strForNYI(pleaf, str, bind);
        case LF_FRIENDCLS:      return strForNYI(pleaf, str, bind);
            // these need to be fixed for ST strings
        case LF_TYPESERVER_ST:  return strForNYI(pleaf, str, bind);
        case LF_ENUMERATE_ST:   return strForNYI(pleaf, str, bind);
        case LF_ARRAY_ST:       return strForArray((lfArray *) pleaf, str, bind);
        case LF_CLASS_ST:       return strForClassStruct((lfStructure *) pleaf, str);
        case LF_STRUCTURE_ST:   return strForClassStruct((lfStructure *) pleaf, str);
        case LF_UNION_ST:       return strForClassStruct((lfStructure *) pleaf, str);
        case LF_ENUM_ST:        return strForEnum((lfEnum *) pleaf, str);
        case LF_DIMARRAY_ST:    return strForNYI(pleaf, str, bind);
        case LF_PRECOMP_ST:     return strForNYI(pleaf, str, bind);
        case LF_ALIAS_ST:       return strForNYI(pleaf, str, bind);
        case LF_DEFARG_ST:      return strForNYI(pleaf, str, bind);
        case LF_FRIENDFCN_ST:   return strForNYI(pleaf, str, bind);
        case LF_MEMBER_ST:      return strForNYI(pleaf, str, bind);
        case LF_STMEMBER_ST:    return strForNYI(pleaf, str, bind);
        case LF_METHOD_ST:      return strForNYI(pleaf, str, bind);
        case LF_NESTTYPE_ST:    return strForNYI(pleaf, str, bind);
        case LF_ONEMETHOD_ST:   return strForNYI(pleaf, str, bind);
        case LF_NESTTYPEEX_ST:  return strForNYI(pleaf, str, bind);
        case LF_MEMBERMODIFY_ST:return strForNYI(pleaf, str, bind);
        case LF_CHAR:           return strForNYI(pleaf, str, bind);
        case LF_SHORT:          return strForNYI(pleaf, str, bind);
        case LF_USHORT:         return strForNYI(pleaf, str, bind);
        case LF_LONG:           return strForNYI(pleaf, str, bind);
        case LF_ULONG:          return strForNYI(pleaf, str, bind);
        //
        //  The following two cases were added for BIGINT support
        //
        case LF_OCTWORD:        return strForNYI(pleaf, str, bind);
        case LF_UOCTWORD:       return strForNYI(pleaf, str, bind);

        case LF_REAL32:         return strForNYI(pleaf, str, bind);
        case LF_REAL64:         return strForNYI(pleaf, str, bind);
        case LF_REAL80:         return strForNYI(pleaf, str, bind);
        case LF_REAL128:        return strForNYI(pleaf, str, bind);
        case LF_QUADWORD:       return strForNYI(pleaf, str, bind);
        case LF_UQUADWORD:      return strForNYI(pleaf, str, bind);
        case LF_REAL48:         return strForNYI(pleaf, str, bind);
        case LF_PAD0:           return strForNYI(pleaf, str, bind);
        case LF_PAD1:           return strForNYI(pleaf, str, bind);
        case LF_PAD2:           return strForNYI(pleaf, str, bind);
        case LF_PAD3:           return strForNYI(pleaf, str, bind);
        case LF_PAD4:           return strForNYI(pleaf, str, bind);
        case LF_PAD5:           return strForNYI(pleaf, str, bind);
        case LF_PAD6:           return strForNYI(pleaf, str, bind);
        case LF_PAD7:           return strForNYI(pleaf, str, bind);
        case LF_PAD8:           return strForNYI(pleaf, str, bind);
        case LF_PAD9:           return strForNYI(pleaf, str, bind);
        case LF_PAD10:          return strForNYI(pleaf, str, bind);
        case LF_PAD11:          return strForNYI(pleaf, str, bind);
        case LF_PAD12:          return strForNYI(pleaf, str, bind);
        case LF_PAD13:          return strForNYI(pleaf, str, bind);
        case LF_PAD14:          return strForNYI(pleaf, str, bind);
        case LF_PAD15:
        default:
            return strForNYI(pleaf, str, bind);
    }
}


Str strForPrimitiveTi(TI ti)
{
    const wchar_t *szPrim = NULL;

    switch (ti) {
#define P(X) case X: szPrim = L#X; break;
#define PS(X,S) case X: szPrim = L#S; break;
    P(T_NOTYPE) P(T_ABS) P(T_SEGMENT) PS(T_VOID,void) PS(T_PVOID,void near*)
    PS(T_PFVOID,void far*) PS(T_PHVOID,void huge*) PS(T_32PVOID,void *)
    P(T_32PFVOID) P(T_NOTTRANS)
    PS(T_CHAR,signed char) PS(T_UCHAR,unsigned char) PS(T_PCHAR,signed char near*)
    PS(T_PUCHAR,unsigned char near*) PS(T_PFCHAR,char far*)
    PS(T_PFUCHAR,unsigned char far*) PS(T_PHCHAR,char huge*)
    PS(T_PHUCHAR,unsigned char huge*) PS(T_32PCHAR,char *)
    PS(T_32PUCHAR,unsigned char *) P(T_32PFCHAR) P(T_32PFUCHAR)
    PS(T_RCHAR,char) PS(T_PRCHAR,char near*) PS(T_PFRCHAR,char far*)
    PS(T_PHRCHAR,char huge*) PS(T_32PRCHAR,char *) P(T_32PFRCHAR)
    PS(T_WCHAR,wchar_t) PS(T_PWCHAR,wchar_t near*) PS(T_PFWCHAR,wchar far*)
    PS(T_PHWCHAR,wchar_t huge*) PS(T_32PWCHAR,wchar_t *)
    P(T_32PFWCHAR) PS(T_SHORT,short) PS(T_USHORT,unsigned)
    PS(T_PSHORT,short near*) PS(T_PUSHORT,unsigned short near*)
    PS(T_PFSHORT,short far*) PS(T_PFUSHORT,unsigned short far*)
    PS(T_PHSHORT,short huge*) PS(T_PHUSHORT,unsigned short huge*)
    P(T_32PSHORT) P(T_32PUSHORT) P(T_32PFSHORT)
    P(T_32PFUSHORT) PS(T_INT2,int16) PS(T_UINT2,unsigned int16)
    PS(T_PINT2,int16 near*) PS(T_PUINT2,unsigned int16 near*)
    PS(T_PFINT2,int16 far*) PS(T_PFUINT2,unsigned int16 far*)
    PS(T_PHINT2,int16 huge*) PS(T_PHUINT2,unsigned int16 huge*) P(T_32PINT2)
    P(T_32PUINT2) P(T_32PFINT2) P(T_32PFUINT2) PS(T_LONG,long)
    PS(T_ULONG,unsigned long) PS(T_PLONG,long near*)
    PS(T_PULONG,unsigned long near*) PS(T_PFLONG,long far*)
    PS(T_PFULONG,unsigned long far*) PS(T_PHLONG,long huge*)
    PS(T_PHULONG,unsigned long huge*) PS(T_32PLONG, long *)
    PS(T_32PULONG, unsigned long *) P(T_32PFLONG)
    P(T_32PFULONG) PS(T_INT4,int) PS(T_UINT4,unsigned)
    P(T_PINT4) P(T_PUINT4) P(T_PFINT4) P(T_PFUINT4) P(T_PHINT4) P(T_PHUINT4)
    PS(T_32PINT4,int *) PS(T_32PUINT4,unsigned *)
    P(T_32PFINT4) P(T_32PFUINT4)
    //
    //  The following were added for BIGINT support
    //
    PS(T_QUAD,quad) PS(T_UQUAD,unsigned quad) PS(T_PQUAD,quad near*)
    PS(T_PUQUAD,unsigned quad near*) PS(T_PFQUAD,quad far*)
    PS(T_PFUQUAD,unsigned quad far*) PS(T_PHQUAD,quad huge*)
    PS(T_PHUQUAD,unsigned quad huge*) P(T_32PQUAD) P(T_32PUQUAD) P(T_32PFQUAD)
    P(T_32PFUQUAD)

    PS(T_INT8,int64) PS(T_UINT8,unsigned int64) PS(T_PINT8,int64 near*)
    PS(T_PUINT8,unsigned int64 near*) PS(T_PFINT8,int64 far*)
    PS(T_PFUINT8,unsigned int64 far*) PS(T_PHINT8,int64 huge*)
    PS(T_PHUINT8,unsigned int64 huge*) P(T_32PINT8) P(T_32PUINT8) P(T_32PFINT8)
    P(T_32PFUINT8)

    PS(T_OCT,octet) PS(T_UOCT,unsigned octet) PS(T_POCT,octet near*)
    PS(T_PUOCT,unsigned octet near*) PS(T_PFOCT,octet far*)
    PS(T_PFUOCT,unsigned octet far*) PS(T_PHOCT,octet huge*)
    PS(T_PHUOCT,unsigned octet huge*) P(T_32POCT) P(T_32PUOCT) P(T_32PFOCT)
    P(T_32PFUOCT)

    PS(T_INT16,int128) PS(T_UINT16,unsigned int128) PS(T_PINT16,int128 near*)
    PS(T_PUINT16,unsigned int128 near*) PS(T_PFINT16,int128 far*)
    PS(T_PFUINT16,unsigned int128 far*) PS(T_PHINT16,int128 huge*)
    PS(T_PHUINT16,unsigned int128 huge*) P(T_32PINT16) P(T_32PUINT16) P(T_32PFINT16)
    P(T_32PFUINT16)
    //
    //  end of BIGINT support
    //
    PS(T_REAL32,float) PS(T_PREAL32,float near*)
    PS(T_PFREAL32,float far*) PS(T_PHREAL32,float huge*)
    PS(T_32PREAL32,float *) P(T_32PFREAL32) PS(T_REAL64,double)
    PS(T_PREAL64,double near*) PS(T_PFREAL64,double far*)
    PS(T_PHREAL64,double huge*) PS(T_32PREAL64,double *)
    P(T_32PFREAL64) PS(T_REAL80,long double) PS(T_PREAL80,long double near*)
    PS(T_PFREAL80,long double far*) PS(T_PHREAL80,long double huge*)
    PS(T_32PREAL80,long double *) P(T_32PFREAL80)
    }

    return szPrim ? Str(szPrim) : L"<" + Str(ti) + L">";
}


Str strForNYI(void *pleaf, Str strBase, Bind bind)
{
    return L"<<" + strBase + L">>";
}


Str strForModifier(lfModifier* pm, Str str)
{
    Str strMod;

    if (pm->attr.MOD_const) {
        strMod += L"const ";
    }

    if (pm->attr.MOD_volatile) {
        strMod += L"volatile ";
    }

    return strMod + strForTypeTi(pm->type) + str;
}


Str strForPtr(lfPointer* pp, Str strBase, Bind bind)
{
    static const wchar_t * const mppmenumsz[] =
    {
        L"pdm16_nonvirt",
        L"pdm16_vfcn",
        L"pdm16_vbase",
        L"pdm32_nvvfcn",
        L"pdm32_vbase",
        L"pmf16_nearnvsa",
        L"pmf16_nearnvma",
        L"pmf16_nearvbase",
        L"pmf16_farnvsa",
        L"pmf16_farnvma",
        L"pmf16_farvbase",
        L"pmf32_nvsa",
        L"pmf32_nvma",
        L"pmf32_vbase"
    };

    static const wchar_t * const mpptrtypesz[] =
    {
        L"near",
        L"far",
        L"huge",
        L"base(seg)",
        L"base(val)",
        L"base(segval)",
        L"base(addr)",
        L"base(segaddr)",
        L"base(type)",
        L"base(self)",
        L"",
        L"far32",
        L"far64",
        L"unspecified",
    };

    Str str;

    if (pp->attr.isflat32) {
        str = L"flat ";
    }

    switch (pp->attr.ptrmode) {
        case CV_PTR_MODE_PTR:
            str += Str(mpptrtypesz[pp->attr.ptrtype]) + L"*";
            break;

        case CV_PTR_MODE_LVREF:
            str += Str(mpptrtypesz[pp->attr.ptrtype]) + L"&";
            break;

        case CV_PTR_MODE_RVREF:
            str += Str(mpptrtypesz[pp->attr.ptrtype]) + L"&&";
            break;

        case CV_PTR_MODE_PMEM:
        case CV_PTR_MODE_PMFUNC:
            str = strForTypeTi(pp->pbase.pm.pmclass) + L"::* <" +
                  mppmenumsz[pp->pbase.pm.pmenum] + L">";
            break;
    }

    if (pp->attr.isconst)
        str += L"const ";

    if (pp->attr.isvolatile)
        str += L"volatile ";

    // TODO: exotic based modes

    return strForTypeTi(pp->utype, str + L" " + strBase, bindPtr);
}


Str strForArray(lfArray* pa, Str strBase, Bind bind)
{
    if (bind < bindArray) {
        strBase = L"(" + strBase + L")";
    }

    ulong size;
    CbExtractNumeric(pa->data, &size);

    Str strIdx = L"<" + strForTypeTi(pa->idxtype) + L">";
    Str str = strBase + L"[" + Str(size) + strIdx + L"]";

    // TODO: exotic subscript types

    return strForTypeTi(pa->elemtype, str, bindArray);
}


Str strForClassStruct(lfStructure* ps, Str strBase)
{
    ulong size;
    size_t dcb = CbExtractNumeric(ps->data, &size);

    Str str = (SZLEAFIDX(ps->leaf) == LF_STRUCTURE) ? L"struct " : L"class ";
    str += Str((char *) ps->data + dcb);

    str += L"<" + Str(ps->count) + L",";
    str += strForProp(ps->property) + L">";

    if (ps->field) {
        str += strForTypeTi(ps->field);
    }

    return str + L" " + strBase;
}


Str strForUnion(lfUnion* pu, Str strBase)
{
    ulong size;
    size_t dcb = CbExtractNumeric(pu->data, &size);

    Str str = L"union " + Str((char *) pu->data + dcb);
    str += L"<" + Str(pu->count) + L",";
    str += strForProp(pu->property) + L">";

    if (pu->field) {
        str += strForTypeTi(pu->field);
    }

    return str + L" " + strBase;
}


Str strForEnum(lfEnum* pe, Str strBase)
{
    Str str = L"enum ";
    str += Str((char *) pe->Name);
    str += L"<" + Str(pe->count) + L",";
    str += strForTypeTi(pe->utype) + L",";
    str += strForProp(pe->property) + L">";

    if (pe->field) {
        str += strForTypeTi(pe->field);
    }

    return str + L" " + strBase;
}


Str strForProp(CV_prop_t prop)
{
    Str str;
    bool fFirst = true;

    if (prop.packed)          str += strSep(fFirst) + L"packed";
    if (prop.ctor)            str += strSep(fFirst) + L"ctor";
    if (prop.ovlops)          str += strSep(fFirst) + L"ovlops";
    if (prop.isnested)        str += strSep(fFirst) + L"isnested";
    if (prop.cnested)         str += strSep(fFirst) + L"cnested";
    if (prop.opassign)        str += strSep(fFirst) + L"opassign";
    if (prop.opcast)          str += strSep(fFirst) + L"opcast";
    if (prop.fwdref)          str += strSep(fFirst) + L"fwdref";
    if (prop.scoped)          str += strSep(fFirst) + L"scoped";
    if (prop.hasuniquename)   str += strSep(fFirst) + L"hasuniquename";
    if (prop.sealed)          str += strSep(fFirst) + L"sealed";
    switch (prop.hfa) {
        case CV_HFA_none :    break;
        case CV_HFA_float :   str += strSep(fFirst) + L"hfaFloat"; break;
        case CV_HFA_double :  str += strSep(fFirst) + L"hfaDoub;e"; break;
        default :             str += strSep(fFirst) + L"hfa(" + Str(prop.hfa) + L")"; break;
    }
    if (prop.intrinsic)       str += strSep(fFirst) + L"intrinsic";
    switch (prop.mocom) {
        case CV_MOCOM_UDT_none :        break;
        case CV_MOCOM_UDT_ref :         str += strSep(fFirst) + L"ref"; break;
        case CV_MOCOM_UDT_value :       str += strSep(fFirst) + L"value"; break;
        case CV_MOCOM_UDT_interface :   str += strSep(fFirst) + L"interface"; break;
    }

    return str;
}


Str strForFieldList(lfFieldList* pfl, size_t cbList)
{
    Str     str;
    PB      pdata;
    size_t  cb          = 0;
    bool    fBases      = true;
    bool    fMembers    = true;

    while (cb < cbList) {
        // skip pad bytes
        for (;;) {
            pdata = (PB) pfl->data + cb;
            if (*(BYTE *) pdata < LF_PAD0)
                break;
            cb++;
        }

        switch (SZLEAFIDX(*(ushort *) pdata)) {
            case LF_BCLASS:
                str += strSep(fBases, L" : ", L", ");
                str += strForBClass((lfBClass *) pdata, &cb);
                break;

            case LF_VBCLASS:
            case LF_IVBCLASS:
                str += strSep(fBases, L" : ", L", ");
                str += strForVBClass((lfVBClass *) pdata, &cb);
                break;

            case LF_MEMBER:
                str += strSep(fMembers, L" { ", L" ");
                str += strForMember((lfMember *) pdata, &cb);
                str += L";";
                break;

            case LF_ENUMERATE:
                str += strSep(fMembers, L" { ", L" ");
                str += strForEnumerate((lfEnumerate *) pdata, &cb);
                str += L",";

            default:
                str += L"...";
                goto out;
        }
    }

out:
    str += strSep(fMembers, L" {}", L" }");

    return str;
}


Str strForBClass(lfBClass* pb, size_t* pcb)
{
    ulong offset;

    *pcb += sizeof(lfBClass) + CbExtractNumeric(pb->offset, &offset);

    Str strOff = L"<@" + Str(offset) + L"> ";

    return strForAttr(pb->attr) + L" " + strForTypeTi(pb->index) + strOff;
}


Str strForVBClass(lfVBClass *pb, size_t *pcb)
{
    bool fInd = (pb->leaf == LF_IVBCLASS);
    Str str;
    ulong offVbp;
    ulong offVbte;

    size_t cb = CbExtractNumeric(pb->vbpoff, &offVbp);

    *pcb += sizeof(lfVBClass) + cb +
            CbExtractNumeric(pb->vbpoff + cb, &offVbte);

    if (fInd) {
        str = L"< indirect ";
    }

    str += strForAttr(pb->attr) + L" " + strForTagTi(pb->index);

    if (!fInd) {
        str += L"< ";
    }

    str += strForTypeTi(pb->vbptr, L"vbp") + L";";
    str += L"offVbp=" + Str(offVbp) + L"; offVbte=" + Str(offVbte) + L";";
    str += L" >";

    return str;
}


Str strForTagTi(TI ti)
{
    PB pb;

    if (!ptpi->QueryPbCVRecordForTi(ti, &pb)) {
        return strNoRecForTi(ti);
    }

    TYPTYPE *ptype = (TYPTYPE *) pb;
    assert(SZLEAFIDX(ptype->leaf) == LF_STRUCTURE || SZLEAFIDX(ptype->leaf) == LF_CLASS);

    lfStructure *ps = (lfStructure *) &ptype->leaf;

    ulong size;
    size_t dcb = CbExtractNumeric(ps->data, &size);

    return Str((char *) ps->data + dcb);
}


Str strForMember(lfMember* pm, size_t *pcb)
{
    ulong offset;
    size_t cbOffset = CbExtractNumeric(pm->offset, &offset);

    Str str = strForAttr(pm->attr) + L": " +
              strForTypeTi(pm->index, Str((char *) pm->offset + cbOffset)) +
              L"<@" + Str(offset) + L">";

    *pcb += sizeof(lfMember) + cbOffset + strlen((char *) &pm->offset[cbOffset]) + 1;

    return str;
}


Str strForAttr(struct CV_fldattr_t a)
{
    static const wchar_t * const mpaccesssz[] =
    {
        L"",
        L"private",
        L"protected",
        L"public"
    };

    static const wchar_t * const mpmpropsz[]  =
    {
        L"",
        L"virtual",
        L"static",
        L"friend",
        L"<intro>",
        L"<pure>",
        L"<intro,pure>"
    };

    Str str = Str(mpaccesssz[a.access]) + Str(mpmpropsz[a.mprop]);

    if (a.pseudo || a.noinherit || a.noconstruct) {
        bool fFirst = true;

        str += L"<";

        if (a.pseudo)       str += strSep(fFirst) + L"pseudo";
        if (a.noinherit)    str += strSep(fFirst) + L"noinherit";
        if (a.noconstruct)  str += strSep(fFirst) + L"noconstruct";

        str += L">";
    }

    return str;
}


Str strForProc(lfProc* pp, Str strBase, Bind bind)
{
    if (bind < bindProc) {
        strBase = L"(" + strBase + L")";
    }

    strBase += strForTypeTi(pp->arglist);

    return strForTypeTi(pp->rvtype, strBase, bindProc);
}


Str strForMFunc(lfMFunc* pf, Str strBase, Bind bind)
{
    if (bind < bindProc) {
        strBase = L"(" + strBase + L")";
    }

    Str str;

    str = strForTypeTi(pf->classtype) + L"::";
    str += strBase + strForTypeTi(pf->arglist);
    str += L"<" + strForTypeTi(pf->thistype, L"this") + L",";
    str += Str((ulong) pf->thisadjust) + L"," + Str(pf->parmcount) + L",";
    str += strForFuncAttr(pf->funcattr) + L">";

    return strForTypeTi(pf->rvtype, str, bindProc);
}


Str strForArgList(lfArgList *pa)
{
    Str str = L"(";

    for (unsigned i = 0; i < pa->count; i++) {
        if (i > 0) {
            str += L", ";
        }

        str += strForTypeTi(pa->arg[i]);
    }

    str += L")";

    return str;
}


Str strForEnumerate(lfEnumerate *pe, size_t *pcb)
{
    ulong value;
    size_t cb = CbExtractNumeric(pe->value, &value);

    return Str((char *) pe->value + cb) + L"=" + Str(value);

    *pcb += sizeof(lfEnumerate) + cb + strlen((char *) &pe->value[cb]) + 1;   // We need this????
}


Str strSep(bool& fFirst, const wchar_t *szFirst, const wchar_t *szRest)
{
    Str str = fFirst ? szFirst : szRest;

    fFirst = false;

    return str;
}


size_t CbExtractNumeric(BYTE *pb, ulong *pul)
{
    ushort leaf = *(ushort *) pb;

    if (leaf < LF_NUMERIC) {
        *pul = leaf;

        return sizeof(leaf);
    }

    switch (leaf) {
        case LF_CHAR:
            *pul = *(char *) pb;
            return sizeof(leaf) + sizeof(char);

        case LF_SHORT:
            *pul = *(short *) pb;
            return sizeof(leaf) + sizeof(short);

        case LF_USHORT:
            *pul = *(ushort *) pb;
            return sizeof(leaf) + sizeof(ushort);

        case LF_LONG:
            *pul = *(long *) pb;
            return sizeof(leaf) + sizeof(long);

        case LF_ULONG:
            *pul = *(ulong *) pb;
            return sizeof(leaf) + sizeof(ulong);

        case LF_QUADWORD:
            return sizeof(leaf) + sizeof(__int64);

        case LF_UQUADWORD:
            return sizeof(leaf) + sizeof(unsigned __int64);
    }

    return 0;
}


bool dumpLines(Mod *);


void dumpLinesSz(DBI *pdbi, const wchar_t *sz)
{
    Mod *pmod = 0;

    if (wcscmp(sz, L"all") == 0) {
        while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
            dumpLines(pmod);
        }

        return;
    }

    if (!pdbi->OpenModW(sz, 0, &pmod))
        return;

    dumpLines(pmod);

    pmod->Close();
}

// Don't warn about the zero sized array in the FSB/SPB/SPO structures

#pragma warning(disable:4200)

bool dumpLines(Mod *pmod)
{
    USES_STACKBUFFER(0x400);

    CB cb;

    if (!pmod->QueryLines(0, &cb)) {
        return false;
    }

    if (cb > 0) {
        PB pbLines = new BYTE[cb];

        if (!pbLines) {
            return false;
        }

        if (!pmod->QueryLines(pbLines, &cb)) {
            return false;
        }

        PB pb = pbLines;
        struct FSB { unsigned short cFile; unsigned short cSeg; unsigned long baseSrcFile[]; };
        FSB* pfsb = (FSB *) pb;
        pb += sizeof(FSB) + sizeof(unsigned long) * pfsb->cFile;
        struct SE { unsigned long start, end; };
        SE* pse = (SE *) pb;
        pb += sizeof(SE) * pfsb->cSeg;
        unsigned short* seg = (unsigned short *) pb;
        pb += sizeof(unsigned short) * pfsb->cSeg;
        wprintf(L"HDR: cFile=%u cSeg=%u\n", pfsb->cFile, pfsb->cSeg);

        for (int ifile = 0; ifile < pfsb->cFile; ifile++) {
            wprintf(L"baseSrcFile[%d]=%04lx\n", ifile, pfsb->baseSrcFile[ifile]);
        }

        for (int iseg = 0; iseg < pfsb->cSeg; iseg++) {
            wprintf(L"%d: start=%04lx end=%04lx seg=%02x\n",
                   iseg, pse[iseg].start, pse[iseg].end, seg[iseg]);
        }

        for (int ifile = 0; ifile < pfsb->cFile; ifile++) {
            PB pb = pbLines + pfsb->baseSrcFile[ifile];
            struct SPB { unsigned short cSeg; unsigned short pad; unsigned long baseSrcLn[]; };

            SPB *pspb = (SPB *) pb;
            pb += sizeof SPB + sizeof(long) * pspb->cSeg;

            SE *pse = (SE *) pb;
            pb += sizeof(SE) * pspb->cSeg;

            wchar_t *Name = GetSZUnicodeFromSZUTF8((char *) pb);
            size_t cbName = (strlen((char *) pb) + 1);

            wprintf(L"  file[%d]: cSeg=%u pad=%02x cbName=%u, Name=%ws\n",
                   ifile, pspb->cSeg, pspb->pad, cbName, Name);

            for (int iseg = 0; iseg < pspb->cSeg; iseg++) {
                wprintf(L"  %d: baseSrcLn=%04lx start=%04lx end=%04lx\n",
                       iseg, pspb->baseSrcLn[iseg], pse[iseg].start, pse[iseg].end);
            }

            for (int iseg = 0; iseg < pspb->cSeg; iseg++) {
                PB pb = pbLines + pspb->baseSrcLn[iseg];
                struct SPO { unsigned short Seg; unsigned short cPair; unsigned long offset[]; };
                SPO* pspo = (SPO *) pb;
                pb += sizeof(SPO) + sizeof(unsigned long) * pspo->cPair;
                unsigned short *linenumber = (unsigned short *) pb;
                wprintf(L"  seg[%d]: Seg=%02x cPair=%u\n", iseg, pspo->Seg, pspo->cPair);
                wprintf(L"   ");

                for (unsigned int ipair = 0; ipair < pspo->cPair; ipair++) {
                    wprintf(L" %4u:%04lx", linenumber[ipair], pspo->offset[ipair]);

                    if (ipair + 1 < pspo->cPair && (ipair & 3) == 3) {
                        wprintf(L"\n   ");
                    }
                }

                wprintf(L"\n");
            }
        }

        fflush(stdout);
    }

    return true;
}

#pragma warning(default:4200)

bool checkAddrsToPublics(DBI *pdbi)
{
    bool fOK = true;
    GSI* pgsi;

    if (!pdbi->OpenPublics(&pgsi))
        return false;

    for (int dib = 0; dib <= 1; dib++) {
        PB pbSym = 0;

        while (pbSym = pgsi->NextSym(pbSym)) {
            DATASYM32 *p = (DATASYM32 *) pbSym;

            if (SZSYMIDX(p->rectyp) == S_PUB32) {
                OFF off;
                PB pbNearest = pgsi->NearestSym(p->seg, p->off + dib, &off);

                if (!pbNearest) {
                    wprintf(L"none nearest!\n");
                    fOK = false;
                }
                else if (pbNearest != pbSym || off != dib) {
                    char szName[1024];

                    ST stName = (ST) p->name;
                    memcpy(szName, stName + 1, cbForSt(stName) - 1);
                    szName[cbForSt(stName) - 1] = 0;

                    wprintf(L"not nearest: %hs (%lx)\n", szName, off);

                    fOK = false;
                }
            }
            else {
                wprintf(L"non-public!\n");
                fOK = false;
            }
        }
    }

    return fOK;
}


// A stripped down version of the corresponding routine from dumpbin
void
DumpSectionHeader (
    WORD i,
    PIMAGE_SECTION_HEADER Sh
    )
{
    const wchar_t *name;
    DWORD li, lj;
    WORD memFlags;

    wprintf(L"\n"
           L"SECTION HEADER #%hX\n"
           L"%8.8hs name",
           i,
           Sh->Name);

    wprintf(L"\n"
           L"%8lX %s\n"
           L"%8lX virtual address\n"
           L"%8lX size of raw data\n"
           L"%8lX file pointer to raw data\n"
           L"%8lX file pointer to relocation table\n"
           L"%8lX file pointer to line numbers\n"
           L"%8hX number of relocations\n"
           L"%8hX number of line numbers\n"
           L"%8lX flags\n",
           Sh->Misc.PhysicalAddress,
           L"virtual size",
           Sh->VirtualAddress,
           Sh->SizeOfRawData,
           Sh->PointerToRawData,
           Sh->PointerToRelocations,
           Sh->PointerToLinenumbers,
           Sh->NumberOfRelocations,
           Sh->NumberOfLinenumbers,
           Sh->Characteristics);

    memFlags = 0;

    li = Sh->Characteristics;

    // Clear the padding bits

    li &= ~0x00700000;

    for (lj = 0L; li; li = li >> 1, lj++) {
        if (li & 1) {
            switch ((li & 1) << lj) {
                case IMAGE_SCN_TYPE_NO_PAD  : name = L"No Pad"; break;

                case IMAGE_SCN_CNT_CODE     : name = L"Code"; break;
                case IMAGE_SCN_CNT_INITIALIZED_DATA : name = L"Initialized Data"; break;
                case IMAGE_SCN_CNT_UNINITIALIZED_DATA : name = L"Uninitialized Data"; break;

                case IMAGE_SCN_LNK_OTHER    : name = L"Other"; break;
                case IMAGE_SCN_LNK_INFO     : name = L"Info"; break;
                case IMAGE_SCN_LNK_REMOVE   : name = L"Remove"; break;
                case IMAGE_SCN_LNK_COMDAT   : name = L"Communal"; break;
                case IMAGE_SCN_LNK_NRELOC_OVFL : name = L"Extended relocations"; break;

                case IMAGE_SCN_MEM_DISCARDABLE: name = L"Discardable"; break;
                case IMAGE_SCN_MEM_NOT_CACHED: name = L"Not Cached"; break;
                case IMAGE_SCN_MEM_NOT_PAGED: name = L"Not Paged"; break;
                case IMAGE_SCN_MEM_SHARED   : name = L"Shared"; break;
                case IMAGE_SCN_MEM_EXECUTE  : name = L""; memFlags |= 1; break;
                case IMAGE_SCN_MEM_READ     : name = L""; memFlags |= 2; break;
                case IMAGE_SCN_MEM_WRITE    : name = L""; memFlags |= 4; break;

                case IMAGE_SCN_MEM_FARDATA  : name = L"Far Data"; break;
                case IMAGE_SCN_MEM_PURGEABLE: name = L"Purgeable or 16-Bit"; break;
                case IMAGE_SCN_MEM_LOCKED   : name = L"Locked"; break;
                case IMAGE_SCN_MEM_PRELOAD  : name = L"Preload"; break;

                default : name = L"RESERVED - UNKNOWN";
            }

            if (*name) {
                wprintf(L"         %s\n", name);
            }
        }
    }

    // print alignment
    switch (Sh->Characteristics & 0x00700000) {
        default:                      name = L"(no align specified)"; break;
        case IMAGE_SCN_ALIGN_1BYTES:  name = L"1 byte align";  break;
        case IMAGE_SCN_ALIGN_2BYTES:  name = L"2 byte align";  break;
        case IMAGE_SCN_ALIGN_4BYTES:  name = L"4 byte align";  break;
        case IMAGE_SCN_ALIGN_8BYTES:  name = L"8 byte align";  break;
        case IMAGE_SCN_ALIGN_16BYTES: name = L"16 byte align"; break;
        case IMAGE_SCN_ALIGN_32BYTES: name = L"32 byte align"; break;
        case IMAGE_SCN_ALIGN_64BYTES: name = L"64 byte align"; break;
    }

    wprintf(L"         %s\n", name);

    if (memFlags) {
        switch (memFlags) {
            case 1 : name = L"Execute Only"; break;
            case 2 : name = L"Read Only"; break;
            case 3 : name = L"Execute Read"; break;
            case 4 : name = L"Write Only"; break;
            case 5 : name = L"Execute Write"; break;
            case 6 : name = L"Read Write"; break;
            case 7 : name = L"Execute Read Write"; break;
        }

        wprintf(L"         %s\n", name);
    }
}


bool dumpFpo (DBI *pdbi)
{
    Dbg *pdbg;

    static const wchar_t * const szFrameTypes[] = {L"fpo", L"trap", L"tss", L"std"};

    if (!pdbi->OpenDbg(dbgtypeFPO, &pdbg))
        return false;

    long cData = pdbg->QuerySize();

    if (cData > 0) {
        wprintf(L"\nFPO Data (%ld):\n", cData);

        _putws(L"                                       Use Has  Frame\n"
               L" Address  Proc Size   Locals   Prolog  BP  SEH  Type   Params\n");

        FPO_DATA data ;
        for (pdbg->Reset(); pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            wprintf(L"%08X   %8X %8X %8X   %c   %c   %4s     %4X\n",
                                data.ulOffStart,
                                data.cbProcSize,
                                data.cdwLocals,
                                data.cbProlog,
                                data.fUseBP ? L'Y' : L'N',
                                data.fHasSEH ? L'Y' : L'N',
                                szFrameTypes[data.cbFrame],
                                data.cdwParams * 4);
        }
    }

    pdbg->Close();

    return true;
}


bool dumpPdata (DBI *pdbi)
{
    Dbg *pdbg;
    DWORD ife = 0;

    if (!pdbi->OpenDbg(dbgtypeException, &pdbg))
        return false;

    long cData = pdbg->QuerySize();

    if (cData > 0) {
        wprintf(L"\nFunction Table (%lu)\n\n", cData);
        wprintf(L"         Begin     End       PrologEnd\n\n");

        IMAGE_FUNCTION_ENTRY data;
        for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            wprintf(L"%08lX %08lX  %08lX  %08lX\n",
                ife * sizeof(IMAGE_FUNCTION_ENTRY),
                data.StartingAddress,
                data.EndingAddress,
                data.EndOfPrologue);
            ife++;
        }
    }

    pdbg->Close();

    return true;
}


bool dumpXFixup(DBI *pdbi)
{
    Dbg *pdbg;

    if (!pdbi->OpenDbg(dbgtypeFixup, &pdbg))
        return false;

    long cData = pdbg->QuerySize();

    if (cData > 0) {
        wprintf(L"\nFixup Data (%ld):\n\n", cData);

        _putws(L"    Type        Rva        RvaTarget\n"
               L"    ----  ----  --------   --------\n");

        XFIXUP data;
        for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            wprintf(L"    %04X  %04X  %08X   %08X\n",
            data.wType, data.wExtra, data.rva, data.rvaTarget);
        }
    }

    pdbg->Close();

    return true;
}


bool dumpOmap(DBI *pdbi, bool fFromSrc)
{
    Dbg *pdbg;

    if (!pdbi->OpenDbg(fFromSrc ? dbgtypeOmapFromSrc : dbgtypeOmapToSrc, &pdbg))
        return false;

    long cData = pdbg->QuerySize();

    if (cData > 0) {
        wprintf(L"\nOMAP Data (%s_SRC) - (%ld):\n\n",
            fFromSrc ? L"FROM" : L"TO", cData);

        _putws(L"    Rva        RvaTo\n"
               L"    --------   --------\n");

        OMAP data;
        for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            wprintf(L"    %08X   %08X\n", data.rva, data.rvaTo);
        }
    }

    pdbg->Close();

    return true;
}


bool dumpSectionHdr (DBI *pdbi)
{
    Dbg *pdbg;

    if (!pdbi->OpenDbg(dbgtypeSectionHdr, &pdbg))
        return false;

    long cData = pdbg->QuerySize();

    if (cData > 0) {
        IMAGE_SECTION_HEADER data;
        for (WORD iHdr = 1; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            DumpSectionHeader(iHdr, &data);
            iHdr++;
        }
    }
    pdbg->Close();
    return true;
}

bool dumpNamemap(PDB *ppdb, bool fUseDecimal)
{
    NameMap *pmap =  NULL;
    EnumNameMap *pe = NULL;

    if (NameMap::open(ppdb, false, &pmap) &&
        pmap->getEnumNameMap((Enum **)&pe)) {

        const wchar_t *szFmt = L"0x%08X: %s\n";

        if (fUseDecimal) {
            szFmt = L"%08d: %s\n";
        }

        while(pe->next())  {
            SZ_CONST szName;
            NI ni;

            pe->get(&szName, &ni);

            wchar_t wsz[4096];
            _GetSZUnicodeFromSZUTF8(szName, wsz, 4096);

            wprintf(szFmt, ni, wsz);
        }
    }
    else {
        wprintf(L"Problem loading namemap\n");
    }

    if (pe) pe->release();
    if (pmap) pmap->close();

    return true;
}


bool dumpSrcFormat(BYTE *pb, CB cb)
{
    static const GUID guidSourceHashMD5 = { 0x406ea660, 0x64cf, 0x4c82, 0xb6, 0xf0, 0x42, 0xd4, 0x81, 0x72, 0xa7, 0x99 };
    static const GUID guidSourceHashSHA1 = { 0xff1816ec, 0xaa5e, 0x4d10, 0x87, 0xf7, 0x6f, 0x49, 0x63, 0x83, 0x34, 0x60 };

    struct SrcFormat {
        GUID language;
        GUID languageVendor;
        GUID documentType;
        GUID algorithmId;
        DWORD checkSumSize;
        DWORD sourceSize;
        // followed by 'checkSumSize' bytes of checksum
        // followed by 'sourceSize' source bytes
    };

    if (cb < sizeof(SrcFormat)) {
        // This can't be SrcFormat structure
        return false;
    }

    SrcFormat *pSrcFormat = reinterpret_cast<SrcFormat *>(pb);
    bool fMD5  = (memcmp(&(pSrcFormat->algorithmId), &guidSourceHashMD5,  sizeof(GUID) ) == 0);
    bool fSHA1 = (memcmp(&(pSrcFormat->algorithmId), &guidSourceHashSHA1, sizeof(GUID) ) == 0);

    if (fMD5 || fSHA1) {
        wprintf(L"Raw Data \n\t");

        for (CB i = 0; i < cb; i++) {
            wprintf(L" %02x", pb[i]);
            if (i % 0x10 == 0xF) {
                wprintf(L"\n\t");
            }
        }

        if (fMD5) {
            wprintf(L"\nChecksum = MD5 -");
        }
        else {
            wprintf(L"\nChecksum = SHA1 -");
        }

        PB pbChksum = pb + sizeof(SrcFormat);
        for (unsigned i = 0; i < pSrcFormat->checkSumSize; i++) {
            wprintf(L" %02x", pbChksum[i]);
        }

        wprintf(L"\n");
    }

    return (fMD5 || fSHA1);
}


bool dumpSrcHeader(PDB *ppdb)
{
    Src *       psrc = NULL;
    NameMap *   pnamemap = NULL;
    EnumSrc *   pe = NULL;
    bool        fRet = false;
    BYTE *      pb = new BYTE[ 4096 ];
    size_t      cb = pb ? 4096 : 0;


    if (ppdb->QueryInterfaceVersion() >= PDBIntv61 &&
        ppdb->OpenSrc(&psrc) &&
        NameMap::open(ppdb, false, &pnamemap) && psrc->GetEnum(&pe))
    {
        SrcHeaderBlock shb;

        if (psrc->GetHeaderBlock(shb)) {
            wprintf(L"Header = {\n");
            wprintf(L"\tver =   %d\n", shb.ver);
            wprintf(L"\tcb  =   %d\n", shb.cb);
            wprintf(L"\tage =   %d\n", shb.age);
            wprintf(L"\tft  = 0x%016I64x", *(__int64*)&shb.ft);

            SYSTEMTIME st;

            if (::FileTimeToSystemTime(PFILETIME(&shb.ft), &st)) {
                wprintf(L" (%d-%02d-%02d@%02d:%02d:%02d.%03d UTC)",
                    st.wYear, st.wMonth, st.wDay,
                    st.wHour, st.wMinute, st.wSecond, st.wMilliseconds
                    );
            }

            _putws(L"\n}\n");
        }

        fRet = true;
        while (pe->next()) {
            PCSrcHeaderOut  p;
            pe->get(&p);
            wprintf(L"         cb = %d (0x%08X)\n", p->cb, p->cb);
            wprintf(L"        ver = %d (0x%08X)\n", p->ver, p->ver);
            wprintf(L"        sig = 0x%08X\n", p->sig);
            wprintf(L"   cbSource = %d (0x%08X)\n", p->cbSource, p->cbSource);
            SZ_CONST  sz;
            wprintf(L"     niFile = 0x%08X (%hs)\n", p->niFile, pnamemap->isValidNi(p->niFile) ? (pnamemap->getName(p->niFile,&sz), sz) : "invalidNI");
            wprintf(L"      niObj = 0x%08X (%hs)\n", p->niObj, pnamemap->isValidNi(p->niObj) ? (pnamemap->getName(p->niObj,&sz), sz) : "invalidNI");
            wprintf(L"     niVirt = 0x%08X (%hs)\n", p->niVirt, pnamemap->isValidNi(p->niVirt) ? (pnamemap->getName(p->niVirt,&sz), sz) : "invalidNI");
            wprintf(L"srcCompress = %d\n", p->srccompress);
            wprintf(L"   fVirtual = %s\n\n", p->fVirtual ? L"true" : L"false");

            if (p->cbSource > cb) {
                delete [] pb;
                cb = p->cbSource;
                pb = new BYTE[cb];
            }

            if (pb) {
                if (psrc->GetData(p, pb)) {
                    if (!dumpSrcFormat(pb, p->cbSource)) {
                        dumpText(p->cbSource, pb);
                    }
                }
            }
        }
    }

    if (pe) pe->release();
    if (psrc) psrc->Close();
    if (pnamemap) pnamemap->close();
    if (pb) delete [] pb;

    return fRet;
}


void dumpText(size_t cb, BYTE *pb)
{
    BYTE *pbMax = pb + cb;
    int   cLine = 1;

    while (pb < pbMax) {
        wprintf(L"%d\t", cLine++);

        int ch;

        for (; pb < pbMax; pb++) {
            ch = unsigned(*pb);

            if (ch != '\n' && ch != '\r') {
                putchar(ch);
            }

            if (ch == '\n') {
                pb++;
                break;
            }
        }

        _putws(L"");
    }

    _putws(L"");
}


extern "C" void failAssertionFunc(SZ_CONST szAssertFile, SZ_CONST szFunction, int line, SZ_CONST szCond)
{
    fwprintf(
        stderr,
        L"assertion failure:\n\tfile: %hs,\n\tfunction: %hs,\n\tline: %d,\n\tcondition(%hs)\n\n",
        szAssertFile,
        szFunction,
        line,
        szCond
        );

    fflush(stderr);

    __debugbreak();
}
