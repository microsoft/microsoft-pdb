#include "pdbimpl.h"
#include "dbiimpl.h"

#ifdef  _M_IX86
void InitPfnWow64FsRedirection();
#endif  // _M_IX86

#if 0

enum PRTY
{
    prtyMin  = THREAD_PRIORITY_LOWEST,
    prtyMost = THREAD_PRIORITY_HIGHEST
};

#endif


#ifdef  _M_IX86

BOOL WINAPI DllMain(HINSTANCE h, ULONG ulReason, PVOID pvReserved)
{
#if 0
    static int dPrty = 0;
    static bool fDoSetPrty;
#endif

    switch ( ulReason ) {
        case DLL_PROCESS_ATTACH:
#ifdef  _M_IX86
            InitPfnWow64FsRedirection();
#endif  // _M_IX86

#if 0
        {
            LPCTSTR szMsDev = _tgetenv(_TEXT("_MSDEV_BLD_ENV_"));

            if (szMsDev != NULL) {
                // check for the reg key first, it overrides the value,
                // but not the existence of the env var

                bool fRegKeyPresent = false;
                HKEY hkey;

                if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_CURRENT_USER,
                                                  _TEXT("Software\\Microsoft\\DevStudio"),
                                                  0,
                                                  KEY_READ,
                                                  &hkey)) {
                    DWORD dwValType;
                    DWORD cbVal = sizeof(int);

                    if (ERROR_SUCCESS == RegQueryValueEx(hkey,
                                                         _TEXT("ToolPriorityDelta"),
                                                         0,
                                                         &dwValType,
                                                         PB(&dPrty),
                                                         &cbVal)) {
                        fRegKeyPresent = dwValType == REG_DWORD;
                    }

                    RegCloseKey(hkey);
                }

                SYSTEM_INFO si;

                ::GetSystemInfo(&si);

                fDoSetPrty = ((si.dwNumberOfProcessors > 1) || fRegKeyPresent) &&
                             (dPrty >= prtyMin) &&
                             (dPrty <= prtyMost);
            }
        }

        // Fall through and get the initial thread...

        case DLL_THREAD_ATTACH :
            if (fDoSetPrty) {
                HANDLE hThread = ::GetCurrentThread();

                ::SetThreadPriority(hThread, dPrty);

                debug(_TCHAR szMsg[128]);
                debug(_stprintf(szMsg, _TEXT("MSPdb::PriorityDelta(%d)\r\n"), dPrty));
                debug(::OutputDebugString(szMsg));
            }
            break;
#endif
    }

    return true;
}

#endif