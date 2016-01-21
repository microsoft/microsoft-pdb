/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

    /* The first typedef index after the primitive
     * reserved indices
     */
#define PRIM_TYPES 511
#define CV_DERIVED_TYPES 512
#define CV_NULL_TYPE_LIST       CV_DERIVED_TYPES
#define CV_HIGHEST_TYPE  0x7FFF
#define MAXLOCALTINDEX  (CV_HIGHEST_TYPE / 2)

#define MS_VOID         0xffff

#if ! PASS3             /* For p1 */
    /* MS primitive types are built from the following bits:
     *  x xx xxx xx
     *  r md typ siz
     */
#define MS_SIZ_CHAR     0
#define MS_SIZ_SHORT    1
#define MS_SIZ_LONG     2
#define MS_SIZ_R32      0
#define MS_SIZ_R64      1
#define MS_SIZ_R80      2
#define MS_SIZ_C32      0
#define MS_SIZ_C64      1
#define MS_SIZ_C80      2
#define MS_SIZ          0x3

#define MS_TYP_SGN      (0x00 << 2)
#define MS_TYP_UNS      (0x01 << 2)
#define MS_TYP_REAL     (0x02 << 2)
#define MS_TYP_CMPLX    (0x03 << 2)
#define MS_TYP_BOOL     (0x04 << 2)
#define MS_TYP_ASCII    (0x05 << 2)
#define MS_TYP_VOID     0x1c
#define MS_TYP          (0x07 << 2)

#define MS_MOD_DIR      (0x00 << 5)
#define MS_MOD_NEAR     (0x01 << 5)
#define MS_MOD_FAR      (0x02 << 5)
#define MS_MOD_HUGE     (0x03 << 5)
#define MS_MOD          (0x03 << 5)

#define MS_SEGMENT      2   /* from SHSYMLIB.H T_SEGMENT */
#endif

    /* Leaves used to build types */
#define OLF_BITFIELD      92
#define OLF_NEWTYPE       93
#define OLF_HUGE          94

#define OLF_OFFSETSIZE32  90     /* OLF_POINTER */
#define OLF_NOPOP32       91     /* OLF_LONG_NOPOP */
#define OLF_NOPOP16       92     /* OLF_SHORT_NOPOP */
#define OLF_POP32         93     /* OLF_LONG_POP */
#define OLF_POP16         94     /* OLF_SHORT_POP */
#define OLF_FAR32         95     /* procs (?) */
#define OLF_NEAR32        96     /* labels (OLF_NEAR), procs (?) */

#define OLF_PLSTRUCTURE   97
#define OLF_PLARRAY       98
#define OLF_SHORT_NOPOP   99
#define OLF_LONG_NOPOP   100
#define OLF_SELECTOR     101
#define OLF_INTERRUPT    102
#define OLF_FILE         103
#define OLF_PACKED       104
#define OLF_UNPACKED     105
#define OLF_SET          106
#define OLF_CHAMELEON    107
#define OLF_BOOLEAN      108
#define OLF_TRUE         109
#define OLF_FALSE        110
#define OLF_CHAR         111
#define OLF_INTEGER      112
#define OLF_CONST        113
#define OLF_LABEL        114
#define OLF_FAR          115
#define OLF_LONG_POP     115
#define OLF_NEAR         116
#define OLF_SHORT_POP    116
#define OLF_PROCEDURE    117
#define OLF_PARAMETER    118
#define OLF_DIMENSION    119
#define OLF_ARRAY        120
#define OLF_STRUCTURE    121
#define OLF_POINTER      122
#define OLF_SCALAR       123
#define OLF_UNSINT       124
#define OLF_SGNINT       125
#define OLF_REAL         126
#define OLF_LIST         127

    /* Nice and easy null leaves defined */
#define OLF_EASY         128
#define OLF_NICE         129

    /* Prefixes to other leaves */
#define OLF_STRING       130
#define OLF_INDEX        131
#define OLF_REPEAT       132
    /* Prefixes for constants */
#define OLF_2_UNSIGNED   133
#define OLF_4_UNSIGNED   134
#define OLF_8_UNSIGNED   135
#define OLF_1_SIGNED     136
#define OLF_2_SIGNED     137
#define OLF_4_SIGNED     138
#define OLF_8_SIGNED     139
    /* Fortran string & array bounds */
