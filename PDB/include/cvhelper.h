#ifndef _CVHELPER_H_
#define _CVHELPER_H_

// MP is used to buffer the incoming public info for a module

struct MP {

    typedef const char* SZ_CONST;

    PUBSYM32 *  m_pPubSym;

    // use a local copy for almost all cases,
    // go for the heap version only on extra long symbols.
    //
    struct BigEnough_PubSym : public PUBSYM32 {
        BYTE        rgbExtra[2049];
    } m_pubsym;

    MP(SZ_CONST utfszPublic, ISECT isect_, OFF off_, CV_pubsymflag_t cvpsf =0)
    {
        size_t  cbName = strlen(utfszPublic);  // UTF8 has same length as ASCII
        size_t  cbRec  = cbAlign(offsetof(PUBSYM32,name) + cbName + 1); 

        if (cbRec < sizeof(m_pubsym)) {
            m_pPubSym = &m_pubsym;
        }
        else {
            m_pPubSym = reinterpret_cast<PUBSYM32*>(new BYTE[cbRec]);
        }

        m_pPubSym->reclen = (unsigned short) (cbRec - sizeof(m_pPubSym->reclen));
        m_pPubSym->rectyp = S_PUB32;
        m_pPubSym->off    = off_;
        m_pPubSym->seg    = isect_;
        m_pPubSym->pubsymflags.grfFlags = cvpsf;
        memcpy(SZ(m_pPubSym->name), utfszPublic, cbName + 1);

        memset(m_pPubSym->name + cbName, 0, dcbAlign(offsetof(DATASYM32,name) + cbName + 1));
    }

    ~MP()
    {
        if (m_pPubSym != &m_pubsym) {
            delete [] reinterpret_cast<BYTE*>(m_pPubSym);
        }
    }

    PSYM operator &()
    {
        return reinterpret_cast<PSYM>(m_pPubSym);
    }
};


#endif
