#include "pdbimpl.h"
#include "pdbcommon.h"
#include "locator.h"

#ifdef _DEBUG

// Do some support for devenv and other processes for using vsassert instead of this

typedef BOOL (__stdcall *PfnVsAssert)(
    SZ_CONST szMsg,
    SZ_CONST szAssert,
    SZ_CONST szFile,
    UINT line,
    BOOL * pfDisableAssert
    );

// List of process names we try to use VsAssert for

const wchar_t * const rgszVsAssertProcesses[] = {
    L"concordeetest",
    L"devenv",
    L"glass2",
    L"glass3",
    L"msvsmon",
    L"natvisvalidator",
    L"pdbexplorer",
    L"rascal",
    L"rascalpro",
    L"vpdexpress",
    L"vswinexpress",
    L"wdexpress",
    NULL
};

const wchar_t * const rgszVsAssertDlls[] = {
    L"diasymreader",
    L"shmetapdb",
    L"cpde",
    NULL
};

const wchar_t szVsAssertDll[] = L"vsassert.dll";
const char  szVsAssertProc[] = "_VsAssert@20";

extern "C" const IMAGE_DOS_HEADER __ImageBase;

static void BaseName(__inout_ecount_z(cchModule) wchar_t *szModule, size_t cchModule)
{
    // generate the base name of the filename back into the same buffer

    wchar_t szFile[_MAX_PATH];

    _wsplitpath_s(szModule, NULL, 0, NULL, 0, szFile, _countof(szFile), NULL, 0);
    wcscpy_s(szModule, cchModule, szFile);
}

template <size_t cchModule>
static void BaseName(wchar_t (&szModule)[cchModule])
{
    BaseName(szModule, cchModule);
}

static bool fVsAssertDllOrProcess()
{
    HMODULE hmodMe = HMODULE(&__ImageBase);

    // Check for various executables
    wchar_t szProcessName[_MAX_PATH];
    wchar_t szMyName[_MAX_PATH];

    if (GetModuleFileNameW(hmodMe, szMyName, _countof(szMyName))) {
        
        BaseName(szMyName);

        unsigned isz;
        for (isz = 0; rgszVsAssertDlls[isz] != NULL; isz++) {
            if (_wcsicmp(szMyName, rgszVsAssertDlls[isz]) == 0) {
                break;
            }
        }
        if (rgszVsAssertDlls[isz] != NULL) {
            return true;
        }
    }

    if (GetModuleFileNameW(NULL, szProcessName, _countof(szProcessName))) {
        
        BaseName(szProcessName);

        unsigned isz;
        for (isz = 0; rgszVsAssertProcesses[isz] != NULL; isz++) {
            if (_wcsicmp(szProcessName, rgszVsAssertProcesses[isz]) == 0) {
                break;
            }
        }
        if (rgszVsAssertProcesses[isz] != NULL) {
            return true;
        }
    }
    
    return false;
}


static bool fVsAssert(SZ_CONST szAssertFile, SZ_CONST szFunc, int line, SZ_CONST szCond)
{
    bool    fRet = false;   // vsassert did/did not handle the assert

    if (fVsAssertDllOrProcess()) {
        
        HMODULE hmodVsAssert = GetModuleHandleW(szVsAssertDll);

        if (hmodVsAssert == NULL) {
            // Try LoadLibrary()
            hmodVsAssert = LoadLibraryExW(szVsAssertDll, NULL, 0);
        }

        if (hmodVsAssert != NULL) {
            PfnVsAssert pfn = PfnVsAssert(GetProcAddress(hmodVsAssert, szVsAssertProc));

            if (pfn) {
                char    szMessage[1024] = "";
                char    szCondition[] = "";

                if (szFunc) {
                    _snprintf_s(
                        szMessage, 
                        _countof(szMessage),
                        _TRUNCATE,
                        "Failure in function %s",
                        szFunc
                        );
                }

                if (!szCond) {
                    szCond = szCondition;
                }

                BOOL fDisable = FALSE;
                pfn(szMessage, szCond, szAssertFile, UINT(line), &fDisable);
                fRet = true;
            }
        }
    }

    return fRet;
}


extern "C" void failAssertion(SZ_CONST szAssertFile, int line)
{
    if (fVsAssert(szAssertFile, NULL, line, NULL)) {
    }
    else {
        fprintf(stderr, "assertion failure: file %s, line %d\n", szAssertFile, line);
        fflush(stderr);
        DebugBreak();
    }
}

