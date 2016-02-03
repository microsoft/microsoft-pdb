#ifndef __MAP_INCLUDED__
#define __MAP_INCLUDED__

#ifndef __ARRAY_INCLUDED__
#include "array.h"
#endif
#ifndef __TWO_INCLUDED__
#include "two.h"
#endif
#ifndef __ISET_INCLUDED__
#include "iset.h"
#endif

#include "mts.h"

#define self (*this)

//
// standard version of the HashClass merely casts the object to a HASH.
//  by convention, this one is always HashClass<H,hcCast>
//
#define hcCast 0
// SIG is an unsigned long (like NI!) and needs a different hash function//
#define hcSig 1
// KEY is an unsigned long (like NI!) and needs a different hash function
#define hcKey 2
// Standard hash class with context
// #define hcContext 3
// simple hash class which hashes using MD5 hash
#define hcMD5 4
// hash class with CRC hash
#define hcCRC 5
// casting long hash
#define hcLCast 6
// casting long pointer hash
#define hcLPtr 7

namespace pdb_internal {

const size_t    HASH_MAX = 0xffff;
const size_t    LHASH_MAX = 0xffffffff;

template<typename D>
class HashClassBase { 
public:
    BOOL Equals(const D& d1, const D& d2) { return d1 == d2; }
};

template<typename D, int i>
class HashClass;

template<typename D, int i, int cbitsInsignificant>
class HashClass2;

template<typename D>
class HashClass<D, hcCast> : public HashClassBase<D> {
public:
    HashClass( void *pvoid = NULL ) { // Just there so that this code compiles
        assert(pvoid == NULL); 
    }
    // Converts domain to hash
    inline HASH __fastcall operator()(const D& d) {
        return HASH(d);
    }
};

template<typename D>
class HashClass<typename D, hcCRC> : public HashClassBase<D> {
public:
    HashClass( void *pvoid = NULL ) { // Just there so that this code compiles
        assert(pvoid == NULL); 
    }
    // Converts domain to hash
    inline LHASH __fastcall operator()(const D& d) {        
        return SigForPbCb(PB(&d), sizeof(D), ULONG(-1));
    }
};

template<typename D>
class HashClass<typename D, hcMD5> : public HashClassBase<D> {
public:
    HashClass( void *pvoid = NULL ) { // Just there so that this code compiles
        assert(pvoid == NULL); 
    }
    // Converts domain to hash
    inline LHASH __fastcall operator()(const D& d) {        
        return LHashPbCb(PB(&d), sizeof(D), ULONG(-1));
    }
};

template <class D, class C>
class HashClassWithContext {
public:
    HashClassWithContext(C *pC) {
        assert(pC != NULL);
        pContext = pC;
    }
    // Converts domain to hash
    inline LHASH __fastcall operator()(const D& d) {
        return d.Hash(pContext);
    }
    BOOL Equals(const D& d1, const D& d2) { 
        // Must have a different equals that
        // fits with the context ....
        return d1.Equals(d2, pContext); 
    }
protected:
    C *pContext;
};

template<typename D>
class HashClassCRC : public HashClass<D, hcCRC> {
public:
    HashClassCRC( void *pvoid = NULL ) {
        assert(pvoid == NULL); 
    }
    BOOL Equals(const D& d1, const D& d2) { 
        return memcmp( &d1, &d2, sizeof(D) ) == 0;
    }
};

template<typename D>
class HashClass<D, hcLCast> : public HashClassBase<D> {
public:
    HashClass( void *pvoid = NULL ) { // Just there so that this code compiles
        assert(pvoid == NULL); 
    }
    // Converts domain to hash
    inline LHASH __fastcall operator()(const D& d) {
        return LHASH( d & LHASH_MAX );
    }
};

template<typename D, int cbitsInsignificant>
class HashClass2<D, hcLPtr, cbitsInsignificant> : public HashClassBase<D> {
public:
    HashClass2( void *pvoid = NULL ) { // Just there so that this code compiles
        assert(pvoid == NULL); 
    }
    // Converts domain to hash
    inline LHASH __fastcall operator()(const D& d) {
        return LHASH( ((reinterpret_cast<UINT_PTR>(d) >> cbitsInsignificant) & LHASH_MAX) );
    }
};

#if defined(_WIN64)
#define cbitsTruncateHash 4
#else
#define cbitsTruncateHash 3
#endif

typedef HashClass<unsigned long, hcCast>    HcNi;
typedef HashClass<unsigned long, hcLCast>   LHcNi;
typedef HashClass<UINT_PTR, hcLCast>        HcPtr;
typedef HashClass2<void *, hcLPtr, cbitsTruncateHash>
                                            HcLPtr;

// fwd decl template enum class used as friend
template <class D, class R, class H, class C, class CS> class EnumMap;

const unsigned iNil = (unsigned)-1;

template <class D, class R, class H, class C=void, class CS = CriticalSectionNop>
class Map { // map from Domain type to Range type
public:
    Map(unsigned cdrInitial =1) :
        rgd(cdrInitial > 0 ? cdrInitial : 1),
        rgr(cdrInitial > 0 ? cdrInitial : 1),
        isetPresent(cdrInitial),
#ifdef PDB_MT
        cs( 0x1000 ),   // spin just enough time to allow for one add/delete
#endif
        pContext(NULL)

