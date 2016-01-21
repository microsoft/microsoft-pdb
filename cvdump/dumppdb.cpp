/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

#include "cvdump.h"

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "_winnt2.h"

#ifndef IMAGE_FILE_MACHINE_POWERPCBE
#define IMAGE_FILE_MACHINE_POWERPCBE         0x01F2  // IBM PowerPC Big-Endian
#endif

// Local include files

#include "utf8.h"
#include "mdalign.h"

// Cor include files
#include "corhdr.h"

#include "safestk.h"
#include "szst.h"

// module variables
bool fUtf8Symbols;

// LANGAPI shared files

#define dassert assert
#define precondition    dassert
#define postcondition   dassert

#include "map_t.h"

DWORD dwMachine;
extern WORD CVDumpMachineType;

enum DbgInfoKind {
    DIK_xScopeExport,
    DIK_xScopeImport,
    DIK_FuncTokenMap,
    DIK_TypeTokenMap,
    DIK_MergedAsmIn,
};

void (*pfn)(size_t, PB);

// map support for src file names

class CWsz {
    wchar_t *m_wsz;

    void Dealloc() {
        if (m_wsz) {
            free(m_wsz);
            m_wsz = NULL;
        }
    }

public:
    CWsz() {
        m_wsz = NULL;
    }

    CWsz(_In_z_ wchar_t * wsz) {
        m_wsz = _wcsdup(wsz);
    }

    CWsz(const CWsz & wsz) {
        m_wsz = _wcsdup(wsz.m_wsz);
    }

    ~CWsz() { Dealloc(); }

    CWsz & operator=(const CWsz & wsz) {
        Dealloc();
        m_wsz = _wcsdup(wsz.m_wsz);
        return *this;
    }

    bool operator==(const CWsz & wsz) {
        return _wcsicmp(m_wsz, wsz.m_wsz) == 0;
    }

    bool operator==(const wchar_t *wsz) {
        return _wcsicmp(m_wsz, wsz) == 0;
    }

    operator wchar_t *() {
        return m_wsz;
    }

    operator LHASH() const {
        if (m_wsz) {
            wchar_t *wszUpper = _wcsdup(m_wsz);
            _wcsupr_s(wszUpper, wcslen(wszUpper));

            LHASH lhash = LHASH(SigForPbCb(PB(wszUpper), wcslen(wszUpper) * sizeof(wchar_t), 0));

            free(wszUpper);

            return lhash;
        }

        return 0;
    }

    static int __cdecl Wcmp(const void * pv1, const void * pv2) {
        const CWsz *pcwsz1 = reinterpret_cast<const CWsz *>(pv1);
        const CWsz *pcwsz2 = reinterpret_cast<const CWsz *>(pv2);

        return _wcsicmp(pcwsz1->m_wsz, pcwsz2->m_wsz);
    }
};

typedef HashClass<CWsz,hcCast>      HcWsz;
typedef Map<CWsz, BYTE, HcWsz>      FileMap;
typedef EnumMap<CWsz, BYTE, HcWsz>  EnumFileMap;
typedef Array<CWsz>                 RgCWsz;

void DumpGSI(GSI *pgsi)
{
    cchIndent = 0;

    BYTE *pb = NULL;

    while (pb = pgsi->NextSym(pb)) {
        DumpOneSymC7(NULL, pb, 0xFFFFFFFF);
    }

    StdOutPutc(L'\n');
}


void DumpPdbModules(DBI *pdbi)
{
    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (!pmod->QueryImod(&imod)) {
            continue;
        }

        if ((iModToList != 0) && (imod != iModToList)) {
            continue;
        }

        StdOutPrintf(L"%04X", imod);

        wchar_t wszFile[PDB_MAX_PATH];
        long cb = _countof(wszFile);

        if (!pmod->QueryFileW(wszFile, &cb)) {
            StdOutPuts(L" *");
        }

        else {
            StdOutPrintf(L" \"%s\"", wszFile);
        }

        wchar_t wszName[PDB_MAX_PATH];
        cb = _countof(wszName);

        if (!pmod->QueryNameW(wszName, &cb)) {
            StdOutPuts(L" *");
        }

        else if (wcscmp(wszFile, wszName) == 0) {
            // If file name matches module name that this isn't a library
        }

        else {
            StdOutPrintf(L" \"%s\"", wszName);
        }

        StdOutPutc(L'\n');
    }
}

const wchar_t *SzErrorFromEc(PDBErrors, _Out_opt_cap_(cchEc) wchar_t *szEc, size_t cchEc);

void DumpPdbPublics(PDB * ppdb, DBI *pdbi)
{
    GSI *pgsi;

    if (!pdbi->OpenPublics(&pgsi)) {
        wchar_t szEc[11];

        PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(NULL, 0));

        StdOutPrintf(L"DBI::OpenPublics failed (%s)\n", SzErrorFromEc(ec, szEc, _countof(szEc)));
        return;
    }

    DumpGSI(pgsi);

    pgsi->Close();
}


void DumpPdbTypes(PDB *ppdb, DBI *pdbi, bool fId)
{
    for (unsigned itsm = 0; itsm < 256; itsm++) {
        TPI *ptpi = 0;

        if (!fId && !pdbi->QueryTypeServer((ITSM) itsm, &ptpi) && itsm) {
            continue;
        }

        if (!ptpi) {
            BOOL f = fId ? ppdb->OpenIpi(pdbRead pdbGetRecordsOnly, &ptpi)
                         : ppdb->OpenTpi(pdbRead pdbGetRecordsOnly, &ptpi);

            if (!f) {
                wchar_t wszErr[1024];
                PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(wszErr, 1024));

                wchar_t szEc[32];

                StdOutPrintf(L"Error on OpenTpi: '%s' (%d)\n", SzErrorFromEc(ec, szEc, _countof(szEc)), ec);
                return;
            }

            if (!ptpi) {
                StdOutPrintf(L"Error OpenTpi return true, but doesn't give us a TPI pointer!\n");
                assert(false);
                return;
            }
        }

        fSupportsQueryUDT = ptpi->SupportQueryTiForUDT() != false;

        if (!fId && ptpi->fIs16bitTypePool()) {
            StdOutPuts(L"*** Converting 16-bit types to 32-bit equivalents\n\n");
        }

        TI tiMin = ptpi->QueryTiMin();
        TI tiMac = ptpi->QueryTiMac();

        for (TI ti = tiMin; ti < tiMac; ti++) {
            TI tiT = (itsm << 24) | ti;

            BYTE *pb;

            BOOL fT = ptpi->QueryPbCVRecordForTi(tiT, &pb);

            if (!fT) {
                PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(NULL, 0));

                wchar_t szName[_MAX_PATH] = L"";

                ppdb->QueryPDBNameExW(szName, _countof(szName));

                wchar_t szEc[32];

                StdOutPrintf(L"Error on QueryPbCVRecordForTI(0x%x): '%s' (%d) on '%s'\n",
                             tiT,
                             SzErrorFromEc(ec, szEc, _countof(szEc)),
                             ec,
                             szName);

                return;
            }

            DumpTypRecC7(tiT, *(unsigned short *) pb, pb + sizeof(unsigned short), ptpi, ppdb);
        }

        if (fId) {
            break;
        }

        // Don't close TPIs opened via DBI::QueryTypeServer()
    }
}


void DumpPdbTypeWarnDuplicateUDTs(PDB *ppdb, DBI *pdbi)
{
    TPI *ptpi = NULL;

    if (!ppdb->OpenTpi(pdbRead pdbGetRecordsOnly, &ptpi)) {
        PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(NULL, 0));

        wchar_t szEc[32];

        StdOutPrintf(L"Error on OpenTpi: '%s' (%d)\n", SzErrorFromEc(ec, szEc, _countof(szEc)), ec);
        return;
    }

    if (!ptpi) {
        StdOutPrintf(L"Error OpenTpi return true, but doesn't give us a TPI pointer!\n");
        assert(false);
        return;
    }

    fSupportsQueryUDT = ptpi->SupportQueryTiForUDT() != false;

    if (!fSupportsQueryUDT) {
        wchar_t wszPdbName[PDB_MAX_PATH];

        StdOutPrintf(L"This PDB '%s' does not support QueryUDT\n", ppdb->QueryPDBNameExW(wszPdbName, PDB_MAX_PATH));
        return;
    }

    TI tiMin = ptpi->QueryTiMin();
    TI tiMac = ptpi->QueryTiMac();

    for (TI ti = tiMin; ti < tiMac; ti++) {
        BYTE *pb;

        BOOL fT = ptpi->QueryPbCVRecordForTi(ti, &pb);

        if (!fT) {
            PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(NULL, 0));

            wchar_t szName[_MAX_PATH] = L"";

            ppdb->QueryPDBNameExW(szName, _countof(szName));

            wchar_t szEc[32];

            StdOutPrintf(L"Error on QueryPbCVRecordForTI(0x%x): '%s' (%d) on '%s'\n",
                         ti,
                         SzErrorFromEc(ec, szEc, _countof(szEc)),
                         ec,
                         szName);

            return;
        }


        TYPPTR ptype = reinterpret_cast<TYPPTR>(pb);
        unsigned leaf = ptype->leaf;

        bool fIsESU = false;
        const unsigned char *szName = NULL;

        switch (leaf) {
            CV_prop_t prop;

            case LF_CLASS:
            case LF_STRUCTURE: {
                plfStructure plf = reinterpret_cast<plfStructure>(&ptype->leaf);

                prop = plf->property;

                fIsESU = !prop.fwdref;

                if (fIsESU) {
                    szName = plf->data + SkipNumeric(plf->data);
                }
                break;
            }

            case LF_UNION: {
                plfUnion plf = reinterpret_cast<plfUnion>(&ptype->leaf);

                prop = plf->property;

                fIsESU = !prop.fwdref;

                if (fIsESU) {
                    szName = plf->data + SkipNumeric(plf->data);
                }
                break;
            }

        }

        if (fIsESU) {
            TI tiUdt;

            if (ptpi->QueryTiForUDT(reinterpret_cast<const char*>(szName), true, &tiUdt)) {
                if (tiUdt != ti) {
                    StdOutPrintf(L"WARNING: UDT mismatch for %S\n<<<<<<\n", szName);

                    DumpTypRecC7(ti, *(unsigned short *) pb, pb + sizeof(unsigned short), ptpi, ppdb);

                    StdOutPuts(L"******\n");

                    ptpi->QueryPbCVRecordForTi(tiUdt, &pb);
                    DumpTypRecC7(tiUdt, *(unsigned short *) pb, pb + sizeof(unsigned short), ptpi, ppdb);

                    StdOutPuts(L">>>>>>\n\n");
                }
            }
        }
    }
}