extern "C" void failExpect(SZ_CONST szFile, int line)
{
    fprintf(stderr, "expectation failure: file %s, line %d\n", szFile, line);
    fflush(stderr);
}

extern "C" void failAssertionFunc(SZ_CONST szAssertFile, SZ_CONST szFunction, int line, SZ_CONST szCond)
{
    if (fVsAssert(szAssertFile, szFunction, line, szCond)) {
    }
    // Still don't know what to do with mspdbsrv.exe yet.
    else {
        fprintf(
            stderr,
            "assertion failure:\n\tfile: %s,\n\tfunction: %s,\n\tline: %d,\n\tcondition(%s)\n\n",
            szAssertFile,
            szFunction,
            line,
            szCond
            );
        fflush(stderr);
        DebugBreak();
    }
}

extern "C" void failExpectFunc(SZ_CONST szFile, SZ_CONST szFunction, int line, SZ_CONST szCond)
{
    fprintf(
        stderr,
        "expectation failure:\n\tfile: %s,\n\tfunction: %s,\n\tline: %d,\n\tcondition(%s)\n\n",
        szFile,
        szFunction,
        line,
        szCond
        );
    fflush(stderr);
}

#endif

extern const long  cbPgPdbDef = 1024;


BOOL PDBCommon::Open2W(const wchar_t *wszPDB, const char *szMode, OUT EC *pec,
       __out_ecount_opt(cchErrMax)  wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszPDB);
    dassert(szMode);
    dassert(pec);
    dassert(pppdb);

    return OpenEx2W(wszPDB, szMode, ::cbPgPdbDef, pec, wszError, cchErrMax, pppdb);
}

BOOL PDBCommon::OpenValidate5(const wchar_t *wszExecutable,
       const wchar_t *wszSearchPath, void *pvClient,
       PfnPDBQueryCallback pfnQueryCallback, OUT EC *pec,
       __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, OUT PDB **pppdb)
{
    PDBLOG_FUNC();
    dassert(wszExecutable);
    dassert(pec);
    dassert(pppdb);

    *pppdb = NULL;

    LOCATOR locator;

    locator.SetPvClient(pvClient);
    locator.SetPfnQueryCallback(pfnQueryCallback);

    if (!locator.FCrackExeOrDbg(wszExecutable) || !locator.FLocateDbg(wszSearchPath)) {
HandleError:
        setError(pec, locator.Ec(), wszError, cchErrMax, locator.WszError());

        return FALSE;
    }

    if (!locator.FLocatePdb(wszSearchPath)) {
        goto HandleError;
    }

    *pppdb = locator.Ppdb();

    *pec = EC_OK;

#ifdef PDB_TYPESERVER
    ((PDBCommon *) *pppdb)->setExeNameW(wszExecutable);
#endif

    return TRUE;
}

EC PDBCommon::QueryLastError(char szErr[cbErrMax])
{
    PDBLOG_FUNC();

    wchar_t wszErrLast[cbErrMax];
    EC ecLast = QueryLastErrorExW(wszErrLast, cbErrMax);
        
    if (szErr != NULL) {
        if (WideCharToMultiByte(CP_ACP, 0, wszErrLast, -1, szErr, cbErrMax, NULL, NULL) == 0) {
            // Conversion failed

            szErr[0] = '\0';
        }
    }

    return ecLast;
}

BOOL PDBCommon::CopyTo(const char *szPdbOut, DWORD dwCopyFilter, DWORD dwReserved)
{
    wchar_t wszPdbOut[_MAX_PATH];

    if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szPdbOut, -1, wszPdbOut, _MAX_PATH) == 0) {
        // If we can't convert then the file will not be found
        //setLastError(EC_NOT_FOUND, szPdbOut);
        return FALSE;
    }

    return CopyToW2(wszPdbOut, dwCopyFilter, NULL, NULL);
}
BOOL PDBCommon::CopyToW(const wchar_t *wszPdbOut, DWORD dwCopyFilter, DWORD dwReserved)
{
    return CopyToW2(wszPdbOut, dwCopyFilter, NULL, NULL);
}

BOOL PDBCommon::OpenStream(SZ_CONST sz, OUT Stream** ppstream) 
{
    return OpenStreamEx(sz, pdbRead pdbWriteShared, ppstream);
}

BOOL PDBCommon::OpenStreamW(const wchar_t * wszStream, OUT Stream** ppstream)
{
    USES_STACKBUFFER(0x400);
    SZ utf8szStream = GetSZUTF8FromSZUnicode(wszStream);
    if (!utf8szStream)
        return FALSE;

    return OpenStream(utf8szStream, ppstream);
}
