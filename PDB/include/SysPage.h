#pragma once
#if !defined(_SysPage_h)
#define _SysPage_h
namespace pdb_internal {
    class SysPage {
        DWORD   m_cbPage;
    public:
        operator DWORD() {
            return m_cbPage;
            }
        SysPage() {
            SYSTEM_INFO si;
            ::GetSystemInfo(&si);
            m_cbPage = si.dwPageSize;
            }
        };
    extern SysPage  cbSysPage;
    }

#endif  // _SysPage_h