void DumpPdbCoffSymRVA(DBI *pdbi)
{
    size_t cbBuf = 0x4000;

    BYTE *pb = (BYTE *) malloc(cbBuf);

    if (pb == NULL) {
        StdOutPuts(L"malloc failed\n");

        return;
    }

    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cch = _countof(szName);

        if (pmod->QueryNameW(szName, &cch)) {
            StdOutPrintf(L"** Module: \"%s\"", szName);

            wchar_t wszLibrary[PDB_MAX_PATH];
            cch = _countof(wszLibrary);

            if (!pmod->QueryFileW(wszLibrary, &cch)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        DWORD cb;

        if (!pmod->QueryCoffSymRVAs(NULL, static_cast<DWORD *>(&cb))) {
            StdOutPuts(L"Mod::QueryCoffSymRVAs failed\n");

            continue;
        }

        if (cb == 0) {
            continue;
        }

        if ((size_t) cb > cbBuf) {
            BYTE *pb2 = (BYTE *) realloc(pb, (size_t) cb * 2);

            if (pb2 == NULL) {
                StdOutPuts(L"realloc failed\n");

                continue;
            }

            pb = pb2;
            cbBuf = cb * 2;
        }

        if (!pmod->QueryCoffSymRVAs(pb, static_cast<DWORD *>(&cb))) {
            StdOutPuts(L"Mod::QueryCoffSymRVAs failed\n");

            continue;
        }

        cchIndent = 0;

        BYTE *pbT = pb;
        const BYTE *pbEnd = pb + cb;

        for (int i = 0; pbT < pbEnd; pbT += sizeof(DWORD), i++) {
            if ((i % 8) == 0) {
                if (i == 0) {
                    StdOutPuts(L"    ");
                } else {
                    StdOutPuts(L"\n    ");
                }
            }

            StdOutPrintf(L" %08X", *(UNALIGNED DWORD *) pbT);
        }

        StdOutPuts(L"\n\n");
    }

    free(pb);
}


void DumpPdbSyms(DBI *pdbi)
{
    size_t cbBuf = 0x4000;

    BYTE *pb = (BYTE *) malloc(cbBuf);

    if (pb == NULL) {
        StdOutPuts(L"malloc failed\n");

        return;
    }

    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cb = _countof(szName);

        if (pmod->QueryNameW(szName, &cb)) {
            StdOutPrintf(L"** Module: \"%s\"", szName);

            wchar_t wszLibrary[PDB_MAX_PATH];
            cb = _countof(wszLibrary);

            if (!pmod->QueryFileW(wszLibrary, &cb)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        if (!pmod->QuerySymbols(NULL, &cb)) {
            StdOutPuts(L"Mod::QuerySymbols failed\n");

            continue;
        }

        if (cb == 0) {
            continue;
        }

        if ((size_t) cb > cbBuf) {
            BYTE *pb2 = (BYTE *) realloc(pb, (size_t) cb * 2);

            if (pb2 == NULL) {
                StdOutPuts(L"realloc failed\n");

                continue;
            }

            pb = pb2;
            cbBuf = cb * 2;
        }

        if (!pmod->QuerySymbols(pb, &cb)) {
            StdOutPuts(L"Mod::QuerySymbols failed\n");

            continue;
        }

        cchIndent = 0;

        const BYTE *pbTmp;
        const BYTE *pbEnd;

        for (pbEnd = pb + cb, pbTmp = pb + sizeof(long);        // skip signature
             pbTmp < pbEnd;
             pbTmp = (BYTE *) NextSym((SYMTYPE *) pbTmp)
            ) {
            DumpOneSymC7(pmod, pbTmp, DWORD(pbTmp - pb));
        }

        StdOutPutc(L'\n');
    }

    free(pb);
}


void DumpPdbGlobals(PDB *ppdb, DBI *pdbi)
{
    GSI *pgsi;

    if (!pdbi->OpenGlobals(&pgsi)) {
        PDBErrors ec = PDBErrors(ppdb->QueryLastErrorExW(NULL, 0));

        wchar_t szEc[11];

        StdOutPrintf(L"DBI::OpenGlobals failed (%s)\n", SzErrorFromEc(ec, szEc, _countof(szEc)));

        return;
    }

    DumpGSI(pgsi);

    pgsi->Close();
}


void DumpPdbInlineeLines(DBI *pdbi)
{
    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cb = _countof(szName);

        if (pmod->QueryNameW(szName, &cb)) {
            wchar_t wszLibrary[PDB_MAX_PATH];
            cb = _countof(wszLibrary);

            StdOutPrintf(L"** Module: \"%s\"", szName);

            if (!pmod->QueryFileW(wszLibrary, &cb)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        if (!pmod->QueryInlineeLines(0, NULL, (DWORD *)&cb)) {
            StdOutPuts(L"Mod::QueryInlineeLines failed\n\n");

            continue;
        }

        if (cb == 0) {
            continue;
        }

        PB pb = (PB) malloc(cb);

        if (pb == NULL) {
            StdOutPuts(L"malloc failed\n");

            return;
        }

        if (!pmod->QueryInlineeLines(cb, pb, (DWORD *)&cb)) {
            StdOutPuts(L"Mod::QueryInlineeLines failed\n\n");

            continue;
        }

        DumpModInlineeSourceLines((DWORD)cb, pb);

        free(pb);

        StdOutPutc(L'\n');
    }
}

void DumpPdbLines(DBI *pdbi, bool fIL)
{
    size_t cbBuf = 0x4000;

    BYTE *pb = (BYTE *) malloc(cbBuf);

    if (pb == NULL) {
        StdOutPuts(L"malloc failed\n");

        return;
    }

    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cb = _countof(szName);

        if (pmod->QueryNameW(szName, &cb)) {
            wchar_t wszLibrary[PDB_MAX_PATH];
            cb = _countof(wszLibrary);

            StdOutPrintf(L"** Module: \"%s\"", szName);

            if (!pmod->QueryFileW(wszLibrary, &cb)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        DWORD flags;

        if (fIL) {
            if (!pmod->QueryILLineFlags(&flags)) {
                flags = 0;
            }
        } else {
            if (!pmod->QueryLineFlags(&flags)) {
                flags = 0;
            }
        }

        EnumLines *penum;

        if (fIL) {
            if (!pmod->GetEnumILLines(&penum)) {
                StdOutPuts(L"Mod::GetEnumILLines failed\n\n");

                continue;
            }
        } else {
            if (!pmod->GetEnumLines(&penum)) {
                StdOutPuts(L"Mod::GetEnumLines failed\n\n");

                continue;
            }
        }

        while (penum->next()) {
            DWORD fileId;
            DWORD offsetBase;
            WORD seg;
            DWORD cb;
            DWORD cLines;

            if (!penum->getLinesColumns(&fileId,
                                        &offsetBase,
                                        &seg,
                                        &cb,
                                        &cLines,
                                        NULL,
                                        NULL)) 
            {
                StdOutPrintf(L"Error: Line number corrupted: invalid file id %d\n", fileId);
                continue;
            }

            wchar_t szName[_MAX_PATH];
            DWORD cchName = _MAX_PATH;
            DWORD checksumtype;
            BYTE rgbChecksum[256];
            DWORD cbChecksum = sizeof(rgbChecksum);

            if (!pmod->QueryFileNameInfo(fileId,
                                    szName,
                                    &cchName,
                                    &checksumtype,
                                    rgbChecksum,
                                    &cbChecksum))
            {
                StdOutPrintf(L"Error: Line number corrupted: invalid file id %d\n", fileId);
                wcscpy_s(szName, _countof(szName), L"<Unknown>");
                cbChecksum = 0;
            }

            StdOutPrintf(L"  %s (", szName);

            switch (checksumtype) {
                case CHKSUM_TYPE_NONE :
                    StdOutPuts(L"None");
                    break;

                case CHKSUM_TYPE_MD5 :
                    StdOutPuts(L"MD5");
                    break;

                case CHKSUM_TYPE_SHA1 :
                    StdOutPuts(L"SHA1");
                    break;

                case CHKSUM_TYPE_SHA_256 :
                    StdOutPuts(L"SHA_256");
                    break;

                default :
                    StdOutPrintf(L"0x%X", checksumtype);
                    break;
            }

            if (cbChecksum != 0) {
                StdOutPuts(L": ");

                for (DWORD i = 0; i < cbChecksum; i++ ) {
                    StdOutPrintf(L"%02X", rgbChecksum[i]);
                }
            }

            DWORD   offsetMac = offsetBase + cb;

            StdOutPrintf(L"), %04X:%08X-%08X, line/addr pairs = %u\n",
                         seg,
                         offsetBase,
                         offsetMac,
                         cLines);

            if (cLines > 0) {
                CV_Line_t *pLines = new CV_Line_t[cLines];
                CV_Column_t *pColumns = new CV_Column_t[cLines];

                if (!penum->getLinesColumns(&fileId,
                                       &offsetBase,
                                       &seg,
                                       &cb,
                                       &cLines,
                                       pLines,
                                       (flags & CV_LINES_HAVE_COLUMNS) ? pColumns : 0))
                {
                    StdOutPuts(L"Error: Line/column number corrupted\n");
                    continue;
                }

                DWORD clinesOutofBounds = 0;

                for (DWORD i = 0; i < cLines; i++) {
                    if (flags & CV_LINES_HAVE_COLUMNS) {
                        if ((i % 2) == 0) {
                            StdOutPutc(L'\n');
                        }

                        if (pColumns[i].offColumnEnd != 0) {
                            StdOutPrintf(L"  %5u:%-5u-%5u:%-5u %08X",
                                         pLines[i].linenumStart,
                                         pColumns[i].offColumnStart,
                                         pLines[i].linenumStart + pLines[i].deltaLineEnd,
                                         pColumns[i].offColumnEnd,
                                         pLines[i].offset + offsetBase);
                        }

                        else {
                            StdOutPrintf(
                                    (pLines[i].linenumStart == 0xfeefee || pLines[i].linenumStart == 0xf00f00)
                                    ? L"  %x:%-5u            %08X" : L"  %5u:%-5u            %08X",
                                    pLines[i].linenumStart,
                                    pColumns[i].offColumnStart,
                                    pLines[i].offset + offsetBase);
                        }
                    }

                    else {
                        if ((i % 4) == 0) {
                            StdOutPutc(L'\n');
                        }

                        StdOutPrintf(
                                (pLines[i].linenumStart == 0xfeefee || pLines[i].linenumStart == 0xf00f00)
                                ? L"  %x %08X" : L"  %5u %08X",
                                pLines[i].linenumStart, pLines[i].offset + offsetBase);
                    }

                    // Check to see if we need to warn about out-of-bounds line numbers

                    if (pLines[i].offset + offsetBase >= offsetMac) {
                        clinesOutofBounds++;
                    }
                }

                if (clinesOutofBounds != 0) {
                    StdOutPrintf(
                        L"\n <<<< WARNING >>>> %u line/addr pairs are out of bounds!",
                        clinesOutofBounds );
                }

                StdOutPutc(L'\n');

                delete [] pLines;
                delete [] pColumns;
            }

            StdOutPutc(L'\n');
        }
    }

    free(pb);
}


void DumpPdbSrcFiles(DBI *pdbi)
{
    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cb = _countof(szName);

        if (pmod->QueryNameW(szName, &cb)) {
            wchar_t wszLibrary[PDB_MAX_PATH];
            cb = _countof(wszLibrary);

            StdOutPrintf(L"** Module: \"%s\"", szName);

            if (!pmod->QueryFileW(wszLibrary, &cb)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        DWORD fileId = 0;

        for (;;) {
            wchar_t szName[_MAX_PATH];
            BYTE    rgbChecksum[256];

            DWORD len = _MAX_PATH;
            DWORD cbChecksum = sizeof(rgbChecksum);
            DWORD checksumtype;

            if (!pmod->QueryFileNameInfo(fileId,
                                         szName,
                                         &len,
                                         &checksumtype,
                                         rgbChecksum,
                                         &cbChecksum)) {
                break;
            }

            StdOutPrintf(L"  %4u %s (", fileId, szName);

            switch (checksumtype) {
                case CHKSUM_TYPE_NONE :
                    StdOutPrintf(L"None");
                    break;

                case CHKSUM_TYPE_MD5 :
                    StdOutPrintf(L"MD5");
                    break;

                case CHKSUM_TYPE_SHA1 :
                    StdOutPrintf(L"SHA1");
                    break;

                case CHKSUM_TYPE_SHA_256 :
                    StdOutPrintf(L"SHA_256");
                    break;

                default :
                    StdOutPrintf(L"0x%X", checksumtype);
                    break;
            }

            if (cbChecksum != 0) {
                StdOutPrintf(L": ");

                for (DWORD i = 0; i < cbChecksum; i++ ) {
                    StdOutPrintf(L"%02X", rgbChecksum[i]);
                }
            }

            StdOutPrintf(L")\n");

            fileId++;
        }

        if (fileId != 0) {
            StdOutPutc(L'\n');
        }
    }
}


void DumpPdbSecContribs(DBI *pdbi)
{
    EnumContrib *penumcontrib;

    if (pdbi->getEnumContrib((Enum **) &penumcontrib)) {
        StdOutPuts(L"  Imod  Address        Size      Characteristics\n");

        while (penumcontrib->next()) {
            WORD imod;
            WORD sn;
            long ib;
            long cb;
            DWORD grbit;

            penumcontrib->get(&imod, &sn, &ib, &cb, &grbit);

            StdOutPrintf(L"  %04X  %04X:%08X  %08X  %08X\n", imod, sn, ib, cb, grbit);
        }
    }

    else if (pdbi->getEnumContrib2((Enum **) &penumcontrib)) {
        StdOutPuts(L"  Imod  Address        Size      Characteristics  IsecCOFF\n");

        while (penumcontrib->next()) {
            WORD imod;
            WORD sn;
            DWORD ib;
            DWORD cb;
            DWORD grbit;
            DWORD snCoff;

            penumcontrib->get2(&imod, &sn, &ib, &snCoff, &cb, &grbit);

            StdOutPrintf(L"  %04X  %04X:%08X  %08X  %08X         %08X\n", imod, sn, ib, cb, grbit, snCoff);
        }
    }

    else {
        StdOutPuts(L"DBI::getEnumContrib and DBI::getEnumContrib2 failed\n");

        return;
    }

    penumcontrib->release();

    return;
}


void DumpPdbSecMap(DBI *pdbi)
{
    long cb;

    if (!pdbi->QuerySecMap(NULL, &cb)) {
        StdOutPuts(L"DBI::QuerySecMap failed\n");

        return;
    }

    if (cb == 0) {
        return;
    }

    BYTE *pb = new BYTE[cb];

    if (pb == NULL) {
        StdOutPuts(L"new failed\n");

        return;
    }

    if (!pdbi->QuerySecMap(pb, &cb)) {
        StdOutPuts(L"DBI::QuerySecMap failed\n");

        delete [] pb;

        return;
    }

    const OMFSegMap *posm = (OMFSegMap *) pb;
    StdOutPrintf(L"Sec  flags  ovl   grp   frm sname cname    offset    cbSeg\n");

    for (size_t iseg = 0; iseg < posm->cSeg; iseg++) {
        StdOutPrintf(L" %02x  %04x  %04x  %04x  %04x  %04x  %04x  %08x %08x\n",
                     iseg+1,
                     posm->rgDesc[iseg].flags.fAll,
                     posm->rgDesc[iseg].ovl,
                     posm->rgDesc[iseg].group,
                     posm->rgDesc[iseg].frame,
                     posm->rgDesc[iseg].iSegName,
                     posm->rgDesc[iseg].iClassName,
                     posm->rgDesc[iseg].offset,
                     posm->rgDesc[iseg].cbSeg);
    }

    delete [] pb;
}


bool FOpenDbg(DBI *pdbi, DBGTYPE dbgtype, const wchar_t *wszDbgtype, Dbg **ppdbg)
{
    if (!pdbi->OpenDbg(dbgtype, ppdbg)) {
        StdOutPrintf(L"DBIOpenDbg(, %s,) failed.\n", wszDbgtype);

        return(false);
    }

    return(true);
}


void DumpPdbFpo(PDB *ppdb, DBI *pdbi)
{
    // Dump Fpo data

    Dbg *pdbg;

    static const wchar_t * const wszFrameTypes[] = { L"fpo", L"trap", L"tss", L"std"};

    if (FOpenDbg(pdbi, dbgtypeFPO, L"dbgtypeFPO", &pdbg)) {
        ULONG celts;

        if ((celts = pdbg->QuerySize()) > 0) {
            StdOutPuts(L"                                                  Use Has  Frame\n"
                       L" Address  Proc Size   Locals   Prolog     Regs    BP  SEH  Type   Params\n");

            FPO_DATA data;

            for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
                StdOutPrintf(L"%08X   %8X %8X %8X %8X     %c   %c   %4s     %4X\n",
                             data.ulOffStart,
                             data.cbProcSize,
                             data.cdwLocals,
                             data.cbProlog,
                             data.cbRegs,
                             data.fUseBP ? L'Y' : L'N',
                             data.fHasSEH ? L'Y' : L'N',
                             wszFrameTypes[data.cbFrame],
                             data.cdwParams * 4);
            }
        }

        pdbg->Close();
    }

    if (FOpenDbg(pdbi, dbgtypeNewFPO, L"dbgtypeNewFPO", &pdbg)) {
        ULONG celts;

        if ((celts = pdbg->QuerySize()) > 0) {
            StdOutPuts(L" Address  Blk Size   cbLocals cbParams cbStkMax cbProlog  cbSavedRegs SEH C++EH FStart  Program\n");

            NameMap* pnm = NULL;

            if (!NameMap::open(ppdb, FALSE, &pnm)) {
                StdOutPuts(L"Error no namemap\n");
                return;
            }

            FRAMEDATA data;

            for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
                wchar_t prog[1024] = L"<Error: Invalid NI>";

                if (pnm->isValidNi(data.frameFunc)) {
                    size_t l = _countof(prog);
                    pnm->getNameW(data.frameFunc, prog, &l);
                }

                StdOutPrintf(L"%08X   %8X %8X %8X %8X %8X %8X        %c   %c      %c     %s\n",
                             data.ulRvaStart,
                             data.cbBlock,
                             data.cbLocals,
                             data.cbParams,
                             data.cbStkMax,
                             data.cbProlog,
                             data.cbSavedRegs,
                             data.fHasSEH ? L'Y' : L'N',
                             data.fHasEH ? L'Y' : L'N',
                             data.fIsFunctionStart ? L'Y' : L'N',
                             prog);
            }
        }

        pdbg->Close();
    }
}


void DumpPdbFixup(DBI *pdbi)
{
    // Dump debug fixups

    Dbg *pdbg;

    if (!FOpenDbg(pdbi, dbgtypeFixup, L"dbgtypeFixup", &pdbg)) {
        return;
    }

    ULONG celts;

    if ((celts = pdbg->QuerySize()) > 0) {
        XFIXUP data;

        StdOutPrintf(L"\nFixup Data (%u):\n\n", celts);

        StdOutPuts(L"    Type        Rva        RvaTarget\n"
                   L"    ----  ----  --------   --------\n");

        for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            StdOutPrintf(L"    %04X  %04X  %08X   %08X\n",
                         data.wType, data.wExtra, data.rva, data.rvaTarget);
        }
    }

    pdbg->Close();
}


void DumpTokenMap(DBI *pdbi)
{
    // Dump Token map data

    Dbg *pdbg;

    if (!FOpenDbg(pdbi, dbgtypeTokenRidMap, L"dbgtypeTokenRidMap", &pdbg)) {
        return;
    }

    ULONG celts;

    if ((celts = pdbg->QuerySize()) > 0) {
        DWORD data;

        StdOutPrintf(L"\nTokenMap Data (%d):\n\n", pdbg->QuerySize());

        StdOutPuts(L"    Rid In     Rid Out\n"
                   L"    --------   --------\n");

        for (DWORD i = 0; pdbg->QueryNext(1, &data); pdbg->Skip(1), ++i) {
            StdOutPrintf(L"    %08X   %08X\n", i, data);
        }
    }

    pdbg->Close();
}

void DumpPdbOmap(DBI *pdbi, bool fFromSrc)
{
    // Dump OMAP data

    Dbg *pdbg;

    if (!FOpenDbg(pdbi, 
                  fFromSrc ? dbgtypeOmapFromSrc : dbgtypeOmapToSrc,
                  fFromSrc ? L"dbgtypeOmapFromSrc" : L"dbgtypeOmapToSrc",
                  &pdbg)) {
        return;
    }

    ULONG celts;

    if ((celts = pdbg->QuerySize()) > 0) {
        OMAP data;

        StdOutPrintf(L"\nOMAP Data (%s_SRC) - (%d):\n\n",
                     fFromSrc ? L"FROM" : L"TO", pdbg->QuerySize());

        StdOutPuts(L"    Rva        RvaTo\n"
                   L"    --------   --------\n");

        for (; pdbg->QueryNext(1, &data); pdbg->Skip(1)) {
            StdOutPrintf(L"    %08X   %08X\n", data.rva, data.rvaTo);
        }
    }

    pdbg->Close();
}

// Unwind information structure definition.
//
// N.B. If the EHANDLER flag is set, personality routine should be called
//      during search for an exception handler.  If the UHANDLER flag is
//      set, the personality routine should be called during the second
//      unwind.
//
#define LINKER_UNW_FLAG_EHANDLER(x) ((x) & 0x1)
#define LINKER_UNW_FLAG_UHANDLER(x) ((x) & 0x2)
#define LINKER_UNW_PERSONALITY_PTR(x) ((x) & 0x3) // ehandler or uhandler

// Version 2 = soft2.3 conventions
// Version 3 = soft2.6 conventions
//
struct LINKER_UNWIND_INFO {
    USHORT Version;               //  Version Number
    USHORT Flags;                 //  Flags
    ULONG DataLength;             //  Length of Descriptor Data
};

struct AMD64_UNWIND_INFO {
    BYTE        bVer : 3;
    BYTE        grFlags : 5;
    BYTE        cbProlog;
    BYTE        cUnwindCodes;
    BYTE        iFrameReg : 4;
    BYTE        offFrameReg : 4;
    USHORT      rgUnwindCodes[0];       // based on cUnwindCodes above
    //ULONG     rvaExceptHandler;       // optional
    //BYTE      rgbLangSpecHandlerData; // optional
};

typedef LINKER_UNWIND_INFO *        PLUI;
typedef const LINKER_UNWIND_INFO *  PCLUI;

enum REGION_TYPE {
    REGION_UNKNOWN,
    REGION_PROLOGUE,
    REGION_BODY
};

DWORDLONG DwlDecodeULEB128(const BYTE *pb)
{
    DWORDLONG dwl = 0;
    size_t ib = 0;

    do {
        dwl += DWORDLONG(*pb & 0x7f) << (ib * 7);

        ib++;
    }
    while ((*pb++ & 0x80) != 0);

    return dwl;
}


size_t CbULEB128(const BYTE *pb)
{
    // Return the number of bytes in the ULEB128

    size_t cb = 0;

    do {
        cb++;
    }
    while ((*pb++ & 0x80) != 0);

    return cb;
}


size_t CbDumpBytes(const BYTE *pb, size_t cb, size_t trailingULEB, const wchar_t *sz, ...)
{
#define DESCRIPTOR_COLUMNS 4

    // Dump bytes and padding out to a fixed column. 'cb' is the fixed of bytes
    // to dump, trailingULEB is a count of trailing ULEB entries to dump.
    // Returns the number of bytes dumped.

    va_list valist;
    va_start(valist, sz);
    size_t cbDumped = 0;   // if we hit 4, we break and do the text label
    bool fNeedIndent = false;

    for (size_t ib = 0; ib < cb; ib++) {
        if (fNeedIndent) {
            StdOutPuts(L"    ");
            fNeedIndent = false;
        }
        StdOutPrintf(L"%02X ", *pb++);
        ++cbDumped;
        if (cbDumped == DESCRIPTOR_COLUMNS) {
            StdOutPuts(L" ");
            StdOutVprintf(sz, valist);
        }
        if ((cbDumped % DESCRIPTOR_COLUMNS) == 0) {
            StdOutPutc(L'\n');
            fNeedIndent = true;
        }
    }

    while (trailingULEB-- > 0) {
        // First: a bunch with high bit set

        while (*pb & 0x80) {
            if (fNeedIndent) {
                StdOutPuts(L"    ");
                fNeedIndent = false;
            }
            StdOutPrintf(L"%02X ", *pb++);
            cbDumped++;
            if (cbDumped == DESCRIPTOR_COLUMNS) {
                StdOutPuts(L" ");
                StdOutVprintf(sz, valist);
            }
            if ((cbDumped % DESCRIPTOR_COLUMNS) == 0) {
                StdOutPutc(L'\n');
                fNeedIndent = true;
            }
        }

        // One last byte

        if (fNeedIndent) {
            StdOutPuts(L"    ");
            fNeedIndent = false;
        }
        StdOutPrintf(L"%02X ", *pb++);
        cbDumped++;
        if (cbDumped == DESCRIPTOR_COLUMNS) {
            StdOutPuts(L" ");
            StdOutVprintf(sz, valist);
        }
        if ((cbDumped % DESCRIPTOR_COLUMNS) == 0) {
            StdOutPutc(L'\n');
            fNeedIndent = true;
        }
    }

    // Add at least one column of padding

    if (cbDumped < DESCRIPTOR_COLUMNS) {
        size_t cbT = cbDumped;

        do {
            StdOutPuts(L"   ");
        }
        while (++cbT < DESCRIPTOR_COLUMNS);
        StdOutPuts(L" ");
        StdOutVprintf(sz, valist);
        StdOutPutc(L'\n');
    }

    else if ((cbDumped % DESCRIPTOR_COLUMNS) != 0) {
        // dump a newline if we haven't already
        StdOutPutc(L'\n');
    }

    // else, it is divisible by DESCRIPTOR_COLUMNS, and we've already dumped the newline

    va_end(valist);

    return cbDumped;
}


void DumpXdataForRvaIA64(PCLUI pclui)
{
    // Dump ia64 xdata

    const BYTE *    pb = reinterpret_cast<const BYTE *>(pclui + 1);
    const BYTE *    pbMax = pb + pclui->DataLength * 8;
    REGION_TYPE     region = REGION_UNKNOWN;
    DWORDLONG       cbCurRegion = 0;
    
    StdOutPrintf(L"    %04X         Version\n"
                 L"    %04X         Flags\n"
                 L"    %08X     Data length\n",
                 pclui->Version,
                 pclui->Flags,
                 pclui->DataLength);
    
    for (;;) {
        BYTE b2;
        BYTE b3;
        BYTE b4;
        unsigned r;
        DWORDLONG rlen;
        unsigned mask;
        unsigned grsave;
        unsigned brmask;
        unsigned grmask;
        unsigned frmask;
        unsigned uleb_count;
        DWORDLONG t = 0;
        DWORDLONG size = 0;
        DWORDLONG pspoff = 0;
        DWORDLONG spoff = 0;
        unsigned abi;
        unsigned context;
        DWORDLONG ecount;
        size_t ibCur;
        unsigned a;
        unsigned reg;
        unsigned x;
        unsigned y;
        unsigned treg;
        unsigned qp;
        unsigned gr;
        unsigned rmask;
        DWORDLONG imask_length;
        DWORDLONG label;
        
        if (pb == pbMax) {
            // We've exhausted the unwind descriptor records
            
            break;
        }
        
        if (pb > pbMax) {
            // INTERNAL ERROR! We read too much!
            
            break;
        }
        
        // Start with the correct indent
        
        StdOutPuts(L"    ");
        
        BYTE b = *pb;
        
        // First, look to see if we only have padding.  There should be <= 7
        // bytes of padding.
        
        if (b == 0) {
            const BYTE *pb2;
            
            for (pb2 = pb+1; pb2 < pbMax; pb2++) {
                if (*pb2 != 0) {
                    break;
                }
            }
            
            if (pb2 == pbMax) {
                pb += CbDumpBytes(pb, (size_t) (pbMax - pb), 0, L"padding");
                break;
            }
        }
        
        if ((b & 0x80) == 0x80) {
            // A region descriptor record
            
            // ASSERT(region != REGION_UNKNOWN);
            
            if (region == REGION_PROLOGUE) {
                // Check for prologue region descriptors
                
                if ((b & 0xe0) == 0x80) {
                    // P1
                    
                    brmask = (b & 0x1f);
                    
                    pb += CbDumpBytes(pb, 1, 0, L"P1: brmask 0x%X", brmask);
                } 

                else if ((b & 0xf0) == 0xa0) {
                    // P2
                    
                    b2 = *(pb+1);
                    brmask = ((b & 0xf) << 1) | ((b2 & 0x80) >> 7);
                    gr = (b2 & 0x7f);
                    
                    pb += CbDumpBytes(pb, 2, 0, L"P2: brmask 0x%X gr %u", brmask, gr);
                } 

                else if ((b & 0xf8) == 0xb0) {
                    // P3
                    
                    static const wchar_t * const P3_types[12] = {
                        L"psp_gr",
                        L"rp_gr",
                        L"pfs_gr",
                        L"preds_gr",
                        L"unat_gr",
                        L"lc_gr",
                        L"rp_br",
                        L"rnat_gr",
                        L"bsp_gr",
                        L"bspstore_gr",
                        L"fpsr_gr",
                        L"priunat_gr"
                    };
                    
                    b2 = *(pb+1);
                    r = ((b & 0x7) << 1) | ((b2 & 0x80) >> 7);
                    
                    if (r > 11) {
                        StdOutPuts(L"Error: unwind info is corrupt!\n");
                        break;
                    }
                    
                    gr = (b2 & 0x7f);
                    
                    pb += CbDumpBytes(pb, 2, 0, L"P3: %s %u", P3_types[r], gr);
                } 

                else if (b == 0xb8) {
                    // P4
                    
                    imask_length = (cbCurRegion * 2 + 7) / 8;
                    
                    pb += CbDumpBytes(pb, (size_t) (1 + imask_length), 0, L"P4");
                } 

                else if (b == 0xb9) {
                    // P5
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    b4 = *(pb+3);
                    grmask = (b2 & 0xf0) >> 4;
                    frmask = ((b2 & 0xf) << 16) | (b3 << 8) | b4;
                    
                    pb += CbDumpBytes(pb, 4, 0, L"P5: grmask 0x%X frmask 0x%X", grmask, frmask);
                } 

                else if ((b & 0xe0) == 0xc0) {
                    // P6
                    
                    static const wchar_t * const P6_types[] = {
                        L"fr_mem",
                        L"gr_mem"
                    };
                    
                    r = (b & 0x10) >> 4;
                    rmask = (b & 0xf);
                    
                    pb += CbDumpBytes(pb, 1, 0, L"P6: %s rmask 0x%X", P6_types[r], rmask);
                } 

                else if ((b & 0xf0) == 0xe0) {
                    // P7
                    
                    enum P7_additional {
                        P7_t      = 0x1,
                        P7_spoff  = 0x2,
                        P7_pspoff = 0x4,
                        P7_size   = 0x8
                    };
                    
                    static const struct {
                        const wchar_t *szName;
                        unsigned options;
                    } P7_config[16] = {
                        { L"mem_stack_f",    P7_t | P7_size },
                        { L"mem_stack_v",    P7_t },
                        { L"spill_base",     P7_pspoff },
                        { L"psp_sprel",      P7_spoff },
                        { L"rp_when",        P7_t },
                        { L"rp_psprel",      P7_pspoff },
                        { L"pfs_when",       P7_t },
                        { L"pfs_psprel",     P7_pspoff },
                        { L"preds_when",     P7_t },
                        { L"preds_psprel",   P7_pspoff },
                        { L"lc_when",        P7_t },
                        { L"lc_psprel",      P7_pspoff },
                        { L"unat_when",      P7_t },
                        { L"unat_psprel",    P7_pspoff },
                        { L"fpsr_when",      P7_t },
                        { L"fpsr_psprel",    P7_pspoff },
                    };
                    
                    r = (b & 0xf);
                    ibCur = 1;
                    uleb_count = 0;
                    
                    if (P7_config[r].options & P7_t) {
                        uleb_count++;
                        t = DwlDecodeULEB128(pb + ibCur);
                        ibCur += CbULEB128(pb + ibCur);
                    }
                    
                    if (P7_config[r].options & P7_size) {
                        uleb_count++;
                        size = DwlDecodeULEB128(pb + ibCur);
                        ibCur += CbULEB128(pb + ibCur);
                    }
                    
                    if (P7_config[r].options & P7_pspoff) {
                        uleb_count++;
                        pspoff = DwlDecodeULEB128(pb + ibCur);
                        ibCur += CbULEB128(pb + ibCur);
                    }
                    
                    if (P7_config[r].options & P7_spoff) {
                        uleb_count++;
                        spoff = DwlDecodeULEB128(pb + ibCur);
                        ibCur += CbULEB128(pb + ibCur);
                    }
                    
                    // ASSERT uleb_count <= 2
                    
                    wchar_t rgchOutput[100];
                    
                    size_t cchOutput = swprintf_s(rgchOutput, _countof(rgchOutput), L"P7: %s", P7_config[r].szName);
                    
                    if (P7_config[r].options & P7_t) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" time %I64u", t);
                    }
                    
                    if (P7_config[r].options & P7_size) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" size %I64u", size);
                    }
                    
                    if (P7_config[r].options & P7_pspoff) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" pspoff %I64u", pspoff);
                    }
                    
                    if (P7_config[r].options & P7_spoff) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" spoff %I64u", spoff);
                    }
                    
                    pb += CbDumpBytes(pb, 1, uleb_count, L"%s", rgchOutput);
                } 

                else if (b == 0xf0) {
                    // P8
                    
                    b2 = *(pb+1);
                    
                    enum P8_additional {
                        P8_t      = 0x1,
                        P8_spoff  = 0x2,
                        P8_pspoff = 0x4
                    };
                    
                    static const struct {
                        const char *szName;
                        unsigned options;
                    } P8_config[19 + 1] = {
                        { NULL,                 0 }, // unused
                        { "rp_sprel",           P8_spoff },
                        { "pfs_sprel",          P8_spoff },
                        { "preds_sprel",        P8_spoff },
                        { "lc_sprel",           P8_spoff },
                        { "unat_sprel",         P8_spoff },
                        { "fpsr_sprel",         P8_spoff },
                        { "bsp_when",           P8_t },
                        { "bsp_psprel",         P8_pspoff },
                        { "bsp_sprel",          P8_spoff },
                        { "bspstore_when",      P8_t },
                        { "bspstore_psprel",    P8_pspoff },
                        { "bspstore_sprel",     P8_spoff },
                        { "rnat_when",          P8_t },
                        { "rnat_psprel",        P8_pspoff },
                        { "rnat_sprel",         P8_spoff },
                        { "priunat_when_gr",    P8_t },
                        { "priunat_psprel",     P8_pspoff },
                        { "priunat_sprel",      P8_spoff },
                        { "priunat_when_mem",   P8_t }
                    };
                    
                    r = b2;
                    
                    if ((r < 1) || (r > 19)) {
                        StdOutPuts(L"Error: unwind info is corrupt!\n");
                        break;
                    }
                    
                    if (P8_config[r].options & P8_t) {
                        t = DwlDecodeULEB128(pb + 2);
                    }
                    
                    if (P8_config[r].options & P8_pspoff) {
                        pspoff = DwlDecodeULEB128(pb + 2);
                    }
                    
                    if (P8_config[r].options & P8_spoff) {
                        spoff = DwlDecodeULEB128(pb + 2);
                    }
                    
                    wchar_t rgchOutput[100];
                    
                    size_t cchOutput = swprintf_s(rgchOutput, _countof(rgchOutput), L"P8: %s", P8_config[r].szName);
                    
                    if (P8_config[r].options & P8_t) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" time %I64u", t);
                    }
                    
                    if (P8_config[r].options & P8_pspoff) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" pspoff %I64u", pspoff);
                    }
                    
                    if (P8_config[r].options & P8_spoff) {
                        cchOutput += swprintf_s(rgchOutput + cchOutput, _countof(rgchOutput) - cchOutput, L" spoff %I64u", spoff);
                    }
                    
                    pb += CbDumpBytes(pb, 2, 1, L"%s", rgchOutput);
                } 

                else if (b == 0xf1) {
                    // P9
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    
                    if (((b2 & 0xf0) != 0) || ((b3 & 0x80) != 0)) {
                        StdOutPuts(L"Error: unwind info is corrupt!\n");
                        break;
                    }
                    
                    grmask = (b2 & 0xf);
                    gr = (b3 & 0x7f);
                    
                    pb += CbDumpBytes(pb, 3, 0, L"P9: grmask 0x%X gr %u", grmask, gr);
                } 

                else if (b == 0xff) {
                    // P10
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    abi = b2;
                    context = b3;
                    
                    pb += CbDumpBytes(pb, 3, 0, L"P10: abi %u context 0x%X", abi, context);
                }
            }

            else if (region == REGION_BODY) {
                // Check for body region descriptors
                
                if ((b & 0xc0) == 0x80) {
                    // B1
                    
                    static const wchar_t * const B1_types[] = {
                        L"label_state",
                        L"copy_state"
                    };
                    
                    r = (b & 0x20) >> 5;
                    label = (b & 0x1f);
                    
                    pb += CbDumpBytes(pb, 1, 0, L"B1: %s %I64u", B1_types[r], label);
                } 

                else if ((b & 0xe0) == 0xc0) {
                    // B2
                    
                    ecount = (b & 0x1f);
                    t = DwlDecodeULEB128(pb + 1);
                    
                    pb += CbDumpBytes(pb, 1, 1, L"B2: ecount %I64u time %I64u", ecount, t);
                } 

                else if (b == 0xe0) {
                    // B3
                    
                    t = DwlDecodeULEB128(pb + 1);
                    ibCur = 1 + CbULEB128(pb + 1);
                    ecount = DwlDecodeULEB128(pb + ibCur);
                    
                    pb += CbDumpBytes(pb, 1, 2, L"B3: ecount %I64u time %I64u", ecount, t);
                } 

                else if ((b & 0xf7) == 0xf0) {
                    // B4
                    
                    static const wchar_t * const B4_types[] = {
                        L"label_state",
                        L"copy_state"
                    };
                    
                    r = (b & 0x8) >> 3;
                    label = DwlDecodeULEB128(pb + 1);
                    
                    pb += CbDumpBytes(pb, 1, 1, L"B4: %s label %I64u", B4_types[r], label);
                }
                
                // Check for descriptors used in both body and prologue regions
                
                else if (b == 0xf9) {
                    // X1
                    
                    static const wchar_t * const X1_types[] = {
                        L"spill_psprel",
                        L"spill_sprel"
                    };
                    
                    b2 = *(pb+1);
                    r = (b2 & 0x80) >> 7;
                    a = (b2 & 0x40) >> 6;
                    b = (b2 & 0x20) >> 5;
                    reg = (b2 & 0x1f);
                    t = DwlDecodeULEB128(pb + 2);
                    ibCur = 2 + CbULEB128(pb + 2);
                    spoff = DwlDecodeULEB128(pb + ibCur);
                    
                    pb += CbDumpBytes(pb, 2, 2,
                                      L"X1: %s a %u b %u reg %u time %I64u off %I64u",
                                      X1_types[r],
                                      a,
                                      b,
                                      reg,
                                      t,
                                      spoff);
                } 

                else if (b == 0xfa) {
                    // X2
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    x = (b2 & 0x80) >> 7;
                    a = (b2 & 0x40) >> 6;
                    b = (b2 & 0x20) >> 5;
                    reg = (b2 & 0x1f);
                    y = (b3 & 0x80) >> 7;
                    treg = (b3 & 0x7f);
                    t = DwlDecodeULEB128(pb + 3);
                    
                    pb += CbDumpBytes(pb, 3, 1,
                                      L"X2: x %u a %u b %u y %u reg %u treg %u time %I64u",
                                      x,
                                      a,
                                      b,
                                      y,
                                      reg,
                                      treg,
                                      t);
                } 

                else if (b == 0xfb) {
                    // X3
                    
                    static const wchar_t * const X3_types[] = {
                        L"spill_psprel_p",
                        L"spill_sprel_p"
                    };
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    
                    if (((b2 & 0x40) != 0) || ((b3 & 0x80) != 0)) {
                        StdOutPuts(L"Error: unwind info is corrupt!\n");
                        break;
                    }
                    
                    r = (b2 & 0x80) >> 7;
                    qp = (b2 & 0x3f);
                    a = (b3 & 0x40) >> 6;
                    b = (b3 & 0x20) >> 5;
                    reg = (b2 & 0x1f);
                    t = DwlDecodeULEB128(pb + 3);
                    ibCur = 3 + CbULEB128(pb + 3);
                    spoff = DwlDecodeULEB128(pb + ibCur);
                    
                    pb += CbDumpBytes(pb, 3, 2,
                                      L"X3: %s qp %u a %u b %u reg %u time %I64u off %I64u",
                                      X3_types[r],
                                      qp,
                                      a,
                                      b,
                                      reg,
                                      t,
                                      spoff);
                } 

                else if (b == 0xfc) {
                    // X4
                    
                    b2 = *(pb+1);
                    b3 = *(pb+2);
                    b4 = *(pb+3);
                    
                    if ((b2 & 0xc0) != 0) {
                        StdOutPuts(L"Error: unwind info is corrupt!\n");
                        break;
                    }
                    
                    qp = (b2 & 0x3f);
                    x = (b3 & 0x80) >> 7;
                    a = (b3 & 0x40) >> 6;
                    b = (b3 & 0x20) >> 5;
                    reg = (b3 & 0x1f);
                    y = (b4 & 0x80) >> 7;
                    treg = (b4 & 0x7f);
                    t = DwlDecodeULEB128(pb + 3);
                    pb += CbDumpBytes(pb, 4, 1,
                                      L"X4: qp %u x %u a %u b %u y %u reg %u treg %u time %I64u",
                                      qp,
                                      x,
                                      a,
                                      b,
                                      y,
                                      reg,
                                      treg,
                                      t);
                }
            }

            else {
                // If nothing matched, we have corrupt unwind info
                
                StdOutPuts(L"Error: unwind info is corrupt!\n");
                break;
            }
        }

        else {
            // A header record
            
            if ((b & 0xc0) == 0) {
                // R1
                
                static const wchar_t * const R1_types[] = {
                    L"prologue",
                    L"body"
                };
                
                r = (b & 0x20) >> 5;
                cbCurRegion = rlen = (b & 0x1f);
                
                region = (r == 0) ? REGION_PROLOGUE : REGION_BODY;
                
                pb += CbDumpBytes(pb, 1, 0, L"R1: %s size %I64u", R1_types[r], rlen);
            } 

            else if ((b & 0xe0) == 0x40) {
                // R2
                
                if ((b & 0x18) != 0) {
                    StdOutPuts(L"Error: unwind info is corrupt!\n");
                    break;
                }
                
                b2 = *(pb+1);
                mask = ((b & 0x7) << 1) | ((b2 & 0x80) >> 7);
                grsave = (b2 & 0x7f);
                cbCurRegion = rlen = DwlDecodeULEB128(pb + 2);
                
                region = REGION_PROLOGUE;
                
                pb += CbDumpBytes(pb, 2, 1, L"R2: size %I64u mask 0x%X grsave %u", rlen, mask, grsave);
            } 

            else if ((b & 0xe0) == 0x60) {
                // R3
                
                static const wchar_t * const R3_types[] = {
                    L"prologue",
                    L"body"
                };
                
                if (((b & 0x1c) != 0) || ((b & 0x2) != 0)) {
                    StdOutPuts(L"Error: unwind info is corrupt!\n");
                    break;
                }
                r = (b & 0x3);
                cbCurRegion = rlen = DwlDecodeULEB128(pb + 1);
                
                region = (r == 0) ? REGION_PROLOGUE : REGION_BODY;
                
                pb += CbDumpBytes(pb, 1, 1, L"R3: %s size %I64u", R3_types[r], rlen);
            } 

            else {
                // If nothing matched, we have corrupt unwind info
                
                StdOutPuts(L"Error: unwind info is corrupt!\n");
                break;
            }
        }
    }

    if (LINKER_UNW_PERSONALITY_PTR(pclui->Flags)) {
        // Pointer to RVA of personality routine, if one

        const DWORD *prvaPersonality = (DWORD *) pbMax;

        StdOutPrintf(L"    %08X     Personality\n", *prvaPersonality);
    }

    StdOutPutc(L'\n');
}

