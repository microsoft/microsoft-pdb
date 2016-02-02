//////////////////////////////////////////////////////////////////////////////
// C Binding for PDB, DBI, TPI, and Mod

#include "pdbimpl.h"
#include "dbiimpl.h"
#include "szcanon.h"

#pragma code_seg(".text$cthunks")

extern "C" {

PDB_IMPORT_EXPORT(IMPV)
PDBQueryImplementationVersionStatic()
{
    return PDB::QueryImplementationVersionStatic();
}

PDB_IMPORT_EXPORT(IMPV)
PDBQueryInterfaceVersionStatic()
{
    return PDB::QueryInterfaceVersionStatic();
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpen2W(const wchar_t *wszPDB, const char *szMode, OUT EC *pec,
       OUT wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    return PDB::Open2W(wszPDB, szMode, pec, wszError, cchErrMax, pppdb);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenEx2W(const wchar_t *wszPDB, const char *szMode, long cbPage,
       OUT EC *pec, OUT wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    return PDB::OpenEx2W(wszPDB, szMode, cbPage, pec, wszError, cchErrMax, pppdb);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenValidate4(const wchar_t *wszPDB, const char *szMode,
       const struct _GUID *pguidSig, SIG sig, AGE age, OUT EC *pec,
       OUT wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    return PDB::OpenValidate4(wszPDB, szMode, pguidSig, sig, age, pec, wszError, cchErrMax, pppdb);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenValidate5(const wchar_t *wszExecutable, const wchar_t *wszSearchPath,
       void *pvClient, PfnPDBQueryCallback pfnQueryCallback, OUT EC *pec,
       OUT wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
   return PDB::OpenValidate5(wszExecutable, wszSearchPath, pvClient, pfnQueryCallback, pec, wszError, cchErrMax, pppdb);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenNgenPdb(const wchar_t *wszNgenImage, const wchar_t *wszPdbPath,
       OUT EC *pec, OUT wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    return PDB::OpenNgenPdb(wszNgenImage, wszPdbPath, pec, wszError, cchErrMax, pppdb);
}

PDB_IMPORT_EXPORT(BOOL)
PDBExportValidateInterface(INTV intv)
{
    return PDB::ExportValidateInterface(intv);
}

PDB_IMPORT_EXPORT(BOOL)
PDBExportValidateImplementation(INTV intv)
{
    return PDB::ExportValidateImplementation(intv);
}

PDB_IMPORT_EXPORT(BOOL)
PDBRPC()
{
    return PDB::RPC();
}

PDB_IMPORT_EXPORT(EC)
PDBQueryLastError(PDB *ppdb, OUT char szError[cbErrMax])
{
    return ppdb->QueryLastError(szError);
}

PDB_IMPORT_EXPORT(EC)
PDBQueryLastErrorExW(PDB *ppdb, OUT wchar_t *wszError, size_t cchMax)
{
    return ppdb->QueryLastErrorExW(wszError, cchMax);
}

PDB_IMPORT_EXPORT(INTV)
PDBQueryInterfaceVersion(PDB* ppdb)
{
    return ppdb->QueryInterfaceVersion();
}

PDB_IMPORT_EXPORT(IMPV)
PDBQueryImplementationVersion(PDB* ppdb)
{
    return ppdb->QueryImplementationVersion();
}

PDB_IMPORT_EXPORT(BOOL)
PDBIsSZPDB(PDB* ppdb)
{
    return ppdb->fIsSZPDB();
}

PDB_IMPORT_EXPORT(SZ)
PDBQueryPDBName(PDB* ppdb, OUT char szPDB[_MAX_PATH])
{
    return ppdb->QueryPDBName(szPDB);
}

PDB_IMPORT_EXPORT(SIG)
PDBQuerySignature(PDB* ppdb)
{
    return ppdb->QuerySignature();
}

PDB_IMPORT_EXPORT(BOOL)
PDBQuerySignature2(PDB* ppdb, PSIG70 psig70)
{
    return ppdb->QuerySignature2(psig70);
}

PDB_IMPORT_EXPORT(AGE)
PDBQueryAge(PDB* ppdb)
{
    return ppdb->QueryAge();
}

PDB_IMPORT_EXPORT(BOOL)
PDBCreateDBI(PDB* ppdb, SZ_CONST szTarget, OUT DBI** ppdbi)
{
    return ppdb->CreateDBI(szTarget, ppdbi);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenDBIEx(PDB* ppdb, const char *szMode, const char *szTarget, OUT DBI **ppdbi,
        PfnFindDebugInfoFile srchfcn)
{
    return ppdb->OpenDBIEx(szTarget, szMode, ppdbi, srchfcn);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenDBI(PDB* ppdb, SZ_CONST szMode, SZ_CONST szTarget, OUT DBI** ppdbi)
{
    return ppdb->OpenDBI(szTarget, szMode, ppdbi);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenTpi(PDB* ppdb, SZ_CONST szMode, OUT TPI** pptpi)
{
    return ppdb->OpenTpi(szMode, pptpi);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenIpi(PDB* ppdb, SZ_CONST szMode, OUT TPI** pptpi)
{
    return ppdb->OpenIpi(szMode, pptpi);
}

PDB_IMPORT_EXPORT(BOOL)
PDBCommit(PDB* ppdb)
{
    return ppdb->Commit();
}

PDB_IMPORT_EXPORT(BOOL)
PDBClose(PDB* ppdb)
{
    return ppdb->Close();
}

PDB_IMPORT_EXPORT(BOOL)
PDBCopyTo(PDB* ppdb, SZ_CONST szTargetPdb, DWORD dwCopyFilter, DWORD dwReserved)
{
    return ppdb->CopyTo(szTargetPdb, dwCopyFilter, dwReserved);
}

PDB_IMPORT_EXPORT(BOOL)
PDBCopyToW(PDB *ppdb, const wchar_t *szTargetPdb, DWORD dwCopyFilter, DWORD dwReserved)
{
    return ppdb->CopyToW(szTargetPdb, dwCopyFilter, dwReserved);
}

PDB_IMPORT_EXPORT(BOOL)
PDBCopyToW2(PDB *ppdb, const wchar_t *szTargetPdb, DWORD dwCopyFilter, PfnPDBCopyQueryCallback pfnCallBack, void * pvClientContext)
{
    return ppdb->CopyToW2(szTargetPdb,dwCopyFilter, pfnCallBack, pvClientContext);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenStream(PDB* ppdb, SZ_CONST szStream, OUT Stream** ppstream)
{
    return ppdb->OpenStream(szStream, ppstream);
}

PDB_IMPORT_EXPORT(BOOL)
PDBOpenStreamEx(PDB* ppdb, SZ_CONST szStream, SZ_CONST szMode, OUT Stream** ppstream)
{
    return ppdb->OpenStreamEx(szStream, szMode, ppstream);
}

PDB_IMPORT_EXPORT(BOOL)
PDBRegisterPDBMapping(PDB *ppdb, const wchar_t *wszPDBFrom, const wchar_t *wszPDBTo)
{
    return ppdb->RegisterPDBMapping(wszPDBFrom, wszPDBTo);
}

PDB_IMPORT_EXPORT(BOOL)
PDBEnablePrefetching(PDB *ppdb)
{
    return ppdb->EnablePrefetching();
}

PDB_IMPORT_EXPORT(BOOL)
PDBFLazy(PDB *ppdb)
{
    return ppdb->FLazy();
}

PDB_IMPORT_EXPORT(BOOL)
PDBFMinimal(PDB *ppdb)
{
    return ppdb->FMinimal();
}

PDB_IMPORT_EXPORT(wchar_t *)
PDBQueryPDBNameExW(PDB *ppdb, OUT wchar_t *wszPDB, size_t cchMax)
{
    return ppdb->QueryPDBNameExW(wszPDB, cchMax);
}

PDB_IMPORT_EXPORT(BOOL)
PDBResetGUID(PDB *ppdb, BYTE *pb, DWORD cb)
{
    return ppdb->ResetGUID(pb, cb);
}

PDB_IMPORT_EXPORT(CB)
StreamQueryCb(Stream* pstream)
{
    return pstream->QueryCb();
}

PDB_IMPORT_EXPORT(BOOL)
StreamRead(Stream* pstream, OFF off, void* pvBuf, CB* pcbBuf)
{
    return pstream->Read(off, pvBuf, pcbBuf);
}

PDB_IMPORT_EXPORT(BOOL)
StreamWrite(Stream* pstream, OFF off, void* pvBuf, CB cbBuf)
{
    return pstream->Write(off, pvBuf, cbBuf);
}

PDB_IMPORT_EXPORT(BOOL)
StreamReplace(Stream* pstream, void* pvBuf, CB cbBuf)
{
    return pstream->Replace(pvBuf, cbBuf);
}

PDB_IMPORT_EXPORT(BOOL)
StreamAppend(Stream* pstream, void* pvBuf, CB cbBuf)
{
    return pstream->Append(pvBuf, cbBuf);
}

PDB_IMPORT_EXPORT(BOOL)
StreamDelete(Stream* pstream)
{
    return pstream->Delete();
}

PDB_IMPORT_EXPORT(BOOL)
StreamTruncate(Stream* pstream, long cb)
{
    return pstream->Truncate(cb);
}

PDB_IMPORT_EXPORT(BOOL)
StreamRelease(Stream* pstream)
{
    return pstream->Release();
}

PDB_IMPORT_EXPORT(INTV)
DBIQueryInterfaceVersion(DBI* pdbi)
{
    return pdbi->QueryInterfaceVersion();
}

PDB_IMPORT_EXPORT(IMPV)
DBIQueryImplementationVersion(DBI* pdbi)
{
    return pdbi->QueryImplementationVersion();
}

PDB_IMPORT_EXPORT(BOOL)
DBIOpenMod(DBI* pdbi, SZ_CONST szModule, SZ_CONST szFile, OUT Mod** ppmod)
{
    return pdbi->OpenMod(szModule, szFile, ppmod);
}

PDB_IMPORT_EXPORT(BOOL)
DBIOpenModW(DBI* pdbi, const wchar_t *szModule, const wchar_t *szFile, OUT Mod** ppmod)
{
    return pdbi->OpenModW(szModule, szFile, ppmod);
}

PDB_IMPORT_EXPORT(BOOL)
DBIDeleteMod(DBI* pdbi, SZ_CONST szModule)
{
    return pdbi->DeleteMod(szModule);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQueryNextMod(DBI* pdbi, Mod* pmod, Mod** ppmodNext)
{
    return pdbi->QueryNextMod(pmod, ppmodNext);
}

PDB_IMPORT_EXPORT(BOOL)
DBIOpenGlobals(DBI* pdbi, OUT GSI **ppgsi)
{
    return pdbi->OpenGlobals(ppgsi);
}

PDB_IMPORT_EXPORT(BOOL)
DBIOpenPublics(DBI* pdbi, OUT GSI **ppgsi)
{
    return pdbi->OpenPublics(ppgsi);
}

PDB_IMPORT_EXPORT(BOOL)
DBIAddSec(DBI* pdbi, ISECT isect, USHORT flags, OFF off, CB cb)
{
    return pdbi->AddSec(isect, flags, off, cb);
}

PDB_IMPORT_EXPORT(BOOL)
DBIAddPublic(DBI* pdbi, SZ_CONST szPublic, ISECT isect, OFF off)
{
    return pdbi->AddPublic(szPublic, isect, off);
}

PDB_IMPORT_EXPORT(BOOL)
DBIRemovePublic(DBI* pdbi, SZ_CONST szPublic)
{
    return pdbi->RemovePublic(szPublic);
}

PDB_IMPORT_EXPORT(BOOL)
DBIAddPublic2(DBI* pdbi, SZ_CONST szPublic, ISECT isect, OFF off, CV_pubsymflag_t cvpsf)
{
    return pdbi->AddPublic2(szPublic, isect, off, cvpsf);
}

PDB_IMPORT_EXPORT(BOOL)
    DBIQueryModFromAddr(DBI* pdbi, ISECT isect, OFF off, OUT Mod** ppmod,
    OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb)
{
    return pdbi->QueryModFromAddr(isect, off, ppmod, pisect, poff, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
    DBIQueryModFromAddr2(DBI* pdbi, ISECT isect, OFF off, OUT Mod** ppmod,
    OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb, OUT ULONG * pdwCharacteristics)
{
    return pdbi->QueryModFromAddr2(isect, off, ppmod, pisect, poff, pcb, pdwCharacteristics);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQuerySecMap(DBI* pdbi, OUT PB pb, CB* pcb)
{
    return pdbi->QuerySecMap(pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQueryFileInfo(DBI*pdbi, OUT PB pb, CB* pcb)
{
    return pdbi->QueryFileInfo(pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQuerySupportsEC(DBI* pdbi)
{
    return pdbi->QuerySupportsEC();
}

PDB_IMPORT_EXPORT(void)
DBIDumpMods(DBI* pdbi)
{
    pdbi->DumpMods();
}

PDB_IMPORT_EXPORT(void)
DBIDumpSecContribs(DBI* pdbi)
{
    pdbi->DumpSecContribs();
}

PDB_IMPORT_EXPORT(void)
DBIDumpSecMap(DBI* pdbi)
{
    pdbi->DumpSecMap();
}

PDB_IMPORT_EXPORT(BOOL)
DBIClose(DBI* pdbi)
{
    return pdbi->Close();
}


PDB_IMPORT_EXPORT(BOOL)
DBIAddThunkMap(DBI* pdbi, OFF* poffThunkMap, UINT nThunks, CB cbSizeOfThunk,
        SO* psoSectMap, UINT nSects, ISECT isectThunkTable, OFF offThunkTable)
{
    return pdbi->AddThunkMap(poffThunkMap, nThunks, cbSizeOfThunk, psoSectMap, nSects, isectThunkTable, offThunkTable);
}

PDB_IMPORT_EXPORT(BOOL)
DBIGetEnumContrib(DBI* pdbi, OUT Enum** ppenum)
{
    return pdbi->getEnumContrib(ppenum);
}

PDB_IMPORT_EXPORT(BOOL)
DBIGetEnumContrib2(DBI* pdbi, OUT Enum** ppenum)
{
    return pdbi->getEnumContrib2(ppenum);
}

PDBAPI(BOOL)
DBIQueryTypeServer(DBI* pdbi, ITSM itsm, OUT TPI** pptpi )
{
    return pdbi->QueryTypeServer(itsm, pptpi);
}

PDBAPI(BOOL)
DBIQueryItsmForTi(DBI* pdbi, TI ti, OUT ITSM* pitsm )
{
    return pdbi->QueryItsmForTi(ti, pitsm);
}

PDBAPI(BOOL)
DBIQueryNextItsm(DBI* pdbi, ITSM itsm, OUT ITSM *inext )
{
    return pdbi->QueryNextItsm(itsm, inext);
}

PDBAPI(BOOL)
DBIQueryLazyTypes(DBI* pdbi)
{
    return pdbi->QueryLazyTypes();
}

PDBAPI(BOOL)
DBISetLazyTypes(DBI* pdbi, BOOL fLazy)
{
    return pdbi->SetLazyTypes(fLazy);
}

PDB_IMPORT_EXPORT(BOOL)
DBIOpenDbg(DBI* pdbi, DBGTYPE dbgtype, OUT Dbg ** ppdbg)
{
    return pdbi->OpenDbg(dbgtype, ppdbg);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQueryDbgTypes(DBI* pdbi, OUT DBGTYPE *pdbgtype, OUT long* pcDbgtype)
{
    return pdbi->QueryDbgTypes(pdbgtype, pcDbgtype);
}

PDB_IMPORT_EXPORT(BOOL)
DBIAddLinkInfo(DBI* pdbi, IN PLinkInfo pli)
{
    return pdbi->AddLinkInfo(pli);
}

PDB_IMPORT_EXPORT(BOOL)
DBIAddLinkInfoW(DBI* pdbi, IN PLinkInfoW pli)
{
    return pdbi->AddLinkInfoW(pli);
}

PDB_IMPORT_EXPORT(BOOL)
DBIQueryLinkInfo(DBI* pdbi, PLinkInfo pli, IN OUT long * pcb)
{
    return pdbi->QueryLinkInfo(pli, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
DBIFStripped(DBI* pdbi)
{
    return pdbi->FStripped();
}

PDB_IMPORT_EXPORT(BOOL)
DBIFAddSourceMappingItem(DBI* pdbi, const wchar_t * szMapTo, const wchar_t * szMapFrom, ULONG grFlags)
{
    return pdbi->FAddSourceMappingItem(szMapTo, szMapFrom, grFlags);
}

PDB_IMPORT_EXPORT(void)
DBISetMachineType(DBI* pdbi, USHORT wMachine)
{
    pdbi->SetMachineType(wMachine);
}

PDB_IMPORT_EXPORT(void)
DBIRemoveDataForRva(DBI* pdbi, ULONG rva, ULONG cb)
{
    pdbi->RemoveDataForRva(rva, cb);
}

PDB_IMPORT_EXPORT(BOOL)
DBIFSetPfnNotePdbUsed(DBI* pdbi, void * pvContext, PFNNOTEPDBUSED pfn)
{
    return pdbi->FSetPfnNotePdbUsed(pvContext, pfn);
}

PDB_IMPORT_EXPORT(BOOL)
DBIFSetPfnNoteTypeMismatch(DBI* pdbi, void * pvContext, PFNNOTETYPEMISMATCH pfn)
{
    return pdbi->FSetPfnNoteTypeMismatch(pvContext, pfn);
}

PDB_IMPORT_EXPORT(BOOL)
DBIFSetPfnTmdTypeFilter(DBI* pdbi, void *pvContext, PFNTMDTYPEFILTER pfn)
{
    return pdbi->FSetPfnTmdTypeFilter(pvContext, pfn);
}

PDB_IMPORT_EXPORT(INTV)
ModQueryInterfaceVersion(Mod* pmod)
{
    return pmod->QueryInterfaceVersion();
}

PDB_IMPORT_EXPORT(IMPV)
ModQueryImplementationVersion(Mod* pmod)
{
    return pmod->QueryImplementationVersion();
}

PDB_IMPORT_EXPORT(BOOL)
ModAddTypes(Mod* pmod, PB pbTypes, CB cb)
{
    return pmod->AddTypes(pbTypes, cb);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddSymbols(Mod* pmod, PB pbSym, CB cb)
{
    return pmod->AddSymbols(pbSym, cb);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddSymbols2(Mod* pmod, PB pbSym, DWORD cb, DWORD isectCoff)
{
    return pmod->AddSymbols2(pbSym, cb, isectCoff);
}

PDB_IMPORT_EXPORT(BOOL)
ModRemoveGlobalRefs(Mod* pmod)
{
    return pmod->RemoveGlobalRefs();
}

PDB_IMPORT_EXPORT(BOOL)
ModAddPublic(Mod* pmod, SZ_CONST szPublic, ISECT isect, OFF off)
{
    return pmod->AddPublic(szPublic, isect, off);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddPublic2(Mod* pmod, SZ_CONST szPublic, ISECT isect, OFF off, CV_pubsymflag_t cvpsf)
{
    return pmod->AddPublic2(szPublic, isect, off, cvpsf);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddLines(Mod* pmod, SZ_CONST szSrc, ISECT isect, OFF offCon, CB cbCon,
      OFF doff, LINE lineStart, PB pbCoff, CB cbCoff)
{
    return pmod->AddLines(szSrc, isect, offCon, cbCon, doff, lineStart, pbCoff, cbCoff);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddLinesW(Mod* pmod, const wchar_t *szSrc, ISECT isect, OFF offCon, CB cbCon,
      OFF doff, ULONG lineStart, PB pbCoff, CB cbCoff)
{
    return pmod->AddLinesW(szSrc, isect, offCon, cbCon, doff, lineStart, pbCoff, cbCoff);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddSecContrib(Mod* pmod, ISECT isect, OFF off, CB cb, DWORD dwCharacteristics)
{
    return pmod->AddSecContrib(isect, off, cb, dwCharacteristics);
}

PDB_IMPORT_EXPORT(BOOL)
ModAddSecContribEx(Mod* pmod, ISECT isect, OFF off, CB cb, DWORD dwCharacteristics, DWORD dwDataCrc, DWORD dwRelocCrc)
{
    return pmod->AddSecContribEx(isect, off, cb, dwCharacteristics, dwDataCrc, dwRelocCrc);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCBName(Mod* pmod, OUT CB* pcb)
{
    return pmod->QueryCBName(pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryName(Mod* pmod, OUT char szName[_MAX_PATH], OUT CB* pcb)
{
    return pmod->QueryName(szName, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQuerySymbols(Mod* pmod, PB pbSym, CB* pcb)
{
    return pmod->QuerySymbols(pbSym, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModIsTypeServed(Mod* pmod, DWORD index, BOOL fID)
{
    return pmod->IsTypeServed(index, fID);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryTypes(Mod* pmod, PB pb, DWORD* pcb)
{
    return pmod->QueryTypes(pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryIDs(Mod* pmod, PB pb, DWORD* pcb)
{
    return pmod->QueryIDs(pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCVRecordForTi(Mod* pmod, DWORD index, BOOL fID, PB pb, DWORD* pcb)
{
    return pmod->QueryCVRecordForTi(index, fID, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryPbCVRecordForTi(Mod* pmod, DWORD index, BOOL fID, PB *ppb)
{
    return pmod->QueryPbCVRecordForTi(index, fID, ppb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQuerySrcLineForUDT(Mod* pmod, TI ti, _Deref_out_z_ char **pszSrc, DWORD *pLine)
{
    return pmod->QuerySrcLineForUDT(ti, pszSrc, pLine);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryTiForUDT(Mod* pmod, const char *sz, BOOL fCase, TI *pti)
{
    return pmod->QueryTiForUDT(sz, fCase, pti);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCoffSymRVAs(Mod* pmod, PB pb, DWORD* pcb)
{
    return pmod->QueryCoffSymRVAs(pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryLines(Mod* pmod, PB pbLines, CB* pcb)
{
    return pmod->QueryLines(pbLines, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryLines2(Mod* pmod, CB cb, PB pbLines, CB* pcb)
{
    return pmod->QueryLines2(cb, pbLines, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModSetPvClient(Mod* pmod, void *pvClient)
{
    return pmod->SetPvClient(pvClient);
}

PDB_IMPORT_EXPORT(BOOL)
ModGetPvClient(Mod* pmod, OUT void** ppvClient)
{
    return pmod->GetPvClient(ppvClient);
}

PDB_IMPORT_EXPORT(BOOL)
ModQuerySecContrib(Mod *pmod, OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb, OUT DWORD* pdwCharacteristics)
{
    return pmod->QuerySecContrib(pisect, poff, pcb, pdwCharacteristics);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryFirstCodeSecContrib(Mod *pmod, OUT ISECT* pisect, OUT OFF* poff, OUT CB* pcb, OUT DWORD* pdwCharacteristics)
{
    return pmod->QuerySecContrib(pisect, poff, pcb, pdwCharacteristics);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryImod(Mod* pmod, OUT IMOD* pimod)
{
    return pmod->QueryImod(pimod);
}

PDB_IMPORT_EXPORT(BOOL)
ModQuerySrcFile(Mod* pmod, OUT char szFile[PDB_MAX_PATH], OUT long* pcb)
{
    return pmod->QuerySrcFile(szFile, pcb );
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryPdbFile(Mod* pmod, OUT char szFile[PDB_MAX_PATH], OUT long* pcb)
{
    return pmod->QueryPdbFile(szFile, pcb );
}

PDB_IMPORT_EXPORT(BOOL)
ModQuerySupportsEC(Mod* pmod)
{
    return pmod->QuerySupportsEC();
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryDBI(Mod* pmod, OUT DBI** ppdbi)
{
    return pmod->QueryDBI(ppdbi);
}

PDB_IMPORT_EXPORT(BOOL)
ModClose(Mod* pmod)
{
    return pmod->Close();
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCBFile(Mod* pmod, OUT CB* pcb)
{
    return pmod->QueryCBFile(pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryFile(Mod* pmod, OUT char szFile[_MAX_PATH], OUT CB* pcb)
{
    return pmod->QueryFile(szFile, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryTpi(Mod* pmod, OUT TPI** pptpi)
{
    return pmod->QueryTpi(pptpi);
}

PDB_IMPORT_EXPORT(BOOL)
ModReplaceLines(Mod* pmod, BYTE* pbLines, long cb)
{
    return pmod->ReplaceLines(pbLines, cb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCrossScopeExports(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryCrossScopeExports(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryCrossScopeImports(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryCrossScopeImports(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryInlineeLines(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryInlineeLines(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModTranslateFileId(Mod* pmod, DWORD id, DWORD* pid)
{
    return pmod->TranslateFileId(id, pid);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryFuncMDTokenMap(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryFuncMDTokenMap(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryTypeMDTokenMap(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryTypeMDTokenMap(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryMergedAssemblyInput(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryMergedAssemblyInput(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryILLines(Mod* pmod, DWORD cb, BYTE* pb, DWORD* pcb)
{
    return pmod->QueryILLines(cb, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
ModGetEnumILLines(Mod* pmod, EnumLines** ppenum)
{
    return pmod->GetEnumILLines(ppenum);
}

PDB_IMPORT_EXPORT(BOOL)
ModQueryILLineFlags(Mod* pmod, OUT DWORD* pdwFlags)
{
    return pmod->QueryILLineFlags(pdwFlags);
}

PDB_IMPORT_EXPORT(BOOL)
ModMergeTypes(Mod* pmod, BYTE *pbSym, DWORD cb)
{
    return pmod->MergeTypes(pbSym, cb);
}

PDB_IMPORT_EXPORT(INTV)
TypesQueryInterfaceVersion(TPI* ptpi)
{
    return ptpi->QueryInterfaceVersion();
}

PDB_IMPORT_EXPORT(IMPV)
TypesQueryImplementationVersion(TPI* ptpi)
{
    return ptpi->QueryImplementationVersion();
}

PDB_IMPORT_EXPORT(CB)
TypesQueryCb(TPI* ptpi)
{
    return ptpi->QueryCb();
}

PDB_IMPORT_EXPORT(BOOL)
TypesClose(TPI* ptpi)
{
    return ptpi->Close();
}

PDB_IMPORT_EXPORT(BOOL)
TypesCommit(TPI* ptpi)
{
    return ptpi->Commit();
}

PDB_IMPORT_EXPORT(BOOL)
TypesSupportQueryTiForUDT(TPI* ptpi)
{
    return ptpi->SupportQueryTiForUDT();
}

PDB_IMPORT_EXPORT(BOOL)
TypesQueryTiForCVRecordEx(TPI* ptpi, PB pb, OUT TI* pti)
{
    return ptpi->QueryTiForCVRecord(pb, pti);
}

PDB_IMPORT_EXPORT(BOOL)
TypesQueryCVRecordForTiEx(TPI* ptpi, TI ti, OUT PB pb, IN OUT CB* pcb)
{
    return ptpi->QueryCVRecordForTi(ti, pb, pcb);
}

PDB_IMPORT_EXPORT(BOOL)
TypesQueryPbCVRecordForTiEx(TPI* ptpi, TI ti, OUT PB* ppb)
{
    return ptpi->QueryPbCVRecordForTi(ti, ppb);
}

PDB_IMPORT_EXPORT(TI)
TypesQueryTiMinEx(TPI* ptpi)
{
    return ptpi->QueryTiMin();
}

PDB_IMPORT_EXPORT(TI)
TypesQueryTiMacEx(TPI* ptpi)
{
    return ptpi->QueryTiMac();
}

PDB_IMPORT_EXPORT(BOOL)
TypesQueryTiForUDTEx(TPI* ptpi, const char *sz, BOOL fCase, OUT TI* pti)
{
    return ptpi->QueryTiForUDT(sz, fCase, pti);
}

PDB_IMPORT_EXPORT(BOOL)
TypesfIs16bitTypePool(TPI * ptpi)
{
    return ptpi->fIs16bitTypePool();
}

PDB_IMPORT_EXPORT(BOOL)
TypesAreTypesEqual( TPI* ptpi, TI ti1, TI ti2 )
{
    return ptpi->AreTypesEqual( ti1, ti2 );
}

PDB_IMPORT_EXPORT(BOOL)
TypesIsTypeServed( TPI* ptpi, TI ti )
{
    return ptpi->IsTypeServed( ti );
}

PDB_IMPORT_EXPORT(BOOL) 
DBIFindTypeServers(DBI* pdbi, OUT EC *pec, OUT char szError[cbErrMax])
{
    return pdbi->FindTypeServers( pec, szError );
}

PDB_IMPORT_EXPORT(PB)
GSINextSym (GSI *pgsi, PB pbSym)
{
    return pgsi->NextSym(pbSym);
}

PDB_IMPORT_EXPORT(PB)
GSIHashSym (GSI *pgsi, SZ_CONST szName, PB pbSym)
{
    return pgsi->HashSym (szName, pbSym);
}

PDB_IMPORT_EXPORT(PB)
GSINearestSym (GSI* pgsi, ISECT isect, OFF off,OUT OFF* pdisp)
{
    return pgsi->NearestSym (isect, off, pdisp);
}

PDB_IMPORT_EXPORT(BOOL)
GSIClose(GSI* pgsi)
{
    return pgsi->Close();
}

PDB_IMPORT_EXPORT(unsigned long)
GSIOffForSym(GSI *pgsi, BYTE *pbSym)
{
    return pgsi->OffForSym( pbSym );
}

PDB_IMPORT_EXPORT(BYTE*)
GSISymForOff(GSI *pgsi, unsigned long off)
{
    return pgsi->SymForOff( off );
}

PDB_IMPORT_EXPORT(void)
EnumContribRelease(EnumContrib* penum)
{
    penum->release();
}

PDB_IMPORT_EXPORT(void)
EnumContribReset(EnumContrib* penum)
{
    penum->reset();
}

PDB_IMPORT_EXPORT(BOOL)
EnumContribNext(EnumContrib* penum)
{
    return penum->next();
}

PDB_IMPORT_EXPORT(void)
EnumContribGet(EnumContrib* penum, OUT USHORT* pimod, OUT USHORT* pisect, OUT long* poff, OUT long* pcb, OUT ULONG* pdwCharacteristics)
{
    penum->get(pimod, pisect, poff, pcb, pdwCharacteristics);
}

PDB_IMPORT_EXPORT(void)
EnumContribGetCrcs(EnumContrib* penum, OUT DWORD* pcrcData, OUT DWORD* pcrcReloc)
{
    penum->getCrcs( pcrcData, pcrcReloc );
}

PDB_IMPORT_EXPORT(BOOL)
DbgClose(Dbg *pdbg)
{
    return pdbg->Close();
}

PDB_IMPORT_EXPORT(long)
DbgQuerySize(Dbg *pdbg)
{
    return pdbg->QuerySize();
}

PDB_IMPORT_EXPORT(void)
DbgReset(Dbg *pdbg)
{
    pdbg->Reset();
}

PDB_IMPORT_EXPORT(BOOL)
DbgSkip(Dbg *pdbg, ULONG celt)
{
    return pdbg->Skip(celt);
}

PDB_IMPORT_EXPORT(BOOL)
DbgQueryNext(Dbg *pdbg, ULONG celt, void *rgelt)
{
    return pdbg->QueryNext(celt, rgelt);
}

PDB_IMPORT_EXPORT(BOOL)
DbgFind(Dbg *pdbg, void *pelt)
{
    return pdbg->Find(pelt);
}

PDB_IMPORT_EXPORT(BOOL)
DbgClear(Dbg *pdbg)
{
    return pdbg->Clear();
}

PDB_IMPORT_EXPORT(BOOL)
DbgAppend(Dbg *pdbg, ULONG celt, const void *rgelt)
{
    return pdbg->Append(celt, rgelt);
}

PDB_IMPORT_EXPORT(BOOL)
DbgReplaceNext(Dbg *pdbg, ULONG celt, const void *rgelt)
{
    return pdbg->ReplaceNext(celt, rgelt);
}

PDB_IMPORT_EXPORT(wchar_t *)
SzCanonFilename(wchar_t * szFilename) {
    return CCanonFile::SzCanonFilename(szFilename);
}

PDB_IMPORT_EXPORT(BOOL)
NameMapOpen(PDB *ppdb, BOOL fWrite, OUT NameMap** ppnm)
{
    return NameMap::open(ppdb, fWrite, ppnm);
}

}; //extern "C"
