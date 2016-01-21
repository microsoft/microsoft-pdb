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


bool fStdOutConsole;


bool FIsConsole(FILE *fd)
{
    int fh = _fileno(fd);

    HANDLE hFile = (HANDLE) _get_osfhandle(fh);

    DWORD dwType = GetFileType(hFile);

    dwType &= ~FILE_TYPE_REMOTE;

    if (dwType != FILE_TYPE_CHAR) {
        return false;
    }

    DWORD dwMode;

    if (!GetConsoleMode(hFile, &dwMode)) {
        return false;
    }

    return true;
}


void OutputInit()
{
    if (FIsConsole(stdout)) {
        fStdOutConsole = true;
    }
}


int StdOutFlush()
{
    if (fStdOutConsole) {
        return 0;
    }

    return fflush(stdout);
}


int __cdecl StdOutPrintf(const wchar_t *szFormat, ...)
{
    va_list valist;

    va_start(valist, szFormat);

    int ret = StdOutVprintf(szFormat, valist);

    va_end(valist);

    return ret;
}


int StdOutPutc(wchar_t ch)
{
    if (fStdOutConsole) {
        return _putwch(ch);
    }

    return fputwc(ch, stdout);
}


int StdOutPuts(const wchar_t *sz)
{
    if (fStdOutConsole) {
        return _cputws(sz);
    }

    return fputws(sz, stdout);
}


int StdOutVprintf(const wchar_t *szFormat, va_list valist)
{
    if (fStdOutConsole) {
        return _vcwprintf(szFormat, valist);
    }

    return vfwprintf(stdout, szFormat, valist);
}
