#include "pdbimpl.h"
#include "dbiimpl.h"

#include <share.h>

#include "crtwrap.h"
#include "utf8.h"

#include "critsec.h"
#include "locator.h"

#include "imagehlp.h"

CriticalSection LOCATOR::SYMSRV::m_cs(CriticalSection::scPreAlloc);
CriticalSection LOCATOR::REGISTRY::m_cs(CriticalSection::scPreAlloc);

LOCATOR::REGISTRY LOCATOR::m_registryUser(HKEY_CURRENT_USER);
LOCATOR::REGISTRY LOCATOR::m_registryMachine(HKEY_LOCAL_MACHINE);
LOCATOR::SYMSRV LOCATOR::m_symsrv;


    // ----------------------------------------------------------------
    // Public Methods
    // ----------------------------------------------------------------


LOCATOR::LOCATOR()
{
    SetError(EC_OK);

    m_pvClient = NULL;
    m_pfnQueryCallback = 0;
    
    m_fPfnNotifyDebugDir = false;
    m_fPfnNotifyMiscPath = false;
    m_fPfnNotifyOpenDBG = false;
    m_fPfnNotifyOpenPDB = false;
    m_fPfnRestrictDBG = false;
    m_fPfnRestrictOriginalPath = false;
    m_fPfnRestrictReferencePath = false;
    m_fPfnRestrictRegistry = false;
    m_fPfnRestrictSymsrv = false;
    m_fPfnRestrictSystemRoot = false;
    
    m_pfnReadExecutableAt = 0;
    m_pfnReadExecutableAtRVA = 0;

    m_fdExe = NULL;
    m_wszExePath = NULL;
    m_wszExeFName[0] = L'\0';
    m_wszExt = NULL;

    m_fStripped = false;
    m_dwSizeOfImage = 0;
    m_dwTimeStampExe = 0;
    m_dwTimeStampDbg = 0;
    m_wszMiscPath = NULL;

    m_fdDbg = NULL;
    m_wszDbgPath = NULL;

    m_fCvInExe = false;
    m_foCv = 0;
    m_cbCv = 0;

    m_wszPdb = NULL;

    m_ppdb = NULL;

    m_wszCache = NULL;
}


LOCATOR::~LOCATOR()
{
    Close(m_fdExe);

    free(m_wszExePath);                // Allocated with wcsdup()

    delete [] m_wszMiscPath;           // Allocated with new []

    Close(m_fdDbg);

    free(m_wszDbgPath);                // Allocated with wcsdup()

    delete [] m_wszPdb;                // Allocated with new []

    if (m_wszCache != NULL) {
        free(m_wszCache);              // Allocated with wcsdup()
    }

    // Note: m_ppdb is not released.
}


DWORD LOCATOR::CbCv() const
{
    return m_cbCv;
}


EC LOCATOR::Ec() const
{
    return m_ec;
}


DWORD LOCATOR::FoCv() const
{
    return m_foCv;
}