void DumpPdbPdata(DBI *pdbi)
{
    // Dump RISC function table

    Dbg *   pdbgPdata;
    Dbg *   pdbgXdata = NULL;
    BYTE *  pbXdata = NULL;

    if (!FOpenDbg(pdbi, dbgtypePdata, L"dbgtypePdata", &pdbgPdata)) {
        return;
    }
    DWORD cb;
    
    const wchar_t *szMachine = L"Unknown";
    const wchar_t *szPdataHdr = L"         Begin     End       PrologEnd (defaulting to 3 RVA format)\n";

    if ((cb = pdbgPdata->QuerySize()) > 0) {
        DbgRvaVaBlob drvbPdata;

        if (pdbgPdata->QueryNext(sizeof drvbPdata, &drvbPdata)) {
            DWORD cbImgFE = sizeof IMAGE_FUNCTION_ENTRY;

            switch (dwMachine) {
                case IMAGE_FILE_MACHINE_IA64:
                    szMachine = L"IA64";
                    szPdataHdr = L"         RVABegin  RVAEnd    RVAUnwindInfo\n";
                    cbImgFE = sizeof IMAGE_FUNCTION_ENTRY;
                    break;

                case IMAGE_FILE_MACHINE_AMD64:
                    szMachine = L"x64";
                    szPdataHdr = L"         RVABegin  RVAEnd    RVAUnwindInfo\n";
                    cbImgFE = sizeof IMAGE_FUNCTION_ENTRY;
                    break;
            };


            DbgRvaVaBlob    drvbXdata = {0};
        
            if (fXdata && 
                FOpenDbg(pdbi, dbgtypeXdata, L"dbgtypeXdata", &pdbgXdata) &&
                pdbgXdata->QueryNext(sizeof drvbXdata, &drvbXdata))
            {
                pdbgXdata->Skip(drvbXdata.cbHdr);
                if (drvbXdata.cbData) {
                    pbXdata = new BYTE[drvbXdata.cbData];

                    if ( pbXdata && pdbgXdata->QueryNext(drvbXdata.cbData, pbXdata) ) {
                    }

                    else {
                        // skip it.  why bother?
                        fXdata = false;
                    }
                }

                else {
                    fXdata = false;
                }
            }

            else {
                fXdata = false;
            }

            StdOutPrintf(L"\nFunction Table (%lu entries)\n"
                         L"\tMachine = %s\n"
                         L"\tHeader = {\n"
                         L"\t\tver     = %d\n"
                         L"\t\tcbHdr   = %d\n"
                         L"\t\tcbData  = %d\n"
                         L"\t\trvaData = 0x%08x\n"
                         L"\t\tvaImage = 0x%016I64x\n"
                         L"\t\tdwRes1  = 0x%08x\n"
                         L"\t\tdwRes2  = 0x%08x\n"
                         L"\t};\n"
                         L"\n",
                         drvbPdata.cbData / cbImgFE,
                         szMachine,
                         drvbPdata.ver,
                         drvbPdata.cbHdr,
                         drvbPdata.cbData,
                         drvbPdata.rvaDataBase,
                         drvbPdata.vaImageBase,
                         drvbPdata.ulReserved1,
                         drvbPdata.ulReserved2);

            if (fXdata) {
                StdOutPrintf(L"Xdata header\n"
                             L"\tHeader = {\n"
                             L"\t\tver     = %d\n"
                             L"\t\tcbHdr   = %d\n"
                             L"\t\tcbData  = %d\n"
                             L"\t\trvaData = 0x%08x\n"
                             L"\t\tvaImage = 0x%016I64x\n"
                             L"\t\tdwRes1  = 0x%08x\n"
                             L"\t\tdwRes2  = 0x%08x\n"
                             L"\t};\n\n",
                             drvbXdata.ver,
                             drvbXdata.cbHdr,
                             drvbXdata.cbData,
                             drvbXdata.rvaDataBase,
                             drvbXdata.vaImageBase,
                             drvbXdata.ulReserved1,
                             drvbXdata.ulReserved2);
            }

            StdOutPrintf(L"%s", szPdataHdr);
            pdbgPdata->Skip(drvbPdata.cbHdr);

            IMAGE_FUNCTION_ENTRY data;
            DWORD ife = 0;

            for (; pdbgPdata->QueryNext(cbImgFE, &data); pdbgPdata->Skip(cbImgFE)) {
                StdOutPrintf(L"%08X %08X  %08X  %08X\n",
                             ife * sizeof(IMAGE_FUNCTION_ENTRY),
                             data.StartingAddress,
                             data.EndingAddress,
                             data.EndOfPrologue);

                if (fXdata) {
                    switch (dwMachine) {
                        case IMAGE_FILE_MACHINE_IA64:
                            DumpXdataForRvaIA64(PCLUI(pbXdata + data.EndOfPrologue - drvbXdata.rvaDataBase));
                            break;

                        case IMAGE_FILE_MACHINE_AMD64:
                            StdOutPrintf(L"NYI\n");
                            break;
                    };
                }

                ife++;
            }
        }
    }

    pdbgPdata->Close();
    if (pdbgXdata) {
        pdbgXdata->Close();
    }
    if (pbXdata) {
        delete [] pbXdata;
    }

}


