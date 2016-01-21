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

#include "debsym.h"
#include "typeinfo.h"


#ifndef UNALIGNED

#ifdef  _M_IX86
#define UNALIGNED
#else
#define UNALIGNED __unaligned
#endif

#endif


struct TEXTTAB {
    int txtkey;
    const wchar_t *txtstr;
};

#define SMALL_BUFFER    128
#define END_OF_TABLE -1


void dump_typdef();
void dump_hex(size_t, bool);
BYTE getbyte();
void DumpCobol(BYTE);
void DumpCobL0(BYTE);
void DumpCobLinkage(BYTE);
void DumpCobOccurs(BYTE);
void DumpVCount();
void DumpCobItem(BYTE);


// Leaf_bytes - Number of bytes in this leaf
// Leaf_pos - Position we've parsed to in leaf
// Types_bytes, Types_pos - Same as Leaf_ but for whole TYPES buffer
// Types_count - number of types parsed


static size_t Leaf_bytes;
static size_t Types_bytes;
static size_t Types_count;
static size_t Leaf_pos;
static size_t Types_pos;
static char Txt[SMALL_BUFFER];         // Tmp buffer for strings read in
unsigned char getbyte();
BYTE *Buf;                            // Ptr into TYPES buf to where we've parsed


static const TEXTTAB leaf_table[] = {
        (int) OLF_BITFIELD,      L"LF_BITFIELD",
        (int) OLF_NEWTYPE,       L"LF_NEWTYPE",
        (int) OLF_HUGE,          L"LF_HUGE",
//      (int) OLF_SCHEMA,        L"LF_SCHEMA",
        (int) OLF_PLSTRUCTURE,   L"LF_PLSTRUCTURE",
        (int) OLF_PLARRAY,       L"LF_PLARRAY",
        (int) OLF_SHORT_NOPOP,   L"LF_SHORT_NOPOP",
        (int) OLF_LONG_NOPOP,    L"LF_LONG_NOPOP",
        (int) OLF_SELECTOR,      L"LF_SELECTOR",
        (int) OLF_INTERRUPT,     L"LF_INTERRUPT",
        (int) OLF_FILE,          L"LF_FILE",
        (int) OLF_PACKED,        L"LF_PACKED",
        (int) OLF_UNPACKED,      L"LF_UNPACKED",
        (int) OLF_SET,           L"LF_SET",
        (int) OLF_CHAMELEON,     L"LF_CHAMELEON",
        (int) OLF_BOOLEAN,       L"LF_BOOLEAN",
        (int) OLF_TRUE,          L"LF_TRUE",
        (int) OLF_FALSE,         L"LF_FALSE",
        (int) OLF_CHAR,          L"LF_CHAR",
        (int) OLF_INTEGER,       L"LF_INTEGER",
        (int) OLF_CONST,         L"LF_CONST",
        (int) OLF_LABEL,         L"LF_LABEL",
        (int) OLF_FAR,           L"LF_FAR",
        (int) OLF_LONG_POP,      L"LF_LONG_POP",
        (int) OLF_NEAR,          L"LF_NEAR",
        (int) OLF_SHORT_POP,     L"LF_SHORT_POP",
        (int) OLF_PROCEDURE,     L"LF_PROCEDURE",
        (int) OLF_PARAMETER,     L"LF_PARAMETER",
        (int) OLF_DIMENSION,     L"LF_DIMENSION",
        (int) OLF_ARRAY,         L"LF_ARRAY",
        (int) OLF_STRUCTURE,     L"LF_STRUCTURE",
        (int) OLF_POINTER,       L"LF_POINTER",
        (int) OLF_SCALAR,        L"LF_SCALAR",
        (int) OLF_UNSINT,        L"LF_UNSINT",
        (int) OLF_SGNINT,        L"LF_SGNINT",
        (int) OLF_REAL,          L"LF_REAL",
        (int) OLF_LIST,          L"LF_LIST",
        (int) OLF_EASY,          L"LF_EASY",
        (int) OLF_NICE,          L"LF_NICE",
        (int) OLF_STRING,        L"LF_STRING",
        (int) OLF_INDEX,         L"LF_INDEX",
        (int) OLF_REPEAT,        L"LF_REPEAT",
        (int) OLF_2_UNSIGNED,    L"LF_2_UNSIGNED",
        (int) OLF_4_UNSIGNED,    L"LF_4_UNSIGNED",
        (int) OLF_8_UNSIGNED,    L"LF_8_UNSIGNED",
        (int) OLF_1_SIGNED,      L"LF_1_SIGNED",
        (int) OLF_2_SIGNED,      L"LF_2_SIGNED",
        (int) OLF_4_SIGNED,      L"LF_4_SIGNED",
        (int) OLF_8_SIGNED,      L"LF_8_SIGNED",
        (int) OLF_BARRAY,        L"LF_BARRAY",
        (int) OLF_FSTRING,       L"LF_FSTRING",
        (int) OLF_FARRIDX,       L"LF_FARRIDX",
        (int) OLF_SKIP,          L"LF_SKIP",
        (int) OLF_BASEPTR,       L"LF_BASEPTR",
        (int) OLF_COBOLTYPREF,   L"LF_COBOLTYPREF",
        (int) OLF_COBOL,         L"COBOL",
        (int) END_OF_TABLE,      NULL
};