bool LOCATOR::FCrackExeOrDbg(const wchar_t *wszFilename, bool fCheckNgenImage)
{
    FILE *fd = NULL;

    m_pfnReadCodeViewDebugData = (PfnPDBReadCodeViewDebugData) QueryCallback(povcReadCodeViewDebugData);

    if (m_pfnReadCodeViewDebugData != 0) {
        // We'll read the actual codeview data 
        // from the client in FLocatePdb().

        m_fCvInExe = true;

        return true;
    }

    m_pfnReadMiscDebugData = (PfnPDBReadMiscDebugData) QueryCallback(povcReadMiscDebugData);

    if (m_pfnReadMiscDebugData != 0) {
        if (FReadMiscDebugData()) {
            // UNDONE : This is really true only if header has
            //          IMAGE_FILE_DEBUG_STRIPPED bit set. But
            //          in this case, there is no way to verify!

            m_fStripped = true;

            return FSaveFileNames(wszFilename);
        }
    }

    m_pfnReadExecutableAt = (PfnPDBReadExecutableAt) QueryCallback(povcReadExecutableAt);

    if (m_pfnReadExecutableAt == 0) {
        m_pfnReadExecutableAtRVA = (PfnPDBReadExecutableAtRVA) QueryCallback(povcReadExecutableAtRVA);

        if (m_pfnReadExecutableAtRVA == 0) {
            fd = PDB_wfsopen(wszFilename, L"rb", SH_DENYWR);

            if (fd == NULL) {
                SetError(EC_NOT_FOUND, wszFilename);

                return false;
            }
        }
    }

    IMAGE_DOS_HEADER DosHeader;

    if (!FReadHeader(fd, 0, sizeof(DosHeader), &DosHeader)) {
InvalidExecutable:
        SetError(EC_INVALID_EXECUTABLE, wszFilename);

        Close(fd);

        return false;
    }

    if (fd != NULL) {
        // We only check for a DBG file when accessing a file locally

        if (DosHeader.e_magic == IMAGE_SEPARATE_DEBUG_SIGNATURE) {
            // The file is a DBG file

            Close(fd);

            // Set m_dwTimeStampExe so that this DBG is recognized as valid

            m_dwTimeStampExe = ((IMAGE_SEPARATE_DEBUG_HEADER *) &DosHeader)->TimeDateStamp;

            bool f = FLocateDbgValidate(wszFilename);

            if (!f && (Ec() == EC_FORMAT)) {
                // If this is an invalid format DBG then we guessed

                goto InvalidExecutable;
            }

            return f;
        }
    }

    if (DosHeader.e_magic != IMAGE_DOS_SIGNATURE) {
        // The file isn't an MS-DOS executable

        goto InvalidExecutable;
    }

    if (!FSaveFileNames(wszFilename)) {
        Close(fd);

        return false;
    }

    if (DosHeader.e_lfanew <= 0) {
        // There is no pointer to a PE header

        goto InvalidExecutable;
    }

    // Read the PE header.

    IMAGE_NT_HEADERS64 NtHeader;

    if (!FReadHeader(fd, (DWORD) DosHeader.e_lfanew, sizeof(NtHeader), &NtHeader)) {
        // The file isn't large enough to contain a PE header

        goto InvalidExecutable;
    }

    if (NtHeader.Signature != IMAGE_NT_SIGNATURE) {
        // The file isn't a PE executable

        goto InvalidExecutable;
    }

    DWORD rva = 0, cb  = 0;
    DWORD rvaComDescriptor = 0, cbComDescriptor = 0;

    switch (NtHeader.OptionalHeader.Magic) {
        case IMAGE_NT_OPTIONAL_HDR32_MAGIC :
        {
            const IMAGE_OPTIONAL_HEADER32 *pImgOptHdr32 = (IMAGE_OPTIONAL_HEADER32 *) &NtHeader.OptionalHeader;

            m_dwSizeOfImage = pImgOptHdr32->SizeOfImage;

            if (pImgOptHdr32->NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_DEBUG) {
                rva = pImgOptHdr32->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
                cb  = pImgOptHdr32->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
            }

            if (fCheckNgenImage && (pImgOptHdr32->NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR)) {
                rvaComDescriptor = pImgOptHdr32->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
                cbComDescriptor  = pImgOptHdr32->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
            }

            break;
        }

        case IMAGE_NT_OPTIONAL_HDR64_MAGIC :
        {
            const IMAGE_OPTIONAL_HEADER64 *pImgOptHdr64 = &NtHeader.OptionalHeader;

            m_dwSizeOfImage = pImgOptHdr64->SizeOfImage;

            if (pImgOptHdr64->NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_DEBUG) {
                rva = pImgOptHdr64->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
                cb  = pImgOptHdr64->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
            }

            if (fCheckNgenImage && (pImgOptHdr64->NumberOfRvaAndSizes >= IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR)) {
                rvaComDescriptor = pImgOptHdr64->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].VirtualAddress;
                cbComDescriptor  = pImgOptHdr64->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
            }
            
            break;
        }

        default :
            // We don't recognize this format optional header

            goto InvalidExecutable;
    }

    if ((rva == 0) || (cb == 0)) {
        // There are no debug directories in the executable.

        SetError(EC_NO_DEBUG_INFO, wszFilename);

        Close(fd);

        return false;
    }

    if (fCheckNgenImage && ((rvaComDescriptor == 0) || (cbComDescriptor == 0))) {
        // Valid ngen images should have COM descriptor directories.

        goto InvalidExecutable;
    }

    DWORD fo;
    DWORD foComDescriptor = 0;

    if ((m_pfnReadExecutableAt == 0) && (m_pfnReadExecutableAtRVA != 0))
    {
        // We can avoid mapping RVA to file offset if we are using
        // RVAs to read from the source file

        fo = 0;
    }

    else {
        fo = (DWORD) DosHeader.e_lfanew + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + NtHeader.FileHeader.SizeOfOptionalHeader;

        size_t csec = (size_t) NtHeader.FileHeader.NumberOfSections;

        IMAGE_SECTION_HEADER SecHeader;

        DWORD ptrToRawDataDbgDir = 0, vaDbgDir = 0;
        DWORD ptrToRawDataComDescriptor = 0, vaComDescriptor = 0;

        size_t isec = 0;

        for (isec = 0; isec < csec; isec++) {
            if (!FReadHeader(fd, fo, sizeof(IMAGE_SECTION_HEADER), &SecHeader)) {
                break;
            }

            fo += sizeof(IMAGE_SECTION_HEADER);

            if ((SecHeader.VirtualAddress <= rva) &&
                ((SecHeader.VirtualAddress + SecHeader.SizeOfRawData) >= (rva + cb))) {
                ptrToRawDataDbgDir = SecHeader.PointerToRawData;
                vaDbgDir = SecHeader.VirtualAddress;
            }

            if (fCheckNgenImage &&
                (SecHeader.VirtualAddress <= rvaComDescriptor) &&
                ((SecHeader.VirtualAddress + SecHeader.SizeOfRawData) >= (rvaComDescriptor + cbComDescriptor))) {
                ptrToRawDataComDescriptor = SecHeader.PointerToRawData;
                vaComDescriptor = SecHeader.VirtualAddress;
            }

            if ((ptrToRawDataDbgDir != 0) && (!fCheckNgenImage || ptrToRawDataComDescriptor != 0)) {
                break;
            }
        }

        if (isec == csec) {
            // Can't find debug directories, or
            // Can't find COM descriptor directories for ngen image

            goto InvalidExecutable;
        }

        fo = ptrToRawDataDbgDir + (rva - vaDbgDir);

        if (fCheckNgenImage) {
            foComDescriptor = ptrToRawDataComDescriptor + (rvaComDescriptor - vaComDescriptor);
        }
    }

    if (fCheckNgenImage) {
        IMAGE_COR20_HEADER corHdr;

        if (!FReadAt(fd, foComDescriptor, rvaComDescriptor, sizeof(IMAGE_COR20_HEADER), &corHdr)) {
            // Can't read the COR20 header

            goto InvalidExecutable;
        }

        if ((corHdr.ManagedNativeHeader.VirtualAddress == 0) || (corHdr.ManagedNativeHeader.Size == 0)) {
            // Invalid data directory for ManagedNativeHeader in an ngen image

            goto InvalidExecutable;
        }
    }

    IMAGE_DEBUG_DIRECTORY dbgdirCv = {0};
    IMAGE_DEBUG_DIRECTORY dbgdirMisc = {0};

    dbgdirCv.Type = IMAGE_DEBUG_TYPE_UNKNOWN;
    dbgdirMisc.Type = IMAGE_DEBUG_TYPE_UNKNOWN;

    size_t cdbgdir = cb / sizeof(IMAGE_DEBUG_DIRECTORY);

    wchar_t wszNgenFName[_MAX_FNAME];

    if (fCheckNgenImage) {
        _wsplitpath_s(wszFilename, NULL, 0, NULL, 0, wszNgenFName, _countof(wszNgenFName), NULL, 0);
    }

    for (size_t idbgdir = 0; idbgdir < cdbgdir; idbgdir++) {
        IMAGE_DEBUG_DIRECTORY dbgdir;

        if (!FReadAt(fd, fo, rva, sizeof(IMAGE_DEBUG_DIRECTORY), &dbgdir)) {
            // Can't read debug directories

            goto InvalidExecutable;
        }

        rva += sizeof(IMAGE_DEBUG_DIRECTORY);

        if ((m_pfnReadExecutableAt != 0) || (m_pfnReadExecutableAtRVA == 0))
        {
            fo += sizeof(IMAGE_DEBUG_DIRECTORY);
        }

        NotifyDebugDir(true, &dbgdir);

        if (dbgdir.Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
            if (fCheckNgenImage) {
                m_fCvInExe = true;
                m_fdExe    = fd;
                m_cbCv     = dbgdir.SizeOfData;
                m_rvaCv    = dbgdir.AddressOfRawData;
                m_foCv     = dbgdir.PointerToRawData;

                if (FReadDebugDirInfo() && m_fGuid && (m_age != 0) && (m_wszPdb != NULL)) {
                    wchar_t wszPdbFName[_MAX_FNAME];

                    _wsplitpath_s(m_wszPdb, NULL, 0, NULL, 0, wszPdbFName, _countof(wszPdbFName), NULL, 0);

                    if (_wcsicmp(wszNgenFName, wszPdbFName) == 0) {
                        dbgdirCv = dbgdir;
                        return true;
                    }
                }
            }
            else {
                dbgdirCv = dbgdir;
            }
        }

        else if (dbgdir.Type == IMAGE_DEBUG_TYPE_MISC) {
            dbgdirMisc = dbgdir;
        }
    }

    m_fdExe = fd;

    bool fDebugInfo = false;

    if (dbgdirCv.Type != IMAGE_DEBUG_TYPE_UNKNOWN) {
        fDebugInfo = true;

        m_fCvInExe = true;
        m_cbCv = dbgdirCv.SizeOfData;
        m_rvaCv = dbgdirCv.AddressOfRawData;
        m_foCv = dbgdirCv.PointerToRawData;
    }
    else if (fCheckNgenImage) {
        // When opening PDB for an ngen image, we should have found
        // (in the loop above) a debug directory entry of type
        // IMAGE_DEBUG_TYPE_CODEVIEW and the contained PDB's filename
        // should the same as that of the ngen image.

        goto InvalidExecutable;
    }

    m_fStripped = (NtHeader.FileHeader.Characteristics & IMAGE_FILE_DEBUG_STRIPPED) != 0;

    if (m_fStripped && (dbgdirMisc.Type != IMAGE_DEBUG_TYPE_UNKNOWN)) {
        fDebugInfo = true;

        m_dwTimeStampExe = NtHeader.FileHeader.TimeDateStamp;
        m_dwTimeStampDbg = dbgdirMisc.TimeDateStamp;

        ReadMiscPath(NULL, fd, dbgdirMisc.PointerToRawData, dbgdirMisc.AddressOfRawData, dbgdirMisc.SizeOfData);
    }

    if (!fDebugInfo) {
        SetError(EC_NO_DEBUG_INFO, wszFilename);
    }

    return fDebugInfo;
}


