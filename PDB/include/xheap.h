// xheap.h -- exogenous heap manager -- manages a heap in some other medium
//
// Most commonly used to manage a heap in a stream on disk,
// without itself requiring disk I/O.

#pragma once
#ifndef __XHEAP_INCLUDED__
#define __XHEAP_INCLUDED__

#ifndef __ARRAY_INCLUDED__
#include "array.h"
#endif

class XHeap {
	struct Regn {
		OFF off;
		unsigned size;
		Regn() {
			off = 0;
			size = 0;
		}
		Regn(OFF off_, unsigned size_) {
			off = off_;
			size = size_;
		}
		OFF end() {
			return off + size;
		}
		static BOOL hasSize(Regn* pregn, void* pvSize) {    
			return pregn->size >= *(unsigned*)pvSize;
		}
		static BOOL isOffLE(Regn* pregn, void* pvRegn) {
			return ((Regn*)pvRegn)->off <= pregn->off;
		}
	};
	Array<Regn> rgregnFree;
	unsigned iregnRover;
	OFF offMac;
public:
	XHeap() {
		iregnRover = 0;
		offMac = 0;
	}
 	void reset() {
		rgregnFree.reset();
		iregnRover = 0;
		offMac = 0;

	}
	OFF alloc(unsigned size) {
		precondition(invariants());
		precondition(size > 0);

		OFF off;

#ifdef _MIPS_
		size = cbAlign(size);
#endif
		if (rgregnFree.findFirstEltSuchThat_Rover(&Regn::hasSize, &size, &iregnRover)) {
			Regn* pregn = &rgregnFree[iregnRover];
			assert(pregn->size >= size);
			off = pregn->off;
			pregn->off += size;
			if ((pregn->size -= size) == 0)
				rgregnFree.deleteAt(iregnRover);
		}
		else {
			// No suitable free block found.  Return storage at the end of the stream.
			off = offMac;
			offMac += size;
		}

		postcondition(invariants());
		return off;
	}

	BOOL free(OFF off, unsigned size) {
		precondition(invariants());
		precondition(off >= 0 && size > 0);
			
#ifdef _MIPS_
		size = cbAlign(size);
#endif
		Regn regn(off, size);		
		unsigned iregn = rgregnFree.binarySearch(&Regn::isOffLE, &regn);

		Regn* pregnPrev = (iregn > 0) ? &rgregnFree[iregn-1] : 0;
		Regn* pregnNext = (iregn < rgregnFree.size()) ? &rgregnFree[iregn] : 0;
		BOOL  fAdjPrev  = (pregnPrev && pregnPrev->end() == regn.off); 
		BOOL  fAdjNext  = (pregnNext && regn.end() == pregnNext->off);

		if (fAdjPrev) {
			if (fAdjNext) {
				// merge all three blocks into previous block
				pregnPrev->size += regn.size + pregnNext->size;
				rgregnFree.deleteAt(iregn);
			} else {
				// merge this block into previous
				pregnPrev->size += regn.size;
			}
		} else {
			if (fAdjNext) {
				// merge this block into next
				pregnNext->off = regn.off;
				pregnNext->size += regn.size;
			} else {
				// insert a new block here
				if (!rgregnFree.insertAt(iregn, regn))
					return FALSE;
			}
		}

		// Restart the rover index prior to this free block.
		if (iregn < iregnRover)
			iregnRover = (iregn > 0) ? iregn - 1 : 0;

		postcondition(invariants());
		return TRUE;
	}

	BOOL save(Buffer* pbuf) {
		precondition(invariants());

		return pbuf->Append((PB)&iregnRover, sizeof(iregnRover)) &&
			   pbuf->Append((PB)&offMac, sizeof(offMac)) &&
			   rgregnFree.save(pbuf);
	}
	BOOL reload(PB* ppb) {
		iregnRover = *((unsigned*&)*ppb)++;
		offMac     = *((OFF*&)*ppb)++;
		BOOL OK    = rgregnFree.reload(ppb);

		postcondition(invariants());
		return OK;
	}

	BOOL invariants() {
		unsigned size = rgregnFree.size();

#ifdef _MIPS_
		// all free regions should be in aligned-sized chunks
		for (unsigned i = 0; i < size; i++)
			if (rgregnFree[i].size != (ulong)cbAlign(rgregnFree[i].size))
				return FALSE;


		// regions should be in sorted order
		for (i = 1; i < size; i++)
#else
 		for (unsigned i = 1; i < size; i++)
#endif
			if (rgregnFree[i-1].off >= rgregnFree[i].off ||
				rgregnFree[i-1].end() >= rgregnFree[i].off)
				return FALSE;


		// the last region should preceed offMac
		if (size > 0 && rgregnFree[size-1].end() > offMac)
			return FALSE;

		return TRUE;
	}
#ifdef _DEBUG
	friend class EnumXHeap;
#endif
};

#ifdef _DEBUG
class EnumXHeap : public Enum {  // Enumerates the used regions of an XHeap
private:
	unsigned    current;
    unsigned    size;
    unsigned    rgnSize;
	OFF         rgnOff;
    OFF         nextOff;
	XHeap *     xheap;

public:
	EnumXHeap( XHeap *xheap_ ) { 
		current = unsigned(-1);
        rgnOff = 0;
        nextOff = 0;
		rgnSize = 0;
		xheap = xheap_;
		size = xheap->rgregnFree.size();
    }
	
	void release() { 
        delete this;
    }
	
	void reset() {
        current = unsigned(-1);
        rgnOff = 0;
        nextOff = 0;
    }

	BOOL next() {
		++current; 
		if (current > size)
            return FALSE;
        if ( current == 0 && size > 0 && xheap->rgregnFree[0].off == 0)  {
            // First block is free
			nextOff = xheap->rgregnFree[0].size;
			current++;
		}
		rgnOff = nextOff; // Advance our pointer
		if (current < size) {
			nextOff = xheap->rgregnFree[current].off + 
						 xheap->rgregnFree[current].size;
			rgnSize = xheap->rgregnFree[current].off - rgnOff;
			assert(rgnOff + rgnSize == (unsigned)xheap->rgregnFree[current].off );
		}
		else  // last blob of data (can be empty)
			rgnSize = xheap->offMac - rgnOff;
		assert( !current || (current && rgnSize >= 0) );
		return rgnSize > 0; // Returns false when the region is empty
	}

	void get( OUT OFF *poff, OUT unsigned *psize ) {
		if (poff)
            *poff = rgnOff;
		if (psize) 
            *psize = rgnSize;
	}
};

#endif

#endif // !__XHEAP_INCLUDED__
