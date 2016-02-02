#include "pdbimpl.h"
#include "dbiimpl.h"
#include "srcimpl.h"


BOOL    PDB1::OpenSrc(Src ** ppsrc) {
    MTS_PROTECT(m_csForSrcImpl);
#ifdef PDB_MT
    if (psrcimpl) {
        if (!m_fWriteShared && !m_fRead) {
            // can't open multiple time if we have write-exclusive
            setAccessError();
            return FALSE;
        }
        assert(dwSrcImplOpenedCount);
        dwSrcImplOpenedCount++;
        *ppsrc = psrcimpl;
        return TRUE;
    }
    SrcImpl *   psrc = new SrcImpl(this, (&s_internal_CloseSrc));
#else
    SrcImpl *   psrc = new SrcImpl(this);
#endif
      
    if (psrc) {
        if (psrc->internalInit(!m_fRead)) {
            *ppsrc = psrc;
#ifdef PDB_MT
            assert(dwSrcImplOpenedCount == 0);
            dwSrcImplOpenedCount = 1;
            psrcimpl = psrc;
#endif
            return TRUE;
        }
        delete psrc;
    }
    return FALSE;
}

#ifdef PDB_MT
BOOL 
PDB1::internal_CloseSrc() {
    MTS_PROTECT(m_csForSrcImpl);

    assert(dwSrcImplOpenedCount);
    assert(psrcimpl);

    if (--dwSrcImplOpenedCount == 0) {
        psrcimpl = NULL;
        return TRUE;
    }
    return FALSE;
}
#endif

void
PDB1::CopySrc( PDB* ppdbFrom )
{
#ifdef PDB_MT
#pragma message(MTS_MESSAGE("Not thread safe - PDB1::CopySrc()"))
#endif

    // we need to transfer any source file info from the from pdb to the to pdb
    //
    Src *       psrcSrc = NULL;
    Src *       psrcDst = NULL;
    NameMap *   pnmp = NULL;

    // get setup to do the work by getting the two Src interfaces
    // and the namemap to get the names from the "from" pdb
    if (ppdbFrom->OpenSrc(&psrcSrc) &&
        OpenSrc(&psrcDst) &&
        NameMap::open(ppdbFrom, false, &pnmp))
    {
        EnumSrc *   pe = NULL;
        if (psrcSrc->GetEnum(&pe)) {
            
            // enumerate all of the "from" source files
            //
            while (pe->next()) {
                PCSrcHeaderOut pcsh;
                SZ_CONST        sz;

                pe->get(&pcsh);
                if (pnmp->isValidNi(pcsh->niVirt) &&
                    pnmp->getName(pcsh->niVirt, &sz))
                {
                    // check to see if the file already exists
                    // in the "to" side, or if the signatures don't match
                    //
                    SrcHeaderOut    sho;
                    if (!psrcDst->QueryByName(sz, &sho) ||
                        sho.sig != pcsh->sig)
                    {
                        Buffer  buf;
                        if (buf.SetInitAlloc(pcsh->cbSource) &&
                            psrcSrc->GetData(pcsh, PV(buf.Start())))
                        {
                            // Build up the input SrcHeader based on
                            // the info we retrieved from the "to" side,
                            // which resides in the pcsh
                            //
                            BYTE        rgb[ sizeof(SrcHeader) + 4 * _MAX_PATH];
                            PSrcHeader  psh = PSrcHeader(rgb);
                            psh->ver = pcsh->ver;
                            psh->sig = pcsh->sig;
                            psh->cbSource = pcsh->cbSource;
                            psh->srccompress = pcsh->srccompress;
                            psh->grFlags = pcsh->grFlags;
                            PB  pb = psh->szNames;
                            
                            // store the names and bail out if any are
                            // invalid or cannot be retrieved.
                            do {
                                if (pnmp->isValidNi(pcsh->niFile) &&
                                    pnmp->getName(pcsh->niFile, &sz))
                                {
                                    size_t len = strlen(sz);
                                    strncpy_s(SZ(pb), _MAX_PATH, sz, _TRUNCATE);
                                    pb += strlen(SZ(pb)) + 1;
                                }
                                else
                                {
                                    break;
                                }

                                if (pnmp->isValidNi(pcsh->niObj) &&
                                    pnmp->getName(pcsh->niObj, &sz))
                                {
                                    size_t len = strlen(sz);
                                    strncpy_s(SZ(pb), _MAX_PATH, sz, _TRUNCATE);
                                    pb += strlen(SZ(pb)) + 1;
                                }
                                else
                                {
                                    break;
                                }

                                if (pnmp->isValidNi(pcsh->niVirt) &&
                                    pnmp->getName(pcsh->niVirt, &sz))
                                {
                                    size_t len = strlen(sz);
                                    strncpy_s(SZ(pb), _MAX_PATH * 2, sz, _TRUNCATE);
                                    pb += strlen(SZ(pb)) + 1;
                                }
                                else
                                {
                                    break;
                                }

                                // finally, set the size based on what we store
                                // and write it to the "to" side
                                //
                                psh->cb = (unsigned long)(pb - PB(psh));
                                psrcDst->Add(psh, PV(buf.Start()));

                            // We want to do one iteration only and use the
                            // do/while to be able to easily skip.
                            } while (false);
                        }
                    }
                }
            }
        }
        
        if (pe) {
            pe->release();
        }
    }

    if (psrcSrc)
        psrcSrc->Close();
    if (psrcDst)
        psrcDst->Close();
    if (pnmp)
        pnmp->close();
}