bool LOCATOR::FGetNgenPdbInfo(const wchar_t *wszNgenImage, wchar_t **pWszPdb, GUID **ppGuidSig, AGE *pAge)
{
    if (!FCrackExeOrDbg(wszNgenImage, true)) {
        return false;
    }

    assert(m_fGuid);
    assert(m_age != 0);
    assert(m_wszPdb != NULL);

    if (pWszPdb != NULL) {
        *pWszPdb = m_wszPdb;
    }

    if (ppGuidSig != NULL) {
        *ppGuidSig = &m_guidSig;
    }

    if (pAge != NULL) {
        *pAge = m_age;
    }

    return true;
}


bool LOCATOR::FLocateDbg(const wchar_t *wszSearchPath)
{
    if(FRestrictDBG()) {

        return m_fCvInExe;
    }

    if (m_fdDbg != NULL) {
        // We already have a DBG file from FCrackDbgOrExe.

        return true;
    }

    if (!m_fStripped) {
        // The executable isn't stripped so there is no DBG

        return m_fCvInExe;
    }

    if (FLocateDbgDefault()) {
        return true;
    }

    // Attempt to locate the DBG file along provided search path

    if (FLocateDbgPath(wszSearchPath)) {
        return true;
    }

    // Attempt to locate the DBG file using registry path

    if (FLocateDbgRegistry()) {
        return true;
    }

    // Attempt to locate the DBG file along default search paths

    size_t iszMax = sizeof(rgwszEnvName) / sizeof(rgwszEnvName[0]);

    if (FRestrictSystemRoot()) {
        iszMax--;
    }

    for (size_t isz = 0; isz < iszMax; isz++) {
        wchar_t *wszEnv = NULL;
        PDB_wdupenv_s(&wszEnv, NULL, rgwszEnvName[isz]);

        if (FLocateDbgPath(wszEnv)) {
            free(wszEnv);
            return true;
        }
        free(wszEnv);
    }

    // Attempt to locate the DBG file using server

    if (FLocateDbgServer()) {
        return true;
    }

    SetError(EC_DBG_NOT_FOUND);

    return false;
}


bool LOCATOR::FLocatePdb(const wchar_t *wszSearchPath)
{
    if (!FReadDebugDirInfo()) {
        return false;
    }

    const wchar_t *wszReference = m_fCvInExe ? m_wszExePath : m_wszDbgPath;

    if (FLocatePdbDefault(wszReference)) {
        return true;
    }

    // Attempt to locate the PDB file along provided search path

    if (FLocatePdbPath(wszSearchPath)) {
        return true;
    }

    // Attempt to locate the PDB file using registry path

    if (FLocatePdbRegistry()) {
        return true;
    }

    // Attempt to locate the PDB file along default search paths

    size_t iszMax = sizeof(rgwszEnvName) / sizeof(rgwszEnvName[0]);

    if (FRestrictSystemRoot()) {
        iszMax--;
    }

    for (size_t isz = 0; isz < iszMax; isz++) {
        wchar_t *wszEnv = NULL;
        PDB_wdupenv_s(&wszEnv, NULL, rgwszEnvName[isz]);

        if (FLocatePdbPath(wszEnv)) {
            free(wszEnv)
            ;
            return true;
        }

        free(wszEnv);
    }

    // Attempt to locate the PDB file using server

    if (FLocatePdbServer()) {
        return true;
    }

    SetError(EC_NOT_FOUND, m_wszPdb);

    return false;
}


bool LOCATOR::FReadDebugDirInfo()
{
    if (m_pfnReadCodeViewDebugData != 0) {
        assert(m_cbCv == 0);

        HRESULT hr = m_pfnReadCodeViewDebugData(m_pvClient,
                                                &m_cbCv,
                                                NULL);

        if (FAILED(hr)) {
            SetError(EC_NO_DEBUG_INFO);

            return false;
        }
    }

    if (m_cbCv == 0) {
        SetError(EC_NO_DEBUG_INFO);

        return false;
    }

    if (m_cbCv > sizeof(CV)) {
        // There isn't PDB based CodeView information

        SetError(EC_DEBUG_INFO_NOT_IN_PDB);

        return false;
    }

    CV cv;

    if (m_pfnReadCodeViewDebugData != 0) {
        HRESULT hr = m_pfnReadCodeViewDebugData(m_pvClient,
                                                &m_cbCv,
                                                (BYTE *) &cv);

        if (FAILED(hr)) {
            SetError(EC_NO_DEBUG_INFO);

            return false;
        }
    }

    else {
        FILE *fd = m_fCvInExe ? m_fdExe : m_fdDbg;

        if (!FReadAt(fd, m_foCv, m_rvaCv, m_cbCv, &cv)) {
            // The debug information can't be read

            SetError(EC_NO_DEBUG_INFO);

            return false;
        }
    }

    if ((cv.dwSig != '01BN') && (cv.dwSig != 'SDSR')) {
        // There isn't PDB based CodeView information

        SetError(EC_DEBUG_INFO_NOT_IN_PDB);

        return false;
    }

    if (!FDecodeCvData(&cv, (size_t) m_cbCv)) {
        return false;
    }

    return true;
}


PDB *LOCATOR::Ppdb() const
{
    return m_ppdb;
}


void LOCATOR::SetPfnQueryCallback(PfnPDBQueryCallback pfnQueryCallback)
{
    m_pfnQueryCallback = pfnQueryCallback;
}


void LOCATOR::SetPvClient(void *pvClient)
{
    m_pvClient = pvClient;
}


const wchar_t *LOCATOR::WszDbgPath() const
{
    return m_wszDbgPath;
}


const wchar_t *LOCATOR::WszError() const
{
    return m_wszError;
}


    // ----------------------------------------------------------------
    // Private Methods
    // ----------------------------------------------------------------


const wchar_t * const LOCATOR::rgwszEnvName[3] =
{
    // Note: There is hard coded knowledge that SystemRoot is last

    L"_NT_ALT_SYMBOL_PATH",
    L"_NT_SYMBOL_PATH",
    L"SystemRoot",
};


void LOCATOR::Close(FILE *fd)
{
    if (fd == NULL) {
        return;
    }

    fclose(fd);
}