static const TEXTTAB bptr_leaf_table[] = {
        (int) OLF_BASESEG,       L"LF_BASESEG",
        (int) OLF_BASEVAL,       L"LF_BASEVAL",
        (int) OLF_BASESEGVAL,    L"LF_BASESEGVAL",
        (int) OLF_BASEADR,       L"LF_BASEADR",
        (int) OLF_BASESEGADR,    L"LF_BASESEGADR",
        (int) OLF_INDEX,         L"LF_BASEONTYPE",
        (int) OLF_NICE,          L"LF_BASEONSELF",
        (int) END_OF_TABLE,      NULL
};


// Dumps types for a single module
void DumpModTypC6(size_t cbTyp)
{
    Types_count = 0;
    while (cbTyp > 0) {
        Buf = RecBuf;
        if (_read(exefile, &RecBuf, 3) != 3) {
            Fatal(L"Types subsection wrong length");
        }
        Types_bytes = *((UNALIGNED WORD *)(Buf + 1));
        if (Types_bytes >= MAXTYPE - 3) {
            Fatal(L"Type string too long");
        }
        if ((size_t) _read(exefile, RecBuf + 3, (unsigned int) Types_bytes) != Types_bytes) {
            Fatal(L"Types subsection wrong length");
        }
        Types_bytes += 3;
        Types_pos = 0;
        dump_typdef();
        cbTyp -= Types_bytes;
    }
}


// TABLOOK - Utility for finding the text string in a table by matching
// its numerical key. Returns NULL if key not found.

const wchar_t *tablook(const TEXTTAB *table, int key)
{
    while (table->txtkey != END_OF_TABLE) {
        if (table->txtkey == key) {
            return(table->txtstr);
        }

        ++table;
    }

    return (NULL);
}

// ty_error()
//  Prints error message and also location and byte that seems
// to be in question.

void ty_error(BYTE ch, const wchar_t *msg)
{
    StdOutPrintf(L"\n??? Illegal value 0x%x, byte 0x%x of Types, byte 0x%x of Leaf\n", ch, Types_pos, Leaf_pos);
    StdOutPrintf(L"??? %s\n", msg);
}

// dump_hex()
//  Prints bytes in hex format. If update is FALSE, merely
// previews bytes. If true, dumps bytes and skips over them
// so that next read occurs after.

void dump_hex(size_t bytes, bool update)
{
    int num_on_line = 0;
    unsigned char *p = Buf;

    while (bytes--) {
        StdOutPrintf(L" 0x%02x", update ? getbyte() : *p++);

        if (! (++num_on_line % 8)) {
            StdOutPuts(L"\n");
        }
    }
}

// getbyte()
//  Returns next byte from buffer. Tries to play safe
// with how many bytes it thinks are left in the buffer
// and left in the type leaf it's looking at. If there
// are no bytes left in either, return 0.

BYTE getbyte()
{
    if ( (Leaf_pos < Leaf_bytes) && (Types_pos < Types_bytes)) {
        Types_pos++;
        Leaf_pos++;
        return (*Buf++);
    }

    if (Leaf_pos >= Leaf_bytes) {
        StdOutPuts(L"\nRead past end of leaf\n");
    }

    if (Types_pos >= Types_bytes) {
        StdOutPuts(L"\nRead past end of Types\n");
    }

    return (0);
}

