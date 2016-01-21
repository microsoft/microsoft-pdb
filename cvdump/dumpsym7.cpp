/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

#include "cvdump.h"

#define STRICT
#include "windows.h"

#include "wtypes.h"
#include "oleauto.h"

#include "utf8.h"

#include "armregs.h"
#include "arm64regs.h"

#ifndef UNALIGNED

#ifdef  _M_IX86
#define UNALIGNED
#else
#define UNALIGNED __unaligned
#endif

#endif

#pragma warning(disable:4069)  // Disable warning about long double same as double

const wchar_t * const C7HLSLRegTypeStrings[] =
{
    L"TEMP",  
    L"INPUT",  
    L"OUTPUT",  
    L"INDEXABLE_TEMP",  
    L"IMMEDIATE32",  
    L"IMMEDIATE64",  
    L"SAMPLER",  
    L"RESOURCE",  
    L"CONSTANT_BUFFER",  
    L"IMMEDIATE_CONSTANT_BUFFER",  
    L"LABEL", 
    L"INPUT_PRIMITIVEID", 
    L"OUTPUT_DEPTH", 
    L"NULL", 
    L"RASTERIZER", 
    L"OUTPUT_COVERAGE_MASK", 
    L"STREAM", 
    L"FUNCTION_BODY", 
    L"FUNCTION_TABLE", 
    L"INTERFACE", 
    L"FUNCTION_INPUT", 
    L"FUNCTION_OUTPUT", 
    L"OUTPUT_CONTROL_POINT_ID", 
    L"INPUT_FORK_INSTANCE_ID", 
    L"INPUT_JOIN_INSTANCE_ID", 
    L"INPUT_CONTROL_POINT", 
    L"OUTPUT_CONTROL_POINT", 
    L"INPUT_PATCH_CONSTANT", 
    L"INPUT_DOMAIN_POINT", 
    L"THIS_POINTER", 
    L"UNORDERED_ACCESS_VIEW", 
    L"THREAD_GROUP_SHARED_MEMORY", 
    L"INPUT_THREAD_ID", 
    L"INPUT_THREAD_GROUP_ID", 
    L"INPUT_THREAD_ID_IN_GROUP", 
    L"INPUT_COVERAGE_MASK", 
    L"INPUT_THREAD_ID_IN_GROUP_FLATTENED",
    L"INPUT_GS_INSTANCE_ID", 
    L"OUTPUT_DEPTH_GREATER_EQUAL", 
    L"OUTPUT_DEPTH_LESS_EQUAL", 
    L"CYCLE_COUNTER" 
};

void C7EndArgs                    (const void *);
void C7EntrySym                   (const void *);
void C7SkipSym                    (const SYMTYPE *);
void C7EndSym                     (const void *);
void C7EndSym2                    (const wchar_t *);
void C7RegSym                     (const REGSYM *pSym);
void C7ConSym                     (const CONSTSYM *);
void C7ManConSym                  (const CONSTSYM *);
void C7UDTSym                     (const UDTSYM *);
void C7CobolUDT                   (const UDTSYM *);
void C7RefSym                     (const REFSYM *);
void C7RefSym2                    (const REFSYM2 *);
void C7AnnotationRefSym           (const REFSYM2 *);
void C7RefMiniPDB                 (const REFMINIPDB *);
void C7PDBMap                     (const PDBMAP *);
void C7TokenRefSym                (const REFSYM2 *);
void C7ObjNameSym                 (const OBJNAMESYM *);
void C7StartSearchSym             (const SEARCHSYM *);
void C7CompileFlags               (const CFLAGSYM *);
void C7Data16Sym                  (const DATASYM16 *, const wchar_t *);
void C7Data32Sym                  (const DATASYM32 *, const wchar_t *);
void C7DataHLSLSym                (const DATASYMHLSL *, const wchar_t *);
void C7DataHLSL32Sym              (const DATASYMHLSL32 *, const wchar_t *);
void C7DataHLSL32ExSym            (const DATASYMHLSL32_EX *, const wchar_t *);
void C7Proc16Sym                  (const PROCSYM16 *, const wchar_t *);
void C7Proc32Sym                  (const PROCSYM32 *, const wchar_t *, bool);
void C7ProcMipsSym                (const PROCSYMMIPS *, const wchar_t *, bool);
void C7ProcIa64Sym                (const PROCSYMIA64 *, const wchar_t *);
void C7Slink32                    (const SLINK32 *);
void C7FrameProc                  (const FRAMEPROCSYM *);
void C7Compile2                   (const COMPILESYM *);
void C7Compile3                   (const COMPILESYM3 *);
void C7Envblock                   (const ENVBLOCKSYM *);
void C7Slot                       (const SLOTSYM32 *);
void C7Annotation                 (const ANNOTATIONSYM *);
void C7OemSym                     (const OEMSYMBOL *);

void C7BpRel16Sym                 (const BPRELSYM16 *);
void C7LProc16Sym                 (const PROCSYM16 *);
void C7GProc16Sym                 (const PROCSYM16 *);
void C7LData16Sym                 (const DATASYM16 *);
void C7GData16Sym                 (const DATASYM16 *);
void C7Public16Sym                (const DATASYM16 *);
void C7Thunk16Sym                 (const THUNKSYM16 *);
void C7Block16Sym                 (const BLOCKSYM16 *);
void C7With16Sym                  (const BLOCKSYM16 *);
void C7Lab16Sym                   (const LABELSYM16 *);
void C7ChangeModel16Sym           (const CEXMSYM16 *);

void C7BpRel32Sym                 (const BPRELSYM32 *);
void C7LProc32Sym                 (const PROCSYM32 *);
void C7LProc32IdSym               (const PROCSYM32 *);
void C7GProc32Sym                 (const PROCSYM32 *);
void C7GProc32IdSym               (const PROCSYM32 *);
void C7LData32Sym                 (const DATASYM32 *);
void C7GData32Sym                 (const DATASYM32 *);
void C7TLData32Sym                (const DATASYM32 *);
void C7TGData32Sym                (const DATASYM32 *);
void C7LDataHLSLSym               (const DATASYMHLSL *);
void C7LDataHLSL32Sym             (const DATASYMHLSL32 *);
void C7LDataHLSL32ExSym           (const DATASYMHLSL32_EX *);
void C7GDataHLSLSym               (const DATASYMHLSL *);
void C7GDataHLSL32Sym             (const DATASYMHLSL32 *);
void C7GDataHLSL32ExSym           (const DATASYMHLSL32_EX *);
void C7Public32Sym                (const PUBSYM32 *);
void C7Thunk32Sym                 (const THUNKSYM32 *);
void C7Block32Sym                 (const BLOCKSYM32 *);
void C7With32Sym                  (const BLOCKSYM32 *);
void C7Lab32Sym                   (const LABELSYM32 *);
void C7ChangeModel32Sym           (const CEXMSYM32 *);
void C7AlignSym                   (const ALIGNSYM *);
void C7RegRel32Sym                (const REGREL32 *);
void C7RegRel16Sym                (const REGREL16 *);

void C7LProcMipsSym               (const PROCSYMMIPS *);
void C7LProcMipsIdSym             (const PROCSYMMIPS *);
void C7GProcMipsSym               (const PROCSYMMIPS *);
void C7GProcMipsIdSym             (const PROCSYMMIPS *);

void C7LProcIa64Sym               (const PROCSYMIA64 *);
void C7LProcIa64IdSym             (const PROCSYMIA64 *);
void C7GProcIa64Sym               (const PROCSYMIA64 *);
void C7GProcIa64IdSym             (const PROCSYMIA64 *);

void SymHash32                    (const OMFSymHash *, const OMFDirEntry *);
void SymHash32Long                (const OMFSymHash *, const OMFDirEntry *);
void AddrHash32                   (const OMFSymHash *, const OMFDirEntry *, int iHash);

void AddrHash32NB09               (const OMFSymHash *, const OMFDirEntry *);

void C7RegSym_16t                 (const REGSYM_16t *);
void C7ConSym_16t                 (const CONSTSYM_16t *);
void C7UDTSym_16t                 (const UDTSYM_16t *);
void C7CobolUDT_16t               (const UDTSYM_16t *);
void C7Data32Sym_16t              (const DATASYM32_16t *, const wchar_t *);
void C7Proc32Sym_16t              (const PROCSYM32_16t *, const wchar_t *);
void C7ProcMipsSym_16t            (const PROCSYMMIPS_16t *, const wchar_t *);
void C7BpRel32Sym_16t             (const BPRELSYM32_16t *);
void C7LProc32Sym_16t             (const PROCSYM32_16t *);
void C7GProc32Sym_16t             (const PROCSYM32_16t *);
void C7LData32Sym_16t             (const DATASYM32_16t *);
void C7GData32Sym_16t             (const DATASYM32_16t *);
void C7TLData32Sym_16t            (const DATASYM32_16t *);
void C7TGData32Sym_16t            (const DATASYM32_16t *);
void C7Public32Sym_16t            (const PUBSYM32_16t *);
void C7RegRel32Sym_16t            (const REGREL32_16t *);

void C7GProcMipsSym_16t           (const PROCSYMMIPS_16t *);
void C7LProcMipsSym_16t           (const PROCSYMMIPS_16t *);

void C7ManRegister                (const ATTRREGSYM *);
// void C7ManManyReg                 (const ATTRMANYREGSYM *);
// void C7ManManyReg2                (const ATTRMANYREGSYM2 *);
void C7ManTypeRef                 (const MANTYPREF *);
void C7ManFrameRel                (const FRAMERELSYM *);
void C7ManSlotSym                 (const ATTRSLOTSYM *);
void C7ManRegRel                  (const ATTRREGREL *);
void C7UNameSpace                 (const UNAMESPACE *);
void C7LManProc                   (const MANPROCSYM *);
void C7GManProc                   (const MANPROCSYM *);
void C7LManData                   (const DATASYM32 *);
void C7GManData                   (const DATASYM32 *);
void C7TrampolineSym              (const TRAMPOLINESYM *);

void C7AttrFrameRel               (const ATTRFRAMERELSYM *);
void C7AttrRegister               (const ATTRREGSYM *);
void C7AttrRegRel                 (const ATTRREGRELSYM *);
void C7AttrManyReg                (const ATTRMANYREGSYM2 *);

void C7SeparatedCode              (const SEPCODESYM *);
void C7Local                      (const LOCALSYM *);
void C7FileStatic                 (const FILESTATICSYM *);
void C7DefRange                   (const DEFRANGESYM *);
void C7DefRange2                  (const DEFRANGESYMSUBFIELD *);
void C7DefRangeHLSL               (const DEFRANGESYMHLSL *);

#if defined(CC_DP_CXX)
void C7LProc32Sym_DPC             (const PROCSYM32 *);
void C7LProc32IdSym_DPC           (const PROCSYM32 *);
void C7LocalDPCGroupShared        (const LOCALDPCGROUPSHAREDSYM *);
void C7DPCSymTagMap               (const DPCSYMTAGMAP *);
#endif // CC_DP_CXX

void C7ArmSwitchTable             (const ARMSWITCHTABLE *);
void C7Callees                    (const FUNCTIONLIST *);
void C7Callers                    (const FUNCTIONLIST *);
void C7PogoData                   (const POGOINFO *);



void C7BuildInfo                  (const BUILDINFOSYM *);
void C7InlineSite                 (const INLINESITESYM *);
void C7InlineSite2                (const INLINESITESYM2 *);
void C7InlineSiteEnd              (const void *);
void C7ProcIdEnd                  (const void *);

void C7Section                    (const SECTIONSYM *);
void C7CoffGroup                  (const COFFGROUPSYM *);
void C7Export                     (const EXPORTSYM *);

void C7CallSiteInfo               (const CALLSITEINFO *);
void C7HeapAllocSite              (const HEAPALLOCSITE *);
void C7FrameCookie                (const FRAMECOOKIE *);
void C7Discarded                  (const DISCARDEDSYM *);
void C7ModTypeRef                 (const MODTYPEREF *);

extern WORD rect;                      // Type of symbol record

DWORD  SymOffset;
Mod *pmodSym;
Mod *pmodCache;
DWORD *rgfileidCache;
size_t cfileidCache;
bool fSkip;

WORD CVDumpMachineType = 0xFFFF;       // which string tables to use

struct SYMFCN
{
    unsigned rectyp;
    void (*pfcn)(const void *);
};

SYMFCN SymFcnC7[] =
{
    { S_END,                (void (*) (const void *)) C7EndSym          },
    { S_ENDARG,             (void (*) (const void *)) C7EndArgs         },
    { S_REGISTER,           (void (*) (const void *)) C7RegSym          },
    { S_CONSTANT,           (void (*) (const void *)) C7ConSym          },
    { S_SKIP,               (void (*) (const void *)) C7SkipSym         },
    { S_UDT,                (void (*) (const void *)) C7UDTSym          },
    { S_OBJNAME,            (void (*) (const void *)) C7ObjNameSym      },
    { S_COBOLUDT,           (void (*) (const void *)) C7CobolUDT        },

// 16 bit specific
    { S_BPREL16,            (void (*) (const void *)) C7BpRel16Sym      },
    { S_LDATA16,            (void (*) (const void *)) C7LData16Sym      },
    { S_GDATA16,            (void (*) (const void *)) C7GData16Sym      },
    { S_LPROC16,            (void (*) (const void *)) C7LProc16Sym      },
    { S_GPROC16,            (void (*) (const void *)) C7GProc16Sym      },
    { S_PUB16,              (void (*) (const void *)) C7Public16Sym     },
    { S_THUNK16,            (void (*) (const void *)) C7Thunk16Sym      },
    { S_BLOCK16,            (void (*) (const void *)) C7Block16Sym      },
    { S_WITH16,             (void (*) (const void *)) C7With16Sym       },
    { S_LABEL16,            (void (*) (const void *)) C7Lab16Sym        },
    { S_CEXMODEL16,         (void (*) (const void *)) C7ChangeModel16Sym},
    { S_REGREL16,           (void (*) (const void *)) C7RegRel16Sym     },

// 32 bit specific
    { S_BPREL32,            (void (*) (const void *)) C7BpRel32Sym      },
    { S_LDATA32,            (void (*) (const void *)) C7LData32Sym      },
    { S_GDATA32,            (void (*) (const void *)) C7GData32Sym      },
    { S_LDATA_HLSL,         (void (*) (const void *)) C7LDataHLSLSym    },
    { S_GDATA_HLSL,         (void (*) (const void *)) C7GDataHLSLSym    },
    { S_LDATA_HLSL32,       (void (*) (const void *)) C7LDataHLSL32Sym  },
    { S_GDATA_HLSL32,       (void (*) (const void *)) C7GDataHLSL32Sym  },
    { S_LDATA_HLSL32_EX,    (void (*) (const void *)) C7LDataHLSL32ExSym},
    { S_GDATA_HLSL32_EX,    (void (*) (const void *)) C7GDataHLSL32ExSym},
    { S_LPROC32,            (void (*) (const void *)) C7LProc32Sym      },
    { S_LPROC32_ID,         (void (*) (const void *)) C7LProc32IdSym    },
    { S_GPROC32,            (void (*) (const void *)) C7GProc32Sym      },
    { S_GPROC32_ID,         (void (*) (const void *)) C7GProc32IdSym    },
    { S_PUB32,              (void (*) (const void *)) C7Public32Sym     },
    { S_THUNK32,            (void (*) (const void *)) C7Thunk32Sym      },
    { S_BLOCK32,            (void (*) (const void *)) C7Block32Sym      },
    { S_WITH32,             (void (*) (const void *)) C7With32Sym       },
    { S_LABEL32,            (void (*) (const void *)) C7Lab32Sym        },
    { S_REGREL32,           (void (*) (const void *)) C7RegRel32Sym     },
    { S_CEXMODEL32,         (void (*) (const void *)) C7ChangeModel32Sym},
    { S_LPROCMIPS,          (void (*) (const void *)) C7LProcMipsSym    },
    { S_LPROCMIPS_ID,       (void (*) (const void *)) C7LProcMipsIdSym  },
    { S_GPROCMIPS,          (void (*) (const void *)) C7GProcMipsSym    },
    { S_GPROCMIPS_ID,       (void (*) (const void *)) C7GProcMipsIdSym  },
    { S_LPROCIA64,          (void (*) (const void *)) C7LProcIa64Sym    },
    { S_LPROCIA64_ID,       (void (*) (const void *)) C7LProcIa64IdSym  },
    { S_GPROCIA64,          (void (*) (const void *)) C7GProcIa64Sym    },
    { S_GPROCIA64_ID,       (void (*) (const void *)) C7GProcIa64IdSym  },
    { S_LTHREAD32,          (void (*) (const void *)) C7TLData32Sym     },
    { S_GTHREAD32,          (void (*) (const void *)) C7TGData32Sym     },
    { S_SLINK32,            (void (*) (const void *)) C7Slink32         },
    { S_FRAMEPROC,          (void (*) (const void *)) C7FrameProc       },
    { S_COMPILE2,           (void (*) (const void *)) C7Compile2        },
    { S_LOCALSLOT,          (void (*) (const void *)) C7Slot            },
    { S_PARAMSLOT,          (void (*) (const void *)) C7Slot            },
    { S_ANNOTATION,         (void (*) (const void *)) C7Annotation      },

    { S_SSEARCH,            (void (*) (const void *)) C7StartSearchSym  },
    { S_COMPILE,            (void (*) (const void *)) C7CompileFlags    },
    { S_PROCREF,            (void (*) (const void *)) C7RefSym2         },
    { S_DATAREF,            (void (*) (const void *)) C7RefSym2         },
    { S_LPROCREF,           (void (*) (const void *)) C7RefSym2         },
    { S_ANNOTATIONREF,      (void (*) (const void *)) C7AnnotationRefSym},
    { S_REF_MINIPDB,        (void (*) (const void *)) C7RefMiniPDB      },
    { S_PDBMAP,             (void (*) (const void *)) C7PDBMap          },

    { S_REGISTER_16t,       (void (*) (const void *)) C7RegSym_16t      },
    { S_CONSTANT_16t,       (void (*) (const void *)) C7ConSym_16t      },
    { S_UDT_16t,            (void (*) (const void *)) C7UDTSym_16t      },
    { S_COBOLUDT_16t,       (void (*) (const void *)) C7CobolUDT_16t    },
//  { S_MANYREG_16t,        (void (*) (const void *)) C7ManyReg_16t     },
    { S_BPREL32_16t,        (void (*) (const void *)) C7BpRel32Sym_16t  },
    { S_LDATA32_16t,        (void (*) (const void *)) C7LData32Sym_16t  },
    { S_GDATA32_16t,        (void (*) (const void *)) C7GData32Sym_16t  },
    { S_PUB32_16t,          (void (*) (const void *)) C7Public32Sym_16t },
    { S_GPROC32_16t,        (void (*) (const void *)) C7GProc32Sym_16t  },
    { S_LPROC32_16t,        (void (*) (const void *)) C7LProc32Sym_16t  },

    { S_REGREL32_16t,       (void (*) (const void *)) C7RegRel32Sym_16t },
    { S_LTHREAD32_16t,      (void (*) (const void *)) C7TLData32Sym_16t },
    { S_GTHREAD32_16t,      (void (*) (const void *)) C7TGData32Sym_16t },
    { S_GPROCMIPS_16t,      (void (*) (const void *)) C7LProcMipsSym_16t},
    { S_LPROCMIPS_16t,      (void (*) (const void *)) C7GProcMipsSym_16t},

// Managed symbols

    { S_GMANPROC,           (void (*) (const void *)) C7GManProc        },
    { S_LMANPROC,           (void (*) (const void *)) C7LManProc        },
    { S_LMANDATA,           (void (*) (const void *)) C7LManData        },
    { S_GMANDATA,           (void (*) (const void *)) C7GManData        },
    { S_MANFRAMEREL,        (void (*) (const void *)) C7ManFrameRel     },
    { S_MANREGISTER,        (void (*) (const void *)) C7ManRegister     },
    { S_MANSLOT,            (void (*) (const void *)) C7ManSlotSym      },
    { S_MANREGREL,          (void (*) (const void *)) C7ManRegRel       },
    { S_MANTYPREF,          (void (*) (const void *)) C7ManTypeRef      },
    { S_UNAMESPACE,         (void (*) (const void *)) C7UNameSpace      },
    { S_MANCONSTANT,        (void (*) (const void *)) C7ManConSym       },

// ST symbols
    { S_REGISTER_ST,        (void (*) (const void *)) C7RegSym          },
    { S_CONSTANT_ST,        (void (*) (const void *)) C7ConSym          },
    { S_UDT_ST,             (void (*) (const void *)) C7UDTSym          },
    { S_OBJNAME_ST,         (void (*) (const void *)) C7ObjNameSym      },
    { S_COBOLUDT_ST,        (void (*) (const void *)) C7CobolUDT        },
    { S_BPREL32_ST,         (void (*) (const void *)) C7BpRel32Sym      },
    { S_LDATA32_ST,         (void (*) (const void *)) C7LData32Sym      },
    { S_GDATA32_ST,         (void (*) (const void *)) C7GData32Sym      },
    { S_LPROC32_ST,         (void (*) (const void *)) C7LProc32Sym      },
    { S_GPROC32_ST,         (void (*) (const void *)) C7GProc32Sym      },
    { S_PUB32_ST,           (void (*) (const void *)) C7Public32Sym     },
    { S_THUNK32_ST,         (void (*) (const void *)) C7Thunk32Sym      },
    { S_BLOCK32_ST,         (void (*) (const void *)) C7Block32Sym      },
    { S_WITH32_ST,          (void (*) (const void *)) C7With32Sym       },
    { S_LABEL32_ST,         (void (*) (const void *)) C7Lab32Sym        },
    { S_REGREL32_ST,        (void (*) (const void *)) C7RegRel32Sym     },
    { S_LPROCMIPS_ST,       (void (*) (const void *)) C7LProcMipsSym    },
    { S_GPROCMIPS_ST,       (void (*) (const void *)) C7GProcMipsSym    },
    { S_LPROCIA64_ST,       (void (*) (const void *)) C7LProcIa64Sym    },
    { S_GPROCIA64_ST,       (void (*) (const void *)) C7GProcIa64Sym    },
    { S_LTHREAD32_ST,       (void (*) (const void *)) C7TLData32Sym     },
    { S_GTHREAD32_ST,       (void (*) (const void *)) C7TGData32Sym     },
    { S_COMPILE2_ST,        (void (*) (const void *)) C7Compile2        },
    { S_PROCREF_ST,         (void (*) (const void *)) C7RefSym          },
    { S_DATAREF_ST,         (void (*) (const void *)) C7RefSym          },
    { S_LPROCREF_ST,        (void (*) (const void *)) C7RefSym          },
    { S_OEM,                (void (*) (const void *)) C7OemSym          },
    { S_LOCALSLOT_ST,       (void (*) (const void *)) C7Slot            },
    { S_PARAMSLOT_ST,       (void (*) (const void *)) C7Slot            },

    { S_GMANPROC_ST,        (void (*) (const void *)) C7GManProc        },
    { S_LMANPROC_ST ,       (void (*) (const void *)) C7LManProc        },
    { S_LMANDATA_ST,        (void (*) (const void *)) C7LManData        },
    { S_GMANDATA_ST,        (void (*) (const void *)) C7GManData        },

    { S_MANFRAMEREL_ST,     (void (*) (const void *)) C7ManFrameRel     },
    { S_MANREGISTER_ST,     (void (*) (const void *)) C7ManRegister     },
    { S_MANSLOT_ST,         (void (*) (const void *)) C7ManSlotSym      },
    { S_MANREGREL_ST,       (void (*) (const void *)) C7ManRegRel       },
    { S_UNAMESPACE_ST,      (void (*) (const void *)) C7UNameSpace      },
    { S_TOKENREF,           (void (*) (const void *)) C7TokenRefSym     },
    { S_TRAMPOLINE,         (void (*) (const void *)) C7TrampolineSym   },

    { S_ATTR_FRAMEREL,      (void (*) (const void *)) C7AttrFrameRel    },
    { S_ATTR_REGISTER,      (void (*) (const void *)) C7AttrRegister    },
    { S_ATTR_REGREL,        (void (*) (const void *)) C7AttrRegRel      },
    { S_ATTR_MANYREG,       (void (*) (const void *)) C7AttrManyReg     },

    { S_SEPCODE,            (void (*) (const void *)) C7SeparatedCode   },
    { S_LOCAL,              (void (*) (const void *)) C7Local           },
    { S_FILESTATIC,         (void (*) (const void *)) C7FileStatic      },
    { S_DEFRANGE,           (void (*) (const void *)) C7DefRange        },
    { S_DEFRANGE_REGISTER,  (void (*) (const void *)) C7DefRange        },

    { S_DEFRANGE_FRAMEPOINTER_REL,
                            (void (*) (const void *)) C7DefRange        },
    
    { S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE,  
                            (void (*) (const void *)) C7DefRange        },
    
    { S_DEFRANGE_SUBFIELD,  (void (*) (const void *)) C7DefRange2       },
    
    { S_DEFRANGE_SUBFIELD_REGISTER,
                            (void (*) (const void *)) C7DefRange2       },
    
    { S_DEFRANGE_REGISTER_REL,
                            (void (*) (const void *)) C7DefRange2       },

    { S_DEFRANGE_HLSL,      (void (*) (const void *)) C7DefRangeHLSL    },
    
#if defined(CC_DP_CXX)
    { S_LOCAL_DPC_GROUPSHARED,
                            (void (*) (const void *)) C7LocalDPCGroupShared  },
    { S_LPROC32_DPC,        (void (*) (const void *)) C7LProc32Sym_DPC  },
    { S_LPROC32_DPC_ID,     (void (*) (const void *)) C7LProc32IdSym_DPC},
    { S_DEFRANGE_DPC_PTR_TAG,
                            (void (*) (const void *)) C7DefRangeHLSL    },
    { S_DPC_SYM_TAG_MAP,    (void (*) (const void *)) C7DPCSymTagMap    },
#endif // CC_DP_CXX
    { S_ARMSWITCHTABLE,     (void (*) (const void *)) C7ArmSwitchTable  },
    { S_CALLEES,            (void (*) (const void *)) C7Callees         },
    { S_CALLERS,            (void (*) (const void *)) C7Callers         },
    { S_POGODATA,           (void (*) (const void *)) C7PogoData        },

    { S_BUILDINFO,          (void (*) (const void *)) C7BuildInfo       },
    { S_INLINESITE,         (void (*) (const void *)) C7InlineSite      },
    { S_INLINESITE2,        (void (*) (const void *)) C7InlineSite2     },    
    { S_INLINESITE_END,     (void (*) (const void *)) C7InlineSiteEnd   },
    { S_PROC_ID_END,        (void (*) (const void *)) C7ProcIdEnd       },
    
    { S_SECTION,            (void (*) (const void *)) C7Section         },
    { S_COFFGROUP,          (void (*) (const void *)) C7CoffGroup       },
    { S_EXPORT,             (void (*) (const void *)) C7Export          },

    { S_CALLSITEINFO,       (void (*) (const void *)) C7CallSiteInfo    },
    { S_HEAPALLOCSITE,      (void (*) (const void *)) C7HeapAllocSite   },
    { S_FRAMECOOKIE,        (void (*) (const void *)) C7FrameCookie     },

    { S_DISCARDED,          (void (*) (const void *)) C7Discarded       },
    { S_COMPILE3,           (void (*) (const void *)) C7Compile3        },
    { S_ENVBLOCK,           (void (*) (const void *)) C7Envblock        },
    { S_MOD_TYPEREF,        (void (*) (const void *)) C7ModTypeRef      },
};

#define SYMCNT (sizeof(SymFcnC7) / sizeof(SymFcnC7[0]))


int __cdecl CmpSymFcn(const void *pv1, const void *pv2)
{
    const SYMFCN *ps1 = (const SYMFCN *) pv1;
    const SYMFCN *ps2 = (const SYMFCN *) pv2;

    unsigned rectyp1 = ps1->rectyp;
    unsigned rectyp2 = ps2->rectyp;

    if (rectyp1 < rectyp2) {
        return -1;
    }

    if (rectyp1 > rectyp2) {
        return 1;
    }

    return 0;
}


bool FSortSymFcn()
{
    qsort((void *) SymFcnC7, SYMCNT, sizeof(SymFcnC7[0]), CmpSymFcn);

    return true;
}


bool fUseBsearch = FSortSymFcn();


void DumpOneSymC7(Mod *pmod, const void *pvSym, DWORD ibSym)
{
    pmodSym = pmod;
    SymOffset = ibSym;

    unsigned rectyp = ((SYMTYPE *) pvSym)->rectyp;
    unsigned i = 0;

    if (fUseBsearch) {
        SYMFCN s = { rectyp, NULL };

        SYMFCN *ps = (SYMFCN *) bsearch(&s,
                                        (const void *) SymFcnC7,
                                        SYMCNT,
                                        sizeof(SymFcnC7[0]),
                                        CmpSymFcn);

        if (ps != NULL) {
            ps->pfcn(pvSym);
        }
        else {
            i = SYMCNT;
        }
    }
    else {
        for (i = 0; i < SYMCNT; i++) {
            if (SymFcnC7[i].rectyp == rectyp) {
                SymFcnC7[i].pfcn(pvSym);
                break;
            }
        }
    }

    if (i == SYMCNT) {
        StdOutPrintf(L"Error: unknown symbol record type %04X!\n\n", rectyp);
    }
}


