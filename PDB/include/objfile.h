//
// class ObjectFile
//
// Readonly access to an object file 
// (or container object, e.g., library )
//
#if CC_LAZYSYMS

class ObjectFile 
{
public:
    ObjectFile( SZ_CONST _szModule, SZ_CONST _szObjFile, SZ_CONST _szFullPath );   
    ~ObjectFile();
    BOOL Open();
    BOOL Section( size_t i, OUT PB& pb, OUT CB& cb );
    DWORD Timestamp();
    PB pbObjView( DWORD fo=0 ) { assert( (CB)fo < cbObj ); return pbObj+fo; }

    static void Flush();    // close and unmap files

private:
    SZ_CONST    szModule;
    SZ_CONST    szObjFile;
    SZ_CONST    szFullPath;
    DWORD       dwSize;
    LPVOID      pvMapView;
    HANDLE      hFile;
    PB          pbObj;
    CB          cbObj;
    BOOL        cacheHit;

    BOOL LocateObj();   // locate the object in the container
    BOOL OpenFile( SZ_CONST );  // map the file contents
    PB pbMapView( DWORD fo=0 ) 
    { return (PB)pvMapView+fo; }

    const IMAGE_ARCHIVE_MEMBER_HEADER& hdrMapView( DWORD fo ) 
    { return *(IMAGE_ARCHIVE_MEMBER_HEADER*)pbMapView(fo); }

    DWORD foMember( DWORD foHdr ) 
    { return foHdr + sizeof( IMAGE_ARCHIVE_MEMBER_HEADER ); }

    const IMAGE_FILE_HEADER& ihdrMapView()
    { assert(pbObj); return *(IMAGE_FILE_HEADER*)pbObj; }

    BOOL shdrMapView( size_t i, const IMAGE_SECTION_HEADER*& );

    BOOL ExtractMember();
    SZ_CONST ExpandName( char szName[], SZ_CONST longnames );
    BOOL NextMember( DWORD& fo );
    BOOL MemberSize( const IMAGE_ARCHIVE_MEMBER_HEADER& hdr, OUT CB& cb );

    struct LibMap {
        LPVOID      pvMapView;
        HANDLE      hFile;
        DWORD       dwSize;
        char        szObjFile[_MAX_PATH];

        LibMap( SZ_CONST _szObjFile, LPVOID _pvMapView, HANDLE _hFile, DWORD _dwSize )
            : pvMapView( _pvMapView ), hFile( _hFile ), dwSize( _dwSize )
        { 
            assert( _szObjFile ); 
            strcpy( szObjFile, _szObjFile );
        }
        BOOL operator==( SZ_CONST sz ) 
        { 
            return strcmp( sz, szObjFile ) == 0;
        }
    };

    static ArrayBuf<LibMap> cache;  // cached library files
    static BOOL Lookup( SZ_CONST sz, OUT LPVOID& pvMapView, OUT HANDLE& hFile, OUT DWORD& dwSize );
    static void Insert( SZ_CONST sz, LPVOID pvMapView, HANDLE hFile, DWORD dwSize );
};

#endif
