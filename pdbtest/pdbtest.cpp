#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vcbudefs.h> // From \langapi\include
#include <pdb.h>
#include <cvinfo.h>
#include <cvr.h>

#define FALSE 0
#define TRUE  1

int CbNumLeaf (void *pleaf, TI ti);
BOOL FUDTInAnyMod(PB pbName);
void ReadAllMods(DBI *pdbi);
BOOL verifyGlobals(DBI* pdbi);
void verifyAllSyms(DBI* pdbi);
BOOL verifySymbols(Mod* pmod);
void verifySymbol(PSYM psym) ;

int Errors=0;
GSI *pgsi;
TPI *ptpi;

void main(int argc, char **argv)
{
	if (argc < 2 )
	{
		printf("Usage: pdbtest file.pdb\n");
		exit(0);
	}

	PDB *pdb;
	EC ec;
	char szError[cbErrMax];

	if (!PDB::Open(argv[1], "r", (SIG)0, &ec, szError, &pdb)) {
		printf("Couldn't open %s :E%d %s", argv[1], ec, szError);
		exit(1);
	}

	DBI *pdbi;

	if (!pdb->OpenDBI("xxx", "r", &pdbi)) {
		printf("Couldn't open DBI\n");
		exit(1);
	}

	ReadAllMods(pdbi);

	if (!pdbi->OpenGlobals(&pgsi))	{
		printf("Couldn't open globals\n");
		exit(1);
	}

	if (!pdb->OpenTpi("r", &ptpi))	{
		printf("Couldn't open types info\n");
		exit(1);
	}

	TI tiMin = ptpi->QueryTiMin();
	TI tiMac = ptpi->QueryTiMac();


	TI ti;

	for (ti = tiMin ; ti < tiMac ; ti++)
	{
		PB pb;

		ptpi->QueryPbCVRecordForTi(ti, &pb);

		lfClass *plfClass = (lfClass*)(pb+2);
		lfUnion *plfUnion = (lfUnion*)(pb+2);

		unsigned char *pdata = NULL;
		char *szType;

		switch (plfClass->leaf)
		{
		case LF_CLASS:
			if (plfClass->property.fwdref)
				pdata = &plfClass->data[0];
			szType = "class";
			break;

		case LF_STRUCTURE:
			if (plfClass->property.fwdref)
				pdata = &plfClass->data[0];
			szType = "struct";
			break;
		case LF_UNION:
			if (plfUnion->property.fwdref)
				pdata = &plfUnion->data[0];
			szType = "union";
			break;
		}

		if (pdata) {
			char buf[256];
			pdata += CbNumLeaf((void*)pdata, ti);
			memcpy(buf, pdata+1, pdata[0]);
			buf[pdata[0]] = 0;
			
			UDTSYM *pbSym = NULL;

			while (pbSym = (UDTSYM*)pgsi->HashSym(buf, (PB)pbSym)) {
				if (pbSym->rectyp != S_UDT)
					continue;

				if (!memcmp((void*)&pbSym->name[0], (void*)pdata, pdata[0]))
					break;
			}
			
			if (!pbSym && !FUDTInAnyMod(pdata)) {
				printf("0x%04x %s '%s' UDT not found\n", ti, szType, buf);
				Errors++;
			}
		}
	}
	

	verifyAllSyms( pdbi );
	
	pgsi->Close();
	ptpi->Close();
	pdbi->Close();
	pdb->Close();

	if(!Errors) printf("No errors were found.\n");
	else printf("%d errors were found in %s.\n", Errors, argv[1] );
	exit(Errors);
}

int CbNumLeaf (void *pleaf, TI ti)
{
    unsigned short  val;

    if ((val = ((lfEasy*)pleaf)->leaf) < LF_NUMERIC) {
        // No leaf can have an index less than LF_NUMERIC (0x8000)
		// so word is the value...
		return 2;
    }

    switch (val) {
        case LF_CHAR:
            return 3;

        case LF_USHORT:
		case LF_SHORT:
			return 4;

        case LF_LONG:
        case LF_ULONG:
            return 6;

        case LF_QUADWORD:
        case LF_UQUADWORD:
            return 10;

        default:
            printf("Error! 0x%04x bogus type encountered, aborting...\n", ti);
			exit(1);
    }
	return 0;
}

struct ML
{
	ML *pNext;
	PB  pbSym;
	CB	cbSym;
};

ML *pmlHead;

void ReadAllMods(DBI *pdbi)
{
	Mod *pmod = NULL;
	Mod *pmodNext = NULL;
	while (pdbi->QueryNextMod(pmod, &pmodNext) && pmodNext)
	{
		pmod = pmodNext;
		CB cb;
		if (!pmod->QuerySymbols(NULL, &cb))
			continue;

		// empty symbols
		if (!cb)
			continue;

		ML *pml = new ML;
		pml->pbSym = (PB)malloc(cb);
		pml->cbSym = cb;
		
		if (!pmod->QuerySymbols(pml->pbSym, &cb))
			continue;

		pml->pNext = pmlHead;
		pmlHead = pml;
	}
}

BOOL FUDTInAnyMod(PB pbName)
{
	for (ML *pml = pmlHead; pml; pml = pml->pNext)
	{
		PB pb = pml->pbSym + sizeof(ULONG);
		CB cb = pml->cbSym - sizeof(ULONG);

		while (cb > 0)
		{
			UDTSYM *pbSym = (UDTSYM*)pb;
			CB cbRec = pbSym->reclen + sizeof(pbSym->reclen);
			cb -= cbRec;
			pb += cbRec;

			if (pbSym->rectyp != S_UDT)
				continue;

			if (!memcmp((void*)&pbSym->name[0], (void*)pbName, pbName[0]))
				return TRUE;
		}
	}

	return FALSE;
}

BOOL verifyGlobals(DBI* pdbi) {
	printf("Verifying globals.\n");
	PB pbSym = 0;
	GSI* pgsi;
	if (!pdbi->OpenGlobals(&pgsi))
		return FALSE;
	while (pbSym = pgsi->NextSym(pbSym))
		verifySymbol((PSYM)pbSym);
	return TRUE;
}

void verifyAllSyms(DBI* pdbi) {
	verifyGlobals( pdbi );
	printf("Verifying modules.\n");
	Mod* pmod = 0;
	while (pdbi->QueryNextMod(pmod, &pmod) && pmod) {
		verifySymbols(pmod);
	}
}

BOOL verifySymbols(Mod* pmod) {
	CB cb;
	if (!pmod->QuerySymbols(0, &cb))
		return FALSE;
	PB pb = new BYTE[cb];
	if (!pb)
		return FALSE;
	if (!pmod->QuerySymbols(pb, &cb))
		return FALSE;

	PSYM psymMac = (PSYM)(pb + cb);
	for (PSYM psym = (PSYM)(pb + sizeof(ULONG)); psym < psymMac; psym = (PSYM)pbEndSym(psym))
		verifySymbol(psym);
	delete [] pb;
	return TRUE;
}

void verifySymbol(PSYM psym)
{
	SymTiIter tii(psym);
	if (tii.next())
	{
		TI ti=tii.rti();
		if (ti < ptpi->QueryTiMin()) {
			return;
		}

		PB pb;
		if (!ptpi->QueryPbCVRecordForTi(ti, &pb))
		{
			if(Errors==100) printf("More than 100 errors, output turned off...\n");
			Errors++;
			if(Errors>100) return;  /// Stop the ouput after 100 errors.
			printf("Error: cannot find type index: 0x%x\n", ti );
			return;
		}
	}
}

