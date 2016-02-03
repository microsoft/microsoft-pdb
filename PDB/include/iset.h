#ifndef __ISET_INCLUDED__
#define __ISET_INCLUDED__

#ifndef __ARRAY_INCLUDED__
#include "array.h"
#endif

#ifndef __TWO_INCLUDED__
#include "two.h"
#endif

namespace pdb_internal {

class ISet {    // dense set of small integers, implemented using a bit array
public:
    ISet() {
    }
    ISet(unsigned size)
        : rgw(size ? words(size - 1) : 0)
    {
        rgw.fill(0);
    }
    ~ISet() {
    }
    void reset() {
        rgw.reset();
    }
    BOOL __fastcall contains(unsigned i) const {
        return (i < size()) && (word(i) & bitmask(i));
    }
    BOOL __fastcall add(unsigned i) {
        if (!ensureRoomFor(i))
            return FALSE;
        word(i) |= bitmask(i);
        return TRUE;
    }
    unsigned __fastcall size() const {
        return rgw.size() << lgBPW;
    }
    BOOL __fastcall remove(unsigned i) {
        if (i < size())
            word(i) &= ~bitmask(i);
        return TRUE;
    }
    unsigned cardinality() const {
        unsigned n = 0;
        for (unsigned i = 0; i < rgw.size(); i++)
            n += bitcount(rgw[i]);
        return n;
    }
    friend BOOL intersect(const ISet& s1, const ISet& s2, ISet& iset) {
        iset.reset();
        if (iset.rgw.setSize(min(s1.rgw.size(), s2.rgw.size()))) {
            for (unsigned i = 0; i < iset.rgw.size(); i++)
                iset.rgw[i] = s1.rgw[i] & s2.rgw[i];
            return TRUE;
        }
        else {
            return FALSE;
        }
    }
    BOOL save(Buffer* pbuf) {
        return rgw.save(pbuf);
    }
#if 0
    BOOL reload(PB* ppb) {
        return rgw.reload(ppb);
    }
#endif
    BOOL reload(PB* ppb, CB cbReloadBuf) {
        return rgw.reload(ppb, cbReloadBuf);
    }
    void swap(ISet& is) {
        rgw.swap(is.rgw);
    }
    CB cbSave() const {
        return rgw.cbSave();
    }

    void clear() {
        rgw.clear();
    }

private:

#if 0   // defined(_M_IA64)
    // we cannot do this because of the serialization/reload semantics
    // in Array<>.  It saves/restores the number of base elements at the head
    // of the buffer and that has to be consistent across 32- and 64-bit
    // platforms for shareability of the PDB files.
    //
    typedef unsigned __int64    native_word;
    enum { BPW = 64, lgBPW = 6 };
#else
    typedef ulong native_word;
    enum { BPW = 32, lgBPW = 5 };
#endif
    Array<native_word>  rgw;


    ISet(const ISet&);
    unsigned __fastcall index(unsigned i) const {
        return i >> lgBPW;
    }
    unsigned __fastcall words(unsigned i) const {
        return index(i) + 1;
    }
    unsigned __fastcall bit(unsigned i) const {
        return i & (BPW-1);
    }
    native_word & __fastcall word(unsigned i) {
        return rgw[index(i)];
    }
    const native_word & __fastcall word(unsigned i) const {
        return rgw[index(i)];
    }
    native_word bitmask(unsigned i) const {
        return 1 << bit(i);
    }
    BOOL ensureRoomFor(unsigned i) {
        const native_word zero = 0;
        while (words(i) > (unsigned)rgw.size())
            if (!rgw.append(zero))
                return FALSE;
        return TRUE;
    }
};

}   // namespace pdb_internal

#endif // !__ISET_INCLUDED__
