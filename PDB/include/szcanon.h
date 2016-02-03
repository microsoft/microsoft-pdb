//-----------------------------------------------------------------------------
//  SzCanon.h
//
//  Copyright (C) 1995, Microsoft Corporation
//
//  Purpose: Perform filename canonicalizations
//
//  Revision History:
//
//  []      09/01/95    Dans    Created
//
//-----------------------------------------------------------------------------

#pragma once

#include <stdlib.h>

class CCanonFile {
    public:

        static wchar_t *
        SzCanonFilename( __inout_z wchar_t *szFilename);

        static wchar_t * 
        SzFullCanonFilename(__in_z const wchar_t *szFile,  __ecount(cch) wchar_t *szCanonFile, size_t cch);
};