bool LOCATOR::FDecodeCvData(const void *pvCv, size_t cbCv)
{
    if (cbCv < sizeof(DWORD)) {
        // Can not be valid CodeView format

        SetError(EC_NO_DEBUG_INFO);

        return false;
    }

    DWORD dwSig = *(DWORD UNALIGNED *) pvCv;

    bool fUtf8;
    const char *szPdb;
    size_t cchMax;

    if (dwSig == '01BN') {
        if (cbCv < offsetof(NB10I, szPdb)) {
            // There is no space for a filename

            return false;
        }

        if (((NB10I UNALIGNED *) pvCv)->dwOffset != 0) {
            // This must be zero field is invalid

            return false;
        }

        m_fGuid = false;
        m_sig = ((NB10I UNALIGNED *) pvCv)->sig;
        m_age = ((NB10I UNALIGNED *) pvCv)->age;

        fUtf8 = false;
        szPdb = ((NB10I UNALIGNED *) pvCv)->szPdb;
        cchMax = cbCv - offsetof(NB10I, szPdb);
    }

    else if (dwSig == 'SDSR') {
        if (cbCv < offsetof(RSDSI, szPdb)) {
            // There is no space for a filename

            return false;
        }

        m_fGuid = true;
        m_guidSig = ((RSDSI UNALIGNED *) pvCv)->guidSig;
        m_age = ((RSDSI UNALIGNED *) pvCv)->age;

        fUtf8 = true;
        szPdb = ((RSDSI UNALIGNED *) pvCv)->szPdb;
        cchMax = cbCv - offsetof(RSDSI, szPdb);
    }

    else {
        // Not a known valid CodeView format

        return false;
    }

    // Or else, just pickup path from the debug directory
    size_t cch = 0;

    for (;;)
    {
        if (cch == cchMax) {
            // The PDB path is not zero terminated

            return false;
        }

        if (szPdb[cch++] == '\0') {
            break;
        }
    }

    if (cch == 1) {
        // The PDB path is empty

        return false;
    }

    wchar_t *wszPdb;

    if (fUtf8) {
        size_t cwch = UnicodeLengthOfUTF8Cb(szPdb, cch);

        // UNDONE: UnicodeLengthOfUTF8Cb doesn't fail when presented with invalid UTF-8

        if (cwch == 0) {
            // The PDB path is not a valid UTF-8 string

            return false;
        }

        wszPdb = new wchar_t[cwch];

        if (wszPdb == NULL) {
            SetError(EC_OUT_OF_MEMORY);

            return false;
        }

        cwch = UTF8ToUnicodeCch(szPdb, cch, wszPdb, cwch);

        if (cwch == 0) {
            delete [] wszPdb;

            return false;
        }
    }

    else {
        int cwch = MultiByteToWideChar(CP_ACP, 0, szPdb, int(cch), NULL, 0);

        if (cwch <= 0) {
            // The PDB path is not a valid CP_ACP string

            return false;
        }

        wszPdb = new wchar_t[size_t(cwch)];

        if (wszPdb == NULL) {
            SetError(EC_OUT_OF_MEMORY);

            return false;
        }

        cwch = MultiByteToWideChar(CP_ACP, 0, szPdb, int(cch), wszPdb, cwch);

        if (cwch <= 0) {
            delete [] wszPdb;

            return false;
        }
    }

    m_wszPdb = wszPdb;

    return true;
}


bool LOCATOR::FLocateDbgDefault()
{
    if (FRestrictReferencePath()) {
        return false;
    }

    wchar_t wszDrive[_MAX_DRIVE];
    wchar_t wszDir[_MAX_DIR];
    wchar_t wszDbgPath[PDB_MAX_FILENAME];

    _wsplitpath_s(m_wszExePath, wszDrive, _countof(wszDrive), wszDir, _countof(wszDir), NULL, 0, NULL, 0);

    _wmakepath_s(wszDbgPath, _countof(wszDbgPath), wszDrive, wszDir, m_wszExeFName, L".dbg");

    bool f = FLocateDbgValidate(wszDbgPath);

    return f;
}


bool LOCATOR::FLocateDbgPath(const wchar_t *wszSearchPath)
{
    if (wszSearchPath == NULL) {
        return false;
    }

    wchar_t wszRelPath[PDB_MAX_FILENAME];

    wcscpy_s(wszRelPath, _countof(wszRelPath), L"symbols\\");

    const wchar_t *wszMiscPath = m_wszMiscPath;

    for (;;) {
        size_t cchExt;

        if (wszMiscPath != NULL) {
            wcscpy_s(wszRelPath + 8, _countof(wszRelPath) - 8, wszMiscPath);

            cchExt = 0;
        }

        else {
            if (m_wszExt != NULL) {
                // The executable filename includes a file extension

                wcscpy_s(wszRelPath + 8, _countof(wszRelPath) - 8, m_wszExt);
                wcscat_s(wszRelPath + 8, _countof(wszRelPath) - 8, L"\\");

                cchExt = wcslen(wszRelPath);
            }

            else {
                cchExt = 0;
            }

            wcscat_s(wszRelPath + 8, _countof(wszRelPath) - 8, m_wszExeFName);
            wcscat_s(wszRelPath + 8, _countof(wszRelPath) - 8, L".dbg");
        }

        if (FLocateCvFilePathHelper(wszSearchPath, wszRelPath, cchExt, false)) {
            return true;
        }

        if (wszMiscPath == NULL) {
            break;
        }

        wszMiscPath = NULL;
    }

    return false;
}


bool LOCATOR::FLocateCvFilePathHelper(const wchar_t *wszSearchPath, const wchar_t *wszRelPath, size_t cchExt, bool fPdb)
{
    // Max length includes trailing slash for search path and trailing null terminator

    size_t cchMax = wcslen(wszSearchPath) + wcslen(wszRelPath) + 2;

    wchar_t *wszDbg = new wchar_t[cchMax];

    if (wszDbg == NULL) {
        SetError(EC_OUT_OF_MEMORY);

        return false;
    }

    bool fFound = false;

    const wchar_t *wszIn = wszSearchPath;

    do {
        wchar_t *wszOut = wszDbg;

        while (*wszIn != L'\0') {
            if (*wszIn == L';') {
                wszIn++;
                break;
            }

            *wszOut++ = *wszIn++;
        }

        if (wszOut == wszDbg) {
            continue;
        }

        *wszOut = L'\0';

        bool fSymsrv = (_wcsnicmp(wszDbg, L"SRV*", 4) == 0) || (_wcsnicmp(wszDbg, L"SYMSRV*", 7) == 0);
        bool fCache = _wcsnicmp(wszDbg, L"CACHE*", 6) == 0;

        if (fPdb && fCache && (m_wszCache != NULL)) {
            // In case of multiple cache locations, don't store symbol
            // found from the current cache into the previous one.

            free(m_wszCache);
            m_wszCache = NULL;
        }

        // Try symsrv.dll if path string starts with SRV*, SYMSRV*, or CACHE*

        if (fSymsrv || fCache) {
            fFound = fPdb ? FLocatePdbSymsrv(wszDbg) : FLocateDbgSymsrv(wszDbg);

            if (fFound) {
                break;
            }

            if (fPdb && fCache) {
                // Update the cache location

                assert(m_wszCache == NULL);

                m_wszCache = _wcsdup(wszDbg + 6);
            }

            continue;
        }

        // Try searching in <search-path>\symbols\<ext>

        if ((wszOut[-1] != L'\\') && (wszOut[-1] != L'/')) {
            *wszOut++ = L'\\';
        }

        wcscpy_s(wszOut, cchMax - (wszOut - wszDbg), wszRelPath);

        fFound = fPdb ? FOpenValidate4(wszDbg) : FLocateDbgValidate(wszDbg);

        if (fFound) {
            break;
        }

        // Try again without "symbols\" directory prefix

        wcscpy_s(wszOut, cchMax - (wszOut - wszDbg), wszRelPath + 8);

        fFound = fPdb ? FOpenValidate4(wszDbg) : FLocateDbgValidate(wszDbg);

        if (fFound) {
            break;
        }

        if (cchExt != 0) {
            // Try again without "symbols\<ext>" directory prefix

            wcscpy_s(wszOut, cchMax - (wszOut - wszDbg), wszRelPath + cchExt);

            fFound = fPdb ? FOpenValidate4(wszDbg) : FLocateDbgValidate(wszDbg);

            if (fFound) {
                break;
            }
        }
    }
    while (*wszIn != L'\0');

    delete [] wszDbg;

    return fFound;
}


