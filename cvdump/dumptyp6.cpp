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

static const wchar_t * const nametype[] =               // The primitive types
{
    L"CHAR",                           //  0    8 bit signed
    L"SHORT",                          //  1   16 bit signed
    L"LONG",                           //  2   32 bit signed
    L"???",
    L"UCHAR",                          //  4    8 bit unsigned
    L"USHORT",                         //  5   16 bit unsigned
    L"ULONG",                          //  6   32 bit unsigned
    L"???",
    L"REAL32",                         //  8   32 bit real
    L"REAL64",                         //  9   64 bit real
    L"REAL80",                         //  10  80 bit real
    L"???",
    L"CPLX32",                         //  12   8 byte complex
    L"CPLX64",                         //  13  16 byte complex
    L"CPLX80",                         //  14  20 byte complex
    L"???",
    L"BOOL08",                         //  16   8 bit boolean
    L"BOOL16",                         //  17  16 bit boolean
    L"BOOL32",                         //  18  32 bit boolean
    L"???",
    L"ASCII",                          //  20  1 byte character
    L"ASCII2",                         //  21  2 byte characters
    L"ASCII4",                         //  22  4 byte characters
    L"BSTRING",                        //  23  BASIC string
    L"???",
    L"???",
    L"???",
    L"???",
    L"VOID",                           //  28  VOID
    L"???",
    L"???",
    L"???",
    L"PCHAR",                          //  32  near pointer to  8 bit signed
    L"PSHORT",                         //  33  near pointer to 16 bit signed
    L"PLONG",                          //  34  near pointer to 32 bit signed
    L"???",
    L"PUCHAR",                         //  36  near pointer to  8 bit unsigned
    L"PUSHORT",                        //  37  near pointer to 16 bit unsigned
    L"PULONG",                         //  38  near pointer to 32 bit unsigned
    L"???",
    L"PREAL32",                        //  40  near pointer to 32 bit real
    L"PREAL64",                        //  41  near pointer to 64 bit real
    L"PREAL80",                        //  42  near pointer to 80 bit real
    L"???",
    L"PCPLX32",                        //  44  near pointer to  8 byte complex
    L"PCPLX64",                        //  45  near pointer to 16 byte complex
    L"PCPLX80",                        //  46  near pointer to 20 byte complex
    L"???",
    L"PBOOL08",                        //  48  near pointer to  8 bit boolean
    L"PBOOL16",                        //  49  near pointer to 16 bit boolean
    L"PBOOL32",                        //  50  near pointer to 32 bit boolean
    L"???",
    L"PASCII",                         //  52  near pointer to 1 byte character
    L"PASCII2",                        //  53  near pointer to 2 byte characters
    L"PASCII4",                        //  54  near pointer to 4 byte characters
    L"PBSTRING",                       //  55  near pointer to BASIC string
    L"???",
    L"???",
    L"???",
    L"???",
    L"PVOID",                          //  60  near pointer to VOID
    L"???",
    L"???",
    L"???",
    L"PFCHAR",                         //  64  far pointer to  8 bit signed
    L"PFSHORT",                        //  65  far pointer to 16 bit signed
    L"PFLONG",                         //  66  far pointer to 32 bit signed
    L"???",
    L"PFUCHAR",                        //  68  far pointer to  8 bit unsigned
    L"PFUSHORT",                       //  69  far pointer to 16 bit unsigned
    L"PFULONG",                        //  70  far pointer to 32 bit unsigned
    L"???",
    L"PFREAL32",                       //  72  far pointer to 32 bit real
    L"PFREAL64",                       //  73  far pointer to 64 bit real
    L"PFREAL80",                       //  74  far pointer to 80 bit real
    L"???",
    L"PFCPLX32",                       //  76  far pointer to  8 byte complex
    L"PFCPLX64",                       //  77  far pointer to 16 byte complex
    L"PFCPLX80",                       //  78  far pointer to 20 byte complex
    L"???",
    L"PFBOOL08",                       //  80  far pointer to  8 bit boolean
    L"PFBOOL16",                       //  81  far pointer to 16 bit boolean
    L"PFBOOL32",                       //  82  far pointer to 32 bit boolean
    L"???",
    L"PFASCII",                        //  84  far pointer to 1 byte character
    L"PFASCII2",                       //  85  far pointer to 2 byte characters
    L"PFASCII4",                       //  86  far pointer to 4 byte characters
    L"PFBSTRING",                      //  87  far pointer to BASIC string
    L"???",
    L"???",
    L"???",
    L"???",
    L"PFVOID",                         //  92  far pointer to VOID
    L"???",
    L"???",
    L"???",
    L"PHCHAR",                         //  96  huge pointer to  8 bit signed
    L"PHSHORT",                        //  97  huge pointer to 16 bit signed
    L"PHLONG",                         //  98  huge pointer to 32 bit signed
    L"???",
    L"PHUCHAR",                        //  100 huge pointer to  8 bit unsigned
    L"PHUSHORT",                       //  101 huge pointer to 16 bit unsigned
    L"PHULONG",                        //  102 huge pointer to 32 bit unsigned
    L"???",
    L"PHREAL32",                       //  104 huge pointer to 32 bit real
    L"PHREAL64",                       //  105 huge pointer to 64 bit real
    L"PHREAL80",                       //  106 huge pointer to 80 bit real
    L"???",
    L"PHCPLX32",                       //  108 huge pointer to  8 byte complex
    L"PHCPLX64",                       //  109 huge pointer to 16 byte complex
    L"PHCPLX80",                       //  110 huge pointer to 20 byte complex
    L"???",
    L"PHBOOL08",                       //  112 huge pointer to  8 bit boolean
    L"PHBOOL16",                       //  113 huge pointer to 16 bit boolean
    L"PHBOOL32",                       //  114 huge pointer to 32 bit boolean
    L"???",
    L"PHASCII",                        //  116 huge pointer to 1 byte character
    L"PHASCII2",                       //  117 huge pointer to 2 byte characters
    L"PHASCII4",                       //  118 huge pointer to 4 byte characters
    L"BHBSTRING",                      //  119 huge pointer to BASIC string
    L"???",
    L"???",
    L"???",
    L"???",
    L"PHVOID"                          //  124 huge pointer to VOID
  };


#define T_NOTYPE	0x0000		// uncharacterized type
#define T_ABS		0x0001		// absolute symbol
#define T_SEGMENT	0x0002		// segment symbol

const wchar_t *SzNameType(CV_typ_t typ)
{
    static wchar_t buf[16];

    if (typ > 511) {        // Not primitive
        swprintf_s(buf, _countof(buf), L"%u", typ);

        return(buf);
    }

    switch (typ) {
        case T_ABS:
            return(L"ABS");

        case T_NOTYPE:
            return(L"");

        case T_SEGMENT:
            return(L"SEG");

        default:
            if ((typ & 0xff00) || !(typ & 0x80)) {
                return(L"?unknown-type?");
            }
    }

    return(nametype[typ & 0x7f]);
}