void DumpSectionHeader(SHORT i, const IMAGE_SECTION_HEADER *psh)
{
    const wchar_t *name;
    DWORD li, lj;
    WORD memFlags;

    StdOutPrintf(L"\n"
                 L"SECTION HEADER #%hX\n"
                 L"%8.8S name",
                 i,
                 psh->Name);

    StdOutPrintf(L"\n"
                 L"%8X virtual size\n"
                 L"%8X virtual address\n"
                 L"%8X size of raw data\n"
                 L"%8X file pointer to raw data\n"
                 L"%8X file pointer to relocation table\n"
                 L"%8X file pointer to line numbers\n"
                 L"%8hX number of relocations\n"
                 L"%8hX number of line numbers\n"
                 L"%8X flags\n",
                 psh->Misc.PhysicalAddress,
                 psh->VirtualAddress,
                 psh->SizeOfRawData,
                 psh->PointerToRawData,
                 psh->PointerToRelocations,
                 psh->PointerToLinenumbers,
                 psh->NumberOfRelocations,
                 psh->NumberOfLinenumbers,
                 psh->Characteristics);

    memFlags = 0;

    li = psh->Characteristics;

    // Clear the padding bits

    li &= ~0x00F00000;

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
                StdOutPrintf(L"         %s\n", name);
            }
        }
    }

    // print alignment
    switch (psh->Characteristics & 0x00F00000) {
        case 0 :                         name = L"(no align specified)"; break;
        case IMAGE_SCN_ALIGN_1BYTES :    name = L"1 byte align";    break;
        case IMAGE_SCN_ALIGN_2BYTES :    name = L"2 byte align";    break;
        case IMAGE_SCN_ALIGN_4BYTES :    name = L"4 byte align";    break;
        case IMAGE_SCN_ALIGN_8BYTES :    name = L"8 byte align";    break;
        case IMAGE_SCN_ALIGN_16BYTES :   name = L"16 byte align";   break;
        case IMAGE_SCN_ALIGN_32BYTES :   name = L"32 byte align";   break;
        case IMAGE_SCN_ALIGN_64BYTES :   name = L"64 byte align";   break;
        case IMAGE_SCN_ALIGN_128BYTES :  name = L"128 byte align";  break;
        case IMAGE_SCN_ALIGN_256BYTES :  name = L"256 byte align";  break;
        case IMAGE_SCN_ALIGN_512BYTES :  name = L"512 byte align";  break;
        case IMAGE_SCN_ALIGN_1024BYTES : name = L"1024 byte align"; break;
        case IMAGE_SCN_ALIGN_2048BYTES : name = L"2048 byte align"; break;
        case IMAGE_SCN_ALIGN_4096BYTES : name = L"4096 byte align"; break;
        case IMAGE_SCN_ALIGN_8192BYTES : name = L"8192 byte align"; break;
        default:                         name = L"(invalid align)"; break;
    }

    StdOutPrintf(L"         %s\n", name);

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

        StdOutPrintf(L"         %s\n", name);
    }
}