#define OLF_BARRAY       140
#define OLF_FSTRING      141
#define OLF_FARRIDX      142

    /* incremental compilation support */
#define OLF_SKIP         144

    /* base pointer support */
#define OLF_BASEPTR      145
#define OLF_BASESEG      146
#define OLF_BASEVAL      147
#define OLF_BASESEGVAL   148
#define OLF_BASEADR      151
#define OLF_BASESEGADR   152

    /* fastcall return type leaves */
#define OLF_NFASTCALL    149
#define OLF_FFASTCALL    150

    /* c++ support */
#define OLF_C7PTR        143
#define OLF_MODIFIER     153
#define OLF_BASECLASS    154
#define OLF_VBASECLASS   155
#define OLF_FRIENDCLASS  156
#define OLF_MEMBER       157
#define OLF_STATICMEMBER 158
#define OLF_VTABPTR      159
#define OLF_METHOD       160
#define OLF_CLASS        161
#define OLF_C7STRUCTURE  162
#define OLF_UNION        163
#define OLF_MEMBERFUNC   164

#define OLF_DEFARG       165

/* reserved for COBOL */

#define OLF_COBOLTYPREF  166
#define OLF_COBOL        167

/* used for enumerate types */
#define OLF_ENUM         168
#define OLF_ENUMERATE    169

#define OLF_VTSHAPE      170
#define OLF_NESTDEF      171
#define OLF_DERIVLIST    172
#define OLF_FIELDLIST    173
#define OLF_ARGLIST      174
#define OLF_METHODLIST   175
#define OLF_VBCLASS      176
#define OLF_IVBCLASS     177


#define DL_SEG_BLOCK    0   /* block start id */
#define DL_SEG_PROC     1   /* procedure start id */
#define DL_SEG_END      2   /* block or procedure end id */
#define DL_SEG_EBP      4   /* EBP relative base id */
#define DL_SEG_BP       4   /* BP relative base id */
#define DL_SEG_BASE     5   /* symbol base id */
#define DL_SEG_LABEL    11  /* Label id */
#define DL_SEG_WITH     12  /* With start id */
#define DL_SEG_REG      13  /* Register symbol id */
#define DL_SEG_CONST    14  /* Constant symbol */
#define DL_SEG_FENTRY   15  /* Fortran entry id */
#define DL_SEG_NOOP     16  /* Noop used to pad for incremental compilation */
#define DL_SEG_CODSEG   17  /* Used to specify the effective code segment */
#define DL_SEG_TYPEDEF  18  /* Used to specify a typedef */
#define DL_SEG_GLOBAL   19  /* Used to specify global data */
#define DL_SEG_GLOBPROC 20  /* Used to specify global procedure */
#define DL_SEG_LOCALPROC    21  /* Used to specify global procedure */
#if CC_PCODE
#define DL_SEG_CHANGEMODEL 22   // Change Execution Model
#endif
#define DL_SEG_RESERVED 23
#define DL_SEG_THUNK    24  /* Thunk symbol */
#define DL_SEG_CV4LABEL 25  /* New label symbol */
#define DL_SEG_CV4BLOCK 26  /* New label symbol */
#define DL_SEG_CV4WITH  27  /* New label symbol */

/* class field descriptor field attribute support */

#define ACCESS_NONE     0
#define ACCESS_PRIVATE  1
#define ACCESS_PROTECT  2
#define ACCESS_PUBLIC   3

#define ACCESS_ISNONE(attr)     ((attr & FLMASKACCESS) == ACCESS_NONE)
#define ACCESS_ISPRIVATE(attr)  ((attr & FLMASKACCESS) == ACCESS_PRIVATE)
#define ACCESS_ISPROTECT(attr)  ((attr & FLMASKACCESS) == ACCESS_PROTECT)
#define ACCESS_ISPUBLIC(attr)   ((attr & FLMASKACCESS) == ACCESS_PUBLIC)

/* Bits for property field of method list:
**  (shifted here instead of at masking time)
*/
#define MPROP_VANILLA   0;          // Nothing special
#define MPROP_VIRTUAL   (1 << 2);   // Virtual redefinition
#define MPROP_STATIC    (2 << 2);   // Static member function
#define MPROP_FRIEND    (3 << 2);   // A friend, not a method (?)
#define MPROP_IVIRTUAL  (4 << 2);   // 'Introducing' virtual function