void DumpModSymC7(size_t cbSymSeg, DWORD ibInitial)
{
    BYTE SymBuf[65536];
    SYMTYPE *pSymType = (SYMTYPE *) SymBuf;

    DWORD ibSym = ibInitial;

    while (cbSymSeg > 0) {
        // Read record length

        cbRec = 2;
        GetBytes(SymBuf, 2);

        // Get record length

        cbRec = pSymType->reclen;

        if ((DWORD) (cbRec + 2) > cbSymSeg) {
            StdOutPrintf(L"cbSymSeg: %d\tcbRec: %d\tRecType: 0x%X\n", cbSymSeg, cbRec, pSymType->rectyp);
            Fatal(L"Overran end of symbol table");
        }

        cbSymSeg -= cbRec + sizeof(pSymType->reclen);

        GetBytes(SymBuf + 2, (size_t) pSymType->reclen);

        if (fRaw) {
            StdOutPrintf(L"(0x%04X) ", ibSym);

            for (size_t i = 0; i < pSymType->reclen + 2U; i++) {
                StdOutPrintf(L" %02x", SymBuf[i]);
            }

            StdOutPutc(L'\n');
        }

        if (!fStatics) {
            DumpOneSymC7(NULL, SymBuf, ibSym);
        } else {
            switch (pSymType->rectyp) {
                case S_GDATA32_ST :
                case S_LDATA32_ST :
                case S_GDATA16 :
                case S_LDATA16 :
                case S_GDATA32 :
                case S_LDATA32 :
                case S_GDATA32_16t :
                case S_LDATA32_16t :
                    DumpOneSymC7(NULL, SymBuf, ibSym);
                    break;
            }
        }

        ibSym += pSymType->reclen + sizeof(pSymType->reclen);
    }

    StdOutPutc(L'\n');
}


void DumpModStringTable(size_t cbTable, DWORD)
{
    BYTE buf[65537];

    buf[65536] = 0;

    size_t  offString = 0;

    while (cbTable > 0) {
        // Read record length

        size_t cbRead = min( cbTable, sizeof(buf));

        GetBytes(buf, cbRead);

        SZ utfszStart = reinterpret_cast<SZ>(buf);

        for(SZ utfsz = utfszStart; utfsz < utfszStart + cbRead; utfsz += strlen(utfsz) + 1) {
            StdOutPrintf(L"%08x ", offString);
            PrintSt(true, reinterpret_cast<unsigned char *>(utfsz));
            offString += strlen(utfsz) + 1;
        }

        cbTable -= cbRead;
    }

    StdOutPutc(L'\n');
}