void DumpPdbSectionHdr(DBI *pdbi)
{
    // Dump image section headers

    Dbg *pdbg;

    if (!FOpenDbg(pdbi, dbgtypeSectionHdr, L"dbgtypeSectionHdr", &pdbg)) {
        return;
    }

    ULONG celts;

    if ((celts = pdbg->QuerySize()) > 0) {
        IMAGE_SECTION_HEADER sh;
        SHORT iHdr;

        for (iHdr = 1; pdbg->QueryNext(1, &sh); pdbg->Skip(1)) {
            DumpSectionHeader(iHdr, &sh);
            iHdr++;
        }
    }

    pdbg->Close();
}


void DumpPdbSectionHdrOrig(DBI *pdbi)
{
    // Dump image section headers

    Dbg *pdbg;

    if (!FOpenDbg(pdbi, dbgtypeSectionHdrOrig, L"dbgtypeSectionHdrOrig", &pdbg)) {
        return;
    }

    ULONG celts;

    if ((celts = pdbg->QuerySize()) > 0) {
        IMAGE_SECTION_HEADER sh;
        SHORT iHdr;

        for (iHdr = 1; pdbg->QueryNext(1, &sh); pdbg->Skip(1)) {
            DumpSectionHeader(iHdr, &sh);
            iHdr++;
        }
    }

    pdbg->Close();
}


