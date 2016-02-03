#ifndef __ARRAY_INCLUDED__
#define __ARRAY_INCLUDED__

template <class T> inline void __swap(T& t1, T& t2) {
    T t = t1;
    t1 = t2;
    t2 = t;
}

#define self (*this)

namespace pdb_internal {

class Buffer;       // forward declaration of Buffer;

template <class T> class Array {
    T* rgt;
    unsigned itMac;
    unsigned itMax;
    enum { itMaxMax = (1<<29) };
public:
    Array() {
        rgt = 0;
        itMac = itMax = 0;
    }
    Array(unsigned itMac_) {
        rgt = (itMac_ > 0) ? new T[itMac_] : 0;
        itMac = itMax = rgt ? itMac_ : 0;
    }
    ~Array() {
        if (rgt)
            delete [] rgt;
    }
    BOOL isValidSubscript(size_t it) const {
        return it < itMac;
    }
    unsigned size() const {
        return itMac;
    }
    unsigned sizeMax() const {
        return itMax;
    }
    size_t cbArray() const {
        return itMac * sizeof T;
    }

    BOOL getAt(size_t it, T** ppt) const {
        if (isValidSubscript(it)) {
            *ppt = &rgt[it];
            return TRUE;
        }
        else
            return FALSE;
    }
    BOOL putAt(size_t it, const T& t) {
        if (isValidSubscript(it)) {
            rgt[it] = t;
            return TRUE;
        }
        else
            return FALSE;
    }
    T& operator[](size_t it) const {
        precondition(isValidSubscript(it));
        return rgt[it];
    }
    BOOL append(const T& t) {
        if (setSize(size() + 1)) {
            self[size() - 1] = t;
            return TRUE;
        } else
            return FALSE;
    }
    BOOL append(const T& t, unsigned &it) {
        if (append(t)) {
            it = size() - 1;
            return TRUE;
        }
        return FALSE;
    }

    void swap(Array& a) {
        ::__swap(rgt,   a.rgt);
        ::__swap(itMac, a.itMac);
        ::__swap(itMax, a.itMax);
    }
    void reset() {
        setSize(0);
    }
    void clear() {
        reset();
        if (rgt) {
            delete [] rgt;
            rgt = NULL;
            itMax = 0;
        }
    }
    void fill(const T& t) {
        for (unsigned it = 0; it < size(); it++)
            self[it] = t;
    }
    T * pBase() const {
        return rgt;
    }

    BOOL insertAt(size_t itInsert, const T& t);
    void deleteAt(size_t it);
    BOOL insertManyAt(size_t itInsert, unsigned ct);
    void deleteManyAt(size_t it, unsigned ct);
    BOOL setSize(size_t itMacNew);
    BOOL growMaxSize(size_t itMaxNew);
    BOOL findFirstEltSuchThat(BOOL (*pfn)(T*, void*), void* pArg, unsigned *pit) const;
    BOOL findFirstEltSuchThat_Rover(BOOL (*pfn)(T*, void*), void* pArg, unsigned *pit) const;
    unsigned binarySearch(BOOL (*pfnLE)(T*, void*), void* pArg) const;
    BOOL save(Buffer* pbuf) const;
#if 0
    BOOL reload(PB* ppb);   // REVIEW: Move this unsafe version to protected or remove entirely.
#endif
    BOOL reload(PB* ppb, CB cbReloadBuf);
    CB cbSave() const;

    static const size_t m_cElMax = __min(itMaxMax, ULONG_MAX / sizeof(T));
};

template <class T> inline BOOL Array<T>::insertAt(size_t it, const T& t) {
    precondition(isValidSubscript(it) || it == size());

    if (setSize(size() + 1)) {
        memmove(&rgt[it + 1], &rgt[it], (size() - (it + 1)) * sizeof(T));
        rgt[it] = t;
        return TRUE;
    }
    else
        return FALSE;
}

template <class T> inline BOOL Array<T>::insertManyAt(size_t it, unsigned ct) {
    precondition(isValidSubscript(it) || it == size());

    if (setSize(size() + ct)) {
        memmove(&rgt[it + ct], &rgt[it], (size() - (it + ct)) * sizeof(T));
        for (size_t itT = it; itT < it + ct; itT++) {
            rgt[itT] = T();
        }
        return TRUE;
    }
    else
        return FALSE;
}

template <class T> inline void Array<T>::deleteAt(size_t it) {
    precondition(isValidSubscript(it));

    memmove(&rgt[it], &rgt[it + 1], (size() - (it + 1)) * sizeof(T));
    verify(setSize(size() - 1));
    rgt[size()] = T();
}

template <class T> inline void Array<T>::deleteManyAt(size_t it, unsigned ct) {
    precondition(isValidSubscript(it));

    unsigned    ctActual = max(size() - it, ct);

    memmove(&rgt[it], &rgt[it + ctActual], (size() - (it + ctActual)) * sizeof(T));
    verify(setSize(size() - ctActual));
    for (unsigned itT = size(); itT < size() + ctActual; itT++) {
        rgt[itT] = T();
    }
}

// Make sure the array is big enough, only grows, never shrinks.
template <class T> inline
BOOL Array<T>::growMaxSize(size_t itMaxNew) {
    precondition(0 <= itMaxNew && itMaxNew <= m_cElMax);
    if (itMaxNew > m_cElMax) {
        return FALSE;
    }
    if (itMaxNew > itMax) {
        // Ensure growth is by at least 50% of former size.
        size_t  itMaxNewT = max(itMaxNew, 3*itMax/2);
        if (itMaxNewT > m_cElMax) {
            itMaxNewT = m_cElMax;
        }

        T* rgtNew = new T[itMaxNewT];
        if (!rgtNew)
            return FALSE;
        if (rgt) {
            for (unsigned it = 0; it < itMac; it++)
                rgtNew[it] = rgt[it];
            delete [] rgt;
        }
        rgt = rgtNew;
        itMax = static_cast<unsigned>(itMaxNewT);
    }
    return TRUE;
}

// Grow the array to a new size.
template <class T> inline
BOOL Array<T>::setSize(size_t itMacNew) {
    if (growMaxSize(itMacNew)) {
        itMac = static_cast<unsigned>(itMacNew);
        return TRUE;
    }
    return FALSE;
}

template <class T> inline
BOOL Array<T>::save(Buffer* pbuf) const {
    return pbuf->Append((PB)&itMac, sizeof itMac) &&
           (itMac == 0 || pbuf->Append((PB)rgt, itMac*sizeof(T)));
}

#if 0
template <class T> inline
BOOL Array<T>::reload(PB* ppb) {
    unsigned itMacNew = *((unsigned UNALIGNED *&)*ppb)++;

    if (!setSize(itMacNew)) {
        return FALSE;
    }
    memcpy(rgt, *ppb, itMac*sizeof(T));
    *ppb += itMac*sizeof(T);
    return TRUE;
}
#endif

template <class T> inline
BOOL Array<T>::reload(PB* ppb, CB cbReloadBuf) {

    assert(cbReloadBuf >= sizeof(unsigned));
    if (cbReloadBuf < sizeof(unsigned)) {
        return FALSE;
    }
    unsigned itMacNew = *((unsigned UNALIGNED *&)*ppb)++;
    cbReloadBuf -= sizeof(unsigned);

    if (itMacNew > m_cElMax || static_cast<unsigned>(cbReloadBuf) < itMacNew * sizeof(T)) {
        return FALSE;
    }

    if (!setSize(itMacNew)) {
        return FALSE;
    }
    memcpy(rgt, *ppb, itMac*sizeof(T));
    *ppb += itMac*sizeof(T);
    return TRUE;
}
template <class T> inline
CB Array<T>::cbSave() const {
    return sizeof(itMac) + itMac * sizeof(T);
}

template <class T> inline
BOOL Array<T>::findFirstEltSuchThat(BOOL (*pfn)(T*, void*), void* pArg, unsigned *pit) const
{
    for (unsigned it = 0; it < size(); ++it) {
        if ((*pfn)(&rgt[it], pArg)) {
            *pit = it;
            return TRUE;
        }
    }
    return FALSE;
}

template <class T> inline
BOOL Array<T>::findFirstEltSuchThat_Rover(BOOL (*pfn)(T*, void*), void* pArg, unsigned *pit) const
{
    precondition(pit);

    if (!(0 <= *pit && *pit < size()))
        *pit = 0;

    for (unsigned it = *pit; it < size(); ++it) {
        if ((*pfn)(&rgt[it], pArg)) {
            *pit = it;
            return TRUE;
        }
    }

    for (it = 0; it < *pit; ++it) {
        if ((*pfn)(&rgt[it], pArg)) {
            *pit = it;
            return TRUE;
        }
    }

    return FALSE;
}

template <class T> inline
unsigned Array<T>::binarySearch(BOOL (*pfnLE)(T*, void*), void* pArg) const
{
    unsigned itLo = 0;
    unsigned itHi = size();
    while (itLo < itHi) {
        // (low + high) / 2 might overflow
        unsigned itMid = itLo + (itHi - itLo) / 2;
        if ((*pfnLE)(&rgt[itMid], pArg))
            itHi = itMid;
        else
            itLo = itMid + 1;
    }
    postcondition(itLo == 0      || !(*pfnLE)(&rgt[itLo - 1], pArg));
    postcondition(itLo == size() ||  (*pfnLE)(&rgt[itLo], pArg));
    return itLo;
}

}   // namespace pdb_internal

#undef self

#endif // !__ARRAY_INCLUDED__