void DumpModC13Lines(size_t cbTable, DWORD)
{
    struct {
        DWORD  offCon;
        WORD   segCon;
        WORD   flags;
        DWORD  cbCon; 
    } header;

    assert(cbTable >= sizeof(header));

    GetBytes(&header, sizeof(header));

    DWORD offMac = header.offCon + header.cbCon;

    bool fHasColumn = false;
    
    if (header.flags & CV_LINES_HAVE_COLUMNS) {
        fHasColumn = true;
    }

    StdOutPrintf(L"  %04X:%08X-%08X, flags = %04X",
                 header.segCon,
                 header.offCon,
                 offMac,
                 header.flags);

    bool fFirst = true;

    cbTable -= sizeof(header);

    while (cbTable > 0) {
        struct {
            DWORD fileid;
            DWORD nLines;
            DWORD cbFileBlock;
        } fileblock;

        // Read file block header

        assert(cbTable >= sizeof(fileblock));

        GetBytes(&fileblock, sizeof(fileblock));

        cbTable -= fileblock.cbFileBlock;

        if (fFirst) {
            fFirst = false;

            StdOutPutc(L',');
        }

        else {
            StdOutPutc(L'\n');
            StdOutPutc(L' ');
        }

        StdOutPrintf(L" fileid = %08X\n", fileblock.fileid);

        fileblock.cbFileBlock -= sizeof(fileblock);

        // Check whether file block size makes sense to the number of line records

        DWORD cbLineInfo = fileblock.nLines *
                           (sizeof(CV_Line_t) + (fHasColumn ? sizeof(CV_Column_t) : 0));

        if (cbLineInfo > fileblock.cbFileBlock) {
            Fatal(L"Incorrect line block header");
        }

        // Read in all line records and column records if any

        CV_Line_t *pLines = (CV_Line_t *) malloc(sizeof(CV_Line_t) * fileblock.nLines);

        if (pLines == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(pLines, sizeof(CV_Line_t) * fileblock.nLines);

        CV_Column_t *pColumns = NULL;

        if (fHasColumn) {
            pColumns = (CV_Column_t *) malloc(sizeof(CV_Column_t) * fileblock.nLines);

            if (pColumns == NULL) {
                Fatal(L"Out of memory");
            }

            GetBytes(pColumns, sizeof(CV_Column_t) * fileblock.nLines);
        }


        DWORD clinesOutofBounds = 0;
        unsigned i;

        for (i = 0; i < fileblock.nLines; i++) {
            CV_Line_t line = *(pLines + i);

            bool fSpecialLine = false;

            if ((line.linenumStart == 0xfeefee) || (line.linenumStart == 0xf00f00)) {
                fSpecialLine = true;
            }

            if (fHasColumn) {
                if ((i % 2) == 0) {
                    StdOutPutc(L'\n');
                }

                CV_Column_t column = *(pColumns + i);

                if (column.offColumnEnd != 0) {
                    StdOutPrintf(L"  %5u:%-5u-%5u:%-5u %08X",
                                 line.linenumStart,
                                 column.offColumnStart,
                                 line.linenumStart + line.deltaLineEnd,
                                 column.offColumnEnd,
                                 line.offset + header.offCon);
                } else {
                    StdOutPrintf(fSpecialLine
                                 ? L"  %x:%-5u            %08X"
                                 : L"  %5u:%-5u            %08X",
                                 line.linenumStart,
                                 column.offColumnStart,
                                 line.offset + header.offCon);
                }
            } else {
                if ((i % 4) == 0) {
                    StdOutPutc(L'\n');
                }

                StdOutPrintf(fSpecialLine ? L"  %x %08X" : L"  %5u %08X",
                             line.linenumStart,
                             line.offset + header.offCon);
            }

            if ((line.offset + header.offCon) >= offMac) {
                clinesOutofBounds++;
            }
        }

        free(pLines);

        if (fHasColumn) {
            free(pColumns);
        }

        if (i > 0) {
            StdOutPutc(L'\n');
        }

        if (clinesOutofBounds != 0) {
            StdOutPrintf(L"\n"
                         L" <<<< WARNING >>>> %u line/addr pairs are out of bounds!\n",
                         clinesOutofBounds);
        }
    }

    if (fFirst) {
        StdOutPutc(L'\n');
    }
}

void DumpModILLines(size_t cb, DWORD ib)
{
    DumpModC13Lines(cb, ib);
}

using namespace CodeViewInfo;

void DumpModInlineeSourceLines(size_t cb, PB pb)
{
    DWORD sig;

    if (pb == NULL) {
        GetBytes(&sig, sizeof(DWORD));
    }
    else {
        sig = *(DWORD *)pb;
        pb += sizeof(DWORD);
    }

    cb -= static_cast<DWORD>(sizeof(DWORD));

    if (sig == CV_INLINEE_SOURCE_LINE_SIGNATURE) {
        StdOutPrintf(L"InlineeId  FileId  StaringLine\n");

        InlineeSourceLine    inlSrcLine;
        InlineeSourceLine   *pInlSrcLine = &inlSrcLine;

        for (size_t i = 0; i < cb / sizeof(InlineeSourceLine); i++) {
            if (pb == NULL) {
                GetBytes(pInlSrcLine, sizeof(InlineeSourceLine));
            }
            else {
                pInlSrcLine = (InlineeSourceLine *) pb;
            }

            StdOutPrintf(L"%9X  %6X  %11u\n", pInlSrcLine->inlinee,
                         pInlSrcLine->fileId, pInlSrcLine->sourceLineNum);

            if (pb != NULL) {
                pb += sizeof(InlineeSourceLine);
            }
        }
    }
    else {
        StdOutPrintf(L"InlineeId  FileId  StaringLine  ExtraFileIDs\n");

        InlineeSourceLineEx  inlSrcLineEx;
        InlineeSourceLineEx *pInlSrcLineEx = &inlSrcLineEx;

        while (cb >= sizeof(InlineeSourceLineEx)) {
            if (pb == NULL) {
                GetBytes(pInlSrcLineEx, sizeof(InlineeSourceLineEx));
            }
            else {
                pInlSrcLineEx = (InlineeSourceLineEx *) pb;
            }

            StdOutPrintf(L"%9X  %6X  %11u ", pInlSrcLineEx->inlinee,
                         pInlSrcLineEx->fileId, pInlSrcLineEx->sourceLineNum);

            for (DWORD i = 0; i < pInlSrcLineEx->countOfExtraFiles; i++) {
                DWORD fileId;

                if (pb == NULL) {
                    GetBytes(&fileId, sizeof(DWORD));
                }
                else {
                    fileId = pInlSrcLineEx->extraFileId[i];
                }

                StdOutPrintf(L" %X", fileId);
            }

            StdOutPrintf(L"\n");

            size_t cbRec = sizeof(InlineeSourceLineEx) +
                           sizeof(CV_off32_t) * pInlSrcLineEx->countOfExtraFiles;
            if (pb != NULL) {
                pb += cbRec;
            }

            cb -= static_cast<DWORD>(cbRec);
        }
    }
}

void DumpModCrossScopeExports(size_t cb, PB pb)
{
    StdOutPrintf(L"LocalId   GlobalId\n");

    LocalIdAndGlobalIdPair  pair;
    LocalIdAndGlobalIdPair *pPair = &pair;

    for (size_t i = 0; i < cb / sizeof(LocalIdAndGlobalIdPair); i++) {
        if (pb == NULL) {
            GetBytes(pPair, sizeof(LocalIdAndGlobalIdPair));
        }
        else {
            pPair = (LocalIdAndGlobalIdPair *)pb;
        }

        StdOutPrintf(L"%8X  %8X\n", pPair->localId, pPair->globalId);

        if (pb != NULL) {
            pb += sizeof(LocalIdAndGlobalIdPair);
        }
    }
}

void DumpModCrossScopeRefs(size_t cb, PB pb)
{
    StdOutPrintf(L"ModId  Count  CrossReferences\n");

    CrossScopeReferences  ref;
    CrossScopeReferences *pRef = &ref;

    while (cb >= sizeof(CrossScopeReferences)) {
        if (pb == NULL) {
            GetBytes(pRef, sizeof(CrossScopeReferences));
        }
        else {
            pRef = (CrossScopeReferences *) pb;
        }

        StdOutPrintf(L"%5X  %5u ", pRef->externalScope,
                                   pRef->countOfCrossReferences);

        for (DWORD i = 0; i < pRef->countOfCrossReferences; i++) {
            CV_ItemId  id;
            CV_ItemId *pId = &id;

            if (pb == NULL) {
                GetBytes(pId, sizeof(CV_ItemId));
            }
            else {
                pId = &pRef->referenceIds[i];
            }

            StdOutPrintf(L" %08X", *pId);
        }

        StdOutPrintf(L"\n");

        if (pb != NULL) {
            pb += sizeof(CrossScopeReferences) +
                  sizeof(CV_ItemId) * pRef->countOfCrossReferences;
        }

        cb -= sizeof(CrossScopeReferences) +
              sizeof(CV_ItemId) * pRef->countOfCrossReferences;
    }
}

void DumpModTokenMap(size_t cb, PB pb, bool fType)
{
    PB pbBuf = NULL;

    if (pb == NULL) {
        pbBuf = new BYTE[cb];

        if (pbBuf == NULL) {
            StdOutPuts(L"new failed\n");
            return;
        }

        pb = pbBuf;
        GetBytes(pb, cb);
    }

    PB pbMax = pb + cb;
    DWORD cEntries = *(DWORD *)pb;

    pb += sizeof(DWORD);

    StdOutPrintf(L"  Total %u map entries.\n", cEntries);

    for (DWORD i = 0;
         i < cEntries;
         i++, pb += sizeof(DWORD) * 2) {
        DWORD dw1 = *(DWORD *)pb;
        DWORD dw2 = *(DWORD *)(pb + 4);

        if ((i % 3) == 0) {
            StdOutPuts(L"\n");
        }

        if ((dw2 >> 31) == 1) {
            StdOutPrintf(L"   %08X: tok=%08X", dw1, dw2 & 0x7FFFFFFF);
        } else {
            StdOutPrintf(L"   %08X: off=%08X", dw1, dw2);
        }
    }

    if (pb < pbMax) {
        StdOutPuts(L"\n\n  RAW DATA:\n");
    }

    DWORD offset = 0;

    while (pb < pbMax) {
        if ((offset % 16) == 0) {
            StdOutPrintf(L"\n   %08X:", offset);
        }

        StdOutPrintf(L" %02X", *pb);

        pb++;
        offset++;
    }

    if (pbBuf != NULL) {
        delete [] pbBuf;
    }
}

void DumpModFuncTokenMap(size_t cb, PB pb)
{
    DumpModTokenMap(cb, pb, false);
}

void DumpModTypeTokenMap(size_t cb, PB pb)
{
    DumpModTokenMap(cb, pb, true);
}

void DumpModMergedAssemblyInput(size_t cb, PB pb)
{
    PB pbBuf = NULL;

    if (pb == NULL) {
        pbBuf = new BYTE[cb];

        if (pbBuf == NULL) {
            StdOutPuts(L"new failed\n");
            return;
        }

        pb = pbBuf;
        GetBytes(pb, cb);
    }

    PB pbMax = pb + cb;

    while (pb < pbMax) {
        DWORD dwTimeStamp = *(DWORD *)pb;
        DWORD dwIndex = *(DWORD *)(pb + 4);
        bool fPDB = (dwIndex >> 31) != 0;

        if (fPDB) {
            dwIndex &= 0x7FFFFFFF;
        }

        const DWORD dwOffsetVersion = sizeof(DWORD) * 2;

        PB pbVersion = pb + dwOffsetVersion;
        WORD cbVer = *((WORD *) pbVersion);
        char *name = (char *) pb + dwOffsetVersion + cbVer;

        StdOutPrintf(L" Name:         %S\n"
                     L" Index:        %u\n"
                     L" TimeStamp:    %08X",
                     name, dwIndex, dwTimeStamp);

        const wchar_t *time;
        time_t tt = dwTimeStamp;

        if ((tt != 0x00000000) && ((time = _wctime(&tt)) != NULL)) {
            StdOutPrintf(L"  %s", time);
        } else {
            StdOutPutc(L'\n');
        }

        StdOutPrintf(L" PDB present:  %s\n"
                     L" Version (%u bytes):",
                     fPDB ? L"yes" : L"no", cbVer);

        for (WORD i = 0; i < cbVer - sizeof(WORD); i++) {
            if (i % 32 == 0) {
                StdOutPuts(L"\n ");
            }
            StdOutPrintf(L" %02X", *(pbVersion + sizeof(WORD) + i));
        }

        StdOutPuts(L"\n\n");

        DWORD cb = dwOffsetVersion + cbVer + static_cast<DWORD>(strlen(name) + 1);

        cb = (cb + 3) & 0xFFFFFFFC;
        pb += cb;
    }

    if (pbBuf != NULL) {
        delete [] pbBuf;
    }
}

void DumpModFileChecksums(size_t cb, DWORD)
{
#pragma pack(push, 1)
    struct {
        DWORD offstFileName;
        BYTE  cbChecksum;
        BYTE  ChecksumType;
    } filedata;
#pragma pack(pop)

    StdOutPrintf(L"FileId  St.Offset  Cb  Type  ChksumBytes\n");

    size_t cbBlob = cb;

    while (cb >= sizeof(filedata)) {
        GetBytes(&filedata, sizeof(filedata));

        StdOutPrintf(L"%6X  %08X   %02X  ",
                     cbBlob - cb,
                     filedata.offstFileName,
                     filedata.cbChecksum);

        switch (filedata.ChecksumType) {
            case CHKSUM_TYPE_NONE :
                StdOutPrintf(L"None");
                break;

            case CHKSUM_TYPE_MD5 :
                StdOutPrintf(L"MD5 ");
                break;

            case CHKSUM_TYPE_SHA1 :
                StdOutPrintf(L"SHA1");
                break;

            case CHKSUM_TYPE_SHA_256 :
                StdOutPrintf(L"SHA_256");
                break;

            default :
                StdOutPrintf(L"0x%02X", filedata.ChecksumType);
                break;
        }

        cb -= sizeof(filedata);

        BYTE checksum[255];

        size_t cbChecksum = min(filedata.cbChecksum, sizeof(checksum));

        if (cbChecksum != 0) {
            GetBytes(checksum, cbChecksum);

            cb -= cbChecksum;

            StdOutPrintf(L"  ");

            for (DWORD i = 0; i < cbChecksum; i++) {
                StdOutPrintf(L"%02X", checksum[i]);
            }
        }

        StdOutPrintf(L"\n");

        // Next record starts at next 4 byte boundary.

        size_t  cbExtra = (cbChecksum + sizeof(filedata)) % 4;

        if (cbExtra != 0) {
            size_t cbFiller = 4 - cbExtra;

            GetBytes(checksum, cbFiller);

            cb -= cbFiller;
        }
    }

    GetBytes(&filedata, cb);
}


void DumpModFramedata(size_t cb, DWORD)
{
    unsigned rva;

    GetBytes(&rva, sizeof(rva));

    cb -= sizeof(rva);

    // StdOutPrintf(L"RVACon = %08x\n", rva);

    FRAMEDATA data;

    StdOutPuts(L" Address  Blk Size   cbLocals cbParams cbStkMax cbProlog  cbSavedRegs SEH C++EH FStart  Program\n");

    while (cb >= sizeof(data)) {
        GetBytes(&data, sizeof(data));

        cb -= sizeof(data);

        StdOutPrintf(L"%08X   %8X %8X %8X %8X %8X %8X        %c   %c      %c     %08X\n",
                     data.ulRvaStart,
                     data.cbBlock,
                     data.cbLocals,
                     data.cbParams,
                     data.cbStkMax,
                     data.cbProlog,
                     data.cbSavedRegs,
                     data.fHasSEH ? L'Y' : L'N',
                     data.fHasEH ? L'Y' : L'N',
                     data.fIsFunctionStart ? L'Y' : L'N',
                     data.frameFunc);
    }

    GetBytes(&data, cb);
}


void DumpGlobal(const wchar_t *pszTitle, const OMFDirEntry *pDir)
{
    OMFSymHash hash;
    BYTE       SymBuf[65535];
    SYMTYPE    *pSymType = (SYMTYPE *) SymBuf;
    DWORD      cbSymbols;
    WORD       cb;
    DWORD      cbOff;

    StdOutPrintf(L"\n\n*** %s section\n", pszTitle);

    // Read Hash information
    _lseek(exefile, lfoBase + pDir->lfo, SEEK_SET);
    _read(exefile, &hash, sizeof(hash));

    cbOff = (Sig == SIG09 || Sig == SIG11) ? 0 : sizeof(hash);

    StdOutPrintf(L"\nSymbol hash function index = %d : ", hash.symhash);
    switch (hash.symhash) {
        case 0 :
            StdOutPuts(L"no hashing\n");
            break;

        case 1 :
            StdOutPrintf(L"sum of bytes, 16 bit addressing, 0x%lx\n", hash.cbHSym);
            break;

        case 2 :
            StdOutPrintf(L"sum of bytes, 32 bit addressing, 0x%lx\n", hash.cbHSym);
            break;

        case 5 :
            StdOutPrintf(L"shifted sum of bytes, 16 bit addressing, 0x%lx\n", hash.cbHSym);
            break;

        case 6 :
            StdOutPrintf(L"shifted sum of bytes, 32 bit addressing, 0x%lx\n", hash.cbHSym);
            break;

        case 10 :
            StdOutPrintf(L"xor shift of dwords (MSC 8), 32-bit addressing, 0x%lx\n", hash.cbHSym);
            break;

        default :
            StdOutPuts(L"unknown\n");
            break;
    }

    StdOutPrintf(L"\nAdress hash function index = %d : ", hash.addrhash);
    switch (hash.addrhash) {
        case 0 :
            StdOutPuts(L"no hashing\n");
            break;

        case 1 :
            StdOutPrintf(L"sum of bytes, 16 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 2 :
            StdOutPrintf(L"sum of bytes, 32 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 3 :
            StdOutPrintf(L"seg :off sort, 16 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 4 :
            StdOutPrintf(L"seg :off sort, 32 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 5 :
            StdOutPrintf(L"seg :off sort, 32 bit addressing - 32-bit aligned, 0x%lx\n", hash.cbHAddr);
            break;

        case 7 :
            StdOutPrintf(L"modified seg :off sort, 16 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 8 :
            StdOutPrintf(L"modified seg :off sort, 32 bit addressing, 0x%lx\n", hash.cbHAddr);
            break;

        case 12 :
            StdOutPrintf(L"seg :off grouped sort, 32 bit addressing - 32-bit aligned, 0x%lx\n", hash.cbHAddr);
            break;

        default :
            StdOutPuts(L"unknown\n");
            break;
    }

    StdOutPutc(L'\n');

    cbSymbols = hash.cbSymbol;

    StdOutPrintf(L"Symbol byte count = 0x%lx\n\n", cbSymbols);

    while (cbSymbols > 0) {
        if (_read(exefile, SymBuf, LNGTHSZ) != LNGTHSZ) {
           Fatal(L"Invalid file");
        }

        cbSymbols -= LNGTHSZ;

        // Get record length

        cb = pSymType->reclen;

        if ((WORD)(_read(exefile, (BYTE *) SymBuf + LNGTHSZ, cb)) != cb) {
           Fatal(L"Invalid file");
        }

        cbSymbols -= cb;

        StdOutPrintf(L"0x%08X ", cbOff);

        switch (pSymType->rectyp) {
            case S_PUB16 :
                C7Data16Sym((DATASYM16 *) SymBuf, L"S_PUB16");
                break;

            case S_GDATA16 :
                C7Data16Sym((DATASYM16 *) SymBuf, L"S_GDATA16");
                break;

            case S_PUB32_ST :
                C7Public32Sym((PUBSYM32 *) SymBuf);
                break;

            case S_GDATA32_ST :
                C7Data32Sym((DATASYM32 *) SymBuf, L"S_GDATA32");
                break;

            case S_UDT_ST :
                C7UDTSym((UDTSYM *) SymBuf);
                break;

            case S_CONSTANT_ST :
                C7ConSym((CONSTSYM *) SymBuf);
                break;

            case S_OBJNAME_ST :
                C7ObjNameSym((OBJNAMESYM *) SymBuf);
                break;

            case S_COBOLUDT_ST :
                C7CobolUDT((UDTSYM *) SymBuf);
                break;

            case S_GTHREAD32_ST :
                C7Data32Sym((DATASYM32 *) SymBuf, L"S_GTHREAD32");
                break;

            case S_PROCREF_ST :
            case S_DATAREF_ST :
            case S_LPROCREF_ST :
                C7RefSym((REFSYM *) SymBuf);
                break;

            case S_ALIGN :
                C7AlignSym((ALIGNSYM *) SymBuf);
                break;

            case S_PUB32_16t :
                C7Data32Sym_16t((DATASYM32_16t *) SymBuf, L"S_PUB32_16t");
                break;

            case S_GDATA32_16t :
                C7Data32Sym_16t((DATASYM32_16t *) SymBuf, L"S_GDATA32_16t");
                break;

            case S_UDT_16t :
                C7UDTSym_16t((UDTSYM_16t *) SymBuf);
                break;

            case S_CONSTANT_16t :
                C7ConSym_16t((CONSTSYM_16t *) SymBuf);
                break;

            case S_COBOLUDT_16t :
                C7CobolUDT_16t((UDTSYM_16t *) SymBuf);
                break;

            case S_GTHREAD32_16t :
                C7Data32Sym_16t((DATASYM32_16t *) SymBuf, L"S_GTHREAD32_16t");
                break;

            default :
                assert(false);
                break;
        }

        StdOutPutc(L'\n');

        cbOff += cb + LNGTHSZ;
    }

    StdOutPutc(L'\n');

    // dump symbol and address hashing tables

    switch (hash.symhash) {
        case 2 :
        case 6 :
            SymHash32(&hash, pDir);
            break;

        case 10 :
            SymHash32Long(&hash, pDir);
            break;
    }
    switch (hash.addrhash) {
        case 4 :
            AddrHash32(&hash, pDir, 4);
            break;

        case 5 :
            AddrHash32(&hash, pDir, 5);
            break;

        case 8 :
            AddrHash32NB09(&hash, pDir);
            break;

        case 12 :
            AddrHash32(&hash, pDir, 12);
            break;
    }
}


void PrtSkip()
{
    fSkip = true;
}


void PrtIndent(bool fOffset = true)
{
    if (fSkip) {
        StdOutPutc(L'\n');

        fSkip = false;
    }

    if (SymOffset != 0xFFFFFFFF) {
        if (fOffset) {
            StdOutPrintf(L"(%06X) ", SymOffset);
        } else {
            StdOutPuts(L"         ");
        }
    }

    for (size_t n = 0; n < cchIndent; n++) {
        StdOutPutc(L' ');
    }
}

const unsigned char *PrintSt(bool fSz, const unsigned char *rgch, bool fNewLine)
{
    const unsigned char *pchNext;

    if (fSz) {
        size_t cch = UnicodeLengthOfUTF8((char *) rgch);

        wchar_t *wsz = (wchar_t *) _alloca(cch * sizeof(wchar_t));

        if (UTF8ToUnicode((char *) rgch, wsz, cch)) {
            StdOutPuts(wsz);
        }

        pchNext = rgch + strlen((char *) rgch) + 1;
    } else {
        // rgch is a length prefixed string

        size_t cch = *rgch;

        if (cch == 0) {
            StdOutPuts(L"(none)");
        } else {
            StdOutPrintf(L"%.*S", cch, rgch+1);
        }

        pchNext = rgch + cch + 1;
    }

    if (fNewLine) {
        StdOutPutc(L'\n');
    }

    return pchNext;
}

const unsigned char *ShowSt(bool fSz, const wchar_t *psz, const unsigned char *pst, bool fNewLine)
{
    // psz is normal C type zero terminated string
    // pst is either length prefixed ANSI or zero terminated UTF-8

    StdOutPrintf(L"%s", psz);

    return PrintSt(fSz, pst, fNewLine);
}


void C7EndSym2(const wchar_t *name)
{
    PrtSkip();

    if (cchIndent > 0) {
        cchIndent--;
    }

    PrtIndent();

    StdOutPrintf(L"%s\n", name);

    PrtSkip();
}


void C7EndSym(const void*)
{
    C7EndSym2(L"S_END");
}


void C7EndArgs(const void *)
{
    PrtIndent();

    StdOutPuts(L"S_ENDARG\n");
}


void C7BpRel16Sym(const BPRELSYM16 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_BPREL16: [%04X], Type: %18s, ",
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7Data16Sym(const DATASYM16 *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: [%04X:%04X], Type: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7LData16Sym(const DATASYM16 *psym)
{
    C7Data16Sym(psym, L"S_LDATA16");
}


void C7GData16Sym(const DATASYM16 *psym)
{
    C7Data16Sym(psym, L"S_GDATA16");
}


void C7Public16Sym(const DATASYM16 *psym)
{
    C7Data16Sym(psym, L"S_PUB16");
}


void C7BpRel32Sym(const BPRELSYM32 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_BPREL32: [%08X], Type: %18s, ",
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}

void PrintLvarFlags( CV_LVARFLAGS flags, CV_typ_t typind )
{
    if (flags.fIsParam) {
        StdOutPrintf(L"Param: %08X, ", typind);
    }
    else {
        StdOutPrintf(L"Local: %08X, ", typind);
    }

    if (flags.fAddrTaken) {
        StdOutPuts(L"Address Taken, ");
    }

    if (flags.fCompGenx) {
        StdOutPuts(L"Compiler Generated, ");
    }

    if (flags.fIsAggregate) {
        StdOutPuts(L"aggregate, ");
    }

    if (flags.fIsAggregated) {
        StdOutPuts(L"aggregated, ");
    }

    if (flags.fIsAlias) {
        StdOutPuts(L"alias, ");
    }

    if (flags.fIsAliased) {
        StdOutPuts(L"aliased, ");
    }

    if (flags.fIsRetValue) {
        StdOutPuts(L"return value, ");
    }

    if (flags.fIsOptimizedOut) {
        StdOutPuts(L"optimized away, ");
    }

    if (flags.fIsEnregGlob && !flags.fIsEnregStat) {
        StdOutPuts(L"global, ");
    } else if (flags.fIsEnregGlob && flags.fIsEnregStat) {
        StdOutPuts(L"file static, ");
    } else if (!flags.fIsEnregGlob && flags.fIsEnregStat) {
        StdOutPuts(L"static local, ");
    }
}

void PrintLvAttr( const CV_lvar_attr* pAttr, CV_typ_t typind)
{
    PrintLvarFlags( pAttr->flags, typind);
}

void C7ManFrameRel(const FRAMERELSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_MANFRAMEREL: [%08X], ",
                 psym->off);

    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7AttrFrameRel(const FRAMERELSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_ATTR_FRAMEREL: [%08X], ",
                 psym->off);

    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7BpRel32Sym_16t(const BPRELSYM32_16t *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_BPREL32_16t: [%08X], Type: %18s, ",
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7Data32Sym(const DATASYM32 *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: [%04X:%08X], Type: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}

void C7ManDataSym(const DATASYM32 *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: [%04X:%08X], Token: %08X, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7LData32Sym(const DATASYM32 *psym)
{
    C7Data32Sym(psym, L"S_LDATA32");
}


void C7GData32Sym(const DATASYM32 *psym)
{
    C7Data32Sym(psym, L"S_GDATA32");
}

const wchar_t * SzNameHLSLRegType(unsigned short regType)
{
    DWORD index = regType - CV_HLSLREG_TEMP;

    if (index > sizeof(C7HLSLRegTypeStrings) / sizeof(C7HLSLRegTypeStrings[0])) {
        static wchar_t buf[80];

        swprintf_s(buf, _countof(buf), L"****Warning**** invalid register type value: %04X", regType);

        return buf;
    }
    
    return C7HLSLRegTypeStrings[index];
}

void C7DataHLSLSym(const DATASYMHLSL *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: Type: %18s, %s\n", szSymType,
            SzNameC7Type2(psym->typind), SzNameHLSLRegType(psym->regType));

    StdOutPrintf(L"\tbase data: slot = %u offset = %u, texture slot = %u, sampler slot = %u, UAV slot = %u\n\t",
            psym->dataslot, psym->dataoff, psym->texslot, psym->sampslot, psym->uavslot);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7DataHLSL32Sym(const DATASYMHLSL32 *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: Type: %18s, %s\n", szSymType,
            SzNameC7Type2(psym->typind), SzNameHLSLRegType(psym->regType));

    StdOutPrintf(L"\tbase data: slot = %lu offset = %lu, texture slot = %lu, sampler slot = %lu, UAV slot = %lu\n\t",
            psym->dataslot, psym->dataoff, psym->texslot, psym->sampslot, psym->uavslot);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7DataHLSL32ExSym(const DATASYMHLSL32_EX *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: Type: %18s, %s\n", szSymType,
            SzNameC7Type2(psym->typind), SzNameHLSLRegType(psym->regType));

    StdOutPrintf(L"\tregister index = %u, base data offset start = %u, bind space = %u, bind slot = %u\n\t",
            psym->regID, psym->dataoff, psym->bindSpace, psym->bindSlot);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7LDataHLSLSym(const DATASYMHLSL *psym)
{
    C7DataHLSLSym(psym, L"S_LDATAHLSL");
}

void C7LDataHLSL32Sym(const DATASYMHLSL32 *psym)
{
    C7DataHLSL32Sym(psym, L"S_LDATAHLSL32");
}

void C7LDataHLSL32ExSym(const DATASYMHLSL32_EX *psym)
{
    C7DataHLSL32ExSym(psym, L"S_LDATAHLSL32_EX");
}

void C7GDataHLSLSym(const DATASYMHLSL *psym)
{
    C7DataHLSLSym(psym, L"S_GDATAHLSL");
}

void C7GDataHLSL32Sym(const DATASYMHLSL32 *psym)
{
    C7DataHLSL32Sym(psym, L"S_GDATAHLSL32");
}

void C7GDataHLSL32ExSym(const DATASYMHLSL32_EX *psym)
{
    C7DataHLSL32ExSym(psym, L"S_GDATAHLSL32_EX");
}

void C7LManData(const DATASYM32 *psym)
{
    C7ManDataSym(psym, L"S_LMANDATA");
}

void C7GManData(const DATASYM32 *psym)
{
    C7ManDataSym(psym, L"S_GMANDATA");
}

void C7TLData32Sym(const DATASYM32 *psym)
{
    C7Data32Sym(psym, L"S_LTHREAD32");
}


void C7TGData32Sym(const DATASYM32 *psym)
{
    C7Data32Sym(psym, L"S_GTHREAD32");
}


void C7Public32Sym(const PUBSYM32 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_PUB32: [%04X:%08X], Flags: %08X, ",
                 psym->seg, psym->off,
                 psym->pubsymflags.grfFlags);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7Data32Sym_16t(const DATASYM32_16t *psym, const wchar_t *szSymType)
{
    PrtIndent();

    StdOutPrintf(L"%s: [%04X:%08X], Type: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7LData32Sym_16t(const DATASYM32_16t *psym)
{
    C7Data32Sym_16t(psym, L"S_LDATA32_16t");
}


void C7GData32Sym_16t(const DATASYM32_16t *psym)
{
    C7Data32Sym_16t(psym, L"S_GDATA32_16t");
}


void C7TLData32Sym_16t(const DATASYM32_16t *psym)
{
    C7Data32Sym_16t(psym, L"S_LTHREAD32_16t");
}


void C7TGData32Sym_16t(const DATASYM32_16t *psym)
{
    C7Data32Sym_16t(psym, L"S_GTHREAD32_16t");
}


void C7Public32Sym_16t(const DATASYM32_16t *psym)
{
    C7Data32Sym_16t(psym, L"S_PUB32_16t");
}


void C7RegRel16Sym(const REGREL16 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_REGREL16: %s+%04X, Type: %18s, ",
                 SzNameC7Reg(psym->reg),
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7RegRel32Sym(const REGREL32 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_REGREL32: %s+%08X, Type: %18s, ",
                 SzNameC7Reg(psym->reg),
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}

void C7ManRegRel(const ATTRREGREL *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_MANREGREL: %s+%08X, ",
                 SzNameC7Reg(psym->reg),
                 psym->off);

    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7AttrRegRel(const ATTRREGRELSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_ATTR_REGREL: %s+%08X, ",
                 SzNameC7Reg(psym->reg),
                 psym->off);

    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7RegRel32Sym_16t(const REGREL32_16t *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_REGREL32_16t: %s+%08X, Type: %18s, ",
                 SzNameC7Reg(psym->reg),
                 psym->off,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}

void C7ManTypeRef( const MANTYPREF*psym)
{
    PrtIndent();

    StdOutPrintf(L"S_MANTYPREF: %s", SzNameC7Type2(psym->typind));
}

void C7UNameSpace( const UNAMESPACE*psym)
{
    PrtIndent();

    StdOutPuts(L"S_UNAMESPACE: ");

    PrintSt(fUtf8Symbols, psym->name);
}

const wchar_t * const rgszRegX86[] = {
    L"None",         // 0   CV_REG_NONE
    L"al",           // 1   CV_REG_AL
    L"cl",           // 2   CV_REG_CL
    L"dl",           // 3   CV_REG_DL
    L"bl",           // 4   CV_REG_BL
    L"ah",           // 5   CV_REG_AH
    L"ch",           // 6   CV_REG_CH
    L"dh",           // 7   CV_REG_DH
    L"bh",           // 8   CV_REG_BH
    L"ax",           // 9   CV_REG_AX
    L"cx",           // 10  CV_REG_CX
    L"dx",           // 11  CV_REG_DX
    L"bx",           // 12  CV_REG_BX
    L"sp",           // 13  CV_REG_SP
    L"bp",           // 14  CV_REG_BP
    L"si",           // 15  CV_REG_SI
    L"di",           // 16  CV_REG_DI
    L"eax",          // 17  CV_REG_EAX
    L"ecx",          // 18  CV_REG_ECX
    L"edx",          // 19  CV_REG_EDX
    L"ebx",          // 20  CV_REG_EBX
    L"esp",          // 21  CV_REG_ESP
    L"ebp",          // 22  CV_REG_EBP
    L"esi",          // 23  CV_REG_ESI
    L"edi",          // 24  CV_REG_EDI
    L"es",           // 25  CV_REG_ES
    L"cs",           // 26  CV_REG_CS
    L"ss",           // 27  CV_REG_SS
    L"ds",           // 28  CV_REG_DS
    L"fs",           // 29  CV_REG_FS
    L"gs",           // 30  CV_REG_GS
    L"ip",           // 31  CV_REG_IP
    L"flags",        // 32  CV_REG_FLAGS
    L"eip",          // 33  CV_REG_EIP
    L"eflags",       // 34  CV_REG_EFLAG
    L"???",          // 35
    L"???",          // 36
    L"???",          // 37
    L"???",          // 38
    L"???",          // 39
    L"temp",         // 40  CV_REG_TEMP
    L"temph",        // 41  CV_REG_TEMPH
    L"quote",        // 42  CV_REG_QUOTE
    L"pcdr3",        // 43  CV_REG_PCDR3
    L"pcdr4",        // 44  CV_REG_PCDR4
    L"pcdr5",        // 45  CV_REG_PCDR5
    L"pcdr6",        // 46  CV_REG_PCDR6
    L"pcdr7",        // 47  CV_REG_PCDR7
    L"???",          // 48
    L"???",          // 49
    L"???",          // 50
    L"???",          // 51
    L"???",          // 52
    L"???",          // 53
    L"???",          // 54
    L"???",          // 55
    L"???",          // 56
    L"???",          // 57
    L"???",          // 58
    L"???",          // 59
    L"???",          // 60
    L"???",          // 61
    L"???",          // 62
    L"???",          // 63
    L"???",          // 64
    L"???",          // 65
    L"???",          // 66
    L"???",          // 67
    L"???",          // 68
    L"???",          // 69
    L"???",          // 70
    L"???",          // 71
    L"???",          // 72
    L"???",          // 73
    L"???",          // 74
    L"???",          // 75
    L"???",          // 76
    L"???",          // 77
    L"???",          // 78
    L"???",          // 79
    L"cr0",          // 80  CV_REG_CR0
    L"cr1",          // 81  CV_REG_CR1
    L"cr2",          // 82  CV_REG_CR2
    L"cr3",          // 83  CV_REG_CR3
    L"cr4",          // 84  CV_REG_CR4
    L"???",          // 85
    L"???",          // 86
    L"???",          // 87
    L"???",          // 88
    L"???",          // 89
    L"dr0",          // 90  CV_REG_DR0
    L"dr1",          // 91  CV_REG_DR1
    L"dr2",          // 92  CV_REG_DR2
    L"dr3",          // 93  CV_REG_DR3
    L"dr4",          // 94  CV_REG_DR4
    L"dr5",          // 95  CV_REG_DR5
    L"dr6",          // 96  CV_REG_DR6
    L"dr7",          // 97  CV_REG_DR7
    L"???",          // 98
    L"???",          // 99
    L"???",          // 100
    L"???",          // 101
    L"???",          // 102
    L"???",          // 103
    L"???",          // 104
    L"???",          // 105
    L"???",          // 106
    L"???",          // 107
    L"???",          // 108
    L"???",          // 109
    L"gdtr",         // 110 CV_REG_GDTR
    L"gdtl",         // 111 CV_REG_GDTL
    L"idtr",         // 112 CV_REG_IDTR
    L"idtl",         // 113 CV_REG_IDTL
    L"ldtr",         // 114 CV_REG_LDTR
    L"tr",           // 115 CV_REG_TR
    L"???",          // 116
    L"???",          // 117
    L"???",          // 118
    L"???",          // 119
    L"???",          // 120
    L"???",          // 121
    L"???",          // 122
    L"???",          // 123
    L"???",          // 124
    L"???",          // 125
    L"???",          // 126
    L"???",          // 127
    L"st(0)",        // 128 CV_REG_ST0
    L"st(1)",        // 129 CV_REG_ST1
    L"st(2)",        // 130 CV_REG_ST2
    L"st(3)",        // 131 CV_REG_ST3
    L"st(4)",        // 132 CV_REG_ST4
    L"st(5)",        // 133 CV_REG_ST5
    L"st(6)",        // 134 CV_REG_ST6
    L"st(7)",        // 135 CV_REG_ST7
    L"ctrl",         // 136 CV_REG_CTRL
    L"stat",         // 137 CV_REG_STAT
    L"tag",          // 138 CV_REG_TAG
    L"fpip",         // 139 CV_REG_FPIP
    L"fpcs",         // 140 CV_REG_FPCS
    L"fpdo",         // 141 CV_REG_FPDO
    L"fpds",         // 142 CV_REG_FPDS
    L"isem",         // 143 CV_REG_ISEM
    L"fpeip",        // 144 CV_REG_FPEIP
    L"fped0"         // 145 CV_REG_FPEDO
};

const wchar_t * const rgszRegAMD64[] = {
    L"None",         // 0   CV_REG_NONE
    L"al",           // 1   CV_AMD64_AL
    L"cl",           // 2   CV_AMD64_CL
    L"dl",           // 3   CV_AMD64_DL
    L"bl",           // 4   CV_AMD64_BL
    L"ah",           // 5   CV_AMD64_AH
    L"ch",           // 6   CV_AMD64_CH
    L"dh",           // 7   CV_AMD64_DH
    L"bh",           // 8   CV_AMD64_BH
    L"ax",           // 9   CV_AMD64_AX
    L"cx",           // 10  CV_AMD64_CX
    L"dx",           // 11  CV_AMD64_DX
    L"bx",           // 12  CV_AMD64_BX
    L"sp",           // 13  CV_AMD64_SP
    L"bp",           // 14  CV_AMD64_BP
    L"si",           // 15  CV_AMD64_SI
    L"di",           // 16  CV_AMD64_DI
    L"eax",          // 17  CV_AMD64_EAX
    L"ecx",          // 18  CV_AMD64_ECX
    L"edx",          // 19  CV_AMD64_EDX
    L"ebx",          // 20  CV_AMD64_EBX
    L"esp",          // 21  CV_AMD64_ESP
    L"ebp",          // 22  CV_AMD64_EBP
    L"esi",          // 23  CV_AMD64_ESI
    L"edi",          // 24  CV_AMD64_EDI
    L"es",           // 25  CV_AMD64_ES
    L"cs",           // 26  CV_AMD64_CS
    L"ss",           // 27  CV_AMD64_SS
    L"ds",           // 28  CV_AMD64_DS
    L"fs",           // 29  CV_AMD64_FS
    L"gs",           // 30  CV_AMD64_GS
    L"flags",        // 31  CV_AMD64_FLAGS
    L"rip",          // 32  CV_AMD64_RIP
    L"eflags",       // 33  CV_AMD64_EFLAGS
    L"???",          // 34
    L"???",          // 35
    L"???",          // 36
    L"???",          // 37
    L"???",          // 38
    L"???",          // 39
    L"???",          // 40
    L"???",          // 41
    L"???",          // 42
    L"???",          // 43
    L"???",          // 44
    L"???",          // 45
    L"???",          // 46
    L"???",          // 47
    L"???",          // 48
    L"???",          // 49
    L"???",          // 50
    L"???",          // 51
    L"???",          // 52
    L"???",          // 53
    L"???",          // 54
    L"???",          // 55
    L"???",          // 56
    L"???",          // 57
    L"???",          // 58
    L"???",          // 59
    L"???",          // 60
    L"???",          // 61
    L"???",          // 62
    L"???",          // 63
    L"???",          // 64
    L"???",          // 65
    L"???",          // 66
    L"???",          // 67
    L"???",          // 68
    L"???",          // 69
    L"???",          // 70
    L"???",          // 71
    L"???",          // 72
    L"???",          // 73
    L"???",          // 74
    L"???",          // 75
    L"???",          // 76
    L"???",          // 77
    L"???",          // 78
    L"???",          // 79
    L"cr0",          // 80  CV_AMD64_CR0
    L"cr1",          // 81  CV_AMD64_CR1
    L"cr2",          // 82  CV_AMD64_CR2
    L"cr3",          // 83  CV_AMD64_CR3
    L"cr4",          // 84  CV_AMD64_CR4
    L"???",          // 85
    L"???",          // 86
    L"???",          // 87
    L"cr8",          // 88  CV_AMD64_CR8
    L"???",          // 89
    L"dr0",          // 90  CV_AMD64_DR0
    L"dr1",          // 91  CV_AMD64_DR1
    L"dr2",          // 92  CV_AMD64_DR2
    L"dr3",          // 93  CV_AMD64_DR3
    L"dr4",          // 94  CV_AMD64_DR4
    L"dr5",          // 95  CV_AMD64_DR5
    L"dr6",          // 96  CV_AMD64_DR6
    L"dr7",          // 97  CV_AMD64_DR7
    L"dr8",          // 98  CV_AMD64_DR8
    L"dr9",          // 99  CV_AMD64_DR9
    L"dr10",         // 100 CV_AMD64_DR10
    L"dr11",         // 101 CV_AMD64_DR11
    L"dr12",         // 102 CV_AMD64_DR12
    L"dr13",         // 103 CV_AMD64_DR13
    L"dr14",         // 104 CV_AMD64_DR14
    L"dr15",         // 105 CV_AMD64_DR15
    L"???",          // 106
    L"???",          // 107
    L"???",          // 108
    L"???",          // 109
    L"gdtr",         // 110 CV_AMD64_GDTR
    L"gdtl",         // 111 CV_AMD64_GDTL
    L"idtr",         // 112 CV_AMD64_IDTR
    L"idtl",         // 113 CV_AMD64_IDTL
    L"ldtr",         // 114 CV_AMD64_LDTR
    L"tr",           // 115 CV_AMD64_TR
    L"???",          // 116
    L"???",          // 117
    L"???",          // 118
    L"???",          // 119
    L"???",          // 120
    L"???",          // 121
    L"???",          // 122
    L"???",          // 123
    L"???",          // 124
    L"???",          // 125
    L"???",          // 126
    L"???",          // 127
    L"st(0)",        // 128 CV_AMD64_ST0
    L"st(1)",        // 129 CV_AMD64_ST1
    L"st(2)",        // 130 CV_AMD64_ST2
    L"st(3)",        // 131 CV_AMD64_ST3
    L"st(4)",        // 132 CV_AMD64_ST4
    L"st(5)",        // 133 CV_AMD64_ST5
    L"st(6)",        // 134 CV_AMD64_ST6
    L"st(7)",        // 135 CV_AMD64_ST7
    L"ctrl",         // 136 CV_AMD64_CTRL
    L"stat",         // 137 CV_AMD64_STAT
    L"tag",          // 138 CV_AMD64_TAG
    L"fpip",         // 139 CV_AMD64_FPIP
    L"fpcs",         // 140 CV_AMD64_FPCS
    L"fpdo",         // 141 CV_AMD64_FPDO
    L"fpds",         // 142 CV_AMD64_FPDS
    L"isem",         // 143 CV_AMD64_ISEM
    L"fpeip",        // 144 CV_AMD64_FPEIP
    L"fped0",        // 145 CV_AMD64_FPEDO
    L"mm0",          // 146 CV_AMD64_MM0
    L"mm1",          // 147 CV_AMD64_MM1
    L"mm2",          // 148 CV_AMD64_MM2
    L"mm3",          // 149 CV_AMD64_MM3
    L"mm4",          // 150 CV_AMD64_MM4
    L"mm5",          // 151 CV_AMD64_MM5
    L"mm6",          // 152 CV_AMD64_MM6
    L"mm7",          // 153 CV_AMD64_MM7
    L"xmm0",         // 154 CV_AMD64_XMM0
    L"xmm1",         // 155 CV_AMD64_XMM1
    L"xmm2",         // 156 CV_AMD64_XMM2
    L"xmm3",         // 157 CV_AMD64_XMM3
    L"xmm4",         // 158 CV_AMD64_XMM4
    L"xmm5",         // 159 CV_AMD64_XMM5
    L"xmm6",         // 160 CV_AMD64_XMM6
    L"xmm7",         // 161 CV_AMD64_XMM7
    L"xmm0_0",       // 162 CV_AMD64_XMM0_0
    L"xmm0_1",       // 163 CV_AMD64_XMM0_1
    L"xmm0_2",       // 164 CV_AMD64_XMM0_2
    L"xmm0_3",       // 165 CV_AMD64_XMM0_3
    L"xmm1_0",       // 166 CV_AMD64_XMM1_0
    L"xmm1_1",       // 167 CV_AMD64_XMM1_1
    L"xmm1_2",       // 168 CV_AMD64_XMM1_2
    L"xmm1_3",       // 169 CV_AMD64_XMM1_3
    L"xmm2_0",       // 170 CV_AMD64_XMM2_0
    L"xmm2_1",       // 171 CV_AMD64_XMM2_1
    L"xmm2_2",       // 172 CV_AMD64_XMM2_2
    L"xmm2_3",       // 173 CV_AMD64_XMM2_3
    L"xmm3_0",       // 174 CV_AMD64_XMM3_0
    L"xmm3_1",       // 175 CV_AMD64_XMM3_1
    L"xmm3_2",       // 176 CV_AMD64_XMM3_2
    L"xmm3_3",       // 177 CV_AMD64_XMM3_3
    L"xmm4_0",       // 178 CV_AMD64_XMM4_0
    L"xmm4_1",       // 179 CV_AMD64_XMM4_1
    L"xmm4_2",       // 180 CV_AMD64_XMM4_2
    L"xmm4_3",       // 181 CV_AMD64_XMM4_3
    L"xmm5_0",       // 182 CV_AMD64_XMM5_0
    L"xmm5_1",       // 183 CV_AMD64_XMM5_1
    L"xmm5_2",       // 184 CV_AMD64_XMM5_2
    L"xmm5_3",       // 185 CV_AMD64_XMM5_3
    L"xmm6_0",       // 186 CV_AMD64_XMM6_0
    L"xmm6_1",       // 187 CV_AMD64_XMM6_1
    L"xmm6_2",       // 188 CV_AMD64_XMM6_2
    L"xmm6_3",       // 189 CV_AMD64_XMM6_3
    L"xmm7_0",       // 190 CV_AMD64_XMM7_0
    L"xmm7_1",       // 191 CV_AMD64_XMM7_1
    L"xmm7_2",       // 192 CV_AMD64_XMM7_2
    L"xmm7_3",       // 193 CV_AMD64_XMM7_3
    L"xmm0l",        // 194 CV_AMD64_XMM0L
    L"xmm1l",        // 195 CV_AMD64_XMM1L
    L"xmm2l",        // 196 CV_AMD64_XMM2L
    L"xmm3l",        // 197 CV_AMD64_XMM3L
    L"xmm4l",        // 198 CV_AMD64_XMM4L
    L"xmm5l",        // 199 CV_AMD64_XMM5L
    L"xmm6l",        // 200 CV_AMD64_XMM6L
    L"xmm7l",        // 201 CV_AMD64_XMM7L
    L"xmm0h",        // 202 CV_AMD64_XMM0H
    L"xmm1h",        // 203 CV_AMD64_XMM1H
    L"xmm2h",        // 204 CV_AMD64_XMM2H
    L"xmm3h",        // 205 CV_AMD64_XMM3H
    L"xmm4h",        // 206 CV_AMD64_XMM4H
    L"xmm5h",        // 207 CV_AMD64_XMM5H
    L"xmm6h",        // 208 CV_AMD64_XMM6H
    L"xmm7h",        // 209 CV_AMD64_XMM7H
    L"???",          // 210
    L"mxcsr",        // 211 CV_AMD64_MXCSR
    L"???",          // 212
    L"???",          // 213
    L"???",          // 214
    L"???",          // 215
    L"???",          // 216
    L"???",          // 217
    L"???",          // 218
    L"???",          // 219
    L"emm0l",        // 220 CV_AMD64_EMM0L
    L"emm1l",        // 221 CV_AMD64_EMM1L
    L"emm2l",        // 222 CV_AMD64_EMM2L
    L"emm3l",        // 223 CV_AMD64_EMM3L
    L"emm4l",        // 224 CV_AMD64_EMM4L
    L"emm5l",        // 225 CV_AMD64_EMM5L
    L"emm6l",        // 226 CV_AMD64_EMM6L
    L"emm7l",        // 227 CV_AMD64_EMM7L
    L"emm0h",        // 228 CV_AMD64_EMM0H
    L"emm1h",        // 229 CV_AMD64_EMM1H
    L"emm2h",        // 230 CV_AMD64_EMM2H
    L"emm3h",        // 231 CV_AMD64_EMM3H
    L"emm4h",        // 232 CV_AMD64_EMM4H
    L"emm5h",        // 233 CV_AMD64_EMM5H
    L"emm6h",        // 234 CV_AMD64_EMM6H
    L"emm7h",        // 235 CV_AMD64_EMM7H
    L"mm00",         // 236 CV_AMD64_MM00
    L"mm01",         // 237 CV_AMD64_MM01
    L"mm10",         // 238 CV_AMD64_MM10
    L"mm11",         // 239 CV_AMD64_MM11
    L"mm20",         // 240 CV_AMD64_MM20
    L"mm21",         // 241 CV_AMD64_MM21
    L"mm30",         // 242 CV_AMD64_MM30
    L"mm31",         // 243 CV_AMD64_MM31
    L"mm40",         // 244 CV_AMD64_MM40
    L"mm41",         // 245 CV_AMD64_MM41
    L"mm50",         // 246 CV_AMD64_MM50
    L"mm51",         // 247 CV_AMD64_MM51
    L"mm60",         // 248 CV_AMD64_MM60
    L"mm61",         // 249 CV_AMD64_MM61
    L"mm70",         // 250 CV_AMD64_MM70
    L"mm71",         // 251 CV_AMD64_MM71
    L"xmm8",         // 252 CV_AMD64_XMM8
    L"xmm9",         // 253 CV_AMD64_XMM9
    L"xmm10",        // 254 CV_AMD64_XMM10
    L"xmm11",        // 255 CV_AMD64_XMM11
    L"xmm12",        // 256 CV_AMD64_XMM12
    L"xmm13",        // 257 CV_AMD64_XMM13
    L"xmm14",        // 258 CV_AMD64_XMM14
    L"xmm15",        // 259 CV_AMD64_XMM15
    L"xmm8_0",       // 260 CV_AMD64_XMM8_0
    L"xmm8_1",       // 261 CV_AMD64_XMM8_1
    L"xmm8_2",       // 262 CV_AMD64_XMM8_2
    L"xmm8_3",       // 263 CV_AMD64_XMM8_3
    L"xmm9_0",       // 264 CV_AMD64_XMM9_0
    L"xmm9_1",       // 265 CV_AMD64_XMM9_1
    L"xmm9_2",       // 266 CV_AMD64_XMM9_2
    L"xmm9_3",       // 267 CV_AMD64_XMM9_3
    L"xmm10_0",      // 268 CV_AMD64_XMM10_0
    L"xmm10_1",      // 269 CV_AMD64_XMM10_1
    L"xmm10_2",      // 270 CV_AMD64_XMM10_2
    L"xmm10_3",      // 271 CV_AMD64_XMM10_3
    L"xmm11_0",      // 272 CV_AMD64_XMM11_0
    L"xmm11_1",      // 273 CV_AMD64_XMM11_1
    L"xmm11_2",      // 274 CV_AMD64_XMM11_2
    L"xmm11_3",      // 275 CV_AMD64_XMM11_3
    L"xmm12_0",      // 276 CV_AMD64_XMM12_0
    L"xmm12_1",      // 277 CV_AMD64_XMM12_1
    L"xmm12_2",      // 278 CV_AMD64_XMM12_2
    L"xmm12_3",      // 279 CV_AMD64_XMM12_3
    L"xmm13_0",      // 280 CV_AMD64_XMM13_0
    L"xmm13_1",      // 281 CV_AMD64_XMM13_1
    L"xmm13_2",      // 282 CV_AMD64_XMM13_2
    L"xmm13_3",      // 283 CV_AMD64_XMM13_3
    L"xmm14_0",      // 284 CV_AMD64_XMM14_0
    L"xmm14_1",      // 285 CV_AMD64_XMM14_1
    L"xmm14_2",      // 286 CV_AMD64_XMM14_2
    L"xmm14_3",      // 287 CV_AMD64_XMM14_3
    L"xmm15_0",      // 288 CV_AMD64_XMM15_0
    L"xmm15_1",      // 289 CV_AMD64_XMM15_1
    L"xmm15_2",      // 290 CV_AMD64_XMM15_2
    L"xmm15_3",      // 291 CV_AMD64_XMM15_3
    L"xmm8l",        // 292 CV_AMD64_XMM8L
    L"xmm9l",        // 293 CV_AMD64_XMM9L
    L"xmm10l",       // 294 CV_AMD64_XMM10L
    L"xmm11l",       // 295 CV_AMD64_XMM11L
    L"xmm12l",       // 296 CV_AMD64_XMM12L
    L"xmm13l",       // 297 CV_AMD64_XMM13L
    L"xmm14l",       // 298 CV_AMD64_XMM14L
    L"xmm15l",       // 299 CV_AMD64_XMM15L
    L"xmm8h",        // 300 CV_AMD64_XMM8H
    L"xmm9h",        // 301 CV_AMD64_XMM9H
    L"xmm10h",       // 302 CV_AMD64_XMM10H
    L"xmm11h",       // 303 CV_AMD64_XMM11H
    L"xmm12h",       // 304 CV_AMD64_XMM12H
    L"xmm13h",       // 305 CV_AMD64_XMM13H
    L"xmm14h",       // 306 CV_AMD64_XMM14H
    L"xmm15h",       // 307 CV_AMD64_XMM15H
    L"emm8l",        // 308 CV_AMD64_EMM8L
    L"emm9l",        // 309 CV_AMD64_EMM9L
    L"emm10l",       // 310 CV_AMD64_EMM10L
    L"emm11l",       // 311 CV_AMD64_EMM11L
    L"emm12l",       // 312 CV_AMD64_EMM12L
    L"emm13l",       // 313 CV_AMD64_EMM13L
    L"emm14l",       // 314 CV_AMD64_EMM14L
    L"emm15l",       // 315 CV_AMD64_EMM15L
    L"emm8h",        // 316 CV_AMD64_EMM8H
    L"emm9h",        // 317 CV_AMD64_EMM9H
    L"emm10h",       // 318 CV_AMD64_EMM10H
    L"emm11h",       // 319 CV_AMD64_EMM11H
    L"emm12h",       // 320 CV_AMD64_EMM12H
    L"emm13h",       // 321 CV_AMD64_EMM13H
    L"emm14h",       // 322 CV_AMD64_EMM14H
    L"emm15h",       // 323 CV_AMD64_EMM15H
    L"sil",          // 324 CV_AMD64_SIL
    L"dil",          // 325 CV_AMD64_DIL
    L"bpl",          // 326 CV_AMD64_BPL
    L"spl",          // 327 CV_AMD64_SPL
    L"rax",          // 328 CV_AMD64_RAX
    L"rbx",          // 329 CV_AMD64_RBX
    L"rcx",          // 330 CV_AMD64_RCX
    L"rdx",          // 331 CV_AMD64_RDX
    L"rsi",          // 332 CV_AMD64_RSI
    L"rdi",          // 333 CV_AMD64_RDI
    L"rbp",          // 334 CV_AMD64_RBP
    L"rsp",          // 335 CV_AMD64_RSP
    L"r8",           // 336 CV_AMD64_R8
    L"r9",           // 337 CV_AMD64_R9
    L"r10",          // 338 CV_AMD64_R10
    L"r11",          // 339 CV_AMD64_R11
    L"r12",          // 340 CV_AMD64_R12
    L"r13",          // 341 CV_AMD64_R13
    L"r14",          // 342 CV_AMD64_R14
    L"r15",          // 343 CV_AMD64_R15
    L"r8b",          // 344 CV_AMD64_R8B
    L"r9b",          // 345 CV_AMD64_R9B
    L"r10b",         // 346 CV_AMD64_R10B
    L"r11b",         // 347 CV_AMD64_R11B
    L"r12b",         // 348 CV_AMD64_R12B
    L"r13b",         // 349 CV_AMD64_R13B
    L"r14b",         // 350 CV_AMD64_R14B
    L"r15b",         // 351 CV_AMD64_R15B
    L"r8w",          // 352 CV_AMD64_R8W
    L"r9w",          // 353 CV_AMD64_R9W
    L"r10w",         // 354 CV_AMD64_R10W
    L"r11w",         // 355 CV_AMD64_R11W
    L"r12w",         // 356 CV_AMD64_R12W
    L"r13w",         // 357 CV_AMD64_R13W
    L"r14w",         // 358 CV_AMD64_R14W
    L"r15w",         // 359 CV_AMD64_R15W
    L"r8d",          // 360 CV_AMD64_R8D
    L"r9d",          // 361 CV_AMD64_R9D
    L"r10d",         // 362 CV_AMD64_R10D
    L"r11d",         // 363 CV_AMD64_R11D
    L"r12d",         // 364 CV_AMD64_R12D
    L"r13d",         // 365 CV_AMD64_R13D
    L"r14d",         // 366 CV_AMD64_R14D
    L"r15d",         // 367 CV_AMD64_R15D
};


const wchar_t * const rgszRegMips[] = {
    L"None",         // 0   CV_M4_NOREG
    L"???",          // 1
    L"???",          // 2
    L"???",          // 3
    L"???",          // 4
    L"???",          // 5
    L"???",          // 6
    L"???",          // 7
    L"???",          // 8
    L"???",          // 9
    L"zero",         // 10  CV_M4_IntZERO
    L"at",           // 11  CV_M4_IntAT
    L"v0",           // 12  CV_M4_IntV0
    L"v1",           // 13  CV_M4_IntV1
    L"a0",           // 14  CV_M4_IntA0
    L"a1",           // 15  CV_M4_IntA1
    L"a2",           // 16  CV_M4_IntA2
    L"a3",           // 17  CV_M4_IntA3
    L"t0",           // 18  CV_M4_IntT0
    L"t1",           // 19  CV_M4_IntT1
    L"t2",           // 20  CV_M4_IntT2
    L"t3",           // 21  CV_M4_IntT3
    L"t4",           // 22  CV_M4_IntT4
    L"t5",           // 23  CV_M4_IntT5
    L"t6",           // 24  CV_M4_IntT6
    L"t7",           // 25  CV_M4_IntT7
    L"s0",           // 26  CV_M4_IntS0
    L"s1",           // 27  CV_M4_IntS1
    L"s2",           // 28  CV_M4_IntS2
    L"s3",           // 29  CV_M4_IntS3
    L"s4",           // 30  CV_M4_IntS4
    L"s5",           // 31  CV_M4_IntS5
    L"s6",           // 32  CV_M4_IntS6
    L"s7",           // 33  CV_M4_IntS7
    L"t8",           // 34  CV_M4_IntT8
    L"t9",           // 35  CV_M4_IntT9
    L"k0",           // 36  CV_M4_IntKT0
    L"k1",           // 37  CV_M4_IntKT1
    L"gp",           // 38  CV_M4_IntGP
    L"sp",           // 39  CV_M4_IntSP
    L"s8",           // 40  CV_M4_IntS8
    L"ra",           // 41  CV_M4_IntRA
    L"lo",           // 42  CV_M4_IntLO
    L"hi",           // 43  CV_M4_IntHI
    L"???",          // 44
    L"???",          // 45
    L"???",          // 46
    L"???",          // 47
    L"???",          // 48
    L"???",          // 49
    L"Fir",          // 50  CV_M4_Fir
    L"Psr",          // 51  CV_M4_Psr
    L"???",          // 52
    L"???",          // 53
    L"???",          // 54
    L"???",          // 55
    L"???",          // 56
    L"???",          // 57
    L"???",          // 58
    L"???",          // 59
    L"$f0",          // 60  CV_M4_FltF0
    L"$f1",          // 61  CV_M4_FltF1
    L"$f2",          // 62  CV_M4_FltF2
    L"$f3",          // 63  CV_M4_FltF3
    L"$f4",          // 64  CV_M4_FltF4
    L"$f5",          // 65  CV_M4_FltF5
    L"$f6",          // 66  CV_M4_FltF6
    L"$f7",          // 67  CV_M4_FltF7
    L"$f8",          // 68  CV_M4_FltF8
    L"$f9",          // 69  CV_M4_FltF9
    L"$f10",         // 70  CV_M4_FltF10
    L"$f11",         // 71  CV_M4_FltF11
    L"$f12",         // 72  CV_M4_FltF12
    L"$f13",         // 73  CV_M4_FltF13
    L"$f14",         // 74  CV_M4_FltF14
    L"$f15",         // 75  CV_M4_FltF15
    L"$f16",         // 76  CV_M4_FltF16
    L"$f17",         // 77  CV_M4_FltF17
    L"$f18",         // 78  CV_M4_FltF18
    L"$f19",         // 79  CV_M4_FltF19
    L"$f20",         // 80  CV_M4_FltF20
    L"$f21",         // 81  CV_M4_FltF21
    L"$f22",         // 82  CV_M4_FltF22
    L"$f23",         // 83  CV_M4_FltF23
    L"$f24",         // 84  CV_M4_FltF24
    L"$f25",         // 85  CV_M4_FltF25
    L"$f26",         // 86  CV_M4_FltF26
    L"$f27",         // 87  CV_M4_FltF27
    L"$f28",         // 88  CV_M4_FltF28
    L"$f29",         // 89  CV_M4_FltF29
    L"$f30",         // 90  CV_M4_FltF30
    L"$f31",         // 91  CV_M4_FltF31
    L"Fsr"           // 92  CV_M4_FltFsr
};

const wchar_t * const rgszReg68k[] = {
    L"D0",           // 0   CV_R68_D0
    L"D1",           // 1   CV_R68_D1
    L"D2",           // 2   CV_R68_D2
    L"D3",           // 3   CV_R68_D3
    L"D4",           // 4   CV_R68_D4
    L"D5",           // 5   CV_R68_D5
    L"D6",           // 6   CV_R68_D6
    L"D7",           // 7   CV_R68_D7
    L"A0",           // 8   CV_R68_A0
    L"A1",           // 9   CV_R68_A1
    L"A2",           // 10  CV_R68_A2
    L"A3",           // 11  CV_R68_A3
    L"A4",           // 12  CV_R68_A4
    L"A5",           // 13  CV_R68_A5
    L"A6",           // 14  CV_R68_A6
    L"A7",           // 15  CV_R68_A7
    L"CCR",          // 16  CV_R68_CCR
    L"SR",           // 17  CV_R68_SR
    L"USP",          // 18  CV_R68_USP
    L"MSP",          // 19  CV_R68_MSP
    L"SFC",          // 20  CV_R68_SFC
    L"DFC",          // 21  CV_R68_DFC
    L"CACR",         // 22  CV_R68_CACR
    L"VBR",          // 23  CV_R68_VBR
    L"CAAR",         // 24  CV_R68_CAAR
    L"ISP",          // 25  CV_R68_ISP
    L"PC",           // 26  CV_R68_PC
    L"???",          // 27
    L"FPCR",         // 28  CV_R68_FPCR
    L"FPSR",         // 29  CV_R68_FPSR
    L"FPIAR",        // 30  CV_R68_FPIAR
    L"???",          // 31
    L"FP0",          // 32  CV_R68_FP0
    L"FP1",          // 33  CV_R68_FP1
    L"FP2",          // 34  CV_R68_FP2
    L"FP3",          // 35  CV_R68_FP3
    L"FP4",          // 36  CV_R68_FP4
    L"FP5",          // 37  CV_R68_FP5
    L"FP6",          // 38  CV_R68_FP6
    L"FP7",          // 39  CV_R68_FP7
    L"???",          // 40
    L"???",          // 41  CV_R68_MMUSR030
    L"???",          // 42  CV_R68_MMUSR
    L"???",          // 43  CV_R68_URP
    L"???",          // 44  CV_R68_DTT0
    L"???",          // 45  CV_R68_DTT1
    L"???",          // 46  CV_R68_ITT0
    L"???",          // 47  CV_R68_ITT1
    L"???",          // 48
    L"???",          // 49
    L"???",          // 50
    L"PSR",          // 51  CV_R68_PSR
    L"PCSR",         // 52  CV_R68_PCSR
    L"VAL",          // 53  CV_R68_VAL
    L"CRP",          // 54  CV_R68_CRP
    L"SRP",          // 55  CV_R68_SRP
    L"DRP",          // 56  CV_R68_DRP
    L"TC",           // 57  CV_R68_TC
    L"AC",           // 58  CV_R68_AC
    L"SCC",          // 59  CV_R68_SCC
    L"CAL",          // 60  CV_R68_CAL
    L"TT0",          // 61  CV_R68_TT0
    L"TT1",          // 62  CV_R68_TT1
    L"???",          // 63
    L"BAD0",         // 64  CV_R68_BAD0
    L"BAD1",         // 65  CV_R68_BAD1
    L"BAD2",         // 66  CV_R68_BAD2
    L"BAD3",         // 67  CV_R68_BAD3
    L"BAD4",         // 68  CV_R68_BAD4
    L"BAD5",         // 69  CV_R68_BAD5
    L"BAD6",         // 70  CV_R68_BAD6
    L"BAD7",         // 71  CV_R68_BAD7
    L"BAC0",         // 72  CV_R68_BAC0
    L"BAC1",         // 73  CV_R68_BAC1
    L"BAC2",         // 74  CV_R68_BAC2
    L"BAC3",         // 75  CV_R68_BAC3
    L"BAC4",         // 76  CV_R68_BAC4
    L"BAC5",         // 77  CV_R68_BAC5
    L"BAC6",         // 78  CV_R68_BAC6
    L"BAC7"          // 79  CV_R68_BAC7
};


const wchar_t * const rgszRegAlpha[] =
{
    L"None",         // 0   CV_ALPHA_NOREG
    L"???",          // 1
    L"???",          // 2
    L"???",          // 3
    L"???",          // 4
    L"???",          // 5
    L"???",          // 6
    L"???",          // 7
    L"???",          // 8
    L"???",          // 9
    L"$f0",          // 10  CV_ALPHA_FltF0
    L"$f1",          // 11  CV_ALPHA_FltF1
    L"$f2",          // 12  CV_ALPHA_FltF2
    L"$f3",          // 13  CV_ALPHA_FltF3
    L"$f4",          // 14  CV_ALPHA_FltF4
    L"$f5",          // 15  CV_ALPHA_FltF5
    L"$f6",          // 16  CV_ALPHA_FltF6
    L"$f7",          // 17  CV_ALPHA_FltF7
    L"$f8",          // 18  CV_ALPHA_FltF8
    L"$f9",          // 19  CV_ALPHA_FltF9
    L"$f10",         // 20  CV_ALPHA_FltF10
    L"$f11",         // 21  CV_ALPHA_FltF11
    L"$f12",         // 22  CV_ALPHA_FltF12
    L"$f13",         // 23  CV_ALPHA_FltF13
    L"$f14",         // 24  CV_ALPHA_FltF14
    L"$f15",         // 25  CV_ALPHA_FltF15
    L"$f16",         // 26  CV_ALPHA_FltF16
    L"$f17",         // 27  CV_ALPHA_FltF17
    L"$f18",         // 28  CV_ALPHA_FltF18
    L"$f19",         // 29  CV_ALPHA_FltF19
    L"$f20",         // 30  CV_ALPHA_FltF20
    L"$f21",         // 31  CV_ALPHA_FltF21
    L"$f22",         // 32  CV_ALPHA_FltF22
    L"$f23",         // 33  CV_ALPHA_FltF23
    L"$f24",         // 34  CV_ALPHA_FltF24
    L"$f25",         // 35  CV_ALPHA_FltF25
    L"$f26",         // 36  CV_ALPHA_FltF26
    L"$f27",         // 37  CV_ALPHA_FltF27
    L"$f28",         // 38  CV_ALPHA_FltF28
    L"$f29",         // 39  CV_ALPHA_FltF29
    L"$f30",         // 40  CV_ALPHA_FltF30
    L"$f31",         // 41  CV_ALPHA_FltF31
    L"v0",           // 42  CV_ALPHA_IntV0
    L"t0",           // 43  CV_ALPHA_IntT0
    L"t1",           // 44  CV_ALPHA_IntT1
    L"t2",           // 45  CV_ALPHA_IntT2
    L"t3",           // 46  CV_ALPHA_IntT3
    L"t4",           // 47  CV_ALPHA_IntT4
    L"t5",           // 48  CV_ALPHA_IntT5
    L"t6",           // 49  CV_ALPHA_IntT6
    L"t7",           // 50  CV_ALPHA_IntT7
    L"s0",           // 51  CV_ALPHA_IntS0
    L"s1",           // 52  CV_ALPHA_IntS1
    L"s2",           // 53  CV_ALPHA_IntS2
    L"s3",           // 54  CV_ALPHA_IntS3
    L"s4",           // 55  CV_ALPHA_IntS4
    L"s5",           // 56  CV_ALPHA_IntS5
    L"fp",           // 57  CV_ALPHA_IntFP
    L"a0",           // 58  CV_ALPHA_IntA0
    L"a1",           // 59  CV_ALPHA_IntA1
    L"a2",           // 60  CV_ALPHA_IntA2
    L"a3",           // 61  CV_ALPHA_IntA3
    L"a4",           // 62  CV_ALPHA_IntA4
    L"a5",           // 63  CV_ALPHA_IntA5
    L"t8",           // 64  CV_ALPHA_IntT8
    L"t9",           // 65  CV_ALPHA_IntT9
    L"t10",          // 66  CV_ALPHA_IntT10
    L"t11",          // 67  CV_ALPHA_IntT11
    L"ra",           // 68  CV_ALPHA_IntRA
    L"t12",          // 69  CV_ALPHA_IntT12
    L"at",           // 70  CV_ALPHA_IntAT
    L"gp",           // 71  CV_ALPHA_IntGP
    L"sp",           // 72  CV_ALPHA_IntSP
    L"zero",         // 73  CV_ALPHA_IntZERO
    L"Fpcr",         // 74  CV_ALPHA_Fpcr
    L"Fir",          // 75  CV_ALPHA_Fir
    L"Psr",          // 76  CV_ALPHA_Psr
    L"FltFsr",       // 77  CV_ALPHA_FltFsr
};


const wchar_t * const rgszRegPpc[] = {
    L"None",         // 0
    L"r0",           // 1   CV_PPC_GPR0
    L"r1",           // 2   CV_PPC_GPR1
    L"r2",           // 3   CV_PPC_GPR2
    L"r3",           // 4   CV_PPC_GPR3
    L"r4",           // 5   CV_PPC_GPR4
    L"r5",           // 6   CV_PPC_GPR5
    L"r6",           // 7   CV_PPC_GPR6
    L"r7",           // 8   CV_PPC_GPR7
    L"r8",           // 9   CV_PPC_GPR8
    L"r9",           // 10  CV_PPC_GPR9
    L"r10",          // 11  CV_PPC_GPR10
    L"r11",          // 12  CV_PPC_GPR11
    L"r12",          // 13  CV_PPC_GPR12
    L"r13",          // 14  CV_PPC_GPR13
    L"r14",          // 15  CV_PPC_GPR14
    L"r15",          // 16  CV_PPC_GPR15
    L"r16",          // 17  CV_PPC_GPR16
    L"r17",          // 18  CV_PPC_GPR17
    L"r18",          // 19  CV_PPC_GPR18
    L"r19",          // 20  CV_PPC_GPR19
    L"r20",          // 21  CV_PPC_GPR20
    L"r21",          // 22  CV_PPC_GPR21
    L"r22",          // 23  CV_PPC_GPR22
    L"r23",          // 24  CV_PPC_GPR23
    L"r24",          // 25  CV_PPC_GPR24
    L"r25",          // 26  CV_PPC_GPR25
    L"r26",          // 27  CV_PPC_GPR26
    L"r27",          // 28  CV_PPC_GPR27
    L"r28",          // 29  CV_PPC_GPR28
    L"r29",          // 30  CV_PPC_GPR29
    L"r30",          // 31  CV_PPC_GPR30
    L"r31",          // 32  CV_PPC_GPR31
    L"cr",           // 33  CV_PPC_CR
    L"cr0",          // 34  CV_PPC_CR0
    L"cr1",          // 35  CV_PPC_CR1
    L"cr2",          // 36  CV_PPC_CR2
    L"cr3",          // 37  CV_PPC_CR3
    L"cr4",          // 38  CV_PPC_CR4
    L"cr5",          // 39  CV_PPC_CR5
    L"cr6",          // 40  CV_PPC_CR6
    L"cr7",          // 41  CV_PPC_CR7
    L"f0",           // 42  CV_PPC_FPR0
    L"f1",           // 43  CV_PPC_FPR1
    L"f2",           // 44  CV_PPC_FPR2
    L"f3",           // 45  CV_PPC_FPR3
    L"f4",           // 46  CV_PPC_FPR4
    L"f5",           // 47  CV_PPC_FPR5
    L"f6",           // 48  CV_PPC_FPR6
    L"f7",           // 49  CV_PPC_FPR7
    L"f8",           // 50  CV_PPC_FPR8
    L"f9",           // 51  CV_PPC_FPR9
    L"f10",          // 52  CV_PPC_FPR10
    L"f11",          // 53  CV_PPC_FPR11
    L"f12",          // 54  CV_PPC_FPR12
    L"f13",          // 55  CV_PPC_FPR13
    L"f14",          // 56  CV_PPC_FPR14
    L"f15",          // 57  CV_PPC_FPR15
    L"f16",          // 58  CV_PPC_FPR16
    L"f17",          // 59  CV_PPC_FPR17
    L"f18",          // 60  CV_PPC_FPR18
    L"f19",          // 61  CV_PPC_FPR19
    L"f20",          // 62  CV_PPC_FPR20
    L"f21",          // 63  CV_PPC_FPR21
    L"f22",          // 64  CV_PPC_FPR22
    L"f23",          // 65  CV_PPC_FPR23
    L"f24",          // 66  CV_PPC_FPR24
    L"f25",          // 67  CV_PPC_FPR25
    L"f26",          // 68  CV_PPC_FPR26
    L"f27",          // 69  CV_PPC_FPR27
    L"f28",          // 70  CV_PPC_FPR28
    L"f29",          // 71  CV_PPC_FPR29
    L"f30",          // 72  CV_PPC_FPR30
    L"f31",          // 73  CV_PPC_FPR31
    L"Fpscr",        // 74  CV_PPC_FPSCR
    L"Msr",          // 75  CV_PPC_MSR
};


const wchar_t * const rgszRegSh[] = {
    L"None",         // 0   CV_SH3_NOREG
    L"???",          // 1
    L"???",          // 2
    L"???",          // 3
    L"???",          // 4
    L"???",          // 5
    L"???",          // 6
    L"???",          // 7
    L"???",          // 8
    L"???",          // 9
    L"r0",           // 10  CV_SH3_IntR0
    L"r1",           // 11  CV_SH3_IntR1
    L"r2",           // 12  CV_SH3_IntR2
    L"r3",           // 13  CV_SH3_IntR3
    L"r4",           // 14  CV_SH3_IntR4
    L"r5",           // 15  CV_SH3_IntR5
    L"r6",           // 16  CV_SH3_IntR6
    L"r7",           // 17  CV_SH3_IntR7
    L"r8",           // 18  CV_SH3_IntR8
    L"r9",           // 19  CV_SH3_IntR9
    L"r10",          // 20  CV_SH3_IntR10
    L"r11",          // 21  CV_SH3_IntR11
    L"r12",          // 22  CV_SH3_IntR12
    L"r13",          // 23  CV_SH3_IntR13
    L"fp",           // 24  CV_SH3_IntFp
    L"sp",           // 25  CV_SH3_IntSp
    L"???",          // 26
    L"???",          // 27
    L"???",          // 28
    L"???",          // 29
    L"???",          // 30
    L"???",          // 31
    L"???",          // 32
    L"???",          // 33
    L"???",          // 34
    L"???",          // 35
    L"???",          // 36
    L"???",          // 37
    L"gbr",          // 38  CV_SH3_Gbr
    L"pr",           // 39  CV_SH3_Pr
    L"mach",         // 40  CV_SH3_Mach
    L"macl",         // 41  CV_SH3_Macl
    L"???",          // 42
    L"???",          // 43
    L"???",          // 44
    L"???",          // 45
    L"???",          // 46
    L"???",          // 47
    L"???",          // 48
    L"???",          // 49
    L"pc",           // 50
    L"sr",           // 51
    L"???",          // 52
    L"???",          // 53
    L"???",          // 54
    L"???",          // 55
    L"???",          // 56
    L"???",          // 57
    L"???",          // 58
    L"???",          // 59
    L"bara",         // 60  CV_SH3_BarA
    L"basra",        // 61  CV_SH3_BasrA
    L"bamra",        // 62  CV_SH3_BamrA
    L"bbra",         // 63  CV_SH3_BbrA
    L"barb",         // 64  CV_SH3_BarB
    L"basrb",        // 65  CV_SH3_BasrB
    L"bamrb",        // 66  CV_SH3_BamrB
    L"bbrb",         // 67  CV_SH3_BbrB
    L"bdrb",         // 68  CV_SH3_BdrB
    L"bdmrb",        // 69  CV_SH3_BdmrB
    L"brcr",         // 70  CV_SH3_Brcr
};

typedef struct MapIa64Reg
{
    CV_HREG_e     iCvReg;
    const wchar_t *szRegName;
} MapIa64Reg;

const MapIa64Reg mpIa64regSz[] = {
    { CV_IA64_NOREG,       L"None"        },
    { CV_IA64_Br0,         L"br0"         },
    { CV_IA64_Br1,         L"br1"         },
    { CV_IA64_Br2,         L"br2"         },
    { CV_IA64_Br3,         L"br3"         },
    { CV_IA64_Br4,         L"br4"         },
    { CV_IA64_Br5,         L"br5"         },
    { CV_IA64_Br6,         L"br6"         },
    { CV_IA64_Br7,         L"br7"         },
    { CV_IA64_Preds,       L"preds"       },
    { CV_IA64_IntH0,       L"h0"          },
    { CV_IA64_IntH1,       L"h1"          },
    { CV_IA64_IntH2,       L"h2"          },
    { CV_IA64_IntH3,       L"h3"          },
    { CV_IA64_IntH4,       L"h4"          },
    { CV_IA64_IntH5,       L"h5"          },
    { CV_IA64_IntH6,       L"h6"          },
    { CV_IA64_IntH7,       L"h7"          },
    { CV_IA64_IntH8,       L"h8"          },
    { CV_IA64_IntH9,       L"h9"          },
    { CV_IA64_IntH10,      L"h10"         },
    { CV_IA64_IntH11,      L"h11"         },
    { CV_IA64_IntH12,      L"h12"         },
    { CV_IA64_IntH13,      L"h13"         },
    { CV_IA64_IntH14,      L"h14"         },
    { CV_IA64_IntH15,      L"h15"         },
    { CV_IA64_Ip,          L"ip"          },
    { CV_IA64_Umask,       L"umask"       },
    { CV_IA64_Cfm,         L"cfm"         },
    { CV_IA64_Psr,         L"psr"         },
    { CV_IA64_Nats,        L"nats"        },
    { CV_IA64_Nats2,       L"nats2"       },
    { CV_IA64_Nats3,       L"nats3"       },
    { CV_IA64_IntR0,       L"r0"          },
    { CV_IA64_IntR1,       L"r1"          },
    { CV_IA64_IntR2,       L"r2"          },
    { CV_IA64_IntR3,       L"r3"          },
    { CV_IA64_IntR4,       L"r4"          },
    { CV_IA64_IntR5,       L"r5"          },
    { CV_IA64_IntR6,       L"r6"          },
    { CV_IA64_IntR7,       L"r7"          },
    { CV_IA64_IntR8,       L"r8"          },
    { CV_IA64_IntR9,       L"r9"          },
    { CV_IA64_IntR10,      L"r10"         },
    { CV_IA64_IntR11,      L"r11"         },
    { CV_IA64_IntR12,      L"r12"         },
    { CV_IA64_IntR13,      L"r13"         },
    { CV_IA64_IntR14,      L"r14"         },
    { CV_IA64_IntR15,      L"r15"         },
    { CV_IA64_IntR16,      L"r16"         },
    { CV_IA64_IntR17,      L"r17"         },
    { CV_IA64_IntR18,      L"r18"         },
    { CV_IA64_IntR19,      L"r19"         },
    { CV_IA64_IntR20,      L"r20"         },
    { CV_IA64_IntR21,      L"r21"         },
    { CV_IA64_IntR22,      L"r22"         },
    { CV_IA64_IntR23,      L"r23"         },
    { CV_IA64_IntR24,      L"r24"         },
    { CV_IA64_IntR25,      L"r25"         },
    { CV_IA64_IntR26,      L"r26"         },
    { CV_IA64_IntR27,      L"r27"         },
    { CV_IA64_IntR28,      L"r28"         },
    { CV_IA64_IntR29,      L"r29"         },
    { CV_IA64_IntR30,      L"r30"         },
    { CV_IA64_IntR31,      L"r31"         },
    { CV_IA64_IntR32,      L"r32"         },
    { CV_IA64_IntR33,      L"r33"         },
    { CV_IA64_IntR34,      L"r34"         },
    { CV_IA64_IntR35,      L"r35"         },
    { CV_IA64_IntR36,      L"r36"         },
    { CV_IA64_IntR37,      L"r37"         },
    { CV_IA64_IntR38,      L"r38"         },
    { CV_IA64_IntR39,      L"r39"         },
    { CV_IA64_IntR40,      L"r40"         },
    { CV_IA64_IntR41,      L"r41"         },
    { CV_IA64_IntR42,      L"r42"         },
    { CV_IA64_IntR43,      L"r43"         },
    { CV_IA64_IntR44,      L"r44"         },
    { CV_IA64_IntR45,      L"r45"         },
    { CV_IA64_IntR46,      L"r46"         },
    { CV_IA64_IntR47,      L"r47"         },
    { CV_IA64_IntR48,      L"r48"         },
    { CV_IA64_IntR49,      L"r49"         },
    { CV_IA64_IntR50,      L"r50"         },
    { CV_IA64_IntR51,      L"r51"         },
    { CV_IA64_IntR52,      L"r52"         },
    { CV_IA64_IntR53,      L"r53"         },
    { CV_IA64_IntR54,      L"r54"         },
    { CV_IA64_IntR55,      L"r55"         },
    { CV_IA64_IntR56,      L"r56"         },
    { CV_IA64_IntR57,      L"r57"         },
    { CV_IA64_IntR58,      L"r58"         },
    { CV_IA64_IntR59,      L"r59"         },
    { CV_IA64_IntR60,      L"r60"         },
    { CV_IA64_IntR61,      L"r61"         },
    { CV_IA64_IntR62,      L"r62"         },
    { CV_IA64_IntR63,      L"r63"         },
    { CV_IA64_IntR64,      L"r64"         },
    { CV_IA64_IntR65,      L"r65"         },
    { CV_IA64_IntR66,      L"r66"         },
    { CV_IA64_IntR67,      L"r67"         },
    { CV_IA64_IntR68,      L"r68"         },
    { CV_IA64_IntR69,      L"r69"         },
    { CV_IA64_IntR70,      L"r70"         },
    { CV_IA64_IntR71,      L"r71"         },
    { CV_IA64_IntR72,      L"r72"         },
    { CV_IA64_IntR73,      L"r73"         },
    { CV_IA64_IntR74,      L"r74"         },
    { CV_IA64_IntR75,      L"r75"         },
    { CV_IA64_IntR76,      L"r76"         },
    { CV_IA64_IntR77,      L"r77"         },
    { CV_IA64_IntR78,      L"r78"         },
    { CV_IA64_IntR79,      L"r79"         },
    { CV_IA64_IntR80,      L"r80"         },
    { CV_IA64_IntR81,      L"r81"         },
    { CV_IA64_IntR82,      L"r82"         },
    { CV_IA64_IntR83,      L"r83"         },
    { CV_IA64_IntR84,      L"r84"         },
    { CV_IA64_IntR85,      L"r85"         },
    { CV_IA64_IntR86,      L"r86"         },
    { CV_IA64_IntR87,      L"r87"         },
    { CV_IA64_IntR88,      L"r88"         },
    { CV_IA64_IntR89,      L"r89"         },
    { CV_IA64_IntR90,      L"r90"         },
    { CV_IA64_IntR91,      L"r91"         },
    { CV_IA64_IntR92,      L"r92"         },
    { CV_IA64_IntR93,      L"r93"         },
    { CV_IA64_IntR94,      L"r94"         },
    { CV_IA64_IntR95,      L"r95"         },
    { CV_IA64_IntR96,      L"r96"         },
    { CV_IA64_IntR97,      L"r97"         },
    { CV_IA64_IntR98,      L"r98"         },
    { CV_IA64_IntR99,      L"r99"         },
    { CV_IA64_IntR100,     L"r100"        },
    { CV_IA64_IntR101,     L"r101"        },
    { CV_IA64_IntR102,     L"r102"        },
    { CV_IA64_IntR103,     L"r103"        },
    { CV_IA64_IntR104,     L"r104"        },
    { CV_IA64_IntR105,     L"r105"        },
    { CV_IA64_IntR106,     L"r106"        },
    { CV_IA64_IntR107,     L"r107"        },
    { CV_IA64_IntR108,     L"r108"        },
    { CV_IA64_IntR109,     L"r109"        },
    { CV_IA64_IntR110,     L"r110"        },
    { CV_IA64_IntR111,     L"r111"        },
    { CV_IA64_IntR112,     L"r112"        },
    { CV_IA64_IntR113,     L"r113"        },
    { CV_IA64_IntR114,     L"r114"        },
    { CV_IA64_IntR115,     L"r115"        },
    { CV_IA64_IntR116,     L"r116"        },
    { CV_IA64_IntR117,     L"r117"        },
    { CV_IA64_IntR118,     L"r118"        },
    { CV_IA64_IntR119,     L"r119"        },
    { CV_IA64_IntR120,     L"r120"        },
    { CV_IA64_IntR121,     L"r121"        },
    { CV_IA64_IntR122,     L"r122"        },
    { CV_IA64_IntR123,     L"r123"        },
    { CV_IA64_IntR124,     L"r124"        },
    { CV_IA64_IntR125,     L"r125"        },
    { CV_IA64_IntR126,     L"r126"        },
    { CV_IA64_IntR127,     L"r127"        },
    { CV_IA64_FltF0,       L"f0"          },
    { CV_IA64_FltF1,       L"f1"          },
    { CV_IA64_FltF2,       L"f2"          },
    { CV_IA64_FltF3,       L"f3"          },
    { CV_IA64_FltF4,       L"f4"          },
    { CV_IA64_FltF5,       L"f5"          },
    { CV_IA64_FltF6,       L"f6"          },
    { CV_IA64_FltF7,       L"f7"          },
    { CV_IA64_FltF8,       L"f8"          },
    { CV_IA64_FltF9,       L"f9"          },
    { CV_IA64_FltF10,      L"f10"         },
    { CV_IA64_FltF11,      L"f11"         },
    { CV_IA64_FltF12,      L"f12"         },
    { CV_IA64_FltF13,      L"f13"         },
    { CV_IA64_FltF14,      L"f14"         },
    { CV_IA64_FltF15,      L"f15"         },
    { CV_IA64_FltF16,      L"f16"         },
    { CV_IA64_FltF17,      L"f17"         },
    { CV_IA64_FltF18,      L"f18"         },
    { CV_IA64_FltF19,      L"f19"         },
    { CV_IA64_FltF20,      L"f20"         },
    { CV_IA64_FltF21,      L"f21"         },
    { CV_IA64_FltF22,      L"f22"         },
    { CV_IA64_FltF23,      L"f23"         },
    { CV_IA64_FltF24,      L"f24"         },
    { CV_IA64_FltF25,      L"f25"         },
    { CV_IA64_FltF26,      L"f26"         },
    { CV_IA64_FltF27,      L"f27"         },
    { CV_IA64_FltF28,      L"f28"         },
    { CV_IA64_FltF29,      L"f29"         },
    { CV_IA64_FltF30,      L"f30"         },
    { CV_IA64_FltF31,      L"f31"         },
    { CV_IA64_FltF32,      L"f32"         },
    { CV_IA64_FltF33,      L"f33"         },
    { CV_IA64_FltF34,      L"f34"         },
    { CV_IA64_FltF35,      L"f35"         },
    { CV_IA64_FltF36,      L"f36"         },
    { CV_IA64_FltF37,      L"f37"         },
    { CV_IA64_FltF38,      L"f38"         },
    { CV_IA64_FltF39,      L"f39"         },
    { CV_IA64_FltF40,      L"f40"         },
    { CV_IA64_FltF41,      L"f41"         },
    { CV_IA64_FltF42,      L"f42"         },
    { CV_IA64_FltF43,      L"f43"         },
    { CV_IA64_FltF44,      L"f44"         },
    { CV_IA64_FltF45,      L"f45"         },
    { CV_IA64_FltF46,      L"f46"         },
    { CV_IA64_FltF47,      L"f47"         },
    { CV_IA64_FltF48,      L"f48"         },
    { CV_IA64_FltF49,      L"f49"         },
    { CV_IA64_FltF50,      L"f50"         },
    { CV_IA64_FltF51,      L"f51"         },
    { CV_IA64_FltF52,      L"f52"         },
    { CV_IA64_FltF53,      L"f53"         },
    { CV_IA64_FltF54,      L"f54"         },
    { CV_IA64_FltF55,      L"f55"         },
    { CV_IA64_FltF56,      L"f56"         },
    { CV_IA64_FltF57,      L"f57"         },
    { CV_IA64_FltF58,      L"f58"         },
    { CV_IA64_FltF59,      L"f59"         },
    { CV_IA64_FltF60,      L"f60"         },
    { CV_IA64_FltF61,      L"f61"         },
    { CV_IA64_FltF62,      L"f62"         },
    { CV_IA64_FltF63,      L"f63"         },
    { CV_IA64_FltF64,      L"f64"         },
    { CV_IA64_FltF65,      L"f65"         },
    { CV_IA64_FltF66,      L"f66"         },
    { CV_IA64_FltF67,      L"f67"         },
    { CV_IA64_FltF68,      L"f68"         },
    { CV_IA64_FltF69,      L"f69"         },
    { CV_IA64_FltF70,      L"f70"         },
    { CV_IA64_FltF71,      L"f71"         },
    { CV_IA64_FltF72,      L"f72"         },
    { CV_IA64_FltF73,      L"f73"         },
    { CV_IA64_FltF74,      L"f74"         },
    { CV_IA64_FltF75,      L"f75"         },
    { CV_IA64_FltF76,      L"f76"         },
    { CV_IA64_FltF77,      L"f77"         },
    { CV_IA64_FltF78,      L"f78"         },
    { CV_IA64_FltF79,      L"f79"         },
    { CV_IA64_FltF80,      L"f80"         },
    { CV_IA64_FltF81,      L"f81"         },
    { CV_IA64_FltF82,      L"f82"         },
    { CV_IA64_FltF83,      L"f83"         },
    { CV_IA64_FltF84,      L"f84"         },
    { CV_IA64_FltF85,      L"f85"         },
    { CV_IA64_FltF86,      L"f86"         },
    { CV_IA64_FltF87,      L"f87"         },
    { CV_IA64_FltF88,      L"f88"         },
    { CV_IA64_FltF89,      L"f89"         },
    { CV_IA64_FltF90,      L"f90"         },
    { CV_IA64_FltF91,      L"f91"         },
    { CV_IA64_FltF92,      L"f92"         },
    { CV_IA64_FltF93,      L"f93"         },
    { CV_IA64_FltF94,      L"f94"         },
    { CV_IA64_FltF95,      L"f95"         },
    { CV_IA64_FltF96,      L"f96"         },
    { CV_IA64_FltF97,      L"f97"         },
    { CV_IA64_FltF98,      L"f98"         },
    { CV_IA64_FltF99,      L"f99"         },
    { CV_IA64_FltF100,     L"f100"        },
    { CV_IA64_FltF101,     L"f101"        },
    { CV_IA64_FltF102,     L"f102"        },
    { CV_IA64_FltF103,     L"f103"        },
    { CV_IA64_FltF104,     L"f104"        },
    { CV_IA64_FltF105,     L"f105"        },
    { CV_IA64_FltF106,     L"f106"        },
    { CV_IA64_FltF107,     L"f107"        },
    { CV_IA64_FltF108,     L"f108"        },
    { CV_IA64_FltF109,     L"f109"        },
    { CV_IA64_FltF110,     L"f110"        },
    { CV_IA64_FltF111,     L"f111"        },
    { CV_IA64_FltF112,     L"f112"        },
    { CV_IA64_FltF113,     L"f113"        },
    { CV_IA64_FltF114,     L"f114"        },
    { CV_IA64_FltF115,     L"f115"        },
    { CV_IA64_FltF116,     L"f116"        },
    { CV_IA64_FltF117,     L"f117"        },
    { CV_IA64_FltF118,     L"f118"        },
    { CV_IA64_FltF119,     L"f119"        },
    { CV_IA64_FltF120,     L"f120"        },
    { CV_IA64_FltF121,     L"f121"        },
    { CV_IA64_FltF122,     L"f122"        },
    { CV_IA64_FltF123,     L"f123"        },
    { CV_IA64_FltF124,     L"f124"        },
    { CV_IA64_FltF125,     L"f125"        },
    { CV_IA64_FltF126,     L"f126"        },
    { CV_IA64_FltF127,     L"f127"        },
    { CV_IA64_ApKR0,       L"apkr0"       },
    { CV_IA64_ApKR1,       L"apkr1"       },
    { CV_IA64_ApKR2,       L"apkr2"       },
    { CV_IA64_ApKR3,       L"apkr3"       },
    { CV_IA64_ApKR4,       L"apkr4"       },
    { CV_IA64_ApKR5,       L"apkr5"       },
    { CV_IA64_ApKR6,       L"apkr6"       },
    { CV_IA64_ApKR7,       L"apkr7"       },
    { CV_IA64_AR8,         L"ar8"         },
    { CV_IA64_AR9,         L"ar9"         },
    { CV_IA64_AR10,        L"ar10"        },
    { CV_IA64_AR11,        L"ar11"        },
    { CV_IA64_AR12,        L"ar12"        },
    { CV_IA64_AR13,        L"ar13"        },
    { CV_IA64_AR14,        L"ar14"        },
    { CV_IA64_AR15,        L"ar15"        },
    { CV_IA64_RsRSC,       L"rsbsc"       },
    { CV_IA64_RsBSP,       L"rsbsp"       },
    { CV_IA64_RsBSPSTORE,  L"rsbspstore"  },
    { CV_IA64_RsRNAT,      L"rsrnat"      },
    { CV_IA64_AR20,        L"ar20"        },
    { CV_IA64_StFCR,       L"stfcr"       },
    { CV_IA64_AR22,        L"ar22"        },
    { CV_IA64_AR23,        L"ar23"        },
    { CV_IA64_EFLAG,       L"eflag"       },
    { CV_IA64_CSD,         L"csd"         },
    { CV_IA64_SSD,         L"ssd"         },
    { CV_IA64_CFLG,        L"cflg"        },
    { CV_IA64_StFSR,       L"stfsr"       },
    { CV_IA64_StFIR,       L"stfir"       },
    { CV_IA64_StFDR,       L"stfdr"       },
    { CV_IA64_AR31,        L"ar31"        },
    { CV_IA64_ApCCV,       L"apccv"       },
    { CV_IA64_AR33,        L"ar33"        },
    { CV_IA64_AR34,        L"ar34"        },
    { CV_IA64_AR35,        L"ar35"        },
    { CV_IA64_ApUNAT,      L"apunat"      },
    { CV_IA64_AR37,        L"ar37"        },
    { CV_IA64_AR38,        L"ar38"        },
    { CV_IA64_AR39,        L"ar39"        },
    { CV_IA64_StFPSR,      L"stfpsr"      },
    { CV_IA64_AR41,        L"ar41"        },
    { CV_IA64_AR42,        L"ar42"        },
    { CV_IA64_AR43,        L"ar43"        },
    { CV_IA64_ApITC,       L"apitc"       },
    { CV_IA64_AR45,        L"ar45"        },
    { CV_IA64_AR46,        L"ar46"        },
    { CV_IA64_AR47,        L"ar47"        },
    { CV_IA64_AR48,        L"ar48"        },
    { CV_IA64_AR49,        L"ar49"        },
    { CV_IA64_AR50,        L"ar50"        },
    { CV_IA64_AR51,        L"ar51"        },
    { CV_IA64_AR52,        L"ar52"        },
    { CV_IA64_AR53,        L"ar53"        },
    { CV_IA64_AR54,        L"ar54"        },
    { CV_IA64_AR55,        L"ar55"        },
    { CV_IA64_AR56,        L"ar56"        },
    { CV_IA64_AR57,        L"ar57"        },
    { CV_IA64_AR58,        L"ar58"        },
    { CV_IA64_AR59,        L"ar59"        },
    { CV_IA64_AR60,        L"ar60"        },
    { CV_IA64_AR61,        L"ar61"        },
    { CV_IA64_AR62,        L"ar62"        },
    { CV_IA64_AR63,        L"ar63"        },
    { CV_IA64_RsPFS,       L"rspfs"       },
    { CV_IA64_ApLC,        L"aplc"        },
    { CV_IA64_ApEC,        L"apec"        },
    { CV_IA64_AR67,        L"ar67"        },
    { CV_IA64_AR68,        L"ar68"        },
    { CV_IA64_AR69,        L"ar69"        },
    { CV_IA64_AR70,        L"ar70"        },
    { CV_IA64_AR71,        L"ar71"        },
    { CV_IA64_AR72,        L"ar72"        },
    { CV_IA64_AR73,        L"ar73"        },
    { CV_IA64_AR74,        L"ar74"        },
    { CV_IA64_AR75,        L"ar75"        },
    { CV_IA64_AR76,        L"ar76"        },
    { CV_IA64_AR77,        L"ar77"        },
    { CV_IA64_AR78,        L"ar78"        },
    { CV_IA64_AR79,        L"ar79"        },
    { CV_IA64_AR80,        L"ar80"        },
    { CV_IA64_AR81,        L"ar81"        },
    { CV_IA64_AR82,        L"ar82"        },
    { CV_IA64_AR83,        L"ar83"        },
    { CV_IA64_AR84,        L"ar84"        },
    { CV_IA64_AR85,        L"ar85"        },
    { CV_IA64_AR86,        L"ar86"        },
    { CV_IA64_AR87,        L"ar87"        },
    { CV_IA64_AR88,        L"ar88"        },
    { CV_IA64_AR89,        L"ar89"        },
    { CV_IA64_AR90,        L"ar90"        },
    { CV_IA64_AR91,        L"ar91"        },
    { CV_IA64_AR92,        L"ar92"        },
    { CV_IA64_AR93,        L"ar93"        },
    { CV_IA64_AR94,        L"ar94"        },
    { CV_IA64_AR95,        L"ar95"        },
    { CV_IA64_AR96,        L"ar96"        },
    { CV_IA64_AR97,        L"ar97"        },
    { CV_IA64_AR98,        L"ar98"        },
    { CV_IA64_AR99,        L"ar99"        },
    { CV_IA64_AR100,       L"ar100"       },
    { CV_IA64_AR101,       L"ar101"       },
    { CV_IA64_AR102,       L"ar102"       },
    { CV_IA64_AR103,       L"ar103"       },
    { CV_IA64_AR104,       L"ar104"       },
    { CV_IA64_AR105,       L"ar105"       },
    { CV_IA64_AR106,       L"ar106"       },
    { CV_IA64_AR107,       L"ar107"       },
    { CV_IA64_AR108,       L"ar108"       },
    { CV_IA64_AR109,       L"ar109"       },
    { CV_IA64_AR110,       L"ar110"       },
    { CV_IA64_AR111,       L"ar111"       },
    { CV_IA64_AR112,       L"ar112"       },
    { CV_IA64_AR113,       L"ar113"       },
    { CV_IA64_AR114,       L"ar114"       },
    { CV_IA64_AR115,       L"ar115"       },
    { CV_IA64_AR116,       L"ar116"       },
    { CV_IA64_AR117,       L"ar117"       },
    { CV_IA64_AR118,       L"ar118"       },
    { CV_IA64_AR119,       L"ar119"       },
    { CV_IA64_AR120,       L"ar120"       },
    { CV_IA64_AR121,       L"ar121"       },
    { CV_IA64_AR122,       L"ar122"       },
    { CV_IA64_AR123,       L"ar123"       },
    { CV_IA64_AR124,       L"ar124"       },
    { CV_IA64_AR125,       L"ar125"       },
    { CV_IA64_AR126,       L"ar126"       },
    { CV_IA64_AR127,       L"ar127"       },
    { CV_IA64_CPUID0,      L"cpuid0"      },
    { CV_IA64_CPUID1,      L"cpuid1"      },
    { CV_IA64_CPUID2,      L"cpuid2"      },
    { CV_IA64_CPUID3,      L"cpuid3"      },
    { CV_IA64_CPUID4,      L"cpuid4"      },
    { CV_IA64_ApDCR,       L"apdcr"       },
    { CV_IA64_ApITM,       L"apitm"       },
    { CV_IA64_ApIVA,       L"apiva"       },
    { CV_IA64_CR3,         L"cr3"         },
    { CV_IA64_CR4,         L"cr4"         },
    { CV_IA64_CR5,         L"cr5"         },
    { CV_IA64_CR6,         L"cr6"         },
    { CV_IA64_CR7,         L"cr7"         },
    { CV_IA64_ApPTA,       L"appta"       },
    { CV_IA64_ApGPTA,      L"apgpta"      },
    { CV_IA64_CR10,        L"cr10"        },
    { CV_IA64_CR11,        L"cr11"        },
    { CV_IA64_CR12,        L"cr12"        },
    { CV_IA64_CR13,        L"cr13"        },
    { CV_IA64_CR14,        L"cr14"        },
    { CV_IA64_CR15,        L"cr15"        },
    { CV_IA64_StIPSR,      L"stipsr"      },
    { CV_IA64_StISR,       L"stisr"       },
    { CV_IA64_CR18,        L"cr18"        },
    { CV_IA64_StIIP,       L"stiip"       },
    { CV_IA64_StIFA,       L"stifa"       },
    { CV_IA64_StITIR,      L"stitir"      },
    { CV_IA64_StIIPA,      L"stiipa"      },
    { CV_IA64_StIFS,       L"stifs"       },
    { CV_IA64_StIIM,       L"stiim"       },
    { CV_IA64_StIHA,       L"stiha"       },
    { CV_IA64_CR26,        L"cr26"        },
    { CV_IA64_CR27,        L"cr27"        },
    { CV_IA64_CR28,        L"cr28"        },
    { CV_IA64_CR29,        L"cr29"        },
    { CV_IA64_CR30,        L"cr30"        },
    { CV_IA64_CR31,        L"cr31"        },
    { CV_IA64_CR32,        L"cr32"        },
    { CV_IA64_CR33,        L"cr33"        },
    { CV_IA64_CR34,        L"cr34"        },
    { CV_IA64_CR35,        L"cr35"        },
    { CV_IA64_CR36,        L"cr36"        },
    { CV_IA64_CR37,        L"cr37"        },
    { CV_IA64_CR38,        L"cr38"        },
    { CV_IA64_CR39,        L"cr39"        },
    { CV_IA64_CR40,        L"cr40"        },
    { CV_IA64_CR41,        L"cr41"        },
    { CV_IA64_CR42,        L"cr42"        },
    { CV_IA64_CR43,        L"cr43"        },
    { CV_IA64_CR44,        L"cr44"        },
    { CV_IA64_CR45,        L"cr45"        },
    { CV_IA64_CR46,        L"cr46"        },
    { CV_IA64_CR47,        L"cr47"        },
    { CV_IA64_CR48,        L"cr48"        },
    { CV_IA64_CR49,        L"cr49"        },
    { CV_IA64_CR50,        L"cr50"        },
    { CV_IA64_CR51,        L"cr51"        },
    { CV_IA64_CR52,        L"cr52"        },
    { CV_IA64_CR53,        L"cr53"        },
    { CV_IA64_CR54,        L"cr54"        },
    { CV_IA64_CR55,        L"cr55"        },
    { CV_IA64_CR56,        L"cr56"        },
    { CV_IA64_CR57,        L"cr57"        },
    { CV_IA64_CR58,        L"cr58"        },
    { CV_IA64_CR59,        L"cr59"        },
    { CV_IA64_CR60,        L"cr60"        },
    { CV_IA64_CR61,        L"cr61"        },
    { CV_IA64_CR62,        L"cr62"        },
    { CV_IA64_CR63,        L"cr63"        },
    { CV_IA64_SaLID,       L"salid"       },
    { CV_IA64_SaIVR,       L"saivr"       },
    { CV_IA64_SaTPR,       L"satpr"       },
    { CV_IA64_SaEOI,       L"saeoi"       },
    { CV_IA64_SaIRR0,      L"sairr0"      },
    { CV_IA64_SaIRR1,      L"sairr1"      },
    { CV_IA64_SaIRR2,      L"sairr2"      },
    { CV_IA64_SaIRR3,      L"sairr3"      },
    { CV_IA64_SaITV,       L"saitv"       },
    { CV_IA64_SaPMV,       L"sapmv"       },
    { CV_IA64_SaCMCV,      L"sacmcv"      },
    { CV_IA64_CR75,        L"cr75"        },
    { CV_IA64_CR76,        L"cr76"        },
    { CV_IA64_CR77,        L"cr77"        },
    { CV_IA64_CR78,        L"cr78"        },
    { CV_IA64_CR79,        L"cr79"        },
    { CV_IA64_SaLRR0,      L"salrr0"      },
    { CV_IA64_SaLRR1,      L"salrr1"      },
    { CV_IA64_CR82,        L"cr82"        },
    { CV_IA64_CR83,        L"cr83"        },
    { CV_IA64_CR84,        L"cr84"        },
    { CV_IA64_CR85,        L"cr85"        },
    { CV_IA64_CR86,        L"cr86"        },
    { CV_IA64_CR87,        L"cr87"        },
    { CV_IA64_CR88,        L"cr88"        },
    { CV_IA64_CR89,        L"cr89"        },
    { CV_IA64_CR90,        L"cr90"        },
    { CV_IA64_CR91,        L"cr91"        },
    { CV_IA64_CR92,        L"cr92"        },
    { CV_IA64_CR93,        L"cr93"        },
    { CV_IA64_CR94,        L"cr94"        },
    { CV_IA64_CR95,        L"cr95"        },
    { CV_IA64_CR96,        L"cr96"        },
    { CV_IA64_CR97,        L"cr97"        },
    { CV_IA64_CR98,        L"cr98"        },
    { CV_IA64_CR99,        L"cr99"        },
    { CV_IA64_CR100,       L"cr100"       },
    { CV_IA64_CR101,       L"cr101"       },
    { CV_IA64_CR102,       L"cr102"       },
    { CV_IA64_CR103,       L"cr103"       },
    { CV_IA64_CR104,       L"cr104"       },
    { CV_IA64_CR105,       L"cr105"       },
    { CV_IA64_CR106,       L"cr106"       },
    { CV_IA64_CR107,       L"cr107"       },
    { CV_IA64_CR108,       L"cr108"       },
    { CV_IA64_CR109,       L"cr109"       },
    { CV_IA64_CR110,       L"cr110"       },
    { CV_IA64_CR111,       L"cr111"       },
    { CV_IA64_CR112,       L"cr112"       },
    { CV_IA64_CR113,       L"cr113"       },
    { CV_IA64_CR114,       L"cr114"       },
    { CV_IA64_CR115,       L"cr115"       },
    { CV_IA64_CR116,       L"cr116"       },
    { CV_IA64_CR117,       L"cr117"       },
    { CV_IA64_CR118,       L"cr118"       },
    { CV_IA64_CR119,       L"cr119"       },
    { CV_IA64_CR120,       L"cr120"       },
    { CV_IA64_CR121,       L"cr121"       },
    { CV_IA64_CR122,       L"cr122"       },
    { CV_IA64_CR123,       L"cr123"       },
    { CV_IA64_CR124,       L"cr124"       },
    { CV_IA64_CR125,       L"cr125"       },
    { CV_IA64_CR126,       L"cr126"       },
    { CV_IA64_CR127,       L"cr127"       },
    { CV_IA64_Pkr0,        L"pkr0"        },
    { CV_IA64_Pkr1,        L"pkr1"        },
    { CV_IA64_Pkr2,        L"pkr2"        },
    { CV_IA64_Pkr3,        L"pkr3"        },
    { CV_IA64_Pkr4,        L"pkr4"        },
    { CV_IA64_Pkr5,        L"pkr5"        },
    { CV_IA64_Pkr6,        L"pkr6"        },
    { CV_IA64_Pkr7,        L"pkr7"        },
    { CV_IA64_Pkr8,        L"pkr8"        },
    { CV_IA64_Pkr9,        L"pkr9"        },
    { CV_IA64_Pkr10,       L"pkr10"       },
    { CV_IA64_Pkr11,       L"pkr11"       },
    { CV_IA64_Pkr12,       L"pkr12"       },
    { CV_IA64_Pkr13,       L"pkr13"       },
    { CV_IA64_Pkr14,       L"pkr14"       },
    { CV_IA64_Pkr15,       L"pkr15"       },
    { CV_IA64_Rr0,         L"rr0"         },
    { CV_IA64_Rr1,         L"rr1"         },
    { CV_IA64_Rr2,         L"rr2"         },
    { CV_IA64_Rr3,         L"rr3"         },
    { CV_IA64_Rr4,         L"rr4"         },
    { CV_IA64_Rr5,         L"rr5"         },
    { CV_IA64_Rr6,         L"rr6"         },
    { CV_IA64_Rr7,         L"rr7"         },
    { CV_IA64_PFD0,        L"pfd0"        },
    { CV_IA64_PFD1,        L"pfd1"        },
    { CV_IA64_PFD2,        L"pfd2"        },
    { CV_IA64_PFD3,        L"pfd3"        },
    { CV_IA64_PFD4,        L"pfd4"        },
    { CV_IA64_PFD5,        L"pfd5"        },
    { CV_IA64_PFD6,        L"pfd6"        },
    { CV_IA64_PFD7,        L"pfd7"        },
    { CV_IA64_PFD8,        L"pfd8"        },
    { CV_IA64_PFD9,        L"pfd9"        },
    { CV_IA64_PFD10,       L"pfd10"       },
    { CV_IA64_PFD11,       L"pfd11"       },
    { CV_IA64_PFD12,       L"pfd12"       },
    { CV_IA64_PFD13,       L"pfd13"       },
    { CV_IA64_PFD14,       L"pfd14"       },
    { CV_IA64_PFD15,       L"pfd15"       },
    { CV_IA64_PFD16,       L"pfd16"       },
    { CV_IA64_PFD17,       L"pfd17"       },
    { CV_IA64_PFC0,        L"pfc0"        },
    { CV_IA64_PFC1,        L"pfc1"        },
    { CV_IA64_PFC2,        L"pfc2"        },
    { CV_IA64_PFC3,        L"pfc3"        },
    { CV_IA64_PFC4,        L"pfc4"        },
    { CV_IA64_PFC5,        L"pfc5"        },
    { CV_IA64_PFC6,        L"pfc6"        },
    { CV_IA64_PFC7,        L"pfc7"        },
    { CV_IA64_PFC8,        L"pfc8"        },
    { CV_IA64_PFC9,        L"pfc9"        },
    { CV_IA64_PFC10,       L"pfc10"       },
    { CV_IA64_PFC11,       L"pfc11"       },
    { CV_IA64_PFC12,       L"pfc12"       },
    { CV_IA64_PFC13,       L"pfc13"       },
    { CV_IA64_PFC14,       L"pfc14"       },
    { CV_IA64_PFC15,       L"pfc15"       },
    { CV_IA64_TrI0,        L"tri0"        },
    { CV_IA64_TrI1,        L"tri1"        },
    { CV_IA64_TrI2,        L"tri2"        },
    { CV_IA64_TrI3,        L"tri3"        },
    { CV_IA64_TrI4,        L"tri4"        },
    { CV_IA64_TrI5,        L"tri5"        },
    { CV_IA64_TrI6,        L"tri6"        },
    { CV_IA64_TrI7,        L"tri7"        },
    { CV_IA64_TrD0,        L"trd0"        },
    { CV_IA64_TrD1,        L"trd1"        },
    { CV_IA64_TrD2,        L"trd2"        },
    { CV_IA64_TrD3,        L"trd3"        },
    { CV_IA64_TrD4,        L"trd4"        },
    { CV_IA64_TrD5,        L"trd5"        },
    { CV_IA64_TrD6,        L"trd6"        },
    { CV_IA64_TrD7,        L"trd7"        },
    { CV_IA64_DbI0,        L"dbi0"        },
    { CV_IA64_DbI1,        L"dbi1"        },
    { CV_IA64_DbI2,        L"dbi2"        },
    { CV_IA64_DbI3,        L"dbi3"        },
    { CV_IA64_DbI4,        L"dbi4"        },
    { CV_IA64_DbI5,        L"dbi5"        },
    { CV_IA64_DbI6,        L"dbi6"        },
    { CV_IA64_DbI7,        L"dbi7"        },
    { CV_IA64_DbD0,        L"dbd0"        },
    { CV_IA64_DbD1,        L"dbd1"        },
    { CV_IA64_DbD2,        L"dbd2"        },
    { CV_IA64_DbD3,        L"dbd3"        },
    { CV_IA64_DbD4,        L"dbd4"        },
    { CV_IA64_DbD5,        L"dbd5"        },
    { CV_IA64_DbD6,        L"dbd6"        },
    { CV_IA64_DbD7,        L"dbd7"        },
};

int __cdecl cmpIa64regSz(const void *pv1, const void *pv2)
{
    const MapIa64Reg *p1 = (MapIa64Reg *) pv1;
    const MapIa64Reg *p2 = (MapIa64Reg *) pv2;

    if (p1->iCvReg < p2->iCvReg) {
        return -1;
    }

    if (p1->iCvReg > p2->iCvReg) {
        return 1;
    }

    return 0;
}

const wchar_t *SzBadRegisterValue(unsigned reg)
{
    static wchar_t rgwch[16];

    swprintf_s(rgwch, _countof(rgwch), L"???" L"(0x%04x)", reg);

    return rgwch;
}

const wchar_t *SzNameFrameReg(WORD reg)
{
    WORD cvReg = CodeViewInfo::ExpandEncodedBasePointerReg(CVDumpMachineType, reg);
    if (cvReg != CV_REG_NONE) {
        return SzNameC7Reg(cvReg);
    } else {
        return L"default";
    }
}

const wchar_t *SzNameC7Reg(WORD reg)
{
    switch (CVDumpMachineType) {
        case CV_CFL_8080 :
        case CV_CFL_8086 :
        case CV_CFL_80286 :
        case CV_CFL_80386 :
        case CV_CFL_80486 :
        case CV_CFL_PENTIUM :
        case CV_CFL_PENTIUMII :
        case CV_CFL_PENTIUMIII :
            if (reg < _countof(rgszRegX86)) {
                return rgszRegX86[reg];
            }
            break;

        case CV_CFL_ALPHA :
        case CV_CFL_ALPHA_21164 :
        case CV_CFL_ALPHA_21164A :
        case CV_CFL_ALPHA_21264 :
        case CV_CFL_ALPHA_21364 :
            if (reg < _countof(rgszRegAlpha)) {
                return rgszRegAlpha[reg];
            }
            break;

        case CV_CFL_MIPS :
        case CV_CFL_MIPS16 :
        case CV_CFL_MIPS32 :
        case CV_CFL_MIPS64 :
        case CV_CFL_MIPSI :
        case CV_CFL_MIPSII :
        case CV_CFL_MIPSIII :
        case CV_CFL_MIPSIV :
        case CV_CFL_MIPSV :
            if (reg < _countof(rgszRegMips)) {
                return rgszRegMips[reg];
            }
            break;

        case CV_CFL_M68000 :
        case CV_CFL_M68010 :
        case CV_CFL_M68020 :
        case CV_CFL_M68030 :
        case CV_CFL_M68040 :
            if (reg < _countof(rgszReg68k)) {
                return rgszReg68k[reg];
            }
            break;

        case CV_CFL_PPC601 :
        case CV_CFL_PPC603 :
        case CV_CFL_PPC604 :
        case CV_CFL_PPC620 :
        case CV_CFL_PPCFP :
        case CV_CFL_PPCBE :
            if (reg < _countof(rgszRegPpc)) {
                return rgszRegPpc[reg];
            }
            break;

        case CV_CFL_SH3 :
        case CV_CFL_SH3E :
        case CV_CFL_SH3DSP :
        case CV_CFL_SH4 :
            if (reg < _countof(rgszRegSh)) {
                return rgszRegSh[reg];
            }
            break;

        case CV_CFL_ARM3 :
        case CV_CFL_ARM4 :
        case CV_CFL_ARM4T :
        case CV_CFL_ARM5 :
        case CV_CFL_ARM5T :
        case CV_CFL_ARM7 :
        case CV_CFL_THUMB :
        case CV_CFL_ARMNT :
            if (reg < _countof(rgszRegArm)) {
                return rgszRegArm[reg];
            }
            break;

        case CV_CFL_ARM64 :
            if (reg < _countof(rgszRegArm64)) {
                return rgszRegArm64[reg];
            }
            break;

        case CV_CFL_IA64_1 :
        case CV_CFL_IA64_2 : {
            MapIa64Reg *p;
            MapIa64Reg  m = { (CV_HREG_e) reg };

            p = (MapIa64Reg *) bsearch(&m,
                                       mpIa64regSz,
                                       _countof(mpIa64regSz),
                                       sizeof(MapIa64Reg),
                                       cmpIa64regSz);

            if (p) {
                return p->szRegName;
            }
            break;
        }

        case CV_CFL_AMD64 :
            if (reg < _countof(rgszRegAMD64)) {
                return rgszRegAMD64[reg];
            }
            break;

        default :
            break;
    }

    return SzBadRegisterValue(reg);
}


void C7RegSym(const REGSYM *psym)
{
    PrtIndent();

    StdOutPuts(L"S_REGISTER: ");

    StdOutPrintf(L"%s, Type: %18s, ",
                 SzNameC7Reg(psym->reg),
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}

void C7ManRegister(const ATTRREGSYM *psym)
{
    PrtIndent();

    StdOutPuts(L"S_MANREGISTER: ");

    StdOutPrintf(L"%s, ", SzNameC7Reg(psym->reg));
    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7AttrRegister(const ATTRREGSYM *psym)
{
    PrtIndent();

    StdOutPuts(L"S_ATTR_REGISTER: ");

    StdOutPrintf(L"%s, ", SzNameC7Reg(psym->reg));
    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7AttrManyReg(const ATTRMANYREGSYM2 *psym)
{
    PrtIndent();

    StdOutPuts(L"S_ATTR_MANYREG: ");

    const wchar_t *szSep = (psym->count > 1) ? L":" : L", ";

    for (unsigned ireg = 0; ireg < psym->count; ireg++) {
        StdOutPrintf(L"%s%s", SzNameC7Reg(psym->reg[ireg]), szSep);
        if (ireg >= unsigned(psym->count) - 2) {
            szSep = L", ";
        }
    }

    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7RegSym_16t(const REGSYM_16t *psym)
{
    PrtIndent();

    StdOutPuts(L"S_REGISTER_16t: ");

    // Leave old style packed enumerates for regs alone.
    // Current records never packed regs into this field.

    if ((psym->reg >> 8) != CV_REG_NONE) {
        StdOutPrintf(L"%s:", SzNameC7Reg((WORD)(psym->reg >> 8)));
    }

    StdOutPrintf(L"%s, Type: %18s, ",
           SzNameC7Reg((WORD) (psym->reg & 0xff)),
           SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7ConSym(const CONSTSYM *psym)
{
    unsigned char *pstName;      // Length prefixed name

    PrtIndent();

    StdOutPrintf(L"S_CONSTANT: Type: %18s, Value: ", SzNameC7Type2(psym->typind));
    pstName = (unsigned char *) &psym->value + PrintNumeric(&psym->value);
    ShowSt(fUtf8Symbols, L", ", pstName);
}

void C7ManConSym(const CONSTSYM *psym)
{
    unsigned char *pstName;      // Length prefixed name

    PrtIndent();

    StdOutPrintf(L"S_MANCONSTANT: Token: %08X, Value: ", psym->typind);
    pstName = (unsigned char *) &psym->value + PrintNumeric(&psym->value);
    ShowSt(fUtf8Symbols, L", ", pstName);
}

void C7ConSym_16t(const CONSTSYM_16t *psym)
{
    unsigned char *pstName;      // Length prefixed name

    PrtIndent();

    StdOutPrintf(L"S_CONSTANT_16t: Type: %18s, Value: ", SzNameC7Type2(psym->typind));
    pstName = (unsigned char *) &psym->value + PrintNumeric(&psym->value);
    ShowSt(fUtf8Symbols, L", ", pstName);
}


void C7ObjNameSym(const OBJNAMESYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_OBJNAME: Signature: %08X, ", psym->signature);
    PrintSt(fUtf8Symbols, psym->name);

    PrtSkip();
}


void C7UDTSym(const UDTSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_UDT: %18s, ", SzNameC7Type2(psym->typind));
    PrintSt(fUtf8Symbols, psym->name);
}


void C7UDTSym_16t(const UDTSYM_16t *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_UDT_16t: %18s, ", SzNameC7Type2(psym->typind));
    PrintSt(fUtf8Symbols, psym->name);
}


void C7CobolUDT(const UDTSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_COBOLUDT: %18s, ", SzNameC7Type2(psym->typind));
    PrintSt(fUtf8Symbols, psym->name);
}


void C7CobolUDT_16t(const UDTSYM_16t *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_COBOLUDT_16t: %18s, ", SzNameC7Type2(psym->typind));
    PrintSt(fUtf8Symbols, psym->name);
}


void C7RefSym(const REFSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"%s: 0x%08X: (%4d, %08X)\n",
                 (psym->rectyp == S_DATAREF) ? L"S_DATAREF" :
                     (psym->rectyp == S_PROCREF) ? L"S_PROCREF" : L"S_LPROCREF",
                 psym->sumName,
                 psym->imod,
                 psym->ibSym);
}


void C7RefSym2(const REFSYM2 *psym)
{
    PrtIndent();

    StdOutPrintf(L"%s: 0x%08X: (%4d, %08X) ",
                 (psym->rectyp == S_DATAREF) ? L"S_DATAREF" :
                     (psym->rectyp == S_PROCREF) ? L"S_PROCREF" : L"S_LPROCREF",
                 psym->sumName,
                 psym->imod,
                 psym->ibSym);
    PrintSt(fUtf8Symbols, psym->name);
}


void C7AnnotationRefSym(const REFSYM2 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_ANNOTATIONREF: 0x%08X: (%4d, %08X) %S\n",
                 psym->sumName,
                 psym->imod,
                 psym->ibSym,
                 psym->name);
}


void C7RefMiniPDB(const REFMINIPDB *psym)
{
    PrtIndent();

    if (psym->fUDT) {
        StdOutPrintf(L"S_REF_MINIPDB: (UDT) imod = %04X, TI = %s, %S\n",
                     psym->imod,
                     SzNameC7Type(psym->typind),
                     psym->name);
    } else {
        StdOutPrintf(L"S_REF_MINIPDB: (%s %s) imod = %04X, isectCoff = %X, %S\n",
                     psym->fLocal ? L"local" : L"global",
                     psym->fData ? L"data" : (psym->fLabel ? L"label" : (psym->fConst ? L"const" : L"func")),
                     psym->imod,
                     psym->isectCoff,
                     psym->name);
    }
}


void C7PDBMap(const PDBMAP *psym)
{
    PrtIndent();

    const wchar_t *szFrom = (const wchar_t *) psym->name;
    const wchar_t *szTo = szFrom + wcslen(szFrom) + 1;

    StdOutPrintf(L"S_PDBMAP: %s -> %s\n", szFrom, szTo);
}


void C7TokenRefSym(const REFSYM2 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_TOKENREF: 0x%08X: (%4d, %08X) %S\n",
                 psym->sumName,
                 psym->imod,
                 psym->ibSym,
                 psym->name);
}


void C7OemSym(const OEMSYMBOL *psym)
{
    PrtIndent();

    wchar_t szGuid[39];

    StringFromGUID2(*(GUID *) psym->idOem, szGuid, sizeof(szGuid) / sizeof(wchar_t));

    StdOutPrintf(L"S_OEM: %s, Type: %18s\n",
                 szGuid,
                 SzNameC7Type2(psym->typind));

    static const GUID SS_OEMID = { 0xc6ea3fc9, 0x59b3, 0x49d6, 0xbc, 0x25, 0x09, 0x02, 0xbb, 0xab, 0xb4, 0x60 };

    if (IsEqualGUID(*(GUID *) psym->idOem, SS_OEMID)) {
        // UNDONE: Print name and data
    }
}


void PrintProcFlags(CV_PROCFLAGS cvpf)
{
    if (cvpf.bAll == 0) {
        return;
    }

    PrtIndent(false);

    StdOutPuts(L"Flags: ");

    bool fComma = false;

    if (cvpf.CV_PFLAG_NOFPO) {
        StdOutPuts(L"Frame Ptr Present");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_INT) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Interrupt");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_FAR) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"FAR");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_NEVER) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Never Return");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_NOTREACHED) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Not Reached");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_CUST_CALL) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Custom Calling Convention");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_NOINLINE) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Do Not Inline");

        fComma = true;
    }

    if (cvpf.CV_PFLAG_OPTDBGINFO) {
        if (fComma) {
            StdOutPuts(L", ");
        }

        StdOutPuts(L"Optimized Debug Info");

        fComma = true;
    }

    StdOutPutc(L'\n');
}


void C7Proc16Sym(const PROCSYM16 *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%04X], Cb: %04X, Type: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->len,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %04X, Debug end: %04X\n",
                 psym->DbgStart,
                 psym->DbgEnd);

    PrintProcFlags(psym->flags);

    cchIndent++;

    PrtSkip();
}


void C7LProc16Sym(const PROCSYM16 *psym)
{
    C7Proc16Sym(psym, L"S_LPROC16");
}


void C7GProc16Sym(const PROCSYM16 *psym)
{
    C7Proc16Sym(psym, L"S_GPROC16");
}


void C7Proc32Sym_16t(const PROCSYM32_16t *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, Type: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->len,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
                 psym->DbgStart,
                 psym->DbgEnd);

    PrintProcFlags(psym->flags);

    cchIndent++;

    PrtSkip();
}


void C7LProc32Sym_16t(const PROCSYM32_16t *psym)
{
    C7Proc32Sym_16t(psym, L"S_LPROC32_16t");
}


void C7GProc32Sym_16t(const PROCSYM32_16t *psym)
{
    C7Proc32Sym_16t(psym, L"S_GPROC32_16t");
}


void C7Proc32Sym(const PROCSYM32 *psym, const wchar_t *szSymType, bool fId)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, %s: %18s, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->len,
                 fId ? L"ID" : L"Type",
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
                 psym->DbgStart,
                 psym->DbgEnd);

    PrintProcFlags(psym->flags);

    cchIndent++;

    PrtSkip();
}

#if defined(CC_DP_CXX)
void C7LProc32Sym_DPC(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_LPROC32_DPC", false);
}

void C7LProc32IdSym_DPC(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_LPROC32_DPC_ID", true);
}
#endif // CC_DP_CXX

void C7LProc32Sym(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_LPROC32", false);
}

void C7LProc32IdSym(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_LPROC32_ID", true);
}

void C7GProc32Sym(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_GPROC32", false);
}

void C7GProc32IdSym(const PROCSYM32 *psym)
{
    C7Proc32Sym(psym, L"S_GPROC32_ID", true);
}

void C7ManProcSym(const MANPROCSYM *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    if (psym->token == PdbMapToken(psym->token)) {
        StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, Token: %08X, ",
               szSymType,
               psym->seg, psym->off,
               psym->len,
               psym->token);
    } else {
        StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, Token: %08X (mapped to %08X), ",
               szSymType,
               psym->seg, psym->off,
               psym->len,
               psym->token,
               PdbMapToken( psym->token ));
    }

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
           psym->pParent,
           psym->pEnd,
           psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
           psym->DbgStart,
           psym->DbgEnd);

    PrintProcFlags(psym->flags);

    PrtIndent(false);
    StdOutPrintf(L"Return Reg: %s\n",
           SzNameC7Reg(psym->retReg));

    cchIndent++;

    PrtSkip();
}


void C7LManProc(const MANPROCSYM *psym)
{
    C7ManProcSym(psym, L"S_LMANPROC");
}

void C7GManProc(const MANPROCSYM *psym)
{
    C7ManProcSym(psym, L"S_GMANPROC");
}

void C7ProcMipsSym(const PROCSYMMIPS *psym, const wchar_t *szSymType, bool fId)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, %s: %18s, ",
           szSymType,
           psym->seg, psym->off,
           psym->len,
           fId ? L"ID" : L"Type",
           SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
           psym->pParent,
           psym->pEnd,
           psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
           psym->DbgStart,
           psym->DbgEnd);

    PrtIndent(false);
    StdOutPrintf(L"Reg Save: %08X, FP Save: %08X, Int Off: %08X, FP Off: = %08X,\n",
           psym->regSave,
           psym->fpSave,
           psym->intOff,
           psym->fpOff);

    PrtIndent(false);
    StdOutPrintf(L"Return Reg: %s, Frame Reg: = %s\n",
           rgszRegMips[psym->retReg],
           rgszRegMips[psym->frameReg]);

    cchIndent++;

    PrtSkip();
}


void C7LProcMipsSym(const PROCSYMMIPS *psym)
{
    C7ProcMipsSym(psym, L"S_LPROCMIPS", false);
}

void C7LProcMipsIdSym(const PROCSYMMIPS *psym)
{
    C7ProcMipsSym(psym, L"S_LPROCMIPS_ID", true);
}

void C7GProcMipsSym(const PROCSYMMIPS *psym)
{
    C7ProcMipsSym(psym, L"S_GPROCMIPS", false);
}

void C7GProcMipsIdSym(const PROCSYMMIPS *psym)
{
    C7ProcMipsSym(psym, L"S_GPROCMIPS_ID", true);
}

void C7ProcMipsSym_16t(const PROCSYMMIPS_16t *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, Type: %18s, ",
           szSymType,
           psym->seg, psym->off,
           psym->len,
           SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
           psym->pParent,
           psym->pEnd,
           psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
           psym->DbgStart,
           psym->DbgEnd);

    PrtIndent(false);
    StdOutPrintf(L"Reg Save: %08X, FP Save: %08X, Int Off: %08X, FP Off: = %08X,\n",
           psym->regSave,
           psym->fpSave,
           psym->intOff,
           psym->fpOff);

    PrtIndent(false);
    StdOutPrintf(L"Return Reg: %s, Frame Reg: = %s\n",
           rgszRegMips[psym->retReg],
           rgszRegMips[psym->frameReg]);

    cchIndent++;

    PrtSkip();
}