// getshort()
//  Returns short value, assumes 8086 byte ordering

short getshort()
{
    register short i;

    if ( ( (Leaf_pos + 2) <= Leaf_bytes) && ( (Types_pos + 2) <= Types_bytes)) {
        Types_pos += 2;
        Leaf_pos += 2;
        i = * (short *) Buf;
        Buf += sizeof (short);
        return (i);
    }

    if (Leaf_pos >= Leaf_bytes){
        StdOutPuts(L"\nRead past end of leaf\n");
    }

    if (Types_pos >= Types_bytes) {
        StdOutPuts(L"\nRead past end of Types\n");
    }

    return (0);
}

// getlong()
//  Returns long value, assumes 8086 byte ordering

long getlong()
{
    long l;

    if ( ( (Leaf_pos + 4) <= Leaf_bytes) && ( (Types_pos + 4) <= Types_bytes)) {
        Types_pos += 4;
        Leaf_pos += 4;
        l = * (short *) Buf;
        Buf += sizeof (long);
        return (l);
    }

    if (Leaf_pos >= Leaf_bytes) {
        StdOutPuts(L"\nRead past end of leaf\n");
    }

    if (Types_pos >= Types_bytes) {
        StdOutPuts(L"\nRead past end of Types\n");
    }

    return (0);
}

// getstring (char)
//  Reads a length preceeded string and returns zero terminated string.

char *getstring()
{
    char *p = Txt;
    unsigned char n;

    n = getbyte();
    assert(n < SMALL_BUFFER);
    while (n--) {
        *p++ = (char) getbyte();
    }

    *p = '\0';

    return(Txt);
}

// getname()
//  returns Intel formatted string/name. Format
// is OLF_STRING | len | len bytes of text. Appends
// null byte before returning. Name is assumed
// to be less than SMALL_BUFFER

char *getname()
{
    unsigned char ch;

    ch = getbyte();

    if (ch != OLF_STRING) {
        ty_error(ch, L"Expecting string leaf");
    }

    return(getstring());
}

// getindex()
//  returns a type index for another type definition leaf
// Format is OLF_INDEX | (short)

unsigned short getindex()
{
    unsigned char ch;

    ch = getbyte();

    if (ch != OLF_INDEX) {
        ty_error(ch, L"Expecting index leaf");
    }

    return(getshort());
}

// getvalue()
//  Reads any of several different numeric leaves.
// not too rigorous on signed vs. unsigned but
// this is a dumper so not too crucial. May be
// fixed at some point in time.

long getvalue()
{
    unsigned char ch;
    char c;
    short s;

    ch = getbyte();
    if (ch < 128) {
        return (ch);
    }
    switch (ch & 0xff) {
        case OLF_STRING:
            s = getbyte();
            dump_hex(s, true);
            return (0);

        case OLF_1_SIGNED:
                // Fix for vax compiler bug, doesn't cast procedure return
                // values correctly

            c = getbyte();
            return (c);

        case OLF_2_SIGNED:
            s = getshort();
            return (s);

        case OLF_2_UNSIGNED:
            return (getshort() & 0xffff);

        case OLF_4_UNSIGNED:
        case OLF_4_SIGNED:
            return (getlong());

        case OLF_8_UNSIGNED:
        case OLF_8_SIGNED:
            StdOutPuts(L"??? 8 byte values not handled presently\n");
            dump_hex(8, true);
            return(0);

        default:
            ty_error(ch, L"Expecting numeric leaf");
            return (0);
    }
}


// dump_typdef() -
//  Dumps out a single type definition record from Buf
// If DB_TYPEHEX is set, will preface interpretation with
// hex dump of type leaf less linkage and length fields.
//  If it doesn't know what to do with a leaf, it will
// simply dump bytes in hex and continue to next leaf.


