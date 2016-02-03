#ifndef __PDBLOG_H__
#define __PDBLOG_H__


// PDB Logging or not - compiler needs these to be defined.
enum {
    LOG_STARTTIME,
    LOG_LOCK,
    LOG_LOCK_FAILED,
    LOG_RELEASE,
    LOG_RELEASE_EVENTMUTEX,
    LOG_FUNCSTART,
    LOG_FUNCEND,
    LOG_FUNCPARAM,
    LOG_FUNCPARAMW,
    LOG_FUNCRET,
    LOG_CTOR,
    LOG_DTOR,
    LOG_CUSTOM,
    LOG_MAX
};


#ifdef PDB_LOGGING

#pragma message("Logging enabled")

// USE THESE MACROS
void PDBLogStartLogEntry(void * ptr, char const * file, DWORD line, char const * func, char const* funcsig);
void PDBLogEndLogEntry(DWORD msgid, ...);

#define PDBLOGSTART(x) PDBLogStartLogEntry(x, __FILE__, __LINE__, __FUNCTION__, __FUNCSIG__);
#define PDBLOG __if_exists(this) { PDBLOGSTART((void*)this); } __if_not_exists(this) { PDBLOGSTART(NULL); } PDBLogEndLogEntry
#define PDBLOG_FUNCEX(x) AutoFuncLog autofunclog(x, __FILE__, __LINE__, __FUNCTION__, __FUNCSIG__)
#define PDBLOG_FUNC() __if_exists(this) { PDBLOG_FUNCEX((void *)this); } __if_not_exists(this) { PDBLOG_FUNCEX(NULL); }
#define PDBLOG_FUNCARG(x) autofunclog.LogParam(ConvertToString(x))
#define PDBLOG_FUNCRET(x) PDBLOG(LOG_FUNCRET, x)

#include <stdio.h>
#include <stdarg.h>
#include "buffer.h"
#include "array.h"

#ifdef PDB_MT
#include "critsec.h"
#endif

class PDBLogger {
public:
    PDBLogger() {};
    virtual ~PDBLogger() {};
    virtual void WriteLogEx(void *, char const *, DWORD, char const*, char const *, DWORD, va_list l) = 0;
    virtual void WriteLog( void * ptr, char const * file, DWORD line, char const * func, char const * funcsig, DWORD msgid, ...) {
        va_list arg;
        va_start(arg, msgid);
        WriteLogEx(ptr, file, line, func, funcsig, msgid, arg);
        va_end(arg);
    }

    static void StartLogEntry(void * ptr, char const * file, DWORD line, char const * func, char const * funcsig);
    static void EndLogEntry(DWORD msgid, ...);
    static void EndLogEntryEx(DWORD msgid, va_list l);
    static void AddLogger(PDBLogger * logger);

    static class Filter; 
    static Filter& GetFilter() { return s_filter; }
private:
    static void *               s_this_ptr;
    static char const *         s_filename;
    static DWORD                s_linenumber;
    static char const *         s_funcname;
    static char const *         s_funcsig;
    static bool                 s_fEnabled;
    static Array<PDBLogger *>   s_rgPDBLogger;
    static Filter               s_filter;
#ifdef PDB_MT
    static CriticalSectionNoLog s_cs;
#endif

protected:
    static char const * msgtype[LOG_MAX];
    static char const * GetMessage(DWORD msgid, va_list& l) {
        if (msgtype[msgid] != NULL) { return msgtype[msgid]; }
        return va_arg(l, char const*);
    }

    friend class DoneInit;
};

/* ====================================================================
 *  Auto function/method loggin
 * ====================================================================*/
class AutoFuncLog {
public:
    AutoFuncLog(void * t, char const * f, DWORD l, char const * fc, char const * fs) :
        _t(t), _f(f), _l(l), _fc(fc), _fs(fs), _argnum(1)
    {
        PDBLogger::StartLogEntry(_t, _f, _l, _fc, _fs);
        PDBLogger::EndLogEntry(LOG_FUNCSTART);
    }
    ~AutoFuncLog() {
        PDBLogger::StartLogEntry(_t, _f, _l, _fc, _fs);
        PDBLogger::EndLogEntry(LOG_FUNCEND);
    }
/*
    template<class T>
    void LogParam(T const& t) {
        char buf[sizeof(T) * 3 + 1];
        char * curr = buf;
        for (unsigned i = 0; i < sizeof(T); i++) {
            sprintf(curr, "%02X ", ((char *)&t)[i]);
            curr += 3;
        }
        PDBLogger::StartLogEntry(_t, _f, _l, _fc, _fs);
        PDBLogger::EndLogEntry(LOG_FUNCPARAM, _argnum++, buf);
    }
*/

