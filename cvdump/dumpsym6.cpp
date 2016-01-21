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

#include "debsym.h"         // SYMBOLS definitions
#include "symrec.h"         // SYMBOLS definitions


void C6BlockSym();
void C6ProcSym();
void C6LabSym();
void C6WithSym();
void C6EntrySym();
void C6SkipSym();
void C6ChangeModel();
void C6CodeSegSym();
void C6EndSym();
void C6BpSym();
void C6RegSym();
void C6ConSym();
void C6CobolTypeDefSym();
void C6LocalSym();
void C6ChangeModelSym();


bool    f386;
size_t  cchIndent;
#define GetOffset() (f386 ? LGets() : (long) WGets())

typedef struct {
    BYTE   tsym;
    void (*pfcn)();
} symfcn;

const symfcn SymFcnC6[] =
{
    { S_BLOCK,      C6BlockSym          },
    { S_PROC,       C6ProcSym           },
    { S_END,        C6EndSym            },
    { S_BPREL,      C6BpSym             },
    { S_LOCAL,      C6LocalSym          },
    { S_LABEL,      C6LabSym            },
    { S_WITH,       C6WithSym           },
    { S_REG,        C6RegSym            },
    { S_CONST,      C6ConSym            },
    { S_ENTRY,      C6EntrySym          },
    { S_NOOP,       C6SkipSym           },
    { S_CODSEG,     C6CodeSegSym        },
    { S_TYPEDEF,    C6CobolTypeDefSym   },
    { S_CHGMODEL,   C6ChangeModelSym    },
};

#define SYMCNT (sizeof SymFcnC6 / sizeof (SymFcnC6[0]))


/**
 *
 *   Display SYMBOLS section.
 *
 */

void DumpSym()
{
    StdOutPuts(L"\n\n*** SYMBOLS section\n");

    for (PMOD pMod = ModList; pMod != NULL; pMod = pMod->next) {
        DWORD cbSym;

        if (((cbSym = pMod->SymbolSize) != 0) &&
            ((iModToList == 0) || ((WORD)iModToList == pMod->iMod))) {
            _lseek(exefile, lfoBase + pMod->SymbolsAddr, SEEK_SET);

            cchIndent = 0;

            char szName[256];

            strcpy_s(szName, _countof(szName), pMod->ModName);
            StdOutPrintf(L"%S\n", szName);

            cbRec = 4;

            DWORD ulSignature = LGets();

            switch( ulSignature ){
                case CV_SIGNATURE_C7:
                case CV_SIGNATURE_C11:
                    // Dump C7 debug info
                    cbSym -= sizeof (DWORD);    // Subtract size of signature
                    DumpModSymC7 (cbSym, sizeof(DWORD));
                    break;

                default:
                    // Symbols are in C6 format
                    // M00 - seek could be eliminated for speed improvement
                    // Re-seek because first four bytes are not signature
                    _lseek(exefile, lfoBase + pMod->SymbolsAddr, SEEK_SET);
                    DumpModSymC6 (cbSym);
                    break;
            }
        }
    }

    StdOutPutc(L'\n');
}


void DumpModSymC6(size_t cbSym)
{
    char        sb[255];    // String buffer
    WORD        cbName;     // Length of string
    unsigned    i;

    while (cbSym > 0) {
        WORD rect;

        // Get record length
        cbRec = 1;
        cbRec = Gets();
        cbSym -= cbRec + 1;
        rect = Gets();
        f386 = (rect & 0x80) != 0;    // check for 32-bit sym
        rect &= ~0x80;
        for (i = 0; i < SYMCNT; i++) {
            if (SymFcnC6[i].tsym == (BYTE) rect) {
                SymFcnC6[i].pfcn ();
                break;
            }
        }
        if( i == SYMCNT ){
            // Couldn't find symbol record type in the table
            Fatal(L"Invalid symbol record type");
        }
        if (cbRec > 0) {
            // display symbol name
            cbName = Gets();
            GetBytes(sb, (size_t) cbName);
            sb[cbName] = '\0';
            StdOutPrintf(L"\t%s", sb);
        }
        StdOutPutc(L'\n');
        if (cbRec) {
            Fatal(L"Invalid file");
        }
        StdOutPutc(L'\n');
    }
}