void dump_typdef()
{
    unsigned char   ch;
    const wchar_t   *s;
    int             i;
    int             numfields;
    int             tagcount;
    int             hi;
    int             low;
    const char     *hiname;
    const char     *lowname;

    Leaf_pos = -3;
    Leaf_bytes = 3; // Let it get first 3 bytes of leaf for free
    ch = getbyte(); // Linkage
    Leaf_bytes = getshort();
    Types_count++;

    StdOutPrintf(L"#%d: ", Types_count + PRIM_TYPES);
//  StdOutPrintf(L"Linkage = %s ", ch ? L"TRUE " : L"FALSE");
    StdOutPrintf(L"Length = %d ", Leaf_bytes);

    ch = getbyte();

    StdOutPrintf(L" Leaf = %d %s\n", ch, tablook(leaf_table, ch));
    switch (ch) {
        case OLF_ARRAY :
            StdOutPrintf(L"    Length = %d, ", getvalue());
            StdOutPrintf(L"Element type %d, ", getindex());
            if (Leaf_pos < Leaf_bytes) {
                StdOutPrintf(L"Index type %d, ", getindex());
                StdOutPrintf(L"Name '%S'", getname());
            }
            StdOutPuts(L"\n");
            break;

        case OLF_LABEL:
            StdOutPrintf(L"    nil leaf %d ", getbyte());
            StdOutPrintf(L"'%s'\n", (getbyte() == OLF_NEAR) ? L"NEAR" : L"FAR");
            break;

        case OLF_PARAMETER:
            StdOutPrintf(L"    Type = %d\n", getindex());
            break;

        case OLF_PROCEDURE:
            StdOutPuts(L"\t");
            if (ch == OLF_PROCEDURE) {
                StdOutPrintf(L"nil leaf %d, ", getbyte());
            }
            if (ch == OLF_MEMBERFUNC) {
                StdOutPrintf(L"return type %d, ", getshort());
                StdOutPrintf(L"class type %d, ", getshort());
                StdOutPrintf(L"this type %d, ", getshort());
            }
            else {
                if (*Buf == OLF_INDEX) {
                    StdOutPrintf(L"return type %d, ", getindex());
                }
                else {
                    ch = getbyte();
                    StdOutPrintf(L"void function, nice leaf %d %s, ", ch, tablook(leaf_table, ch));
                }
            }

            switch (getbyte()) {
                case OLF_SHORT_POP:
                    s = L"PLM SHORT POP";
                    break;

                case OLF_SHORT_NOPOP:
                    s = L"C SHORT NO POP";
                    break;

                case OLF_LONG_POP:
                    s = L"PLM LONG POP";
                    break;

                case OLF_LONG_NOPOP:
                    s = L"C LONG NO POP";
                    break;

                case OLF_NFASTCALL:
                    s = L"FASTCALLS SHORT";
                    break;

                case OLF_FFASTCALL:
                    s = L"FASTCALLS LONG";
                    break;

                default:
                    s = L"???";
                    assert(false);
            }
            StdOutPrintf(L"'%s'\n", s);
            StdOutPrintf(L"    %ld parameters in ", getvalue());

            if (ch == OLF_MEMBERFUNC) {
                StdOutPrintf(L"this adj %ld ", getvalue());
                StdOutPrintf(L"type index %d\n", getshort());
            }
            else {
                StdOutPrintf(L"type index %d\n", getindex());
            }
            break;

        case OLF_SCALAR:
            StdOutPrintf(L"    %ld bits, ", getvalue());
            if (*Buf == OLF_INDEX) {
                StdOutPrintf(L"index %d, ", i = getindex());
            }
            else {
                i = (int) getvalue();
                StdOutPrintf(L"style '%s' (%d), ", tablook(leaf_table, i), i);
            }
            StdOutPrintf(L"name '%S' ", getname());
                // For Inteltypes, pointers are partially completed
                // scalar leaves that end after the name. Here
                // we check to see if the scalar leaf is done or not

            if (Leaf_pos < Leaf_bytes) {
                if (*Buf == OLF_INDEX) {
                    StdOutPrintf(L"index %d, ", i = getindex());
                }
                else {
                    i = (int) getvalue();
                    StdOutPrintf(L"more style '%s' (%d), ", tablook(leaf_table, i), i);
                }
                StdOutPrintf(L"\n  low bound %ld, ", getvalue());
                StdOutPrintf(L"hi bound %ld", getvalue());
            }
            StdOutPuts(L"\n");
            break;

        case OLF_EASY:
            StdOutPuts(L"\tEASY (dummy)\n");
            break;

        case OLF_BITFIELD:
            StdOutPrintf(L"    %d bits, ", getvalue());
            StdOutPrintf(L"%s, ", (getbyte() == OLF_SGNINT) ? L"OLF_SGNINT" : L"OLF_UNSINT");
            StdOutPrintf(L"%d starting position\n", getbyte());
            break;

        case OLF_NEWTYPE:
            StdOutPrintf(L"    %d new type, ", getindex());
            StdOutPrintf(L"alias type name '%S'\n", getname());
            break;

        case OLF_CONST:
        case OLF_FILE:
        case OLF_REAL:
        case OLF_SET:
            StdOutPuts(L"Not implemented quite yet...\n");

        case OLF_BARRAY:
            StdOutPrintf(L"    Element type %d\n", getindex());
            break;

        case OLF_FSTRING:
            i = getbyte();
            StdOutPrintf(L"    tag = %d, ", i);
            switch (i) {
                case 0:
                    StdOutPrintf(L"Fixed length string, length = %d\n", getvalue());
                    break;

                case 1:
                    StdOutPrintf(L"Variable length string, offset = %d\n", getvalue());
                    break;

                default:
                    StdOutPuts(L"Bad tag\n");
                    break;
            }
            break;

        case OLF_FARRIDX:
            i = getbyte();
            StdOutPrintf(L"    tag = %d, ", i);
            switch (i) {
                case 0:
                    lowname = getname();
                    StdOutPrintf(L"low_name = '%S'\n", lowname);
                    break;

                case 1:
                    StdOutPrintf(L"low_bound = %d\n", getvalue());
                    break;

                case 2:
                    low = (int) getvalue();
                    hi = (int) getvalue();
                    StdOutPrintf(L"low_bound = %d, hi_bound = %d\n", low, hi);
                    break;

                case 3:
                    low = (int) getvalue();
                    hiname = getname();
                    StdOutPrintf(L"low_bound = %d, hi_name = '%S'\n", low, hiname);
                    break;

                case 4:
                    lowname = getname();
                    hi = (int) getvalue();
                    StdOutPrintf(L"low_name = '%S', hi_bound = %d\n", lowname, hi);
                    break;

                case 5:
                    lowname = getname();
                    StdOutPrintf(L"low_name = '%S', ", lowname);
                    hiname = getname();
                    StdOutPrintf(L"hi_name = '%S'\n", hiname);
                    break;

                case 6:
                    StdOutPrintf(L"tmp value = %d\n", getvalue());
                    break;

                case 7:
                    StdOutPrintf(L"tmp name = '%S'\n", getname());
                    break;

                default:
                    StdOutPuts(L"Bad tag\n");
                    break;
            }
            break;

        case OLF_SKIP:
            Types_count = getshort();
            StdOutPrintf(L"\tNext effective type index: %d.\n", Types_count);
            Types_count -= PRIM_TYPES + 1;
            dump_hex((Leaf_bytes - Leaf_pos), true);
            break;

        case OLF_POINTER:
            // If Index follows, then Intel format
            if (*Buf == OLF_INDEX) {
                StdOutPrintf(L"    Type = %d\n", getindex());
            }
            else {
                // MS format
                ch = getbyte();
                StdOutPrintf(L"    %s (%d) ", tablook(leaf_table, ch), ch);
                StdOutPrintf(L" Base Type = %d ", getindex());
                // print pointer name, if any
                if (Leaf_pos < Leaf_bytes) {
                    StdOutPrintf(L"'%S'\n", getname());
                }
                else {
                    StdOutPuts(L"\n");
                }
            }
            break;

        case OLF_STRUCTURE:
            StdOutPrintf(L"    %d bits, ", getvalue());
            numfields = (int) getvalue();
            StdOutPrintf(L"%d fields, ", numfields);
            if (numfields) {    // Structures, Variants, or Equivalences
                StdOutPrintf(L"%d type list, ", getindex());
                StdOutPrintf(L"%d name list, ", getindex());
            }
            else {              // Unions, 2 easy leaves
                i = (int) getvalue();
                StdOutPrintf(L"'%s', ", tablook(leaf_table, i), i);
                i = (int) getvalue();
                StdOutPrintf(L"'%s', ", tablook(leaf_table, i), i);
            }
            StdOutPrintf(L"\n  name '%S', ", getname());
            if (Leaf_pos > Leaf_bytes) {
                break;
            }
            StdOutPrintf(L"'%s'\n", (getbyte() == OLF_PACKED) ? L"OLF_PACKED" : L"OLF_UNPACKED");
            if (Leaf_pos >= Leaf_bytes) {
                // Structure leaves finished
                break;
            }

            StdOutPrintf(L"    tagcount %d, ", tagcount = (int) getvalue());
            if (numfields) {
                StdOutPrintf(L"%d tag type, ", getindex());
                StdOutPrintf(L"tag name '%S'\n", getname());
            }
            else {
                i = (int) getvalue();
                StdOutPrintf(L"'%s', ", tablook(leaf_table, i), i);
                i = (int) getvalue();
                StdOutPrintf(L"'%s'\n", tablook(leaf_table, i), i);
            }
            while (tagcount--) {
                StdOutPrintf(L"    %d values - ", i = (int) getvalue());
                if (numfields)
                    while (i--)
                        StdOutPrintf(L"%d, ", getvalue());
                StdOutPrintf(L"%d type list, ", getindex());
                StdOutPrintf(L"%d name list\n", getindex());
            }
            break;

        case OLF_BASEPTR:
            StdOutPrintf(L"\tElement type: %d, ", getindex());
            StdOutPrintf(L"Base: %s",
                tablook (bptr_leaf_table, (int) (ch = getbyte())));
            switch (ch) {
                case OLF_BASESEG:
                    StdOutPrintf(L", Segment#: %d.\n", getshort());
                    break;
                case OLF_BASEVAL:
                case OLF_BASESEGVAL:
                case OLF_BASEADR:
                case OLF_BASESEGADR:
                    StdOutPrintf(L",\n\t$SYMBOLS offset: %d", getshort());
                    StdOutPrintf(L", reserved: %d.\n", getshort());
                    break;
                case OLF_INDEX:
                    StdOutPrintf(L", type index: %d.\n", getshort());
                    break;
                case OLF_NICE:
                    StdOutPuts(L".\n");
                    break;
                default:
                    StdOutPuts(L"Bad tag\n");
            }
            break;

        case OLF_COBOLTYPREF:
            StdOutPrintf(L"\tParent index: %d",getindex());
            ch = getbyte();
            if (ch == 0) {
                DumpCobL0(ch);
            }
            else {
                DumpCobol(ch);
            }
            break;

        case OLF_COBOL:
            ch = getbyte();
            if (ch == 0) {
                DumpCobL0(ch);
            }
            else {
                DumpCobol(ch);
            }
            break;

        default :
            dump_hex(Leaf_bytes - 1, true);
    }
    StdOutPuts(L"\n");
}


