#ifndef _CVLINES_H_
#define _CVLINES_H_

#pragma warning(push)
#pragma warning(disable:4200)

    class LineBuffer
    {
    private:
        struct FSB { WORD cFile; WORD cSeg; DWORD baseSrcFile[]; };

    public:
        LineBuffer( bool fDeletePb = false ) : m_pb( NULL ), m_cb( 0 ), m_fDeletePb( fDeletePb )
        {}
        ~LineBuffer() {
            if ( m_fDeletePb && m_pb != NULL ) {
                delete [] m_pb;
                m_pb = NULL;
            }
        }
        struct SPB { WORD cSeg; WORD pad; DWORD baseSrcLn[]; };
        struct SE  { DWORD start, end; };
        struct SPO { WORD Seg; WORD cPair; DWORD offset[]; };

        DWORD cFile(){
            return ((FSB*)pb())->cFile;
        }
        DWORD cSeg(){
            return ((FSB*)pb())->cSeg;
        }
        DWORD* baseSrcFile(){
            return ((FSB*)pb())->baseSrcFile;
        }
        SE* startEnd() {
            return (SE*)(baseSrcFile()+cFile());
        }
        WORD* iSegs() {
            return (WORD*)(startEnd()+cSeg());
        }
        bool handlesSeg( DWORD iSect ) {
            WORD* pSegs = iSegs();
            for ( WORD i = 0; i < cSeg(); ++i ) {
                if ( pSegs[ i ] == iSect ) {
                    return true;
                }
            }
            return false;
        }

        SPB* file( DWORD i ) {
            return (SPB*)(pb() + baseSrcFile()[i]);
        }
        SE*  segStartEnd( DWORD i ) {
            return (SE*)&file( i )->baseSrcLn[file(i)->cSeg];
        }
        SE*  segStartEnd( SPB* pspb ) {
            return (SE*)&pspb->baseSrcLn[pspb->cSeg];
        }
        const char* fileName( DWORD i, DWORD* pcb ) {
            if ( pcb )
                *pcb = static_cast<DWORD>(strlen((char*)(&segStartEnd( i )[file(i)->cSeg])));
            return (char*)(&segStartEnd( i )[file(i)->cSeg]);
        }

        SPO* fileSeg( DWORD i, DWORD s ) {
            return (SPO*)(pb() + file(i)->baseSrcLn[s]);
        }
        SPO* fileSeg( SPB* pspb, DWORD s ) {
            return (SPO*)(pb() + pspb->baseSrcLn[s]);
        }
        USHORT* linenumbers( SPO* pspo ) {
            return (USHORT*)&pspo->offset[ pspo->cPair ];
        }
        USHORT* linenumbers( DWORD i, DWORD s ) {
            return linenumbers( fileSeg( i, s ) );
        }

        bool fInit( PB pb, CB cb ) {
            m_pb = pb;
            m_cb = cb;
            return true;
        }
        bool isEmpty() {
            return cb() == 0;
        }
    private:
        bool m_fDeletePb;
        PB pb() { return m_pb; }
        CB cb() { return m_cb; }
        PB m_pb;
        CB m_cb;
    };

#pragma warning(pop)

#endif
