//////////////////////////////////////////////////////////////////////////////
// MLI: Mod linenumber information API implementation
//
// Contributed by Amit Mital of VCE, 8/3/93, subsequently C++ized and
// modified to use DBI buffers and heaps.  (Actually, Jan apologizes for
// largely rewriting, but it seemed the best way to learn the code.)

#include "pdbimpl.h"
#ifndef __NO_MOD__
#include "dbiimpl.h"
#else
#include "mli.h"
#include "misc.h"
#endif
#include "output.h"

#include "poolcpy.h"

inline int adjustLineNumber(LINE32 lineStart, DWORD line) 
{
    return (line == 0x7fff) ? lineStart : (lineStart + line);
}

BOOL MLI::AddLines(SZ_CONST szSrc, ISECT isect, OFF offCon, CB cbCon,   OFF doff,
                   LINE32 lineStart, IMAGE_LINENUMBER *plnumCoff, CB cb)

//  szSrc      the source file name
//  isect       section index
//  offMin     starting offset
//  offMax     ending offset
//  lineStart  starting line number
//  plnumCoff  pointer to linenumber coff info ??
//  cb         byte count

{
    assert(cb % sizeof(IMAGE_LINENUMBER) == 0);
    int clnum = cb / sizeof(IMAGE_LINENUMBER);

    pSrcFile pSrcFile = AddSrcFile(szSrc);
    if (!pSrcFile)
        return FALSE;

    OFF offMac = offCon + cbCon;    // byte count comes in, then we use last included offset internally
    // If this contribution does not start a function (linenumber != 0),
    // offCon is passed in the first lnum entry.
    if (plnumCoff[0].Linenumber)
        offCon = plnumCoff[0].Type.VirtualAddress + doff;

    pSectInfo pSectInfo = pSrcFile->AddSectInfo(isect, offCon, offMac, pool);
    if (!pSectInfo)
        return FALSE;
    if (!pSectInfo->AddLineNumbers(adjustLineNumber(lineStart, plnumCoff[0].Linenumber), offCon, pool))
        return FALSE;
    for (IMAGE_LINENUMBER* pln = &plnumCoff[1]; pln < &plnumCoff[clnum]; pln++)
        if (!pSectInfo->AddLineNumbers(adjustLineNumber(lineStart, pln->Linenumber), pln->Type.VirtualAddress + doff, pool))
            return FALSE;

    return TRUE;
}

#ifdef PDB_DEFUNCT_V8
bool MLI::FindSecByAddr( ISECT isect, OFF off, OFF* poffMax )
{
    //
    // on true: returns end of section that contains this address
    // on false: returns address of first section after this address
    //
    for (pSrcFile* ppFile = &pSrcFiles; *ppFile; ppFile = &(*ppFile)->pNext) {
        if ( (*ppFile)->FindSecByAddr( isect, off, poffMax ) ) {
            return true;
        }
    }
    //
    // Cannot find this address.
    //
    return false;
}

bool MLI::Merge( MLI* pmliNew, FileNameMapCmd* pfnMapCmd )
{
    for (pSrcFile* ppFile = &pSrcFiles; *ppFile; ppFile = &(*ppFile)->pNext) {
        if ( !(*ppFile)->MergeSrc( pmliNew, pfnMapCmd ) )
            return false;
    }
    return true;
}
#endif

// Find a pointer to the source file linked list structure - add it if it isn't there
pSrcFile MLI::AddSrcFile(SZ_CONST szFile)
{
    pSrcFile *  ppFile;
    for (ppFile = &pSrcFiles; *ppFile; ppFile = &(*ppFile)->pNext)
        if (strcmp((*ppFile)->szFile, szFile) == 0)
            return *ppFile;

    SZ szName = szCopyPool(szFile, pool);
    if (!szName || !(*ppFile = new (pool) SrcFile(szName)))
        return 0;

    cfiles++;
    return *ppFile;
}