void DumpCobol(BYTE level)
{
    BYTE   ch;

    StdOutPrintf(L"\tLevel = %2d", level & 0x7f);
    if (level & 0x80) {
        StdOutPuts(L"(Group)");
    }
loop:
    // check next byte of type string

    ch = getbyte();
    if ((ch & 0xfe) == 0xc0) {
        // output linkage informatioon byte
        DumpCobLinkage (ch);
        if (Leaf_pos < Leaf_bytes) {
            ch = getbyte();
        }
        goto loop;
    }
    if (Leaf_pos <= Leaf_bytes) {
        if ((ch & 0xe0) == 0xe0) {
            // output OCCURS subscript information
            DumpCobOccurs (ch);
            goto loop;
        }
    }
    if (Leaf_pos <= Leaf_bytes) {
        DumpCobItem (ch);
    }
    dump_hex((Leaf_bytes - Leaf_pos), true);
    StdOutPuts(L"\n");
}


void DumpCobL0 (BYTE level)
{
    WORD  NameAlg = getshort();

    StdOutPrintf(L"\tLevel = %02d ", level);
    StdOutPrintf(L"root = \"%S\"", getstring());
    dump_hex((Leaf_bytes - Leaf_pos), true);
    StdOutPuts(L"\n");
}


void DumpCobLinkage(BYTE linkage)
{
    StdOutPuts(L"Linkage");
    if (linkage & 0x01) {
        DumpVCount();
    }
}