void C7LProcMipsSym_16t(const PROCSYMMIPS_16t *psym)
{
    C7ProcMipsSym_16t(psym, L"S_LPROCMIPS_16t");
}


void C7GProcMipsSym_16t(const PROCSYMMIPS_16t *psym)
{
    C7ProcMipsSym_16t(psym, L"S_GPROCMIPS_16t");
}


void C7ProcIa64Sym(const PROCSYMIA64 *psym, const wchar_t *szSymType, bool fId)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, %s: %18s, ",
           szSymType,
           psym->seg, psym->off,
           psym->len,
           fId ? L"ID" : L"Type",
           SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
           psym->pParent,
           psym->pEnd,
           psym->pNext);

    PrtIndent(false);
    StdOutPrintf(L"Debug start: %08X, Debug end: %08X\n",
           psym->DbgStart,
           psym->DbgEnd);

    PrintProcFlags(psym->flags);

    PrtIndent(false);
    StdOutPrintf(L"Return Reg: %s\n",
           SzNameC7Reg(psym->retReg));

    cchIndent++;

    PrtSkip();
}


void C7LProcIa64Sym(const PROCSYMIA64 *psym)
{
    C7ProcIa64Sym(psym, L"S_LPROCIA64", false);
}

void C7LProcIa64IdSym(const PROCSYMIA64 *psym)
{
    C7ProcIa64Sym(psym, L"S_LPROCIA64_ID", true);
}