BOOL DumpPdbQueryModDebugInfo(Mod *pmod, enum DbgInfoKind e, DWORD cb, PB pb, DWORD *pcb)
{
    switch (e) {
        case DIK_xScopeExport:
            return pmod->QueryCrossScopeExports(cb, pb, pcb);
        case DIK_xScopeImport:
            return pmod->QueryCrossScopeImports(cb, pb, pcb);
        case DIK_FuncTokenMap:
            return pmod->QueryFuncMDTokenMap(cb, pb, pcb);
        case DIK_TypeTokenMap:
            return pmod->QueryTypeMDTokenMap(cb, pb, pcb);
        case DIK_MergedAsmIn:
            return pmod->QueryMergedAssemblyInput(cb, pb, pcb);
    }

    return FALSE;
}


void DumpPdbModDbgInfo(DBI *pdbi, enum DbgInfoKind e)
{
    Mod *pmod = NULL;

    while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
        USHORT imod;

        if (iModToList && pmod->QueryImod(&imod) && (iModToList != imod)) {
            continue;
        }

        wchar_t szName[PDB_MAX_PATH];
        long cch = _countof(szName);

        if (pmod->QueryNameW(szName, &cch)) {
            wchar_t wszLibrary[PDB_MAX_PATH];
            cch = _countof(wszLibrary);

            StdOutPrintf(L"** Module: \"%s\"", szName);

            if (!pmod->QueryFileW(wszLibrary, &cch)) {
            }

            else if (wcscmp(wszLibrary, szName) == 0) {
                // If file name matches module name that this isn't a library
            }

            else {
                StdOutPrintf(L" from \"%s\"", wszLibrary);
            }

            StdOutPutc(L'\n');
            StdOutPutc(L'\n');
        }

        DWORD cb = 0;

        if (!DumpPdbQueryModDebugInfo(pmod, e, 0, NULL, &cb)) {
            StdOutPuts(L"Mod::QueryCrossScopeExports failed\n\n");

            continue;
        }

        if (cb == 0) {
            continue;
        }

        PB pb = (PB) malloc(cb);

        if (pb == NULL) {
            StdOutPuts(L"malloc failed\n");

            return;
        }

        if (!DumpPdbQueryModDebugInfo(pmod, e, cb, pb, &cb)) {
            StdOutPuts(L"Mod::QueryCrossScopeExports failed\n\n");

            continue;
        }

        pfn(cb, pb);

        free(pb);

        StdOutPutc(L'\n');
    }
}


