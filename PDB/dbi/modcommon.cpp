#include "pdbimpl.h"
#include "modcommon.h"

BOOL ModCommon::QueryPdbFile(OUT char szFile[_MAX_PATH], OUT CB* pcb)
{
    wchar_t    wszFile[PDB_MAX_PATH];
    BOOL fResult = QueryPdbFileW(wszFile, pcb);
    if (fResult) {
        //convert it to MBCS
        SafeStackAllocator<0x400> allocator;
        SZ mbsFile = (szFile == NULL) ? allocator.Alloc<char>(_MAX_PATH) : szFile;
        if (_GetSZMBCSFromSZUnicode(wszFile, mbsFile, _MAX_PATH) == NULL) {
            return FALSE;
        }
        *pcb = static_cast<CB>(strlen(mbsFile) + 1);
    }

    return fResult;
}
BOOL ModCommon::QueryCBName(OUT CB* pcb)
{
    return QueryName(NULL, pcb);
}

BOOL ModCommon::QueryName(OUT char szName[_MAX_PATH], OUT CB* pcb)
{
    wchar_t wszName[PDB_MAX_PATH];
    BOOL fResult = QueryNameW(wszName, pcb);
    if (fResult) {
        // convert it to MBCS
        SafeStackAllocator<0x400> allocator;
        SZ mbsz = (szName == NULL) ? allocator.Alloc<char>(_MAX_PATH) : szName;
        if (_GetSZMBCSFromSZUnicode(wszName, mbsz, _MAX_PATH) == NULL) {
            return FALSE;
        }

        *pcb = static_cast<CB>(strlen(mbsz) + 1);
    }
    return fResult;
}

BOOL ModCommon::QueryCBFile(OUT CB* pcb)
{
    return QueryFile(NULL, pcb);
}

BOOL ModCommon::QueryFile(OUT char szFile[_MAX_PATH], OUT CB* pcb)
{
    wchar_t wszFile[PDB_MAX_PATH];
    BOOL fResult = QueryFileW(wszFile, pcb);
    if (fResult) {
        // convert it to MBCS
        SafeStackAllocator<0x400> allocator;
        SZ mbsz = (szFile == NULL) ? allocator.Alloc<char>(_MAX_PATH) : szFile;
        if (_GetSZMBCSFromSZUnicode(wszFile, mbsz, _MAX_PATH) == NULL)
            return FALSE;

        *pcb = static_cast<CB>(strlen(mbsz) + 1);
    }
    return fResult;
}

BOOL ModCommon::QuerySrcFile(OUT char szFile[_MAX_PATH], OUT CB* pcb)
{
    wchar_t    wszFile[_MAX_PATH];
    BOOL    fResult = QuerySrcFileW(wszFile, pcb );
    if (fResult) {
        // convert it to MBCS
        SafeStackAllocator<0x400> allocator;
        SZ mbsFile = (szFile == NULL) ? allocator.Alloc<char>(_MAX_PATH) : szFile;
        if (_GetSZMBCSFromSZUnicode(wszFile, mbsFile, _MAX_PATH) == NULL) {
            return FALSE;
        }
        *pcb = static_cast<CB>(strlen(mbsFile) + 1);
    }

    return fResult;
}

BOOL ModCommon::AddLines(SZ_CONST szSrc, ISECT isect, OFF offCon, CB cbCon, OFF doff, LINE lineStart, PB pbCoff, CB cbCoff)
{
    dassert(szSrc);
    dassert(pbCoff);
//    assert(IS_SZ_FORMAT_PDB(ppdb1));

    wchar_t wcsSrc[_MAX_PATH];
    if (_GetSZUnicodeFromSZMBCS(szSrc, wcsSrc, _MAX_PATH) == NULL)
        return FALSE;

    return AddLinesW(wcsSrc, isect, offCon, cbCon, doff, lineStart, pbCoff, cbCoff);
}

