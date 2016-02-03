#pragma once

#include <stdio.h>

#ifdef _M_IX86

FILE *PDB_wfsopen(const wchar_t *wszPath, const wchar_t *wszMode, int shflag);
wchar_t *PDB_wfullpath(__out_ecount(maxlen) wchar_t *wszFullpath, const wchar_t *wszPath, size_t maxlen);

#else   // !_M_IX86

#define PDB_wfsopen _wfsopen
#define PDB_wfullpath _wfullpath

#endif  // !_M_IX86

#ifdef _CRT_ALTERNATIVE_INLINES

errno_t __cdecl PDB_wdupenv_s(_Deref_out_opt_z_ wchar_t **pwszDest, size_t * pcchDest, const wchar_t *wszVarName);

#else

#define PDB_wdupenv_s _wdupenv_s

#endif
