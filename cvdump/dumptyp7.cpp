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

#ifndef CC_BIGINT
#define CC_BIGINT 1
#endif


typedef struct TYPNAME
{
    CV_typ_t typind;                   // Constant value
    const wchar_t *name;               // Name of constant used to define
} TYPNAME;

// Used to relate typeind to constant name
#define MAKE_TYPNAME(typ)  { typ, L#typ }


// A lookup table is used because speed is not important but ease
// of modification is.
const TYPNAME typnameC7[] = {
//      Special Types

        MAKE_TYPNAME(T_NOTYPE),
        MAKE_TYPNAME(T_ABS),
        MAKE_TYPNAME(T_SEGMENT),
        MAKE_TYPNAME(T_VOID),
        MAKE_TYPNAME(T_HRESULT),
        MAKE_TYPNAME(T_32PHRESULT),
        MAKE_TYPNAME(T_64PHRESULT),
        MAKE_TYPNAME(T_PVOID),
        MAKE_TYPNAME(T_PFVOID),
        MAKE_TYPNAME(T_PHVOID),
        MAKE_TYPNAME(T_32PVOID),
        MAKE_TYPNAME(T_32PFVOID),
        MAKE_TYPNAME(T_64PVOID),
        MAKE_TYPNAME(T_CURRENCY),
        MAKE_TYPNAME(T_NBASICSTR),
        MAKE_TYPNAME(T_FBASICSTR),
        MAKE_TYPNAME(T_NOTTRANS),
        MAKE_TYPNAME(T_BIT),
        MAKE_TYPNAME(T_PASCHAR),


//      Character types

        MAKE_TYPNAME(T_CHAR),
        MAKE_TYPNAME(T_PCHAR),
        MAKE_TYPNAME(T_PFCHAR),
        MAKE_TYPNAME(T_PHCHAR),
        MAKE_TYPNAME(T_32PCHAR),
        MAKE_TYPNAME(T_32PFCHAR),
        MAKE_TYPNAME(T_64PCHAR),

        MAKE_TYPNAME(T_UCHAR),
        MAKE_TYPNAME(T_PUCHAR),
        MAKE_TYPNAME(T_PFUCHAR),
        MAKE_TYPNAME(T_PHUCHAR),
        MAKE_TYPNAME(T_32PUCHAR),
        MAKE_TYPNAME(T_32PFUCHAR),
        MAKE_TYPNAME(T_64PUCHAR),


//      really a character types

        MAKE_TYPNAME(T_RCHAR),
        MAKE_TYPNAME(T_PRCHAR),
        MAKE_TYPNAME(T_PFRCHAR),
        MAKE_TYPNAME(T_PHRCHAR),
        MAKE_TYPNAME(T_32PRCHAR),
        MAKE_TYPNAME(T_32PFRCHAR),
        MAKE_TYPNAME(T_64PRCHAR),


//      really a wide character types

        MAKE_TYPNAME(T_WCHAR),
        MAKE_TYPNAME(T_PWCHAR),
        MAKE_TYPNAME(T_PFWCHAR),
        MAKE_TYPNAME(T_PHWCHAR),
        MAKE_TYPNAME(T_32PWCHAR),
        MAKE_TYPNAME(T_32PFWCHAR),
        MAKE_TYPNAME(T_64PWCHAR),

//      really a 16-bit unicode char

        MAKE_TYPNAME(T_CHAR16),
        MAKE_TYPNAME(T_PCHAR16),
        MAKE_TYPNAME(T_PFCHAR16),
        MAKE_TYPNAME(T_PHCHAR16),
        MAKE_TYPNAME(T_32PCHAR16),
        MAKE_TYPNAME(T_32PFCHAR16),
        MAKE_TYPNAME(T_64PCHAR16),

//      really a 32-bit unicode char

        MAKE_TYPNAME(T_CHAR32),
        MAKE_TYPNAME(T_PCHAR32),
        MAKE_TYPNAME(T_PFCHAR32),
        MAKE_TYPNAME(T_PHCHAR32),
        MAKE_TYPNAME(T_32PCHAR32),
        MAKE_TYPNAME(T_32PFCHAR32),
        MAKE_TYPNAME(T_64PCHAR32),

//      8 bit int types

        MAKE_TYPNAME(T_INT1),
        MAKE_TYPNAME(T_PINT1),
        MAKE_TYPNAME(T_PFINT1),
        MAKE_TYPNAME(T_PHINT1),
        MAKE_TYPNAME(T_32PINT1),
        MAKE_TYPNAME(T_32PFINT1),
        MAKE_TYPNAME(T_64PINT1),

        MAKE_TYPNAME(T_UINT1),
        MAKE_TYPNAME(T_PUINT1),
        MAKE_TYPNAME(T_PFUINT1),
        MAKE_TYPNAME(T_PHUINT1),
        MAKE_TYPNAME(T_32PUINT1),
        MAKE_TYPNAME(T_32PFUINT1),
        MAKE_TYPNAME(T_64PUINT1),


//      16 bit short types

        MAKE_TYPNAME(T_SHORT),
        MAKE_TYPNAME(T_PSHORT),
        MAKE_TYPNAME(T_PFSHORT),
        MAKE_TYPNAME(T_PHSHORT),
        MAKE_TYPNAME(T_32PSHORT),
        MAKE_TYPNAME(T_32PFSHORT),
        MAKE_TYPNAME(T_64PSHORT),

        MAKE_TYPNAME(T_USHORT),
        MAKE_TYPNAME(T_PUSHORT),
        MAKE_TYPNAME(T_PFUSHORT),
        MAKE_TYPNAME(T_PHUSHORT),
        MAKE_TYPNAME(T_32PUSHORT),
        MAKE_TYPNAME(T_32PFUSHORT),
        MAKE_TYPNAME(T_64PUSHORT),


//      16 bit int types

        MAKE_TYPNAME(T_INT2),
        MAKE_TYPNAME(T_PINT2),
        MAKE_TYPNAME(T_PFINT2),
        MAKE_TYPNAME(T_PHINT2),
        MAKE_TYPNAME(T_32PINT2),
        MAKE_TYPNAME(T_32PFINT2),
        MAKE_TYPNAME(T_64PINT2),

        MAKE_TYPNAME(T_UINT2),
        MAKE_TYPNAME(T_PUINT2),
        MAKE_TYPNAME(T_PFUINT2),
        MAKE_TYPNAME(T_PHUINT2),
        MAKE_TYPNAME(T_32PUINT2),
        MAKE_TYPNAME(T_32PFUINT2),
        MAKE_TYPNAME(T_64PUINT2),


//      32 bit long types

        MAKE_TYPNAME(T_LONG),
        MAKE_TYPNAME(T_ULONG),
        MAKE_TYPNAME(T_PLONG),
        MAKE_TYPNAME(T_PULONG),
        MAKE_TYPNAME(T_PFLONG),
        MAKE_TYPNAME(T_PFULONG),
        MAKE_TYPNAME(T_PHLONG),
        MAKE_TYPNAME(T_PHULONG),

        MAKE_TYPNAME(T_32PLONG),
        MAKE_TYPNAME(T_32PULONG),
        MAKE_TYPNAME(T_32PFLONG),
        MAKE_TYPNAME(T_32PFULONG),
        MAKE_TYPNAME(T_64PLONG),
        MAKE_TYPNAME(T_64PULONG),


//      32 bit int types

        MAKE_TYPNAME(T_INT4),
        MAKE_TYPNAME(T_PINT4),
        MAKE_TYPNAME(T_PFINT4),
        MAKE_TYPNAME(T_PHINT4),
        MAKE_TYPNAME(T_32PINT4),
        MAKE_TYPNAME(T_32PFINT4),
        MAKE_TYPNAME(T_64PINT4),

        MAKE_TYPNAME(T_UINT4),
        MAKE_TYPNAME(T_PUINT4),
        MAKE_TYPNAME(T_PFUINT4),
        MAKE_TYPNAME(T_PHUINT4),
        MAKE_TYPNAME(T_32PUINT4),
        MAKE_TYPNAME(T_32PFUINT4),
        MAKE_TYPNAME(T_64PUINT4),


//      64 bit quad types

        MAKE_TYPNAME(T_QUAD),
        MAKE_TYPNAME(T_PQUAD),
        MAKE_TYPNAME(T_PFQUAD),
        MAKE_TYPNAME(T_PHQUAD),
        MAKE_TYPNAME(T_32PQUAD),
        MAKE_TYPNAME(T_32PFQUAD),
        MAKE_TYPNAME(T_64PQUAD),

        MAKE_TYPNAME(T_UQUAD),
        MAKE_TYPNAME(T_PUQUAD),
        MAKE_TYPNAME(T_PFUQUAD),
        MAKE_TYPNAME(T_PHUQUAD),
        MAKE_TYPNAME(T_32PUQUAD),
        MAKE_TYPNAME(T_32PFUQUAD),
        MAKE_TYPNAME(T_64PUQUAD),


//      64 bit int types

        MAKE_TYPNAME(T_INT8),
        MAKE_TYPNAME(T_PINT8),
        MAKE_TYPNAME(T_PFINT8),
        MAKE_TYPNAME(T_PHINT8),
        MAKE_TYPNAME(T_32PINT8),
        MAKE_TYPNAME(T_32PFINT8),
        MAKE_TYPNAME(T_64PINT8),

        MAKE_TYPNAME(T_UINT8),
        MAKE_TYPNAME(T_PUINT8),
        MAKE_TYPNAME(T_PFUINT8),
        MAKE_TYPNAME(T_PHUINT8),
        MAKE_TYPNAME(T_32PUINT8),
        MAKE_TYPNAME(T_32PFUINT8),
        MAKE_TYPNAME(T_64PUINT8),


//      128 bit octet types

        MAKE_TYPNAME(T_OCT),
        MAKE_TYPNAME(T_POCT),
        MAKE_TYPNAME(T_PFOCT),
        MAKE_TYPNAME(T_PHOCT),
        MAKE_TYPNAME(T_32POCT),
        MAKE_TYPNAME(T_32PFOCT),
        MAKE_TYPNAME(T_64POCT),

        MAKE_TYPNAME(T_UOCT),
        MAKE_TYPNAME(T_PUOCT),
        MAKE_TYPNAME(T_PFUOCT),
        MAKE_TYPNAME(T_PHUOCT),
        MAKE_TYPNAME(T_32PUOCT),
        MAKE_TYPNAME(T_32PFUOCT),
        MAKE_TYPNAME(T_64PUOCT),


//      128 bit int types

        MAKE_TYPNAME(T_INT16),
        MAKE_TYPNAME(T_PINT16),
        MAKE_TYPNAME(T_PFINT16),
        MAKE_TYPNAME(T_PHINT16),
        MAKE_TYPNAME(T_32PINT16),
        MAKE_TYPNAME(T_32PFINT16),
        MAKE_TYPNAME(T_64PINT16),

        MAKE_TYPNAME(T_UINT16),
        MAKE_TYPNAME(T_PUINT16),
        MAKE_TYPNAME(T_PFUINT16),
        MAKE_TYPNAME(T_PHUINT16),
        MAKE_TYPNAME(T_32PUINT16),
        MAKE_TYPNAME(T_32PFUINT16),
        MAKE_TYPNAME(T_64PUINT16),


//      32 bit real types

        MAKE_TYPNAME(T_REAL32),
        MAKE_TYPNAME(T_PREAL32),
        MAKE_TYPNAME(T_PFREAL32),
        MAKE_TYPNAME(T_PHREAL32),
        MAKE_TYPNAME(T_32PREAL32),
        MAKE_TYPNAME(T_32PFREAL32),
        MAKE_TYPNAME(T_64PREAL32),


//      48 bit real types

        MAKE_TYPNAME(T_REAL48),
        MAKE_TYPNAME(T_PREAL48),
        MAKE_TYPNAME(T_PFREAL48),
        MAKE_TYPNAME(T_PHREAL48),
        MAKE_TYPNAME(T_32PREAL48),
        MAKE_TYPNAME(T_32PFREAL48),
        MAKE_TYPNAME(T_64PREAL48),


//      64 bit real types

        MAKE_TYPNAME(T_REAL64),
        MAKE_TYPNAME(T_PREAL64),
        MAKE_TYPNAME(T_PFREAL64),
        MAKE_TYPNAME(T_PHREAL64),
        MAKE_TYPNAME(T_32PREAL64),
        MAKE_TYPNAME(T_32PFREAL64),
        MAKE_TYPNAME(T_64PREAL64),


//      80 bit real types

        MAKE_TYPNAME(T_REAL80),
        MAKE_TYPNAME(T_PREAL80),
        MAKE_TYPNAME(T_PFREAL80),
        MAKE_TYPNAME(T_PHREAL80),
        MAKE_TYPNAME(T_32PREAL80),
        MAKE_TYPNAME(T_32PFREAL80),
        MAKE_TYPNAME(T_64PREAL80),


//      128 bit real types

        MAKE_TYPNAME(T_REAL128),
        MAKE_TYPNAME(T_PREAL128),
        MAKE_TYPNAME(T_PFREAL128),
        MAKE_TYPNAME(T_PHREAL128),
        MAKE_TYPNAME(T_32PREAL128),
        MAKE_TYPNAME(T_32PFREAL128),
        MAKE_TYPNAME(T_64PREAL128),


//      32 bit complex types

        MAKE_TYPNAME(T_CPLX32),
        MAKE_TYPNAME(T_PCPLX32),
        MAKE_TYPNAME(T_PFCPLX32),
        MAKE_TYPNAME(T_PHCPLX32),
        MAKE_TYPNAME(T_32PCPLX32),
        MAKE_TYPNAME(T_32PFCPLX32),
        MAKE_TYPNAME(T_64PCPLX32),


//      64 bit complex types

        MAKE_TYPNAME(T_CPLX64),
        MAKE_TYPNAME(T_PCPLX64),
        MAKE_TYPNAME(T_PFCPLX64),
        MAKE_TYPNAME(T_PHCPLX64),
        MAKE_TYPNAME(T_32PCPLX64),
        MAKE_TYPNAME(T_32PFCPLX64),
        MAKE_TYPNAME(T_64PCPLX64),


//      80 bit complex types

        MAKE_TYPNAME(T_CPLX80),
        MAKE_TYPNAME(T_PCPLX80),
        MAKE_TYPNAME(T_PFCPLX80),
        MAKE_TYPNAME(T_PHCPLX80),
        MAKE_TYPNAME(T_32PCPLX80),
        MAKE_TYPNAME(T_32PFCPLX80),
        MAKE_TYPNAME(T_64PCPLX80),


//      128 bit complex types

        MAKE_TYPNAME(T_CPLX128),
        MAKE_TYPNAME(T_PCPLX128),
        MAKE_TYPNAME(T_PFCPLX128),
        MAKE_TYPNAME(T_PHCPLX128),
        MAKE_TYPNAME(T_32PCPLX128),
        MAKE_TYPNAME(T_32PFCPLX128),
        MAKE_TYPNAME(T_64PCPLX128),


//      boolean types

        MAKE_TYPNAME(T_BOOL08),
        MAKE_TYPNAME(T_PBOOL08),
        MAKE_TYPNAME(T_PFBOOL08),
        MAKE_TYPNAME(T_PHBOOL08),
        MAKE_TYPNAME(T_32PBOOL08),
        MAKE_TYPNAME(T_32PFBOOL08),
        MAKE_TYPNAME(T_64PBOOL08),

        MAKE_TYPNAME(T_BOOL16),
        MAKE_TYPNAME(T_PBOOL16),
        MAKE_TYPNAME(T_PFBOOL16),
        MAKE_TYPNAME(T_PHBOOL16),
        MAKE_TYPNAME(T_32PBOOL16),
        MAKE_TYPNAME(T_32PFBOOL16),
        MAKE_TYPNAME(T_64PBOOL16),

        MAKE_TYPNAME(T_BOOL32),
        MAKE_TYPNAME(T_PBOOL32),
        MAKE_TYPNAME(T_PFBOOL32),
        MAKE_TYPNAME(T_PHBOOL32),
        MAKE_TYPNAME(T_32PBOOL32),
        MAKE_TYPNAME(T_32PFBOOL32),
        MAKE_TYPNAME(T_64PBOOL32),

        MAKE_TYPNAME(T_BOOL64),
        MAKE_TYPNAME(T_PBOOL64),
        MAKE_TYPNAME(T_PFBOOL64),
        MAKE_TYPNAME(T_PHBOOL64),
        MAKE_TYPNAME(T_32PBOOL64),
        MAKE_TYPNAME(T_32PFBOOL64),
        MAKE_TYPNAME(T_64PBOOL64),

        MAKE_TYPNAME(T_BOOL32FF),


//      ???

        MAKE_TYPNAME(T_NCVPTR),
        MAKE_TYPNAME(T_FCVPTR),
        MAKE_TYPNAME(T_HCVPTR),
        MAKE_TYPNAME(T_32NCVPTR),
        MAKE_TYPNAME(T_32FCVPTR),
        MAKE_TYPNAME(T_64NCVPTR),
};