bool LOCATOR::FLocateDbgRegistry()
{
    if (FRestrictRegistry()) {
        return false;
    }

    if (FLocateDbgPath(m_registryUser.WszSearchPath())) {
        return true;
    }

    if (FLocateDbgPath(m_registryMachine.WszSearchPath())) {
        return true;
    }

    return false;
}


bool LOCATOR::FLocateDbgServer()
{
    // UNDONE

    return false;
}


bool LOCATOR::FLocateDbgSymsrv(const wchar_t *wsz)
{
    if (m_wszMiscPath == NULL) {
        return false;
    }

    if (FRestrictSymsrv()) {
        return false;
    }

    wchar_t wszDbg[PDB_MAX_FILENAME];
    DWORD dwLastError = 0;

    if (!m_symsrv.SymbolServer(this, wsz, m_wszMiscPath, NULL, m_dwTimeStampExe, m_dwSizeOfImage, 0, wszDbg, &dwLastError)) {
        // Try with other timestamp

        if (!m_symsrv.SymbolServer(this, wsz, m_wszMiscPath, NULL, m_dwTimeStampDbg, m_dwSizeOfImage, 0, wszDbg, &dwLastError)) {
            // UNDONE : Try with checksum. Turns out that some NT4 executables don't have
            //          valid timestamps. For them you are supposed to use the checksum!!
            //          Needed for Kernel debugging only though!

            // if files could not be opened because the cache
            // location was bad or cache was full, notify the user

            if (dwLastError == ERROR_PATH_NOT_FOUND) {
                NotifyOpenDBG(wsz, EC_BAD_CACHE_PATH, NULL);
            }

            else if ((dwLastError == ERROR_HANDLE_DISK_FULL) || (dwLastError == ERROR_DISK_FULL)) {
                NotifyOpenDBG(wsz, EC_CACHE_FULL, NULL);
            }

            return false;
        }
    }

    return FLocateDbgValidate(wszDbg);
}


bool LOCATOR::FLocateDbgValidate(const wchar_t *wszDbgPath)
{
    FILE *fd = PDB_wfsopen(wszDbgPath, L"rb", SH_DENYWR);

    if (fd == NULL) {
        NotifyOpenDBG(wszDbgPath, EC_DBG_NOT_FOUND, NULL);

        return false;
    }

    IMAGE_SEPARATE_DEBUG_HEADER DbgHeader;

    if (fread(&DbgHeader, sizeof(DbgHeader), 1, fd) != 1) {
        // The file isn't large enough to contain a DBG header

        NotifyOpenDBG(wszDbgPath, EC_FORMAT, NULL);

        fclose(fd);
        return false;
    }

    if (DbgHeader.Signature != IMAGE_SEPARATE_DEBUG_SIGNATURE) {
        // The file isn't a DBG file

        NotifyOpenDBG(wszDbgPath, EC_FORMAT, NULL);

        fclose(fd);
        return false;
    }

    // Skip over the section headers and exported names

    DWORD cb = DbgHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER) + DbgHeader.ExportedNamesSize;

    // Try to find the CodeView directory.
    // It's OK if we don't find it.

    if (fseek(fd, (long) cb, SEEK_CUR) != 0) {
        NotifyOpenDBG(wszDbgPath, EC_FORMAT, NULL);

        fclose(fd);
        return false;
    }

    long fo = ftell(fd);

    size_t cdbgdir = DbgHeader.DebugDirectorySize / sizeof(IMAGE_DEBUG_DIRECTORY);

    while (cdbgdir-- != 0) {
        IMAGE_DEBUG_DIRECTORY dbgdir;

        if (fread(&dbgdir, sizeof(dbgdir), 1, fd) != 1) {
            NotifyOpenDBG(wszDbgPath, EC_FORMAT, NULL);

            fclose(fd);
            return false;
        }

        if (!m_fCvInExe && (dbgdir.Type == IMAGE_DEBUG_TYPE_CODEVIEW)) {
            m_cbCv = dbgdir.SizeOfData;
            m_rvaCv = 0;
            m_foCv = dbgdir.PointerToRawData;
        }
    }

    // We check the timestamp after the debug directories so that we know that the
    // file is structurally valid before report a timestamp mismatch.  This is so that
    // we don't remember the timestamp mismatch for something not a valid DBG file.

    if (DbgHeader.TimeDateStamp != m_dwTimeStampExe) {
        if ((m_dwTimeStampDbg == 0) || (DbgHeader.TimeDateStamp != m_dwTimeStampDbg)) {
            // The timestamp doesn't match the executable

            NotifyOpenDBG(wszDbgPath, EC_INVALID_EXE_TIMESTAMP, NULL);

            fclose(fd);
            return false;
        }
    }

    m_wszDbgPath = _wcsdup(wszDbgPath);
    m_fdDbg = fd;

    NotifyOpenDBG(wszDbgPath, EC_OK, NULL);

    // We notify caller of debug directories after notifying
    // caller of DBG file otherwise the caller doesn't know
    // where to look during the NotifyDebugDir callback.

    if (fseek(fd, fo, SEEK_SET) == 0) {
        cdbgdir = DbgHeader.DebugDirectorySize / sizeof(IMAGE_DEBUG_DIRECTORY);

        while (cdbgdir-- != 0) {
            IMAGE_DEBUG_DIRECTORY dbgdir;

            if (fread(&dbgdir, sizeof(dbgdir), 1, fd) != 1) {
                break;;
            }

            PREFAST_SUPPRESS(6029, "NotifyDebugDir() won't write into dbgdir");
            NotifyDebugDir(false, &dbgdir);
        }
    }

    return true;
}


bool LOCATOR::FLocatePdbDefault(const wchar_t *wszReference)
{
    assert(m_wszCache == NULL);

    if (wszReference != NULL && !FRestrictReferencePath()) {
        // Attempt to open the PDB in the reference directory

        wchar_t wszDrive[_MAX_DRIVE];
        wchar_t wszDir[_MAX_DIR];

        _wsplitpath_s(wszReference, wszDrive, _countof(wszDrive), wszDir, _countof(wszDir), NULL, 0, NULL, 0);

        if (wszDir[0] == L'\0') {
            wszDir[0] = L'.';
            wszDir[1] = L'\0';
        }

        wchar_t wszFName[_MAX_FNAME];
        wchar_t wszExt[_MAX_EXT];

        _wsplitpath_s(m_wszPdb, NULL, 0, NULL, 0, wszFName, _countof(wszFName), wszExt, _countof(wszExt));

        wchar_t wszPdb[PDB_MAX_FILENAME];

        _wmakepath_s(wszPdb, _countof(wszPdb), wszDrive, wszDir, wszFName, wszExt);

        if (FOpenValidate4(wszPdb)) {
            return true;
        }
    }

    if (FRestrictOriginalPath()) {
        return false;
    }

    return FOpenValidate4(m_wszPdb);
}


bool LOCATOR::FLocatePdbPath(const wchar_t *wszSearchPath)
{
    if (wszSearchPath == NULL) {
        return false;
    }

    wchar_t wszRelPath[PDB_MAX_FILENAME];

    wcscpy_s(wszRelPath, _countof(wszRelPath), L"symbols\\");

    size_t cchExt;

    if (m_wszExt != NULL) {
        // The executable filename includes a file extension

        wcscat_s(wszRelPath + 8, _countof(wszRelPath) - 8, m_wszExt);
        wcscat_s(wszRelPath + 8, _countof(wszRelPath) - 8, L"\\");

        cchExt = wcslen(wszRelPath);
    }

    else {
        cchExt = 0;
    }

    wchar_t wszFName[_MAX_FNAME];
    wchar_t wszExt[_MAX_EXT];

    _wsplitpath_s(m_wszPdb, NULL, 0, NULL, 0, wszFName, _countof(wszFName), wszExt, _countof(wszExt));

    wcscat_s(wszRelPath, _countof(wszRelPath), wszFName);
    wcscat_s(wszRelPath, _countof(wszRelPath), wszExt);

    if (FLocateCvFilePathHelper(wszSearchPath, wszRelPath, cchExt, true)) {
        return true;
    }

    return false;
}