    {
        cdr = 0;
        traceOnly(cFinds = 0;)
        traceOnly(cProbes = 0;)

        postcondition(fullInvariants());
    }
    void setContext(C *pC) {
        MTS_PROTECT_T(CS, cs);
        pContext = pC;
    }
    ~Map() {
        traceOnly(if (cProbes>50) trace((trMap, _TEXT("~Map() cFinds=%d cProbes=%d cdr=%u rgd.size()=%d\n"),
                                         cFinds, cProbes, cdr, rgd.size()));)
    }
    void reset();
    void clear();
    BOOL map(D d, R* pr) const;
    BOOL map(D d, R** ppr) const;
    BOOL mapU(D d, R UNALIGNED * pr) const;
    BOOL contains(D d) const;
    BOOL add(D d, R r);
    BOOL add2(D d, const R & r);
    BOOL remove(D d);
    BOOL mapThenRemove(D d, R* pr);
    BOOL save(Buffer* pbuf);
#if 0
    BOOL reload(PB* ppb);   // REVIEW: move to protected or remove entirely.
#endif
    BOOL reload(PB* ppb, CB cbReloadBuf);
    void swap(Map& m);
    CB   cbSave() const;
    unsigned count() const;
protected:
    Array<D> rgd;
    Array<R> rgr;
    ISet isetPresent;
    ISet isetDeleted;
    unsigned cdr;
    C *pContext;
#ifdef PDB_MT
    mutable CS cs;
#endif
    traceOnly(unsigned cProbes;)
    traceOnly(unsigned cFinds;)

    BOOL setHashSize(unsigned size) {
        assert(size >= rgd.size());
        if (size < rgd.size()) {
            return FALSE;
        }
        return rgd.setSize(size) && rgr.setSize(size);
    }
    BOOL find(D d, unsigned *pi) const;
    BOOL grow();
    BOOL grow2();
    void shrink();
    BOOL fullInvariants() const;
    BOOL partialInvariants() const;
    unsigned cdrLoadMax() const {
        MTS_ASSERT(cs);
        // we do not permit the hash table load factor to exceed 67%
        return rgd.size() * 2/3 + 1;
    }
    Map(const Map&);
    friend class EnumMap<D,R,H,C,CS>;