void DumpCobOccurs(BYTE occurs)
{
    StdOutPrintf(L" OCCURS (0x%02x) ", occurs);
    if ((occurs & 0x10) == 0) {
        StdOutPrintf(L" stride - 1 = %d", occurs & 0x0f);
    }
    else {
        StdOutPuts(L" extended stride - 1 = ");
        DumpVCount();
    }
    StdOutPuts(L" maximum bound = ");
    DumpVCount();
    StdOutPuts(L"\n");
}


void DumpVCount()
{
    BYTE   ch;
    WORD  ush;
    long    lng;

    ch = getbyte();

    if ((ch & 0x80) == 0) {
        StdOutPrintf(L"%d", ch);
    }
    else if ((ch & 0xc0) == 0x80) {
        ush = ((ch & 0x37) << 8) | getbyte();
        StdOutPrintf(L"%d", ush);
    }
    else if ((ch & 0xf0) == 0xc0) {
        lng = (ch & 0x1f << 24) | getbyte() << 16 | getshort();
        StdOutPrintf(L"%d", lng);
    }
    else if ((ch & 0xf0) == 0xf0) {
        lng = (ch << 24) | getbyte() << 16 | getshort();
        StdOutPrintf(L"%d", lng);
    }
    else {
        StdOutPuts(L"unknown vcount format");
    }
}