bool LOCATOR::FLocatePdbRegistry()
{
    if (FRestrictRegistry()) {
        return false;
    }

    if (FLocatePdbPath(m_registryUser.WszSearchPath())) {
        return true;
    }

    if (FLocatePdbPath(m_registryMachine.WszSearchPath())) {
        return true;
    }

    return false;
}


bool LOCATOR::FLocatePdbServer()
{
    // UNDONE

    return false;
}


bool LOCATOR::FLocatePdbSymsrv(const wchar_t *wsz)
{
    if (FRestrictSymsrv()) {
        return false;
    }

    const GUID *pguid = m_fGuid ? &m_guidSig : NULL;

    wchar_t wszPdb[PDB_MAX_FILENAME];
    DWORD dwLastError = 0;

    if (!m_symsrv.SymbolServer(this, wsz, m_wszPdb, pguid, m_sig, m_age, 0, wszPdb, &dwLastError)) {
        // if files could not be opened because the cache
        // location was bad or cache was full, notify the user

        if (dwLastError == ERROR_PATH_NOT_FOUND) {
            NotifyOpenPDB(wsz, EC_BAD_CACHE_PATH, NULL);
        }

        else if ((dwLastError == ERROR_HANDLE_DISK_FULL) || (dwLastError == ERROR_DISK_FULL)) {
            NotifyOpenPDB(wsz, EC_CACHE_FULL, NULL);
        }

        return false;
    }

    return FOpenValidate4(wszPdb);
}


bool LOCATOR::FOpenValidate4(const wchar_t *wszPdb)
{
    BOOL f = PDB::OpenValidate4(wszPdb, pdbRead, m_fGuid ? &m_guidSig : NULL, m_sig, m_age, &m_ec, m_wszError, cbErrMax, &m_ppdb);

    wchar_t wszCachedPdb[PDB_MAX_FILENAME];

    if (f) {
        // Cache the PDB that is successfully located.

        bool fCached = (m_wszCache != NULL) &&
                       (m_symsrv.m_pfnsymbolserverstorefilew != 0) &&
                       (*m_symsrv.m_pfnsymbolserverstorefilew)(m_wszCache,
                                                               wszPdb,
                                                               (void *) &m_guidSig,
                                                               m_age,
                                                               0,
                                                               wszCachedPdb,
                                                               PDB_MAX_FILENAME,
                                                               0);

        // If caching succeeded, use the cached PDB.

        if (fCached) {
            m_ppdb->Close();

            wszPdb = wszCachedPdb;

            f = PDB::OpenValidate4(wszPdb, pdbRead, m_fGuid ? &m_guidSig : NULL, m_sig,
                                   m_age, &m_ec, m_wszError, cbErrMax, &m_ppdb);
        }
    }

    PDBErrors ec;
    const wchar_t *wszError;

    if (f) {
        ec = EC_OK;
        wszError = NULL;
    }
    else {
        ec = (PDBErrors) m_ec;
        wszError = (m_wszError[0] == L'\0') ? NULL : m_wszError;
    }

    NotifyOpenPDB(wszPdb, ec, wszError);

    return f != FALSE;
}


bool LOCATOR::FReadAt(FILE *fd, DWORD fo, DWORD rva, size_t cb, void *pv, bool fAllowZeroRVA)
{
    // Handle the case where fd != NULL first.  FLocatePdb calls for either
    // the executable or DBG.  The callbacks don't apply to the DBG.

    if (fd != NULL) {
        if (fseek(fd, (long) fo, SEEK_SET) != 0) {
            return false;
        }

        size_t cbRead = fread(pv, 1, cb, fd);

        return cbRead == cb;
    }

    if (m_pfnReadExecutableAt != 0) {
        HRESULT hr = (*m_pfnReadExecutableAt)(m_pvClient, fo, (DWORD) cb, (BYTE *)pv);

        return SUCCEEDED(hr);
    }

    if ((m_pfnReadExecutableAtRVA != 0) && (fAllowZeroRVA || (rva != 0))) {
        HRESULT hr = (*m_pfnReadExecutableAtRVA)(m_pvClient, rva, (DWORD) cb, (BYTE *)pv);

        return SUCCEEDED(hr);
    }

    return false;
}


bool LOCATOR::FReadHeader(FILE *fd, DWORD fo, size_t cb, void *pv)
{
    return FReadAt(fd, fo, fo, cb, pv, true);
}


bool LOCATOR::FReadMiscDebugData()
{
    DWORD cbDebugData = 0;

    HRESULT hr = (*m_pfnReadMiscDebugData)(m_pvClient,
                                           &m_dwTimeStampExe,
                                           &m_dwTimeStampDbg,
                                           &m_dwSizeOfImage,
                                           &cbDebugData,
                                           NULL);

    if (FAILED(hr)) {
        return false;
    }

    SafeStackAllocator<0x400> allocator;
    BYTE *pbMiscData = allocator.Alloc<BYTE>(cbDebugData);

    if (pbMiscData == NULL) {
        SetError(EC_OUT_OF_MEMORY);

        return false;
    }

    hr = (*m_pfnReadMiscDebugData)(m_pvClient,
                                   &m_dwTimeStampExe,
                                   &m_dwTimeStampDbg,
                                   &m_dwSizeOfImage,
                                   &cbDebugData,
                                   pbMiscData);

    if (FAILED(hr)) {
        return false;
    }

    ReadMiscPath(pbMiscData, NULL, 0, 0, cbDebugData);

    return m_wszMiscPath != NULL;
}


bool LOCATOR::FRestrictDBG()
{
    if(!m_fPfnRestrictDBG) {
        m_fPfnRestrictDBG = true;

        m_pfnPdbRestrictDBG = (PfnPdbRestrictDBG)QueryCallback(povcRestrictDBG);
    }

    if (!m_pfnPdbRestrictDBG) {
        return false;
    }

    HRESULT hr = (*m_pfnPdbRestrictDBG)(m_pvClient);

    return hr != S_OK;
}


bool LOCATOR::FRestrictOriginalPath()
{
    if (!m_fPfnRestrictOriginalPath) {
        m_fPfnRestrictOriginalPath = true;

        m_pfnPdbRestrictOriginalPath = (PfnPdbRestrictOriginalPath) QueryCallback(povcRestrictOriginalPath);
    }

    if (m_pfnPdbRestrictOriginalPath == 0) {
        return false;
    }

    HRESULT hr = (*m_pfnPdbRestrictOriginalPath)(m_pvClient);

    return (hr != S_OK);
}


bool LOCATOR::FRestrictReferencePath()
{
    if (!m_fPfnRestrictReferencePath) {
        m_fPfnRestrictReferencePath = true;

        m_pfnPdbRestrictReferencePath = (PfnPdbRestrictReferencePath) QueryCallback(povcRestrictReferencePath);
    }

    if (m_pfnPdbRestrictReferencePath == 0) {
        return false;
    }

    HRESULT hr = (*m_pfnPdbRestrictReferencePath)(m_pvClient);

    return (hr != S_OK);
}