// Find the section structure within the source file structure - add it if it is not there
pSectInfo SrcFile::AddSectInfo(ISECT isect, OFF offMin, OFF offMax, POOL_AlignNative& pool)
{
    pSectInfo * ppSI;
    for (ppSI = &pSectInfos; *ppSI; ppSI = &(*ppSI)->pNext) {
        if ((*ppSI)->isect == isect && (*ppSI)->offMax + 1 == offMin) {  // see if the new start is close enough to the old end
           (*ppSI)->offMax = offMax;
           return *ppSI;
        }
    }
    if (!(*ppSI = new (pool) SectInfo(isect, offMin, offMax)))
        return 0;

    csect++;
    return *ppSI;
}

#ifdef PDB_DEFUNCT_V8

bool SrcFile::FindSecByAddr( ISECT isect, OFF off, OFF* poffMax )
{
    for (pSectInfo* ppSI = &pSectInfos; *ppSI; ppSI = &(*ppSI)->pNext) {
        if ((*ppSI)->isect == isect && (*ppSI)->offMin <= off && (*ppSI)->offMax >= off) {  // see if the new start is close enough to the old end
           if ( (*ppSI)->offMax < *poffMax )
               *poffMax = (*ppSI)->offMax;
           return true;
        } else if ( (*ppSI)->offMin > off && (*ppSI)->offMin < *poffMax ) {
            *poffMax = (*ppSI)->offMin;
        }
    }
    return false;
}

bool SrcFile::MergeSrc( MLI* pmliNew, FileNameMapCmd* pfnMapCmd )
{
    for (pSectInfo* ppSI = &pSectInfos; *ppSI; ppSI = &(*ppSI)->pNext) {
        OFF offMin = (*ppSI)->offMin;
        while ( offMin <= (*ppSI)->offMax ) {
            //
            // At this point (*ppSI)->offMin thru offMin have been handled
            //
            OFF offExtent = (*ppSI)->offMax;
            if ( !pmliNew->FindSecByAddr( (*ppSI)->isect, offMin, &offExtent ) ) {
                assert( offExtent <= (*ppSI)->offMax );
                //
                // address not covered, process the affected lines
                //
                pSrcFile pSrcFile = pmliNew->AddSrcFile( pfnMapCmd->map( szFile ) ) ;
                if ( pSrcFile == 0 ) {
                    return false;
                }
                pSectInfo pSectInfo = pSrcFile->AddSectInfo( (*ppSI)->isect, offMin, offExtent, pmliNew->pool );
                if ( pSectInfo ==0 ) {
                    return false;
                }
                if ( !(*ppSI)->MergeLines( pSectInfo, offMin, offExtent, pmliNew->pool ) ) {
                    return false;
                }
            }
            offMin = offExtent+1;
        }
    }
    return true;
}
#endif

// Add linenumber information to the record
BOOL SectInfo::AddLineNumbers(int linenumber, int offset, POOL_AlignNative& pool)
{
    if (!(*ppTail = new (pool) Lines(offset, linenumber)))
        return FALSE;
    ppTail = &(*ppTail)->pNext;
    cPair++;
    return TRUE;
}

