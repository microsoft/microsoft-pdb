/***********************************************************************
* Microsoft (R) Debugging Information Dumper
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
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <malloc.h>
#include <search.h>
#include <share.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys\stat.h>
#include <sys\types.h>
#include <time.h>

#pragma pack(1)
#include "cvexefmt.h"
#pragma pack()
#include "cvinfo.h"
#include "output.h"

#include "pdb.h"

#include "cvtdef.h"

// typedef unsigned char   BYTE;
// typedef int             BOOL;
// typedef unsigned short  WORD;
// typedef unsigned long   DWORD;


// Types necessary to use szst.h
typedef char *          SZ;
typedef const char *    SZ_CONST;
typedef char *          ST;
typedef const char *    ST_CONST;
typedef unsigned char * PB;
typedef long            CB;

#define LNGTHSZ 2       // The size of the length field
#define MAXTYPE  0xffff

//    enumeration defining the OMF signature

enum CVSIG
{
    SIG02 = 0,                         // NB02 signature
    SIG05,                             // NB05 signature
    SIG06,                             // NB06 signature
    SIG07,                             // NB07 signature QCWIN 1.0 cvpacked
    SIG08,                             // NB08 signature C7.00 cvpacked
    SIG09,                             // NB08 signature C8.00 cvpacked
    SIG10,                             // NB10 signature VC 2.0
    SIG11,
    SIGOBSOLETE
};

/*
 * definition of in core list of modules
 */

typedef struct DMC                     // DM Code
{
    WORD        sa;                    // Code seg base
    long        ra;                    // Offset in code seg
    long        cb;
} DMC;


typedef struct modlist
{
    struct modlist  *next;
    WORD          iMod;
    char            *ModName;
    DWORD           ModulesAddr;
    DWORD           SymbolsAddr;
    DWORD           TypesAddr;
    DWORD           PublicsAddr;
    DWORD           PublicSymAddr;
    DWORD           SrcLnAddr;
    DWORD           SrcModuleAddr;
    DWORD           ModuleSize;
    DWORD           SymbolSize;
    DWORD           TypeSize;
    DWORD           PublicSize;
    DWORD           SrcLnSize;
    DMC             *rgdmc;
    char            style[2];          // debugging style
    WORD          dm_iov;            // Overlay number of module
    WORD          dm_ilib;           // Library name index
    WORD          dm_cSeg;           // number of segments
} modlist;

typedef modlist *PMOD;

extern  int         exefile;           // Executable file handle
extern  size_t      cbRec;
extern  long        lfoBase;           // file offset of base
extern  PMOD        ModList;           // List of module entries
extern  OMFDirEntry Libraries;         // sstLibraries directory entry
extern  OMFDirEntry GlobalPub;
extern  OMFDirEntry GlobalSym;
extern  OMFDirEntry GlobalTypes;
BYTE   RecBuf[];
extern  WORD      Sig;               // file signature


void Fatal(const wchar_t *);

void        DumpCom();
void        DumpTyp();
void        DumpSym();
void        GetBytes(void *, size_t);
const wchar_t *SzNameReg(BYTE);
const wchar_t *SzNameC7Reg(WORD);

WORD        Gets();
WORD        WGets();
DWORD       LGets();
const wchar_t *SzNameType(CV_typ_t);
const wchar_t *SzNameC7Type(CV_typ_t typ);
const wchar_t *SzNameC7Type2(CV_typ_t typ);
void        DumpModTypC6(size_t cbTyp);
void        DumpModTypC7(size_t cbTyp);
DWORD       PdbMapToken(DWORD tokenOld);

        // From cvdump.c

extern bool fCoffSymRVA;
extern bool fFileInd;
extern bool fFpo;
extern bool fFuncTokenMap;
extern bool fGPSym;
extern bool fHdr;
extern bool fIDs;
extern bool fILLines;
extern bool fInlineeLines;
extern bool fLines;
extern bool fMergedAsmInput;
extern bool fMod;
extern bool fNum;
extern bool fOmapf;
extern bool fOmapt;
extern bool fPdata;
extern bool fPdbPath;
extern bool fPub;
extern bool fRaw;
extern bool fSecContribs;
extern bool fSectionHdr;
extern bool fSegMap;
extern bool fSrcFiles;
extern bool fStatics;
extern bool fStringTable;
extern bool fSTSym;
extern bool fSym;
extern bool fTokenMap;
extern bool fTyp;
extern bool fTypMW;
extern bool fTypTokenMap;
extern bool fXdata;
extern bool fXFixup;
extern bool fXModExportIDs;
extern bool fXModImportIDs;

extern bool fSupportsQueryUDT;
extern int  iModToList;
extern bool fUtf8Symbols;

void        DumpLines(const OMFSourceModule *);

        // From dumppdb.cpp

void        DumpPdb(PDB *);
void        DumpPdbFile(const wchar_t *);
void        ShowUdtTypeId(TPI *, const char *);

        // From dumpsym6.c

extern size_t cchIndent;

        // From dumpsym7.cpp

const unsigned char *PrintSt(bool fSz, const unsigned char *pst, bool fNewLine = true);
const unsigned char *ShowSt(bool fSz, const wchar_t *psz, const unsigned char *pst, bool fNewLine = true);
size_t      PrintNumeric(const void *);
size_t      SkipNumeric(const void *);
void        DumpModSymC6(size_t);
void        DumpModSymC7(size_t, DWORD);
void        DumpModStringTable(size_t, DWORD);
void        DumpModC13Lines(size_t, DWORD);
void        DumpModInlineeSourceLines(size_t, PB);
void        DumpModFileChecksums(size_t, DWORD);
void        DumpModFramedata(size_t, DWORD);
void        DumpModCrossScopeRefs(size_t, PB);
void        DumpModCrossScopeExports(size_t, PB);
void        DumpModILLines(size_t, DWORD);
void        DumpModFuncTokenMap(size_t, PB);
void        DumpModTypeTokenMap(size_t, PB);
void        DumpModMergedAssemblyInput(size_t, PB);
void        DumpOneSymC7(Mod *, const void *, DWORD);
void        DumpGlobal(const wchar_t *pszTitle, const OMFDirEntry *pDir);


         // From type6.c

extern const wchar_t * const display[4];
extern const wchar_t * const notdisplay[4];

         // From type7.cpp

CV_typ_t DumpTypRecC7(CV_typ_t, WORD, BYTE *, TPI *, PDB *);


struct OMAP
{
    DWORD       rva;
    DWORD       rvaTo;
};

struct XFIXUP
{
   WORD wType;
   WORD wExtra;
   DWORD rva;
   DWORD rvaTarget;
};