bool LOCATOR::FRestrictRegistry()
{
    if (!m_fPfnRestrictRegistry) {
        m_fPfnRestrictRegistry = true;

        m_pfnRestrictRegistry = (PfnPDBRestrictRegistry) QueryCallback(povcRestrictRegistry);
    }

    if (m_pfnRestrictRegistry == 0) {
        return false;
    }

    HRESULT hr = (*m_pfnRestrictRegistry)(m_pvClient);

    return (hr != S_OK);
}


bool LOCATOR::FRestrictSymsrv()
{
    if (!m_fPfnRestrictSymsrv) {
        m_fPfnRestrictSymsrv = true;

        m_pfnRestrictSymsrv = (PfnPDBRestrictSymsrv) QueryCallback(povcRestrictSymsrv);
    }

    if (m_pfnRestrictSymsrv == 0) {
        return false;
    }

    HRESULT hr = (*m_pfnRestrictSymsrv)(m_pvClient);

    return (hr != S_OK);
}


bool LOCATOR::FRestrictSystemRoot()
{
    if (!m_fPfnRestrictSystemRoot) {
        m_fPfnRestrictSystemRoot = true;

        m_pfnRestrictSystemRoot = (PfnPDBRestrictSystemRoot) QueryCallback(povcRestrictSystemRoot);
    }

    if (m_pfnRestrictSystemRoot == 0) {
        return false;
    }

    HRESULT hr = (*m_pfnRestrictSystemRoot)(m_pvClient);

    return (hr != S_OK);
}


bool LOCATOR::FSaveFileNames(const wchar_t *wszFilename)
{
    m_wszExePath = _wcsdup(wszFilename);

    if (m_wszExePath == NULL) {
        SetError(EC_OUT_OF_MEMORY);

        return false;
    }

    _wsplitpath_s(m_wszExePath, NULL, 0, NULL, 0, m_wszExeFName, _countof(m_wszExeFName), m_wszExeExt, _countof(m_wszExeExt));

    if ((m_wszExeExt[0] != L'\0') && (m_wszExeExt[1] != L'\0')) {
        // The executable filename includes a file extension

        m_wszExt = m_wszExeExt + 1;
    }

    else {
        // The executable filename does not include a file extension

        m_wszExt = NULL;
    }

    return true;
}


void LOCATOR::NotifyDebugDir(BOOL fExecutable, const struct _IMAGE_DEBUG_DIRECTORY *pdbgdir)
{
    if (!m_fPfnNotifyDebugDir) {
        m_fPfnNotifyDebugDir = true;

        m_pfnNotifyDebugDir = (PfnPDBNotifyDebugDir) QueryCallback(povcNotifyDebugDir);
    }

    if (m_pfnNotifyDebugDir == 0) {
        return;
    }

    (*m_pfnNotifyDebugDir)(m_pvClient, fExecutable, (const _IMAGE_DEBUG_DIRECTORY *)pdbgdir);
}


void LOCATOR::NotifyMiscPath(const wchar_t *wszMiscPath)
{
    if (!m_fPfnNotifyMiscPath) {
        m_fPfnNotifyMiscPath = true;

        m_pfnNotifyMiscPath = (PfnPDBNotifyMiscPath) QueryCallback(povcNotifyMiscPath);
    }

    if (m_pfnNotifyMiscPath == 0) {
        return;
    }

    (*m_pfnNotifyMiscPath)(m_pvClient, wszMiscPath);
}


void LOCATOR::NotifyOpenDBG(const wchar_t *wszDbgPath, PDBErrors ec, const wchar_t *wszError)
{
    SetError(ec, wszError);

    if (!m_fPfnNotifyOpenDBG) {
        m_fPfnNotifyOpenDBG = true;

        m_pfnNotifyOpenDBG = (PfnPDBNotifyOpenDBG) QueryCallback(povcNotifyOpenDBG);
    }

    if (m_pfnNotifyOpenDBG == 0) {
        return;
    }

    (*m_pfnNotifyOpenDBG)(m_pvClient, wszDbgPath, ec, wszError);
}


void LOCATOR::NotifyOpenPDB(const wchar_t *wszPdbPath, PDBErrors ec, const wchar_t *wszError)
{
    if (!m_fPfnNotifyOpenPDB) {
        m_fPfnNotifyOpenPDB = true;

        m_pfnNotifyOpenPDB = (PfnPDBNotifyOpenPDB) QueryCallback(povcNotifyOpenPDB);
    }

    if (m_pfnNotifyOpenPDB == 0) {
        return;
    }

    // Do some extra work to make sure we give back a full path to the file we
    // tried to open.

    SafeStackAllocator<1024> _allocator;
    wchar_t *wszPdbFull = NULL;

    DWORD cwchFullPath = GetFullPathNameW(wszPdbPath, 0, NULL, NULL);

    if (cwchFullPath != 0) {
        wszPdbFull = _allocator.Alloc<wchar_t>(cwchFullPath);

        if (wszPdbFull != NULL) {
            cwchFullPath = GetFullPathNameW(wszPdbPath, cwchFullPath, wszPdbFull, NULL);
        }
    }

    if (cwchFullPath == 0) {
        // Use the passed in PDB filename if GetFullPathNameW() failed.

        wszPdbFull = const_cast<wchar_t *>(wszPdbPath);
    }

    (*m_pfnNotifyOpenPDB)(m_pvClient, wszPdbFull, ec, wszError);
}


PDBCALLBACK LOCATOR::QueryCallback(POVC povc)
{
    if (m_pfnQueryCallback == 0) {
        return 0;
    }

    return (*m_pfnQueryCallback)(m_pvClient, povc);
}


void LOCATOR::ReadMiscPath(PB pb, FILE *fd, DWORD fo, DWORD rva, DWORD cb)
{
    DWORD off = 0;
    for (;;) {
        if (cb < offsetof(IMAGE_DEBUG_MISC, Data)) {
            // There isn't sufficient space for a misc entry header

            break;
        }

        IMAGE_DEBUG_MISC idm;

        if (pb != NULL) {
            memcpy(&idm, pb + off, sizeof(idm));
        }

        else if (!FReadAt(fd, fo + off, rva + off, offsetof(IMAGE_DEBUG_MISC, Data), &idm)) {
            // The debug information can't be read

            break;
        }

        if (idm.Length < offsetof(IMAGE_DEBUG_MISC, Data)) {
            // Length is less than size of header structure

            break;
        }

        if (idm.Length > cb) {
            // Data extends beyond end of length given in directory

            break;
        }

        if ((idm.DataType == IMAGE_DEBUG_MISC_EXENAME) && !idm.Unicode) {
            size_t cbPath = idm.Length - offsetof(IMAGE_DEBUG_MISC, Data);

            // I'm using _MAX_PATH +4 instead of _MAX_PATH because I found
            // some executable that had this size but otherwise looked valid.
            // Some early tools must have used too large a value.

            if (cbPath > (_MAX_PATH + 4)) {
                break;
            }

            char szMiscPath[_MAX_PATH + 4];

            if (pb != NULL) {
                memcpy(szMiscPath, pb + off + offsetof(IMAGE_DEBUG_MISC, Data), cbPath);
            }

            else if (!FReadAt(fd,
                              fo + off + offsetof(IMAGE_DEBUG_MISC, Data),
                              rva + off + offsetof(IMAGE_DEBUG_MISC, Data),
                              cbPath,
                              szMiscPath)) {
                // The debug information can't be read

                break;
            }

            szMiscPath[cbPath-1] = '\0';

            int cwch = MultiByteToWideChar(CP_ACP, 0, szMiscPath, -1, NULL, 0);

            if (cwch <= 0) {
                // The PDB path is not a valid CP_ACP string

                break;
            }

            wchar_t *wszMiscPath = new wchar_t[size_t(cwch)];

            if (wszMiscPath == NULL) {
                return;
            }

            cwch = MultiByteToWideChar(CP_ACP, 0, szMiscPath, -1, wszMiscPath, cwch);

            if (cwch <= 0) {
                delete [] wszMiscPath;

                break;
            }

            m_wszMiscPath = wszMiscPath;

            NotifyMiscPath(wszMiscPath);

            break;
        }

        off += idm.Length;
        cb  -= idm.Length;
    }
}


