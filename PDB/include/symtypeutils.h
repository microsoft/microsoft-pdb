//SymTypeUtils - utilities to navigate through the cv sym and type records
#ifndef __SymTypeUtils_H_
#define __SymTypeUtils_H_

#include <stdio.h>

unsigned
CbGetNumericData ( const void * pv, DWORD & dwVal );

inline CB cbNumField(PB pb)
{
    DWORD dw;
    return CbGetNumericData( pb, dw );
}

inline bool getNumField(PB pb, PB pbOut)
{
    unsigned cb = cbNumField( pb );
    memcpy(pbOut, pb, cb);
    return true;
}


inline SZ szUDTName(PB pb)
{
    assert(!fNeedsSzConversion((PTYPE)pb));

    switch(((PTYPE)pb)->leaf) {
        case LF_STRUCTURE:
        case LF_CLASS:
        case LF_INTERFACE:
            return (SZ)(((lfClass *)(pb + sizeof(USHORT)))->data + cbNumField(((lfClass *)(pb + sizeof(USHORT)))->data));
        case LF_UNION:
            return (SZ)(((lfUnion *)(pb + sizeof(USHORT)))->data + cbNumField(((lfUnion *)(pb + sizeof(USHORT)))->data));
        case LF_ENUM:
            return (SZ)(&((lfEnum *)(pb + sizeof(USHORT)))->Name);
        case LF_STRUCTURE_16t:
        case LF_CLASS_16t:
            return (SZ)(((lfClass_16t *)(pb + sizeof(USHORT)))->data + cbNumField(((lfClass *)(pb + sizeof(USHORT)))->data));
        case LF_UNION_16t:
            return (SZ)(((lfUnion_16t *)(pb + sizeof(USHORT)))->data + cbNumField(((lfUnion *)(pb + sizeof(USHORT)))->data));
        case LF_ENUM_16t:
            return (SZ)(&((lfEnum_16t *)(pb + sizeof(USHORT)))->Name);
        case LF_ALIAS:
            return SZ(&((lfAlias *)(pb + sizeof(USHORT)))->Name);
        default:
            assert(FALSE);
            return (SZ)0;
    }
}


inline ST stUDTName(PB pb)
{
    switch(SZLEAFIDX(((PTYPE)pb)->leaf)) {
        case LF_STRUCTURE:
        case LF_CLASS:
            return (ST)(((lfClass *)(pb + sizeof(USHORT)))->data + cbNumField(((lfClass *)(pb + sizeof(USHORT)))->data));
        case LF_UNION:
            return (ST)(((lfUnion *)(pb + sizeof(USHORT)))->data + cbNumField(((lfUnion *)(pb + sizeof(USHORT)))->data));
        case LF_ENUM:
            return (ST)(&((lfEnum *)(pb + sizeof(USHORT)))->Name);
        case LF_STRUCTURE_16t:
        case LF_CLASS_16t:
            return (ST)(((lfClass_16t *)(pb + sizeof(USHORT)))->data + cbNumField(((lfClass *)(pb + sizeof(USHORT)))->data));
        case LF_UNION_16t:
            return (ST)(((lfUnion_16t *)(pb + sizeof(USHORT)))->data + cbNumField(((lfUnion *)(pb + sizeof(USHORT)))->data));
        case LF_ENUM_16t:
            return (ST)(&((lfEnum_16t *)(pb + sizeof(USHORT)))->Name);
        case LF_ALIAS:
            return ST(&((lfAlias *)(pb + sizeof(USHORT)))->Name);
        default:
            assert(FALSE);
            return (ST)0;
    }
}

static const char stVFuncTab[] = {8,'v', 'f', 'u', 'n', 'c', 't', 'a', 'b'};
static char stNull[] = {0, 0};

