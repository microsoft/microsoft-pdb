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

#define BYTELN          8
#define WORDLN          16
typedef unsigned short  WORD;


void InvalidObject()
{
    Fatal(L"Invalid file");
}


WORD Gets()
{
    WORD b;                          // A byte of input

    if (((_read(exefile, &b, 1)) != 1) || cbRec < 1) {
        InvalidObject();
    }

    --cbRec;

    return (b & 0xff);
}


void GetBytes(void *pv, size_t n)
{
    if ((size_t) _read(exefile, pv, (unsigned int) n) != n) {
        InvalidObject();
    }

    cbRec -= n;
}


WORD WGets()
{
    WORD w;                            /* Word of input */

    w = Gets();                        /* Get low-order byte */
    return (w | (Gets() << BYTELN));   /* Return word */
}



DWORD LGets()
{
    DWORD ul;

    ul = (DWORD) WGets();

    return (ul | ((DWORD) WGets() << WORDLN));
}