void C7GProcIa64Sym(const PROCSYMIA64 *psym)
{
    C7ProcIa64Sym(psym, L"S_GPROCIA64", false);
}

void C7GProcIa64IdSym(const PROCSYMIA64 *psym)
{
    C7ProcIa64Sym(psym, L"S_GPROCIA64_ID", true);
}

void C7ChangeModel16Sym(const CEXMSYM16 *psym)
{
    StdOutPuts(L"S_CEXMODEL16:\n");

    PrtIndent(false);
    StdOutPrintf(L"segment, offset = %04X:%04X, model = ", psym->seg, psym->off);

    switch ( psym->model) {
        case CEXM_MDL_table :
            StdOutPuts(L"DATA\n");
            break;

        case CEXM_MDL_native :
            StdOutPuts(L"NATIVE\n");
            break;

        case CEXM_MDL_cobol :
            StdOutPuts(L"COBOL\n");

            PrtIndent(false);
            switch (psym->cobol.subtype) {
                case 0x00 :
                    StdOutPuts(L"don't stop until next execution model\n");
                    break;

                case 0x01 :
                    StdOutPuts(L"inter segment perform - treat as single call instruction\n");
                    break;

                case 0x02 :
                    StdOutPuts(L"false call - step into even with F10\n");
                    break;

                case 0x03 :
                    StdOutPrintf(L"call to EXTCALL - step into %d call levels\n", psym->cobol.flag);
                    break;

                default :
                    StdOutPrintf(L"UNKNOWN COBOL CONTROL 0x%04X\n", psym->cobol.subtype);
                    break;
            }
            break;

        case CEXM_MDL_pcode :
            StdOutPuts(L"PCODE\n");

            PrtIndent(false);
            StdOutPrintf(L"pcdtable = %04X, pcdspi = %04X\n",
                         psym->pcode.pcdtable,
                         psym->pcode.pcdspi);
            break;

        default :
            StdOutPrintf(L"UNKNOWN MODEL = %04X\n", psym->model);
    }
}


