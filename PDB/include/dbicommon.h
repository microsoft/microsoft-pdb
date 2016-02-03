#pragma once

#if !defined(isectNil)
#define isectNil 	((ISECT)-1)			
#endif

typedef IMOD XIMOD; // external module index; 1 based

inline IMOD imodForXimod(XIMOD ximod) { return ximod - 1; }
inline XIMOD ximodForImod(IMOD imod) { return imod + 1; }

struct SC20 {
    ISECT   isect;
    OFF     off;
    CB      cb;
    IMOD    imod;
};

struct SC40 {
    ISECT   isect;
    OFF     off;
    CB      cb;
    DWORD   dwCharacteristics;
    IMOD    imod;

    SC40() : isect(isectNil), off(0), cb(cbNil), dwCharacteristics(0), imod(imodNil) { expect(fAlign(this)); }

    inline int IsAddrInSC(ISECT isect_, OFF off_) const
    {
        if (isect == isect_) {
            if (off_ < off)
                return -1;
            if (off_ - off < cb)
                return 0;
            return 1;
        }
        else
            return (isect_ - isect);
    }
};

struct SC: public SC40 {
    DWORD   dwDataCrc;
    DWORD   dwRelocCrc;

    SC() : dwDataCrc(0), dwRelocCrc(0) {}

    inline bool Match(IMOD imod_, CB cb_, DWORD dwDataCrc_, DWORD dwRelocCrc_) const
    {
        return 
            imod == imod_       // only interested in this one imod
            && dwDataCrc == dwDataCrc_   
            && dwRelocCrc == dwRelocCrc_
            && cb == cb_;
    }
    inline bool Match(IMOD imod_, CB cb_, DWORD dwDataCrc_, DWORD dwRelocCrc_, DWORD dwCharacteristics_) const
    {
        return 
            imod == imod_       // only interested in this one imod
            && dwDataCrc == dwDataCrc_   
            && dwRelocCrc == dwRelocCrc_
            && cb == cb_
            && dwCharacteristics == dwCharacteristics_;
    }
    SC& operator=(const SC20& sc20)
    {
        isect = sc20.isect;
        off = sc20.off;
        cb = sc20.cb;
        dwCharacteristics = 0;
        imod = sc20.imod;
        dwDataCrc = 0;
        dwRelocCrc = 0;
        return *this;
    }
    SC& operator=(const SC40& sc40)
    {
        isect = sc40.isect;
        off = sc40.off;
        cb = sc40.cb;
        dwCharacteristics = sc40.dwCharacteristics;
        imod = sc40.imod;
        dwDataCrc = 0;
        dwRelocCrc = 0;
        return *this;
    }
    SC& operator=(const SC& sc)
    {
        isect = sc.isect;
        off = sc.off;
        cb = sc.cb;
        dwCharacteristics = sc.dwCharacteristics;
        imod = sc.imod;
        dwDataCrc = sc.dwDataCrc;
        dwRelocCrc = sc.dwRelocCrc;
        return *this;
    }
    static int compareSC(const void* pv1, const void* pv2) 
    {
        const SC *psc1 = reinterpret_cast<const SC *>(pv1);
        const SC *psc2 = reinterpret_cast<const SC *>(pv2);
        return psc2->IsAddrInSC(psc1->isect, psc1->off);
    }
};


struct SC2: public SC {
    DWORD   isectCoff;

    SC2() : isectCoff(0) {}

    SC2& operator=(const SC2& sc)
    {
        isect = sc.isect;
        off = sc.off;
        cb = sc.cb;
        dwCharacteristics = sc.dwCharacteristics;
        imod = sc.imod;
        dwDataCrc = sc.dwDataCrc;
        dwRelocCrc = sc.dwRelocCrc;
        isectCoff = sc.isectCoff;
        return *this;
    }
    static int compareSC2(const void* pv1, const void* pv2) 
    {
        const SC2 *psc1 = reinterpret_cast<const SC2 *>(pv1);
        const SC2 *psc2 = reinterpret_cast<const SC2 *>(pv2);
        return psc2->IsAddrInSC(psc1->isect, psc1->off);
    }
};


template <typename T>
class EnumSC : public EnumContrib
{
public:
    EnumSC(const Buffer& buf) : bufSC(buf)
    {
        reset();
    }

    EnumSC(const EnumSC& esc) : bufSC(esc.bufSC), i(esc.i)
    {
    }

    void release()
    {
        delete this;
    }

    void reset()
    {
        i = (size_t)-1;
    }

    BOOL next()
    {
        if (++i * sizeof(T) < (size_t) bufSC.Size()) {
            return TRUE;
        }

        return FALSE;
    }

