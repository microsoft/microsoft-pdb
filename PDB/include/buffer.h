#pragma once
#ifndef __BUFFER_INCLUDED__
#define __BUFFER_INCLUDED__

// UNDONE: we shouldn't use pdbimpl.h here.  Include only the stuff need
#include "pdbimpl.h"

#include "crefobj.h"
#include "ref.h"

#include <stdlib.h>
#include "SysPage.h"

namespace pdb_internal {

class Buffer {
public:
    Buffer(void (*pfn)(void*, void*, void*) = 0, void* pfnArg = 0, bool fAllocPadding = true, bool fPadSubseq = true ) {
        cbAlloc = 0;
        pbStart = 0;
        pbEnd = 0;
        pfnMove = pfn;
        pfnMoveArg = pfnArg;
        m_fAllocPadding = fAllocPadding;
        // Pin m_fPadSubseq based on all padding first off--this allows us
        // to have good default behavior for everyone right now.  To enable
        // padfirstonly vs padsubsequentonly, tweak all callers of the ctor
        // and then set m_fPadSubseq = fPadSubSeq.
        m_fPadSubseq = fAllocPadding || fPadSubseq;
    }
    ~Buffer() {
        if (pbStart)
            Free();
    }
    BOOL SetInitAlloc(CB cbIn);
    BOOL Append(const PB pbIn, CB cbIn, OUT PB* ppbOut = 0);
    BOOL AppendFmt(SZ_CONST szFmt, ...);
    BOOL Reserve(CB cbIn, OUT PB* ppbOut = 0);
    BOOL ReserveNoZeroed(CB cbIn, OUT PB* ppbOut = 0);
    BOOL TrimToAllocatedSize();

    BOOL SetInitAllocAndReserve(CB cbIn, OUT PB* ppbOut = 0) {
        return SetInitAlloc(cbIn) && Reserve(cbIn, ppbOut);
    }
    PB Start() const {
        return pbStart;
    }
    PB End() const {
        return pbEnd;
    }
    CB Size() const {
        return CB(pbEnd - pbStart); // REVIEW:WIN64 CAST
    }
    void Reset() {
        pbEnd = pbStart;
    }
    BOOL ResetAndReserve(CB cbIn, OUT PB* ppb =0) {
        Reset();
        return Reserve(cbIn, ppb);
    }
    BOOL ResetAndReserveNoZeroed(CB cbIn, OUT PB* ppb =0) {
        Reset();
        return ReserveNoZeroed(cbIn, ppb);
    }
    BOOL Truncate(CB cb) {
        return 0 <= cb && cb <= Size() && setPbExtent(pbStart, pbStart + cb);
    }
    void Free() {
        if (pbStart) {
            delete [] pbStart;
        }
        setPbExtent(0, 0);
        cbAlloc = 0;
    }
    void Clear() {
        if (pbStart) {
            memset(pbStart, 0, CB(pbEnd - pbStart));    // REVIEW:WIN64 CAST
            setPbExtent(pbStart, pbStart);
        }
    }
    BOOL save(Buffer* pbuf) const {
        CB cb = Size();
        return pbuf->Append((PB)&cb, sizeof cb) &&
               (Size() == 0 || pbuf->Append(Start(), Size()));
    }

    BOOL reload(PB* ppb) {  // REVIEW: Move to protected or remove entirely.

        // can only reload in a pristine buffer
        if (Size() != 0 ) {
            return FALSE;
        }

        CB cb = *((CB*&)*ppb)++;

        if (Append(*ppb, cb)) {
            *ppb += cb;
            return TRUE;
        }
        return FALSE;
    }

    BOOL reload(PB* ppb, CB cbReloadBuf) {

        assert(cbReloadBuf >= sizeof(CB));

        // can only reload in a pristine buffer
        if (Size() != 0 || cbReloadBuf < sizeof(CB)) {
            return FALSE;
        }

        CB cb = *((CB*&)*ppb)++;
        cbReloadBuf -= sizeof(CB);

        if (cb <= cbReloadBuf && cb >= 0) {
            if (Append(*ppb, cb)) {
                *ppb += cb;
                return TRUE;
            }
        }
        return FALSE;
    }

    Buffer &operator =(Buffer& buf) {
        // Make a deep copy
        Free();
        Append(buf.Start(), buf.Size());
        return *this;
    }

