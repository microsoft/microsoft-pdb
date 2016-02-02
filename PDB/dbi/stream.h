class Strm : public Stream {
public:
#ifdef PDB_MT
    Strm(PDB1* ppdb1, SN sn, Mutex& mutex ) : m_refcount(0), m_mutexOpenClose( mutex ) {
#else
    Strm(PDB1* ppdb1, SN sn) {
#endif
        PDBLOG_FUNC();
        m_ppdb1 = ppdb1;
        m_sn   = sn;
    }
    CB QueryCb() {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->GetCbStream(m_sn);
    }
    BOOL Read(OFF off, void* pvBuf, CB* pcbBuf) {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->ReadStream(m_sn, off, pvBuf, pcbBuf);
    }
    BOOL Read2(OFF off, void* pvBuf, CB cbBuf) {
        CB cbT = cbBuf;
        return m_ppdb1->pmsf->ReadStream(m_sn, off, pvBuf, &cbT) && cbT == cbBuf;
    }
    BOOL Write(OFF off, void* pvBuf, CB cbBuf) {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->WriteStream(m_sn, off, pvBuf, cbBuf);
    }
    BOOL Replace(void* pvBuf, CB cbBuf) {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->ReplaceStream(m_sn, pvBuf, cbBuf);
    }
    BOOL Append(void* pvBuf, CB cbBuf) {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->AppendStream(m_sn, pvBuf, cbBuf);
    }
    BOOL Truncate(CB cb) {
        PDBLOG_FUNC();
        return m_ppdb1->pmsf->TruncateStream(m_sn, cb);
    }
    BOOL Delete() {
        PDBLOG_FUNC();
#ifdef PDB_MT
        MTS_PROTECT_T( Mutex, m_mutexOpenClose );
#endif
        BOOL fRet = m_ppdb1->pmsf->DeleteStream(m_sn);
        fRet &= m_ppdb1->RemoveStreamName(m_sn);
        return fRet;
    }
    BOOL Release() {
        PDBLOG_FUNC();
#ifdef PDB_MT
        MTS_PROTECT_T( Mutex, m_mutexOpenClose );
        assert(m_refcount != 0);
        if (DecRefcount() == 0) {
            m_evRelease.Set();
        }
#else
        m_ppdb1->pmsf->CloseStream(m_sn);
        delete this;
#endif
        return TRUE;
    }
#ifdef PDB_MT
    ULONG IncRefcount() {
        return ++m_refcount;
    }
    ULONG DecRefcount() {
        return --m_refcount;
    }
    ULONG GetRefcount() {
        return m_refcount;
    }
    BOOL& fExclusive() {
        return m_fExclusive;
    }
    BOOL ReleaseAndAcquire(Mutex & mutex) {
        if ( SignalObjectAndWait( mutex.GetHandle(), m_evRelease.GetHandle(), INFINITE, FALSE ) == WAIT_OBJECT_0 ) {
            return mutex.Enter();
        }
        else {
            assert( FALSE );
            return FALSE;
        }
    }
#endif
private:
    PDB1*  m_ppdb1;
    SN     m_sn;
#ifdef PDB_MT
    BOOL   m_fExclusive;
    DWORD  m_refcount;
    Mutex& m_mutexOpenClose; 
    Event  m_evRelease;

    friend class PDB1;
#endif
};
