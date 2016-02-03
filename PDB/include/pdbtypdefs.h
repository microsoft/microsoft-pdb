#pragma once
//standard typedefs used by pdb and its associated components

typedef unsigned short  ushort;
typedef unsigned long   ulong;
typedef char*	        ST;			// length prefixed string
typedef const char*	ST_CONST;		// const length prefixed string
typedef char*	        SZ;			// zero terminated string
typedef const char*     SZ_CONST;               // const string
typedef wchar_t*	USZ;			// zero terminated string
typedef const wchar_t* USZ_CONST;               // const string
typedef BYTE*	        PB;			// pointer to some bytes
typedef long	        CB;			// count of bytes
typedef long	        OFF;		        // offset
typedef OFF*            POFF;                   // pointer to offset
typedef wchar_t *       WSZ;                    // Wide string
typedef const wchar_t *	WSZ_CONST;              // Const wide string

// define types that hold machine dependent pointer types as
// integers or longs.  Needs to be used in place of CB/OFF whereever
// a pointer is also stored.
//
#if defined(_M_IA64) || defined(_M_ALPHA64) || defined(_M_AMD64) || defined(_M_ARM64)
typedef unsigned __int64    uint_ptr_t;
typedef __int64             int_ptr_t;
typedef unsigned __int64    ulong_ptr_t;
typedef __int64             long_ptr_t;
#else
typedef unsigned int        uint_ptr_t;
typedef int                 int_ptr_t;
typedef unsigned long       ulong_ptr_t;
typedef long                long_ptr_t;
#endif

typedef uint_ptr_t *        puint_ptr_t;
typedef int_ptr_t *         pint_ptr_t;
typedef ulong_ptr_t *       pulong_ptr_t;
typedef long_ptr_t *        plong_ptr_t;

typedef USHORT	IFILE;		// file index
typedef USHORT	IMOD;		// module index
typedef USHORT	ISECT;		// section index
typedef USHORT	LINE;		// line number
typedef ULONG   LINE32;         // long line number
typedef USHORT	HASH;		// short hash
typedef ULONG	LHASH;		// long hash

// TFS #498256 -- We used to define a local buffer with length _MAX_PATH
// and use it as dest buffer to call _wmakepath().  When the combined length
// of drive, path, filename, and extension is larger than _MAX_PATH, Watson
// would be invoked and mspdbsrv.exe would then crash.  So we define a large
// enough size for the dest buffer to prevent Watson from being invoked and
// mspdbsrv.exe from crashing.
#define PDB_MAX_FILENAME (_MAX_DRIVE + _MAX_PATH + _MAX_FNAME + _MAX_EXT + 1)