#ifdef PDB_DEFUNCT_V8
bool SectInfo::MergeLines( pSectInfo pSectInfo, OFF offMin, OFF offMax, POOL_AlignNative& pool )
{
    for (pLines plines = pHead; plines; plines = plines->pNext) {
        if ( plines->off >= offMin && plines->off < offMax ) {
            if ( !pSectInfo->AddLineNumbers( plines->line, plines->off, pool ) ) {
                return false;
            }
        } else if ( plines->off > offMax ) {
            break;
        }
    }
    return true;
}
#endif
BOOL MLI::Emit(Buffer& buffer)
{
    if (!CollectSections())
        return FALSE;

    // emit header information
    if (!buffer.AppendFmt("ss", cfiles, csect))
        return FALSE;

    // emit base src file offsets
    OFF off = OFF(cbAlign(2*sizeof(USHORT) + cfiles*sizeof(ULONG) + csect*(2*sizeof(ULONG) + sizeof(USHORT))));
    OFF offFiles = off;
    pSrcFile    pFile;
    for (pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        if (!buffer.AppendFmt("l", off))
            return FALSE;
        off += pFile->Size();
    }

    // emit section info
    pSectInfo   pSI;
    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!buffer.AppendFmt("ll", pSI->offMin, pSI->offMax))
            return FALSE;
    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!buffer.AppendFmt("s", pSI->isect))
            return FALSE;

    if (!buffer.AppendFmt("f", (int)dcbAlign(buffer.Size())))
        return FALSE;

    // emit each source file's info
    off = offFiles;
    for (pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        if (!pFile->Emit(buffer, off))
            return FALSE;
        off += pFile->Size();
    }

    return TRUE;
}


BOOL MLI::Construct(PB pbStart)
{
    cfiles = *(USHORT *)pbStart;
    csect  = *(USHORT *)(pbStart + sizeof(USHORT));

    OFF offMaxMin = 2*sizeof(USHORT) + cfiles*sizeof(ULONG);
    OFF offISect  = offMaxMin + 2*csect*sizeof(ULONG);

    int i;
    /*
    for(i = 0; i < csect; i++) {
        // Do we need to do something ???
    }
    */

    size_t  offFile = cbAlign(2*sizeof(USHORT) + cfiles*sizeof(ULONG) 
                    + csect*(2*sizeof(ULONG) + sizeof(USHORT)));

    pSrcFile* ppFile = &pSrcFiles;
    for(i = 0; i < cfiles; i++)
    {
        if ((*ppFile = new (pool) SrcFile(NULL)) == NULL)
            return FALSE;

        OFF size = (*ppFile)->Construct(pbStart, static_cast<OFF>(offFile), pool);

        if (!size)
            return FALSE;

        offFile += size;
        ppFile = &(*ppFile)->pNext;
    }

    return TRUE;
}

BOOL MLI::ConvertSrcFileNames()
{
    for (pSrcFile pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        if (!pFile->ConvertFileName(pool))
            return FALSE;
    }

    return TRUE;
}

BOOL MLI::UpdateSectionList()
{
    // Just like collect sections,
    // but we don't update csect here
    // neither do we bother to change
    // the section information
    pSectInfo* ppsiTail = &pSectInfos;
    if ( pSectInfos == NULL )
        csect = 0;

    for (pSrcFile pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        for (pSectInfo psiFile = pFile->pSectInfos; psiFile; psiFile = psiFile->pNext) {

            BOOL fFound = FALSE;
            // search the module's section infos for existing info on this section
            for (pSectInfo psi = pSectInfos; psi; psi = psi->pNext) {
                if (psi->isect == psiFile->isect) {
                    // found: update extent of section
                    fFound = TRUE;
                    psi->offMin = __min(psi->offMin, psiFile->offMin);
                    psi->offMax = __max(psi->offMax, psiFile->offMax);
                    break;
                }
            }

            // not found: add new section info to module's section info
            if (!fFound) {
                if (!(*ppsiTail = new (pool) 
                    SectInfo(psiFile->isect, psiFile->offMin, psiFile->offMax)))
                return FALSE;
                ppsiTail = &(*ppsiTail)->pNext;
                csect++;
            }
        }
    }

    return TRUE;
}

