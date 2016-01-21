/***********************************************************************
* Microsoft (R) Debugging Information Dumper
*
* Copyright (c) Microsoft Corporation.  All rights reserved.
*
* File Comments:
*
*
***********************************************************************/

/*
*   SYMBOLS segment definitions
*/

    /* Record types */

#define S_BLOCK         0              /* Block start - obsolete */
#define S_PROC          1              /* Procedure start - obsolete */
#define S_END           2              /* Block, procedure, or "with" end */
#define S_BPREL         4              /* BP-relative */
#define S_LOCAL         5              /* Module-local symbol */
#define S_LABEL         11             /* Code label - obsolete */
#define S_WITH          12             /* "With" start - obsolete */
#define S_REG           13             /* Register variable */
#define S_CONST         14             /* Constant symbol */
#define S_ENTRY         15             /* entry symbol */
#define S_NOOP          16             /* noop - used for incremental padding */
#define S_CODSEG        17             /* effective code segment */
#define S_TYPEDEF       18             /* Used to specify a typedef */
#define S_GLOBAL        19             /* Used to specify global data */
#define S_GLOBPROC      20             /* Used to specify global procedure */
#define S_LOCPROC       21             /* Used to specify local procedure */
#define S_CHGMODEL      22             /* Change execution model - obsolete */
#define S_PUBLIC        23             /* Symbol in $$PUBLICS section */
#define S_THUNK         24             /* Thunk start */
#define S_SEARCH        25             /* Start Search */
#define S_CV4BLOCK      26             /* New version of S_BLOCK */
#define S_CV4WITH       27             /* New version of S_WITH */
#define S_CV4LABEL      28             /* New version of S_LABEL */
#define S_CV4CHGMODEL   29             /* New version of S_CHGMODEL */
#define S_COMPILEFLAG   30             // Some info about the compiler

    /* Code return type */

#define S_NEAR      0   /* NEAR code flag */
#define S_FAR       4   /* FAR code flag */