const wchar_t *SzNameC7Type(CV_typ_t type)
{
    static wchar_t buf[40];
    const wchar_t *szName;
    int i;

    if (type >= CV_FIRST_NONPRIM) {        // Not primitive
        swprintf_s(buf, _countof(buf), L"0x%0*X", type > 0xffff ? 8 : 4, type);

        return(buf);
    }

    szName = L"???";

    for (i = 0; i < _countof(typnameC7); i++) {
        if (typnameC7[i].typind == type) {
            szName = typnameC7[i].name;
            break;
        }
    }

    swprintf_s(buf, _countof(buf), L"%s(%04X)", szName, type);

    return(buf);
}


// Right justifies the type name
const wchar_t *SzNameC7Type2(CV_typ_t type)
{
    static wchar_t buf2[40];
    const wchar_t *szName;
    int i;

    if (type >= CV_FIRST_NONPRIM) {        // Not primitive
        size_t cchX = type > 0xffff ? 8 : 4;
        size_t cchSpace = 18 - 2 - cchX;

        swprintf_s(buf2, _countof(buf2), L"%*s0x%0*X", cchSpace, L"", cchX, type);

        return(buf2);
    }

    szName = L"???";

    for (i = 0; i < sizeof(typnameC7) / sizeof(typnameC7[0]); i++) {
        if (typnameC7[i].typind == type) {
            szName = typnameC7[i].name;
            break;
        }
    }

    swprintf_s(buf2, _countof(buf2), L"%12s(%04X)", szName, type);

    return(buf2);
}