void DumpPdbStringTable(PDB *ppdb)
{
    NameMap* pnm = NULL;

    if (!NameMap::open(ppdb, FALSE, &pnm)) {
        StdOutPuts(L"NameMap::open failed\n");
        return;
    }

    NI ni = 1;
    const char *sz;

    while (pnm->isValidNi(ni) && pnm->getName(ni, &sz)) {
        StdOutPrintf(L"%08x ", ni);
        PrintSt(true, reinterpret_cast<const unsigned char *>(sz));

        ni += static_cast<NI>(strlen(sz) + 1);
    }
}


ULONG cTokenMap;
DWORD *pTokenMap;

void LoadTokenMap(DBI* pdbi)
{
    Dbg* pdbg;
    cTokenMap = 0;
    pTokenMap = NULL;
    if ( pdbi->OpenDbg(dbgtypeTokenRidMap, &pdbg) ) {
        if (  (cTokenMap = pdbg->QuerySize() ) > 0 ) {
            pTokenMap = new DWORD[ cTokenMap ];
        }

        if (pTokenMap != NULL) {
            pdbg->QueryNext( cTokenMap, pTokenMap );
        }

        else {
            cTokenMap = 0;
        }

        pdbg->Close();
    }
}

DWORD PdbMapToken( DWORD tokenOld )
{
    if ( pTokenMap == NULL )  // no map
        return tokenOld;

    if ( TypeFromToken( tokenOld ) != mdtMethodDef )    // only support a method map
        return tokenOld;

    if ( RidFromToken( tokenOld ) < cTokenMap )       // in range
        return TokenFromRid( TypeFromToken( tokenOld ), pTokenMap[ RidFromToken( tokenOld ) ] );

    return tokenOld;
}


void UnloadTokenMap()
{
    delete [] pTokenMap;

    cTokenMap = 0;
    pTokenMap = NULL;
}