BOOL MLI::EmitWithSZName(Buffer& buffer)
{
    if (!UpdateSectionList())
        return FALSE;

    // emit header information
    if (!buffer.AppendFmt("ss", cfiles, csect))
        return FALSE;
    
    // emit base src file offsets
    size_t      off = cbAlign(2*sizeof(USHORT) + cfiles*sizeof(ULONG) + csect*(2*sizeof(ULONG) + sizeof(USHORT)));
    size_t      offFiles = off;
    pSrcFile    pFile;
    for (pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        if (!buffer.AppendFmt("l", off))
            return FALSE;
        off += pFile->Size();
    }

    // emit section info
    pSectInfo   pSI;
    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!buffer.AppendFmt("ll", pSI->offMin, pSI->offMax))
            return FALSE;
    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!buffer.AppendFmt("s", pSI->isect))
            return FALSE;

    if (!buffer.AppendFmt("f", (int)dcbAlign(buffer.Size())))
        return FALSE;

    // emit each source file's info
    off = offFiles;
    for (pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        if (!pFile->Emit(buffer, static_cast<OFF>(off)))
            return FALSE;
        off += pFile->Size();
    }
            
    return TRUE;
}


// Given the module's per-file, per-section information, collect
// section information across files, into a flattened, per-module
// per-section information summary.
//
BOOL MLI::CollectSections()
{
    pSectInfo* ppsiTail = &pSectInfos;
    for (pSrcFile pFile = pSrcFiles; pFile; pFile = pFile->pNext) {
        for (pSectInfo psiFile = pFile->pSectInfos; psiFile; psiFile = psiFile->pNext) {
            // search the module's section infos for existing info on this section
            for (pSectInfo psi = pSectInfos; psi; psi = psi->pNext)
                if (psi->isect == psiFile->isect) {
                    // found: update extent of section
                    psi->offMin = __min(psi->offMin, psiFile->offMin);
                    psi->offMax = __max(psi->offMax, psiFile->offMax);
                    goto nextSIFile;
                }
            // not found: add new section info to module's section info
            if (!(*ppsiTail = new (pool) SectInfo(psiFile->isect, psiFile->offMin, psiFile->offMax)))
                return FALSE;
            ++csect;
            ppsiTail = &(*ppsiTail)->pNext;
nextSIFile:;
        }
    }
    return TRUE;
}


// calculate the size of the data for the file pointed to; return it's size
OFF SrcFile::Size()
{
    // check memoization
    if (size)
        return size;

    // header size
    size = 2*sizeof(USHORT); // csect, pad
    size += static_cast<OFF>(cbName + 1 + dcbAlign(cbName + 1)); // cbname

    // file table size
    size += csect * 3*sizeof(ULONG); // baseSrcLn, start, end

    // line table size
    for (pSectInfo pSI = pSectInfos; pSI; pSI = pSI->pNext) {
        pSI->size = sizeof(ULONG);
        pSI->size += pSI->cPair * (sizeof(ULONG) + sizeof(USHORT)); // offset, line number
        if (pSI->cPair & 1)
            pSI->size += sizeof(USHORT); // maintain alignment
        size += pSI->size;
    }
    return size;
}

BOOL SrcFile::Emit(Buffer& buffer, OFF off) const
{
    if (!buffer.AppendFmt("ss", csect, (USHORT)0 /*pad*/))
        return FALSE;
    off += csect*3*sizeof(ULONG); // space for baseSrcLine offset and start/end
    off += static_cast<OFF>(2*sizeof(USHORT) + cbName + 1 + dcbAlign(cbName + 1));

    pSectInfo pSI;
    for (pSI = pSectInfos; pSI; pSI = pSI->pNext) {
        if (!buffer.AppendFmt("l", off))
            return FALSE;
        off += pSI->size;
    }

    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!buffer.AppendFmt("ll", pSI->offMin, pSI->offMax))
            return FALSE;

    if (!buffer.AppendFmt("zbf", szFile, '\0', (int)dcbAlign(cbName + 1)))
        return FALSE;

    for (pSI = pSectInfos; pSI; pSI = pSI->pNext)
        if (!pSI->Emit(buffer))
            return FALSE;

    return TRUE;
}


