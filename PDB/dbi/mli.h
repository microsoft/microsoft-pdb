// module line info internal interfaces

#ifndef __MLI_INCLUDED__
#define __MLI_INCLUDED__

#include "cvlines.h"

struct SrcFile;
typedef SrcFile* pSrcFile;
struct SectInfo;
typedef SectInfo* pSectInfo;
struct Lines;
typedef Lines *pLines;

struct FileNameMapCmd
{
    virtual const char* map( const char* szFileName ) = 0;
};

struct MLI {
	MLI() : cfiles(0), pSrcFiles(0), csect(0), pSectInfos(0) { }

	BOOL AddLines(SZ_CONST szSrc, ISECT isect, OFF offCon, CB cbCon, OFF doff,
	              LINE32 lineStart, IMAGE_LINENUMBER *plnumCoff, CB cb);
	pSrcFile AddSrcFile(SZ_CONST szfilename);
	BOOL Emit(Buffer& buffer);
	BOOL CollectSections();
	BOOL Dump(const Buffer& buffer) const;
#ifdef PDB_DEFUNCT_V8
    bool FindSecByAddr( ISECT isect, OFF off, OFF* poffMax );
    bool Merge( MLI* pmliNew, FileNameMapCmd* pfnMapCmd ); // called on old MLI, passing new in
#endif


#ifndef __NO_MOD__
	BOOL EmitFileInfo(Mod1* pmod1);
#ifdef PDB_DEFUNCT_V8
    void EmitSecContribs(Mod1* pmod1);
	Mod1*	pmod1;
#endif
#endif

    BOOL Construct(PB pbStart);
    BOOL ConvertSrcFileNames();
    BOOL UpdateSectionList();
    BOOL EmitWithSZName(Buffer& buffer);

	POOL_AlignNative    pool;       // native aligned pool

    USHORT	cfiles;
	pSrcFile pSrcFiles;

	USHORT	csect;
	pSectInfo pSectInfos;
};

struct SrcFile {
	SrcFile(SZ_CONST szFile_) : pNext(0), cbName(szFile_ ? strlen(szFile_) : 0),
                    size(0), szFile(szFile_), csect(0), pSectInfos(0) { }
    pSectInfo AddSectInfo(ISECT isect, OFF offMin, OFF offMax, POOL_AlignNative & pool);
	OFF Size();
	BOOL Emit(Buffer& buffer, OFF off) const;
#ifdef PDB_DEFUNCT_V8
    bool FindSecByAddr( ISECT isect, OFF off, OFF* poffMax );
    bool MergeSrc( MLI* pmliNew, FileNameMapCmd* pfnMapCmd );
#endif
    OFF  Construct(PB pbStart, OFF off, POOL_AlignNative& pool);
    BOOL ConvertFileName(POOL_AlignNative& pool);

	pSrcFile    pNext;
	pSectInfo   pSectInfos;
	SZ_CONST    szFile;
	size_t      cbName;
	OFF         size;
	USHORT	    csect;
};

struct SectInfo {
	SectInfo(ISECT isect_, OFF offMin_, OFF offMax_)
		: pNext(0), isect(isect_), cPair(0), pHead(0), ppTail(&pHead), size(0), offMin(offMin_), offMax(offMax_) { }
    BOOL AddLineNumbers(int linenumber, int offset, POOL_AlignNative& pool);
	BOOL Emit(Buffer& buffer) const;
#ifdef PDB_DEFUNCT_V8
    bool MergeLines( pSectInfo pSectInfo, OFF offMin, OFF offMax, POOL_AlignNative& pool );
#endif
    OFF Construct(PB pbStart, OFF off, POOL_AlignNative &pool);

	pSectInfo pNext;
	USHORT  isect;
	USHORT  cPair;
	pLines	pHead;
	pLines* ppTail;
	OFF		size;
	OFF		offMin;
	OFF		offMax;
};

struct Lines {
	Lines(OFF off_, ULONG line_) : pNext(0), off(off_), line(line_) { }

	pLines pNext;
	OFF off;
	ULONG line;
};

#endif // !__MLI_INCLUDED__