void DumpPdb(PDB *ppdb)
{
    fUtf8Symbols = (ppdb->QueryInterfaceVersion() >= PDBImpvVC70);

    DBI *pdbi;

    if (!ppdb->OpenDBI(NULL, pdbRead, &pdbi)) {
        StdOutPuts(L"PDB::OpenDBI failed\n");
        return;
    }

    // get the machine type set up.  note that we also
    // set the CVDumpMachineType based on it since we may
    // have symbols embedded in types that need register
    // enumerates setup properly (s_regrel32 embedded in
    // lf_refsym types for Fortran).

    dwMachine = pdbi->QueryMachineType();

    switch (dwMachine) {
        case IMAGE_FILE_MACHINE_AM33 :
            CVDumpMachineType = CV_CFL_AM33;
            break;

        case IMAGE_FILE_MACHINE_AMD64 :
            CVDumpMachineType = CV_CFL_X64;
            break;

        case IMAGE_FILE_MACHINE_ARM :
            CVDumpMachineType = CV_CFL_ARM3;
            break;

        case IMAGE_FILE_MACHINE_ARM64 :
            CVDumpMachineType = CV_CFL_ARM64;
            break;

        case IMAGE_FILE_MACHINE_ARMNT :
            CVDumpMachineType = CV_CFL_ARMNT;
            break;

        case IMAGE_FILE_MACHINE_CEE :
            CVDumpMachineType = CV_CFL_CEE;
            break;

        case IMAGE_FILE_MACHINE_EBC :
            CVDumpMachineType = CV_CFL_EBC;
            break;

        case IMAGE_FILE_MACHINE_I386 :
            CVDumpMachineType = CV_CFL_80386;
            break;

        case IMAGE_FILE_MACHINE_IA64 :
            CVDumpMachineType = CV_CFL_IA64_1;
            break;

        case IMAGE_FILE_MACHINE_M32R :
            CVDumpMachineType = CV_CFL_M32R;
            break;

        case IMAGE_FILE_MACHINE_MIPS16 :
        case IMAGE_FILE_MACHINE_MIPSFPU :
        case IMAGE_FILE_MACHINE_MIPSFPU16 :
        case IMAGE_FILE_MACHINE_R3000 :
        case IMAGE_FILE_MACHINE_R4000 :
        case IMAGE_FILE_MACHINE_R10000 :
            CVDumpMachineType = CV_CFL_MIPS;
            break;

        case IMAGE_FILE_MACHINE_POWERPC :
            CVDumpMachineType = CV_CFL_PPC601;
            break;

        case IMAGE_FILE_MACHINE_POWERPCBE :
            CVDumpMachineType = CV_CFL_PPCBE;
            break;

        case IMAGE_FILE_MACHINE_POWERPCFP :
            CVDumpMachineType = CV_CFL_PPCFP;
            break;

        case IMAGE_FILE_MACHINE_SH3 :
            CVDumpMachineType = CV_CFL_SH3;
            break;

        case IMAGE_FILE_MACHINE_SH3DSP :
            CVDumpMachineType = CV_CFL_SH3DSP;
            break;

        case IMAGE_FILE_MACHINE_SH4 :
            CVDumpMachineType = CV_CFL_SH4;
            break;

        case IMAGE_FILE_MACHINE_SH5 :
            CVDumpMachineType = CV_CFL_SH4;     // UNDONE
            break;

        case IMAGE_FILE_MACHINE_THUMB :
            CVDumpMachineType = CV_CFL_THUMB;
            break;
    }

    LoadTokenMap(pdbi);

    if (fIDs) {
        StdOutPuts(L"\n*** IDs\n\n");

        DumpPdbTypes(ppdb, pdbi, true);
    }

    if (fMod) {
        StdOutPuts(L"\n*** MODULES\n\n");

        DumpPdbModules(pdbi);
    }

    if (fPub) {
        StdOutPuts(L"\n*** PUBLICS\n\n");

        DumpPdbPublics(ppdb, pdbi);
    }

    if (fTyp) {
        StdOutPuts(L"\n*** TYPES\n\n");

        DumpPdbTypes(ppdb, pdbi, false);
    }

    if (fTypMW) {
        StdOutPuts(L"\n*** TYPES Mismatch Warnings\n\n");

        DumpPdbTypeWarnDuplicateUDTs(ppdb, pdbi);
    }

    if (fSym) {
        StdOutPuts(L"\n*** SYMBOLS\n\n");

        DumpPdbSyms(pdbi);
    }

    if (fGPSym) {
        StdOutPuts(L"\n*** GLOBALS\n\n");

        DumpPdbGlobals(ppdb, pdbi);
    }

    if (fCoffSymRVA) {
        StdOutPuts(L"\n*** COFF SYMBOL RVA\n\n");

        DumpPdbCoffSymRVA(pdbi);
    }

    if (fInlineeLines) {
        StdOutPuts(L"\n*** INLINEE LINES\n\n");

        DumpPdbInlineeLines(pdbi);
    }

    if (fLines) {
        StdOutPuts(L"\n*** LINES\n\n");

        DumpPdbLines(pdbi, false);
    }

    if (fILLines) {
        StdOutPuts(L"\n*** IL LINES\n\n");

        DumpPdbLines(pdbi, true);
    }
    
    if (fSrcFiles) {
        StdOutPuts(L"\n*** SOURCE FILES\n\n");

        DumpPdbSrcFiles(pdbi);
    }

    if (fSecContribs) {
        StdOutPuts(L"\n*** SECTION CONTRIBUTIONS\n\n");

        DumpPdbSecContribs(pdbi);
    }

    if (fSegMap) {
        StdOutPuts(L"\n*** SEGMENT MAP\n\n");

        DumpPdbSecMap(pdbi);
    }

    if (fFpo) {
        StdOutPuts(L"\n*** FPO\n\n");

        DumpPdbFpo(ppdb, pdbi);
    }

    if (fTokenMap) {
        StdOutPuts(L"\n*** TokenMap\n\n");

        DumpTokenMap(pdbi);
    }

    if (fXFixup) {
        StdOutPuts(L"\n*** FIXUPS\n\n");

        DumpPdbFixup(pdbi);
    }

    if (fOmapf) {
        StdOutPuts(L"\n*** OMAP FROM SRC\n\n");

        DumpPdbOmap(pdbi, true);
    }

    if (fOmapt) {
        StdOutPuts(L"\n*** OMAP TO SRC\n\n");

        DumpPdbOmap(pdbi, false);
    }

    if (fPdata || fXdata) {
        if (fXdata) {
            StdOutPuts(L"\n*** PDATA/XDATA\n\n");
        }

        else {
            StdOutPuts(L"\n*** PDATA\n\n");
        }

        DumpPdbPdata(pdbi);
    }

    if (fSectionHdr) {
        StdOutPuts(L"\n*** SECTION HEADERS\n\n");

        DumpPdbSectionHdr(pdbi);

        StdOutPuts(L"\n*** ORIGINAL SECTION HEADERS\n\n");

        DumpPdbSectionHdrOrig(pdbi);
    }

    if (fXModExportIDs) {
        StdOutPuts(L"\n*** CROSS MODULE EXPORTS\n\n");

        pfn = DumpModCrossScopeExports;
        DumpPdbModDbgInfo(pdbi, DIK_xScopeExport);
    }

    if (fXModImportIDs) {
        StdOutPuts(L"\n*** CROSS MODULE IMPORTS\n\n");

        pfn = DumpModCrossScopeRefs;
        DumpPdbModDbgInfo(pdbi, DIK_xScopeImport);
    }

    if (fFuncTokenMap) {
        StdOutPuts(L"\n*** FUNCTION TOKEN MAP\n\n");

        pfn = DumpModFuncTokenMap;
        DumpPdbModDbgInfo(pdbi, DIK_FuncTokenMap);
    }

    if (fTypTokenMap) {
        StdOutPuts(L"\n*** TYPE TOKEN MAP\n\n");

        pfn = DumpModTypeTokenMap;
        DumpPdbModDbgInfo(pdbi, DIK_TypeTokenMap);
    }

    if (fMergedAsmInput) {
        StdOutPuts(L"\n*** MERGED ASSEMBLY INPUT\n\n");

        pfn = DumpModMergedAssemblyInput;
        DumpPdbModDbgInfo(pdbi, DIK_MergedAsmIn);
    }

    if (fStringTable) {
        StdOutPuts(L"\n*** STRINGTABLE\n\n");

        DumpPdbStringTable(ppdb);
    }

    UnloadTokenMap();

    pdbi->Close();
}


void DumpPdbFile(const wchar_t *szFilename)
{
    EC ec;
    PDB *ppdb;

    if (!PDB::Open2W(szFilename, pdbRead, &ec, NULL, 0, &ppdb)) {
        return;
    }

    DumpPdb(ppdb);

    ppdb->Close();
}


void ShowUdtTypeId(TPI *ptpi, const char *stName)
{
    char *szName;
    char szBuf[_MAX_PATH];

    if (fUtf8Symbols) {
        szName = (char *) stName;
    }

    else {
       size_t cch = *(BYTE *) stName;

       memcpy(szBuf, stName + 1, cch);
       szBuf[cch] = 0;

       szName = szBuf;
    }

    CV_typ_t ti;

    if (ptpi->QueryTiForUDT(szName, TRUE, &ti)) {
        StdOutPrintf(L", UDT(0x%08x)", (unsigned long) ti);
    }
}

#ifdef _DEBUG
#ifdef _DEFINE_FAILASSERTION_

extern "C" void failAssertion(SZ_CONST szAssertFile, int line)
{
    fprintf(stderr, "assertion failure: file %s, line %d\n", szAssertFile, line);
    fflush(stderr);
    DebugBreak();
}

extern "C" void failExpect(SZ_CONST szFile, int line)
{
    fprintf(stderr, "expectation failure: file %s, line %d\n", szFile, line);
    fflush(stderr);
}

extern "C" void failAssertionFunc(SZ_CONST szAssertFile, SZ_CONST szFunction, int line, SZ_CONST szCond)
{
    fprintf(
        stderr,
        "assertion failure:\n\tfile: %s,\n\tfunction: %s,\n\tline: %d,\n\tcondition(%s)\n\n",
        szAssertFile,
        szFunction,
        line,
        szCond
        );
    fflush(stderr);
    DebugBreak();
}

extern "C" void failExpectFunc(SZ_CONST szFile, SZ_CONST szFunction, int line, SZ_CONST szCond)
{
    fprintf(
        stderr,
        "expectation failure:\n\tfile: %s,\n\tfunction: %s,\n\tline: %d,\n\tcondition(%s)\n\n",
        szFile,
        szFunction,
        line,
        szCond
        );
    fflush(stderr);
}

#endif
#endif