void PrtIndent()
{
    size_t n;

    for (n = 0; n < cchIndent; n++) {
        StdOutPutc(L' ');
    }
}


void C6EndSym()
{
    cchIndent--;
    PrtIndent();
    StdOutPuts(L"End");
}


void C6BpSym()
{
    BPSYMTYPE       bp;

    bp.off = GetOffset ();
    bp.typind = WGets();
    PrtIndent();
    if (f386) {
        StdOutPrintf(L"BP-relative:\toff = %08lx, type %8s", bp.off, SzNameType(bp.typind));
    }
    else {
        StdOutPrintf(L"BP-relative:\toff = %04lx, type %8s", bp.off, SzNameType(bp.typind));
    }
}


void C6LocalSym()
{
    LOCSYMTYPE  loc;

    loc.off = GetOffset ();
    loc.seg = WGets();
    loc.typind = WGets();
    PrtIndent();
    StdOutPrintf(L"Local:\tseg:off = %04x:%0*lx",
            loc.seg,
            f386? 8 : 4,
            loc.off
            );
    StdOutPrintf(L", type %8s, ", SzNameType(loc.typind));
}


const wchar_t * const namereg[] =
{
    L"AL",        // 0
    L"CL",        // 1
    L"DL",        // 2
    L"BL",        // 3
    L"AH",        // 4
    L"CH",        // 5
    L"DH",        // 6
    L"BH",        // 7
    L"AX",        // 8
    L"CX",        // 9
    L"DX",        // 10
    L"BX",        // 11
    L"SP",        // 12
    L"BP",        // 13
    L"SI",        // 14
    L"DI",        // 15
    L"EAX",       // 16
    L"ECX",       // 17
    L"EDX",       // 18
    L"EBX",       // 19
    L"ESP",       // 20
    L"EBP",       // 21
    L"ESI",       // 22
    L"EDI",       // 23
    L"ES",        // 24
    L"CS",        // 25
    L"SS",        // 26
    L"DS",        // 27
    L"FS",        // 28
    L"GS",        // 29
    L"???",       // 30
    L"???",       // 31
    L"DX:AX",     // 32
    L"ES:BX",     // 33
    L"IP",        // 34
    L"FLAGS",     // 35

};

const wchar_t * const name87[] =
{
    L"ST (0)",
    L"ST (1)",
    L"ST (2)",
    L"ST (3)",
    L"ST (4)",
    L"ST (5)",
    L"ST (6)",
    L"ST (7)"
};

const wchar_t *SzNameReg(BYTE reg)
{
    if (reg <= 37) {
        return(namereg[reg]);
    }

    if (reg < 128 || reg > 135) {
        return(L"???");
    }

    return(name87[reg - 128]);
}

void C6RegSym()
{
    REGSYMTYPE regsym;

    regsym.typind = WGets();
    regsym.reg = (char) Gets();

    PrtIndent();
    StdOutPrintf(L"Register:\ttype %8s, register = %s, ",
                 SzNameType(regsym.typind),
                 SzNameReg(regsym.reg));
}

void C6ConSym()
{
    long len;
    WORD type;
    WORD code;
    static char buf[1024];

    type = WGets();
    PrtIndent();
    StdOutPrintf(L"Constant:\ttype %8s, ", SzNameType(type));

    code = Gets();
    if (code < 128) {
        len = code;
        // skip value bytes
        GetBytes(buf, (size_t) len);
    } else {
        switch (code) {
            case 133:               // unsigned word
            case 137:               // signed word
                len = WGets();
                StdOutPrintf(L"%x", (int) len);
                break;

            case 134:               // signed long
            case 138:               // unsigned long
                len = LGets();
                StdOutPrintf(L"%lx", len);
                break;

            case 136:               // signed byte
                len = Gets();
                StdOutPrintf(L"%x", (int) len);
                break;

            default:
		StdOutPuts(L"??");
                return;
                break;
        }
    }

    // now we should be at the name of the symbol
}