void C7ChangeModel32Sym(const CEXMSYM32 *psym)
{
    StdOutPuts(L"S_CEXMODEL32:\n");

    PrtIndent(false);
    StdOutPrintf(L"segment, offset = %04X:%08X, model = ", psym->seg, psym->off);

    switch (psym->model) {
        case CEXM_MDL_table :
            StdOutPuts(L"DATA\n");
            break;

        case CEXM_MDL_native :
            StdOutPuts(L"NATIVE\n");
            break;

        case CEXM_MDL_cobol :
            StdOutPuts(L"COBOL\n");

            PrtIndent(false);
            switch (psym->cobol.subtype) {
                case 0x00 :
                    StdOutPuts(L"don't stop until next execution model\n");
                    break;

                case 0x01 :
                    StdOutPuts(L"inter segment perform - treat as single call instruction\n");
                    break;

                case 0x02 :
                    StdOutPuts(L"false call - step into even with F10\n");
                    break;

                case 0x03 :
                    StdOutPrintf(L"call to EXTCALL - step into %d call levels\n", psym->cobol.flag);
                    break;

                default :
                    StdOutPrintf(L"UNKNOWN COBOL CONTROL 0x%04X\n", psym->cobol.subtype);
                    break;
            }
            break;

        case CEXM_MDL_pcode :
            StdOutPuts(L"PCODE\n");

            PrtIndent(false);
            StdOutPrintf(L"pcdtable = %08X, pcdspi = %08X\n",
                         psym->pcode.pcdtable,
                         psym->pcode.pcdspi);
            break;

        case CEXM_MDL_pcode32Mac :
            StdOutPuts(L"PCODE for the Mac\n");

            PrtIndent(false);
            StdOutPrintf(L"callTable = %08X, segment = %08X\n",
                         psym->pcode32Mac.calltableOff,
                         psym->pcode32Mac.calltableSeg);
            break;

        case CEXM_MDL_pcode32MacNep :
            StdOutPuts(L"PCODE for the Mac (Native Entry Point)\n");

            PrtIndent(false);
            StdOutPrintf(L"callTable = %08X, segment = %08X\n",
                         psym->pcode32Mac.calltableOff,
                         psym->pcode32Mac.calltableSeg);
            break;

        default :
            StdOutPrintf(L"UNKNOWN MODEL = %04X\n", psym->model);
    }
}


void C7Thunk16Sym(const THUNKSYM16 *psym)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"S_THUNK16: [%04X:%04X], Cb: %04X, ",
                 psym->seg, psym->off,
                 psym->len);

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->pNext);

    const void *pVariant = psym->name + *psym->name + 1;

    switch (psym->ord) {
        case THUNK_ORDINAL_NOTYPE :
            break;

        case THUNK_ORDINAL_ADJUSTOR :
            PrtIndent(false);
            StdOutPrintf(L"Type: Adjustor, Delta: = %d, Target: ", *(short *) pVariant);
            PrintSt(fUtf8Symbols, (unsigned char *) (((short *) pVariant) + 1));
            break;

        case THUNK_ORDINAL_VCALL :
            PrtIndent(false);
            StdOutPrintf(L"Type: VCall, Table Entry: %d\n", *(short *) pVariant);
            break;

        default :
            PrtIndent(false);
            StdOutPrintf(L"Type: %02X\n", psym->ord);
            break;
    }

    cchIndent++;

    PrtSkip();
}


void C7Thunk32Sym(const THUNKSYM32 *psym)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"S_THUNK32: [%04X:%08X], Cb: %08X, ",
                 psym->seg, psym->off,
                 psym->len);

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X, Next: %08X\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->pNext);

    const void *pVariant = psym->name + *psym->name + 1;

    switch (psym->ord) {
        case THUNK_ORDINAL_NOTYPE :
            break;

        case THUNK_ORDINAL_ADJUSTOR :
            PrtIndent(false);
            StdOutPrintf(L"Type: Adjustor, Delta: = %d, Target: ", *(short *) pVariant);
            PrintSt(fUtf8Symbols, (unsigned char *) (((short *) pVariant) + 1));
            break;

        case THUNK_ORDINAL_VCALL :
            PrtIndent(false);
            StdOutPrintf(L"Type: VCall, Table Entry: %d\n", *(short *) pVariant);
            break;

        default :
            PrtIndent(false);
            StdOutPrintf(L"Type: %02X\n", psym->ord);
            break;
    }

    cchIndent++;

    PrtSkip();
}


void C7XBlock16Sym(const BLOCKSYM16 *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%04X], Cb: %04X, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->len);

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X\n",
                 psym->pParent,
                 psym->pEnd);

    cchIndent++;

    PrtSkip();
}


void C7Block16Sym(const BLOCKSYM16 *psym)
{
    C7XBlock16Sym(psym, L"S_BLOCK16");
}


void C7With16Sym(const BLOCKSYM16 *psym)
{
    C7XBlock16Sym(psym, L"S_WITH16");
}


void C7XBlock32Sym(const BLOCKSYM32 *psym, const wchar_t *szSymType)
{
    PrtSkip();

    PrtIndent();
    StdOutPrintf(L"%s: [%04X:%08X], Cb: %08X, ",
                 szSymType,
                 psym->seg, psym->off,
                 psym->len);

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent(false);
    StdOutPrintf(L"Parent: %08X, End: %08X\n",
                 psym->pParent,
                 psym->pEnd);

    cchIndent++;

    PrtSkip();
}


void C7Block32Sym(const BLOCKSYM32 *psym)
{
    C7XBlock32Sym(psym, L"S_BLOCK32");
}


void C7With32Sym(const BLOCKSYM32 *psym)
{
    C7XBlock32Sym(psym, L"S_WITH32");
}


void C7Lab16Sym(const LABELSYM16 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_LABEL16: [%04X:%04X], ",
                 psym->seg, psym->off);

    PrintSt(fUtf8Symbols, psym->name);

    PrintProcFlags(psym->flags);
}


void C7Lab32Sym(const LABELSYM32 *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_LABEL32: [%04X:%08X], ",
                 psym->seg, psym->off);

    PrintSt(fUtf8Symbols, psym->name);

    PrintProcFlags(psym->flags);
}


void C7StartSearchSym(const SEARCHSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"Start search for segment 0x%X at symbol 0x%X",
                 psym->seg, psym->startsym);
}


void C7SkipSym(const SYMTYPE *psym)
{
    PrtIndent();

    StdOutPrintf(L"Skip Record, Length = 0x%X\n", psym->reclen);
}


void C7AlignSym(const ALIGNSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"Align Record, Length = 0x%X", psym->reclen);
}


const wchar_t * const ModelStrings[] = {
    L"NEAR",                           // CV_CFL_xNEAR
    L"FAR",                            // CV_CFL_xFAR
    L"HUGE",                           // CV_CFL_xHUGE
    L"???"
};


const wchar_t * const FloatPackageStrings[] = {
    L"hardware",                       // CV_CFL_NDP
    L"emulator",                       // CV_CFL_EMU
    L"altmath",                        // CV_CFL_ALT
    L"???"
};


