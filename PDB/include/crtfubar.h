#pragma once

// Fix up the CRT's FUBAR'ed _[w]sopen madness; we still have to link/run for/on
// down-level OS versions of the CRT DLL

// Produce prototypes matching the CRT but with different names (C linkage too!)

extern "C" {
    int _ACRTIMP __cdecl _pdb_sopen(const char *, int, int, int);
    int _ACRTIMP __cdecl _pdb_wsopen(const wchar_t *, int, int, int);
    int _ACRTIMP __cdecl _pdb_open(const char *, int, int);
    int _ACRTIMP __cdecl _pdb_wopen(const wchar_t *, int, int);
}

// Redirect the ones we use to the ones above

#define _sopen _pdb_sopen
#define _wsopen _pdb_wsopen
#define _open _pdb_open
#define _wopen _pdb_wopen

// Now, through a bit of linker magic, redirect the ones above to the real CRT ones
// that should have been accessible anyway.

#if defined(_M_IX86)
    #if defined(_DLL)
        #pragma comment(linker, "/alternatename:__imp___pdb_sopen=__imp___sopen")
        #pragma comment(linker, "/alternatename:__imp___pdb_wsopen=__imp___wsopen")
        #pragma comment(linker, "/alternatename:__imp___pdb_open=__imp___open")
        #pragma comment(linker, "/alternatename:__imp___pdb_wopen=__imp___wopen")
    #else
        #pragma comment(linker, "/alternatename:__pdb_sopen=__sopen")
        #pragma comment(linker, "/alternatename:__pdb_wsopen=__wsopen")
        #pragma comment(linker, "/alternatename:__pdb_open=__open")
        #pragma comment(linker, "/alternatename:__pdb_wopen=__wopen")
    #endif
#else
    #if defined(_DLL)
        #pragma comment(linker, "/alternatename:__imp__pdb_sopen=__imp__sopen")
        #pragma comment(linker, "/alternatename:__imp__pdb_wsopen=__imp__wsopen")
        #pragma comment(linker, "/alternatename:__imp__pdb_open=__imp__open")
        #pragma comment(linker, "/alternatename:__imp__pdb_wopen=__imp__wopen")
    #else
        #pragma comment(linker, "/alternatename:_pdb_sopen=_sopen")
        #pragma comment(linker, "/alternatename:_pdb_wsopen=_wsopen")
        #pragma comment(linker, "/alternatename:_pdb_open=_open")
        #pragma comment(linker, "/alternatename:_pdb_wopen=_wopen")
        #if defined(_M_IA64)
            #pragma comment(linker, "/alternatename:._pdb_sopen=._sopen")
            #pragma comment(linker, "/alternatename:._pdb_wsopen=._wsopen")
            #pragma comment(linker, "/alternatename:._pdb_open=._open")
            #pragma comment(linker, "/alternatename:._pdb_wopen=._wopen")
        #endif
    #endif
#endif