OFF SrcFile::Construct(PB pbStart, OFF off, POOL_AlignNative& pool)
{
    // We have a SPB at the beginning
    LineBuffer::SPB *pspb = (LineBuffer::SPB *)(pbStart + off);
    csect = pspb->cSeg;

    // Get the name
    OFF offName = off + 2*sizeof(USHORT) + csect*3*sizeof(ULONG);  // offset of name
    PB pbName = pbStart + offName;
    cbName = *pbName;

    szFile = szCopySt((ST)pbName);

    // Construct the section list
    size_t  offSectInfo = off + 2 * sizeof(USHORT) + csect * sizeof (ULONG);
    pSectInfo* ppSI = &pSectInfos;

    for (int i = 0; i < csect; i++)
    {
        OFF offMin = *(OFF *)(pbStart + offSectInfo + 2 * i * sizeof(ULONG));
        OFF offMax = *(OFF *)(pbStart + offSectInfo + (2 * i + 1) * sizeof(ULONG));

        // Hopefully, isect would get corrected
        // while reading other section info
        if (!(*ppSI = new (pool) SectInfo(0, offMin, offMax)))
            return -1;

        ppSI = &(*ppSI)->pNext;
    }

    // Skip the name part, and read rest of sect info
    offSectInfo = offName + cbAlign(cbName + 1);

    for (ppSI = &pSectInfos; *ppSI; ppSI = &(*ppSI)->pNext) {
         OFF size = (*ppSI)->Construct(pbStart, static_cast<OFF>(offSectInfo), pool);

         if (!size)
             return size;

         offSectInfo += size;
    }

    // Set the size = 0, so that
    // 't'll be calculated when
    // we EmitWithSZName
    size = 0;

    return static_cast<OFF>(offSectInfo - off);
}

BOOL SrcFile::ConvertFileName(POOL_AlignNative& pool)
{
    // Assume the current filename is in MBCS
    // Convert it to UTF8.
    SZ szOldName = (SZ)szFile;

    size_t cch = strlen(szOldName) + 1;
    UTFSZ szNewName = new (pool) char[ cbUTFFromCch(cch) ];

    if (szFile != NULL) {
        if (_GetSZUTF8FromSZMBCS(szOldName, szNewName, cbUTFFromCch(cch)) == NULL) {
            delete [] szFile;
            szFile = szOldName;
        }
        else {
            delete [] szOldName;
            szFile = szNewName;
            cbName = strlen(szFile);
        }
    }

    return szFile != szOldName;
}


BOOL SectInfo::Emit(Buffer& buffer) const
{
    if (!buffer.AppendFmt("ss", isect, cPair))
        return FALSE;

    pLines  plines;
    for (plines = pHead; plines; plines = plines->pNext)
        if (!buffer.AppendFmt("l", plines->off))
            return FALSE;
    for (plines = pHead; plines; plines = plines->pNext)
        if (!buffer.AppendFmt("s", plines->line))
            return FALSE;
    if (cPair & 1)
        if (!buffer.AppendFmt("s", 0))
            return FALSE;
    return TRUE;
}


OFF SectInfo::Construct(PB pbStart, OFF off, POOL_AlignNative& pool)
{
    // Construct a SectInfo object with
    // buffer and return buffer size consumed
    PB pb = pbStart + off;

    isect = *(USHORT *)(pb);
    cPair = *(USHORT *)(pb + sizeof(USHORT));

    pb += 2 * sizeof(USHORT);

    PB pbOffset = pb;
    PB pbLineNo = pb + cPair * 4;

    for (int i = 0; i < cPair; i++)
    {
        int line   = *(USHORT *)pbLineNo;
        int offset = *(ULONG  *)pbOffset;

        if (!(*ppTail = new (pool) Lines(offset, line)))
            return 0;

        ppTail = &(*ppTail)->pNext;
        pbLineNo += sizeof(USHORT);
        pbOffset += sizeof(ULONG );
    }

    size = sizeof(ULONG) + cPair * (sizeof(ULONG) + sizeof(USHORT));
    size += (cPair & 1) ? OFF(sizeof(USHORT)) : 0;
    return size;
}


