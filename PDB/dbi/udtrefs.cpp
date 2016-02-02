//////////////////////////////////////////////////////////////////////////////
// UDTRefs bit maps

#include "pdbimpl.h"
#include "dbiimpl.h"
#include <stdio.h>


UDTRefs::UDTRefs(unsigned int cti_, bool fGrowRefSets_) : 
    cti(cti_), ucur(0), fGrowRefSets(fGrowRefSets_)
{
}

UDTRefs::~UDTRefs()
{
}

unsigned int UDTRefs::normalize(TI ti)
{
	assert(!CV_IS_PRIMITIVE(ti));
	unsigned int retval  = ti - CV_FIRST_NONPRIM;
	assert(retval <= cti);
	return retval;
}

TI UDTRefs::denormalize(unsigned int u)
{
	return u + CV_FIRST_NONPRIM;
}

BOOL UDTRefs::fNoteRef(TI ti)
{
    if (fGrowRefSets && ti - CV_FIRST_NONPRIM > cti ) {
        cti = ti - CV_FIRST_NONPRIM;
    }
	return isetRefs.add(normalize(ti));
}

BOOL UDTRefs::tiNext(TI *pti)
{
    if (cti) {
        unsigned int u = ucur; 
        do {
            if (isetRefs.contains(u) && !isetProcessed.contains(u)) {
                *pti = denormalize(u);
                ucur = (u + 1) % cti;
                return isetProcessed.add(u);
            }
            u = (u + 1) % cti;
        } while (u != ucur);
    }

    *pti = tiNil;
    return TRUE;
}