const wchar_t * const display[4] = {
    L"\ttrailing included ",
    L"\ttrailing separate ",
    L"\tleading included ",
    L"\tleading separate "
};

const wchar_t * const notdisplay[4] = {
    L"\tCOMP ",
    L"\tCOMP-3 ",
    L"\tCOMP-X ",
    L"\tCOMP-5 "
};


void DumpCobItem(BYTE ch)
{
    WORD  ch2;
    WORD  f;
    short   size;

    if ((ch & 0x80) == 0) {
        // dump numeric

        ch2 = getbyte();
        StdOutPrintf(L" (0x%02x 0x%02x) ", ch, ch2);
        StdOutPuts(L"numeric ");
        if ((ch & 0x40) == 0x40) {
            StdOutPuts(L"not ");
        }
        StdOutPuts(L"DISPLAY ");
        if ((ch & 0x20) == 0x20) {
            StdOutPuts(L"not LITERAL ");
        }
        else {
            StdOutPrintf(L"LITERAL = %0x02x", getbyte());
        }
        if ((ch2 & 0x80) == 0x80) {
            StdOutPuts(L"not ");
        }
        StdOutPuts(L"signed\n");
        f = (ch2 & 0x60) >> 5;
        if (ch & 0x20) {
            StdOutPrintf(L"%s", display[f]);
        }
        else {
            StdOutPrintf(L"%s", notdisplay[f]);
        }
        StdOutPrintf(L"N1 = 0x%02x, N2 = 0x%02x\n", ch & 0x1f, ch2 & 0x1f);
    }
    else {
        // dump alphanumeric/alphabetic

        StdOutPrintf(L" (0x%02x) ", ch);
        if ((ch & 0x04) == 0x04) {
            StdOutPuts(L"alphabetic ");
        }
        else {
            StdOutPuts(L"alphanumeric ");
        }
        if ((ch & 0x20) == 0x20) {
            StdOutPuts(L"not ");
        }
        StdOutPuts(L"LITERAL ");
        if ((ch & 0x10) == 0x10) {
            StdOutPuts(L"JUSTIFIED ");
        }
        if ((ch & 0x08) == 0) {
            // extended size is zero, this and next byte contains size
            size = (ch & 0x03) << 8 | getbyte();
            StdOutPrintf(L"size - 1 = %d ", size);

            // if not extended size and literal, then display string

            if ((ch & 0x20) == 0) {
                StdOutPuts(L"\n\t literal = ");
                while (size-- >= 0) {
                    StdOutPrintf(L"%c", getbyte());
                }
            }
        }
        else {
            // extended size is true, read the size in vcount format.
            // I do not believe a literal can follow if extended size
            // true
            StdOutPuts(L"size - 1 = ");
            DumpVCount();
        }
    }
}
