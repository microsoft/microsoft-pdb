/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

void OutputInit();

int StdOutFlush();
int __cdecl StdOutPrintf(const wchar_t *, ...);
int StdOutPutc(wchar_t);
int StdOutPuts(const wchar_t *);
int StdOutVprintf(const wchar_t *, va_list);