const wchar_t * const ProcessorStrings[CV_CFL_D3D11_SHADER + 1] = {
    L"8080",                           //  CV_CFL_8080
    L"8086",                           //  CV_CFL_8086
    L"80286",                          //  CV_CFL_80286
    L"80386",                          //  CV_CFL_80386
    L"80486",                          //  CV_CFL_80486
    L"Pentium",                        //  CV_CFL_PENTIUM
    L"Pentium Pro/Pentium II",         //  CV_CFL_PENTIUMII/CV_CFL_PENTIUMPRO
    L"Pentium III",                    //  CV_CFL_PENTIUMIII
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"MIPS (Generic)",                 //  CV_CFL_MIPSR4000
    L"MIPS16",                         //  CV_CFL_MIPS16
    L"MIPS32",                         //  CV_CFL_MIPS32
    L"MIPS64",                         //  CV_CFL_MIPS64
    L"MIPS I",                         //  CV_CFL_MIPSI
    L"MIPS II",                        //  CV_CFL_MIPSII
    L"MIPS III",                       //  CV_CFL_MIPSIII
    L"MIPS IV",                        //  CV_CFL_MIPSIV
    L"MIPS V",                         //  CV_CFL_MIPSV
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"M68000",                         //  CV_CFL_M68000
    L"M68010",                         //  CV_CFL_M68010
    L"M68020",                         //  CV_CFL_M68020
    L"M68030",                         //  CV_CFL_M68030
    L"M68040",                         //  CV_CFL_M68040
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"Alpha 21064",                    // CV_CFL_ALPHA, CV_CFL_ALPHA_21064
    L"Alpha 21164",                    // CV_CFL_ALPHA_21164
    L"Alpha 21164A",                   // CV_CFL_ALPHA_21164A
    L"Alpha 21264",                    // CV_CFL_ALPHA_21264
    L"Alpha 21364",                    // CV_CFL_ALPHA_21364
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"PPC 601",                        // CV_CFL_PPC601
    L"PPC 603",                        // CV_CFL_PPC603
    L"PPC 604",                        // CV_CFL_PPC604
    L"PPC 620",                        // CV_CFL_PPC620
    L"PPC w/FP",                       // CV_CFL_PPCFP
    L"PPC (Big Endian)",               // CV_CFL_PPCBE
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"SH3",                            // CV_CFL_SH3
    L"SH3E",                           // CV_CFL_SH3E
    L"SH3DSP",                         // CV_CFL_SH3DSP
    L"SH4",                            // CV_CFL_SH4
    L"SHmedia",                        // CV_CFL_SHMEDIA
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"ARMv3 (CE)",                     // CV_CFL_ARM3
    L"ARMv4 (CE)",                     // CV_CFL_ARM4
    L"ARMv4T (CE)",                    // CV_CFL_ARM4T
    L"ARMv5 (CE)",                     // CV_CFL_ARM5
    L"ARMv5T (CE)",                    // CV_CFL_ARM5T
    L"ARMv6 (CE)",                     // CV_CFL_ARM6
    L"ARM (XMAC) (CE)",                // CV_CFL_ARM_XMAC
    L"ARM (WMMX) (CE)",                // CV_CFL_ARM_WMMX
    L"ARMv7 (CE)",                     // CV_CFL_ARM7
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"Omni",                           // CV_CFL_OMNI
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"Itanium",                        // CV_CFL_IA64, CV_CFL_IA64_1
    L"Itanium (McKinley)",             // CV_CFL_IA64_2
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"CEE",                            // CV_CFL_CEE
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"AM33",                           // CV_CFL_AM33
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"M32R",                           // CV_CFL_M32R
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"TriCore",                        // CV_CFL_TRICORE
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"x64",                            // CV_CFL_X64
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"EBC",                            // CV_CFL_EBC
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"Thumb (CE)",                     // CV_CFL_THUMB
    L"???",
    L"???",
    L"???",
    L"ARM",                            // CV_CFL_ARMNT
    L"???",
    L"ARM64",                          // CV_CFL_ARM64
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"???",
    L"D3D11_SHADER",                   // CV_CFL_D3D11_SHADER
};

#define MAX_PROCESSOR_STRINGS   ( sizeof(ProcessorStrings) / sizeof(wchar_t *) )

const wchar_t * const LanguageIdStrings[] = {
    L"C",                              // CV_CFL_C
    L"C++",                            // CV_CFL_CXX
    L"FORTRAN",                        // CV_CFL_FORTRAN
    L"MASM",                           // CV_CFL_MASM
    L"Pascal",                         // CV_CFL_PASCAL
    L"Basic",                          // CV_CFL_BASIC
    L"COBOL",                          // CV_CFL_COBOL
    L"LINK",                           // CV_CFL_LINK
    L"CVTRES",                         // CV_CFL_CVTRES
    L"CVTPGD",                         // CV_CFL_CVTPGD
#if 0
    L"C\u266F",                        // CV_CFL_CSHARP
#else
    L"C#",                             // CV_CFL_CSHARP
#endif
    L"Visual Basic",                   // CV_CFL_VB
    L"ILASM",                          // CV_CFL_ILASM
    L"Java",                           // CV_CFL_JAVA
    L"JScript",                        // CV_CFL_JSCRIPT
    L"MSIL",                           // CV_CFL_MSIL
    L"HLSL",                           // CV_CFL_HLSL
};

#define MAX_LANGUAGE_STRINGS   ( sizeof(LanguageIdStrings) / sizeof(wchar_t *) )


