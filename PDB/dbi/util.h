////////////////////////////////////////////////////////////////////////////////
// Inline utility functions.

inline BOOL DBI1::packRefToGS (PSYM psym, IMOD imod, OFF off, OFF *poff)
{
    assert(pgsiGS);
    assert(fWrite);
    return pgsiGS->packRefSym(psym, imod, off, poff);
}

inline BOOL DBI1::removeGlobalRefsForMod(IMOD imod)
{
    assert(fWrite);
    assert(imod != 0);
    return pgsiGS->removeGlobalRefsForMod(imod);
}

inline BOOL DBI1::packRefToGSForMiniPDB(const char *sz, WORD imod, DWORD isectCoff, bool fLocal, bool fData, bool fUDT, bool fLabel, bool fConst)
{
    assert(pgsiGS);
    assert(fWrite);
    assert(imod != 0);
    return pgsiGS->packRefForMiniPDB(sz, imod, isectCoff, fLocal, fData, fUDT, fLabel, fConst);
}

inline BOOL DBI1::packTokenRefToGS(PSYM psym, IMOD imod, OFF off, OFF *poff)
{
    assert(pgsiGS);
    assert(fWrite);
    return pgsiGS->packTokenRefSym(psym, imod, off, poff);
}

inline BOOL DBI1::packSymToGS (PSYM psym, OFF *poff)
{
    assert(pgsiGS);
    assert(fWrite);
    return pgsiGS->packSym(psym, poff);
}

inline BOOL DBI1::packSymToPS (PSYM psym)
{
    assert(pgsiPS);
    assert(fWrite);
    return pgsiPS->packSym(psym);
}

inline BOOL DBI1::packSymSPD (PSYM psym, OFF * poff, SPD & spd)
{
    assert(pgsiGS);
    assert(fWrite);
    return pgsiGS->packSymSPD(psym, poff, spd);
}

inline BOOL DBI1::decRefCntGS (OFF off)
{
    assert(pgsiGS);
    assert(fWrite);
    return pgsiGS->decRefCnt(off);
}


inline BOOL DBI1::fAddSym(PSYM psymIn, OUT PSYM* psymOut) 
{
    assert(fWrite);
    // MTS_ASSERT(m_csForSymRecs);          // don't need because of fWrite
    expect(fAlign(cbForSym(psymIn)));
    if (!bufSymRecs.Append((PB)psymIn, cbForSym(psymIn), (PB*)psymOut)) {
        ppdb1->setOOMError();
        return FALSE;
    }
    return TRUE;
}

inline SZ DBI1::szObjFile(IMOD imod)
{ 
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    SZ sz = pmodiForImod(imod)->szObjFile();
    return fWrite ? szCopy(sz) : sz;
}

inline SZ DBI1::szModule(IMOD imod)
{
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    SZ sz = pmodiForImod(imod)->szModule();
    return fWrite ? szCopy(sz) : sz;
}

inline CB DBI1::cbSyms(IMOD imod)
{
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    return pmodiForImod(imod)->cbSyms; 
}

inline CB DBI1::cbLines(IMOD imod)
{
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    return pmodiForImod(imod)->cbLines;
}

inline CB DBI1::cbC13Lines(IMOD imod)
{
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    return pmodiForImod(imod)->cbC13Lines;
}

inline SN DBI1::Sn(IMOD imod)
{
    MTS_PROTECT(m_csForMods);
    dassert(pmodiForImod(imod));
    return pmodiForImod(imod)->sn;
}

inline BOOL Mod1::addFileInfo(IFILE ifile, SZ_CONST szFile)
{
    return pdbi1->addFileInfo(imod, ifile, (SZ)szFile);
}