    bool Contains(void * pv) {
        return PB(pv) >= Start() && PB(pv) < End();
    }

private:
    CB   cbRoundUp(CB cb, CB cbMult) { return (cb + cbMult-1) & ~(cbMult-1); }
    BOOL grow(CB dcbGrow);
    BOOL setPbExtent(PB pbStartNew, PB pbEndNew) {
        if (!pbStartNew) {
            pbStart = pbEnd = NULL;
            return FALSE;
        }
        PB pbStartOld = pbStart;
        pbStart = pbStartNew;
        pbEnd = pbEndNew;
        if (pbStartOld != pbStartNew && pfnMove)
            (*pfnMove)(pfnMoveArg, pbStartOld, pbStartNew);
        return TRUE;
    }

    PB  pbStart;
    PB  pbEnd;
    CB  cbAlloc;
    void (*pfnMove)(void* pArg, void* pOld, void* pNew);
    void* pfnMoveArg;
    bool    m_fAllocPadding;
    bool    m_fPadSubseq;

    static const size_t m_cbMaxAlloc = MAXLONG;

};

//
// SetInitAlloc
//
//  sets up the buffer for a large set of operations such that
//  it doesn't have to reallocate and copy data.  I.e. operations
//  such as Reserve and Append will logically work the same
//  after the SetInitAlloc but will never fail as long as they
//  don't go past the end of the pre-allocated buffer.  Also,
//  Size() will return the logical size as you would expect.
//
inline BOOL Buffer::SetInitAlloc(CB cbNew)
{
    dassert(cbNew > 0);

    if (pbStart)
        return FALSE;

    PB pbNew = new BYTE[cbNew];

    if (setPbExtent(pbNew, pbNew)) {
        cbAlloc = cbNew;
        return TRUE;
    }

    return FALSE;
}

inline BOOL Buffer::grow(CB dcbGrow)
{
    CB cbNew;

    assert(cbAlloc >= 0 && cbAlloc <= m_cbMaxAlloc);
    if (m_cbMaxAlloc - cbAlloc < (size_t)dcbGrow) {
        return FALSE;
    }
    if (m_fAllocPadding) {
        // Review:  Should we cap this at some reasonable value and grow at a
        // fixed rate instead of this?

        cbNew = cbAlloc / 2;
        cbNew = cbRoundUp(cbAlloc + __max(dcbGrow, cbNew), cbSysPage);
        cbNew = __max(cbAlloc + dcbGrow, cbNew);
    }
    else {
        cbNew = cbAlloc + dcbGrow;
    }

    PB pbNew = new BYTE[cbNew];

    if (pbNew) {
        cbAlloc = cbNew;
        CB cbUsed = CB(pbEnd - pbStart);    // REVIEW:WIN64 CAST
        memcpy(pbNew, pbStart, cbUsed);

        // ISSUE-BUGFIX-DEV10#847510 setPbExtent may use pbStart's pointer value 
        // to fixup the buffer. So save preserve pbStart's value and only actually delete
        // the memory associated with pbStart after the setPbExtent call.

        PB oldPbStart = pbStart;

        setPbExtent(pbNew, pbNew + cbUsed);
        delete [] oldPbStart;

        // Padding depends on subsequent padding flag after first allocation.

        m_fAllocPadding = m_fPadSubseq;

        return TRUE;
    }

    return FALSE;
}

inline BOOL Buffer::TrimToAllocatedSize()
{
    CB      cbUsed;
    BOOL    fRet = TRUE;

    if (m_fAllocPadding && (cbUsed = Size()) > 0 && cbAlloc > cbUsed) {
        PB  pbNew = new BYTE[cbUsed];

        if (pbNew != NULL) {
            cbAlloc = cbUsed;
            memcpy(pbNew, pbStart, cbUsed);
            setPbExtent(pbNew, pbNew + cbUsed);
        }
        else {
            fRet = FALSE;
        }
    }
    return fRet;
}

inline BOOL Buffer::Reserve(CB cbIn, OUT PB* ppbOut)
{
    dassert(cbIn >= 0);
    dassert(pbEnd >= pbStart);
    dassert(cbAlloc >= Size());

    if (((cbIn < 0) || (cbIn > cbAlloc - Size())) && !grow(cbIn))
        return FALSE;

    if (ppbOut)
        *ppbOut = pbEnd;

    memset(pbEnd, 0, cbIn);
    PREFAST_SUPPRESS(22004, "We just grew the buffer to be cbIn bytes past pbEnd, so pbEnd+cbIn is not past the end");
    setPbExtent(pbStart, pbEnd + cbIn);
    return TRUE;
}

inline BOOL Buffer::ReserveNoZeroed(CB cbIn, OUT PB* ppbOut)
{
    dassert(cbIn >= 0);
    dassert(pbEnd >= pbStart);
    dassert(cbAlloc >= Size());

    if ((cbIn < 0) || (cbIn > cbAlloc - Size() && !grow(cbIn)))
        return FALSE;

    if (ppbOut)
        *ppbOut = pbEnd;

    PREFAST_SUPPRESS(22004, "We just grew the buffer to be cbIn bytes past pbEnd, so pbEnd+cbIn is not past the end");
    setPbExtent(pbStart, pbEnd + cbIn);
    return TRUE;
}

inline BOOL Buffer::Append(const PB pbIn, CB cbIn, OUT PB* ppbOut)
{
    if (!pbIn)
        return FALSE;

    PB pb;
    if (!ReserveNoZeroed(cbIn, &pb))
        return FALSE;

    if (ppbOut)
        *ppbOut = pb;

    memcpy(pb, pbIn, cbIn);
    return TRUE;
}

inline BOOL Buffer::AppendFmt(SZ_CONST szFmt, ...)
{
    va_list args;
    va_start(args, szFmt);

	for (;;) {
		switch (*szFmt++) {
		case 0:
			va_end(args);
			return TRUE;
		case 'b': {
			BYTE b = va_arg(args, BYTE);
			if (!Append(&b, sizeof b, 0))
				goto fail;
			break;
		}
		case 's': {
			USHORT us = va_arg(args, USHORT);
			if (!Append((PB)&us, sizeof us, 0))
				goto fail;
			break;
		}
		case 'l': {
			ULONG ul = va_arg(args, ULONG);
			if (!Append((PB)&ul, sizeof ul, 0))
				goto fail;
			break;
		}
		case 'f': {
			static const BYTE zeroes[7] = { 0, 0, 0, 0, 0, 0, 0 };
			int cb = va_arg(args, int);
			assert(cb <= sizeof(zeroes));
			if (cb != 0 && !Append((PB)zeroes, cb, 0))
				goto fail;
			break;
		}
		case 'z': {
			SZ sz = va_arg(args, SZ);
			CB cb = CB(strlen(sz));
			if (!Append((PB)sz, cb, 0))
				goto fail;
			break;
		}
		case 'w': {     // Wide char string
			wchar_t *wcs = va_arg(args, wchar_t *);
			CB cb = CB(wcslen(wcs) + 1) * sizeof(wchar_t);
			if (!Append((PB)wcs, cb, 0))
				goto fail;
			break;
		}
		default:
			assert(0);
			break;
		}
	}

fail:
    va_end(args);
    return FALSE;
}


// This class basically allows a Buffer object to use virtual memory
//
// If fUseVirtualMem is TRUE, this buffer will use virtual memory.
// If fUseVirtualMem is FALSE, then the base class Buffer functions will be used.
class VirtualBuffer : public Buffer
{
public:
    VirtualBuffer(bool fUseVirtualMem_, void (*pfn)(void*, void*, void*) = 0, void* pfnArg = 0)
        : Buffer(pfn, pfnArg)
    {
        fUseVirtualMem = fUseVirtualMem_;
        pbStart = NULL;
        cb = 0;
    }

