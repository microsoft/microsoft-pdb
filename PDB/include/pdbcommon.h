#include "pdb.h"
#include "pdblog.h"
#include <string.h>

class PDBCommon : public PDB {
public:
    enum {
        impvVC2      = PDBImpvVC2,
        impvVC4      = PDBImpvVC4,
        impvVC50     = PDBImpvVC50,
        impvVC98     = PDBImpvVC98,
        impvVC70Dep  = PDBImpvVC70Dep,   // deprecated 7.0 verison
        impvVC70     = PDBImpvVC70,
        impvVC110    = PDBImpvVC110,
        impvVC140    = PDBImpvVC140,

        // TODO -- need to update this to impvVC140 when we are ready
        impv         = impvVC70,
    };

    enum {
        featNoTypeMerge     = 0x4D544F4E,    // "NOTM"
        featMinimalDbgInfo  = 0x494E494D,    // "MINI"
    };

    static BOOL ExportValidateInterface(INTV intv_)
    {
        // Current interface is assumed to be backward
        // compatible with alternate interface intvAlt;
        // so we validate both
        return
            intv == intv_ ||
            intvVC80 == intv_ ||
            intvVC70 == intv_ ||
            intvVC70Dep == intv_
            ;
    }

    static BOOL ExportValidateImplementation(IMPV impv_)
    {   
        // We could make the implementation versions
        // downword compatible, but like in case of
        // VC70, it might not always be possible
        return (PDBImpv == impv_);
    }

    static INTV QueryInterfaceVersionStatic()
    {
        return intv;
    }

    static IMPV QueryImplementationVersionStatic()
    {
        return impv;
    }
    static BOOL
           OpenValidate5(
               const wchar_t *wszExecutable,
               const wchar_t *wszSearchPath,
               void *pvClient,
               PfnPDBQueryCallback pfnQueryCallback,
               OUT EC *pec,
               __out_ecount_opt(cchErrMax) wchar_t *wszError,
               size_t cchErrMax,
               OUT PDB **pppdb);

    static BOOL 
           Open2W(
                const wchar_t *wszPDB, 
                const char *szMode, 
                OUT EC *pec,
                _Out_opt_cap_(cchErrMax) OUT wchar_t *wszError, 
                size_t cchErrMax, 
                OUT PDB **pppdb);

    BOOL ValidateInterface()
    {
        return ExportValidateInterface(intv);
    }
    BOOL CopyTo(const char *szDst, DWORD dwCopyFilter, DWORD dwReserved);
    BOOL CopyToW(const wchar_t *wszDst, DWORD dwCopyFilter, DWORD dwReserved);
    BOOL OpenStream(SZ_CONST sz, OUT Stream **ppstream);
    BOOL OpenStreamW(const wchar_t * szStream, OUT Stream** ppstream);
    EC QueryLastError(_Out_opt_cap_(cbErrMax) char szError[cbErrMax]);

#ifdef PDB_TYPESERVER
    // used by DBI1::fOpenPdb
    wchar_t *getExeNameW() { return (wszExeName[0] != L'\0') ? wszExeName : NULL; }

    // used by PDBCommon::OpenValidate5()
    void setExeNameW(const wchar_t *wsz) {
        if (wsz != NULL) {
            wcsncpy_s(wszExeName, _countof(wszExeName), wsz, _TRUNCATE);
        }
        else {
            wszExeName[0] = L'\0';
        }
    }
protected:
    PDBCommon() {
        wszExeName[0] = L'\0';
    }

private:
    wchar_t     wszExeName[_MAX_PATH];
#endif
};

inline void setError(OUT EC *pec, EC ec, __out_ecount_opt(cchErrMax) wchar_t *wszError, size_t cchErrMax, const wchar_t *wsz)
{
    PDBLOG_FUNC();
    if (pec != NULL) {
        *pec = ec;
    }

    if (wszError != NULL && cchErrMax > 0) {
        wcsncpy_s(wszError, cchErrMax, wsz, _TRUNCATE);
    }
}
