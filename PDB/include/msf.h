// msf.h: see "The Multistream File API" for more information

#ifndef __MSF_INCLUDED__
#define __MSF_INCLUDED__

#ifndef TRUE

#define TRUE    1
#define FALSE   0
typedef int BOOL;

#if !defined(verify)
#ifdef _DEBUG
#define verify(x)   assert(x)
#else
#define verify(x) (x)
#endif
#endif

#endif

// compile time assert
#if !defined(cassert)
#define cassert(x) extern char dummyAssert[ (x) ]
#endif

#if !defined(pure)
#define pure =0
#endif

#define MsfInterface struct

MsfInterface MSF;

// type of callback arg to MSFGetRawBytes
typedef BOOL (__cdecl *PFNfReadMSFRawBytes)(const void *, long);

typedef void *          PV;
typedef unsigned short  SN;     // stream number
typedef unsigned long   UNSN;   // unified stream number
typedef long            CB;     // size (count of bytes)
typedef long            OFF;    // offset

#define cbNil   ((long)-1)
#define snNil   ((SN)-1)
#define unsnNil UNSN(-1)

typedef long    MSF_EC;
enum MSFErrorCodes {
    MSF_EC_OK,
    MSF_EC_OUT_OF_MEMORY,
    MSF_EC_NOT_FOUND,
    MSF_EC_FILE_SYSTEM,
    MSF_EC_FORMAT,
    MSF_EC_ACCESS_DENIED,
    MSF_EC_CORRUPT,
    MSF_EC_MAX
};

enum {
    MSFBaseIntv = 19960101,
    MSFOleIntv =  19960307
};

typedef unsigned long   MSF_FLAG;
enum {
    msffCreate = 0x0001,
    msffOle = 0x0002,
    msffAccess = 0x0004,
};

#ifdef __cplusplus

const CB    cbPgDef     = 0x0400;
const CB    cbMsfPageDefault = cbPgDef;

struct IStream;

// define the abstract MSF class
//
MsfInterface MSF { // multistream file

    enum { intvBase = MSFBaseIntv, intvOle = MSFOleIntv };
    enum CompressionType {
        ctFileSystem,
        ctInternal,
    };

    static MSF*     Open(
                        const wchar_t *wszFilename,
                        BOOL fWrite,
                        MSF_EC* pec,
                        CB cbPage=cbPgDef,
                        MSF_FLAG msff = msffCreate|msffOle
                        );

    static MSF*     Open(
                        IStream* pIStream,
                        BOOL fWrite,
                        MSF_EC *pec,
                        CB cbPage=cbPgDef
                        );

    virtual long    QueryInterfaceVersion() pure;
    virtual long    QueryImplementationVersion() pure;

    virtual CB      GetCbPage() pure;
    virtual CB      GetCbStream(SN sn) pure;
    virtual SN      GetFreeSn() pure;
    virtual BOOL    ReadStream(SN sn, PV pvBuf, CB cbBuf) pure;
    virtual BOOL    ReadStream(SN sn, OFF off, PV pvBuf, CB* pcbBuf) pure;
    virtual BOOL    WriteStream(SN sn, OFF off, PV pvBuf, CB cbBuf) pure;
    virtual BOOL    ReplaceStream(SN sn, PV pvBuf, CB cbBuf) pure;
    virtual BOOL    AppendStream(SN sn, PV pvBuf, CB cbBuf) pure;
    virtual BOOL    TruncateStream(SN sn, CB cb) pure;
    virtual BOOL    DeleteStream(SN sn) pure;
    virtual BOOL    Commit(MSF_EC *pec = NULL) pure;
    virtual BOOL    Close() pure;
    virtual BOOL    GetRawBytes(PFNfReadMSFRawBytes fSnarfRawBytes) pure;
    virtual UNSN    SnMax() const pure;
    virtual BOOL    FSupportsSharing() const pure;
    virtual BOOL    CloseStream(UNSN sn) pure;
    virtual bool    FIsBigMsf() const pure;
    virtual bool    FSetCompression(CompressionType ct, bool fCompress) const pure;
    virtual bool    FGetCompression(CompressionType ct, bool & fCompress) const pure;
    virtual BOOL    NonTransactionalEraseStream(SN sn) pure;

#ifdef PDB_MT
private:
    typedef unsigned __int64  QWORD;

    typedef struct {
        QWORD    qwVolumeSerialNumber; 
        BYTE     rgb[16];
    } FILE_ID;

    struct MSF_REC {
        MSF * pmsf;
        BOOL fWrite;
        DWORD refcount;
        bool operator==(MSF_REC& b) {
            return (pmsf == b.pmsf) && (fWrite == b.fWrite);
        }
    };

    static BOOL GetFileID(const wchar_t * wszFilename, FILE_ID * id);
    static BOOL GetFileID(HANDLE hfile, FILE_ID * id);
    static Map<FILE_ID, MSF_REC, HashClassCRC<FILE_ID> > s_mpOpenedMSF;
    static CriticalSection s_csForMSFOpen;

    friend class MSF_HB;
#endif
};

#endif

#if defined(__cplusplus)
extern "C" {
#endif

// MSFOpenW         -- open MSF; return MSF* or NULL if error.
// MSFOpenExW       -- open MSF with specific page size; return MSF* or NULL if error.
// MSFGetCbPage     -- return page size
// MSFGetCbStream   -- return size of stream or -1 if stream does not exist
// MSFReadStream    -- read stream into pvBuf; return TRUE if successful
// MSFClose         -- close MSF; return TRUE if successful

#ifdef MSF_IMP
 #ifdef PDB_LIBRARY
  #define MSF_EXPORT2
 #else
  #define MSF_EXPORT2  __declspec(dllexport)
 #endif
#else
 #define MSF_EXPORT2  __declspec(dllimport)
#endif

#define MSF_EXPORT

MSF_EXPORT2 MSF * __cdecl MSFOpenW(const wchar_t *wszFilename, BOOL fWrite, MSF_EC* pec);
MSF_EXPORT2 MSF * __cdecl MSFOpenExW(const wchar_t *wszFilename, BOOL fWrite, MSF_EC* pec, CB cbPage);

#if defined(__cplusplus)
};
#endif

#endif // __MSF_INCLUDED__