inline ST stMemberName(lfEasy *pleaf)
{
    switch(pleaf->leaf) {
        case LF_MEMBER_16t:
            return (ST)(((lfMember_16t *)pleaf)->offset + cbNumField(((lfMember *)pleaf)->offset));

        case LF_MEMBER:
            return (ST)(((lfMember *)pleaf)->offset + cbNumField(((lfMember *)pleaf)->offset));

        case LF_ENUMERATE:
            return (ST)(((lfEnumerate *)pleaf)->value + cbNumField(((lfEnumerate *)pleaf)->value));
        
        case LF_METHOD:
            return (ST)(((lfMethod *)pleaf)->Name);
        
        case LF_METHOD_16t:
            return (ST)(((lfMethod_16t *)pleaf)->Name);

        case LF_ONEMETHOD:     
        {
            lfOneMethod *plfOneMethod = (lfOneMethod *) pleaf;
            ST st = (ST) plfOneMethod->vbaseoff; 
            if (plfOneMethod->attr.mprop == CV_MTintro || 
                plfOneMethod->attr.mprop == CV_MTpureintro) 
                st += sizeof (long);
            return st;
        }
        
        case LF_ONEMETHOD_16t:     
        {
            lfOneMethod_16t *plfOneMethod = (lfOneMethod_16t *) pleaf;
            ST st = (ST) plfOneMethod->vbaseoff; 
            if (plfOneMethod->attr.mprop == CV_MTintro || 
                plfOneMethod->attr.mprop == CV_MTpureintro) 
                st += sizeof (long);
            return st;
        }

        case LF_STMEMBER:
            return (ST)(((lfSTMember *)pleaf)->Name);
        
        case LF_STMEMBER_16t:
            return (ST)(((lfSTMember_16t *)pleaf)->Name);

        case LF_VFUNCTAB:
        case LF_VFUNCTAB_16t:
        case LF_VFUNCOFF:
        case LF_VFUNCOFF_16t:
            return (ST)stVFuncTab;

        case LF_NESTTYPE:
            return (ST)(((lfNestType *)pleaf)->Name);

        case LF_NESTTYPE_16t:
            return (ST)(((lfNestType_16t *)pleaf)->Name);

        case LF_NESTTYPEEX:
            return (ST)(((lfNestTypeEx *)pleaf)->Name);

        case LF_BCLASS:
        case LF_VBCLASS:
        case LF_IVBCLASS:
        case LF_BCLASS_16t:
        case LF_VBCLASS_16t:
        case LF_IVBCLASS_16t:
            // no names on these guys
            return stNull;

        default:
            assert(FALSE);
            return stNull;
    }
}

inline TYPTYPE *ConvertLeaftoType(lfEasy *pleaf)
{
    return (TYPTYPE *) ((PB)pleaf - sizeof(unsigned short));
}

inline bool fDecomposeQualName(_In_ ST st, _Inout_ char rg[257], _Out_opt_ SZ *pszClassName, _Out_opt_ SZ *pszMemberName)
{
    memcpy(rg, st, *(unsigned char *)st + 1);
    // make the rg a sz
    rg[*(unsigned char *)rg + 1] = 0;

    //look for  last ::
    char *pc  = strrchr(rg + 1, ':');
    if (pc == NULL || (*(pc - 1) != ':'))
        // didn't find it just return the original string
        return false;

    // fill in a zero terminated class name
    if (pszClassName)
    {
        *(pc - 1) = 0;
        *pszClassName = rg + 1;
    }

    if (pszMemberName)
    {
        // skip past the ::
        *pszMemberName = pc + 1;
    }

    return true;
}

inline bool FindMemberByNameAndLeafIndexInFlist(lfEasy *ptypeFlist, SZ_CONST szMember, lfEasy **ppleaf, unsigned short *legal_leaves, int leaf_cnt)
{
    TypeTiIter tii(ConvertLeaftoType(ptypeFlist));
    lfEasy *pleaf;
    *ppleaf = 0;
    assert((ptypeFlist->leaf == LF_FIELDLIST) || 
        (ptypeFlist->leaf == LF_FIELDLIST_16t));
    do
    {
        pleaf = (lfEasy *)tii.pbCurField();
        if ( pleaf && strcmp( stMemberName( pleaf ), szMember ) == 0 )
        {
            if (leaf_cnt == 0)
            {
                *ppleaf = pleaf;
                return true;
            }

            for (int i = 0; i < leaf_cnt; i++)
            {
                if ( *(legal_leaves + i) == pleaf->leaf )
                {
                    *ppleaf = pleaf;
                    return true;
                }
            }
        }
    } while (tii.nextField());

    return false;
}