    ~VirtualBuffer()
    {
        if (fUseVirtualMem && pbStart)
        {
            verify(::VirtualFree(pbStart, 0, MEM_RELEASE));
            pbStart = NULL;
            cb = 0;
        }
    }

    PB Start() const;
    CB Size() const;
    BOOL Reserve(CB cbIn, OUT PB* ppbOut = 0);
    BOOL ReserveNoZeroed(CB cbIn, OUT PB* ppbOut = 0);
    BOOL Commit(PB pbIn, CB cbIn);
    BOOL Append(PB pbIn, CB cbIn, OUT PB* ppbOut = 0);
    BOOL Free();
    VirtualBuffer& operator =(VirtualBuffer& buf);
    bool Contains(void * pv);
    BOOL TrimToAllocatedSize();

private:
    PB pbStart;
    CB cb;
    CB cbVirtSize;
    bool fUseVirtualMem;
};


inline PB VirtualBuffer::Start() const
{
    if (!fUseVirtualMem)
    {
        return Buffer::Start();
    }

    return pbStart;
}

inline CB VirtualBuffer::Size() const
{
    if (!fUseVirtualMem)
    {
        return Buffer::Size();
    }

    return cb;
}

inline BOOL VirtualBuffer::Reserve(CB cbIn, OUT PB* ppbOut)
{
    if (!fUseVirtualMem)
    {
        return Buffer::Reserve(cbIn, ppbOut);
    }

    // We currently only support one call to reserve...
    assert(cb == 0 && pbStart == NULL);

    pbStart = (PB) VirtualAlloc(NULL, cbIn, MEM_RESERVE, PAGE_READWRITE);
    if (pbStart)
    {
        cb = cbIn;
        cbVirtSize = ((cb + cbSysPage - 1) / cbSysPage) * cbSysPage;
        if (ppbOut) {
            *ppbOut = pbStart;
        }
        return TRUE;
    }

    return FALSE;
}

inline BOOL VirtualBuffer::ReserveNoZeroed(CB cbIn, OUT PB* ppbOut)
{
    if (!fUseVirtualMem)
    {
        return Buffer::ReserveNoZeroed(cbIn, ppbOut);
    }

    return Reserve(cbIn, ppbOut);
}

inline BOOL VirtualBuffer::Commit(PB pbIn, CB cbIn)
{
    if (fUseVirtualMem)
    {
        assert(Contains(pbIn));
        assert((cbIn ? Contains(pbIn + cbIn - 1) : true));
        return !!VirtualAlloc(pbIn, cbIn, MEM_COMMIT, PAGE_READWRITE);
    }
    return TRUE;
}

inline BOOL VirtualBuffer::Append(PB pbIn, CB cbIn, OUT PB* ppbOut)
{
    assert(!fUseVirtualMem);
    return Buffer::Append(pbIn, cbIn, ppbOut);
}

inline BOOL VirtualBuffer::Free()
{
    if (fUseVirtualMem) {
        cb = 0;
        PB pbT = pbStart;
        pbStart = NULL;
        if (pbT) 
            return VirtualFree(pbT, 0, MEM_RELEASE);
        return TRUE;
    }
    else {
        Buffer::Free();
        return TRUE;
    }
}

inline VirtualBuffer& VirtualBuffer::operator =(VirtualBuffer& buf)
{
    if (fUseVirtualMem) {
        Free();

        verify(Reserve(buf.Size()));
        verify(Commit(pbStart, cb));

        memmove(Start(), buf.Start(), buf.Size());
    }
    *(Buffer*)this = *(Buffer*)&buf;
    return *this;

}

inline bool VirtualBuffer::Contains(void * pv)
{
    if (fUseVirtualMem) {
        return pv >= pbStart && pv < pbStart + cbVirtSize;
    }
    return Buffer::Contains(pv);
}

inline BOOL VirtualBuffer::TrimToAllocatedSize()
{
    if (!fUseVirtualMem) {
        return Buffer::TrimToAllocatedSize();
    }
    
    // We don't support trimming a virtual buffer
    return FALSE;
}

typedef RefCount<Buffer> RefCountedBuffer;
typedef RefPtr<RefCountedBuffer> RefBuf;

template <typename TYPE, size_t cT, size_t cTMax>
class   TBuffer {
    Buffer          m_bufOversize;
    bool            m_fOversized;
    TYPE            m_rgT[cT];