#ifndef __NO_MOD__

// used by Mod1::fUpdateFileInfo
BOOL MLI::EmitFileInfo(Mod1* pmod1)
{
    if (!pmod1->initFileInfo(cfiles))
        return FALSE;
    int ifile = 0;
    for (pSrcFile pFile = pSrcFiles; pFile; pFile = pFile->pNext)
        if (!pmod1->addFileInfo(ifile++, pFile->szFile))
            return FALSE;
    return TRUE;
}

#endif

BOOL MLI::Dump(const Buffer& buffer) const
{
#if !defined(PDB_LIBRARY)

    PB pb = buffer.Start();
    struct FSB { short cFile; short cSeg; long baseSrcFile[]; };
    FSB* pfsb = (FSB*)pb;
    pb += sizeof(FSB) + sizeof(long)*pfsb->cFile;
    struct SE { long start, end; };
    SE* pse = (SE*)pb;
    pb += sizeof(SE)*pfsb->cSeg;
    short* seg = (short*)pb;
    pb += sizeof(short)*pfsb->cSeg;

    OutputInit();

    StdOutPrintf(L"HDR: cFile=%u cSeg=%u\n", pfsb->cFile, pfsb->cSeg);

    int ifile;
    for (ifile = 0; ifile < pfsb->cFile; ifile++)
        StdOutPrintf(L"baseSrcFile[%d]=%04x\n", ifile, pfsb->baseSrcFile[ifile]);

    int iseg;
    for (iseg = 0; iseg < pfsb->cSeg; iseg++)
        StdOutPrintf(L"%d: start=%04x end=%04x seg=%02x\n",
                     iseg,
                     pse[iseg].start,
                     pse[iseg].end,
                     seg[iseg]);

    for (ifile = 0; ifile < pfsb->cFile; ifile++) {
        pb = buffer.Start() + pfsb->baseSrcFile[ifile];
        struct SPB { short cSeg; short pad; long baseSrcLn[]; };
        SPB* pspb = (SPB*)pb;
        pb += sizeof SPB + sizeof(long)*pspb->cSeg;
        pse = (SE*)pb;
        pb += sizeof(SE)*pspb->cSeg;
        unsigned char cbName = *pb++;
        unsigned char *Name = pb;

        // UNDONE: Is this name in UTF-8 or ANSI?

        StdOutPrintf(L"  file[%d]: cSeg=%u pad=%02x cbName=%u, Name=%S\n",
                     ifile,
                     pspb->cSeg,
                     pspb->pad,
                     cbName,
                     Name);

        for (iseg = 0; iseg < pspb->cSeg; iseg++)
            StdOutPrintf(L"  %d: baseSrcLn=%04x start=%04x end=%04x\n",
                         iseg,
                         pspb->baseSrcLn[iseg],
                         pse[iseg].start,
                         pse[iseg].end);

        for (iseg = 0; iseg < pspb->cSeg; iseg++)   {
            pb = buffer.Start() + pspb->baseSrcLn[iseg];
            struct SPO { short Seg; short cPair; long offset[]; };
            SPO* pspo = (SPO*)pb;
            pb += sizeof(SPO) + sizeof(long)*pspo->cPair;
            short* linenumber = (short*)pb;

            StdOutPrintf(L"  seg[%d]: Seg=%02x cPair=%u\n", iseg, pspo->Seg, pspo->cPair);
            StdOutPrintf(L"   ");

            for (int ipair = 0; ipair < pspo->cPair; ipair++) {
                StdOutPrintf(L" %4u:%04x", linenumber[ipair], pspo->offset[ipair]);

                if (ipair < pspo->cPair - 1 && (ipair & 3) == 3)
                    StdOutPrintf(L"\n   ");
            }

            StdOutPrintf(L"\n");
        }
    }

    StdOutFlush();
#endif
    return TRUE;
}
