#include "pdbimpl.h"
#include "dbicommon.h"

BOOL DBICommon::OpenMod(SZ_CONST szModule, SZ_CONST szObjFile, OUT Mod** ppmod)
{
    USES_STACKBUFFER(0x400);

    // szModule, szObjFile are in MBCS, convert to Unicode
    wchar_t* szMod  = GetSZUnicodeFromSZMBCS(szModule);
    wchar_t* szObj  = szObjFile != NULL ? GetSZUnicodeFromSZMBCS(szObjFile) : NULL;

    if ( szMod == NULL || ( szObjFile != NULL && szObj == NULL ) )
        return FALSE;

    return OpenModW(szMod, szObj, ppmod);
}

BOOL DBICommon::QueryTypeServerByPdb(const char* szPdb, OUT ITSM* piTsm )
{
    USES_STACKBUFFER(0x400);

    // Filename in MBCS, convert it to UTF8
    wchar_t * wszPdb = GetSZUnicodeFromSZMBCS(szPdb);
    if ( wszPdb == NULL )
        return FALSE;

    BOOL fResult = QueryTypeServerByPdbW(wszPdb, piTsm);
    return fResult;
}