void C7CompileFlags(const CFLAGSYM *pSym)
{
    PrtIndent();

    StdOutPuts(L"S_COMPILE:\n");

    PrtIndent(false);
    StdOutPrintf(L"Language: %s\n",
                 (pSym->flags.language < MAX_LANGUAGE_STRINGS) ? LanguageIdStrings[pSym->flags.language] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Target processor: %s\n",
                 (pSym->machine < MAX_PROCESSOR_STRINGS) ? ProcessorStrings[pSym->machine] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Floating-point precision: %d\n", pSym->flags.floatprec);

    PrtIndent(false);
    StdOutPrintf(L"Floating-point package: %s\n", FloatPackageStrings[pSym->flags.floatpkg]);

    PrtIndent(false);
    StdOutPrintf(L"Ambient data: %s\n", ModelStrings[pSym->flags.ambdata]);

    PrtIndent(false);
    StdOutPrintf(L"Ambient code: %s\n", ModelStrings[pSym->flags.ambcode]);

    PrtIndent(false);
    StdOutPrintf(L"PCode present: %d\n", pSym->flags.pcode);

    PrtIndent(false);
    ShowSt(false, L"Compiler Version: ", pSym->ver);

    PrtSkip();

    // MBH - this is a side-effect.
    // Later print-outs depend on the machine for which this was
    // compiled.  We have that info now, not later, so remember
    // it globally.

    CVDumpMachineType = pSym->machine;
}


void C7Slink32(const SLINK32 *pSym)
{
    PrtIndent();

    StdOutPrintf(L"SLINK32: framesize = %08X, off = %08X, reg = %s",
           pSym->framesize,
           pSym->off,
           SzNameC7Reg(pSym->reg));
}


void C7FrameProc(const FRAMEPROCSYM *pfps)
{
    PrtIndent();

    StdOutPuts(L"S_FRAMEPROC:\n");

    PrtIndent(false);
    StdOutPrintf(L"Frame size = 0x%08X bytes\n", pfps->cbFrame);

    PrtIndent(false);
    StdOutPrintf(L"Pad size = 0x%08X bytes\n", pfps->cbPad);

    PrtIndent(false);
    StdOutPrintf(L"Offset of pad in frame = 0x%08X\n", pfps->offPad);

    PrtIndent(false);
    StdOutPrintf(L"Size of callee save registers = 0x%08X\n", pfps->cbSaveRegs);

    PrtIndent(false);
    StdOutPrintf(L"Address of exception handler = %04X:%08X\n", pfps->sectExHdlr, pfps->offExHdlr);

    unsigned long ulFlags = *(unsigned long UNALIGNED *) (&pfps->flags);

    PrtIndent(false);
    StdOutPuts(L"Function info: ");
    StdOutPrintf(L"%s", pfps->flags.fHasAlloca ? L"alloca " : L"");
    StdOutPrintf(L"%s", pfps->flags.fHasSetJmp ? L"setjmp " : L"");
    StdOutPrintf(L"%s", pfps->flags.fHasLongJmp ? L"longjmp " : L"");
    StdOutPrintf(L"%s", pfps->flags.fHasInlAsm ? L"inlasm " : L"");
    StdOutPrintf(L"%s", pfps->flags.fHasEH ? L"eh " : L"");
    StdOutPrintf(L"%s", pfps->flags.fInlSpec ? L"inl_specified " : L"");
    StdOutPrintf(L"%s", pfps->flags.fHasSEH ? L"seh " : L"");
    StdOutPrintf(L"%s", pfps->flags.fNaked ? L"naked " : L"");
    StdOutPrintf(L"%s", pfps->flags.fSecurityChecks ? L"gschecks " : L"");
    StdOutPrintf(L"%s", pfps->flags.fAsyncEH ? L"asynceh " : L"");
    StdOutPrintf(L"%s", pfps->flags.fGSNoStackOrdering ? L"gsnostackordering " : L"");
    StdOutPrintf(L"%s", pfps->flags.fWasInlined ? L"wasinlined " : L"");
    StdOutPrintf(L"%s", pfps->flags.fGSCheck ? L"strict_gs_check " : L"");
    StdOutPrintf(L"%s", pfps->flags.fSafeBuffers ? L"safebuffers " : L"");
    StdOutPrintf(L"%s", pfps->flags.fPogoOn ? L"pgo_on " : L"");
    StdOutPrintf(L"%s", pfps->flags.fValidCounts ? L"valid_pgo_counts " : L"invalid_pgo_counts "); 
    StdOutPrintf(L"%s", pfps->flags.fOptSpeed ? L"opt_for_speed " : L""); 
    StdOutPrintf(L"Local=%s ", 
        SzNameFrameReg(pfps->flags.encodedLocalBasePointer));
    StdOutPrintf(L"Param=%s ", 
        SzNameFrameReg(pfps->flags.encodedParamBasePointer));
    StdOutPrintf(L"%s", pfps->flags.fGuardCF ? L"guardcf " : L"");
    StdOutPrintf(L"%s", pfps->flags.fGuardCFW ? L"guardcfw " : L"");
    StdOutPrintf(L"(0x%08X)\n", ulFlags);
}

void C7SeparatedCode(const SEPCODESYM *psym)
{
    PrtIndent();

    StdOutPrintf(
        L"S_SEPCODE: [%04X:%08X], Cb: %08X, ",
        psym->sect,
        psym->off,
        psym->length
        );

    PrtIndent(false);

    StdOutPrintf(
        L"Parent: %08X, End: %08X\n",
        psym->pParent,
        psym->pEnd
        );

    PrtIndent(false);

    StdOutPrintf(
        L"Parent scope begins: [%04X:%08X]\n",
        psym->sectParent,
        psym->offParent
        );

    PrtIndent(false);

    union {
        CV_SEPCODEFLAGS scf;
        unsigned long   ulf;
    } grfSepCode;

    grfSepCode.scf = psym->scf;

    StdOutPrintf(L"Separated code flags: ");

    StdOutPrintf(L"%s", grfSepCode.scf.fIsLexicalScope ? L"lexscope " : L"");
    StdOutPrintf(L"%s", grfSepCode.scf.fReturnsToParent ? L"retparent " : L"");
    StdOutPrintf(L"(0x%08X)\n", grfSepCode.ulf);
    cchIndent++;

    PrtSkip();
}


void C7Local(const LOCALSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_LOCAL: ");

    PrintLvarFlags(psym->flags, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7FileStatic(const FILESTATICSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_FILESTATIC: ");

    PrintLvarFlags(psym->flags, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);

    PrtIndent();
    StdOutPrintf(L" Mod: ");

    PDB *ppdb = NULL;
    DBI *pdbi = NULL;
    NameMap *pnm = NULL;
    const char *modName = NULL;

    if (pmodSym == NULL) {
        StdOutPrintf(L"0x%x\n", psym->modOffset);
        return;
    }

    pmodSym->QueryDBI(&pdbi);
    pdbi->QueryPdb(&ppdb);

    NameMap::open(ppdb, false, &pnm);

    if (pnm) {
        pnm->getName(psym->modOffset, &modName);
        PrintSt(fUtf8Symbols, reinterpret_cast<const unsigned char *>(modName));
        pnm->close();
    }
}

void PrintRangeKind( const DEFRANGESYM * psym)
{
    switch (psym->rectyp) 
    {
    case S_DEFRANGE:
        StdOutPrintf(L"S_DEFRANGE: DIA Program NI: %04X, ", psym->program);
        break;

    case S_DEFRANGE_SUBFIELD:
        StdOutPrintf(L"S_DEFRANGE_SUBFIELD: offset at %04X: DIA Program NI: %04X, ",
            ((const DEFRANGESYMSUBFIELD*)psym)->offParent, psym->program);
        break;

    case S_DEFRANGE_REGISTER:
        StdOutPrintf(L"S_DEFRANGE_REGISTER:%s %s",
            ((const DEFRANGESYMSUBFIELDREGISTER*)psym)->attr.maybe ? L"MayAvailable": L"", 
            SzNameC7Reg(((const DEFRANGESYMREGISTER*)psym)->reg));

        break;

    case S_DEFRANGE_SUBFIELD_REGISTER:
        StdOutPrintf(L"S_DEFRANGE_SUBFIELD_REGISTER:  offset at %04X:%s  %s ", 
            ((const DEFRANGESYMSUBFIELDREGISTER*)psym)->offParent,
            ((const DEFRANGESYMSUBFIELDREGISTER*)psym)->attr.maybe ? L"MayAvailable": L"", 
            SzNameC7Reg(((const DEFRANGESYMSUBFIELDREGISTER*)psym)->reg));
        break;

    case S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE:
    case S_DEFRANGE_FRAMEPOINTER_REL:
        StdOutPrintf(L"S_DEFRANGE_FRAMEPOINTER_REL: FrameOffset: %04X ", 
            ((const DEFRANGESYMFRAMEPOINTERREL*)psym)->offFramePointer);
        break;
    case S_DEFRANGE_REGISTER_REL:
        StdOutPrintf(L"S_DEFRANGE_REGISTER_REL: [%s +  %04X ]", 
            SzNameC7Reg(((const DEFRANGESYMREGISTERREL*)psym)->baseReg),
            ((const DEFRANGESYMREGISTERREL*)psym)->offBasePointer);
        if (((const DEFRANGESYMREGISTERREL*)psym)->spilledUdtMember) {
            StdOutPrintf(L" spilledUdtMember offset at %d", 
                ((const DEFRANGESYMREGISTERREL*)psym)->offsetParent);
        }
        break;

    case S_DEFRANGE_HLSL:
#if defined(CC_DP_CXX)
    case S_DEFRANGE_DPC_PTR_TAG:
        {
            if (psym->rectyp == S_DEFRANGE_HLSL)
            {
                StdOutPrintf(L"S_DEFRANGE_HLSL: ");
            }
            else
            {
                StdOutPrintf(L"S_DEFRANGE_DPC_PTR_TAG: ");
            }

            DEFRANGESYMHLSL *p = (DEFRANGESYMHLSL *) psym;

            StdOutPrintf(L"%s, RegIndices = %u, ",
                         SzNameHLSLRegType(p->regType), p->regIndices);
#else
        {
            DEFRANGESYMHLSL *p = (DEFRANGESYMHLSL *) psym;

            StdOutPrintf(L"S_DEFRANGE_HLSL: %s, RegIndices = %u, ",
                         SzNameHLSLRegType(p->regType), p->regIndices);
#endif // CC_DP_CXX

            if (p->memorySpace == CV_HLSL_MEMSPACE_DATA) {
                StdOutPuts(L"DATA");
            } else if (p->memorySpace == CV_HLSL_MEMSPACE_SAMPLER) {
                StdOutPuts(L"SAMPLER");
            } else if (p->memorySpace == CV_HLSL_MEMSPACE_RESOURCE) {
                StdOutPuts(L"RESOURCE");
            } else if (p->memorySpace == CV_HLSL_MEMSPACE_RWRESOURCE) {
                StdOutPuts(L"RWRESOURCE");
            } else {
                StdOutPuts(L"****Warning**** invalid memory space value: %02X");
            }

            if (p->spilledUdtMember) {
                StdOutPuts(L", spilled member");
            }

            StdOutPrintf(L", offset = %u, size = %u", p->offsetParent, p->sizeInParent);
            break;
        }

    default:
        StdOutPrintf(L"Illegal Definition Range: %04X ", psym->rectyp);
    }
}


void C7DefRange(const DEFRANGESYM *psym)
{
    PrtIndent();

    PrintRangeKind(psym);

    if (psym->rectyp == S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE) {
        StdOutPrintf(L"\t  FULL_SCOPE \n");
        return;
    }

    StdOutPrintf(L"\n\tRange: [%04X:%08X] - [%04X:%08X]",
        psym->range.isectStart,
        psym->range.offStart,
        psym->range.isectStart,
        psym->range.offStart + psym->range.cbRange);

    StdOutPrintf(L", %u Gaps", CV_DEFRANGESYM_GAPS_COUNT(psym));

    if (CV_DEFRANGESYM_GAPS_COUNT(psym) != 0) {
        StdOutPuts(L" (startOffset, length):");
    }

    for (DWORD i = 0; i < CV_DEFRANGESYM_GAPS_COUNT(psym); i++) {
        StdOutPrintf(L" (%04X, %X)",
                     psym->gaps[i].gapStartOffset,
                     psym->gaps[i].cbRange);
    }

    StdOutPuts(L"\n");
}

void C7DefRange2(const DEFRANGESYMSUBFIELD *psym)
{
    PrtIndent();

    PrintRangeKind((const DEFRANGESYM *)psym);

    StdOutPrintf(L"\n\tRange: [%04X:%08X] - [%04X:%08X]",
        psym->range.isectStart,
        psym->range.offStart,
        psym->range.isectStart,
        psym->range.offStart + psym->range.cbRange);

    StdOutPrintf(L", %u Gaps", CV_DEFRANGESYMSUBFIELD_GAPS_COUNT(psym));

    if (CV_DEFRANGESYMSUBFIELD_GAPS_COUNT(psym) != 0) {
        StdOutPuts(L" (startOffset, length):");
    }

    for (DWORD i = 0; i < CV_DEFRANGESYMSUBFIELD_GAPS_COUNT(psym); i++) {
        StdOutPrintf(L" (%04X, %X)",
                     psym->gaps[i].gapStartOffset,
                     psym->gaps[i].cbRange);
    }

    StdOutPuts(L"\n");
}

void C7DefRangeHLSL(const DEFRANGESYMHLSL * psym)
{
    PrtIndent();

    PrintRangeKind((const DEFRANGESYM *)psym);

    StdOutPrintf(L"\n\tRange: [%04X:%08X] - [%04X:%08X]",
        psym->range.isectStart,
        psym->range.offStart,
        psym->range.isectStart,
        psym->range.offStart + psym->range.cbRange);

    StdOutPrintf(L", %u Gaps", CV_DEFRANGESYMHLSL_GAPS_COUNT(psym));

    if (CV_DEFRANGESYMHLSL_GAPS_COUNT(psym) != 0) {
        StdOutPuts(L" (startOffset, length):");
    }

    const CV_LVAR_ADDR_GAP *pgaps = (const CV_LVAR_ADDR_GAP *) psym->data;
    for (DWORD i = 0; i < CV_DEFRANGESYMHLSL_GAPS_COUNT(psym); i++) {
        StdOutPrintf(L" (%04X, %X)",
                     pgaps[i].gapStartOffset,
                     pgaps[i].cbRange);
    }

    StdOutPuts(L"\n\tDimensional offsets:");

    const CV_uoff32_t *poffsets = CV_DEFRANGESYMHLSL_OFFSET_CONST_PTR(psym);
    for (DWORD i = 0; i < psym->regIndices; i++) {
        StdOutPrintf(L" %d", poffsets[i]);
    }

    StdOutPuts(L"\n");
}

#if defined(CC_DP_CXX)
void C7LocalDPCGroupShared(const LOCALDPCGROUPSHAREDSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_LOCAL_DPC_GROUPSHARED: ");

    PrintLvarFlags(psym->flags, psym->typind);

    StdOutPrintf(L"base data: slot = %u offset = %u, ", psym->dataslot, psym->dataoff);

    PrintSt(fUtf8Symbols, psym->name);
}

void C7DPCSymTagMap(const DPCSYMTAGMAP * psym)
{
    PrtIndent();

    unsigned int mapSize = CV_DPCSYMTAGMAP_COUNT(psym);
    const CV_DPC_SYM_TAG_MAP_ENTRY *mapEntries = (const CV_DPC_SYM_TAG_MAP_ENTRY *) psym->mapEntries;

    StdOutPrintf(L"S_DPC_SYM_TAG_MAP: %u entries, ", mapSize);
    for (unsigned int i = 0; i < mapSize; ++i)
    {
        StdOutPrintf(L"(%u, %X)", mapEntries[i].tagValue, mapEntries[i].symRecordOffset);
        if (i != (mapSize - 1))
        {
            StdOutPuts(L", ");
        }
    }

    StdOutPuts(L"\n");
}
#endif // CC_DP_CXX

void C7ArmSwitchTable(const ARMSWITCHTABLE * psym)
{
    PrtIndent();
    StdOutPrintf(L"S_ARMSWITCHTABLE:\n");
    PrtIndent(false);
    StdOutPrintf(L"Base address:   [%04X:%08X]\n", psym->sectBase, psym->offsetBase);
    PrtIndent(false);
    StdOutPrintf(L"Branch address: [%04X:%08X]\n", psym->sectBranch, psym->offsetBranch);
    PrtIndent(false);
    StdOutPrintf(L"Table address:  [%04X:%08X]\n", psym->sectTable, psym->offsetTable);
    PrtIndent(false);
    StdOutPrintf(L"Entry count = %d\n", psym->cEntries);
    PrtIndent(false);
    StdOutPrintf(L"Switch entry type = ");
    switch(psym->switchType) {
        case CV_SWT_INT1:
            StdOutPrintf(L"signed byte");
            break;
        case CV_SWT_UINT1:
            StdOutPrintf(L"unsigned byte");
            break;
        case CV_SWT_INT2:
            StdOutPrintf(L"signed two byte");
            break;
        case CV_SWT_UINT2:
            StdOutPrintf(L"unsigned two byte");
            break;
        case CV_SWT_INT4:
            StdOutPrintf(L"signed four byte");
            break;
        case CV_SWT_UINT4:
            StdOutPrintf(L"unsigned four byte");
            break;
        case CV_SWT_POINTER:
            StdOutPrintf(L"pointer");
            break;
        case CV_SWT_UINT1SHL1:
            StdOutPrintf(L"unsigned byte scaled by two");
            break;
        case CV_SWT_UINT2SHL1:
            StdOutPrintf(L"unsigned two byte scaled by two");
            break;
        case CV_SWT_INT1SHL1:
            StdOutPrintf(L"signed byte scaled by two");
            break;
        case CV_SWT_INT2SHL1:
            StdOutPrintf(L"signed two byte scaled by two");
            break;
        default:
            StdOutPrintf(L"unknown");
            break;
    }
    StdOutPrintf(L"\n");
}

static void PrintFunctionList(const FUNCTIONLIST *psym)
{
    StdOutPrintf(L"Count: %i\n", psym->count);
    ++cchIndent;

    const CV_typ_t* pType = &(psym->funcs[0]);
    DWORD* pCounts = (DWORD*)&(psym->funcs[psym->count]);
    PrtIndent(false);
    for(unsigned int i = 0; i < psym->count; ++i)
    {      
        int count = 0;
        
        // If pCount isn't off the end of the structure, then read the count. Otherwise it's implicitly zero
        if ((BYTE*)pCounts < ((BYTE*)(&psym->rectyp) + psym->reclen)) {
            count = *pCounts;
        }

        StdOutPrintf(L"%s (%u) ", SzNameC7Type2(*pType), count);

        if (i != psym->count-1)
            StdOutPrintf(L", ");

        if ((i+1) % 4 == 0) {
            StdOutPrintf(L"\n");
            PrtIndent(false);            
        }

        ++pType;
        ++pCounts;
    }

    --cchIndent;    
}

void C7Callees(const FUNCTIONLIST *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_CALLEES: ");
    PrintFunctionList(psym);
    StdOutPrintf(L"\n");
}

void C7Callers(const FUNCTIONLIST *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_CALLERS: ");
    PrintFunctionList(psym);
    StdOutPrintf(L"\n");
}

void C7PogoData(const POGOINFO *psym)
{
    PrtIndent();
    StdOutPrintf(L"S_POGODATA:\n");
    ++cchIndent;    
    PrtIndent(false);
    StdOutPrintf(L"Call Count: %i\n", psym->invocations);
    PrtIndent(false);
    StdOutPrintf(L"Dynaminc Instruction Count: %I64i\n", psym->dynCount);
    PrtIndent(false);
    StdOutPrintf(L"Number of Instructions: %i\n", psym->numInstrs);
    PrtIndent(false);
    StdOutPrintf(L"Number of Live Instructions: %i\n", psym->staInstLive);    
    --cchIndent;
    StdOutPrintf(L"\n");
}



void C7BuildInfo(const BUILDINFOSYM * psym)
{
    PrtIndent();

    StdOutPrintf(L"S_BUILDINFO: ");

    StdOutPrintf(L"%s\n", SzNameC7Type2(psym->id));
}

const wchar_t * const rgszInstructionName[] = {
    L"Illegal",                     // illegal
    L"Offset",                      // start offset 
    L"CodeOffsetBase",              // segment number
    L"CodeOffset",                  // parameter :delta of offset
    L"CodeLength",                  // length of code 
    L"File",                        // fileId 
    L"LineOffset",                  // line offset (signed)
    L"LineEndDelta",                // end line offset default 1
    L"RangeKind",                   // IsStatement/IsExpression 
    L"ColumnStart",                 // start column number 
    L"ColumnEndDelta",              // end column number delta (signed)
    L"CodeOffsetAndLineOffset",     // parameter : ((sourceDelta << 4) | CodeDelta)
    L"CodeLengthAndCodeOffset",     // parameter : codeLength, codeOffset
    L"ColumnEnd",                   // end column number
};

void C17BinaryAnnotations (PCompressedAnnotation input, size_t programLen)
{
    PrtIndent(false);

    StdOutPrintf(L"%-20s", L"BinaryAnnotations:");

    PCompressedAnnotation current = input;

    int cnt = 0;

    while (current < (input + programLen)) {
        int instruction = CVUncompressData(current);

        if (instruction == BA_OP_Invalid) {
            // Linker generated pdb may contain padding's.

            current--;
            break;
        }

        if (cnt == 4) {
            cnt = 0;
            StdOutPuts(L"\n");
            PrtIndent(false);
            for (int i = 0; i < 20; i++) {
                StdOutPutc(L' ');
            }
        }

        StdOutPrintf(L"  %s", rgszInstructionName[instruction]);

        assert(current < input + programLen);

        int count = BinaryAnnotationInstructionOperandCount(
            (BinaryAnnotationOpcode) instruction);

        for (int i = 0; i < count; i++) {
            int parameter = CVUncompressData(current);

            if ((instruction == BA_OP_ChangeLineOffset) ||
                (instruction == BA_OP_ChangeColumnEndDelta)) {
                parameter = DecodeSignedInt32(parameter);
            }

            StdOutPrintf(L" %x", parameter);
        }

        cnt++;
    }

    StdOutPrintf(L"\n");

    assert(current <= (input + programLen));

    PrtIndent(false);

    StdOutPrintf(L"BinaryAnnotation Length: %d bytes (%d bytes padding)\n",
                 programLen, input + programLen - current);
}

void C7InlineSite(const INLINESITESYM * psym)
{
    PrtSkip();

    PrtIndent();

    StdOutPrintf(L"S_INLINESITE: ");

    StdOutPrintf(L"Parent: %08X, End: %08X, Inlinee: %s\n",
                 psym->pParent,
                 psym->pEnd,
                 SzNameC7Type2(psym->inlinee));

    C17BinaryAnnotations((PCompressedAnnotation)psym->binaryAnnotations, 
        psym->reclen - sizeof(*psym) + sizeof(psym->reclen));

    cchIndent++;

    PrtSkip();    
}

void C7InlineSite2(const INLINESITESYM2 * psym)
{
    PrtSkip();

    PrtIndent();

    StdOutPrintf(L"S_INLINESITE2: ");

    StdOutPrintf(L"Parent: %08X, End: %08X, PGO Edge Count: %u, Inlinee: %s\n",
                 psym->pParent,
                 psym->pEnd,
                 psym->invocations,
                 SzNameC7Type2(psym->inlinee));

    C17BinaryAnnotations((PCompressedAnnotation)psym->binaryAnnotations, 
        psym->reclen - sizeof(*psym) + sizeof(psym->reclen));

    cchIndent++;

    PrtSkip();    
}


void C7InlineSiteEnd(const void * )
{
    C7EndSym2(L"S_INLINESITE_END");
}

void C7ProcIdEnd(const void * )
{
    C7EndSym2(L"S_PROC_ID_END");
}

void C7Compile2(const COMPILESYM *pcs)
{
    PrtIndent();
    StdOutPuts(L"S_COMPILE2:\n");

    PrtIndent(false);
    StdOutPrintf(L"Language: %s\n",
                 (pcs->flags.iLanguage < MAX_LANGUAGE_STRINGS) ? LanguageIdStrings[pcs->flags.iLanguage] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Target processor: %s\n",
                 (pcs->machine < MAX_PROCESSOR_STRINGS) ? ProcessorStrings[pcs->machine] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Compiled for edit and continue: %s\n", pcs->flags.fEC ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled without debugging info: %s\n", pcs->flags.fNoDbgInfo ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with LTCG: %s\n", pcs->flags.fLTCG ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /bzalign: %s\n", pcs->flags.fNoDataAlign ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Managed code present: %s\n", pcs->flags.fManagedPresent ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /GS: %s\n", pcs->flags.fSecurityChecks ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /hotpatch: %s\n", pcs->flags.fHotPatch ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Converted by CVTCIL: %s\n", pcs->flags.fCVTCIL ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"MSIL module: %s\n", pcs->flags.fMSILModule ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Pad bits = 0x%04x\n", (unsigned long) pcs->flags.pad);

    PrtIndent(false);
    StdOutPrintf(L"Frontend Version: Major = %u, Minor = %u, Build = %u\n",
                 pcs->verFEMajor,
                 pcs->verFEMinor,
                 pcs->verFEBuild);

    PrtIndent(false);
    StdOutPrintf(L"Backend Version: Major = %u, Minor = %u, Build = %u\n",
                 pcs->verMajor,
                 pcs->verMinor,
                 pcs->verBuild);

    PrtIndent(false);
    ShowSt(fUtf8Symbols, L"Version string: ", pcs->verSt);

    PrtIndent(false);
    StdOutPuts(L"Command block:\n");

    const char *pch = (char *) pcs->verSt + 1 + (fUtf8Symbols ?
                                    strlen((char *) pcs->verSt) : pcs->verSt[0]);
    const char *pchMax = (char *) pcs + 2 + pcs->reclen;

    while (pch < pchMax) {
        if (*pch == 0) {
            break;
        }

        PrtIndent(false);
        StdOutPrintf(L" %S = '%S'\n", pch, pch + strlen(pch) + 1);

        pch += strlen(pch) + 1;
        pch += strlen(pch) + 1;
    }

    PrtSkip();

    // MBH - this is a side-effect.
    // Later print-outs depend on the machine for which this was
    // compiled.  We have that info now, not later, so remember
    // it globally.

    CVDumpMachineType = pcs->machine;
}


void C7Compile3(const COMPILESYM3 *pcs)
{
    PrtIndent();
    StdOutPuts(L"S_COMPILE3:\n");

    PrtIndent(false);
    StdOutPrintf(L"Language: %s\n",
                 (pcs->flags.iLanguage < MAX_LANGUAGE_STRINGS) ? LanguageIdStrings[pcs->flags.iLanguage] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Target processor: %s\n",
                 (pcs->machine < MAX_PROCESSOR_STRINGS) ? ProcessorStrings[pcs->machine] : L"???");

    PrtIndent(false);
    StdOutPrintf(L"Compiled for edit and continue: %s\n", pcs->flags.fEC ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled without debugging info: %s\n", pcs->flags.fNoDbgInfo ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with LTCG: %s\n", pcs->flags.fLTCG ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /bzalign: %s\n", pcs->flags.fNoDataAlign ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Managed code present: %s\n", pcs->flags.fManagedPresent ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /GS: %s\n", pcs->flags.fSecurityChecks ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /hotpatch: %s\n", pcs->flags.fHotPatch ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Converted by CVTCIL: %s\n", pcs->flags.fCVTCIL ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"MSIL module: %s\n", pcs->flags.fMSILModule ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with /sdl: %s\n", pcs->flags.fSdl ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Compiled with pgo: %s\n", pcs->flags.fPGO ? L"yes" : L"no");    

    PrtIndent(false);
    StdOutPrintf(L".EXP module: %s\n", pcs->flags.fExp ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPrintf(L"Pad bits = 0x%04x\n", (unsigned long) pcs->flags.pad);

    PrtIndent(false);
    StdOutPrintf(L"Frontend Version: Major = %u, Minor = %u, Build = %u, QFE = %u\n",
                 pcs->verFEMajor,
                 pcs->verFEMinor,
                 pcs->verFEBuild,
                 pcs->verFEQFE);

    PrtIndent(false);
    StdOutPrintf(L"Backend Version: Major = %u, Minor = %u, Build = %u, QFE = %u\n",
                 pcs->verMajor,
                 pcs->verMinor,
                 pcs->verBuild,
                 pcs->verQFE);

    PrtIndent(false);
    ShowSt(true, L"Version string: ", (unsigned char *) pcs->verSz);

    PrtSkip();

    // MBH - this is a side-effect.
    // Later print-outs depend on the machine for which this was
    // compiled.  We have that info now, not later, so remember
    // it globally.

    CVDumpMachineType = pcs->machine;
}


void C7Envblock(const ENVBLOCKSYM *psym)
{
    PrtIndent();
    StdOutPuts(L"S_ENVBLOCK:\n");

    PrtIndent(false);
    StdOutPrintf(L"Compiled for edit and continue: %s\n", psym->flags.fEC ? L"yes" : L"no");

    PrtIndent(false);
    StdOutPuts(L"Command block:\n");

    const char *pch = (char *) psym->rgsz;
    const char *pchMax = (char *) psym + 2 + psym->reclen;

    while (pch < pchMax) {
        if (*pch == '\0') {
            break;
        }

        PrtIndent(false);

        StdOutPuts(L" ");
        PrintSt(true, (unsigned char *) pch, false);

        pch += strlen(pch) + 1;

        StdOutPuts(L" = '");
        PrintSt(true, (unsigned char *) pch, false);

        pch += strlen(pch) + 1;

        StdOutPuts(L"'\n");
    }

    PrtSkip();
}

void C7Slot(const SLOTSYM32 *psym)
{
    PrtIndent();

    StdOutPrintf(L"%s: [%08X], Type: %18s, ",
                 psym->rectyp == S_LOCALSLOT ? L"S_LOCALSLOT" : L"S_PARAMSLOT",
                 psym->iSlot,
                 SzNameC7Type2(psym->typind));

    PrintSt(fUtf8Symbols, psym->name);
}


void C7ManSlotSym(const ATTRSLOTSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_MANSLOT: %d, ", psym->iSlot);
    PrintLvAttr(&psym->attr, psym->typind);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7Annotation(const ANNOTATIONSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_ANNOTATION: [%04X:%08X]\n",
                 psym->seg, psym->off);

    const char *pch = (char *) psym->rgsz;

    for (unsigned short isz = 0; isz < psym->csz; isz++) {
        PrtIndent(false);

        StdOutPrintf(L"%5u: \"%S\"\n", isz + 1, pch);
        pch += strlen(pch) + 1;
    }

    PrtSkip();
}


void C7TrampolineSym(const TRAMPOLINESYM *psym)
{
    const wchar_t *szSubType = L"<unknown subtype>";

    PrtIndent();

    switch (psym->trampType) {
        case trampIncremental:
            szSubType = L"Incremental";
            break;

        case trampBranchIsland:
            szSubType = L"BranchIsland";
            break;
    }

    StdOutPrintf(L"S_TRAMPOLINE: subtype %s, code size = %u bytes\n",
                 szSubType,
                 psym->cbThunk);

    PrtIndent(false);

    StdOutPrintf(L"Thunk address: [%04X:%08X]\n",
                 psym->sectThunk,
                 psym->offThunk);

    PrtIndent(false);

    StdOutPrintf(L"Thunk target:  [%04X:%08X]\n",
                 psym->sectTarget,
                 psym->offTarget);

    PrtSkip();
}


void C7Section(const SECTIONSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_SECTION: [%04X], RVA = %08X, Cb = %08X, Align = %08X, Characteristics = %08X, ",
                 psym->isec,
                 psym->rva,
                 psym->cb,
                 1 << (DWORD) psym->align,
                 psym->characteristics);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7CoffGroup(const COFFGROUPSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_COFFGROUP: [%04X:%08X], Cb: %08X, Characteristics = %08X, ",
                 psym->seg,
                 psym->off,
                 psym->cb,
                 psym->characteristics);

    PrintSt(fUtf8Symbols, psym->name);
}


void C7Export(const EXPORTSYM *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_EXPORT: Ordinal = %u%s, ",
                 psym->ordinal,
                 psym->fOrdinal ? L"" : L" (implicit)");

    if (psym->fConstant) {
        StdOutPuts(L"CONSTANT, ");
    }

    if (psym->fData) {
        StdOutPuts(L"DATA, ");
    }

    if (psym->fPrivate) {
        StdOutPuts(L"PRIVATE, ");
    }

    if (psym->fNoName) {
        StdOutPuts(L"NONAME, ");
    }

    if (psym->fForwarder) {
        StdOutPuts(L"Forwarder, ");
    }

    PrintSt(fUtf8Symbols, psym->name);
}


void C7CallSiteInfo(const CALLSITEINFO *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_CALLSITEINFO: [%04X:%08X], ", psym->sect, psym->off);
    StdOutPrintf(L"type = %18s\n", SzNameC7Type2(psym->typind));
    if (psym->__reserved_0 != 0) {
        PrtIndent();
        StdOutPrintf(
            L"Warning: Reserved bytes in record are non-zero: 0X%04X\n",
            unsigned(psym->__reserved_0));
    }
}

void C7HeapAllocSite(const HEAPALLOCSITE *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_HEAPALLOCSITE: [%04X:%08X], ", psym->sect, psym->off);
    StdOutPrintf(L"instr length = %u, type = %18s\n", psym->cbInstr, SzNameC7Type2(psym->typind));
}

const wchar_t *SzNameC7CookieType(CV_cookietype_e cookieType)
{
    switch(cookieType) {
        case CV_COOKIETYPE_COPY :
            return L"COPY";

        case CV_COOKIETYPE_XOR_SP :
            return L"XOR_SP";

        case CV_COOKIETYPE_XOR_BP :
            return L"XOR_BP";

        case CV_COOKIETYPE_XOR_R13 :
            return L"XOR_R13";
    }

    return L"";
}


void C7FrameCookie(const FRAMECOOKIE *psym)
{
    PrtIndent();

    StdOutPrintf(L"S_FRAMECOOKIE: %s+%08X, Type: %s, Flags: %02X\n",
                 SzNameC7Reg(psym->reg),
                 psym->off,
                 SzNameC7CookieType(psym->cookietype),
                 psym->flags);
}


void C7Discarded(const DISCARDEDSYM *psym)
{
    PrtIndent();

    StdOutPuts(L"S_DISCARDED: ");

    switch (psym->discarded) {
        case CV_DISCARDED_UNKNOWN :
            StdOutPuts(L"Unknown,");
            break;

        case CV_DISCARDED_NOT_SELECTED :
            StdOutPuts(L"Not selected,");
            break;

        case CV_DISCARDED_NOT_REFERENCED :
            StdOutPuts(L"Not referenced");
            break;

        default :
            StdOutPrintf(L"(%02X),", psym->discarded);
            break;
    }

    if (psym->fileid != 0xFFFFFFFF) {
        StdOutPrintf(L" Fileid: %08X", psym->fileid);

        if (pmodSym != NULL) {
            // psym->fileid is a real fileid not a file index.
            // Mod::QueryFileNameInfo takes a file index.  It is too
            // painful at this point in Whidbey to change this
            // because we lack a Mod::getEnumFiles to enumerate files.
            // We do it the hard way

            if (pmodSym != pmodCache) {
                pmodCache = NULL;
                delete [] rgfileidCache;

                DWORD cb;

                if (!pmodSym->QueryLines2(0, NULL, (long *) &cb)) {
                    cb = 0;
                }

                BYTE *rgb = NULL;

                if (cb != 0) {
                    rgb = new BYTE[cb];

                    long cbT;

                    if (!pmodSym->QueryLines2(cb, rgb, &cbT)) {
                        cb = 0;
                    }
                }

                if (cb != 0) {
                    DWORD ib = 0;

                    while (ib < cb) {
                        if ((ib & 3) != 0) {
                           ib += 4 - (ib & 3);

                           if (ib == cb) {
                               break;
                           }
                        }

                        DWORD sst = *(DWORD *) (rgb + ib);
                        ib += sizeof(DWORD);

                        DWORD cbSection = *(DWORD *) (rgb + ib);
                        ib += sizeof(DWORD);

                        if (sst == DEBUG_S_FILECHKSMS) {
                            DWORD ibMax = ib + cbSection;

                            const BYTE *pbMin = rgb + ib;
                            const BYTE *pbMax = rgb + ibMax;

                            cfileidCache = 0;

                            for (const BYTE *pb = pbMin; pb < pbMax;) {
                                cfileidCache++;

                                size_t cbEntry = sizeof(DWORD) + 2 * sizeof(BYTE) + pb[4];

                                if ((cbEntry & 3) != 0) {
                                    cbEntry += 4 - (cbEntry & 3);
                                }

                                pb += cbEntry;
                            }

                            rgfileidCache = new DWORD[cfileidCache];

                            size_t ifileid = 0;

                            for (const BYTE *pb = pbMin; pb < pbMax;) {
                                rgfileidCache[ifileid++] = DWORD(pb - pbMin);

                                size_t cbEntry = sizeof(DWORD) + 2 * sizeof(BYTE) + pb[4];

                                if ((cbEntry & 3) != 0) {
                                    cbEntry += 4 - (cbEntry & 3);
                                }

                                pb += cbEntry;
                            }

                            pmodCache = pmodSym;
                            break;
                        }

                        ib += cbSection;
                    }
                }

                delete [] rgb;
            }

            if (pmodSym == pmodCache) {
                DWORD ifileid;

                for (ifileid = 0; ifileid < cfileidCache; ifileid++) {
                    if (rgfileidCache[ifileid] == psym->fileid) {
                        break;
                    }
                }

                if (ifileid < cfileidCache) {
                    wchar_t wszName[_MAX_PATH];
                    DWORD cchName = _MAX_PATH;
                    DWORD checksumtype;
                    BYTE rgbChecksum[256];
                    DWORD cbChecksum = sizeof(rgbChecksum);

                    pmodSym->QueryFileNameInfo(ifileid,
                                               wszName,
                                               &cchName,
                                               &checksumtype,
                                               rgbChecksum,
                                               &cbChecksum);

                    StdOutPrintf(L" %s (", wszName);

                    switch (checksumtype) {
                        case CHKSUM_TYPE_NONE :
                            StdOutPuts(L"None");
                            break;

                        case CHKSUM_TYPE_MD5 :
                            StdOutPuts(L"MD5");
                            break;

                        case CHKSUM_TYPE_SHA1 :
                            StdOutPuts(L"SHA1");
                            break;

                       case CHKSUM_TYPE_SHA_256 :
                           StdOutPrintf(L"SHA_256");
                           break;

                        default :
                            StdOutPrintf(L"0x%X", checksumtype);
                            break;
                    }

                    if (cbChecksum != 0) {
                        StdOutPuts(L": ");

                        for (DWORD i = 0; i < cbChecksum; i++) {
                            StdOutPrintf(L"%02X", rgbChecksum[i]);
                        }
                    }

                    StdOutPuts(L")");
                }
            }
        }

        StdOutPrintf(L", Line: %8u", psym->linenum);
    }

    StdOutPutc(L'\n');

    size_t cchIndentSave = cchIndent++;

    const BYTE *pbMin = (BYTE *) &psym->data;
    const BYTE *pbMax = (BYTE *) NextSym((SYMTYPE *) psym);

    for (const BYTE *pb = pbMin;
         pb < pbMax;
         pb = (BYTE *) NextSym((SYMTYPE *) pb)) {
        DumpOneSymC7(NULL, pb, (DWORD) (pb - pbMin));
    }

    cchIndent = cchIndentSave;
}


void C7ModTypeRef(const MODTYPEREF *psym)
{
    PrtIndent();

    StdOutPuts(L"S_MODTYPEREF: ");

    if (psym->fNone) {
        StdOutPuts(L"No TypeRef");
    } else if (psym->fOwnTMR) {
        StdOutPrintf(L"/Z7 TypeRef, SN=%04X", psym->word0);

        if (psym->fOwnTMPCT) {
            StdOutPuts(L", own PCH types");
        }
        
        if (psym->fRefTMPCT) {
            StdOutPrintf(L", reference PCH types in Module %04X", psym->word1 + 1);
        }
    } else {
        StdOutPrintf(L"/Zi TypeRef, ");

        if (psym->fOwnTM) {
            StdOutPrintf(L"SN=%04X (type), SN=%04X (ID)", psym->word0, psym->word1);
        }

        if(psym->fRefTM) {
            StdOutPrintf(L"shared with Module %04X", psym->word0 + 1);
        }
    }

    StdOutPutc(L'\n');

    PrtSkip();
}


size_t PrintNumeric(const void *pNum)
{
    // Displays the data and returns how many bytes it occupied

    WORD usIndex = *(WORD *) pNum;

    if (usIndex < LF_NUMERIC) {
        StdOutPrintf(L"%u", usIndex);
        return(2);
    }

    pNum = ((WORD *) pNum) + 1;

    switch (usIndex) {
        char c;
        WORD len;
        unsigned i;

        case LF_CHAR :
            c = *(char UNALIGNED *) pNum;
            StdOutPrintf(L"(LF_CHAR) %d(0x%02X)", (short) c, (BYTE) c);
            return(2 + sizeof(BYTE));

        case LF_SHORT :
            StdOutPrintf(L"(LF_SHORT) %d", *(short UNALIGNED *) pNum);
            return(2 + sizeof(short));

        case LF_USHORT :
            StdOutPrintf(L"(LF_USHORT) %u", *(WORD UNALIGNED *) pNum);
            return(2 + sizeof(WORD));

        case LF_LONG :
            StdOutPrintf(L"(LF_LONG) %ld", *(long UNALIGNED *) pNum);
            return(2 + sizeof(long));

        case LF_ULONG :
            StdOutPrintf(L"(LF_ULONG) %lu", *(DWORD UNALIGNED *) pNum);
            return(2 + sizeof(DWORD));

        case LF_REAL32 :
            StdOutPrintf(L"(LF_REAL32) %f", *(float UNALIGNED *) pNum);
            return(2 + 4);

        case LF_REAL64 :
            StdOutPrintf(L"(LF_REAL64) %f", *(double UNALIGNED *) pNum);
            return(2 + 8);

        case LF_REAL80 :
            StdOutPrintf(L"(LF_REAL80) %lf", *(long double UNALIGNED *) pNum);
            return(2 + 10);

        case LF_REAL128 :
//M00 - Note converts from 128 to 80 bits to display
            StdOutPrintf(L"(LF_REAL128) %lf", *(long double UNALIGNED *) pNum);
            return(2 + 16);

        case LF_QUADWORD :
            StdOutPrintf(L"(LF_QUADWORD) %I64d", *(__int64 UNALIGNED *) pNum);
            return(2 + sizeof(DWORDLONG));

        case LF_UQUADWORD :
            StdOutPrintf(L"(LF_UQUADWORD) %I64u", *(unsigned __int64 UNALIGNED *) pNum);
            return(2 + sizeof(DWORDLONG));

        case LF_REAL48 :
            StdOutPuts(L"(LF_LF_REAL48)");
            return(2 + 6);

        case LF_COMPLEX32 :
            StdOutPrintf(L"(LF_COMPLEX32) (%f, %f)", ((float UNALIGNED *) pNum)[0], ((float UNALIGNED *) pNum)[1]);
            return(2 + 8);

        case LF_COMPLEX64 :
            StdOutPrintf(L"(LF_COMPLEX64) (%f, %f)", ((double UNALIGNED *) pNum)[0], ((double UNALIGNED *) pNum)[1]);
            return(2 + 16);

        case LF_COMPLEX80 :
            StdOutPrintf(L"(LF_COMPLEX80) (%lf, %lf)", ((long double UNALIGNED *) pNum)[0], ((long double UNALIGNED *) pNum)[1]);
            return(2 + 20);

        case LF_COMPLEX128 :
            StdOutPuts(L"(LF_COMPLEX128)");
            return(2 + 32);

        case LF_VARSTRING :
            len = *(WORD UNALIGNED *) pNum;
            pNum = ((WORD *) pNum) + 1;

            StdOutPrintf(L"(LF_VARSTRING) %u ", len);
            for (i = 0; i < len; i++) {
                StdOutPrintf(L"0x%2X ", *(BYTE *) pNum);
                pNum = ((BYTE *) pNum) + 1;
            }
            return(len + 4);

        case LF_OCTWORD :
            StdOutPuts(L"(LF_OCTWORD)");
            return(2 + 16);

        case LF_UOCTWORD :
            StdOutPuts(L"(LF_UOCTWORD)");
            return(2 + 16);

        case LF_DECIMAL:
            {
                BSTR bstr = NULL;

                if (SUCCEEDED(VarBstrFromDec((DECIMAL *) pNum, 0, 0, &bstr))) {
                    StdOutPrintf(L"(LF_DECIMAL) %s", bstr);
                    SysFreeString(bstr);
                } else {
                    StdOutPuts(L"(LF_DECIMAL)");
                }

                return 2 + sizeof(DECIMAL);
            }

        case LF_DATE:
            {
                BSTR bstr = NULL;

                if (SUCCEEDED( VarBstrFromDate(*(DATE UNALIGNED *) pNum, 0, 0, &bstr))) {
                    StdOutPrintf(L"(LF_DATE) %s", bstr);
                    SysFreeString(bstr);
                } else {
                    StdOutPrintf(L"(LF_DATE) %f", *(DATE UNALIGNED *) pNum);
                }

                return 2 + sizeof(DATE);  // DATE == double
            }

        case LF_UTF8STRING:
            StdOutPuts(L"(LF_UTF8STRING) ");
            PrintSt(true, (const unsigned char *) pNum);
            return 2 + strlen((const char *) pNum) + 1;

        default :
            StdOutPuts(L"Invalid Numeric Leaf");
            return(2);
    }
}


size_t SkipNumeric(const void *pNum)
{
    // Returns how many bytes a numeric field occupied

    unsigned uIndex = *(WORD *) pNum;

    if (uIndex < LF_NUMERIC) {
        return 2;
    }

    pNum = ((WORD *) pNum) + 1;

    switch (uIndex) {
        unsigned len;

        case LF_CHAR :
            return 2 + sizeof(BYTE);

        case LF_SHORT :
            return 2 + sizeof(short);

        case LF_USHORT :
            return 2 + sizeof(WORD);

        case LF_LONG :
            return 2 + sizeof(long);

        case LF_ULONG :
            return 2 + sizeof(DWORD);

        case LF_REAL32 :
            return 2 + 4;

        case LF_REAL64 :
            return 2 + 8;

        case LF_REAL80 :
            return 2 + 10;

        case LF_REAL128 :
            return 2 + 16;

        case LF_QUADWORD :
            return 2 + sizeof(DWORDLONG);

        case LF_UQUADWORD :
            return 2 + sizeof(DWORDLONG);

        case LF_REAL48 :
            return 2 + 6;

        case LF_COMPLEX32 :
            return 2 + 8;

        case LF_COMPLEX64 :
            return 2 + 16;

        case LF_COMPLEX80 :
            return 2 + 20;

        case LF_COMPLEX128 :
            return 2 + 32;

        case LF_VARSTRING :
            len = *(WORD UNALIGNED *) pNum;
            return len + 4;

        case LF_OCTWORD :
            return 2 + 16;

        case LF_UOCTWORD :
            return 2 + 16;

        case LF_DECIMAL:
            return 2 + sizeof(DECIMAL);

        case LF_DATE:
            return 2 + sizeof(DATE);  // DATE == double

        case LF_UTF8STRING:
            return 2 + strlen((const char *) pNum) + 1;

        default :
            StdOutPuts(L"Invalid Numeric Leaf");
            return 2;
    }
}


void SymHash32(const OMFSymHash *phash, const OMFDirEntry *)
{
    unsigned i = 0;
    unsigned j = 0;
    unsigned cBuckets;
    DWORD off = 0;
    unsigned iBucket = 0;
    WORD *Counts;

    cbRec = phash->cbHSym;
    cBuckets = WGets();
    StdOutPrintf(L"Symbol hash - number of buckets = %d\n", cBuckets);
    WGets();

    while (j < cBuckets) {
        if (i == 0) {
            StdOutPrintf(L"\t%04X", j);
        }

        StdOutPrintf(L"\t%08X", LGets());

        if (++i == 4) {
            StdOutPutc(L'\n');
            i = 0;
        }

        j++;
    }

    if ((Counts = (WORD *) malloc(sizeof(WORD) * cBuckets)) == NULL) {
        Fatal(L"Out of memory");
    }

    GetBytes(Counts, sizeof(WORD) * cBuckets);
    StdOutPuts(L"\n\n Symbol hash - chains\n");
    off = cBuckets * sizeof(WORD) + cBuckets * sizeof(DWORD) + sizeof(DWORD);

    for (iBucket = 0; iBucket < cBuckets; iBucket++) {
        j = Counts[iBucket];
        StdOutPrintf(L"\n\n%08X: Bucket = %04X, Count = %04X\n", off, iBucket, j);
        i = 0;

        while (i < j) {
            StdOutPrintf(L"    %08X", LGets());

            if ((++i % 6 == 0) && (i < j)) {
                StdOutPutc(L'\n');
            }

            off += sizeof(DWORD);
        }
    }

    StdOutPutc(L'\n');
    StdOutPutc(L'\n');

    free(Counts);
}


void AddrHash32(const OMFSymHash *phash, const OMFDirEntry *, int iHash)
{
    int cseg = 0;
    int iseg = 0;
    DWORD *rgulSeg = NULL;
    WORD *rgsCSeg = NULL;
    DWORD *rglCSeg = NULL;
    unsigned short us;

    cbRec = phash->cbHAddr;

    cseg = WGets();
    StdOutPrintf(L"Address hash - number of segments = %d", cseg);
    WGets();

    if ((rgulSeg = (DWORD *) malloc(sizeof(DWORD)  * cseg)) == NULL) {
        Fatal(L"Out of memory");
    }

    GetBytes(rgulSeg, sizeof(DWORD) * cseg);

    if (iHash != 12) {
        rgsCSeg  = (WORD *) malloc(sizeof(WORD) * cseg);
        if (rgsCSeg == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rgsCSeg, sizeof(WORD) * cseg);
    } else {
        rglCSeg  = (DWORD *) malloc(sizeof(DWORD) * cseg);

        if (rglCSeg == NULL) {
            Fatal(L"Out of memory");
        }

        GetBytes(rglCSeg, sizeof(DWORD) * cseg);
    }

    if ((iHash == 5) && (cseg & 1)) {
        GetBytes(&us, sizeof(WORD));  // UNDONE : What's this value signify???
    }

    for (iseg = 0; iseg < cseg; iseg++) {
        int isym;

        int cSeg = (iHash == 12) ? rglCSeg[iseg] : rgsCSeg [ iseg ];

        StdOutPrintf(L"\n\nSegment #%d - %d symbols\n\n", iseg + 1, cSeg);

        for (isym = 0; isym < cSeg; isym++) {
            StdOutPrintf(L"    %08X", LGets());

            if (iHash == 12) {
                LGets();
            }

            if ((isym + 1) % 6 == 0) {
                StdOutPutc(L'\n');
            }
        }
    }

    free ( rgulSeg);
    if (rgsCSeg)
        free ( rgsCSeg);
    if (rglCSeg)
        free ( rglCSeg);

    StdOutPutc(L'\n');
    StdOutPutc(L'\n');
}


void SymHash32Long(const OMFSymHash *phash, const OMFDirEntry *)
{
    unsigned i;
    unsigned j;
    unsigned cBuckets;
    DWORD off = 0;
    unsigned iBucket = 0;
    DWORD *rgCounts;

    cbRec = phash->cbHSym;
    cBuckets = WGets();
    StdOutPrintf(L"Symbol hash - number of buckets = %d\n", cBuckets);
    WGets();

    for (j=0, i=0; j < cBuckets; j++) {
        if (i == 0) {
            StdOutPrintf(L"\t%04X", j);
        }
        StdOutPrintf(L"\t%08X", LGets());
        if (++i == 4) {
            StdOutPutc(L'\n');

            i = 0;
        }
    }

    if ((rgCounts = (DWORD *) malloc(sizeof(DWORD) * cBuckets)) == NULL) {
        Fatal(L"Out of memory");
    }
    GetBytes(rgCounts, sizeof(DWORD) * cBuckets);

    StdOutPuts(L"\n\n Symbol hash - chains\n");
    off = cBuckets * sizeof(DWORD) + cBuckets * sizeof(DWORD) +
          sizeof(DWORD);

    for (iBucket = 0; iBucket < cBuckets; iBucket++) {
        j = (WORD) rgCounts[iBucket];
        StdOutPrintf(L"\n\n%08X: Bucket = %04X, Count = %04X\n",
                 off, iBucket, j);
        i = 0;

        while (i < j) {
            StdOutPrintf(L"    %08X", LGets());
            LGets();

            if ((++i % 6 == 0) && (i < j)) {
                StdOutPutc(L'\n');
            }

            off += sizeof(DWORD);
        }
    }

    StdOutPutc(L'\n');
    StdOutPutc(L'\n');

    free(rgCounts);
}


void AddrHash32NB09(const OMFSymHash *phash, const OMFDirEntry *)
{
    int  cseg = 0;
    int  iseg = 0;
    DWORD *rgulSeg;
    WORD *rgcseg;
    DWORD off = 0;

    cbRec = phash->cbHAddr;

    cseg = WGets();
    StdOutPrintf(L"Address hash - number of segments = %d", cseg);
    WGets();

    rgulSeg = (DWORD *) malloc(sizeof(DWORD) * cseg);
    rgcseg  = (WORD *) malloc(sizeof(WORD) * cseg);

    if ((rgulSeg == NULL) || (rgcseg  == NULL)) {
        Fatal(L"Out of memory");
    }

    GetBytes(rgulSeg, sizeof(DWORD) * cseg);
    GetBytes(rgcseg, sizeof(WORD) * cseg);

    for (iseg = 0; iseg < cseg; iseg++) {
        int isym;

        StdOutPrintf(L"\n\n%08X: Segment #%d - %d symbols\n\n", off, iseg + 1, rgcseg [ iseg ]);

        for (isym = 0; isym < (int) rgcseg[iseg]; isym++) {
            DWORD uoffSym = LGets();
            DWORD uoffSeg = LGets();

            StdOutPrintf(L"  (%08X,%08X)", uoffSym, uoffSeg);

            if ((isym + 1) % 4 == 0) {
                StdOutPutc(L'\n');
            }

            off += 2 * sizeof(DWORD);
        }
    }

    free(rgulSeg);
    free(rgcseg);

    StdOutPutc(L'\n');
    StdOutPutc(L'\n');
}
