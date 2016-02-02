// PDB Debug Information (DBI) API Implementation

#ifndef __DBIIMPL_INCLUDED__
#define __DBIIMPL_INCLUDED__

#ifndef __PDBIMPL_INCLUDED__
#include "pdbimpl.h"
#endif

#include "map.h"

#ifndef __MSF_INCLUDED__
#define MSF_IMP
#include "..\include\msf.h"
#endif
#ifndef __CVR_INCLUDED__
#define CVR_IMP
#include <cvr.h>
#endif

#include "nmt.h"


struct HDR;
struct REC;
#ifdef PDB_DEFUNCT_V8
struct C8REC;
#endif
struct CHN;
struct DBI1;
struct MODI;			// module information
struct TPI1;
struct Mod1;
struct GSI1;
struct PSGSI1;
class TM;				// abstract type map
 class TMTS;			// type map for modules which use a different TypeServer
 class TMR;				// type map for module with type records
  class TMPCT;			// type map for a PCT module
struct OTM;				// DBI1 helper to find some currently Open TM
class Strm;
typedef REC *PREC;
#ifdef PDB_DEFUNCT_V8
typedef C8REC UNALIGNED * PC8REC;
#endif
typedef CHN *PCHN;
typedef SYMTYPE* PSYM;
typedef TYPTYPE* PTYPE;
typedef USHORT CBREC;
typedef USHORT IFILE;
typedef long ICH;

#ifndef __MLI_INCLUDED__
#include "mli.h"
#endif

#define isectNil 	((ISECT)-1)			

const TI	tiMin		= 0x1000;

// Reserve 28-bits worth of type indices.  256 Meg entries should be plenty for anybody!
const TI        tiMax    = 0x0fffffff;

const TI16      ti16Max  = 0xfff0;
const CB        cbRecMax = 0xff00;
const SN        snPDB = 1;
const SN        snTpi = 2;
const SN        snDbi = 3;
const SN        snIpi = 4;
const SN	snSpecialMax = 5;


#include "ref.h"
#include "nmtni.h"
#include "pdbcommon.h"
#include "pdb1.h"
#include "stream.h"
#if CC_LAZYSYMS
#include "objfile.h"
#endif
#include <delayimp.h>
#include "dbicommon.h"
#include "dbi.h"
#include "modcommon.h"
#include "mod.h"
#include "gsi.h"
#include "udtrefs.h"
#include "tm.h"
#include "tpi.h"
#include "util.h"
#include "misc.h"

#endif // !__DBI_INCLUDED__