void LOCATOR::SetError(EC ec, const wchar_t *wsz)
{
    m_ec = ec;

    if (wsz == NULL) {
        m_wszError[0] = L'\0';
    }

    else {
        wcsncpy_s(m_wszError, _countof(m_wszError), wsz, _TRUNCATE);

        if (IsHighSurrogate(m_wszError[cbErrMax-2])) {
            m_wszError[cbErrMax-2] = L'\0';
        }
    }
}


LOCATOR::REGISTRY::REGISTRY(HKEY hkey)
{
    m_hkey = hkey;
    m_fInit = false;
    m_wszSearchPath = NULL;
}


LOCATOR::REGISTRY::~REGISTRY()
{
    delete [] m_wszSearchPath;
}


void LOCATOR::REGISTRY::Init()
{
    AutoLock lock(&m_cs);

    if (m_fInit) {
        return;
    }

    m_fInit = true;

    HKEY hkey;

    LONG l = RegOpenKeyExW(m_hkey, L"Software\\Microsoft\\VisualStudio\\MSPDB", 0, KEY_QUERY_VALUE, &hkey);

    if (l != ERROR_SUCCESS) {
        return;
    }

    DWORD cb = 0;

    l = RegQueryValueExW(hkey, L"SymbolSearchPath", NULL, NULL, NULL, &cb);

    if (l != ERROR_SUCCESS) {
        RegCloseKey(hkey);
        return;
    }

    DWORD cch = cb / sizeof(wchar_t);

    DWORD dwType;

    wchar_t *wsz = new wchar_t[cch + 1];

    if (wsz == NULL) {
        RegCloseKey(hkey);
        return;
    }

    l = RegQueryValueExW(hkey, L"SymbolSearchPath", NULL, &dwType, (LPBYTE) wsz, &cb);

    RegCloseKey(hkey);

    if (l != ERROR_SUCCESS) {
        delete [] wsz;

        return;
    }

    wsz[cch] = L'\0';              // Ensure zero terminated

    if (dwType == REG_SZ) {
        m_wszSearchPath = wsz;
        return;
    }

    if (dwType != REG_EXPAND_SZ) {
        delete [] wsz;

        return;
    }

    cch = ExpandEnvironmentStringsW(wsz, NULL, 0);

    if (cch == 0) {
        delete [] wsz;

        return;
    }

    wchar_t *wszExpanded = new wchar_t[cch];

    if (wszExpanded == NULL) {
        delete [] wsz;

        return;
    }

    cch = ExpandEnvironmentStringsW(wsz, wszExpanded, cch);

    delete [] wsz;

    if (cch == 0) {
        delete [] wszExpanded;

        return;
    }

    m_wszSearchPath = wszExpanded;
}


const wchar_t *LOCATOR::REGISTRY::WszSearchPath()
{
    Init();

    return m_wszSearchPath;
}


LOCATOR::SYMSRV::SYMSRV()
{
    m_fInit = false;
    m_hmod = NULL;
    m_pfnsymbolserverw = 0;
    m_pfnsymbolserversetoptions = 0;
    m_pfnsymbolserverstorefilew = 0;
}


LOCATOR::SYMSRV::~SYMSRV()
{
    if (m_hmod != NULL) {
        FreeLibrary(m_hmod);
    }
}


void LOCATOR::SYMSRV::Init()
{
    AutoLock lock(&m_cs);

    if (m_fInit) {
        return;
    }

    m_fInit = true;

    PREFAST_SUPPRESS(6321, "It's ok");
    HMODULE hmod = LoadLibraryExW(L"SYMSRV.DLL", NULL, 0);

    if ((UINT_PTR) hmod < 32) {
        return;
    }

    m_hmod = hmod;

    m_pfnsymbolserverw =
        PFNSYMBOLSERVERW(GetProcAddress(m_hmod, "SymbolServerW"));

    m_pfnsymbolserversetoptions =
        PFNSYMBOLSERVERSETOPTIONS(GetProcAddress(m_hmod, "SymbolServerSetOptions"));

    m_pfnsymbolserverstorefilew = 
        PFNSYMBOLSERVERSTOREFILEW(GetProcAddress(m_hmod, "SymbolServerStoreFileW"));
}


bool LOCATOR::SYMSRV::SymbolServer(LOCATOR *plocator, 
                                   const wchar_t *wsz,
                                   const wchar_t *wszPath, 
                                   const GUID *pguid,
                                   DWORD dwParam1,
                                   DWORD dwParam2,
                                   DWORD dwParam3,
                                   __out_ecount(PDB_MAX_FILENAME) wchar_t *wszTarget, 
                                   DWORD *pdwLastError)
{
    const wchar_t *wszSrvPath;

    if (wsz[3] == L'*') {
        // This is SRV*<server>

        wszSrvPath = wsz + 4;
    }
    else if (wsz[6] == L'*') {
        // This is SYMSRV*<dll>*<server>

        // We only support SYMSRV.DLL

        if (_wcsnicmp(wsz + 7, L"SYMSRV.DLL*", 11) != 0) {
            return false;
        }

        wszSrvPath = wsz + 18;
    }
    else if (wsz[5] == L'*') {
        // This is CACHE*<cache>

        assert(plocator->m_wszCache == NULL);

        wszSrvPath = wsz + 6;
    }
    else {
        assert(false);
        return false;
    }

    Init();

    if (m_pfnsymbolserverw == 0) {
        return false;
    }

    const void *pv;

    if (pguid != NULL) {
        if (m_pfnsymbolserversetoptions == 0) {
            return false;
        }

        (*m_pfnsymbolserversetoptions)(SSRVOPT_GUIDPTR, 1);

        pv = pguid;
    }

    else {
        if (m_pfnsymbolserversetoptions != 0) {
            (*m_pfnsymbolserversetoptions)(SSRVOPT_DWORD, 1);
        }

        pv = (void *) (uint_ptr_t) dwParam1;
    }

    // NT has filenames such as exe\notepad.dbg,
    // extract just the filename from there.

    wchar_t wszFName[_MAX_FNAME];
    wchar_t wszExt[_MAX_EXT];

    _wsplitpath_s(wszPath, NULL, 0, NULL, 0, wszFName, _countof(wszFName), wszExt, _countof(wszExt));

    wchar_t wszName[_MAX_FNAME + _MAX_EXT - 1];

    _snwprintf_s(wszName, _countof(wszName), _TRUNCATE, L"%s%s", wszFName, wszExt);

    if ((*m_pfnsymbolserverw)(wszSrvPath, wszName, pv, dwParam2, dwParam3, wszTarget)) {
        *pdwLastError = ERROR_SUCCESS;

        return true;
    }

    *pdwLastError = GetLastError();

    wszTarget[0] = L'\0';

    return false;
}
