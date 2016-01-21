/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

typedef struct BLKSYMTYPE     // Obsolete
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      len;        /* Block length */
    char        name[1];    /* Length-prefixed name */
} BLKSYMTYPE;

typedef struct PROCSYMTYPE     // Obsolete
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      typind;     /* Type index */
    unsigned short      len;        /* Proc length */
    unsigned short      startoff;   /* Debug start offset */
    unsigned short      endoff;     /* Debug end offset */
    short       res;        /* Reserved */
    char        rtntyp;     /* Return type (NEAR/FAR) */
    char        name[1];    /* Length-prefixed name */
} PROCSYMTYPE;

typedef struct WITHSYMTYPE      // Obsolete
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      len;        /* Length of scope */
    char        name[1];    /* String to be evaluated */
} WITHSYMTYPE;

typedef struct BPSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* BP-relative offset */
    unsigned short      typind;     /* Type index */
    char        name[1];    /* Length-prefixed name */
} BPSYMTYPE;

typedef struct LABSYMTYPE     //Obsolete
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    char        rtntyp;     /* Return type (NEAR/FAR) */
    char        name[1];    /* Length-prefixed name */
} LABSYMTYPE;

typedef struct LOCSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in segment */
    unsigned short      seg;        /* Segment address */
    unsigned short      typind;     /* Type index */
    char        name[1];    /* Length-prefixed name */
} LOCSYMTYPE;

typedef struct REGSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned short      typind;     /* Type index */
    char        reg;        /* Which register */
    char        name[1];    /* Length-prefixed name */
} REGSYMTYPE;

typedef struct CONSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned short      typind;     /* Type index */
    char        value[1];   /* Variable-length value */
    char        name[1];    /* Length-prefixed name */
} CONSYMTYPE;

typedef struct TYPEDEFSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned short      typind;     /* Type index */
    char        name[1];    /* Length-prefixed name */
} TYPEDEFSYMTYPE;

typedef struct CV4PROCSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       parentsym;  /* Offset of sym of enclosing proc */
    unsigned long       endsym;     /* matching end */
    unsigned long       nextsym;    /* Sym of closest following proc */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Seg of proc */
    unsigned short      typind;     /* Type index */
    unsigned long       len;        /* Proc length */
    unsigned long       startoff;   /* Debug start offset */
    unsigned long       endoff;     /* Debug end offset */
    char                rtntyp;     /* Return type (NEAR/FAR) */
    char                name[1];    /* Length-prefixed name */
} CV4PROCSYMTYPE;

typedef struct THUNKSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       parentsym;  /* Offset of sym of enclosing proc */
    unsigned long       endsym;     /* matching end */
    unsigned long       nextsym;    /* Sym of closest following proc */
    char                ord;        /* Type of thunk */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Seg of proc */
    unsigned short      len;        /* Thunk length */
    char                name[1];    /* Thunk name */
    union {
        struct {
            short           delta;      /* Size of adjustment */
            char            name[1];    /* Name of target function */
            }             adjustor;
        short             vtaboff;      /* Offset into the vtable */
        }               variant;
} THUNKSYMTYPE;

typedef struct CV4BLKSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       parentsym;  /* Offset of sym of enclosing proc */
    unsigned long       endsym;     /* matching end */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Segment of code */
    unsigned long       len;        /* Block length */
    char                name[1];    /* Length-prefixed name */
} CV4BLKSYMTYPE;

typedef struct CV4WITHSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       parentsym;  /* Offset of sym of enclosing proc */
    unsigned long       endsym;     /* matching end */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Segment of code */
    unsigned long       len;        /* Length of scope */
    char                name[1];    /* String to be evaluated */
} CV4WITHSYMTYPE;

typedef struct CV4LABSYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Segment of code */
    char                rtntyp;     /* Return type (NEAR/FAR) */
    char                name[1];    /* Length-prefixed name */
} CV4LABSYMTYPE;

typedef struct CV4CHANGESYMTYPE
{
    unsigned char       reclen;     /* Record length */
    unsigned char       rectyp;     /* Record type */
    unsigned long       off;        /* Offset in code seg */
    unsigned short      seg;        /* Segment of code */
    char                model;      /* Execution model to change to */
    char                var[1];     /* Variant info (unspecified) */
} CV4CHANGESYMTYPE;

/* Far pointer definitions */

typedef BLKSYMTYPE          *BLKSYMPTR;        // Obsolete
typedef CONSYMTYPE          *CONSYMPTR;
typedef REGSYMTYPE          *REGSYMPTR;
typedef LOCSYMTYPE          *LOCSYMPTR;
typedef LABSYMTYPE          *LABSYMPTR;        // Obsolete
typedef BPSYMTYPE           *BPSYMPTR;
typedef WITHSYMTYPE         *WITHSYMPTR;       // Obsolete
typedef PROCSYMTYPE         *PROCSYMPTR;       // Obsolete
typedef TYPEDEFSYMTYPE      *TYPEDEFSYMPTR;
typedef CV4PROCSYMTYPE      *CV4PROCSYMPTR;
typedef THUNKSYMTYPE        *THUNKSYMPTR;
typedef CV4BLKSYMTYPE       *CV4BLKSYMPTR;
typedef CV4WITHSYMTYPE      *CV4WITHSYMPTR;
typedef CV4LABSYMTYPE       *CV4LABSYMPTR;
typedef CV4CHANGESYMTYPE    *CV4CHANGESYMPTR;