    TYPE *
    pt() {
        if (m_fOversized) {
            assert(m_bufOversize.Size() != 0);
            assert(m_bufOversize.Start() != NULL);
            return reinterpret_cast<TYPE*>(m_bufOversize.Start());
        }
        else {
            return m_rgT;
        }
    }

public:
    operator TYPE * () {
        return pt();
    }

    TYPE & operator[](size_t it) const {
        assert(it < cT || m_fOversized && it < m_bufOversize.Size() / sizeof(TYPE));
        return *(pt() + it);
    }

    bool
    Alloc(size_t cel) {
        if (cel > cT) {
            if (cel < cTMax) {
                return (m_fOversized = (0 != m_bufOversize.ResetAndReserve(static_cast<CB>(cel * sizeof(TYPE)))));
            }
            else {
                return false;
            }
        }
        m_fOversized = false;
        debug(Clear());
        return true;
    }

    void
    Clear() {
        m_fOversized = false;
        memset(m_rgT, 0, sizeof(m_rgT));
        m_bufOversize.Clear();
    }

    size_t
    CbAllocated() const {
        if (m_fOversized) {
            return m_rgT.Size();
        }
        else {
            return sizeof(m_rgT);
        }
    }


};

}   // namespace pdb_internal

#endif // !__BUFFER_INCLUDED__