inline bool FindMethodInMList(lfMethodList *pleaf, TI tiFunc, mlMethod ** ppmlMethod)
{
    mlMethod *pml = (mlMethod *)(pleaf->mList);
    USHORT cbLeaf = sizeof(ushort);  // Size of leaf index
    USHORT cbLen = ConvertLeaftoType((lfEasy *)pleaf)->len;
    USHORT cb;
    int i;
    for (i = 0; cbLeaf < cbLen; i++) 
    {
        if (pml->index == tiFunc) 
        {
            *ppmlMethod = pml;
            return true;
        }

        if (pml->attr.mprop == CV_MTintro || pml->attr.mprop == CV_MTpureintro) 
        {
            cb = sizeof(*pml) + sizeof(long);
        }
        else 
        {
            cb = sizeof(*pml);
        }
        pml = (mlMethod *)((UCHAR *)pml + cb);
        cbLeaf += cb;
    }
    return false;
}

inline TI tiFListFromUdt(TYPTYPE *ptype)
{
    switch (ptype->leaf)
    {
        case LF_STRUCTURE:
        case LF_CLASS:
            return ((lfClass *)&ptype->leaf)->field;

        case LF_UNION:
            return ((lfUnion *)&ptype->leaf)->field;

        case LF_ENUM:
            return ((lfEnum *)&ptype->leaf)->field;

        case LF_STRUCTURE_16t:
        case LF_CLASS_16t:
            return (TI) ((lfClass *)&ptype->leaf)->field;

        case LF_UNION_16t:
            return (TI) ((lfUnion *)&ptype->leaf)->field;
        
        case LF_ENUM_16t:
            return (TI) ((lfEnum *)&ptype->leaf)->field;
        
        default:
            return T_NOTYPE;
    }
}

inline bool fMemberOfClass(TPI *ptpi, _In_ ST st, lfEasy **ppleaf, TI *ptiClass)
{    
    SZ szClass;
    SZ szMember;
    char rg[257];

    // see if we are dealing with a qualified name if so we probably
    // are looking at a static data member
    if (fDecomposeQualName(st, rg, &szClass, &szMember))
    {
        TI tiFlist;
        TYPTYPE *ptype;
        if (!ptpi->QueryTiForUDT(szClass, true, ptiClass))
            return false;

        if (!ptpi->QueryPbCVRecordForTi(*ptiClass, (BYTE **)&ptype))
            return false;
        
        tiFlist = tiFListFromUdt(ptype);
        
        if (!ptpi->QueryPbCVRecordForTi(tiFlist, (BYTE **)&ptype))
            return false;
        
        return (FindMemberByNameAndLeafIndexInFlist((lfEasy *)&ptype->leaf, szMember, ppleaf, NULL, 0));
    }
    else
        return false;
}

static inline wchar_t *SZNameFromTSRecord(PTYPE ptype, __out_ecount(cchNameBuf) wchar_t *wszNameBuf, size_t cchNameBuf)
{
    dassert(ptype != NULL && wszNameBuf != NULL);
    char *pchName = NULL;

    if (ptype->leaf == LF_TYPESERVER2) {
        lfTypeServer2 *plf = (lfTypeServer2 *)&ptype->leaf;
        pchName = (SZ)plf->name;
    }
    else if (ptype->leaf == LF_TYPESERVER || ptype->leaf == LF_TYPESERVER_ST) {
        lfTypeServer *plf = (lfTypeServer *)&ptype->leaf;
        pchName = (SZ)plf->name;
    }
    else if (ptype->leaf == LF_PRECOMP || ptype->leaf == LF_PRECOMP_ST) {
        lfPreComp *plf = (lfPreComp *)&ptype->leaf;
        pchName = (SZ)plf->name;
    }
    else
        assert(FALSE);

    if (!fNeedsSzConversion(ptype)) {
        return _GetSZUnicodeFromSZUTF8(pchName, wszNameBuf, cchNameBuf);
    }
    else
    {
        return _GetSZUnicodeFromSTMBCS(pchName, wszNameBuf, cchNameBuf);
    }
}

#endif
