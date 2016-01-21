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

#include "version.h"

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "_winnt2.h"

#ifndef IMAGE_FILE_MACHINE_POWERPCBE
#define IMAGE_FILE_MACHINE_POWERPCBE         0x01F2  // IBM PowerPC Big-Endian
#endif

#define MAXNAM 256

struct CLIENT
{
   wchar_t *szDbgPath;
   BOOL fCvExecutable;
   DWORD foCv;
   DWORD cbCv;
   DWORD foOmapTo;
   DWORD cbOmapTo;
   DWORD foOmapFrom;
   DWORD cbOmapFrom;
   wchar_t *szFileName;
};


int             exefile;               // Executable file handle
WORD            Sig;                   // file signature
long            lfoBase;               // file offset of base
bool            fLinearExe;            // true if 32 bit exe

PMOD            ModList = NULL;        // List of module entries

bool            fCoffSymRVA;
bool            fFileInd;
bool            fFpo;
bool            fFuncTokenMap;
bool            fGPSym;
bool            fHdr;
bool            fIDs;
bool            fILLines;
bool            fInlineeLines;
bool            fLines;
bool            fMergedAsmInput;
bool            fMod;
bool            fNum;
bool            fOmapf;
bool            fOmapt;
bool            fPdata;
bool            fPdbPath;
bool            fPub;
bool            fRaw;
bool            fSecContribs;
bool            fSectionHdr;
bool            fSegMap;
bool            fSrcFiles;
bool            fStatics;
bool            fStringTable;
bool            fSTSym;
bool            fSym;
bool            fTokenMap;
bool            fTyp;
bool            fTypMW;
bool            fTypTokenMap;
bool            fXdata;
bool            fXFixup;
bool            fXModExportIDs;
bool            fXModImportIDs;
int             iModToList;            // Which modules to list (0==ALL)
BYTE            Signature[4];          // Version signature
long            lfaBase;               // Base address
OMFDirEntry     Libraries;             // sstLibraries directory entry
OMFDirEntry     GlobalSym;
OMFDirEntry     GlobalPub;
OMFDirEntry     StaticSym;
OMFDirEntry     GlobalTypes;
OMFDirEntry     SegMap;
OMFDirEntry     SegName;
OMFDirEntry     FileIndex;
size_t          cbRec;
DWORD           cSST;
DWORD           cSST06;
OMFDirHeader    DirHead;
long            lfoDir;
OMFDirEntry     *pDir = NULL;
bool            fSupportsQueryUDT;


void Fatal(const wchar_t *msg)
{
    fflush(NULL);

    fwprintf(stderr, L"CVDUMP : fatal error : %s\n", msg);

    putwc(L'\n', stderr);

    exit(1);
}


const wchar_t summaryUsage[] =
    L"Usage: cvdump [-?] [-asmin] [-coffsymrva] [-fixup] [-fpo] [-ftm] [-g]\n"
    L"\t[-h] [-headers] [-id] [-inll] [-illines] [-l] [-m] [-MXXX] [-omapf]\n"
    L"\t[-omapt] [-p] [-pdata] [-pdbpath] [-s] [-seccontrib] [-sf] [-S]\n"
    L"\t[-t] [-tmap] [-tmw] [-ttm] [-x] [-xdata] [-xme] [-xmi] file\n\n";

void LongUsage()
{
    StdOutPuts(summaryUsage);

    StdOutPuts(L"    -asmin          Merged assembly input\n");
    StdOutPuts(L"    -fixup          Debug fixups (PDB only)\n");
    StdOutPuts(L"    -fpo            FPO data\n");
    StdOutPuts(L"    -ftm            Function token map\n");
    StdOutPuts(L"    -g              Global Symbols\n");
    StdOutPuts(L"    -h              Header (section table)\n");
    StdOutPuts(L"    -headers        Section Headers (PDB only)\n");
    StdOutPuts(L"    -id             IDs\n");
    StdOutPuts(L"    -inll           Inlinee lines\n");
    StdOutPuts(L"    -illines        IL lines\n");
    StdOutPuts(L"    -l              Source lines\n");
    StdOutPuts(L"    -m              Modules\n");
    StdOutPuts(L"    -MXXX           XXX = Module number to dump\n");
    StdOutPuts(L"    -omapf          OMAP From Source (PDB only)\n");
    StdOutPuts(L"    -omapt          OMAP To Source (PDB only)\n");
    StdOutPuts(L"    -p              Publics\n");
    StdOutPuts(L"    -pdata          Function Table Entries (PDB only)\n");
    StdOutPuts(L"    -pdbpath        PDB search details\n");
    StdOutPuts(L"    -s              Symbols\n");
    StdOutPuts(L"    -seccontrib     Section contributions (PDB only)\n");
    StdOutPuts(L"    -sf             Sorted source file list\n");
    StdOutPuts(L"    -stringtable    String table\n");
    StdOutPuts(L"    -S              Dump static symbols only\n");
    StdOutPuts(L"    -t              Types\n");
    StdOutPuts(L"    -tmap           Token Map (PDB only)\n");
    StdOutPuts(L"    -tmw            Type UDT Mismatches\n");
    StdOutPuts(L"    -ttm            Type token map\n");
    StdOutPuts(L"    -x              Segment Map\n");
    StdOutPuts(L"    -xdata          Exception Data (PDB only)\n");
    StdOutPuts(L"    -xme            Cross module export IDs\n");
    StdOutPuts(L"    -xmi            Cross module import IDs\n");
    StdOutPuts(L"    file            Executable file to dump\n");

    exit(1);
}


void Usage()
{
    StdOutPuts(summaryUsage);

    exit(1);
}

/**     CheckSignature - check file signature
 *
 *              Sig = CheckSignature()
 *
 *              Entry   none
 *
 *              Exit    none
 *
 *              Return  SIG02 if exe has NB02 signature
 *                              SIG05 if exe has NB05 signature
 *                              SIG06 if exe has NB06 signature
 *                              aborts if any other signature
 */

static const enum CVSIG MapOrdToSig[] = {
    SIGOBSOLETE,    //0
    SIGOBSOLETE,    //1
    SIG02,
    SIGOBSOLETE,    //3
    SIGOBSOLETE,    //4
    SIG05,
    SIG06,
    SIG07,
    SIG08,
    SIG09,
    SIG10,
    SIG11,
};


WORD CheckSignature()
{
    enum CVSIG retval;
    int sigOrd;

    if (_read(exefile, Signature, 4) != 4) {
        Fatal(L"Unknown executable signature");
    }

    if ((Signature[0] != 'N') || (Signature[1] != 'B')) {
        Fatal(L"Unknown executable signature");
    }

    sigOrd = (Signature[2] - '0') * 10 + (Signature[3] - '0');

    if (sigOrd > 11) {
        Fatal(L"Unknown executable signature");
    }

    retval = MapOrdToSig[sigOrd];

    if (retval == SIGOBSOLETE) {
        Fatal(L"Obsolete executable signature");
    }

    return((WORD) retval);
}


/**     ReadNB02 - read file with NB02 signature
 *
 *
 */

void ReadNB02()
{
    WORD          i;
    DirEntry        Dir;

    // locate directory, read number of entries, allocate space, read
    // directory entries and sort into ascending module index order

    if ((_read(exefile, &lfoDir, sizeof(long)) != sizeof(long)) ||
        (_lseek(exefile, lfoDir + lfoBase, SEEK_SET) == -1) ||
        (_read(exefile, &cSST, 2) != 2)) {
        Fatal(L"Invalid executable");
    }

    // reformat directory into local memory

    assert(cSST <= 0xFFFFL);

    if ((pDir = (OMFDirEntry *) malloc (((WORD)cSST) * sizeof(OMFDirEntry))) == NULL) {
        Fatal(L"Out of memory");
    }

    for (i = 0; i < (WORD)cSST; i++) {
        if (_read(exefile, &Dir, sizeof(DirEntry)) != sizeof(DirEntry)) {
            Fatal(L"Invalid executable");
        }
        pDir[i].SubSection = Dir.SubSectionType;
        pDir[i].iMod = Dir.ModuleIndex;
        pDir[i].lfo = Dir.lfoStart;
        pDir[i].cb = (DWORD)Dir.Size;
    }
}


/**     ReadNB05 - read file with NB05 signature
 *
 *
 */

void ReadNB05()
{
    // locate directory, read number of entries, allocate space, read
    // directory entries and sort into ascending module index order

    if ((_read(exefile, &lfoDir, sizeof(long)) != sizeof(long)) ||
        (_lseek(exefile, lfoDir + lfoBase, SEEK_SET) == -1) ||
        (_read(exefile, &DirHead, sizeof(OMFDirHeader)) != sizeof(OMFDirHeader))) {
        Fatal(L"Invalid executable");
    }

    cSST = DirHead.cDir;

    // read directory into local memory to sort, then copy to memory buffer

    assert((cSST * sizeof(OMFDirEntry)) < UINT_MAX);
    if ((pDir = (OMFDirEntry *) malloc((size_t) cSST * sizeof(OMFDirEntry))) == NULL) {
        Fatal(L"Out of memory");
    }

    if ((size_t) _read(exefile, pDir, (sizeof(OMFDirEntry) * cSST)) !=
                                      (sizeof(OMFDirEntry) * cSST)) {
        Fatal(L"Invalid executable");
    }
}


/**     Get module - find or create module entry in list
 *
 *              pMod = GetModule(iMod, fAdd)
 *
 *              Entry   iMod = module index
 *                              fAdd = true if module to be added to list
 *
 *              Exit    new module structure added if iMod not in list
 *
 *              Returns pointer to module structure
 */

