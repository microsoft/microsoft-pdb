#pragma once

#include <version.h>
#define _VERSION_H_INCL

#include "verstamp.h"

///////////////////////////////////////////
//
// Set up the defines for use in common.ver
//

// defines for the VS_VERSION block

#define VER_FILESUBTYPE                 VFT2_UNKNOWN
#define VER_FILEDESCRIPTION_STR         "Microsoft\256 Program Database"

#ifdef PDB_CORE
#define VER_FILETYPE                    VFT_DLL
#define VER_INTERNALNAME_STR            "MSPDBCORE.DLL"
#else
#ifdef PDB_SRVEXE
#define VER_FILETYPE                    VFT_APP
#define VER_INTERNALNAME_STR            "MSPDBSRV.EXE"
#else
#define VER_FILETYPE                    VFT_DLL
#define VER_INTERNALNAME_STR            "MSPDB" RMJ_AS_FILENAME_STRING RMM_AS_FILENAME_STRING ".DLL"
#endif
#endif