    void LogParam(char const * t) {
        PDBLogger::StartLogEntry(_t, _f, _l, _fc, _fs);
        PDBLogger::EndLogEntry(LOG_FUNCPARAM, _argnum++, t);
    }


    void LogParam(wchar_t const * t) {
        PDBLogger::StartLogEntry(_t, _f, _l, _fc, _fs);
        PDBLogger::EndLogEntry(LOG_FUNCPARAMW, _argnum++, t);
    }
private:
    void * _t;
    char const * _f;
    DWORD _l;
    char const * _fc;
    char const * _fs;
    DWORD _argnum;
};

static wchar_t s_buf[2048];


template <class T>
wchar_t const * ConvertToString(T * t){
    swprintf_s(s_buf, 2048, L"0x%08X", t);
    return s_buf;
}



inline wchar_t const * ConvertToString(char const * t) {
    if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, t, -1, s_buf, 2047) == 0) {
        return L"Cannot convert to wide char!";
    }
    return s_buf;
}


inline wchar_t const * ConvertToString(wchar_t const * t) {
    return t;
}
inline wchar_t const * ConvertToString(int i) {
    swprintf_s(s_buf, 2048, L"%d", i);
    return s_buf;
}

inline wchar_t const * ConvertToString(bool t) {
    return (t? L"true" : L"false");
}



/* ====================================================================
 *  Filter
 * ====================================================================*/
class PDBLogger::Filter {
public:
    PDBLogger::Filter();
    bool IsFiltered(void * ptr, char const* file, DWORD line, char const * func, char const * funcsig, DWORD msgid) {
        MTS_PROTECTNOLOG(s_cs);
        unsigned it;
        if (!rgFEnabledMessage[msgid]) {
            return true;
        }
        if ((!!rgFilenames.findFirstEltSuchThat(&StrCmp, (void *)file, &it) == fEnabledFilenames) &&
            (!!rgFilenamesSub.findFirstEltSuchThat(&StrStrB, (void *)file, &it) == fEnabledFilenames)) 
        {
            return true;
        }
        if ((!!rgFunctions.findFirstEltSuchThat(&StrCmp, (void *)func, &it) == fEnabledFunctions) &&
            (!!rgFunctionsSub.findFirstEltSuchThat(&StrStrB, (void *)func, &it) == fEnabledFunctions))
        {
            return true;
        }
        return false;
    }

    void ToggleAllMessages(bool fEnable) {
        MTS_PROTECTNOLOG(s_cs);
        memset(rgFEnabledMessage, fEnable? 1 : 0, sizeof(rgFEnabledMessage));
    }
    void ToggleAllFunctions(bool fEnable) {
        MTS_PROTECTNOLOG(s_cs);
        fEnabledFunctions = fEnable;
        rgFunctions.reset();
    }

    void ToggleAllFiles(bool fEnable) {
        MTS_PROTECTNOLOG(s_cs);
        fEnabledFilenames = fEnable;
        rgFilenames.reset();
    }
    
    void ToggleAll(bool fEnable) {
        MTS_PROTECTNOLOG(s_cs);
        ToggleAllFunctions(fEnable);
        ToggleAllFiles(fEnable);
        ToggleAllMessages(fEnable);
    }
    void InvertAll() {
        MTS_PROTECTNOLOG(s_cs);
        fEnabledFunctions = !fEnabledFunctions;
        fEnabledFilenames = !fEnabledFilenames;
        for (DWORD i = 0; i < LOG_MAX; i++) {
            rgFEnabledMessage[i] = !rgFEnabledMessage[i];
        }
    }
    void ToggleMessage(bool fEnable, DWORD msgid) {
        MTS_PROTECTNOLOG(s_cs);
        rgFEnabledMessage[msgid] = fEnable;
    }

    void ToggleFile(bool fEnable, char const * file) {          
        Change(rgFilenames, &StrCmp, file, fEnable == fEnabledFilenames);
    }
    void ToggleFunction(bool fEnable, char const * func) {
        Change(rgFunctions, &StrCmp, func, fEnable == fEnabledFunctions);
    }