    BOOL prev()
    {
        BOOL ret = i != 0;

        if (i > 0) {
            i--;
        }

        return ret;
    }

    void get(OUT USHORT* pimod, OUT USHORT* pisect, OUT long* poff, OUT long* pcb, OUT ULONG* pdwCharacteristics)
    {
        assert(sizeof(T) == sizeof(SC));
        assert(i != (size_t)-1);
        assert(i * sizeof(T) < (size_t)bufSC.Size());

        const SC* psc = reinterpret_cast<SC *>(bufSC.Start()) + i;

        *pimod = ximodForImod(psc->imod);
        *pisect = psc->isect;
        *poff = psc->off;
        *pcb = psc->cb;
        *pdwCharacteristics = psc->dwCharacteristics;
    }

    void get2(OUT USHORT* pimod, OUT USHORT* pisect, OUT DWORD* poff, OUT DWORD* pisectCoff,
              OUT DWORD* pcb, OUT ULONG* pdwCharacteristics)
    {
        assert(sizeof(T) == sizeof(SC2));
        assert(i != (size_t)-1);
        assert(i * sizeof(T) < (size_t)bufSC.Size());

        const SC2* psc = reinterpret_cast<SC2 *>(bufSC.Start()) + i;

        *pimod = ximodForImod(psc->imod);
        *pisect = psc->isect;
        *poff = psc->off;
        *pisectCoff = psc->isectCoff;
        *pcb = psc->cb;
        *pdwCharacteristics = psc->dwCharacteristics;
    }

    void getCrcs(OUT DWORD* pcrcData, OUT DWORD* pcrcReloc)
    {
        const T* psc = (T *) bufSC.Start() + i;

        *pcrcData = psc->dwDataCrc;
        *pcrcReloc = psc->dwRelocCrc;
    }

    bool fUpdate(long off, long cb)
    {
        assert(i * sizeof(T) < size_t(bufSC.Size()));

        const size_t cElMax = s_cbMaxAlloc / sizeof(T);

        if (i <= cElMax && i * sizeof(T) < size_t(bufSC.Size())) {
            T *psc = reinterpret_cast<T *>(bufSC.Start()) + i;
            psc->off = off;
            psc->cb = cb;
            return true;
        }

        return false;
    }

    BOOL clone(EnumContrib **ppEnum)
    {
        return (*ppEnum = new EnumSC(*this)) != NULL;
    }

    BOOL locate(long isect, long off)
    {
        isect &= 0xFFFF;    // ISECT(isect) is going to do this
                            // anyway, just make it explicit!

        T* pscLo = reinterpret_cast<T *>(bufSC.Start());
        T* pscHi = pscLo + ((bufSC.Size() / sizeof(T)) - 1);
        T* psc;

        assert(pscLo != NULL && pscHi != NULL);

        // binary search for containing SC
        while(pscLo < pscHi) {
            psc = pscLo + ((pscHi - pscLo) / 2);
            int iResult = psc->IsAddrInSC(ISECT(isect), off);
            if (iResult < 0) {
                pscHi = psc;
            }
            else if (iResult > 0) {
                pscLo = psc + 1;
            }
            else {
                i = (size_t)(psc - reinterpret_cast<T *>(bufSC.Start())) - 1;
                return TRUE;
            }
        }

        assert(pscHi == pscLo);
        psc = pscHi;

        if (psc->IsAddrInSC(ISECT(isect), off) == 0) {
            i = (size_t)(psc - reinterpret_cast<T *>(bufSC.Start())) - 1;
            return TRUE;
        }

        pscLo = reinterpret_cast<T *>(bufSC.Start());
        pscHi = pscLo + ((bufSC.Size() / sizeof(T)) - 1);

        if (pscLo->IsAddrInSC(ISECT(isect), off) < 0) {
            i = (size_t) -1;
        }
        else if (pscHi->IsAddrInSC(ISECT(isect), off) > 0) {
            i = ((bufSC.Size() / sizeof(T)) - 1);
        }
        else {
            int iResult = psc->IsAddrInSC(ISECT(isect), off);
            i = psc - pscLo + (iResult > 0 ? 0 : -2);
        }

        return FALSE;
    }

private:
    const Buffer& bufSC;
    size_t i;
};


class DBICommon : public DBI {
public:
    BOOL QueryTypeServerByPdb(const char* szPdb, OUT ITSM* pitsm);
    BOOL OpenMod(SZ_CONST szModule, SZ_CONST szObjFile, OUT Mod** ppmod);       // mts safe
};