#define MPROP_PURE      0x20

/* Masks for field property */

#define FPROP_ACCESS_MASK   0x03    // The access part - used a lot
#define FPROP_MPROP_MASK    0x1C    // Method property (mlist only)
#define FPROP_VIRTUAL       0x20    // Used only for base classes
#define FPROP_FLAT32        0x40    // Used only for static members

/* Bits for property field of structures:
*/
#define CPROP_PACKED    0x01    // Is struct packed?
#define CPROP_CTOR      0x02    // Class has ctors/dtors
#define CPROP_OVEROPS   0x04    // Class has overloaded operators
#define CPROP_ISNESTED  0x08    // Is nested class
#define CPROP_HASNESTED 0x10    // Has nested classes

/* describes attribute field of c7 ptr type */
/*
**  typedef struct c7ptr_attr_s {
**      unsigned    ptrtype     :5;
**      unsigned    ptrmode     :3;
**      unsigned    isptr32     :1;
**      unsigned    volatile    :1;
**      unsigned    const       :1;
**               unused         :5;
**  } c7ptr_attr_t;
*/

#define C7PTR_MASK_TYP          0x001f
#define C7PTR_TYP_NEAR          0
#define C7PTR_TYP_FAR           1
#define C7PTR_TYP_HUGE          2
#define C7PTR_TYP_BASESEG       3
#define C7PTR_TYP_BASEVAL       4
#define C7PTR_TYP_BASESEGVAL    5
#define C7PTR_TYP_BASEADDR      6
#define C7PTR_TYP_BASESEGADR    7
#define C7PTR_TYP_BASEDONTYPE   8
#define C7PTR_TYP_BASEDONSELF   9

#define C7PTR_MASK_MODE         0x00E0
#define C7PTR_SHIFT_MODE        5
#define C7PTR_MODE_POINTER      (0 << C7PTR_SHIFT_MODE)
#define C7PTR_MODE_REFERENCE    (1 << C7PTR_SHIFT_MODE)
#define C7PTR_MODE_PMEMBER      (2 << C7PTR_SHIFT_MODE)
#define C7PTR_MODE_PMEMBERFUNC  (3 << C7PTR_SHIFT_MODE)

#define C7PTR_ATTR_ISPTR32      0x0100
#define C7PTR_ATTR_VOLATILE     0x0200
#define C7PTR_ATTR_CONST        0x0400

/* Descriptor values for VTShape record: (4-bit values) */

#define VTS_NEAR        0x0
#define VTS_FAR         0x1
#define VTS_THIN        0x2     // NYI


 /* Island register definitions.
    ** value is P2 register value + 8
    */
/* 8 bit registers */

#define DL_REG_AL   0
#define DL_REG_CL   1
#define DL_REG_DL   2
#define DL_REG_BL   3
#define DL_REG_AH   4
#define DL_REG_CH   5
#define DL_REG_DH   6
#define DL_REG_BH   7

/* 16 bit registers */

#define DL_REG_AX   8
#define DL_REG_CX   9
#define DL_REG_DX   10
#define DL_REG_BX   11
#define DL_REG_SP   12
#define DL_REG_BP   13
#define DL_REG_SI   14
#define DL_REG_DI   15

/* 32 bit registers */

#define DL_REG_EAX  16
#define DL_REG_ECX  17
#define DL_REG_EDX  18
#define DL_REG_EBX  19
#define DL_REG_ESP  20
#define DL_REG_EBP  21
#define DL_REG_ESI  22
#define DL_REG_EDI  23

/* segment registers */

#define DL_REG_ES   24
#define DL_REG_CS   25
#define DL_REG_SS   26
#define DL_REG_DS   27
#define DL_REG_FS   28
#define DL_REG_GS   29

/* special cases */

#define DL_REG_DXAX 32
#define DL_REG_ESBX 33
#define DL_REG_IP   34
#define DL_REG_FLAGS    35

/* registers for 8087/287/387 */

#define DL_REG_ST0  128
#define DL_REG_ST1  129
#define DL_REG_ST2  130
#define DL_REG_ST3  131
#define DL_REG_ST4  132
#define DL_REG_ST5  133
#define DL_REG_ST6  134
#define DL_REG_ST7  135