    static const unsigned m_cElPresentMax = ULONG_MAX / (sizeof(D) + sizeof(R));

};

template <class D, class R, class H, class C, class CS> inline
void Map<D,R,H,C,CS>::reset() {

    MTS_PROTECT_T(CS, cs);
    
    cdr = 0;
    isetPresent.reset();
    isetDeleted.reset();
    rgd.setSize(1);
    rgr.setSize(1);
}

template <class D, class R, class H, class C, class CS> inline
void Map<D,R,H,C,CS>::clear() {

    MTS_PROTECT_T(CS, cs);
    
    cdr = 0;
    isetPresent.clear();
    isetDeleted.clear();
    rgd.clear();
    rgr.clear();
    reset();
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::map(D d, R* pr) const {
    precondition(pr);

    R * prT;
    if (map(d, &prT)) {
        *pr = *prT;
        return TRUE;
    }
    return FALSE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::mapU(D d, R UNALIGNED * pr) const {
    precondition(pr);

    R * prT;
    if (map(d, &prT)) {
        *pr = *prT;
        return TRUE;
    }
    return FALSE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::map(D d, R** ppr) const {

    MTS_PROTECT_T(CS, cs);

    precondition(partialInvariants());
    precondition(ppr);

    unsigned i;

    if (find(d, &i)) {
        *ppr = &rgr[i];
        return TRUE;
    }
    else
        return FALSE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::contains(D d) const {
    unsigned iDummy;
    MTS_PROTECT_T(CS, cs);
    return find(d, &iDummy);
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::add(D d, R r) {
    MTS_PROTECT_T(CS, cs);

    precondition(partialInvariants());

    unsigned i;

    if (find(d, &i)) {
        // some mapping d->r2 already exists, replace with d->r
#ifdef _DEBUG
        H hasher(pContext);
        assert(isetPresent.contains(i) && !isetDeleted.contains(i) && hasher.Equals(rgd[i], d));
#endif
        rgr[i] = r;
    } else if (i != iNil) {
        // establish a new mapping d->r in the first unused entry
        assert(!isetPresent.contains(i));
        isetDeleted.remove(i);
        isetPresent.add(i);
        rgd[i] = d;
        rgr[i] = r;
        grow();
    } else {
        // we run out of memory to grow
        return FALSE;
    }

    debug(R rCheck);
    postcondition(map(d, &rCheck) && r == rCheck);
    postcondition(fullInvariants());
    return TRUE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::add2(D d, const R & r) {
    MTS_PROTECT_T(CS, cs);

    precondition(partialInvariants());

    unsigned i;

    if (find(d, &i)) {
        // some mapping d->r2 already exists, replace with d->r
        assert(isetPresent.contains(i) && !isetDeleted.contains(i) && rgd[i] == d);
        rgr[i] = r;
    } else if (i != iNil) {
        // establish a new mapping d->r in the first unused entry
        assert(!isetPresent.contains(i));
        isetDeleted.remove(i);
        isetPresent.add(i);
        rgd[i] = d;
        rgr[i] = r;
        grow2();
    } else {
        return FALSE;
    }

    debug(R rCheck);
    postcondition(map(d, &rCheck) && r == rCheck);
    postcondition(fullInvariants());
    return TRUE;
}

template <class D, class R, class H, class C, class CS> inline
void Map<D,R,H,C,CS>::shrink() {
    MTS_ASSERT(cs);
    --cdr;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::remove(D d) {
    MTS_PROTECT_T(CS, cs);

    precondition(partialInvariants());

    unsigned i;

    if (find(d, &i)) {
        assert(isetPresent.contains(i) && !isetDeleted.contains(i));
        isetPresent.remove(i);
        isetDeleted.add(i);
        shrink();
    }

    postcondition(fullInvariants());
    return TRUE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::mapThenRemove(D d, R* pr) {
    precondition(partialInvariants());
    precondition(pr);

    unsigned i;

    MTS_PROTECT_T(CS, cs);

    if (find(d, &i)) {
        *pr = rgr[i];
        assert(isetPresent.contains(i) && !isetDeleted.contains(i));
        isetPresent.remove(i);
        isetDeleted.add(i);
        shrink();
        postcondition(fullInvariants());
        return TRUE;
    }
    else
        return FALSE;
}


template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::find(D d, unsigned *pi) const { 

    MTS_ASSERT(cs);

    precondition(partialInvariants());
    precondition(pi);

    traceOnly(++((Map<D,R,H,C,CS>*)this)->cFinds;) 

    H hasher(pContext);
    unsigned n      = rgd.size();
    unsigned h      = hasher(d) % n;
    unsigned i      = h;
    unsigned iEmpty = iNil;

    do {
        traceOnly(++((Map<D,R,H,C,CS>*)this)->cProbes;)

        assert(!(isetPresent.contains(i) && isetDeleted.contains(i)));
        if (isetPresent.contains(i)) {
            if (hasher.Equals(rgd[i],d)) {
                *pi = i;
                return TRUE;
            }
        } else {
            if (iEmpty == iNil)
                iEmpty = i;
            if (!isetDeleted.contains(i))
                break;
        }

        i = (i+1 < n) ? i+1 : 0;
    } while (i != h);

    // not found
    *pi = iEmpty;
    postcondition(*pi != iNil);
    postcondition(!isetPresent.contains(*pi));
    return FALSE;
}

// append a serialization of this map to the buffer
// format:
//  cdr
//  rgd.size()
//  isetPresent
//  isetDeleted
//  group of (D,R) pairs which were present, a total of cdr of 'em
//
template <class D, class R, class H, class C, class CS>
BOOL Map<D,R,H,C,CS>::save(Buffer* pbuf) {
    precondition(fullInvariants());
    
    MTS_PROTECT_T(CS, cs);

    unsigned size = rgd.size();
    if (!(pbuf->Append((PB)&cdr, sizeof(cdr)) &&
          pbuf->Append((PB)&size, sizeof(size)) &&
          isetPresent.save(pbuf) &&
          isetDeleted.save(pbuf)))
    {
        return FALSE;
    }

    for (unsigned i = 0; i < rgd.size(); i++) {
        if (isetPresent.contains(i))
        {
            if (!(pbuf->Append((PB)&rgd[i], sizeof(rgd[i])) &&
                  pbuf->Append((PB)&rgr[i], sizeof(rgr[i]))))
            {
                return FALSE;
            }
        }
    }
    return TRUE;
}
      
#if 0
// reload a serialization of this empty Map from the buffer; leave
// *ppb pointing just past the Map representation
template <class D, class R, class H, class C, class CS>
BOOL Map<D,R,H,C,CS>::reload(PB* ppb) {
    precondition(cdr == 0);

    MTS_PROTECT_T(CS, cs);

    cdr = *((unsigned UNALIGNED *&)*ppb)++;
    unsigned size = *((unsigned UNALIGNED *&)*ppb)++;

    if (!setHashSize(size))
        return FALSE;

    if (!(isetPresent.reload(ppb) && isetDeleted.reload(ppb)))
        return FALSE;

    for (unsigned i = 0; i < rgd.size(); i++) {
        if (isetPresent.contains(i)) {
            rgd[i] = *((D UNALIGNED *&)*ppb)++;
            rgr[i] = *((R UNALIGNED *&)*ppb)++;
        }
    }

    postcondition(fullInvariants());
    return TRUE;
}
#endif

// reload a serialization of this empty Map from the buffer; leave
// *ppb pointing just past the Map representation
template <class D, class R, class H, class C, class CS>
BOOL Map<D,R,H,C,CS>::reload(PB* ppb, CB cbReloadBuf) {

    precondition(cdr == 0);
    precondition(cbReloadBuf > 2 * sizeof(unsigned));

    MTS_PROTECT_T(CS, cs);

    if (cbReloadBuf <= 2 * sizeof(unsigned)) {
        return FALSE;
    }

    assume(cbReloadBuf >= 0);

    cdr = *((unsigned UNALIGNED *&)*ppb)++;
    unsigned size = *((unsigned UNALIGNED *&)*ppb)++;

    cbReloadBuf -= sizeof(unsigned) + sizeof(unsigned);

    PB  pbEnd = *ppb + cbReloadBuf;

    if (!setHashSize(size)) {
        return FALSE;
    }
    if (!isetPresent.reload(ppb, cbReloadBuf)) {
        return FALSE;
    }

    cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

    if (!isetDeleted.reload(ppb, cbReloadBuf)) {
        return FALSE;
    }
    cbReloadBuf = static_cast<CB>(pbEnd - *ppb);

    unsigned    cbitsPresent = isetPresent.cardinality();

    if (cbitsPresent > m_cElPresentMax || static_cast<unsigned>(cbReloadBuf) <  cbitsPresent * (sizeof(D) + sizeof(R))) {
        return FALSE;
    }

    for (unsigned i = 0; i < rgd.size(); i++) {
        if (isetPresent.contains(i)) {
            rgd[i] = *((D UNALIGNED *&)*ppb)++;
            rgr[i] = *((R UNALIGNED *&)*ppb)++;
        }
    }

    postcondition(fullInvariants());
    return TRUE;
}

template <class D, class R, class H, class C, class CS>
BOOL Map<D,R,H,C,CS>::fullInvariants() const {

    MTS_PROTECT_T(CS, cs);

    ISet isetInt;
    if (!partialInvariants())
        return FALSE;
    else if (cdr != isetPresent.cardinality())
        return FALSE;
    else if (!intersect(isetPresent, isetDeleted, isetInt))
        return FALSE;
    else if (isetInt.cardinality() != 0)
        return FALSE;
    else
        return TRUE;
}

template <class D, class R, class H, class C, class CS>
BOOL Map<D,R,H,C,CS>::partialInvariants() const {

    MTS_ASSERT(cs);

    if (rgd.size() == 0)
        return FALSE;
    else if (rgd.size() != rgr.size())
        return FALSE;
    else if (cdr > rgd.size())
        return FALSE;
    else if (cdr > 0 && cdr >= cdrLoadMax())
        return FALSE;
    else
        return TRUE;
}

// Swap contents with "map", a la Smalltalk-80 become.
template <class D, class R, class H, class C, class CS>
void Map<D,R,H,C,CS>::swap(Map<D,R,H,C,CS>& map) {

    MTS_PROTECT_T(CS, cs);
    MTS_PROTECT_T(CS, map.cs);

    isetPresent.swap(map.isetPresent);
    isetDeleted.swap(map.isetDeleted);
    rgd.swap(map.rgd);
    rgr.swap(map.rgr);
    ::__swap(cdr, map.cdr);
    traceOnly(::__swap(cProbes, map.cProbes));
    traceOnly(::__swap(cFinds,  map.cFinds));
}

// Return the size that would be written, right now, via save()
template <class D, class R, class H, class C, class CS> inline
CB Map<D,R,H,C,CS>::cbSave() const {

    MTS_PROTECT_T(CS, cs);

    assert(partialInvariants());
    return
        sizeof(cdr) +
        sizeof(unsigned) +
        isetPresent.cbSave() +
        isetDeleted.cbSave() +
        cdr * (sizeof(D) + sizeof(R))
        ;
}

// Return the count of elements
template <class D, class R, class H, class C, class CS> inline
unsigned Map<D,R,H,C,CS>::count() const {
    MTS_PROTECT_T(CS, cs);
    assert(partialInvariants());
    return cdr;
}


// EnumMap must continue to enumerate correctly in the presence
// of Map<foo>::remove() being called in the midst of the enumeration.
template <class D, class R, class H, class C=void, class CS=CriticalSectionNop>
class EnumMap : public Enum {
public:
    EnumMap(const Map<D,R,H,C,CS>& map) {
        pmap = &map;
        reset();
    }
    void release() {
        delete this;
    }
    void reset() {
        i = (unsigned)-1;
    }

    BOOL next() {
        MTS_PROTECT_T(CS, pmap->cs);
        while (++i < pmap->rgd.size())
            if (pmap->isetPresent.contains(i))
                return TRUE;
        return FALSE;
    }

    // make sure there is a critical section on the map while using the pr return from this funciton
    void get(OUT D* pd, OUT R* pr) {
        MTS_PROTECT_T(CS, pmap->cs);
        precondition(pd && pr);
        precondition(0 <= i && i < pmap->rgd.size());
        precondition(pmap->isetPresent.contains(i));

        *pd = pmap->rgd[i];
        *pr = pmap->rgr[i];
    }
    void get(OUT D* pd, OUT R** ppr) {
        MTS_PROTECT_T(CS, pmap->cs);
        precondition(pd && ppr);
        precondition(0 <= i && i < pmap->rgd.size());
        precondition(pmap->isetPresent.contains(i));

        *pd = pmap->rgd[i];
        *ppr = &pmap->rgr[i];
    }

private:
    const Map<D,R,H,C,CS>* pmap;
    unsigned i;
};

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::grow() {
    MTS_ASSERT(cs);
    if (++cdr >= cdrLoadMax()) {
        // Table is becoming too full.  Rehash.  Create a second map twice
        // as large as the first, propagate current map contents to new map,
        // then "become" (Smalltalk-80 style) the new map.
        //
        // The storage behind the original map is reclaimed on exit from this block.

        // this is protected by Array<T>'s limit            
        Map<D,R,H,C,CS> map;
        map.setContext(pContext);
        if (!map.setHashSize(2*cdrLoadMax()))
            return FALSE;

        EnumMap<D,R,H,C,CS> e(self);
        while (e.next()) {
            D d; R r;
            e.get(&d, &r);
            if (!map.add(d, r))
                return FALSE;
        }
        self.swap(map);
    }
    return TRUE;
}

template <class D, class R, class H, class C, class CS> inline
BOOL Map<D,R,H,C,CS>::grow2() {
    MTS_ASSERT(cs);
    if (++cdr >= cdrLoadMax()) {
        // Table is becoming too full.  Rehash.  Create a second map twice
        // as large as the first, propagate current map contents to new map,
        // then "become" (Smalltalk-80 style) the new map.
        //
        // The storage behind the original map is reclaimed on exit from this block.

        // this is proected by Array<T>'s limit
        Map<D,R,H,C,CS> map;
        map.setContext(pContext);
        if (!map.setHashSize(2*cdrLoadMax()))
            return FALSE;

        EnumMap<D,R,H,C,CS> e(self);
        while (e.next()) {
            D d; R * pr;
            e.get(&d, &pr);
            if (!map.add2(d, *pr))
                return FALSE;
        }
        self.swap(map);
    }
    return TRUE;
}

}   // namespace pdb_internal

#undef self

#endif // !__MAP_INCLUDED__