    void ToggleFileEx(bool fEnable, char const * file) {
        Change(rgFilenamesSub, &StrCmp, file, fEnable == fEnabledFilenames);
    }
    void ToggleFunctionEx(bool fEnable, char const * file) {
        Change(rgFunctionsSub, &StrCmp, file, fEnable == fEnabledFunctions);
    }
private:
    static BOOL __cdecl StrStrA(char const ** a, void * b) {
        return (strstr(*a, (char const *)b) != NULL);
    }
    static BOOL __cdecl StrStrB(char const ** a, void * b) {
        return (strstr((char const *)b, *a) != NULL);
    }
    static BOOL __cdecl StrCmp(char const ** a, void * b) {
        return (strcmp(*a, (char const *)b) == 0);
    }

    template <class T>
    static void Change(Array<T>& rgArray, BOOL (*pfn)(T*, void*), T p, bool remove) {
        MTS_PROTECTNOLOG(s_cs);
        unsigned it;
        BOOL found = rgArray.findFirstEltSuchThat(pfn, (void *)p, &it);
        if (remove) {
            if (found) {
                unsigned newsize = rgArray.size() - 1;
                if (it != newsize) {
                    rgArray[it] = rgArray[newsize];
                }
                rgArray.setSize(newsize);
            }
        } else {
            if (!found) {
                rgArray.append(p);
            }
        }
    }

    bool rgFEnabledMessage[LOG_MAX];
    bool fEnabledFilenames;
    bool fEnabledFunctions;
    Array<char const *> rgFilenames;
    Array<char const *> rgFilenamesSub;
    Array<char const *> rgFunctions;
    Array<char const *> rgFunctionsSub;
};

inline void PDBLogger::StartLogEntry(void * ptr, char const * file, DWORD line, char const * func, char const* funcsig) {
    if (!s_fEnabled) { 
        return; 
    }
    MTS_ENTER(s_cs);
    s_this_ptr = ptr;
    s_filename = file;
    s_linenumber = line;
    s_funcname = func;
    s_funcsig = funcsig;
}

inline void PDBLogger::EndLogEntryEx(DWORD msgid, va_list l) { 
    if (!s_fEnabled) { 
        return; 
    }
    if (!s_filter.IsFiltered(s_this_ptr, s_filename, s_linenumber, s_funcname, s_funcsig, msgid)) {
        for (unsigned i = 0; i < s_rgPDBLogger.size(); i++) {
            s_rgPDBLogger[i]->WriteLogEx(s_this_ptr, s_filename, s_linenumber, s_funcname, s_funcsig, msgid, l);
        }
        va_end(l);
    }
    MTS_LEAVE(s_cs);
}

inline void PDBLogger::EndLogEntry(DWORD msgid, ...) { 
    if (!s_fEnabled) { 
        return; 
    }
    if (!s_filter.IsFiltered(s_this_ptr, s_filename, s_linenumber, s_funcname, s_funcsig, msgid)) {
        va_list l;
        va_start(l, msgid);
        for (unsigned i = 0; i < s_rgPDBLogger.size(); i++) {
            s_rgPDBLogger[i]->WriteLogEx(s_this_ptr, s_filename, s_linenumber, s_funcname, s_funcsig, msgid, l);
        }
        va_end(l);
    }
    MTS_LEAVE(s_cs);
}

#include <time.h>
inline void PDBLogger::AddLogger(PDBLogger * log) {
    if (!s_fEnabled) { 
        return; 
    }
    MTS_PROTECTNOLOG(s_cs);
    s_rgPDBLogger.append(log);
    
    char tmpbuf[128];            
    _strdate( tmpbuf );
    log->WriteLog(log, __FILE__, __LINE__, __FUNCTION__, __FUNCSIG__, LOG_STARTTIME, tmpbuf);
}

inline void PDBLogStartLogEntry(void * ptr, char const * file, DWORD line, char const * func, char const* funcsig)
{
	PDBLogger::StartLogEntry(ptr, file, line, func, funcsig);
}

inline void PDBLogEndLogEntry(DWORD msgid, ...)
{
	va_list l;
	va_start(l, msgid);
	PDBLogger::EndLogEntryEx(msgid, l);
}

#else

void inline dummyPdbLog(...) { }

#define PDBLOG					dummyPdbLog					
#define PDBLOG_FUNC()
#define PDBLOG_FUNCARG(x)
#define PDBLOG_FUNCRET(x)

#endif
#endif // __PDBLOG_H__