PMOD GetModule(WORD index, bool fAdd)
{
    PMOD    pmodNew;
    PMOD    prev;
    PMOD    ptr;

    prev = NULL;
    ptr = ModList;

    // while there are entries left in moduleList

    while (ptr != NULL) {
        if (ptr->iMod == index) {
            return ptr;
        }
        else if (ptr->iMod > index) {
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }

    // create a blank ModuleList entry

    if (!fAdd) {
        Fatal(L"New module added during ilink");
    }

    if ((pmodNew = (PMOD) malloc(sizeof(modlist))) == NULL) {
        Fatal(L"Out of memory");
    }
    memset(pmodNew, 0, sizeof(modlist));
    pmodNew->iMod = index;

    // do sorted list insertion into ModuleList

    if (prev == NULL) {
        ModList = pmodNew;
    } else {
        prev->next = pmodNew;
    }

    pmodNew->next = ptr;

    return(pmodNew);
}


void GetSSTMOD(PMOD pMod, long lfoStart, DWORD cb)
{
    WORD  cbName;

    if (_lseek(exefile, lfoStart, SEEK_SET) == -1) {
        Fatal(L"Error positioning to SSTMODULE entry");
    }

    // M00BUG - this does not handle multiple segments
    cbRec = cb;

    if ((pMod->rgdmc = (DMC *) malloc(sizeof(DMC))) == NULL) {
        Fatal(L"Out of memory");
    }
    pMod->rgdmc [ 0 ].sa = WGets();
    pMod->rgdmc [ 0 ].ra = (DWORD)WGets();
    pMod->rgdmc [ 0 ].cb = (DWORD)WGets();

    pMod->dm_iov = WGets();
    pMod->dm_ilib = WGets();
    pMod->dm_cSeg = WGets();

    if ((pMod->ModName = (char *) malloc(1 + (cbName = Gets()))) == NULL) {
        Fatal(L"Out of memory");
    }

    // Read the name and terminate it with null

    GetBytes(pMod->ModName, (size_t) cbName);
    pMod->ModName[cbName] = '\0';

    pMod->style[0] = 'C';
    pMod->style[0] = 'V';
}


void GetsstModule(PMOD pMod, long lfoStart, DWORD cb)
{
    WORD     cbName;
    OMFSegDesc SegDesc;
    unsigned   i;

    if (_lseek(exefile, lfoStart, SEEK_SET) == -1) {
        Fatal(L"Error positioning to sstModule entry");
    }

    cbRec = cb;

    // read fixed portion of sstModule table

    pMod->dm_iov = WGets();
    pMod->dm_ilib = WGets();
    pMod->dm_cSeg = WGets();
    GetBytes(pMod->style, sizeof(pMod->style));

    // read per segment information

    if (pMod->dm_cSeg > 0) {
        if ((pMod->rgdmc = (DMC *) malloc(sizeof(DMC) * pMod->dm_cSeg)) == NULL) {
            Fatal(L"Out of memory");
        }

        for (i = 0; i < pMod->dm_cSeg; i++) {
            GetBytes(&SegDesc, sizeof(SegDesc));
            pMod->rgdmc [ i ].sa = SegDesc.Seg;
            pMod->rgdmc [ i ].ra = SegDesc.Off;
            pMod->rgdmc [ i ].cb = SegDesc.cbSeg;
        }
    }

    if ((pMod->ModName = (char *) malloc(1 + (cbName = Gets()))) == NULL) {
        Fatal(L"Out of memory");
    }

    // Read the name and terminate it with null

    GetBytes(pMod->ModName, (size_t) cbName);
    pMod->ModName[cbName] = '\0';
}


void BuildModuleList()
{
    DWORD i;
    PMOD  module;
    const wchar_t *wsz;

    // Sweep directory and build module list

    if (fHdr) {
        StdOutPuts(L"Subsection    iMod     cb       lfo    offset\n");
    }

    for (i = 0; i < cSST; i++) {
        switch (pDir[i].SubSection) {
            case SSTMODULE:
                module = GetModule(pDir[i].iMod, true);
                module->ModuleSize = pDir[i].cb;
                module->ModulesAddr = pDir[i].lfo;
                GetSSTMOD(module, lfoBase + pDir[i].lfo, pDir[i].cb);
                wsz = L"Module";
                break;

            case SSTPUBLIC:
                module = GetModule(pDir[i].iMod, true);
                module->PublicSize = pDir[i].cb;
                module->PublicsAddr = pDir[i].lfo;
                wsz = L"Publics";
                break;

            case SSTTYPES:
                module = GetModule(pDir[i].iMod, true);
                module->TypeSize = pDir[i].cb;
                module->TypesAddr = pDir[i].lfo;
                wsz = L"Types";
                break;

            case SSTSYMBOLS:
                module = GetModule(pDir[i].iMod, true);
                module->SymbolSize = pDir[i].cb;
                module->SymbolsAddr = pDir[i].lfo;
                wsz = L"Symbols";
                break;

            case SSTLIBRARIES:
                Libraries = pDir[i];
                wsz = L"Libraries";
                break;

            case SSTSRCLNSEG:
            case sstSrcLnSeg:
                module = GetModule(pDir[i].iMod, true);
                module->SrcLnSize = pDir[i].cb;
                module->SrcLnAddr = pDir[i].lfo;
                wsz = L"SrcLnSeg";
                break;

            case sstModule:
                module = GetModule(pDir[i].iMod, true);
                module->ModuleSize = pDir[i].cb;
                module->ModulesAddr = pDir[i].lfo;
                GetsstModule(module, lfoBase + pDir[i].lfo, pDir[i].cb);
                wsz = L"Module";
                break;

            case sstTypes:
                module = GetModule(pDir[i].iMod, true);
                module->TypeSize = pDir[i].cb;
                module->TypesAddr = pDir[i].lfo;
                wsz = L"Types";
                break;

            case sstPreComp:
                module = GetModule(pDir[i].iMod, true);
                module->TypeSize = pDir[i].cb;
                module->TypesAddr = pDir[i].lfo;
                wsz = L"PreComp";
                break;

            case sstPublic:
                module = GetModule(pDir[i].iMod, true);
                module->PublicSize = pDir[i].cb;
                module->PublicsAddr = pDir[i].lfo;
                wsz = L"Publics";
                break;

            case sstPublicSym:
                module = GetModule(pDir[i].iMod, true);
                module->PublicSize = pDir[i].cb;
                module->PublicSymAddr = pDir[i].lfo;
                wsz = L"PublicSym";
                break;

            case sstAlignSym:
            case sstSymbols:
                module = GetModule(pDir[i].iMod, true);
                module->SymbolSize = pDir[i].cb;
                module->SymbolsAddr = pDir[i].lfo;
                wsz = L"Symbols";
                break;

            case sstSrcModule:
                module = GetModule(pDir[i].iMod, true);
                module->SrcLnSize = pDir[i].cb;
                module->SrcModuleAddr = pDir[i].lfo;
                wsz = L"SrcModule";
                break;

            case sstLibraries:
                Libraries = pDir[i];
                wsz = L"Libraries";
                break;

            case sstGlobalSym:
                GlobalSym = pDir[i];
                wsz = L"Global Sym";
                break;

            case sstGlobalPub:
                GlobalPub = pDir[i];
                wsz = L"Global Pub";
                break;

            case sstGlobalTypes:
                GlobalTypes = pDir[i];
                wsz = L"Global Types";
                break;

            case sstMPC:
                wsz = L"MPC Debug Info";
                break;

            case sstSegMap:
                SegMap = pDir[i];
                wsz = L"SegMap";
                break;

            case sstSegName:
                SegName = pDir[i];
                wsz = L"SegName";
                break;

            case sstFileIndex:
                FileIndex = pDir[i];
                wsz = L"FileIndex";
                break;

            case sstStaticSym:
                StaticSym = pDir[i];
                wsz = L"Static Sym";
                break;

            default:
                wsz = L"Unknown or NYI";
                break;
        }

        if (fHdr) {
            StdOutPrintf(L"%-12s %5d %6ld 0x%08X 0x%08X\n",
                         wsz, pDir[i].iMod, pDir[i].cb, pDir[i].lfo, pDir[i].lfo + lfoBase);
        }
    }
}


void DumpMod()
{
    PMOD pMod;           // pointer to module list

    StdOutPuts(L"\n\n*** MODULES ***\n");

    for (pMod = ModList; pMod != NULL; pMod = pMod->next) {
        char szName[256];

        strcpy_s(szName, _countof(szName), pMod->ModName);

        StdOutPrintf(L"%d\t%S", pMod->iMod, szName);

        unsigned int i = 0;

        size_t ich = strlen(szName) + 8;

        if ((ich < 40) && (pMod->dm_cSeg > 0)) {
            while (ich < 40) {
                StdOutPuts(L"\t");
                ich += 8;
            }

            StdOutPrintf(L"%04X:%08X\t%8X\n",
                         pMod->rgdmc[0].sa,
                         pMod->rgdmc[0].ra,
                         pMod->rgdmc[0].cb);

            i = 1;
        } else {
            StdOutPuts(L"\n");
            i = 0;
        }

        for (; i < pMod->dm_cSeg; i++) {
            StdOutPrintf(L"\t\t\t\t\t%04X:%08X\t%8X\n",
                         pMod->rgdmc[i].sa,
                         pMod->rgdmc[i].ra,
                         pMod->rgdmc[i].cb);
        }
    }
}


/**     DumpLibs - Display libraries section.
 *
 *
 *
 */

void DumpLibs()
{
    char szName[256];
    int  cbName;
    int  i = 0;

    if (Libraries.lfo != 0) {
        StdOutPuts(L"\n\n*** LIBRARIES subsection ***\n");

        _lseek(exefile, lfoBase + Libraries.lfo, SEEK_SET);
        cbRec = Libraries.cb;
        while (cbRec > 0) {
            // Read the name
            GetBytes(szName, (size_t)(cbName = Gets()));
            szName[cbName] = '\0';

            StdOutPrintf(L"%4d:  %S\n", i, szName);
            i++;
        }
    }

    StdOutPutc(L'\n');
}


void DumpNumbers()
{
    unsigned long cbModule  = 0;
    unsigned long cbSource  = 0;
    unsigned long cbSymbols = 0;
    unsigned long cbPublics = 0;
    unsigned long cbTypes   = 0;
    unsigned long cbGlobals = 0;
    unsigned long cbOther   = 0;
    unsigned long cbTotal   = 0;
    unsigned int  isst              = 0;

    for ( isst = 0; isst < cSST; isst++ ) {
        switch (pDir [ isst ].SubSection) {
            case sstModule:
            case SSTMODULE:
                cbModule  += pDir[isst].cb;
                break;

            case SSTPUBLIC:
            case sstPublic:
            case sstPublicSym:
            case sstGlobalPub:
                cbPublics += pDir[isst].cb;
                break;

            case SSTTYPES:
            case sstTypes:
            case sstGlobalTypes:
                cbTypes   += pDir[isst].cb;
                break;

            case SSTSYMBOLS:
            case sstAlignSym:
            case sstSymbols:
                cbSymbols += pDir[isst].cb;
                break;

            case SSTSRCLNSEG:
            case sstSrcModule:
                cbSource += pDir[isst].cb;
                break;

            case sstGlobalSym:
                cbGlobals += pDir[isst].cb;
                break;

            default:
                cbOther += pDir[isst].cb;
                break;
        }
    }

    cbTotal = cbModule + cbSource + cbSymbols + cbPublics + cbTypes + cbGlobals + cbOther;

    StdOutPuts(L"\n\n*** Statistics ***\n\n");

    StdOutPrintf(L"Module       %10d    %2d%%\n", cbModule,  (int) ((cbModule  * 100) / cbTotal));
    StdOutPrintf(L"Source Lines %10d    %2d%%\n", cbSource,  (int) ((cbSource  * 100) / cbTotal));
    StdOutPrintf(L"Symbols      %10d    %2d%%\n", cbSymbols, (int) ((cbSymbols * 100) / cbTotal));
    StdOutPrintf(L"Publics      %10d    %2d%%\n", cbPublics, (int) ((cbPublics * 100) / cbTotal));
    StdOutPrintf(L"Types        %10d    %2d%%\n", cbTypes,   (int) ((cbTypes   * 100) / cbTotal));
    StdOutPrintf(L"Globals      %10d    %2d%%\n", cbGlobals, (int) ((cbGlobals * 100) / cbTotal));
    StdOutPrintf(L"Others       %10d    %2d%%\n", cbOther,   (int) ((cbOther   * 100) / cbTotal));

    StdOutPrintf(L"Total        %10d\n", cbTotal);
}



/**     DumpPub - Display PUBLICS section
 *
 *
 *
 */

void DumpPub()
{
#define MAXPUB (256 + __max(sizeof(PUBSYM16), sizeof(PUBSYM32) + 3))
    WORD      cbName;                 // Length of name
    DWORD       off;
    WORD        seg;
    CV_typ_t    type;
    bool        fNeedsTitle = true;

    for (PMOD pMod = ModList; pMod != NULL; pMod = pMod->next) {
        if ((pMod->PublicSize != 0) &&
           ((iModToList == 0) || ((WORD)iModToList == pMod->iMod))) {
            if (fNeedsTitle) {
                fNeedsTitle = false;
                StdOutPuts(L"\n\n*** PUBLICS ***\n");
            }

            char szName[MAXPUB];   // Name of public symbol

            if (pMod->PublicsAddr != 0) {
                _lseek(exefile, lfoBase + pMod->PublicsAddr, SEEK_SET);

                strcpy_s(szName, _countof(szName), pMod->ModName);

                StdOutPrintf(L"%S\n", szName);

                cbRec = pMod->PublicSize;

                while (cbRec) {
                    if (fLinearExe) {
                        off = LGets();
                    }
                    else {
                        off = (DWORD) WGets();
                    }

                    seg = WGets();
                    type = WGets();
                    cbName = Gets();
                    GetBytes(szName, (size_t) cbName);
                    szName[cbName] = '\0';

                    StdOutPrintf(L"\t%04X:%08X %04X %S\n", seg, off, type, szName);
                }
            } else if (pMod->PublicSymAddr != 0) {
                cbRec = pMod->PublicSize;

                _lseek(exefile, lfoBase + pMod->PublicSymAddr, SEEK_SET);

                strcpy_s(szName, _countof(szName), pMod->ModName);

                StdOutPrintf(L"%s\tsignature = %x\n", szName, LGets());

                SYMPTR pSym = (SYMPTR) szName;

                while (cbRec) {
                    pSym->reclen = WGets();
                    GetBytes(&pSym->rectyp, (size_t) pSym->reclen);

                    switch (pSym->rectyp) {
                        case S_PUB16 :
                            cbName = ((PUBPTR16) pSym)->name[0];
                            ((PUBPTR16) pSym)->name[cbName + 1] = '\0';
                            StdOutPrintf(L"\t%04X:%04X     %04X %S\n",
                                   ((PUBPTR16) pSym)->seg,
                                   ((PUBPTR16) pSym)->off,
                                   ((PUBPTR16) pSym)->typind,
                                   ((PUBPTR16) pSym)->name);
                            break;

                        case S_PUB32 :
                            cbName = ((PUBPTR32) pSym)->name[0];
                            ((PUBPTR32) pSym)->name[cbName + 1] = '\0';
                            StdOutPrintf(L"\t%04X:%08X %08X %S\n",
                                   ((PUBPTR32) pSym)->seg,
                                   ((PUBPTR32) pSym)->off,
                                   ((PUBPTR32) pSym)->pubsymflags.grfFlags,
                                   ((PUBPTR32) pSym)->name);
                            break;
                    }
                }
            }
        }
    }
}


void PrtModName(const char *name, long lfa)
{
    StdOutPrintf(L" *** Module %S at %06X (%d) ***\n\n", name, lfa, lfa);
}


/**     DumpSrcLn - dump sstSrcLnSeg table
 *
 *              DumpSrcLn (pMod)
 *
 *              Entry   pMod = pointer to module
 *
 *              Exit
 *
 *              Returns none
 */

void DumpSrcLn(PMOD pMod)
{
    char  szName[MAXNAM];              // Source file name
    WORD  cbName;                      // Length of name
    WORD  cLin;                        // Count of line numbers
    DWORD loffset;                     // Code offset
    WORD  lineno;                      // Line number
    int   i;

    _lseek(exefile, lfoBase + pMod->SrcLnAddr, SEEK_SET);

    strcpy_s(szName, _countof(szName), pMod->ModName);
    PrtModName(szName, pMod->SrcLnAddr);

    cbRec = pMod->SrcLnSize;

    while (cbRec) {
        cbName = Gets();
        GetBytes(szName, (size_t) cbName);// Get source name
        szName[cbName] = '\0';

        StdOutPrintf(L"%S:", szName);

        int w = WGets();
        StdOutPrintf(L"\tsegment = %4d", w);

        cLin = WGets();                 // Get count

        StdOutPrintf(L"\tcount = %4d:", cLin);

        i = 0;

        while (cLin-- > 0) {
            // Display the offsets

            if (i == 0) {
                StdOutPutc(L'\n');
            }

            // Get next line number

            lineno = WGets();
            loffset = fLinearExe ? LGets(): (DWORD) WGets() ;

            StdOutPrintf(L"\t%4d %08x", lineno, loffset);

            i = (i + 1) % 4;                // 4 offsets per output line
        }

        StdOutPutc(L'\n');
    }

    StdOutPutc(L'\n');
}


void DumpLines(const OMFSourceModule *pModTable)
{
    size_t cfile = pModTable->cFile;
    size_t cseg  = pModTable->cSeg;

    StdOutPuts(L"  Contributor Segments:\n\n");

    for (size_t iseg = 0; iseg < cseg; iseg++) {
        StdOutPrintf(L"    %04X:%08X-%08X\n",
            *((WORD *) &pModTable->baseSrcFile[cfile + cseg * 2] + iseg),
            pModTable->baseSrcFile[cfile + iseg * 2],
            pModTable->baseSrcFile[cfile + iseg * 2 + 1]
        );
    }

    // walk each of the OMFSourceFile tables in the module table

    for (size_t ifile = 0; ifile < cfile; ifile++) {
        const OMFSourceFile *pFileTable = (OMFSourceFile *) ((BYTE *) pModTable +
                                                               pModTable->baseSrcFile[ifile]);

        // Walk each of the OMFSourceLine tables in the file

        // Get pointer to base of range table.

        struct RANGE {
            unsigned long start;
            unsigned long end;
        };

        cseg = pFileTable->cSeg;

        const RANGE *pRangeTable = (RANGE *) &pFileTable->baseSrcLn[cseg];

        // set the length of the file and the pointer to the file name

        const char *pchName = ((char *) pFileTable) +
                                offsetof(OMFSourceFile, baseSrcLn) +
                                3 * sizeof(DWORD) * cseg;

        size_t cbName = fUtf8Symbols ? strlen(pchName) : *(BYTE *) pchName++;

        for (size_t iseg = 0; iseg < cseg; iseg++) {
            const OMFSourceLine *pLineTable = (OMFSourceLine *) ((BYTE *) pModTable +
                                                                   pFileTable->baseSrcLn[iseg]);

            size_t cPair = pLineTable->cLnOff;

            if (fUtf8Symbols) {
                wchar_t wszName[PDB_MAX_PATH];

                MultiByteToWideChar(CP_UTF8, 0, pchName, -1, wszName, PDB_MAX_PATH);

                StdOutPrintf(L"\n"
                             L"  %s, %04X:%08X-%08X, line/addr pairs = %u\n"
                             L"    ",
                             wszName,
                             pLineTable->Seg,
                             pRangeTable[iseg].start,
                             pRangeTable[iseg].end,
                             cPair);
            }

            else {
                StdOutPrintf(L"\n"
                             L"  %.*S, %04X:%08X-%08X, line/addr pairs = %u\n"
                             L"    ",
                             cbName, pchName,
                             pLineTable->Seg,
                             pRangeTable[iseg].start,
                             pRangeTable[iseg].end,
                             cPair);
            }

            const unsigned long *pOff = pLineTable->offset;

            const unsigned short *pLine = (WORD *) (pOff + cPair);

            for (size_t i = 0; i < cPair; i++) {
                if ((i % 4) == 0) {
                    StdOutPutc(L'\n');
                }

                StdOutPrintf(L"  %5u %08X", *pLine++, *pOff++);
            }

            if (cPair != 0) {
                StdOutPutc(L'\n');
            }
        }

        if (cseg != 0) {
            StdOutPutc(L'\n');
        }
    }
}


/**     DumpSrcMod - dump sstSrcModule table
 *
 *              DumpSrcModd (pMod)
 *
 *              Entry   pMod = pointer to module
 *
 *              Exit
 *
 *              Returns none
 */

void DumpSrcMod(PMOD pMod)
{
    _lseek(exefile, lfoBase + pMod->SrcModuleAddr, SEEK_SET);

    char szName[256];

    strcpy_s(szName, _countof(szName), pMod->ModName);

    PrtModName(szName, pMod->SrcModuleAddr);

    size_t cbRec = (size_t) pMod->SrcLnSize;

    OMFSourceModule *pModTable = (OMFSourceModule *) malloc(cbRec);

    if (pModTable == NULL) {
        Fatal(L"Out of memory");
    }

    _read(exefile, pModTable, (unsigned int) cbRec);

    DumpLines(pModTable);

    free(pModTable);
}


/**     DumpSrcLines - display SRCLINES section.
 *
 */

void DumpSrcLines()
{
    PMOD    pMod;

    StdOutPuts(L"\n\n*** SRCLINES ***\n");

    for (pMod = ModList; pMod != NULL; pMod = pMod->next) {
        if ((pMod->SrcLnSize != 0) &&
           ((iModToList == 0) || ((WORD) iModToList == pMod->iMod))) {
            if (pMod->SrcLnAddr != 0) {
                DumpSrcLn(pMod);
            } else if (pMod->SrcModuleAddr != 0) {
                StdOutPutc(L'\n');

                DumpSrcMod(pMod);
            }
        }
    }
}

/**     DumpSegMap - dump sstSegMap info
 *
 *              DumpSegMap ( )
 *
 *              Entry
 *
 *              Exit
 *
 *              Returns none
 */

void DumpSegMap()
{
    unsigned short  iSeg;
    unsigned short  cSegLog;
    unsigned short  cSegEle;
    unsigned short  flags;
    unsigned short  ovlNbr;
    unsigned short  ggr;
    unsigned short  sa;
    unsigned short  iSegName;
    unsigned short  iClassName;
    unsigned long   phyOff;
    unsigned long   cbSeg;

    if (SegMap.lfo != 0) {
        StdOutPuts(L"\n\n*** SEGMENT MAP subsection ***\n");

        _lseek(exefile, lfoBase + SegMap.lfo, SEEK_SET);

        cbRec = SegMap.cb;
        cSegEle = WGets();
        cSegLog = WGets();

        StdOutPuts(L"Seg  Flag  ovl#  ggr    sa   sgnm  clnm  phyoff   cb\n");

        for (iSeg = 0; iSeg < cSegEle; ++iSeg) {
            flags      = WGets();
            ovlNbr     = WGets();
            ggr        = WGets();
            sa                 = WGets();
            iSegName   = WGets();
            iClassName = WGets();
            phyOff     = LGets();
            cbSeg      = LGets();

            StdOutPrintf(L" %02x  %04x  %04x  %04x  %04x  %04x  %04x  %08x %08x\n",
                   iSeg+1, flags, ovlNbr, ggr, sa, iSegName, iClassName, phyOff, cbSeg );
        }
    }

    StdOutPutc(L'\n');
}


void DumpFileInd()
{
    WORD cMod = 0;
    WORD cFileRef = 0;
    WORD *rgiszMod  = NULL;
    WORD *rgcszMod  = NULL;
    DWORD *rgulNames = NULL;
    char *rgchNames = NULL;
    DWORD ichNames = 0;
    int imod = 0;
    DWORD cchNames = 0;

    if (FileIndex.lfo != 0) {
        StdOutPuts(L"\n\n*** File Index DEBUG INFO subsection ***\n\n");

        _lseek(exefile, lfoBase + FileIndex.lfo, SEEK_SET);
        cbRec = FileIndex.cb;

        cMod = WGets();
        cFileRef = WGets();

        StdOutPrintf(L"Modules = %d, Files = %d\n\n", cMod, cFileRef);

        rgiszMod = (WORD *) malloc(sizeof(WORD) * cMod);
        if (rgiszMod == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rgiszMod, sizeof(WORD) * cMod);

        rgcszMod = (WORD *) malloc(sizeof(WORD) * cMod);
        if (rgcszMod == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rgcszMod, sizeof(WORD) * cMod);

        rgulNames = (DWORD *) malloc(sizeof(DWORD) * cFileRef);
        if (rgulNames == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rgulNames, sizeof(DWORD) * cFileRef);

        cchNames = FileIndex.cb - (
                sizeof(WORD) * 2 +
                sizeof(WORD) * 2 * cMod +  // Module list & cfiles
                sizeof(DWORD) * cFileRef    // String offsets
            );

        rgchNames = (char *) malloc((size_t) cchNames);
        if (rgchNames == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rgchNames, (size_t) cchNames);

        for ( imod = 0; imod < (int) cMod; imod++ ) {
            int isz = 0;

            if (rgcszMod[imod] != 0) {
                StdOutPrintf(L"Module #%d: Index = %d, Count = %d\n",
                    imod + 1,
                    rgiszMod [ imod ],
                    rgcszMod [ imod ]
                );

                for ( isz = 0; isz < (int) rgcszMod [ imod ]; isz++ ) {
                    StdOutPrintf(L"  %8x", rgulNames [ rgiszMod [ imod ] + isz ] );

                    if ( isz + 1 % 8 == 0 ) {
                        StdOutPuts(L"\n");
                    }
                }

                StdOutPuts(L"\n");
            }
        }

        StdOutPuts(L"\n");

        ichNames = 0;
        while ( ichNames + 3 < cchNames ) {
            char rgch [256];

            memset(rgch, 0, 256);

            memcpy(rgch, &rgchNames[ichNames + 1], (size_t) rgchNames[ichNames]);

            StdOutPrintf(L"%8x: %S\n", ichNames, rgch);

            ichNames += rgchNames[ichNames] + 1;
        }

        free(rgiszMod);
        free(rgcszMod);
        free(rgulNames);
        free(rgchNames);
    }

    StdOutPutc(L'\n');
}


/**     ClearModules - clear module entries
 *
 *              ClearModules()
 *
 *              Entry   none
 *
 *              Exit    module entry pointer and counts reset
 *
 *              Returns pointer to module structure
 */

void ClearModules()
{
    PMOD pMod = ModList;

    // while there are entries left in moduleList

    while (pMod != NULL) {
        pMod->ModulesAddr = 0;
        pMod->SymbolsAddr = 0;
        pMod->TypesAddr = 0;
        pMod->PublicsAddr = 0;
        pMod->PublicSymAddr = 0;
        pMod->SrcLnAddr = 0;
        pMod->SrcModuleAddr = 0;
        pMod->ModuleSize = 0;
        pMod->SymbolSize = 0;
        pMod->TypeSize = 0;
        pMod->PublicSize = 0;
        pMod->SrcLnSize = 0;
        pMod = pMod->next;
    }
}


/**     ReadNB06 - read incremental link directory
 *
 *
 */

void ReadNB06()
{
    unsigned i;
    PMOD    module;
    const wchar_t *wsz = NULL;

    // locate next directory, read number of entries, read entries and
    // replace existing module directory entries.

    if ((_lseek(exefile, lfoBase + DirHead.lfoNextDir, SEEK_SET) == -1) ||
        (_read(exefile, &DirHead, sizeof(OMFDirHeader)) != sizeof(OMFDirHeader))) {
        Fatal(L"Invalid incrementally linked executable");
    }

    if (cSST < DirHead.cDir) {
        Fatal(L"ilink directory entries exceeds original");
    }

    cSST06 = DirHead.cDir;

    // read directory entries and replace into existing list

    if ((size_t) _read(exefile, pDir, (sizeof(OMFDirEntry) * cSST06)) !=
                                      (sizeof(OMFDirEntry) * cSST06)) {
        Fatal(L"Invalid executable");
    }

    for (i = 0; i < (WORD)cSST06; i++) {
        switch (pDir[i].SubSection) {
            case sstModule:
                module = GetModule(pDir[i].iMod, false);
                module->ModuleSize = pDir[i].cb;
                module->ModulesAddr = pDir[i].lfo;
                GetsstModule (module, lfoBase + pDir[i].lfo, pDir[i].cb);
                wsz = L"Module";
                break;

            case sstTypes:
                module = GetModule(pDir[i].iMod, false);
                module->TypeSize = pDir[i].cb;
                module->TypesAddr = pDir[i].lfo;
                wsz = L"Types";
                break;

            case sstPublic:
                module = GetModule(pDir[i].iMod, false);
                module->PublicSize = pDir[i].cb;
                module->PublicsAddr = pDir[i].lfo;
                wsz = L"Publics";
                break;

            case sstPublicSym:
                module = GetModule(pDir[i].iMod, false);
                module->PublicSize = pDir[i].cb;
                module->PublicSymAddr = pDir[i].lfo;
                wsz = L"PublicSym";
                break;

            case sstAlignSym:
            case sstSymbols:
                module = GetModule(pDir[i].iMod, false);
                module->SymbolSize = pDir[i].cb;
                module->SymbolsAddr = pDir[i].lfo;
                wsz = L"Symbols";
                break;

            case sstSrcModule:
                module = GetModule(pDir[i].iMod, false);
                module->SrcLnSize = pDir[i].cb;
                module->SrcModuleAddr = pDir[i].lfo;
                wsz = L"SrcModule";
                break;

            default:
                assert(0);   // what else is there
                break;
        }

        StdOutPrintf(L"%-12s %5d %6d %8d\n",
                     wsz, pDir[i].iMod, pDir[i].cb, pDir[i].lfo);
    }
}


void DumpExeFile()
{
    switch (Sig) {
        case SIG02:
            StdOutPuts(L"File linked by link 5.20 or earlier linker\n");
            break;

        case SIG05:
            StdOutPuts(L"File linked by link 5.30 and not cvpacked\n");
            break;

        case SIG06:
            StdOutPuts(L"File incrementally linked by ilink 1.30 and not cvpacked\n");
            break;

        case SIG07:
            StdOutPuts(L"File has been cvpacked by QCWIN 1.0\n");
            break;

        case SIG08:
            StdOutPuts(L"File has been cvpacked for C7.0\n");
            break;

        case SIG09:
            StdOutPuts(L"File has been cvpacked for C8.0\n");
            break;

        case SIG10:
            StdOutPuts(L"File is VC++ 2.0+ format\n");
            break;

        case SIG11:
            StdOutPuts(L"File has been cvpacked for C11.0\n");
            break;

        default:
            Fatal(L"File format not recognized\n");
            break;
    }

    // Locate first directory, read number of entries,
    // allocate space and read directory entries

    switch (Sig) {
        case SIG02:
            // Read and sort NB02 directory

            ReadNB02();
            break;

        case SIG10:
            return;

        case SIG05:
        case SIG06:
        case SIG07:
        case SIG08:
        case SIG09:
        case SIG11:
            ReadNB05();
            break;
    }

    BuildModuleList();

    if (fMod) {
        DumpMod();

        DumpLibs();
    }

    if (fNum) {
        DumpNumbers();
    }

    if (fPub) {
        DumpPub();

        if (GlobalPub.cb != 0){
            DumpGlobal(L"Global Publics", &GlobalPub);
        }
    }

    if (fTyp) {
        DumpTyp();

        if (GlobalTypes.cb != 0){
            DumpCom();
        }
    }

    if (fSym) {
        DumpSym();
    }

    if (fGPSym) {
        if (GlobalSym.cb != 0) {
            DumpGlobal(L"Compacted Global Symbols", &GlobalSym);
        }
    }

    if (fSTSym && (StaticSym.cb != 0)) {
        DumpGlobal(L"Static symbol references", &StaticSym);
    }

    if (fLines) {
        DumpSrcLines();
    }

    if (fSegMap) {
        DumpSegMap();
    }

    if (fFileInd) {
        DumpFileInd();
    }

    while (DirHead.lfoNextDir != 0) {
        ClearModules();

        ReadNB06();

        if (fMod) {
            DumpMod();
        }

        if (fPub) {
            DumpPub();
        }

        if (fTyp) {
            DumpTyp();
        }

        if (fSym) {
            DumpSym();
        }

        if (fLines) {
            DumpSrcLines();
        }
    }

    if (pDir != NULL) {
        free(pDir);
    }

    PMOD pMod = ModList;
    
    while (pMod != NULL) {
        PMOD pModNext = pMod->next;

        if (pMod->ModName != NULL) {
            free(pMod->ModName);
        }

        if (pMod->rgdmc != NULL) {
            free(pMod->rgdmc);
        }

        free(pMod);

        pMod = pModNext;
    }
}


void DumpOmap(DWORD cb, bool fFromSrc)
{
    assert(cb % sizeof(OMAP) == 0);

    StdOutPrintf(L"\n\n*** OMAP Data (%s_SRC) - (%d): \n\n", fFromSrc ? L"FROM" : L"TO", cb);
    StdOutPuts(L"    Rva        RvaTo\n"
               L"    --------   --------\n");

    if (cb == 0) {
        return;
    }

    OMAP *pdata = NULL;

    if ((pdata = (OMAP *) malloc(cb)) == NULL) {
        Fatal(L"Out of memory");
    }

    if (_read(exefile, pdata, cb) != cb) {
        Fatal(L"Failed to read OMAP data");
    }

    for (DWORD i = 0; i < cb / sizeof(OMAP); i++) {
        OMAP o = *(pdata + i);
        StdOutPrintf(L"    %08X   %08X\n", o.rva, o.rvaTo);
    }

    free(pdata);
}

void DumpOld(const wchar_t *szFilename, DWORD foCv, DWORD, DWORD foOmapTo, DWORD cbOmapTo, DWORD foOmapFrom, DWORD cbOmapFrom)
{
    fLinearExe = true;

    errno_t e = _wsopen_s(&exefile, szFilename, O_RDONLY | O_BINARY, _SH_DENYWR, 0);

    if (e != 0) {
        _wperror(szFilename);
        exit(1);
    }

    lfoBase = (long) foCv;

    if (_lseek(exefile, lfoBase, SEEK_SET) == -1) {
        Fatal(L"Invalid executable");
    }

    Sig = CheckSignature();

    DumpExeFile();

    if (fOmapt) {
        // Dump OMAP to source

        lfoBase = (long) foOmapTo;

        if (_lseek(exefile, lfoBase, SEEK_SET) == -1) {
            Fatal(L"Invalid executable");
        }

        DumpOmap(cbOmapTo, false);
    }

    if (fOmapf) {
        // Dump OMAP from source

        lfoBase = (long) foOmapFrom;

        if (_lseek(exefile, lfoBase, SEEK_SET) == -1) {
            Fatal(L"Invalid executable");
        }

        DumpOmap(cbOmapFrom, true);
    }

    _close(exefile);
}

#include "ecoff.h"

DWORD GetOffSection(IMAGE_FILE_HEADER const& hdr) {
    return sizeof(hdr) + hdr.SizeOfOptionalHeader;
}


DWORD GetOffSection(IMAGE_FILE_HEADER_EX const& hdr) {
    return sizeof(hdr);
}

template<typename IMAGE_FILE_HEADER_T>
void DumpObjFile()
{
    IMAGE_FILE_HEADER_T    objFileHdr;
    DWORD                  offSection;

    if (_lseek(exefile, 0, SEEK_SET) == -1) {
        Fatal(L"Internal error");
    }

    if (_read(exefile, &objFileHdr, sizeof(objFileHdr)) != sizeof(objFileHdr)) {
        Fatal(L"Can't read file header");
    }

    offSection = GetOffSection(objFileHdr);

    DumpObjFileSections(offSection, objFileHdr.NumberOfSections);
}

void DumpObjFileSections(DWORD offSection, DWORD numberOfSections) 
{
    DWORD                  i;
    IMAGE_SECTION_HEADER   secHdr;
    DWORD                  sig;
    bool                   fNoCvSig = false;

    for (i = 0; i < numberOfSections; i++, offSection += sizeof(secHdr)) {
        if (_lseek(exefile, offSection, SEEK_SET) == -1) {
            Fatal(L"Can't seek to section\n");
        }

        if (_read(exefile, &secHdr, sizeof(secHdr)) != sizeof(secHdr)) {
            Fatal(L"Error reading section header\n");
        }

        /*
         *  check for various types of sections to dump
         */
        bool fPrintSectionNumber = false;

        if (fSym || fLines || fStringTable || fSrcFiles || fFpo) {
            if (_strnicmp((char *) secHdr.Name, ".debug$S", 8) == 0) {
                if (_lseek(exefile, secHdr.PointerToRawData, SEEK_SET) == -1) {
                    Fatal(L"Cannot seek to symbols section");
                }

                DWORD ib = 0;

                if (!fNoCvSig) {
                    if ((_read(exefile, &sig, sizeof(DWORD)) != sizeof(DWORD))) {
                        Fatal(L"Bad signature on .debug$S section");
                    }

                    switch (sig) {
                        case CV_SIGNATURE_C7 :
                        case CV_SIGNATURE_C11 :
                            fNoCvSig = true;
                            fUtf8Symbols = false;
                            break;

                        case CV_SIGNATURE_C13 :
                            fUtf8Symbols = true;
                            break;

                        default :
                            Fatal(L"Bad signature on .debug$S section");
                    }

                    ib = sizeof(DWORD);
                }

                while (ib < secHdr.SizeOfRawData) {
                    DWORD sst;
                    DWORD cb;

                    if (fUtf8Symbols) {
                        if ((ib & 3) != 0) {
                           cb = 4 - (ib & 3);

                           if (_lseek(exefile, (long) cb, SEEK_CUR) == -1) {
                               Fatal(L"Cannot seek over symbols section");
                           }

                           ib += cb;
                        }

                        if ( ib == secHdr.SizeOfRawData ) {
                            break;
                        }

                        if ((_read(exefile, &sst, sizeof(DWORD)) != sizeof(DWORD))) {
                            Fatal(L"Bad subsection header on .debug$S section");
                        }

                        if ((_read(exefile, &cb, sizeof(DWORD)) != sizeof(DWORD))) {
                            Fatal(L"Bad subsection header on .debug$S section");
                        }

                        ib += 2 * sizeof(DWORD);

                        if (cb == 0) {
                            cb = secHdr.SizeOfRawData - ib;
                        }
                    } else {
                        sst = DEBUG_S_SYMBOLS;
                        cb = secHdr.SizeOfRawData - ib;
                    }

                    bool fSkip = true;

                    switch (sst) {
                        case DEBUG_S_SYMBOLS :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** SYMBOLS\n\n");

                                DumpModSymC7(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_INLINEELINES:
                            if (fLines) {
                                
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** INLINEE LINES\n\n");

                                DumpModInlineeSourceLines(cb, NULL);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_LINES :
                            if (fLines) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** LINES\n\n");

                                DumpModC13Lines(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_STRINGTABLE :
                            if (fStringTable) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** STRINGTABLE\n\n");

                                DumpModStringTable(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_FILECHKSMS :
                            if (fSrcFiles || fLines) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** FILECHKSUMS\n\n");

                                DumpModFileChecksums(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_FRAMEDATA :
                            if (fFpo) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** FRAMEDATA\n\n");

                                DumpModFramedata(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_CROSSSCOPEEXPORTS:
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** CROSS SCOPE EXPORTS\n\n");

                                DumpModCrossScopeExports(cb, NULL);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_CROSSSCOPEIMPORTS :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** CROSS SCOPE IMPORTS\n\n");

                                DumpModCrossScopeRefs(cb, NULL);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_IL_LINES :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** IL LINES\n\n");

                                DumpModILLines(cb, ib);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_FUNC_MDTOKEN_MAP :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** FUNC TOKEN MAP\n\n");

                                DumpModFuncTokenMap(cb, NULL);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_TYPE_MDTOKEN_MAP :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** TYPE TOKEN MAP\n\n");

                                DumpModTypeTokenMap(cb, NULL);

                                fSkip = false;
                            }
                            break;

                        case DEBUG_S_MERGED_ASSEMBLYINPUT :
                            if (fSym) {
                                if (!fPrintSectionNumber) {
                                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                                    fPrintSectionNumber = true;
                                }
                                StdOutPuts(L"\n*** MERGED ASSEMBLY INPUT\n\n");

                                DumpModMergedAssemblyInput(cb, NULL);

                                fSkip = false;
                            }
                            break;
                    }

                    if (fSkip) {
                        if (_lseek(exefile, (long) cb, SEEK_CUR) == -1) {
                            Fatal(L"Cannot seek over symbols section");
                        }
                    }

                    ib += cb;
                }
            }
        }

        if (fTyp) {
            if ((_strnicmp((char *) secHdr.Name, ".debug$T", 8) == 0) ||
                (_strnicmp((char *) secHdr.Name, ".debug$P", 8) == 0)) {
                if (_lseek(exefile, secHdr.PointerToRawData, SEEK_SET) == -1) {
                    Fatal(L"Cannot seek to types section");
                }

                if ((_read(exefile, &sig, sizeof(sig)) != sizeof(sig))) {
                    Fatal(L"Bad signature on .debug$T section");
                }

                switch (sig) {
                    case CV_SIGNATURE_C7 :
                    case CV_SIGNATURE_C11 :
                        fUtf8Symbols = false;
                        break;

                    case CV_SIGNATURE_C13 :
                        fUtf8Symbols = true;
                        break;

                    default :
                        Fatal(L"Bad signature on .debug$T section");
                }

                if (!fPrintSectionNumber) {
                    StdOutPrintf(L"\n***** SECTION #%X\n", i + 1);
                    fPrintSectionNumber = true;
                }
                StdOutPuts(L"\n*** TYPES\n\n");

                DumpModTypC7(secHdr.SizeOfRawData - sizeof(sig));
            }
        }
    }
}


void DumpUnknown(const wchar_t *szFilename)
{
    unsigned short wMagic;
    long  dlfaBase;

    errno_t e = _wsopen_s(&exefile, szFilename, O_RDONLY | O_BINARY, _SH_DENYWR, 0);

    if (e != 0) {
        _wperror(szFilename);
        exit(1);
    }

    if (_read(exefile, &wMagic, 2) != 2) {
        Fatal(L"Zero length file input file");
    }

    switch (wMagic) {
        case IMAGE_DOS_SIGNATURE :
        case 'BN' :
            // This is an non-PE executable

            if (_lseek(exefile, -8L, SEEK_END) == -1) {
                Fatal(L"Invalid executable");
            }

            Sig = CheckSignature();

            if ((_read(exefile, &dlfaBase, sizeof(long)) != sizeof(long)) ||
                (_lseek(exefile, -dlfaBase, SEEK_END) == -1)) {
                Fatal(L"No CodeView info");
            }

            lfoBase = _tell(exefile);

            if (CheckSignature() != Sig) {
                Fatal(L"Invalid executable");
            }

            DumpExeFile();
            break;

        case IMAGE_FILE_MACHINE_AM33 :
        case IMAGE_FILE_MACHINE_AMD64 :
        case IMAGE_FILE_MACHINE_ARM :
        case IMAGE_FILE_MACHINE_ARM64 :
        case IMAGE_FILE_MACHINE_ARMNT :
        case IMAGE_FILE_MACHINE_CEE :
        case IMAGE_FILE_MACHINE_EBC :
        case IMAGE_FILE_MACHINE_I386 :
        case IMAGE_FILE_MACHINE_IA64 :
        case IMAGE_FILE_MACHINE_M32R :
        case IMAGE_FILE_MACHINE_MIPS16 :
        case IMAGE_FILE_MACHINE_MIPSFPU :
        case IMAGE_FILE_MACHINE_MIPSFPU16 :
        case IMAGE_FILE_MACHINE_POWERPC :
        case IMAGE_FILE_MACHINE_POWERPCBE :
        case IMAGE_FILE_MACHINE_POWERPCFP :
        case IMAGE_FILE_MACHINE_R3000 :
        case IMAGE_FILE_MACHINE_R4000 :
        case IMAGE_FILE_MACHINE_R10000 :
        case IMAGE_FILE_MACHINE_SH3 :
        case IMAGE_FILE_MACHINE_SH3DSP :
        case IMAGE_FILE_MACHINE_SH4 :
        case IMAGE_FILE_MACHINE_SH5 :
        case IMAGE_FILE_MACHINE_THUMB :
            DumpObjFile<IMAGE_FILE_HEADER>();
            break;

        case IMAGE_FILE_MACHINE_UNKNOWN :
            if (_read(exefile, &wMagic, 2) != 2 || wMagic != 0xFFFF) {
                Fatal(L"Invalid anonmyous object");
            }
            if (_read(exefile, &wMagic, 2) != 2 || wMagic < 1) {
                Fatal(L"Anonmyous object");
            }
            GUID guid;
            if (_lseek(exefile, 12, SEEK_SET) == -1 ||
                _read(exefile, &guid, sizeof(guid)) != sizeof(guid)) {
                Fatal(L"Invalid anonmyous object");
            }
            if (memcmp(&guid, &EXTENDED_COFF_OBJ_GUID, sizeof(guid)) != 0) {
                Fatal(L"Invalid anonmyous object");
            }
            DumpObjFile<IMAGE_FILE_HEADER_EX>();
            break;

        default :
            // Check for a pdb (don't do anything here for easy checking of
            // the bytes in the header since it may be an MSF or an OLE storage.

            DumpPdbFile(szFilename);
            break;
    }

    _close(exefile);
}


const wchar_t *SzErrorFromEc(PDBErrors ec, _Out_opt_cap_(cchEc) wchar_t *szEc, size_t cchEc)
{
    switch (ec) {
        case EC_OUT_OF_MEMORY :
            return L"Out of Memory";

        case EC_FILE_SYSTEM :
            return L"I/O error";

        case EC_NOT_FOUND :
            return L"File not found";

        case EC_INVALID_SIG :
            return L"PDB signature mismatch";

        case EC_INVALID_AGE :
            return L"PDB age mismatch";

        case EC_FORMAT :
            return L"Invalid file format";

        case EC_CORRUPT :
            return L"Corrupt PDB";

        case EC_INVALID_EXE_TIMESTAMP :
            return L"DBG timestamp mismatch";
   }

   _itow_s(ec, szEc, cchEc, 10);

   return szEc;
}


void PDBCALL DumpNotifyDebugDir(void *pvClient, BOOL fExecutable, const _IMAGE_DEBUG_DIRECTORY *pdbgdir)
{
    if (pdbgdir->Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
        CLIENT *pclient = (CLIENT *) pvClient;

        pclient->fCvExecutable = fExecutable;
        pclient->foCv = pdbgdir->PointerToRawData;
        pclient->cbCv = pdbgdir->SizeOfData;
    }
    else if (pdbgdir->Type == IMAGE_DEBUG_TYPE_OMAP_TO_SRC) {
        CLIENT *pclient = (CLIENT *) pvClient;

        pclient->foOmapTo = pdbgdir->PointerToRawData;
        pclient->cbOmapTo = pdbgdir->SizeOfData;
    }
    else if (pdbgdir->Type == IMAGE_DEBUG_TYPE_OMAP_FROM_SRC) {
        CLIENT *pclient = (CLIENT *) pvClient;

        pclient->foOmapFrom = pdbgdir->PointerToRawData;
        pclient->cbOmapFrom = pdbgdir->SizeOfData;
    }
}


void PDBCALL DumpNotifyOpenDBG(void *pvClient, const wchar_t *wszDbgPath, PDBErrors ec, const wchar_t *)
{
    if (ec == EC_OK) {
        CLIENT *pclient = (CLIENT *) pvClient;

        pclient->szDbgPath = _wcsdup(wszDbgPath);

        StdOutPrintf(L"Debug information split into '%s'\n", wszDbgPath);
    }

    else if (fPdbPath) {
        wchar_t szEc[11];

        const wchar_t *szError = SzErrorFromEc(ec, szEc, _countof(szEc));

        StdOutPrintf(L"DBG file '%s' checked.  (%s)\n", wszDbgPath, szError);
    }
}


void PDBCALL DumpNotifyOpenPDB(void *, const wchar_t *wszPdbPath, PDBErrors ec, const wchar_t *)
{
    if (ec == EC_OK) {
        StdOutPrintf(L"Debug information located in PDB '%s'\n", wszPdbPath);
    }

    else if (fPdbPath) {
        wchar_t szEc[11];

        const wchar_t *szError = SzErrorFromEc(ec, szEc, _countof(szEc));

        StdOutPrintf(L"PDB file '%s' checked.  (%s)\n", wszPdbPath, szError);
    }
}

bool fReadAt(FILE *fp, DWORD off, DWORD cb, void *pv)
{
    if (fseek(fp, (long)off, SEEK_SET) != 0) {
        return false;
    }

    DWORD cbRead = (DWORD)fread(pv, 1, cb, fp);
    return cbRead == cb;
}

HRESULT ReadCodeViewDebugData(void *pvClient, unsigned long *pcbData, void *pvData)
{
    CLIENT *pClient = (CLIENT *)pvClient;
    wchar_t *szFileName = pClient->szFileName;

    FILE *fp = _wfsopen(szFileName, L"r", _SH_DENYWR);

    if (fp == NULL) {
        return E_FAIL;
    }
    
    IMAGE_DOS_HEADER DosHeader;
    if (!fReadAt(fp, 0, sizeof(DosHeader), &DosHeader)) {
        return E_FAIL;
    }
    assert(DosHeader.e_magic == IMAGE_DOS_SIGNATURE);

    IMAGE_NT_HEADERS32 NtHeader;
    if (!fReadAt(fp, DosHeader.e_lfanew, sizeof(NtHeader), &NtHeader)) {
        return E_FAIL;
    }
    assert(NtHeader.Signature == IMAGE_NT_SIGNATURE);
    assert(NtHeader.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);

    if (NtHeader.OptionalHeader.NumberOfRvaAndSizes < IMAGE_DIRECTORY_ENTRY_DEBUG) {
        return E_FAIL;
    }

    DWORD rvaDbgDir = NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
    DWORD cbDbgDir = NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;

    DWORD off = DWORD(DosHeader.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + NtHeader.FileHeader.SizeOfOptionalHeader);
    DWORD csec = NtHeader.FileHeader.NumberOfSections;

    IMAGE_SECTION_HEADER SecHeader = {0};
    for(DWORD isec = 0; isec < csec; isec++, off += sizeof(SecHeader)) {
        if (!fReadAt(fp, off, sizeof(SecHeader), &SecHeader)) {
            return E_FAIL;
        }
        if (SecHeader.VirtualAddress > rvaDbgDir) {
            continue;
        }

        if ((SecHeader.VirtualAddress + SecHeader.SizeOfRawData) < (rvaDbgDir + cbDbgDir))
        {
            continue;
        }
        break;
    }

    off = SecHeader.PointerToRawData + (rvaDbgDir - SecHeader.VirtualAddress);

    IMAGE_DEBUG_DIRECTORY dbgdirCv = {0};
    dbgdirCv.Type = IMAGE_DEBUG_TYPE_UNKNOWN;

    DWORD cdbgdir = DWORD(cbDbgDir/sizeof(IMAGE_DEBUG_DIRECTORY));

    for (DWORD idbgdir = 0; idbgdir < cdbgdir; idbgdir++) {
        IMAGE_DEBUG_DIRECTORY dbgdir;

        if (!fReadAt(fp, off, sizeof(dbgdir), &dbgdir)) {
            return E_FAIL;
        }

        rvaDbgDir += sizeof(IMAGE_DEBUG_DIRECTORY);
        off += sizeof(IMAGE_DEBUG_DIRECTORY);

        if (dbgdir.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
            dbgdirCv = dbgdir;
        }
    }

    if (dbgdirCv.Type != IMAGE_DEBUG_TYPE_UNKNOWN) {
        if (*pcbData >= dbgdirCv.SizeOfData && pvData != NULL) {
             fReadAt(fp, dbgdirCv.PointerToRawData, dbgdirCv.SizeOfData, pvData);
        }
        *pcbData = dbgdirCv.SizeOfData;
        return S_OK;
    }

    return E_FAIL;
}

HRESULT ReadMiscDebugData(void *pvClient, DWORD *pdwTSExe, DWORD *pdwTSDbg, DWORD *pdwSize, unsigned long *pcbData, void *pvData)
{
    CLIENT *pClient = (CLIENT *)pvClient;
    wchar_t *szFileName = pClient->szFileName;

    FILE *fp = _wfsopen(szFileName, L"r", _SH_DENYWR);

    if (fp == NULL) {
        return E_FAIL;
    }
    
    IMAGE_DOS_HEADER DosHeader;
    if (!fReadAt(fp, 0, sizeof(DosHeader), &DosHeader)) {
        return E_FAIL;
    }
    assert(DosHeader.e_magic == IMAGE_DOS_SIGNATURE);

    IMAGE_NT_HEADERS32 NtHeader;
    if (!fReadAt(fp, DosHeader.e_lfanew, sizeof(NtHeader), &NtHeader)) {
        return E_FAIL;
    }
    assert(NtHeader.Signature == IMAGE_NT_SIGNATURE);
    assert(NtHeader.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC);

    if (NtHeader.OptionalHeader.NumberOfRvaAndSizes < IMAGE_DIRECTORY_ENTRY_DEBUG) {
        return E_FAIL;
    }

    DWORD rvaDbgDir = NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
    DWORD cbDbgDir = NtHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;

    DWORD off = DWORD(DosHeader.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + NtHeader.FileHeader.SizeOfOptionalHeader);
    DWORD csec = NtHeader.FileHeader.NumberOfSections;

    IMAGE_SECTION_HEADER SecHeader = {0};
    for(DWORD isec = 0; isec < csec; isec++, off += sizeof(SecHeader)) {
        if (!fReadAt(fp, off, sizeof(SecHeader), &SecHeader)) {
            return E_FAIL;
        }

        if (SecHeader.VirtualAddress > rvaDbgDir) {
            continue;
        }

        if ((SecHeader.VirtualAddress + SecHeader.SizeOfRawData) < (rvaDbgDir + cbDbgDir))
        {
            continue;
        }
        break;
    }

    off = SecHeader.PointerToRawData + (rvaDbgDir - SecHeader.VirtualAddress);

    IMAGE_DEBUG_DIRECTORY dbgdirMisc = {0};
    dbgdirMisc.Type = IMAGE_DEBUG_TYPE_UNKNOWN;

    DWORD cdbgdir = DWORD(cbDbgDir/sizeof(IMAGE_DEBUG_DIRECTORY));

    for (DWORD idbgdir = 0; idbgdir < cdbgdir; idbgdir++) {
        IMAGE_DEBUG_DIRECTORY dbgdir;

        if (!fReadAt(fp, off, sizeof(dbgdir), &dbgdir)) {
            return E_FAIL;
        }

        rvaDbgDir += sizeof(IMAGE_DEBUG_DIRECTORY);
        off += sizeof(IMAGE_DEBUG_DIRECTORY);

        if (dbgdir.Type == IMAGE_DEBUG_TYPE_MISC) {
            dbgdirMisc = dbgdir;
        }
    }

    if (dbgdirMisc.Type != IMAGE_DEBUG_TYPE_UNKNOWN) {
        *pdwTSDbg = dbgdirMisc.TimeDateStamp;
        *pdwTSExe = NtHeader.FileHeader.TimeDateStamp;
        *pdwSize  = NtHeader.OptionalHeader.SizeOfImage;

        if (*pcbData >= dbgdirMisc.SizeOfData && pvData != NULL) {
             fReadAt(fp, dbgdirMisc.PointerToRawData, dbgdirMisc.SizeOfData, pvData);
        }

        *pcbData = dbgdirMisc.SizeOfData;
        return S_OK;
    }

    return E_FAIL;
}

PDBCALLBACK PDBCALL PfnDumpQueryCallback(void *, POVC povc)
{
    switch (povc) {
        case povcNotifyDebugDir :
            return (PDBCALLBACK) &DumpNotifyDebugDir;

        case povcNotifyOpenDBG :
            return (PDBCALLBACK) &DumpNotifyOpenDBG;

        case povcNotifyOpenPDB :
            return (PDBCALLBACK) &DumpNotifyOpenPDB;

#if 0
        case povcReadCodeViewDebugData:
            return (PDBCALLBACK) &ReadCodeViewDebugData;

        case povcReadMiscDebugData:
            return (PDBCALLBACK) &ReadMiscDebugData;
#endif
    }

    return 0;
}


int __cdecl wmain(int argc, __in_ecount(argc) wchar_t *argv[])
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

    OutputInit();

    // Print startup microsoft banner and process the arguments

    StdOutPrintf(L"Microsoft (R) Debugging Information Dumper  Version %d.%02d.%04d\n"
                 L"Copyright (C) Microsoft Corporation.  All rights reserved.\n\n",
                 rmj, rmm, rup);

    if (argc < 2) {
        // Check syntax

        Usage();
    }

    bool fAny = false;

    // Skip argv[0]

    argv++;
    argc--;

    while (argc && ((*argv[0] == L'-') || (*argv[0] == L'/'))) {
        switch ((*argv)[1]) {
            case L'a':
                if (!wcscmp(*argv+1, L"asmin")) {
                    fMergedAsmInput = true;
                } else {
                    Usage();
                }

                fAny = true;
                break;

            case L'c':
                if (!wcscmp(*argv+1, L"coffsymrva")) {
                    fCoffSymRVA = true;
                } else {
                    Usage();
                }

                fAny = true;
                break;

            case L'f':
                if (!wcscmp(*argv+1, L"fpo")) {
                    fFpo = true;       // Dump FPO data in PDB
                } else if (!wcscmp(*argv+1, L"fixup")) {
                    fXFixup = true;    // Dump Debug Fixups in PDB
                } else if (!wcscmp(*argv+1, L"ftm")) {
                    fFuncTokenMap = true;
                } else {
                    fFileInd = true;   // Dump File index
                }

                fAny = true;
                break;

            case L'g':                 // Dump GLOBAL
                fGPSym = true;

                fAny = true;
                break;

            case L'h':
                if (!wcscmp(*argv+1, L"headers")) {
                    fSectionHdr = true;// Dump Image Section Headers in PDB
                } else {
                    fHdr = true;       // Dump header (section table)
                }

                fAny = true;
                break;

            case L'i':
                if (!wcscmp(*argv+1, L"i")) {
                    fSTSym = true;     // Dump Statics
                } else if (!wcscmp(*argv+1, L"id")) {
                    fIDs = true;       // Dump IDs
                } else if (!wcscmp(*argv+1, L"inll")) {
                    fInlineeLines = true;
                } else if (!wcscmp(*argv+1, L"illines")) {
                    fILLines = true;
                } else {
                    Usage();
                }

                fAny = true;
                break;

            case L'l':                 // Dump SRCLINES
                fLines = true;

                fAny = true;
                break;

            case L'M':                 // Modules to list symbols and types for
                iModToList = _wtoi((*argv) + 2);
                break;

            case L'm':                 // Dump MODULES
                fMod = true;

                fAny = true;
                break;

            case L'n':                 // Dump size numbers
                fNum = true;

                fAny = true;
                break;

            case L'o':
                if (!wcscmp(*argv+1, L"omapt")) {
                    fOmapt = true;     // Dump OMAP_TO_SRC data in PDB
                } else if (!wcscmp(*argv+1, L"omapf")) {
                    fOmapf = true;     // Dump OMAP_FROM_SRC data in PDB
                } else {
                    Usage();           // Syntax error
                }

                fAny = true;
                break;

            case L'p':
                if (!wcscmp(*argv+1, L"pdata")) {
                    fPdata = true;     // Dump Func data in PDB
                }
                else if (!wcscmp(*argv+1, L"pdbpath")) {
                    fPdbPath = true;   // Display PDB search details
                }
                else {
                    fPub = true;       // Dump PUBLICS
                }

                fAny = true;
                break;

            case L'R':                 // Dump Raw data
                fRaw = true;

                fAny = true;
                break;

            case L'S':                 // Dump Statics only
                fStatics = true;

                fAny = true;
                break;

            case L's':                 
                if (!wcscmp(*argv+1, L"s")) {
                    fSym = true;       // Dump SYMBOLS
                } else if (!wcscmp(*argv+1, L"seccontrib")) {
                    fSecContribs = true;
                } else if (!wcscmp(*argv+1, L"sf")) {
                    fSrcFiles = true;  // Dump source files only
                } else if (!wcscmp(*argv+1, L"stringtable")) {
                    fStringTable = true;
                } else {
                   Usage();
                }

                fAny = true;
                break;

            case L't':
                if (!wcscmp(*argv+1, L"tmap")) {
                    fTokenMap = true;  // Dump TokenMap data in PDB
                } else if (!wcscmp(*argv+1, L"tmw")) {
                    fTypMW = true;     // Dump Type mismatch warnings (PDB only)
                } else if (!wcscmp(*argv+1, L"ttm")) {
                    fTypTokenMap = true;
                } else {
                    fTyp = true;       // Dump TYPES
                }

                fAny = true;
                break;

            case L'x':
                if (!wcscmp(*argv+1, L"xdata")) {
                    fXdata = true;     // Dump Xdata, must include pdata too
                    fPdata = true;
                } else if (!wcscmp(*argv+1, L"xme")) {
                    fXModExportIDs = true;
                } else if (!wcscmp(*argv+1, L"xmi")) {
                    fXModImportIDs = true;
                } else {
                    fSegMap = true;    // Dump Seg Map
                }

                fAny = true;
                break;

            case L'?':                 // Print usage banner
                LongUsage();
                break;

            default:
                Usage();               // Syntax error
                break;

        }

        argv++;
        argc--;
    }

    if (!argv[0] || argv[1] || !argc) {
        Usage();
    }

    if (!fCoffSymRVA &&
        !fStringTable &&
        !fFileInd &&
        !fFpo &&
        !fFuncTokenMap &&
        !fGPSym &&
        !fHdr &&
        !fIDs &&
        !fILLines &&
        !fInlineeLines &&
        !fLines &&
        !fMergedAsmInput &&
        !fMod &&
        !fNum &&
        !fOmapf &&
        !fOmapt &&
        !fPdata &&
        !fPub &&
        // !fRaw &&
        !fSecContribs &&
        !fSectionHdr &&
        !fSegMap &&
        !fSrcFiles &&
        !fSTSym &&
        !fSym &&
        !fTokenMap &&
        !fTyp &&
        !fTypMW &&
        !fTypTokenMap &&
        !fXdata &&
        !fXFixup &&
        !fXModExportIDs &&
        !fXModImportIDs) {
        // If no switches, set all options

        fCoffSymRVA = true;
        fFileInd = true;
        fFpo = true;
        fFuncTokenMap = true;
        fGPSym = true;
        fHdr = true;
        fIDs = true;
        fInlineeLines = true;
        fILLines = true;
        fLines = true;
        fMergedAsmInput = true;
        fMod = true;
        fNum = true;
        fOmapf = true;
        fOmapt = true;
        fPdata = true;
        fPub = true;
        // fRaw = true;
        fSecContribs = true;
        fSectionHdr = true;
        fSegMap = true;
        fSrcFiles = true;
        fSTSym = true;
        fStringTable = true;
        fSym = true;
        fTokenMap = true;
        fTyp = true;
        fTypMW = true;
        fTypTokenMap = true;
        fXdata = true;
        fXFixup = true;
        fXModExportIDs = true;
        fXModImportIDs = true;
    }


    wchar_t szDrive[_MAX_DRIVE];
    wchar_t szDir[_MAX_PATH];
    wchar_t szFName[_MAX_FNAME];
    wchar_t szExt[_MAX_EXT];

    _wsplitpath_s(*argv, szDrive, _countof(szDrive), szDir, _countof(szDir), szFName, _countof(szFName), szExt, _countof(szExt));

    wchar_t szFilename[_MAX_DRIVE + _MAX_PATH + _MAX_FNAME + _MAX_EXT + 1];

    _wmakepath_s(szFilename, _countof(szFilename), szDrive, szDir, szFName, (szExt[0] == '\0') ? L".exe" : szExt);

    CLIENT client;

    client.szDbgPath = NULL;
    client.cbCv = 0;
    client.cbOmapTo = 0;
    client.cbOmapFrom = 0;
    client.szFileName = szFilename;

    EC ec;
    PDB *ppdb;

    BOOL fPdb = PDB::OpenValidate5(szFilename,
                                   NULL,
                                   &client,
                                   &PfnDumpQueryCallback,
                                   &ec,
                                   NULL,
                                   0,
                                   &ppdb);

    if (fPdb) {
        DumpPdb(ppdb);

        ppdb->Close();
    } else if (ec == EC_NOT_FOUND) {
        StdOutPuts(L"Debug information in not found .PDB file\n");

        return(1);
    } else if (ec == EC_INVALID_EXECUTABLE) {
        DumpUnknown(szFilename);
    } else if (ec == EC_DBG_NOT_FOUND) {
        StdOutPuts(L"Debug information split into not found .DBG file\n");

        return(1);
    } else if (ec == EC_NO_DEBUG_INFO) {
        StdOutPuts(L"No CodeView format debug information available\n");

        return(1);
    } else if (ec == EC_DEBUG_INFO_NOT_IN_PDB) {
        // There is old style CodeView information

        DumpOld(client.fCvExecutable ? szFilename : client.szDbgPath, client.foCv, client.cbCv,
                client.foOmapTo, client.cbOmapTo, client.foOmapFrom, client.cbOmapFrom);
    }

    return(0);
}