void C6BlockSym()
{
    BLKSYMTYPE  block;
    int             n;

    block.off = GetOffset();
    block.len = WGets();
    PrtIndent();
    cchIndent++;
    n = f386? 8 : 4;

    StdOutPrintf(L"Block Start : off = %0*lx, len = %04x",
            n, block.off, block.len);
}

void C6ProcSym()
{
    PROCSYMTYPE proc;
    int             n;

    proc.off = GetOffset ();
    proc.typind = WGets();
    proc.len = WGets();
    proc.startoff = WGets();
    proc.endoff = WGets();
    proc.res = WGets();
    proc.rtntyp = (char) Gets();


    PrtIndent();
    cchIndent++;
    n = f386? 8 : 4;

    StdOutPrintf(L"Proc Start: off = %0*lx, type %8s, len = %04x\n",
            n, proc.off, SzNameType(proc.typind), proc.len);
    StdOutPrintf(L"\tDebug start = %04x, debug end = %04x, ",
            proc.startoff, proc.endoff);
    switch (proc.rtntyp) {
        case S_NEAR:
	    StdOutPuts(L"NEAR,");
            break;
        case S_FAR:
	    StdOutPuts(L"FAR,");
            break;
        default:
	    StdOutPuts(L"???,");
        }
}

void C6LabSym ()
{
    LABSYMTYPE      dat;
    int             n;

    dat.off = GetOffset ();
    dat.rtntyp = (char) Gets();
    PrtIndent();
    n = f386? 8 : 4;

    StdOutPrintf(L"Code label: off = %0*lx,",
            n, dat.off);
    switch (dat.rtntyp) {
        case S_NEAR:
	    StdOutPuts(L"NEAR,");
            break;
        case S_FAR:
	    StdOutPuts(L"FAR,");
            break;
        default:
	    StdOutPuts(L"???,");
    }
}

void C6WithSym ()
{
    WITHSYMTYPE     dat;
    int             n;

    dat.off = GetOffset ();
    dat.len = WGets();
    PrtIndent();
    cchIndent++;
    n = f386? 8 : 4;

    StdOutPrintf(L"'With Start: off = %0*lx, len = %04x",
            n, dat.off,dat.len);
}

void C6EntrySym()
{
    PROCSYMTYPE proc;
    int             n;

    proc.off = GetOffset ();
    proc.typind = WGets();
    proc.len = WGets();
    proc.startoff = WGets();
    proc.endoff = WGets();
    proc.res = WGets();
    proc.rtntyp = (char) Gets();


    PrtIndent();
    cchIndent++;
    n = f386? 8 : 4;

    StdOutPrintf(L"FORTARN Entry: off = %0*lx, type %8s, len = %04x\n",
            n, proc.off, SzNameType(proc.typind), proc.len);
    StdOutPrintf(L"\tDebug start = %04x, debug end = %04x, ",
            proc.startoff, proc.endoff);
    switch (proc.rtntyp) {
        case S_NEAR:
	    StdOutPuts(L"NEAR,");
            break;
        case S_FAR:
	    StdOutPuts(L"FAR,");
            break;
        default:
	    StdOutPuts(L"???,");
        }
}

void C6SkipSym()
{
    StdOutPrintf(L"Skip: %d bytes\n", cbRec);

    // skip the bytes in the skip record

    while (cbRec > 0) {
        Gets();
    }
}


void C6CodeSegSym()
{
    WORD seg;
    WORD res;

    seg = WGets();
    res = WGets();

    StdOutPrintf(L"Change Default Seg: seg = %04x\n", seg);
}


void C6CobolTypeDefSym()
{
    CV_typ_t  type;
// review
    type = WGets();
    StdOutPrintf(L"Cobol Typedef: type = %d,", type);
}


void C6ChangeModelSym()
{
    WORD  offset;
    WORD  model;
    WORD  byte1;
    WORD  byte2;
    WORD  byte3;
    WORD  byte4;

    offset = WGets();
    model= Gets();
    byte1 = Gets();
    byte2 = Gets();
    byte3 = Gets();
    byte4 = Gets();

    StdOutPrintf(L"Change Model: offset = 0x%04x model = 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
                 offset, model, byte1, byte2, byte3, byte4);
}
