//
// DO NOT USE THIS FILE IN THE PDB CODE.
//
#pragma once
#if 0
#ifndef __INDBUF_INCLUDED__
#define __INDBUF_INCLUDED__

#include "buffer.h"

//
// IndexBuf
//	Maintain a raw buffer of non-fixed sized elements and an index
// to those elements.
//
class IndexBuf {
public:
	IndexBuf( BOOL fGrow ) // == true if this will grow by appending
		: m_index( 0, 0, fGrow ), m_raw( 0, 0, fGrow ), m_size( 0 ) 
		{}
	PB At( UINT i );	// return pointer to the ith element
	UINT Length();		// number of elements in this buffer
	BOOL Add( PB pb );	// add a new elem at this point (in buffer)
	BOOL Reserve( CB cbIn, OUT PB* ppbOut = 0 );
	void Clear();
	CB RawSize() { return m_raw.Size(); }
	PB RawStart() { return m_raw.Start(); }
	Buffer& RawBuffer() { return m_raw; }  // for new operation
	virtual BOOL scan() = 0;	// subclass provides index building
protected:
	// the index buffer contains a list of CB[0..m_size].
	// m_raw.Start()+m_index[i] is the location of the ith element
	Buffer	m_index;	// index to elements stored in raw bytes
	Buffer	m_raw;		// raw bytes in buffer
	UINT	m_size;		// number of elements
};

inline UINT IndexBuf::Length() 
{ 
	assert( m_index.Size()/sizeof(CB) == m_size ); 
	return m_size; 
}

inline PB IndexBuf::At( UINT i ) 
{ 
	assert( i < Length() );
	return m_raw.Start()+((CB*)m_index.Start())[i];
}

inline BOOL IndexBuf::Add( PB pb )
{
	assert( pb >= m_raw.Start() && pb < m_raw.End() );
	assert( fAlign( pb ) );
    CB cb = CB(pb-m_raw.Start());   // REVIEW:WIN64 CAST
	BOOL b = m_index.Append( (PB)&cb, sizeof(cb) );
	if ( b ) m_size++;
	return b;
}

inline BOOL IndexBuf::Reserve( CB cbIn, OUT PB* ppbOut )
{
	return m_raw.Reserve( cbIn, ppbOut );
}

inline void IndexBuf::Clear()
{
	m_raw.Clear();
	m_index.Clear();
	m_size = 0;
}

template <class T>
class InBuf: public IndexBuf
{
public:
	BOOL append( T* pT )	{ expect(fAlign(pT)); return Add( (PB)pT ); }
	T* operator[]( UINT i )	{ return (T*)At( i ); }
	BOOL scan() {	// find all T*'s in the list
		m_size = 0;
		m_index.Truncate(0);
		for( PB pb = m_raw.Start(); pb < m_raw.End(); pb = ((T*)pb)->pbEnd() ) {
			expect(fAlign(pb));
			if ( !Add( pb ) ) return FALSE;
			assert( At( m_size-1 ) == pb );
		}
		return TRUE;
	}
	InBuf( BOOL fGrow=TRUE ) : IndexBuf( fGrow ) {}
};

template <class T>
class ArrayBuf: public Buffer
{
public:
    size_t length() const { return Size() / sizeof(T); }
    T* At( size_t i ) const
    { 
        assert( i <= length() );    // allow 1 past end of buffer
        return (T*)Start()+i;
    }
    const T& operator[]( size_t i ) const
    {
        return *At(i);
    }
    T& operator[]( size_t i ) 
    {
        return *At(i);
    }
    BOOL append( const T& t ) 
    {
        return Append( (PB)&t, sizeof(T) );
    }
};

template <class T>
class ArrayPtrBuf: public ArrayBuf<T*>
{
public:
    void destroy()  // only works if T is a pointer type
    {
        for ( size_t i = 0; i < length(); ++i )
            delete *At(i);
        Reset();
    }

    typedef T** _Nodeptr;

    class iterator  // based on STL forward iterator
    {
    public:
	    iterator()
		    {}
	    iterator(_Nodeptr _P)
		    : _Ptr(_P) {}
	    T& operator*() const
		    {return **_Ptr; }
	    T* operator->() const
		    {return (&**this); }
	    iterator& operator++()
		    {++_Ptr;
		    return (*this); }
	    iterator operator++(int)
		    {iterator _Tmp = *this;
		    ++*this;
		    return (_Tmp); }
	    bool operator==(const iterator& _X) const
		    {return (_Ptr == _X._Ptr); }
	    bool operator!=(const iterator& _X) const
		    {return (!(*this == _X)); }
	    _Nodeptr _Mynode() const
		    {return (_Ptr); }
    private:
        _Nodeptr _Ptr;
    };

    iterator begin() { return iterator( At(0) ); }
    iterator end() { return iterator( At( length() ) ); }
    iterator begin() const { return iterator( At(0) ); }
    iterator end() const { return At( length() ); }
    bool empty() { return begin() == end(); }
};

#endif
#endif
